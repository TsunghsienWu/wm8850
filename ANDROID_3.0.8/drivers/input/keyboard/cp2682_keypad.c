/*++ 
 * linux/drivers/input/keyboard/cp2682.c
 * WonderMedia input touchkey control driver
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
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/slab.h> 
#include <linux/proc_fs.h>
#include <mach/hardware.h>
#include <mach/wmt_gpio.h>
#include <mach/gpio-cs.h>

#define CP2682_REG_DEVID       0x00
#define CP2682_REG_SYS_RST     0x01
#define CP2682_REG_INT         0x02
#define CP2682_REG_INT_EN      0x03
#define CP2682_REG_GPIO_CFG    0x04
#define CP2682_REG_GPIO_CTRL   0x05
#define CP2682_REG_GPIO_DATA   0x06
#define CP2682_REG_KEY_STATUS  0x07
#define CP2682_REG_KEY_INT     0x10
#define CP2682_REG_KEY_EN      0x11
#define CP2682_REG_SYS_CTRL0   0x12
#define CP2682_REG_SYS_CTRL1   0x13
#define CP2682_REG_KDC         0x16
#define CP2682_REG_PERIOD      0x17

#define CP2682_MASK_RST_DIGITAL  (1 << 1)
#define CP2682_MASK_OSC_EN       (1 << 0)
#define CP2682_MASK_INIT         (1 << 3)
#define CP2682_MASK_ASSKEY       (1 << 1)
#define CP2682_MASK_RAWKEY       (1 << 0)
#define CP2682_MASK_GPIO1        (1 << 1)
#define CP2682_MASK_GPIO0        (1 << 0)
#define CP2682_MASK_ASS          (0xff00)
#define CP2682_MASK_RAW          (0xff)
#define CP2682_MASK_IDLE_EN      (1 << 1)
#define CP2682_MASK_BUZ_FRO      (0x3c)
#define CP2682_MASK_BUZ_EN       (1 << 1)
#define CP2682_MASK_LED          (1 << 0)

#define CP2682_DEVID 0x2682

static unsigned int cp2682_key_codes[8] = {
	[0] = KEY_ENTER,
	[1] = KEY_MENU,
	[2] = KEY_HOME,
	[3] = KEY_RIGHT,
	[4] = KEY_BACK,
	[5] = KEY_UP,
	[6] = KEY_DOWN,
	[7] = KEY_LEFT,
};

struct cp2682 {
	void *control_data;
	struct device *dev;
	int (*reg_read)(struct cp2682 *cp2682, u8 reg, u16 *val);
	int (*reg_write)(struct cp2682 *cp2682, u8 reg, u16 val);
	void (*regs_dump)(struct cp2682 *cp2682);

	int gpio_irq;
	int int_gpidx;
	int int_gpbit;
	int int_type;

	struct input_dev *idev;
	struct timer_list timer;
	struct work_struct work;
	struct proc_dir_entry *proc;
};

static void cp2682_do_work(struct work_struct *work)
{
	u16 val;
	u8 asskey;
	int order = 0;
	struct cp2682 *priv = container_of(work, struct cp2682, work);
	
	priv->reg_read(priv, CP2682_REG_KEY_STATUS, &val);
	printk("key status = 0x%x\n", val);
	
	if (val != 0) {
		asskey = (u8)(val >> 8);
		while (asskey >>= 1)
			order++;

		input_event(priv->idev, EV_KEY, cp2682_key_codes[order], 1);
		input_sync(priv->idev);
		input_event(priv->idev, EV_KEY, cp2682_key_codes[order], 0);
		input_sync(priv->idev);
	}

	priv->reg_read(priv, CP2682_REG_INT, &val);
	priv->reg_read(priv, CP2682_REG_KEY_INT, &val);
	gpio_enable_irq(priv->int_gpidx, priv->int_gpbit);
}

static irqreturn_t cp2682_isr(int irq, void *dev_id)
{
	struct cp2682 *priv = dev_id;

	//printk("cp2682_isr enter\n");

	if (!gpio_irq_state(priv->int_gpidx, priv->int_gpbit) ||
		!gpio_irq_isenable(priv->int_gpidx, priv->int_gpbit))
		return IRQ_NONE;

	gpio_disable_irq(priv->int_gpidx, priv->int_gpbit);
	schedule_work(&priv->work);

	return IRQ_HANDLED;
}

static int cp2682_readproc(char *page, char **start, off_t off,
		int count, int *eof, void *data)
{
	struct cp2682 *priv = data;
	printk("\ncp2682 regs dump:\n");
	priv->regs_dump(priv);
	return 0;
}


static int cp2682_i2c_write(struct cp2682 *cp2682, u8 reg, u16 val)
{
	int ret;
	struct i2c_client *i2c = cp2682->control_data;
	struct i2c_msg xfer[1];
	u8 buf[3];

	/*
	 * [MSG1]: fill the register address data
	 * fill the data Tx buffer
	 */
	xfer[0].addr = i2c->addr;
	xfer[0].len = 3;
	xfer[0].flags = 0 ;
	xfer[0].flags &= ~(I2C_M_RD);
	xfer[0].buf = buf;
	buf[0] = reg; 
	buf[1] = (val>>8) & 0xff;
	buf[2] = val & 0xff;
	ret = i2c_transfer(i2c->adapter, xfer, ARRAY_SIZE(xfer));

	/* i2c_transfer returns number of messages transferred */
	if (ret != ARRAY_SIZE(xfer)) {
		pr_err("cp2682_i2c_write error\n");
		if (ret < 0)
			return ret;
		else
			return -EIO;
	} else
		return 0;
}

static int cp2682_i2c_read(struct cp2682 *cp2682, u8 reg, u16 *val)
{
	int ret;
	struct i2c_client *i2c = cp2682->control_data;
	struct i2c_msg xfer[2];

	/* [MSG1] fill the register address data */
	xfer[0].addr = i2c->addr;
	xfer[0].len = 1;
	xfer[0].flags = 2;	/* Read the register val */
	xfer[0].buf = &reg;
	/* [MSG2] fill the data rx buffer */
	xfer[1].addr = i2c->addr;
	xfer[1].flags = I2C_M_RD;	/* Read the register val */
	xfer[1].len = 2;	/* only n bytes */
	xfer[1].buf = (u8 *)val;
	ret = i2c_transfer(i2c->adapter, xfer, ARRAY_SIZE(xfer));

	/* i2c_transfer returns number of messages transferred */
	if (ret != ARRAY_SIZE(xfer)) {
		pr_err("cp2682_i2c_read error\n");
		if (ret < 0)
			return ret;
		else
			return -EIO;
	} else {
		*val = (*val >> 8) | ((*val & 0xff) << 8);
		return 0;
	}
}

void cp2682_regs_dump(struct cp2682 *cp2682)
{
	int reg, ret;
	u16 val;
	printk(KERN_INFO "\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
	for (reg = 0; reg <= 0x17; reg++) {
		ret = cp2682->reg_read(cp2682, reg, &val);
		if (ret) {
			printk(KERN_INFO "\ncp2682_i2c_read\n");
			goto out;
		}
		printk("reg[%02x]=%04x\n", reg, val);
	}
out:
	printk(KERN_INFO "\n<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n\n");
}


static int __devinit cp2682_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct cp2682 *priv;
	u16 val = 0;
	int i;
	
	if (i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C) == 0) {
		pr_err("cp2682_i2c_probe: can't talk I2C?\n");
		return -EIO;
	}

	priv = kzalloc(sizeof(struct cp2682), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	priv->dev = &i2c->dev;
	priv->control_data = i2c;
	priv->reg_read = cp2682_i2c_read;
	priv->reg_write = cp2682_i2c_write;
	priv->regs_dump = cp2682_regs_dump;

	// INTN connected to GPIO3
	priv->gpio_irq = IRQ_GPIO;
	priv->int_gpidx = GP0;
	priv->int_gpbit = 3;
	priv->int_type = IRQ_FALLING;
	
	i2c_set_clientdata(i2c, priv);
	dev_set_drvdata(priv->dev, priv);

	/* check cp2682 devid */
	priv->reg_read(priv, CP2682_REG_DEVID, &val);
	printk("cp2682 devid = 0x%x\n", val);
	if (val != CP2682_DEVID)
		return -ENODEV;

	/* config SYS_CTRL1: disable buzzer, led off after 10s key off */
	priv->reg_read(priv, CP2682_REG_SYS_CTRL1, &val);
	val &= ~(CP2682_MASK_BUZ_EN | CP2682_MASK_LED);
	priv->reg_write(priv, CP2682_REG_SYS_CTRL1, val);

	/* read int status regs to clear */
	priv->reg_read(priv, CP2682_REG_INT, &val);
	priv->reg_read(priv, CP2682_REG_KEY_INT, &val);

	/* enable s1-s8 keys */
	priv->reg_write(priv, CP2682_REG_KEY_EN, 0xff);

	/* Register an input device. */
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,10))
	if ((priv->idev = input_allocate_device()) == NULL)
		return -1;
#else
	if ((priv->idev = kmalloc(sizeof(struct input_dev), GFP_KERNEL)) == NULL)
		return -1;
	memset(priv->idev, 0, sizeof(struct input_dev));
	init_input_dev(priv->idev);
#endif

	set_bit(EV_KEY, priv->idev->evbit);

	for (i = 0; i < ARRAY_SIZE(cp2682_key_codes); i++) {
		if (cp2682_key_codes[i] != 0)
			set_bit(cp2682_key_codes[i], priv->idev->keybit);
	}

	priv->idev->name = "cp2682_touchkey";
	priv->idev->phys = "cp2682_touchkey";
	input_register_device(priv->idev);

	priv->proc = create_proc_read_entry("cp2682_dumpregs", 0666, NULL, 
			cp2682_readproc, priv);

	/* init work queue */
	INIT_WORK(&priv->work, cp2682_do_work);

	if (request_irq(priv->gpio_irq, cp2682_isr, IRQF_SHARED, "cp2682-touchkey", priv)) {
        printk("cp2682: request IRQ %d failed\n", priv->gpio_irq);
        return -ENODEV;
    }
    
    gpio_enable(priv->int_gpidx, priv->int_gpbit, INPUT);
	//gpio_pull_enable(priv->int_gpidx, priv->int_gpbit, 1);
    gpio_setup_irq(priv->int_gpidx, priv->int_gpbit, priv->int_type);
	gpio_enable_irq(priv->int_gpidx, priv->int_gpbit);

	/* enable asskey interrupt */
	priv->reg_read(priv, CP2682_REG_INT_EN, &val);
	val |= CP2682_MASK_ASSKEY;
	val |= CP2682_MASK_INIT;
	priv->reg_write(priv, CP2682_REG_INT_EN, val);
	
	return 0;
}

static int __devexit cp2682_i2c_remove(struct i2c_client *i2c)
{
	struct cp2682 *priv = i2c_get_clientdata(i2c);

	remove_proc_entry("cp2682_dumpregs", NULL);

	gpio_disable_irq(priv->int_gpidx, priv->int_gpbit);
	cancel_work_sync(&priv->work);
	
	input_unregister_device(priv->idev);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,10))
	input_free_device(priv->idev);
#else
	kfree(priv->idev);
#endif

	dev_set_drvdata(priv->dev, NULL);
    free_irq(priv->gpio_irq, priv);
	kfree(priv);

	return 0;
}

static int cp2682_i2c_suspend(struct i2c_client *i2c, pm_message_t mesg)
{
	struct cp2682 *priv = i2c_get_clientdata(i2c);
	gpio_disable_irq(priv->int_gpidx, priv->int_gpbit);
	return 0;
}

static int cp2682_i2c_resume(struct i2c_client *i2c)
{
	struct cp2682 *priv = i2c_get_clientdata(i2c);
	gpio_enable(priv->int_gpidx, priv->int_gpbit, INPUT);
    gpio_setup_irq(priv->int_gpidx, priv->int_gpbit, priv->int_type);
	return 0;
}

static const struct i2c_device_id cp2682_i2c_id[] = {
	{ "cp2682", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cp2682_i2c_id);

/* corgi i2c codec control layer */
static struct i2c_driver cp2682_i2c_driver = {
	.driver = {
		.name  = "cp2682",
		.owner = THIS_MODULE,
	},
	.probe    = cp2682_i2c_probe,
	.remove   = __devexit_p(cp2682_i2c_remove),
	.suspend   = cp2682_i2c_suspend,
	.resume   = cp2682_i2c_resume,
	.id_table = cp2682_i2c_id,
};

static int __init cp2682_init(void)
{
	int ret = i2c_add_driver(&cp2682_i2c_driver);
	if (ret != 0)
		pr_err("Failed to register cp2682 I2C driver: %d\n", ret);
	return ret;
}
module_init(cp2682_init);

static void __exit cp2682_exit(void)
{
	i2c_del_driver(&cp2682_i2c_driver);
}
module_exit(cp2682_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("WonderMedia Technologies, Inc.");
