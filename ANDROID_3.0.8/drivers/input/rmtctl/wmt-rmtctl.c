/*++ 
 * linux/drivers/input/rmtctl/wmt-rmtctl.c
 * WonderMedia input remote control driver
 *
 * Copyright c 2010  WonderMedia  Technologies, Inc.
 *
 * This program is free software: you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 2 of the License, or 
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details. 
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * WonderMedia Technologies, Inc.
 * 4F, 533, Chung-Cheng Road, Hsin-Tien, Taipei 231, R.O.C
--*/

  
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/input.h>
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,10))
#include <linux/platform_device.h>
#endif
#include <asm/irq.h>
#include <mach/hardware.h>
#include <mach/wmt_pmc.h>
#include <linux/delay.h>
#include "wmt-rmtctl.h"

#define DRIVER_DESC   "WMT rmtctl driver"
#define RC_INT 55

enum cir_signal_type {
	CIR_TYPE_NEC = 0,
	CIR_TYPE_TOSHIBA = 1,
	CIR_TYPE_MAX,
};

struct cir_signal_parameter {
	enum cir_signal_type signal_type;
	unsigned int parameter[7];
	unsigned int repeat_timeout;
};

struct cir_vendor_info {
	char *vendor_name;
	enum cir_signal_type signal_type;
	
	unsigned int vendor_code;
	unsigned int wakeup_code;
	unsigned int key_codes[255]; // usb-keyboard standard

	void (*check_comb_keys)(void *arg);
};

struct cir_comb_keys {
	char shift_pressed;
	char ctrl_pressed;
	char alt_pressed;

	char shift_release;
	char ctrl_release;
	char alt_release;
};

static struct cir_signal_parameter rmtctl_parameters[] = {
	// NEC spec: |9ms carrier wave| + |4.5ms interval|
	[CIR_TYPE_NEC] = {
		.signal_type = CIR_TYPE_NEC,
		.parameter = { 0x10a, 0x8e, 0x42, 0x55, 0x9, 0x13, 0x13 },
		.repeat_timeout = 17965000,
	},

	// TOSHIBA 9012 spec: |4.5ms carrier wave| + |4.5ms interval|
	[CIR_TYPE_TOSHIBA] = {
		.signal_type = CIR_TYPE_TOSHIBA,
		.parameter = { 0x8e, 0x8e, 0x42, 0x55, 0x9, 0x13, 0x13 },
		.repeat_timeout = 17965000,
	},
};

static void rmtctl_mdk079_check_comb_keys(void *arg);

static struct cir_vendor_info rmtctl_vendors[] = {
	// SRC1804
	{
		.vendor_name = "SRC1804",
		.signal_type = CIR_TYPE_NEC,
		.vendor_code = 0x02fd,
		.wakeup_code = 0x57,
		.key_codes = {
			[0x57] = KEY_POWER,      /* power down */
			[0x56] = KEY_VOLUMEDOWN, /* vol- */
			[0x14] = KEY_VOLUMEUP,   /* vol+ */
			[0x53] = KEY_HOME,       /* home */
			[0x11] = KEY_MENU,       /* menu */
			[0x10] = KEY_BACK,       /* back */
			[0x4b] = KEY_ZOOMOUT,    /* zoom out */
			[0x08] = KEY_ZOOMIN,     /* zoom in */
			[0x0d] = KEY_UP,         /* up */
			[0x4e] = KEY_LEFT,       /* left */
			[0x19] = KEY_ENTER,      /* OK */
			[0x0c] = KEY_RIGHT,      /* right */
			[0x4f] = KEY_DOWN,       /* down */
			[0x09] = KEY_PAGEUP,     /* page up */
			[0x47] = KEY_PREVIOUSSONG, /* rewind */
			[0x05] = KEY_PAGEDOWN,   /* page down */
			[0x04] = KEY_NEXTSONG    /* forward */
		},
	},

	// IH8950
	{
		.vendor_name = "IH8950",
		.signal_type = CIR_TYPE_NEC,
		.vendor_code = 0x0909,
		.wakeup_code = 0xdc,
		.key_codes = {
			[0xdc] = KEY_POWER,	     /* power down */
			[0x81] = KEY_VOLUMEDOWN, /* vol- */
			[0x80] = KEY_VOLUMEUP,	 /* vol+ */
			[0x82] = KEY_HOME,		 /* home */
			[0xc5] = KEY_BACK,		 /* back */
			[0xca] = KEY_UP,		 /* up */
			[0x99] = KEY_LEFT,		 /* left */
			[0xce] = KEY_ENTER, 	 /* OK */
			[0xc1] = KEY_RIGHT, 	 /* right */
			[0xd2] = KEY_DOWN,		 /* down */
			[0x9c] = KEY_MUTE,
			[0x95] = KEY_PLAYPAUSE,
			[0x88] = KEY_MENU,
		},
	},

	// sunday
	{
		.vendor_name = "sunday",
		.signal_type = CIR_TYPE_NEC,
		.vendor_code = 0x02fd,
		.wakeup_code = 0x1a,
		.key_codes = {
			[0x1a] = KEY_POWER,	     /* power down */
			[0x16] = KEY_VOLUMEDOWN, /* vol- */
			[0x44] = KEY_VOLUMEUP,	 /* vol+ */
			[0x59] = KEY_HOME,		 /* home */
			[0x1b] = KEY_BACK,		 /* back */
			[0x06] = KEY_UP,		 /* up */
			[0x5d] = KEY_LEFT,		 /* left */
			[0x1e] = KEY_ENTER, 	 /* OK */
			[0x5c] = KEY_RIGHT, 	 /* right */
			[0x1f] = KEY_DOWN,		 /* down */
			[0x55] = KEY_PLAYPAUSE,
			[0x54] = KEY_REWIND,
			[0x17] = KEY_FORWARD,
			[0x88] = KEY_MENU,
		},
	},
};

/** definition & configuration */
#define RMTCTL_DEBUG 1
static enum cir_signal_type rmtctl_signal_type = CIR_TYPE_NEC;
static struct input_dev *idev;
static struct timer_list rmtctl_timer;
static struct cir_comb_keys rmtctl_comb_keys = { 0 };
static int rmtctl_vendor_index = -1;

static void rmtctl_mdk079_check_comb_keys(void *arg)
{
	unsigned int vendor = *(unsigned int *)arg;
	rmtctl_comb_keys.shift_pressed = (vendor & BIT7) ? 1 : 0;
	rmtctl_comb_keys.ctrl_pressed = (vendor & BIT6) ? 1 : 0;
	rmtctl_comb_keys.alt_pressed = (vendor & BIT5) ? 1 : 0;
}

static inline void rmtctl_report_comb_keys_down(void)
{
	if (rmtctl_comb_keys.shift_pressed) {
		input_event(idev, EV_KEY, KEY_LEFTSHIFT, 1);
		input_sync(idev);
		rmtctl_comb_keys.shift_release = 1;
	}
	
	if (rmtctl_comb_keys.ctrl_pressed) {
		input_event(idev, EV_KEY, KEY_LEFTCTRL, 1);
		input_sync(idev);
		rmtctl_comb_keys.ctrl_release= 1;
	}

	if (rmtctl_comb_keys.alt_pressed) {
		input_event(idev, EV_KEY, KEY_LEFTALT, 1);
		input_sync(idev);
		rmtctl_comb_keys.alt_release= 1;
	}
}

static inline void rmtctl_report_comb_keys_up(void)
{
	if (rmtctl_comb_keys.shift_release) {
		input_event(idev, EV_KEY, KEY_LEFTSHIFT, 0);
		input_sync(idev);
		rmtctl_comb_keys.shift_release = 0;
	}

	if (rmtctl_comb_keys.ctrl_release) {
		input_event(idev, EV_KEY, KEY_LEFTCTRL, 0);
		input_sync(idev);
		rmtctl_comb_keys.ctrl_release = 0;
	}

	if (rmtctl_comb_keys.alt_release) {
		input_event(idev, EV_KEY, KEY_LEFTALT, 0);
		input_sync(idev);
		rmtctl_comb_keys.alt_release = 0;
	}
}

static void rmtctl_report_event(const unsigned int code, int repeat)
{
	static unsigned int last_code = KEY_RESERVED;
	int ret;
	
	ret = del_timer(&rmtctl_timer);

	if (!repeat) {
		// New code coming
		if (ret == 1) {
			// del_timer() of active timer returns 1, timer has been stopped by new key,
			// so report last key up event by manually
			if (RMTCTL_DEBUG)
				printk("[%d] up reported by manually\n", last_code);
			
			input_event(idev, EV_KEY, last_code, 0);
			input_sync(idev);

			rmtctl_report_comb_keys_up();
		}

		if (RMTCTL_DEBUG)
			printk("[%d] down\n", code);
		// Report key down event, mod_timer() to report key up event later for measure key repeat.

		rmtctl_report_comb_keys_down();
		
		input_event(idev, EV_KEY, code, 1);
		input_sync(idev);
		rmtctl_timer.data = code;
		mod_timer (&rmtctl_timer, jiffies + 400*HZ/1000);
	}
	else {
		// Report key up event after report repeat event according to usb-keyboard standard
		rmtctl_timer.data = code;
		mod_timer (&rmtctl_timer, jiffies + 400*HZ/1000);
		// Detect 'repeat' flag, report long-pressed event
		if (code != KEY_POWER && code != KEY_END) {
			if (RMTCTL_DEBUG)
				printk("[%d] repeat\n", code);
			input_event(idev, EV_KEY, code, 2);
			input_sync(idev);
		}
	}

	last_code = code;
}

/** No hw repeat detect, so we measure repeat timeout event by sw */
static void rmtctl_report_event_without_hwrepeat(const unsigned int code)
{
	static unsigned int last_code = KEY_RESERVED;
	static unsigned long last_jiffies = 0;

	//printk("-->%d\n", jiffies_to_msecs (jiffies - last_jiffies));
	int repeat = (jiffies_to_msecs (jiffies - last_jiffies) <250 && code==last_code) ? 1 : 0;

	last_code = code;
	last_jiffies = jiffies;

	rmtctl_report_event(code, repeat);
}

static void rmtctl_timer_handler(unsigned long data)
{
	// Report key up event
	if (RMTCTL_DEBUG)
		printk("[%d] up reported by timer\n", data);
	input_report_key(idev, data, 0);
	input_sync(idev);

	rmtctl_report_comb_keys_up();
}

static irqreturn_t rmtctl_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned int status, ir_data, scancode, vendor, repeat;
	unsigned char *key;
	int i;

	/* Get IR status. */
	status = REG32_VAL(IRSTS);

	/* Check 'IR received data' flag. */
	if ((status & 0x1) == 0x0) {
		printk("IR IRQ was triggered without data received. (0x%x)\n",
			status);
		return IRQ_NONE;
	}

	/* Read IR data. */
	ir_data = REG32_VAL(IRDATA(0)) ;
	key = (char *) &ir_data;

	/* clear INT status*/
	REG32_VAL(IRSTS)=0x1 ;

	if (RMTCTL_DEBUG){
		printk("ir_data = 0x%08x, status = 0x%x \n", ir_data, status);
	}
	/* Get vendor ID. */
	vendor = (key[0] << 8) | (key[1]);

	/* Check if key is valid. Key[3] is XORed t o key[2]. */
	if (key[2] & key[3]) { 
		printk("Invalid IR key received. (0x%x, 0x%x)\n", key[2], key[3]);
		return IRQ_NONE;
	}
	
	/* Keycode mapping. */
	scancode = key[2];
	if (rmtctl_vendor_index >= 0) {
		if (vendor == rmtctl_vendors[rmtctl_vendor_index].vendor_code && 
				rmtctl_vendors[rmtctl_vendor_index].signal_type == rmtctl_signal_type) {
			scancode = rmtctl_vendors[rmtctl_vendor_index].key_codes[key[2]];

			/* Check key combination */
			if (rmtctl_vendors[rmtctl_vendor_index].check_comb_keys)
				rmtctl_vendors[rmtctl_vendor_index].check_comb_keys(&vendor);
		}
		else
			return IRQ_HANDLED;
	}
	else {
		for (i = 0; i < ARRAY_SIZE(rmtctl_vendors); i++) {
			// Some remoter has combinate-keys, key 'shift', 'ctrl' and 'alt' reported via 
			// low byte of vendor code, like MDK079
			if ((vendor& rmtctl_vendors[i].vendor_code) == rmtctl_vendors[i].vendor_code && 
					rmtctl_vendors[i].signal_type == rmtctl_signal_type) {
				scancode = rmtctl_vendors[i].key_codes[key[2]];

				/* Check key combination */
				if (rmtctl_vendors[i].check_comb_keys)
					rmtctl_vendors[i].check_comb_keys(&vendor);
				break;
			}
		}
	}
	
	/* Check 'IR code repeat' flag. */
	repeat = status & 0x10;
	
	if ((status & 0x2) || (scancode == KEY_RESERVED)) {
		/* Ignore repeated or reserved keys. */
	}
	else if (rmtctl_signal_type == CIR_TYPE_NEC) {
		rmtctl_report_event(scancode, repeat);
 	}
	else {
		rmtctl_report_event_without_hwrepeat(scancode);
	}

	return IRQ_HANDLED;
}

static void rmtctl_hw_suspend(void)
{
	unsigned int wakeup_code;
	int i;

	if (rmtctl_vendor_index >= 0)
		i = rmtctl_vendor_index;
	else {
		for (i = 0; i < ARRAY_SIZE(rmtctl_vendors); i++)
			if (rmtctl_signal_type == rmtctl_vendors[i].signal_type)
				break;

		if (i == ARRAY_SIZE(rmtctl_vendors))
			goto out;
	}
	
	wakeup_code = 
		((unsigned char)(~rmtctl_vendors[i].wakeup_code) << 24) | 
		((unsigned char)(rmtctl_vendors[i].wakeup_code) << 16 ) |
		((unsigned char)(rmtctl_vendors[i].vendor_code & 0xff) << 8) |
		((unsigned char)((rmtctl_vendors[i].vendor_code) >> 8) << 0 );
	REG32_VAL(WAKEUP_CMD1(0)) = wakeup_code;
	REG32_VAL(WAKEUP_CMD1(1)) = 0x0;
	REG32_VAL(WAKEUP_CMD1(2)) = 0x0;
	REG32_VAL(WAKEUP_CMD1(3)) = 0x0;
	REG32_VAL(WAKEUP_CMD1(4)) = 0x0;

	for (i = i+1; i < ARRAY_SIZE(rmtctl_vendors); i++)
		if (rmtctl_signal_type == rmtctl_vendors[i].signal_type)
			break;

	if (i == ARRAY_SIZE(rmtctl_vendors))
		goto out;

	wakeup_code = 
		((unsigned char)(~rmtctl_vendors[i].wakeup_code) << 24) | 
		((unsigned char)(rmtctl_vendors[i].wakeup_code) << 16 ) |
		((unsigned char)(rmtctl_vendors[i].vendor_code & 0xff) << 8) |
		((unsigned char)((rmtctl_vendors[i].vendor_code) >> 8) << 0 );
	REG32_VAL(WAKEUP_CMD2(0)) = wakeup_code;
	REG32_VAL(WAKEUP_CMD2(1)) = 0x0;
	REG32_VAL(WAKEUP_CMD2(2)) = 0x0;
	REG32_VAL(WAKEUP_CMD2(3)) = 0x0;
	REG32_VAL(WAKEUP_CMD2(4)) = 0x0;

out:
	REG32_VAL(WAKEUP_CTRL) = 0x101;
}

static void rmtctl_hw_init(void)
{
	unsigned int st;
	int i;

	// select CIRIN pin
	GPIO_PIN_SHARING_SEL_4BYTE_VAL &= ~BIT25;

	/* Turn off CIR SW reset. */
	REG32_VAL(IRSWRST) = 1;
	REG32_VAL(IRSWRST) = 0;

	for (i = 0; 
			i < ARRAY_SIZE(rmtctl_parameters[rmtctl_signal_type].parameter);
			i++) {
		REG32_VAL(PARAMETER(i)) = 
				rmtctl_parameters[rmtctl_signal_type].parameter[i];
	}

	if (rmtctl_signal_type == CIR_TYPE_NEC || 
			rmtctl_signal_type == CIR_TYPE_TOSHIBA) {
		REG32_VAL(NEC_REPEAT_TIME_OUT_CTRL) = 0x1;
		REG32_VAL(NEC_REPEAT_TIME_OUT_COUNT) = rmtctl_parameters[rmtctl_signal_type].repeat_timeout;
		REG32_VAL(IRCTL)        =  0X100;//NEC repeat key
	}
	else
		REG32_VAL(IRCTL)        =  0;//NEC repeat key

	REG32_VAL(IRCTL) |= (0x0<<16) |(0x1<<25);

	REG32_VAL(INT_MASK_CTRL) = 0x1;
	REG32_VAL(INT_MASK_COUNT) =50*1000000*1/3;//0x47868C0/4;//count for 1 sec 0x47868C0

	/* Set IR remoter vendor type */ 
	/* BIT[0]: IR Circuit Enable. */
	REG32_VAL(IRCTL) |= 0x1; /*  IR_EN */

	/* Read CIR status to clear IR interrupt. */
	st = REG32_VAL(IRSTS);
}

static int rmtctl_probe(struct platform_device *dev)
{
	int nvendor, nkeycode;
	
	/* Register an input device. */
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,10))
	if ((idev = input_allocate_device()) == NULL)
		return -1;
#else
	if ((idev = kmalloc(sizeof(struct input_dev), GFP_KERNEL)) == NULL)
		return -1;
	memset(idev, 0, sizeof(struct input_dev));
	init_input_dev(idev);
#endif

	set_bit(EV_KEY, idev->evbit);

	if (rmtctl_vendor_index >= 0) {
		if (rmtctl_signal_type == rmtctl_vendors[rmtctl_vendor_index].signal_type) {
			for (nkeycode = 0; nkeycode < ARRAY_SIZE(rmtctl_vendors[rmtctl_vendor_index].key_codes); nkeycode++) {
				if (rmtctl_vendors[rmtctl_vendor_index].key_codes[nkeycode]) {
					set_bit(rmtctl_vendors[rmtctl_vendor_index].key_codes[nkeycode], idev->keybit);
				}
			}
		}
	}
	else {
		for (nvendor = 0; nvendor < ARRAY_SIZE(rmtctl_vendors); nvendor++) {
			if (rmtctl_signal_type == rmtctl_vendors[nvendor].signal_type) {
				for (nkeycode = 0; nkeycode < ARRAY_SIZE(rmtctl_vendors[nvendor].key_codes); nkeycode++) {
					if (rmtctl_vendors[nvendor].key_codes[nkeycode]) {
						set_bit(rmtctl_vendors[nvendor].key_codes[nkeycode], idev->keybit);
					}
				}
			}
		}
	}

	idev->name = "rmtctl";
	idev->phys = "rmtctl";
	input_register_device(idev);

	/* Register an ISR */
	request_irq(RC_INT, rmtctl_interrupt, IRQF_SHARED, "rmtctl", idev);

	/* Initial H/W */
	rmtctl_hw_init();

	if (RMTCTL_DEBUG)
		printk("WonderMedia rmtctl driver v0.98 initialized: ok\n");

	init_timer(&rmtctl_timer);
	rmtctl_timer.data = (unsigned long)idev;
	rmtctl_timer.function = rmtctl_timer_handler;

	return 0;
}

static int rmtctl_remove(struct platform_device *dev)
{
	if (RMTCTL_DEBUG)
		printk("rmtctl_remove\n");

	del_timer_sync(&rmtctl_timer);

	free_irq(RC_INT, idev);
	input_unregister_device(idev);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,10))
	input_free_device(idev);
#else
	kfree(idev);
#endif

	return 0;
}

#ifdef CONFIG_PM
static int rmtctl_suspend(struct platform_device *dev, u32 state, u32 level)
{
	del_timer_sync(&rmtctl_timer);
	
	/* Nothing to suspend? */
	rmtctl_hw_init();

	rmtctl_hw_suspend();
	printk("wmt cir suspend  \n");
	
	disable_irq(RC_INT);

	/* Enable rmt wakeup */
	PMWE_VAL |= BIT22;
	
	return 0;
}

static int rmtctl_resume(struct platform_device *dev, u32 level)
{
	volatile  unsigned int regval;
	int i =0 ;
	printk("wmt cir resume \n");

	/* Initial H/W */
	REG32_VAL(WAKEUP_CTRL) &=~ BIT0;

	for (i=0;i<10;i++)
	{
		regval = REG32_VAL(WAKEUP_STS) ;

		if (regval & BIT0){
			REG32_VAL(WAKEUP_STS) |= BIT4;

		}else{
			break;
		}
		msleep_interruptible(5);
	}
	
	regval = REG32_VAL(WAKEUP_STS) ;
	if (regval & BIT0)
		printk(" 1. CIR resume NG  WAKEUP_STS 0x%08x \n",regval);


	rmtctl_hw_init();
	enable_irq(RC_INT);
	return 0;
}
#else
#define rmtctl_suspend NULL
#define rmtctl_resume NULL
#endif

static struct platform_driver  rmtctl_driver = {
	/* 
	 * Platform bus will compare the driver name 
	 * with the platform device name. 
	 */
	.driver.name = "wmt-rmtctl", 
	//.bus = &platform_bus_type,
	//.probe = rmtctl_probe,
	.remove = rmtctl_remove,
	.suspend = rmtctl_suspend,
	.resume = rmtctl_resume
};

static void rmtctl_release(struct device *dev)
{
	/* Nothing to release? */
	if (RMTCTL_DEBUG)
		printk("rmtctl_release\n");
}

static u64 rmtctl_dmamask = 0xffffffff;

static struct platform_device rmtctl_device = {
	/* 
	 * Platform bus will compare the driver name 
	 * with the platform device name. 
	 */
	.name = "wmt-rmtctl",
	.id = 0,
	.dev = { 
		.release = rmtctl_release,
		.dma_mask = &rmtctl_dmamask,
	},
	.num_resources = 0,     /* ARRAY_SIZE(rmtctl_resources), */
	.resource = NULL,       /* rmtctl_resources, */
};

extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);

static int __init rmtctl_init(void)
{
	int ret;
	char buf[64];
	int varlen = sizeof(buf);
	int type;

	if (RMTCTL_DEBUG)
		printk("rmtctl_init\n");

	if (wmt_getsyspara("wmt.io.rmtctl", buf, &varlen) == 0) {
		sscanf(buf, "%d", &type);
		switch (type) {
		case 0:
			rmtctl_signal_type = CIR_TYPE_NEC;
			break;
		case 1:
			rmtctl_signal_type = CIR_TYPE_TOSHIBA;
			break;
		default:
			rmtctl_signal_type = CIR_TYPE_NEC;
			break;
		}
	}
	else {
		//rmtctl_signal_type = CIR_TYPE_NEC;
		return -1;
	}

	if (wmt_getsyspara("wmt.io.rmtctl.index", buf, &varlen) == 0) {
		sscanf(buf, "%d", &rmtctl_vendor_index);
		if (rmtctl_vendor_index >= ARRAY_SIZE(rmtctl_vendors) ||
			rmtctl_vendors[rmtctl_vendor_index].signal_type != rmtctl_signal_type)
			rmtctl_vendor_index = -1;
	}

	if (platform_device_register(&rmtctl_device))//add by jay,for modules support
		return -1;
	ret = platform_driver_probe(&rmtctl_driver, rmtctl_probe);

	return ret;
}

static void __exit rmtctl_exit(void)
{
	if (RMTCTL_DEBUG)
		printk("rmtctl_exit\n");

	(void)platform_driver_unregister(&rmtctl_driver);
	(void)platform_device_unregister(&rmtctl_device);//add by jay,for modules support
}

module_init(rmtctl_init);
module_exit(rmtctl_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("WonderMedia Technologies, Inc.");
MODULE_DESCRIPTION("WMT [romter] driver");

