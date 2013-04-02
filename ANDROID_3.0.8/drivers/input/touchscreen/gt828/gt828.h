/* 
 * 
 * 2010 - 2012 Goodix Technology.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be a reference 
 * to you, when you are integrating the GOODiX's CTP IC into your system, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
 * General Public License for more details.
 * 
 * Version:1.2
 *        V1.0:2012/05/01,create file.
 *        V1.2:2012/06/08,add some macro define.
 *
 */

#ifndef _LINUX_GOODIX_TOUCH_H
#define	_LINUX_GOODIX_TOUCH_H

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <asm/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <mach/gpio.h>
#include <linux/earlysuspend.h>
#include <mach/gpio-cs.h>
#include <linux/platform_device.h>
#include <linux/irq.h>


#define GOODIX_ADDR 0X5D

#define GOODIX_CFG_PATH     "/lib/firmware/"

#define GT813_DEV               "gt813-touch"
#define GT827_DEV               "gt827-touch"
#define GT828_DEV               "gt828-touch"

enum GTP_DEV_ID{  
    GT813 ,
    GT827_N3849B ,
    GT828_L3816A ,
    GT828_L3765A ,
};

#define GTP_MAX_TOUCH           5
#define GTP_HAVE_TOUCH_KEY      0
//STEP_4(optional):If this project have touch key,Set touch key config.                                    
#if GTP_HAVE_TOUCH_KEY
    #define GTP_KEY_TAB	 {KEY_MENU, KEY_HOME, KEY_SEND}
#endif

//***************************PART3:OTHER define*********************************
#define GTP_DRIVER_VERSION    "V1.2<2012/06/08>"
#define GTP_DEV_NAME          "gt8xx-touch"
#define GTP_POLL_TIME	      10
#define GTP_ADDR_LENGTH       2
#define GTP_CONFIG_LENGTH     112
#define FAIL                  0
#define SUCCESS               1

//Register define
#define GTP_READ_COOR_ADDR    0x0F40
#define GTP_REG_SLEEP         0x0FF2
#define GTP_REG_SENSOR_ID     0x0FF5
#define GTP_REG_CONFIG_DATA   0x0F80
#define GTP_REG_VERSION       0x0F7D

#define RESOLUTION_LOC        71
#define TRIGGER_LOC           66

#define GOODIX_CFG_SDPATH   "/sdcard/_tsc_.cfg"

struct gtp_data {
    int id;
    const u8 *name;

    struct input_dev *input_dev;
    struct work_struct work;
    struct workqueue_struct *workqueue;
    struct early_suspend early_suspend;
    struct kobject *kobj;

    int xresl;
    int yresl;
    int swap;

    int igp_idx;
    int igp_bit;

    int rgp_idx;
    int rgp_bit;

    int penup;
    int earlysus;

    u8 cfg_name[64];
    u8 *cfg_buf;
    int cfg_len;

    int dbg;
};


#define DEV_NAME 				    "wmtts"

extern int wmt_setsyspara(char *varname, unsigned char *varval);
extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);
extern int wmt_i2c_xfer_continue_if_4(struct i2c_msg *msg, unsigned int num,int bus_id);

//#define GTP_DEBUG

#undef dbg
#ifdef GTP_DEBUG
	#define dbg(fmt,args...)  printk("DBG:[%s][%d]:"fmt"\n",__FUNCTION__,__LINE__,##args)
#else
	#define dbg(fmt,args...)  
#endif

#undef dbg_err
#define dbg_err(fmt,args...)  printk("ERR:[%s][%d]:"fmt"\n",__FUNCTION__,__LINE__,##args)

#define BUS_I2C1           1



#endif /* _LINUX_GOODIX_TOUCH_H */
