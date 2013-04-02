/*++
Copyright (c) 2011-2013  WonderMedia Technologies, Inc. All Rights Reserved.

This PROPRIETARY SOFTWARE is the property of WonderMedia
Technologies, Inc. and may contain trade secrets and/or other confidential
information of WonderMedia Technologies, Inc. This file shall not be
disclosed to any third party, in whole or in part, without prior written consent of
WonderMedia.  

THIS PROPRIETARY SOFTWARE AND ANY RELATED
DOCUMENTATION ARE PROVIDED AS IS, WITH ALL FAULTS, AND
WITHOUT WARRANTY OF ANY KIND EITHER EXPRESS OR IMPLIED,
AND WonderMedia TECHNOLOGIES, INC. DISCLAIMS ALL EXPRESS
OR IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE, QUIET ENJOYMENT OR
NON-INFRINGEMENT.  
--*/

/********************************************************************************************
 * Brief: 
 *		Implement the keypad driver including all keypad.
 *	This is only one frame. It doesn't contain the special device.
 ********************************************************************************************/

/*************** Include file ******************************************************************/
#include <linux/module.h>
//#include <linux/input.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/stat.h>
#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/earlysuspend.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/mach-types.h>
#include <mach/hardware.h>

#include "wmt_keypadall.h"
#include "kpad_colrow.h"
//#include "kpad_gpio.h"

/*******************Module variable***********************************************/



/***************Macro const******************************************************************/
#define KEYPADALL_PROC_NAME		"kpad_config"

#define WMT_KPAD_TYPE_COLROW 1
#define WMT_KPAD_TYPE_GPIO   3

#define WMT_KPAD_ITEM_TOTAL  3

/***************Function Macro***************************************************************/

/***************Data Type********************************************************************/


/***************Static Variable**************************************************************/

struct kpadall_dev_set
{
	struct keypadall_device* kpad_dev[WMT_KPAD_ITEM_TOTAL];
	int num;
	char id[WMT_KPAD_DEVID_LEN];	
	struct input_dev* inputdev;
	struct proc_dir_entry* proc;
	int is_dbg; // 0-->don't show debug infor; 1-->show debug infor

	struct timer_list uptimer; // for key up
	struct timer_list wifitimer; // for wifi disable/enable key
	struct timer_list holdtimer; // for hold key

	struct keypadall_device* currdev; // The current device to report key
	struct keypadall_device* holddev; // The devcie to report hold key
	struct keypadall_device* wifidev; // The device to report wifi disabel key

	int iskeyhold; // 1--hold state, 0 -- normal state
	int iswifibtn; // 1--wifi disable, 0--wifi enable
	int isnorkeydown; // 1--normal key down,0--normal key up
	int hold_support; // 0--don't support hold key;1--support hold key.
	int wifibtn_support; // 0--don't support wifi disable/enable key.


};


struct kpadall_dev_set l_kpad_devset = {
	.num = 0,
	.is_dbg = 0,
	.iskeyhold = 0,
	.iswifibtn = 0,
	.isnorkeydown = 0,
	.hold_support = 0,
	.wifibtn_support = 0,
};
static struct early_suspend kpad_early_suspend;

/***************Function declare*************************************************************/
extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);
static int get_onefiled_str(char** ptr, char* fieldstr, int maxlen);
static void release_parse_resource(struct kpadall_dev_set* kpad_devset);
static int create_specialkpad(void);
static int kpadall_hw_init(void);
static int kpadall_enable_int(void);


/***************Function implement***********************************************************/

//return 0--error; positive--the respective long key
unsigned int kpadall_normalkey_2_longkey(unsigned int key)
{
	int i;

	if (KEY_RESERVED == key)
	{
		return 0;
	}
	for (i = 0; i < l_kpad_devset.currdev->longkeynum; i++)
	{
		if (key == l_kpad_devset.currdev->longpressdata[i].normalkey)
		{
			return l_kpad_devset.currdev->longpressdata[i].longkey;
		}		
	}
	return 0;
}

int kpadall_isrundbg(void)
{
	return l_kpad_devset.is_dbg;
}

//return:1--Now can handle down key, 0--now can't handle key down
int kpadall_if_hand_key(void)
{
	if (l_kpad_devset.iskeyhold)
	{
		return 0;
	}
	if (l_kpad_devset.isnorkeydown)
	{
		return 0;
	}
	return 1;
}

//return:1--support hold key,0--don't support it.
int kpadall_support_hold(void)
{
	return (l_kpad_devset.hold_support? 1: 0);
}

//return:1--support wifit button,0--don't suport it.
int kpadall_support_wifibtn(void)
{
	return (l_kpad_devset.wifibtn_support? 1: 0);
}

void kpadall_set_wifibtndown(int down)
{
	l_kpad_devset.iswifibtn = down ? 1:0;
}

void kpadall_set_norkeydown(int down)
{
	l_kpad_devset.isnorkeydown = down ? 1:0;
}

void kpadall_set_holddown(int down)
{
	l_kpad_devset.iskeyhold = down ? 1:0;
}

/*
int kpadall_enable_int(void* param)
{
	int i = 0;
	struct keypadall_device* pkpadalldev = (struct keypadall_device*)param;

	while(pkpadalldev->irq[i]) 
	{
		enable_irq(pkpadalldev->irq[i]);
		i++;
	}
	return 0;
}

int kpadall_disable_int(void* param)
{
	int i = 0;
	struct keypadall_device* pkpadalldev = (struct keypadall_device*)param;

	while(pkpadalldev->irq[i]) 
	{
		disable_irq(pkpadalldev->irq[i]);
		i++;
	}
	return 0;
}
*/

//int kpadall_report_key(unsigned int key, void* param)
// up: 1 --down, 0 --up
int kpadall_report_key(unsigned int key, int up)
{
	//struct keypadall_device* pkpadalldev = (struct keypadall_device*)param;

	if (key != KEY_RESERVED)
	{
		input_report_key(l_kpad_devset.inputdev, key, up);
		input_sync(l_kpad_devset.inputdev);
	}
	return 0;
}

// return:
//    0 -- doesn't handle long press
//    positive -- the respective long preesed keycode
int kpadall_islongkey(unsigned int keycode)
{
	int i = 0;

	for (i = 0; i < l_kpad_devset.currdev->longkeynum; i++)
	{
		if (keycode == l_kpad_devset.currdev->longpressdata[i].normalkey)
		{
			return l_kpad_devset.currdev->longpressdata[i].longkey;
		}
	};
	return 0;
}

unsigned int kpadall_getholdinputkey(void)
{
	return HOLD_INPUT_KEY;
}

unsigned int kpadall_getunholdinputkey(void)
{
	return UNHOLD_INPPUT_KEY;
}

void kpadall_set_current_dev(struct keypadall_device* kpad_dev)
{
	l_kpad_devset.currdev= kpad_dev;
}

struct keypadall_device* kpadall_get_current_dev(void)
{
	return l_kpad_devset.currdev;
}

/// For key up
static void kpadall_uptimer_timeout(unsigned long timeout)
{
	dbg("enter...\n");
	if (l_kpad_devset.currdev->do_up)
	{
		l_kpad_devset.currdev->do_up();
		dbg("end...\n");
	}
}

int kpadall_init_up(void)
{
	//struct keypadall_device* pkpadalldev = (struct keypadall_device*)param;

	setup_timer(&l_kpad_devset.uptimer, kpadall_uptimer_timeout, 0);
	return 0;
}

int kpadall_remove_up(void)
{
	//struct keypadall_device* pkpadalldev = (struct keypadall_device*)param;

	del_timer(&l_kpad_devset.uptimer);
	return 0;
}

int kpadall_handle_up(void)
{
	// now using timer
	//struct keypadall_device* pkpadalldev = (struct keypadall_device*)param;
dbg("begin handle up:%d\n",l_kpad_devset.currdev->up_timeout);
	mod_timer(&l_kpad_devset.uptimer, jiffies +msecs_to_jiffies(l_kpad_devset.currdev->up_timeout));
	return 0;
}

// for hold key
void kpadall_rpt_hold_stat(void)
{
	if (!l_kpad_devset.holddev->hold_is_up())
	{
		kpadall_report_key(HOLD_INPUT_KEY, 1);
		kpadall_report_key(HOLD_INPUT_KEY, 0);
		dbg("report HOLD_INPUT_KEY(%x)!\n", HOLD_INPUT_KEY);
		kpadall_set_holddown(1);
		kpadall_handle_hold();
	} else {
		kpadall_report_key(UNHOLD_INPPUT_KEY, 1);
		kpadall_report_key(UNHOLD_INPPUT_KEY, 0);
		dbg("report UNHOLD_INPPUT_KEY(%x)!\n", UNHOLD_INPPUT_KEY);
		kpadall_set_holddown(0);
		if (l_kpad_devset.holddev->hold_up_end != NULL)
		{
			l_kpad_devset.holddev->hold_up_end();
		}
	}
}


static void kpadall_holdtimer_timeout(unsigned long timeout)
{
	// if enable then report unhold key
	// else continuouslty waiting
	if (!l_kpad_devset.holddev->hold_is_up())
	{
		kpadall_handle_hold();
	} else {
		kpadall_report_key(UNHOLD_INPPUT_KEY, 1);
		kpadall_report_key(UNHOLD_INPPUT_KEY, 0);
		dbg("report UNHOLD_INPPUT_KEY(%x)!\n", UNHOLD_INPPUT_KEY);
		kpadall_set_holddown(0);
		if (l_kpad_devset.holddev->hold_up_end != NULL)
		{
			l_kpad_devset.holddev->hold_up_end();
		}
	}
}


int kpadall_init_hold(void)
{
	setup_timer(&l_kpad_devset.holdtimer, kpadall_holdtimer_timeout, 0);
	return 0;
}

int kpadall_remove_hold(void)
{

	del_timer(&l_kpad_devset.holdtimer);
	return 0;
}

int kpadall_handle_hold(void)
{
	mod_timer(&l_kpad_devset.holdtimer, jiffies + msecs_to_jiffies(l_kpad_devset.holddev->hold_timeout));
	return 0;
}

// for wifi button
void kpadall_rpt_wifibtn_stat(void)
{
	if (!l_kpad_devset.wifidev->wifibtn_is_up())
	{
		kpadall_report_key(DISABLE_WIFI_KEY, 1);
		kpadall_report_key(DISABLE_WIFI_KEY, 0);
		dbg("report DISABLE_WIFI_KEY(%x)!\n", DISABLE_WIFI_KEY);
		kpadall_set_wifibtndown(1);
		kpadall_handle_wifibtn();
	} else {
		kpadall_report_key(ENABLE_WIFI_KEY, 1);
		kpadall_report_key(ENABLE_WIFI_KEY, 0);
		dbg("report ENABLE_WIFI_KEY(%x)!\n", ENABLE_WIFI_KEY);
		kpadall_set_wifibtndown(0);
	}
}

static void kpadall_wifitimer_timeout(unsigned long timeout)
{
	// if enable then report ENABLE_WIFI_KEY
	// else continuouslty waiting
	if (!l_kpad_devset.wifidev->wifibtn_is_up())
	{
		kpadall_handle_wifibtn();
	} else {
		kpadall_report_key(ENABLE_WIFI_KEY, 1);
		kpadall_report_key(ENABLE_WIFI_KEY, 0);
		dbg("report ENABLE_WIFI_KEY(%x)!\n", ENABLE_WIFI_KEY);
		kpadall_set_wifibtndown(0);
	}
}

int kpadall_init_wifibtn(void)
{
	//struct keypadall_device* pkpadalldev = (struct keypadall_device*)param;

	setup_timer(&l_kpad_devset.wifitimer, kpadall_wifitimer_timeout, 0);
	return 0;
}

int kpadall_remove_wifibtn(void)
{
	//struct keypadall_device* pkpadalldev = (struct keypadall_device*)param;

	del_timer(&l_kpad_devset.wifitimer);
	return 0;
}

int kpadall_handle_wifibtn(void)
{
	// now using timer
	//struct keypadall_device* pkpadalldev = (struct keypadall_device*)param;

	mod_timer(&l_kpad_devset.wifitimer, jiffies + msecs_to_jiffies(l_kpad_devset.wifidev->wifi_timeout));
	return 0;
}

static int kpadall_probe(struct platform_device *platformdev)
{
	dbg("now do nothing.\n");
	return 0;
}

static int kpadall_input_init(void)
{
	int ret = 0;
	int i,j;
	//int keytblsize = 0;
	struct keypadall_device* pkpadalldev = NULL;

	//dbg("begin register input device!!!\n");
	// register the input device
	l_kpad_devset.inputdev = input_allocate_device();
	if (!l_kpad_devset.inputdev)
	{
		errlog("Can't allocate input device!!!\n");
		ret = -1;
		goto err_input_alloc;
	}
	set_bit(EV_KEY, l_kpad_devset.inputdev->evbit);
	//dbg("Set input name and phys!!!\n");
	l_kpad_devset.inputdev->name = KEYPADALL_DEVICE_NAME;
	l_kpad_devset.inputdev->phys = KEYPADALL_DEVICE_NAME;
	// For better view of /proc/bus/input/devices
	l_kpad_devset.inputdev->id.bustype = BUS_VIRTUAL;
	l_kpad_devset.inputdev->id.vendor  = 0;
	l_kpad_devset.inputdev->id.product = 0;
	l_kpad_devset.inputdev->id.version = 0;

	for (i = 0; i < l_kpad_devset.num; i++)
	{
		pkpadalldev = l_kpad_devset.kpad_dev[i];
		//register normal key
		dbg("kpadtype: %s,  keycodenum:%d!!!\n", pkpadalldev->type, pkpadalldev->keycodenum);
		for (j = 0; j < pkpadalldev->keycodenum; j++)
		{
			if (pkpadalldev->keycode[j] != KEY_RESERVED)
			{
				set_bit(pkpadalldev->keycode[j], l_kpad_devset.inputdev->keybit);
				if (HOLD_INPUT_KEY == pkpadalldev->keycode[j])
				{
					set_bit(UNHOLD_INPPUT_KEY, l_kpad_devset.inputdev->keybit);
				}
				if (DISABLE_WIFI_KEY == pkpadalldev->keycode[j])
				{
					set_bit(ENABLE_WIFI_KEY, l_kpad_devset.inputdev->keybit);
				}
			}
			//dbg("key%d\n", i);
		};
		dbg("Has registered keycode:(num=%d)...\n", j);
		// register long pressed key
		for (j = 0; j < pkpadalldev->longkeynum; j++)
		{
			if (pkpadalldev->longpressdata[j].longkey != KEY_RESERVED)
			{
				set_bit(pkpadalldev->longpressdata[j].longkey, l_kpad_devset.inputdev->keybit);
			}
		}
		dbg("Has register longkey(num=%d)...\n", j);

		spin_lock_init(&pkpadalldev->slock);
			
		
	};
	// up handle init
	kpadall_init_up();
	// hold handle init
	if (kpadall_support_hold())
	{
		kpadall_init_hold();
	}
	// wifi button init
	if (kpadall_support_wifibtn())
	{
		kpadall_init_wifibtn();
	}
	
	if (input_register_device(l_kpad_devset.inputdev))
	{
		errlog("Can't register input device!!!\n");
		goto err_input_regsiter;
	}
	
	return 0;

err_input_regsiter:
	// release input device
	input_free_device(l_kpad_devset.inputdev);
err_input_alloc:
	return ret;
}
static int kpadall_remove(struct platform_device *platformdev)
{
	int i = 0;
	
	dbg("remove.....\n");
	for (i=0;i<l_kpad_devset.num;i++)
	{
		if (l_kpad_devset.kpad_dev[i]->remove!=NULL)
		{
			l_kpad_devset.kpad_dev[i]->remove(l_kpad_devset.kpad_dev[i]);
		}
	};
	// wifi btn remove
	if (kpadall_support_wifibtn())
	{
		kpadall_remove_wifibtn();
	}
	// hold key remove
	if (kpadall_support_hold())
	{
		kpadall_remove_hold();
	}
	// up remove
	kpadall_remove_up();
	input_unregister_device(l_kpad_devset.inputdev);
	
	return 0;
}

static int kpadall_suspend(struct platform_device *platformdev, pm_message_t state)
{

	struct keypadall_device* pkpadalldev = NULL;
	int i = 0;

	kpadall_remove_up();
	if (kpadall_support_hold())
	{
		kpadall_remove_hold();
	}
	if (kpadall_support_wifibtn())
	{
		kpadall_remove_wifibtn();
	}
	dbg("keypad to suspend.....\n");
	for (i = 0; i < l_kpad_devset.num; i++)
	{
		pkpadalldev = l_kpad_devset.kpad_dev[i];		
		pkpadalldev->suspend(pkpadalldev);
	};	
	
	return 0;
}

static int kpadall_resume(struct platform_device *platformdev)
{
	struct keypadall_device* pkpadalldev = NULL;
	int i = 0;

	dbg("keypad to resume......\n");
	
	kpadall_init_up();
	
	if (kpadall_support_hold())
	{
		kpadall_init_hold();
	}
	if (kpadall_support_wifibtn())
	{
		kpadall_init_wifibtn();
	}
	for (i = 0; i < l_kpad_devset.num; i++)
	{
		pkpadalldev = l_kpad_devset.kpad_dev[i];
		// Should check the hold state
		// ????
		if (pkpadalldev->resume!=NULL)
		{
			pkpadalldev->resume(pkpadalldev);
		}
	};
	
	// handle hold key state
	if (kpadall_support_hold())
	{
		kpadall_rpt_hold_stat();
	}
	// handle wifibtn state
	if (kpadall_support_wifibtn())
	{
		kpadall_rpt_wifibtn_stat();
	}
	kpadall_set_norkeydown(0);
	kpadall_enable_int();
	return 0;
}

static void kpad_early_suspendf(struct early_suspend *handler)
{
	early_suspend();
}

static void kpad_late_resume(struct early_suspend *handler)
{
	late_resume();
}

static struct platform_driver kpadall_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= KEYPADALL_DEVICE_NAME,
	},
	.probe = &kpadall_probe,
	.remove = &kpadall_remove,
	.suspend = &kpadall_suspend,
	.resume	= &kpadall_resume
};

static struct platform_device *kpadall_platform_device = NULL;

#if 0
static int parse_gpio_keyitem(char** ptr, struct keypadall_device* kpad_dev)
{
	char buf[20];
	int i = 0, j= 0;
	char iopin_type; //'g' 
	int pinnum = 0;
	int key = KEY_RESERVED;
	
	if (ptr == NULL || kpad_dev == NULL)
	{
		errlog(" ptr or kpad_dev is NULL !!!\n");
		return -1;
	}
	if ('\0' == **ptr)
	{
		errlog("No key item is parsed for null string!!!\n");
		return -2;
	}
	memset(buf, 0, sizeof(buf));
	for (i = 0; i < kpad_dev->keycodenum; i++)
	{
		// set the used gpio pin  and get key
		if (get_onefiled_str(ptr, buf, 15)< 0)
		{
			errlog("Error gpiopin_key format!!!\n");
			return -2;
		}
		if ((j = sscanf(buf, "%c%d_%x", &iopin_type, &pinnum, &key)) != 3)
		{
			errlog("error gpio pin or key code(%s)!!!\n", buf);
			return -1;
		}
		if (HOLD_INPUT_KEY == key)
		{
			l_kpad_devset.hold_support = 1;
			l_kpad_devset.holddev = kpad_dev;
		}
		if (DISABLE_WIFI_KEY == key)
		{
			l_kpad_devset.wifibtn_support = 1;
			l_kpad_devset.wifidev = kpad_dev;
		}
		if (kpad_gpio_setkey(iopin_type, pinnum, key, i))
		{
			errlog("Error when set colrow key!!!\n");
			return -2;
		}
	};
	kpad_dev->keycode = kpad_gpio_getkeytbl();
	// fill other filed of wmt kpad device
	kpad_gpio_register(kpad_dev);
	return 0;
}

#endif
static int parse_colrow_keyitem(char** ptr, struct keypadall_device* kpad_dev)
{
	char buf[20];
	int i = 0, j= 0;
	char iopin_type; //'c' or 'r'
	int pinnum = 0;
	int key = KEY_RESERVED;
	
	if (ptr == NULL || kpad_dev == NULL)
	{
		errlog(" ptr or kpad_dev is NULL !!!\n");
		return -1;
	}
	if ('\0' == **ptr)
	{
		errlog("No key item is parsed for null string!!!\n");
		return -2;
	}
	memset(buf, 0, sizeof(buf));
	for (i = 0; i < kpad_dev->keycodenum; i++)
	{
		// set the used gpio pin  and get key
		if (get_onefiled_str(ptr, buf, 15)< 0)
		{
			errlog("Error gpiopin_key format!!!\n");
			return -2;
		}
		if ((j = sscanf(buf, "%c%d_%x", &iopin_type, &pinnum, &key)) != 3)
		{
			errlog("error gpio pin or key code(%s)!!!\n", buf);
			return -1;
		}
		if (HOLD_INPUT_KEY == key)
		{
			l_kpad_devset.hold_support = 1;
			l_kpad_devset.holddev = kpad_dev;
		}
		if (DISABLE_WIFI_KEY == key)
		{
			l_kpad_devset.wifibtn_support = 1;
			l_kpad_devset.wifidev = kpad_dev;
		}
		if (kpad_colrow_setkey(iopin_type, pinnum, key, i))
		{
			errlog("Error when set colrow key!!!\n");
			return -2;
		}
	};
	kpad_dev->keycode = kpad_colrow_getkeytbl();
	// fill other filed of wmt kpad device
	kpad_colrow_register(kpad_dev);
	return 0;
}

static int parse_longkey(char** ptr, struct keypadall_device* kpad_dev)
{
	int i = 0, j = 0;
	char buf[11];
	char* oldptr = *ptr;
	
	if (ptr == NULL || kpad_dev == NULL)
	{
		errlog(" ptr or kpad_dev is NULL !!!\n");
		return -1;
	}
	/*if ('\0' == **ptr)
	{
		return 0;
	} */
	// parse longkey num
	memset(buf, 0, sizeof(buf));
	if (get_onefiled_str(ptr, buf, 10)< 0)
	{
		errlog("Error longkey number format!!!\n");
		return -1;
	}
	if ((i = sscanf(buf, "%x", &kpad_dev->longkeynum)) != 1)
	{
		 errlog("Can't find the long key item !!!\n");
		// Probably, it's the other type of item.
		*ptr = oldptr;
		return 0;
	}
	dbg("longkeynum:%d \n", kpad_dev->longkeynum);
	// parse the longkey
	for (j = 0; j < kpad_dev->longkeynum; j++)
	{
		if (get_onefiled_str(ptr, buf, 10)< 0)
		{
			errlog("Error format normal key_longkey!!!\n");
			return -2;
		}
		if ((i = sscanf(buf, "%x_%x:", &kpad_dev->longpressdata[j].normalkey,
								    &kpad_dev->longpressdata[j].longkey)) !=2) 
		{
			errlog("Error to get normal key and long key!!!\n");
			return -3;
		}
		//dbg("longkey str:%s\n", buf);
	}
	return 0;	
}

static int parse_item(char** ptr, struct keypadall_device* kpad_dev)
{
	int type = 0;
	int i = 0;
	char buf[64];

	if (ptr == NULL || kpad_dev == NULL)
	{
		errlog(" ptr or kpad_dev is NULL !!!\n");
		return -1;
	}
	if ('\0' == **ptr)
	{
		errlog("No item is parsed for null string!!!\n");
		return -2;
	}
	// item: type, keynum
	memset(buf, 0, sizeof(buf));
	if (get_onefiled_str(ptr, buf, WMT_KPAD_TYPE_LEN-1)< 0)
	{
		errlog("Error format item type(%s) !!!\n", buf);
		return -2;
	}
	if ((i = sscanf(buf, "%s", kpad_dev->type)) != 1)
	{
		errlog("Error when parsing item type!!!\n");
		return -2;
	}
	if (get_onefiled_str(ptr, buf, 3)< 0)
	{
		errlog("Error format keycodenum(%s) !!!\n", buf);
		return -2;
	}
	if ((i = sscanf(buf, "%d", &(kpad_dev->keycodenum))) != 1)
	{
		errlog("Error when parsing key code number !!!\n");
		return -2;
	}
	memset(buf, 0, sizeof(buf));
	i = sscanf(kpad_dev->type, "%c%c_%d", buf, buf+1, &type);
	if (i != 3)
	{
		errlog("item type format error(%s)!!!\n", kpad_dev->type);
		return -1;
	}
	// parse key item
	switch (type)
	{
		case WMT_KPAD_TYPE_COLROW:			
			if (parse_colrow_keyitem(ptr, kpad_dev))
			{
				errlog("Error when parsing colrow key item!\n");
				return -1;
			}
			break;
		case WMT_KPAD_TYPE_GPIO:
			/*if (parse_gpio_keyitem(ptr, kpad_dev))
			{
				errlog("Error when parsing gpio key item!\n");
				return -1;
			}*/
			break;		
		default:
			errlog("Now the %s kpad isn't supported!!!\n", kpad_dev->type);
			return -1;
			break;
	};
	// parse longkey item
	if (('\0' != **ptr) && parse_longkey(ptr, kpad_dev))
	{
		errlog("Errors when parsing long key!!!\n");
		return -2;
	}
	return 0;
}

// Get one filed of parsing string and move to the next field
// the *ptr shouldn't be NULL and filedstr is bigger enough.
static int get_onefiled_str(char** ptr, char* fieldstr, int maxlen)
{
	int i = 0;

	while (('\0' != **ptr)&&(':' != **ptr))
	{
		fieldstr[i] = **ptr;
		(*ptr)++;
		i++;
		if (i > maxlen)
		{
			errlog("the field is too long!!!\n");
			return -2;
		}
	}
	if (i > 0)
	{
		fieldstr[i] = '\0';
		if (':' == **ptr)
		{
			(*ptr)++; // move to the next filed
		}
	}	
	return ((i>0) ? i : -1);
}

static int parse_kpad_ubootarg(char** ptr)
{
	int i = 0;
	//int itemnum = 0;
	char buf[40];

	// board_id, item num
	if (ptr == NULL)
	{
		errlog(" ptr is NULL !!!\n");
		return -1;
	}
	if ('\0' == **ptr)
	{
		errlog("No item is parsed for null string!!!\n");
		return -2;
	}
	//ID, item num
	dbg("before parsing the str: %s\n", *ptr);
	//i = sscanf(*ptr, "%s:%x:", l_kpad_devset.id, &l_kpad_devset.num);
	if (get_onefiled_str(ptr, l_kpad_devset.id, WMT_KPAD_DEVID_LEN-1) < 0)
	{
		errlog("Error to parse ID !!!\n");
		return -2;
	}
	if (get_onefiled_str(ptr, buf, 5) > 0)
	{
		i = sscanf(buf, "%d", &l_kpad_devset.num);
		if (1 != i)
		{
			errlog("Error to parse item number!!!\n");
			return -2;
		}
	} else
	{
		errlog("Error format of the item number!!!\n");
		return -2;
	}
	
	// parse every item
	for (i = 0; i < l_kpad_devset.num; i++)
	{
		// creat kpad dev		
		l_kpad_devset.kpad_dev[i] = kmalloc(sizeof(struct keypadall_device), GFP_KERNEL);
		memset(l_kpad_devset.kpad_dev[i], 0 ,sizeof(struct keypadall_device));
		spin_lock_init(&l_kpad_devset.kpad_dev[i]->slock);
		if (! l_kpad_devset.kpad_dev[i])
		{
			errlog("Can't alloc memory for kpad device!!!\n");
			return -1;
		}
		// parse the item and fill the kpad device infor
		if (parse_item(ptr, l_kpad_devset.kpad_dev[i]))
		{
			errlog("Error when parsing keyitem !!!\n");
			return -3;
		}
	}
	
	return 0;
}

static void release_parse_resource(struct kpadall_dev_set* kpad_devset)
{
	int i = 0;

	for (i = 0; i < WMT_KPAD_ITEM_TOTAL; i++)
	{
		if (kpad_devset->kpad_dev[i])
		{
			kfree(kpad_devset->kpad_dev[i]);
		}
	}
}

void dump_kpad_devinfo(void)
{
	int i;
	int j;

	klog("===================================================\n");
	klog("kpad id:%s   devnum:%d", l_kpad_devset.id, l_kpad_devset.num);
	for (i = 0; i < l_kpad_devset.num; i++)
	{
		klog("	dev[%d] type:%s\n", i, l_kpad_devset.kpad_dev[i]->type);
		j = 0;
		while (l_kpad_devset.kpad_dev[i]->irq[j] != 0)
		{
			klog("	irq[%d]:%d,", j, l_kpad_devset.kpad_dev[i]->irq[j]);
			j++;
		};
		klog("\n");
		klog("	key number:%d\n", l_kpad_devset.kpad_dev[i]->keycodenum);
		for (j = 0; j < l_kpad_devset.kpad_dev[i]->keycodenum; j++)
		{
			klog("	key%d:%x(%d),", j,
				l_kpad_devset.kpad_dev[i]->keycode[j],
				l_kpad_devset.kpad_dev[i]->keycode[j]);
		};
		klog("\n");
		klog("	longkey num:%d\n", l_kpad_devset.kpad_dev[i]->longkeynum);
		for (j = 0; j < l_kpad_devset.kpad_dev[i]->longkeynum; j++)
		{
			klog("	longkey%d:%x_%x(%d_%d),",j,
				l_kpad_devset.kpad_dev[i]->longpressdata[j].normalkey,
				l_kpad_devset.kpad_dev[i]->longpressdata[j].longkey,
				l_kpad_devset.kpad_dev[i]->longpressdata[j].normalkey,
				l_kpad_devset.kpad_dev[i]->longpressdata[j].longkey);
		};
		
	};
	klog("===================================================\n");
}

/*
 * Brief:
 *		Get the special keypad device by u-boot argument
 * Input:
 *		param -- Now it's dummy.
 * Output:
 *		NONE
 * Return:
 *		the special keypad device.
 * History:
 *		2010-3-19	Created by HangYaqn
 * Author:
 * 	Hang Yan in ShenZhen.
 */
static int create_specialkpad(void)
{
	unsigned char buf[800];
	int varlen = sizeof(buf);
	int ret = 0;
	char* ptr;
	
	// parse the u-boot argument	
	memset(buf, 0, sizeof(buf));
	if (wmt_getsyspara("wmt.sys.keypad", buf, &varlen))
	{
		printk(KERN_ERR "Error when read uboot arg wmt.sys.keypad !!!\n");
		return -1;
	}
	dbg("find uboot arg wmt.sys.keypad=%s !!!!!!!!\n", buf);
	memset(&l_kpad_devset, 0, sizeof(l_kpad_devset));
	if ('\0' != buf[0])
	{
		ptr = buf;
		if ((ret = parse_kpad_ubootarg(&ptr)) != 0)
		{
			errlog("Error when parsing kpad uboot arg!!!\n");
			goto err_parse_ubootarg;
		}
	} 
	dump_kpad_devinfo();
	return 0;

err_parse_ubootarg:
	// release the alloc resource
	release_parse_resource(&l_kpad_devset);
	return ret;
}

static int kpadall_writeproc( struct file   *file,
                           const char    *buffer,
                           unsigned long count,
                           void          *data )
{
	unsigned int value;
	
	if (sscanf(buffer, "isdbg=%d\n", &value))
	{
		l_kpad_devset.is_dbg = value;
	} else if (sscanf(buffer, "key=%d\n", &value))
	{
		klog("kpadall_writeproc key %d\n",value);
		input_report_key(l_kpad_devset.inputdev, value, 1);
		input_report_key(l_kpad_devset.inputdev, value, 0);
		input_sync(l_kpad_devset.inputdev);
		
	} else if (sscanf(buffer, "report_hold_stat=%d\n", &value))
	{
		if ((l_kpad_devset.holddev!= NULL) &&
			(1 == value))
		{
			kpadall_rpt_hold_stat();
			klog("cmd=%d\n", value);
		}
	} else if (sscanf(buffer, "report_wifibtn_stat=%d\n", &value))
	{
		if ((l_kpad_devset.wifidev!= NULL) &&
			(1 == value))
		{
			kpadall_rpt_wifibtn_stat();
			klog("wifibtn cmd=%d\n", value);
		}
	}
	
	return count;
}

static int kpadall_readproc(char *page, char **start, off_t off,
			  int count, int *eof, void *data)
{
	int len = 0;

	len = sprintf(page, "isrundgb=%d\n", l_kpad_devset.is_dbg);
	return len;
}

static int kpadall_hw_init(void)
{
	int i = 0;
	int ret = 0;
	struct keypadall_device* pkpadalldev = NULL;

	for (i = 0; i < l_kpad_devset.num; i++)
	{
		pkpadalldev = l_kpad_devset.kpad_dev[i];
		if (pkpadalldev->probe)
		{
			if ((ret = pkpadalldev->probe(pkpadalldev)) != 0)
			{
				errlog("Probe erro for [%d] !!!\n", i);
				return ret;
			}
		}
		dbg("i=%d\n",i);
	}
	return 0;
}

static int kpadall_enable_int(void)
{
	int i = 0;
	int ret = 0;
	struct keypadall_device* pkpadalldev = NULL;

	for (i = 0; i < l_kpad_devset.num; i++)
	{
		pkpadalldev = l_kpad_devset.kpad_dev[i];
		if (pkpadalldev->enable_int)
		{
			if ((ret = pkpadalldev->enable_int(pkpadalldev)) != 0)
			{
				errlog("enable_int erro for [%d] !!!\n", i);
				return ret;
			}
		}
		dbg("i=%d\n",i);
	};
	return 0;
}
static int __init kpadall_init(void)
{
	int ret = 0;
	int i = 0;

	// create the kpad device object	    
	if (create_specialkpad())
	{
		errlog("Don't load wmt kpad driver for error uboot error!!!\n");
		return -1;
	}
	if ((ret = kpadall_hw_init()) != 0)
	{
		errlog("Don't load wmt kpad driver for error initial !!!\n");
		return  -1;
	}
	if (kpadall_input_init())
	{
		errlog("Don't load wmt kpad driver for kpadall_input_init error!!!\n");
		return -1;
	}
	//kpad_dbg_gpio();
	//ret = -1;
	//return ret;
	//goto dbg_exit;

	// create proc config file
	l_kpad_devset.proc = create_proc_entry(KEYPADALL_PROC_NAME, 0666, NULL/*&proc_root*/);
	if(l_kpad_devset.proc != NULL)
	{
		l_kpad_devset.proc->write_proc = kpadall_writeproc;
		l_kpad_devset.proc->read_proc =  kpadall_readproc;
	}
	
	// register dirver
	ret = platform_driver_register(&kpadall_driver);
	if (ret)
	{
		printk(KERN_ERR "[%s]Can't register kpadall_driver!!!\n", __FUNCTION__);
		return ret;
	}	
	// register device
	kpadall_platform_device = platform_device_alloc(KEYPADALL_DEVICE_NAME, -1);
	if (!kpadall_platform_device)
	{
		ret = -ENOMEM;
		printk(KERN_ERR "[%s]Can't alloc platform device!!!\n", __FUNCTION__);
		goto err_driver_unregister;
	}

	ret = platform_device_add(kpadall_platform_device);
	if (ret)
	{
		printk(KERN_ERR "[%s]Can't add platform device!!!\n", __FUNCTION__);
		goto err_free_device;
	}
	kpad_early_suspend.suspend = kpad_early_suspendf,
	kpad_early_suspend.resume = kpad_late_resume,
	kpad_early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 5; //EARLY_SUSPEND_LEVEL_BLANK_SCREEN
	register_early_suspend(&kpad_early_suspend);

	kpadall_enable_int();
	dbg("platform_device_add ok\n");
	// init kpad device (hw)	
	return 0;
err_free_device:
	platform_device_put(kpadall_platform_device);
err_driver_unregister:
	remove_proc_entry(KEYPADALL_PROC_NAME, NULL);
	platform_driver_unregister(&kpadall_driver);
dbg_exit:
	release_parse_resource(&l_kpad_devset);
	return ret;

}

static void __exit kpadall_exit(void)
{
	unregister_early_suspend(&kpad_early_suspend);
	platform_device_unregister(kpadall_platform_device);
	platform_driver_unregister(&kpadall_driver);
	// remove proc config file
	remove_proc_entry(KEYPADALL_PROC_NAME, NULL);
	release_parse_resource(&l_kpad_devset);
	
}

module_init(kpadall_init);
module_exit(kpadall_exit);

MODULE_DESCRIPTION("WMT integrated keypad driver");
MODULE_LICENSE("GPL");



