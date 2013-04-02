/*
 * stk3310.c - stk3310 ALS & Proximity Driver
 *
 * By Intersil Corp
 * Michael DiGioia
 *
 * Based on isl29011.c
 *	by Mike DiGioia <mdigioia@intersil.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/hwmon.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/sysfs.h>
#include <linux/pm_runtime.h>
#include <linux/input-polldev.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/earlysuspend.h>

#include "sensor.h"

/* Insmod parameters */
//I2C_CLIENT_INSMOD_1(stk3310);

#define MODULE_NAME	"stk3310"


struct stk_device {
	struct input_polled_dev* input_poll_devl;
	struct input_polled_dev* input_poll_devp;
	struct i2c_client* client;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend earlysuspend;
#endif

};

static struct stk_device* l_sensorconfig = NULL;
static int l_enable = 1; // 0:don't report data
static int p_enable = 1; // 0:don't report data

static DEFINE_MUTEX(mutex);

static int isl_get_lux_datal(struct i2c_client* client)
{
	__u16 resH, resL;
	resL = i2c_smbus_read_byte_data(client, 0x14);
	resH = i2c_smbus_read_byte_data(client, 0x13);
	if ((resL < 0) || (resH < 0))
	{
		errlog("Error to read lux_data!\n");
		return -1;
	}
	return (resH <<8 | resL) ;//* idev->range / idev->resolution;
}


static int isl_get_lux_datap(struct i2c_client* client)
{
	__u16 resH, resL;
	resL = i2c_smbus_read_byte_data(client, 0x12);
	resH = i2c_smbus_read_byte_data(client, 0x11);
	if ((resL < 0) || (resH < 0))
	{
		errlog("Error to read lux_data!\n");
		return -1;
	}
	//return (resH <<8 | resL) ;//* idev->range / idev->resolution;
	if(resH>0)
		return 0;
	else
		return 6;
}

//#define PXM 0
static int isl_set_default_config(struct i2c_client *client)
{
	int ret=0;
	unsigned char regval;
//#if PXM
	ret = i2c_smbus_write_byte_data(client, 0, (1 << 1));
	if(p_enable)
	{
		regval = i2c_smbus_read_byte_data(l_sensorconfig->client, 0);
		regval |= (1 << 0);
		i2c_smbus_write_byte_data(l_sensorconfig->client, 0, regval);
	}
	//ret = i2c_smbus_write_byte_data(client, 0, (1 << 0));
//#else
//#endif
	if (ret < 0)
		return -EINVAL;
	return 0;
}

/* Return 0 if detection is successful, -ENODEV otherwise */
static int stk3310_detect(struct i2c_client *client/*, int kind,
                          struct i2c_board_info *info*/)
{
	int device;

	device= i2c_smbus_read_byte_data(client, 0x3e);
	if(0x13==device)
	{
		printk(KERN_ALERT "stk3310 detected OK\n");
        return 0;
	}
	else
		return -1;
}

int isl_input_open(struct input_dev* input)
{
	return 0;
}

void isl_input_close(struct input_dev* input)
{
}

static void isl_input_lux_poll_l(struct input_polled_dev *dev)
{
	struct stk_device* idev = dev->private;
	struct input_dev* input = idev->input_poll_devl->input;
	struct i2c_client* client = idev->client;
	if (l_enable != 0)
	{
		mutex_lock(&mutex);
		//printk(KERN_ALERT "by flashchen val is %x",val); 
		input_report_abs(input, ABS_MISC, isl_get_lux_datal(client));
		input_sync(input);
		mutex_unlock(&mutex);
	}
}

static void isl_input_lux_poll_p(struct input_polled_dev *dev)
{
	struct stk_device* idev = dev->private;
	struct input_dev* input = idev->input_poll_devp->input;
	struct i2c_client* client = idev->client;
	if (p_enable != 0)
	{
		mutex_lock(&mutex);
		//printk(KERN_ALERT "by flashchen val is %x",val); 
		input_report_abs(input, ABS_MISC, isl_get_lux_datap(client));
		input_sync(input);
		mutex_unlock(&mutex);
	}
}

static struct i2c_device_id stk3310_id[] = {
	{"stk3310", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, stk3310_id);


static int mmad_open(struct inode *inode, struct file *file)
{
	dbg("Open the l-sensor node...\n");
	return 0;
}

static int mmad_release(struct inode *inode, struct file *file)
{
	dbg("Close the l-sensor node...\n");
	return 0;
}

static ssize_t mmadl_read(struct file *fl, char __user *buf, size_t cnt, loff_t *lf)
{
	int lux_data = 0;
	
	mutex_lock(&mutex);
	lux_data = isl_get_lux_datal(l_sensorconfig->client);
	mutex_unlock(&mutex);
	if (lux_data < 0)
	{
		errlog("Failed to read lux data!\n");
		return -1;
	}
	printk(KERN_ALERT "lux_data is %x\n",lux_data); 
	return 0;
	copy_to_user(buf, &lux_data, sizeof(lux_data));
	return sizeof(lux_data);
}


static ssize_t mmadp_read(struct file *fl, char __user *buf, size_t cnt, loff_t *lf)
{
	int lux_data = 0;
	
	mutex_lock(&mutex);
	lux_data = isl_get_lux_datap(l_sensorconfig->client);
	mutex_unlock(&mutex);
	if (lux_data < 0)
	{
		errlog("Failed to read lux data!\n");
		return -1;
	}
	printk(KERN_ALERT "lux_data is %x\n",lux_data); 
	return 0;
	copy_to_user(buf, &lux_data, sizeof(lux_data));
	return sizeof(lux_data);
}

static long
mmadl_ioctl(/*struct inode *inode,*/ struct file *file, unsigned int cmd,
	   unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	//char rwbuf[5];
	short enable;  //amsr = -1;
	unsigned int uval;

	dbg("l-sensor ioctr...\n");
	//memset(rwbuf, 0, sizeof(rwbuf));
	switch (cmd) {
		case LIGHT_IOCTL_SET_ENABLE:
			// enable/disable sensor
			if (copy_from_user(&enable, argp, sizeof(short)))
			{
				printk(KERN_ERR "Can't get enable flag!!!\n");
				return -EFAULT;
			}
			dbg("enable=%d\n",enable);
			if ((enable >=0) && (enable <=1))
			{
				dbg("driver: disable/enable(%d) gsensor.\n", enable);
				
				//l_sensorconfig.sensor_enable = enable;
				dbg("Should to implement d/e the light sensor!\n");
				l_enable = enable;
				
			} else {
				printk(KERN_ERR "Wrong enable argument in %s !!!\n", __FUNCTION__);
				return -EINVAL;
			}
			break;
		case WMT_IOCTL_SENSOR_GET_DRVID:
#define DRVID 0
			uval = DRVID ;
			if (copy_to_user((unsigned int*)arg, &uval, sizeof(unsigned int)))
			{
				return -EFAULT;
			}
			dbg("stk3310_driver_id:%d\n",uval);
		default:
			break;
	}

	return 0;
}


static long
mmadp_ioctl(/*struct inode *inode,*/ struct file *file, unsigned int cmd,
	   unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	//char rwbuf[5];
	short enable;  //amsr = -1;
	unsigned int uval;
	unsigned char regval;

	dbg("l-sensor ioctr...\n");
	//memset(rwbuf, 0, sizeof(rwbuf));
	switch (cmd) {
		case LIGHT_IOCTL_SET_ENABLE:
			// enable/disable sensor
			if (copy_from_user(&enable, argp, sizeof(short)))
			{
				printk(KERN_ERR "Can't get enable flag!!!\n");
				return -EFAULT;
			}
			dbg("enable=%d\n",enable);
			if ((enable >=0) && (enable <=1))
			{
				dbg("driver: disable/enable(%d) gsensor.\n", enable);
				
				//l_sensorconfig.sensor_enable = enable;
				dbg("Should to implement d/e the light sensor!\n");
				p_enable = enable;
				if(p_enable)
				{
					regval = i2c_smbus_read_byte_data(l_sensorconfig->client, 0);
					regval |= (1 << 0);
					i2c_smbus_write_byte_data(l_sensorconfig->client, 0, regval);
				}
				else
				{
					regval = i2c_smbus_read_byte_data(l_sensorconfig->client, 0);
					regval &= ~(1 << 0);
					i2c_smbus_write_byte_data(l_sensorconfig->client, 0, regval);
				}
				
			} else {
				printk(KERN_ERR "Wrong enable argument in %s !!!\n", __FUNCTION__);
				return -EINVAL;
			}
			break;
		case WMT_IOCTL_SENSOR_GET_DRVID:
#define DRVID 0
			uval = DRVID ;
			if (copy_to_user((unsigned int*)arg, &uval, sizeof(unsigned int)))
			{
				return -EFAULT;
			}
			dbg("stk3310_driver_id:%d\n",uval);
		default:
			break;
	}

	return 0;
}


static struct file_operations mmadl_fops = {
	.owner = THIS_MODULE,
	.open = mmad_open,
	.release = mmad_release,
	.read = mmadl_read,
	.unlocked_ioctl = mmadl_ioctl,
};

static struct miscdevice mmadl_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "lsensor_ctrl",
	.fops = &mmadl_fops,
};

static struct file_operations mmadp_fops = {
	.owner = THIS_MODULE,
	.open = mmad_open,
	.release = mmad_release,
	.read = mmadp_read,
	.unlocked_ioctl = mmadp_ioctl,
};

static struct miscdevice mmadp_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "psensor_ctrl",
	.fops = &mmadp_fops,
};

static void stk3310_early_suspend(struct early_suspend *h)
{
	dbg("start\n");
	mutex_lock(&mutex);
	//pm_runtime_get_sync(dev);
	//isl_set_mod(client, ISL_MOD_POWERDOWN);
	//pm_runtime_put_sync(dev);
	mutex_unlock(&mutex);
	dbg("exit\n");
}

static void stk3310_late_resume(struct early_suspend *h)
{
	struct i2c_client *client = l_sensorconfig->client;

	dbg("start\n");
	mutex_lock(&mutex);
	//pm_runtime_get_sync(dev);
	//isl_set_mod(client, last_mod);
	isl_set_default_config(client);
	//pm_runtime_put_sync(dev);
	mutex_unlock(&mutex);	
	dbg("exit\n");
}


static int
stk3310_probe(struct i2c_client *client/*, const struct i2c_device_id *id*/)
{
	int res=0;

	struct stk_device* idev = kzalloc(sizeof(struct stk_device), GFP_KERNEL);
	if(!idev)
		return -ENOMEM;

	l_sensorconfig = idev;

/* last mod is ALS continuous */
	//pm_runtime_enable(&client->dev);	
	idev->input_poll_devl = input_allocate_polled_device();
	if(!idev->input_poll_devl)
	{
		res = -ENOMEM;
		goto err_input_allocate_device;
	}
	idev->input_poll_devp = input_allocate_polled_device();
	if(!idev->input_poll_devp)
	{
		res = -ENOMEM;
		goto err_input_allocate_device;
	}
	idev->client = client;

	idev->input_poll_devl->private = idev;
	idev->input_poll_devl->poll = isl_input_lux_poll_l;
	idev->input_poll_devl->poll_interval = 100;//50;
	idev->input_poll_devl->input->open = isl_input_open;
	idev->input_poll_devl->input->close = isl_input_close;
	idev->input_poll_devl->input->name = "lsensor_lux";
	idev->input_poll_devl->input->id.bustype = BUS_I2C;
	idev->input_poll_devl->input->dev.parent = &client->dev;

	input_set_drvdata(idev->input_poll_devl->input, idev);
	input_set_capability(idev->input_poll_devl->input, EV_ABS, ABS_MISC);
	input_set_abs_params(idev->input_poll_devl->input, ABS_MISC, 0, 16000, 0, 0);

	idev->input_poll_devp->private = idev;
	idev->input_poll_devp->poll = isl_input_lux_poll_p;
	idev->input_poll_devp->poll_interval = 100;//50;
	idev->input_poll_devp->input->open = isl_input_open;
	idev->input_poll_devp->input->close = isl_input_close;
	idev->input_poll_devp->input->name = "psensor_lux";
	idev->input_poll_devp->input->id.bustype = BUS_I2C;
	idev->input_poll_devp->input->dev.parent = &client->dev;

	input_set_drvdata(idev->input_poll_devp->input, idev);
	input_set_capability(idev->input_poll_devp->input, EV_ABS, ABS_MISC);
	input_set_abs_params(idev->input_poll_devp->input, ABS_MISC, 0, 16000, 0, 0);
	i2c_set_clientdata(client, idev);
	/* set default config after set_clientdata */
	res = isl_set_default_config(client);	
	res = misc_register(&mmadl_device);
	if (res) {
		errlog("mmad_device register failed\n");
		goto err_misc_registerl;
	}
	res = misc_register(&mmadp_device);
	if (res) {
		errlog("mmad_device register failed\n");
		goto err_misc_registerp;
	}
	res = input_register_polled_device(idev->input_poll_devl);
	if(res < 0)
		goto err_input_register_devicel;
	res = input_register_polled_device(idev->input_poll_devp);
	if(res < 0)
		goto err_input_register_devicep;
	// suspend/resume register
#ifdef CONFIG_HAS_EARLYSUSPEND
        idev->earlysuspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
        idev->earlysuspend.suspend = stk3310_early_suspend;
        idev->earlysuspend.resume = stk3310_late_resume;
        register_early_suspend(&(idev->earlysuspend));
#endif

	dbg("stk3310 probe succeed!\n");
	return 0;
err_input_register_devicep:
	input_free_polled_device(idev->input_poll_devp);
err_input_register_devicel:
	input_free_polled_device(idev->input_poll_devl);
err_misc_registerp:
	misc_deregister(&mmadp_device);
err_misc_registerl:
	misc_deregister(&mmadl_device);
err_input_allocate_device:
	//__pm_runtime_disable(&client->dev, false);
	kfree(idev);
	return res;
}

static int stk3310_remove(struct i2c_client *client)
{
	struct stk_device* idev = i2c_get_clientdata(client);

	unregister_early_suspend(&(idev->earlysuspend));
	misc_deregister(&mmadl_device);
	misc_deregister(&mmadp_device);
	input_unregister_polled_device(idev->input_poll_devl);
	input_unregister_polled_device(idev->input_poll_devp);
	input_free_polled_device(idev->input_poll_devl);
	input_free_polled_device(idev->input_poll_devp);
	//__pm_runtime_disable(&client->dev, false);
	kfree(idev);
 printk(KERN_INFO MODULE_NAME ": %s stk3310 remove call, \n", __func__);
	return 0;
}

static int __init sensor_stk3310_init(void)
{
	 printk(KERN_INFO MODULE_NAME ": %s stk3310 init call, \n", __func__);
	/*
	 * Force device to initialize: i2c-15 0x44
	 * If i2c_new_device is not called, even stk3310_detect will not run
	 * TODO: rework to automatically initialize the device
	 */
	//i2c_new_device(i2c_get_adapter(15), &isl_info);
	//return i2c_add_driver(&stk3310_driver);
	if (sensor_i2c_register_device())
	{
		errlog("Can't register light i2c device!\n");
		return -1;
	}
	if (stk3310_detect(sensor_get_i2c_client()))
	{
		errlog("Can't find light sensor stk3310!\n");
		goto detect_fail;
	}
	if(stk3310_probe(sensor_get_i2c_client()))
	{
		errlog("Erro for probe!\n");
		goto detect_fail;
	}
	return 0;

detect_fail:
	sensor_i2c_unregister_device();
	return -1;
}

static void __exit sensor_stk3310_exit(void)
{
 	printk(KERN_INFO MODULE_NAME ": %s stk3310 exit call \n", __func__);
 	stk3310_remove(sensor_get_i2c_client());
 	sensor_i2c_unregister_device();
	//i2c_del_driver(&stk3310_driver);
}

module_init(sensor_stk3310_init);
module_exit(sensor_stk3310_exit);

MODULE_AUTHOR("flash");
MODULE_ALIAS("stk3310 ALS");
MODULE_DESCRIPTION("Stk3310 Driver");
MODULE_LICENSE("GPL v2");

