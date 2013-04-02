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

#ifndef __SENSORDMARD08_H__
#define __SENSORDMARD08_H__
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/proc_fs.h>
#include <linux/input.h>
#include <linux/delay.h>




#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

/*
#ifdef CONFIG_WMT_SENSOR_MC3210
#define GSENSOR_I2C_NAME	"mc32x0"
#define GSENSOR_I2C_ADDR	0x4c
#elif defined CONFIG_WMT_SENSOR_DMARD08
*/
#define SENSOR_I2C_NAME	"stk3310"
#define SENSOR_I2C_ADDR	0x48
/*#elif defined CONFIG_WMT_SENSOR_MMA7660
#define SENSOR_I2C_NAME	"mma7660"
#define SENSOR_I2C_ADDR	0x4c
#elif defined CONFIG_WMT_SENSOR_KXTE9
#define SENSOR_I2C_NAME	"kxte9"
#define SENSOR_I2C_ADDR	0x0f
#endif
*/

#define SENSOR_PROC_NAME "lsensor_config"
#define SENSOR_INPUT_NAME "l-sensor"
#define SENSOR_DEV_NODE "lsensor_ctrl"

#undef dbg
#define dbg(fmt, args...) //printk(KERN_ALERT "[%s]: " fmt, __FUNCTION__ , ## args)

//#define dbg(fmt, args...) if (l_sensorconfig->isdbg) printk(KERN_ALERT "[%s]: " fmt, __FUNCTION__, ## args)


#undef errlog
#undef klog
#define errlog(fmt, args...) printk(KERN_ERR "[%s]: " fmt, __FUNCTION__, ## args)
#define klog(fmt, args...) printk(KERN_ALERT "[%s]: " fmt, __FUNCTION__, ## args)

struct wmt_SENSOR_data{
	// for control
	int int_gpio; //0-3
	int op;
	int samp;
	int xyz_axis[3][2]; // (axis,direction)
	struct proc_dir_entry* sensor_proc;
	struct input_dev *input_dev;
	//struct work_struct work;
	struct delayed_work work; // for polling
	struct workqueue_struct *queue;
	int isdbg;
	int sensor_samp; // 
	int sensor_enable;  // 0 --> disable sensor, 1 --> enable sensor
	int test_pass;
	int offset[3];
	struct i2c_client *client;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend earlysuspend;
#endif

};

///////////////////////// ioctrl cmd ////////////////////////
#define WMTGSENSOR_IOCTL_MAGIC  0x09
#define WMT_LSENSOR_IOCTL_MAGIC  0x10
//#define WMT_IOCTL_SENSOR_CAL_OFFSET  _IOW(WMT_LSENSOR_IOCTL_MAGIC, 0x01, int) //offset calibration
#define LIGHT_IOCTL_SET_ENABLE		 _IOW(WMT_LSENSOR_IOCTL_MAGIC, 0x01, short)
//#define ECS_IOCTL_APP_SET_DELAY		 _IOW(WMT_LSENSOR_IOCTL_MAGIC, 0x03, short)
#define WMT_IOCTL_SENSOR_GET_DRVID	 _IOW(WMTGSENSOR_IOCTL_MAGIC, 0x04, unsigned int)

#define ISL29023_DRVID 0

/* Function prototypes */
extern int sensor_i2c_register_device (void);
extern void sensor_i2c_unregister_device(void);
extern struct i2c_client* sensor_get_i2c_client(void);

#endif
