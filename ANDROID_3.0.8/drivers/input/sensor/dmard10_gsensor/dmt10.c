/*
 * @file drivers/misc/dmt10.c
 * @brief DMT g-sensor Linux device driver
 * @author Domintech Technology Co., Ltd (http://www.domintech.com.tw)
 * @version 1.00
 *
 * @section LICENSE
 *
 *  Copyright 2012 Domintech Technology Co., Ltd
 *
 * 	This software is licensed under the terms of the GNU General Public
 * 	License version 2, as published by the Free Software Foundation, and
 * 	may be copied, distributed, and modified under those terms.
 *
 * 	This program is distributed in the hope that it will be useful,
 * 	but WITHOUT ANY WARRANTY; without even the implied warranty of
 * 	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * 	GNU General Public License for more details.
 *
 *  V1.00	D10 First Release	date 2012/9/21
 */
#include <linux/module.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/earlysuspend.h>
#include <linux/wakelock.h>
#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/clk.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/string.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>

#include "dmt10.h"
#include "gsensor.h"

//////////////////////////////////////////////////////////
static struct wmt_gsensor_data l_sensorconfig = {
	.op = 0,
	.int_gpio = 3,
	.samp = 5,
	.xyz_axis = {
		{ABS_X, -1},
		{ABS_Y, 1},
		{ABS_Z, -1},
		},
	.sensor_proc = NULL,
	.isdbg = 0,
	.sensor_samp = 10,  // 1 sample/second
	.sensor_enable = 1, // enable sensor
	.test_pass = 0, // for test program
	.offset={0,0,0},
};

////////////////////////////////////////////////////////////


//#define DMT_DEBUG_DATA
static unsigned int interval;
void gsensor_write_offset_to_file(void);
void gsensor_read_offset_from_file(void);
char OffsetFileName[] = "/data/misc/dmt/offset.txt";	/* FILE offset.txt */
//char OffsetFileName[] = "/data/misc/offset.txt";	/* FILE offset.txt */
static char const *const ACCELEMETER_CLASS_NAME = "accelemeter";
static char const *const GSENSOR_DEVICE_NAME = "dmt";
static raw_data offset;
static struct dev_data devdata;
static struct class* l_dev_class = NULL;
static struct device *l_device = NULL;


extern int gsensor_i2c_register_device (void);
extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);

//static int device_init(void);
//static void device_exit(void);
static int device_open(struct inode*, struct file*);
//static ssize_t device_write(struct file*, const char*, size_t, loff_t*);
//static ssize_t device_read(struct file*, char*, size_t, loff_t*);
static long device_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static int device_close(struct inode*, struct file*);

void device_i2c_read_xyz(struct i2c_client *client, s16 *xyz);
static int device_i2c_rxdata(struct i2c_client *client, unsigned char *buffer, int length);
static int device_i2c_txdata(struct i2c_client *client, unsigned char *txData, int length);

static int DMT_GetOpenStatus(void)
{	
	dmtprintk(KERN_INFO "start active=%d\n",devdata.active.counter);
	wait_event_interruptible(devdata.open_wq, (atomic_read(&devdata.active) != 0));
	return 0;
}

static int DMT_GetCloseStatus(void)
{
	dmtprintk(KERN_INFO "start active=%d\n",devdata.active.counter);
	wait_event_interruptible(devdata.open_wq, (atomic_read(&devdata.active) <= 0));
	return 0;
}

static void DMT_sysfs_update_active_status(int en)
{
	unsigned long dmt_delay;
	if(en)
	{
		dmt_delay=msecs_to_jiffies(atomic_read(&devdata.delay));
		if(dmt_delay<1)
			dmt_delay=1;

		dmtprintk(KERN_INFO "schedule_delayed_work start with delay time=%lu\n",dmt_delay);
		schedule_delayed_work(&devdata.delaywork,dmt_delay);
	}
	else 
		cancel_delayed_work_sync(&devdata.delaywork);
}

static bool get_value_as_int(char const *buf, size_t size, int *value)
{
	long tmp;
	if (size == 0)
		return false;

	/* maybe text format value */
	if ((buf[0] == '0') && (size > 1)) {
		if ((buf[1] == 'x') || (buf[1] == 'X')) {
			/* hexadecimal format */
			if (0 != strict_strtol(buf, 16, &tmp))
				return false;
		} else {
			/* octal format */
			if (0 != strict_strtol(buf, 8, &tmp))
				return false;
		}
	} else {
		/* decimal format */
		if (0 != strict_strtol(buf, 10, &tmp))
			return false;
	}

	if (tmp > INT_MAX)
		return false;

	*value = tmp;
	return true;
}
static bool get_value_as_int64(char const *buf, size_t size, long long *value)
{
	long long tmp;

	if (size == 0)
		return false;

	/* maybe text format value */
	if ((buf[0] == '0') && (size > 1)) {
		if ((buf[1] == 'x') || (buf[1] == 'X')) {
			/* hexadecimal format */
			if (0 != strict_strtoll(buf, 16, &tmp))
				return false;
		} else {
			/* octal format */
			if (0 != strict_strtoll(buf, 8, &tmp))
				return false;
		}
	} else {
		/* decimal format */
		if (0 != strict_strtoll(buf, 10, &tmp))
			return false;
	}

	if (tmp > LLONG_MAX)
		return false;

	*value = tmp;
	return true;
}
static ssize_t DMT_enable_acc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char str[2][16]={"ACC enable OFF","ACC enable ON"};
	int flag;
	flag=atomic_read(&devdata.enable); 
	
	return sprintf(buf, "%s\n", str[flag]);
}

static ssize_t DMT_enable_acc_store( struct device *dev, struct device_attribute *attr, char const *buf, size_t count)
{
	int en;
	dbg("buf=%x %x\n",buf[0],buf[1]);
	if (false == get_value_as_int(buf, count, &en))
		return -EINVAL;

	en = en ? 1 : 0;
	atomic_set(&devdata.enable,en);
	DMT_sysfs_update_active_status(en);
	return 1;
}

static ssize_t DMT_delay_acc_show( struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&devdata.delay));
}

static ssize_t DMT_delay_acc_store( struct device *dev, struct device_attribute *attr,char const *buf, size_t count)
{
	long long data;
	if (false == get_value_as_int64(buf, count, &data))
		return -EINVAL;
	atomic_set(&devdata.delay, (unsigned int) data);
	dbg("Driver attribute set delay =%lld\n",data);
	return count;
}

static struct device_attribute DMT_attributes[] = {
	__ATTR(enable_acc, 0666, DMT_enable_acc_show, DMT_enable_acc_store),
	__ATTR(delay_acc,  0666, DMT_delay_acc_show,  DMT_delay_acc_store),
	__ATTR_NULL,
};

static int create_device_attributes(struct device *dev,	struct device_attribute *attrs)
{
	int i;
	int err = 0;
	for (i = 0 ; NULL != attrs[i].attr.name ; ++i) 
	{
		err = device_create_file(dev, &attrs[i]);
		if (0 != err)
			break;
	}

	if (0 != err) 
	{
		for (; i >= 0 ; --i)
			device_remove_file(dev, &attrs[i]);
	}
	return err;
}

static int remove_device_attributes(struct device *dev,	struct device_attribute *attrs)
{
	int i;
	int err = 0;
	for (i = 0 ; NULL != attrs[i].attr.name ; ++i) 
	{
		device_remove_file(dev, &attrs[i]);
	}

	return 0;
}

int input_init(void)
{
	int err=0;
	devdata.input=input_allocate_device();
	if (!devdata.input)
		return -ENOMEM;
	else
	   dmtprintk(KERN_INFO "input device allocate Success !!\n");
	/* Setup input device */
	set_bit(EV_ABS, devdata.input->evbit);
	/* Accelerometer [-78.5, 78.5]m/s2 in Q16 */
	input_set_abs_params(devdata.input, ABS_X, -5144576, 5144576, 0, 0);
	input_set_abs_params(devdata.input, ABS_Y, -5144576, 5144576, 0, 0);
	input_set_abs_params(devdata.input, ABS_Z, -5144576, 5144576, 0, 0);

	/* Set InputDevice Name */
	devdata.input->name = INPUT_NAME_ACC;

	/* Register */
	err = input_register_device(devdata.input);
	if (err) {
		input_free_device(devdata.input);
		return err;
	}
	atomic_set(&devdata.active, 0);
	dmtprintk(KERN_INFO "in driver ,active=%d\n",devdata.active.counter);

	init_waitqueue_head(&devdata.open_wq);

	return err;
}

/*
int gsensor_read_accel_avg(raw_data *avg_p )
{
   	long xyz_acc[SENSOR_DATA_SIZE];   
  	s16 xyz[SENSOR_DATA_SIZE];
  	int i, j;
	
	//initialize the accumulation buffer
  	for(i = 0; i < SENSOR_DATA_SIZE; ++i) 
		xyz_acc[i] = 0;

	for(i = 0; i < AVG_NUM; i++) 
	{      
		device_i2c_read_xyz(devdata.client, (s16 *)&xyz);
		for(j = 0; j < SENSOR_DATA_SIZE; ++j) 
			xyz_acc[j] += xyz[j];
  	}

	// calculate averages
  	for(i = 0; i < SENSOR_DATA_SIZE; ++i) 
		avg_p->v[i] = (s16) (xyz_acc[i] / AVG_NUM);
		
	if(avg_p->v[2] < 0)
		return CONFIG_GSEN_CALIBRATION_GRAVITY_ON_Z_NEGATIVE;
	else
		return CONFIG_GSEN_CALIBRATION_GRAVITY_ON_Z_POSITIVE;
}
// calc delta offset 
int gsensor_calculate_offset(int gAxis,raw_data avg)
{
	switch(gAxis)
	{
		case CONFIG_GSEN_CALIBRATION_GRAVITY_ON_Z_NEGATIVE:  
			offset.u.x =  avg.u.x ;    
			offset.u.y =  avg.u.y ;
			offset.u.z =  avg.u.z + DEFAULT_SENSITIVITY;
		  break;
		case CONFIG_GSEN_CALIBRATION_GRAVITY_ON_X_POSITIVE:  
			offset.u.x =  avg.u.x + DEFAULT_SENSITIVITY;    
			offset.u.y =  avg.u.y ;
			offset.u.z =  avg.u.z ;
		  break;
		case CONFIG_GSEN_CALIBRATION_GRAVITY_ON_Z_POSITIVE:  
			offset.u.x =  avg.u.x ;    
			offset.u.y =  avg.u.y ;
			offset.u.z =  avg.u.z - DEFAULT_SENSITIVITY;
		  break;
		case CONFIG_GSEN_CALIBRATION_GRAVITY_ON_X_NEGATIVE:  
			offset.u.x =  avg.u.x - DEFAULT_SENSITIVITY;    
			offset.u.y =  avg.u.y ;
			offset.u.z =  avg.u.z ;
		  break;
		case CONFIG_GSEN_CALIBRATION_GRAVITY_ON_Y_NEGATIVE:
			offset.u.x =  avg.u.x ;    
			offset.u.y =  avg.u.y + DEFAULT_SENSITIVITY;
			offset.u.z =  avg.u.z ;
		  break;
		case CONFIG_GSEN_CALIBRATION_GRAVITY_ON_Y_POSITIVE: 
			offset.u.x =  avg.u.x ;    
			offset.u.y =  avg.u.y - DEFAULT_SENSITIVITY;
			offset.u.z =  avg.u.z ;
		  break;
		default:  
			return -ENOTTY;
	}
	return 0;
}
*/
int gsensor_calibrate(void)
{	
	raw_data avg;
	int i, j;
	long xyz_acc[SENSOR_DATA_SIZE];   
  	s16 xyz[SENSOR_DATA_SIZE];
		
	/* initialize the accumulation buffer */
  	for(i = 0; i < SENSOR_DATA_SIZE; ++i) 
		xyz_acc[i] = 0;

	for(i = 0; i < AVG_NUM; i++) {      
		device_i2c_read_xyz(devdata.client, (s16 *)&xyz);
		for(j = 0; j < SENSOR_DATA_SIZE; ++j) 
			xyz_acc[j] += xyz[j];
  	}
	/* calculate averages */
  	for(i = 0; i < SENSOR_DATA_SIZE; ++i) 
		avg.v[i] = (s16) (xyz_acc[i] / AVG_NUM);
		
	if(avg.v[2] < 0){
		offset.u.x =  avg.v[0] ;    
		offset.u.y =  avg.v[1] ;
		offset.u.z =  avg.v[2] + DEFAULT_SENSITIVITY;
		return CONFIG_GSEN_CALIBRATION_GRAVITY_ON_Z_POSITIVE;
	}
	else{	
		offset.u.x =  avg.v[0] ;    
		offset.u.y =  avg.v[1] ;
		offset.u.z =  avg.v[2] - DEFAULT_SENSITIVITY;
		return CONFIG_GSEN_CALIBRATION_GRAVITY_ON_Z_NEGATIVE;
	}
	return 0;
}

int gsensor_reset(struct i2c_client *client)
{
	unsigned char buffer[7], buffer2[2];
	int ret = 0;
	/* 1. check D10 , VALUE_STADR = 0x55 , VALUE_STAINT = 0xAA */
	buffer[0] = REG_STADR;
	buffer2[0] = REG_STAINT;
	
	ret = device_i2c_rxdata(client, buffer, 2);
	if (ret < 0 )
	{
		return ret;
	}
	ret = device_i2c_rxdata(client, buffer2, 2);
	if (ret < 0 )
	{
		return ret;
	}		
	if( buffer[0] == VALUE_STADR || buffer2[0] == VALUE_STAINT)
	{
		dmtprintk(KERN_INFO " REG_STADR_VALUE = %d , REG_STAINT_VALUE = %d\n", buffer[0], buffer2[0]);
		dmtprintk(KERN_INFO " %s DMT_DEVICE_NAME registered I2C driver!\n",__FUNCTION__);
		devdata.client = client;
	}
	else
	{
		dmtprintk(KERN_INFO " %s gsensor I2C err @@@ REG_STADR_VALUE = %d , REG_STAINT_VALUE = %d \n", 
						__func__, buffer[0], buffer2[0]);
		devdata.client = NULL;
		return -1;
	}
	/* 2. Powerdown reset */
	buffer[0] = REG_PD;
	buffer[1] = VALUE_PD_RST;
	ret = device_i2c_txdata(client, buffer, 2);
	if (ret < 0 )
	{
		return ret;
	}
	/* 3. ACTR => Standby mode => Download OTP to parameter reg => Standby mode => Reset data path => Standby mode */
	buffer[0] = REG_ACTR;
	buffer[1] = MODE_Standby;
	buffer[2] = MODE_ReadOTP;
	buffer[3] = MODE_Standby;
	buffer[4] = MODE_ResetDataPath;
	buffer[5] = MODE_Standby;
	ret = device_i2c_txdata(client, buffer, 6);
	if (ret < 0 )
	{
		return ret;
	}
	/* 4. OSCA_EN = 1 ,TSTO = b'000(INT1 = normal, TEST0 = normal) */
	buffer[0] = REG_MISC2;
	buffer[1] = VALUE_MISC2_OSCA_EN;
	ret = device_i2c_txdata(client, buffer, 2);
	if (ret < 0 )
	{
		return ret;
	}
	/* 5. AFEN = 1(AFE will powerdown after ADC) */
	buffer[0] = REG_AFEM;
	buffer[1] = VALUE_AFEM_AFEN_Normal;	
	buffer[2] = VALUE_CKSEL_ODR_100_204;	
	buffer[3] = VALUE_INTC;	
	buffer[4] = VALUE_TAPNS_Ave_2;
	buffer[5] = 0x00;	// DLYC, no delay timing
	buffer[6] = 0x07;	// INTD=1 (push-pull), INTA=1 (active high), AUTOT=1 (enable T)
	ret = device_i2c_txdata(client, buffer, 7);
	if (ret < 0 )
	{
		return ret;
	}
	/* 6. write TCGYZ & TCGX */
	buffer[0] = REG_WDAL;	// REG:0x01
	buffer[1] = 0x00;		// set TC of Y,Z gain value
	buffer[2] = 0x00;		// set TC of X gain value
	buffer[3] = 0x03;		// Temperature coefficient of X,Y,Z gain
	ret = device_i2c_txdata(client, buffer, 4);
	if (ret < 0 )
	{
		return ret;
	}	
	buffer[0] = REG_ACTR;			// REG:0x00
	buffer[1] = MODE_Standby;		// Standby
	buffer[2] = MODE_WriteOTPBuf;	// WriteOTPBuf 
	buffer[3] = MODE_Standby;		// Standby
	ret = device_i2c_txdata(client, buffer, 4);
	if (ret < 0 )
	{
		return ret;
	}
	//buffer[0] = REG_TCGYZ;
	//device_i2c_rxdata(client, buffer, 2);
	//GSE_LOG(" TCGYZ = %d, TCGX = %d  \n", buffer[0], buffer[1]);
	
	/* 7. Activation mode */
	buffer[0] = REG_ACTR;
	buffer[1] = MODE_Active;
	ret = device_i2c_txdata(client, buffer, 2);
	if (ret < 0 )
	{
		return ret;
	}
	return 0;
}

void gsensor_set_offset(int val[3])
{
	int i;
	for(i = 0; i < SENSOR_DATA_SIZE; ++i)
		offset.v[i] = (s16) val[i];
}

struct file_operations dmt_g_sensor_fops = 
{
	.owner = THIS_MODULE,
	//.read = device_read,
	//.write = device_write,
	.unlocked_ioctl = device_ioctl,
	.open = device_open,
	.release = device_close,
};

static int sensor_close_dev(struct i2c_client *client)
{    	
	char buffer[3];
	buffer[0] = REG_ACTR;
	buffer[1] = MODE_Standby;
	buffer[2] = MODE_Off;
	dbg("....\n");	
	return device_i2c_txdata(client,buffer, 3);
}

static int dmard10_suspend(struct platform_device *pdev, pm_message_t state)
{
	dbg("Gsensor enter 2 level suspend!!\n");
	return 0;
	//return sensor_close_dev(client);
}

static int dmard10_resume(struct platform_device *pdev)
{
	unsigned long dmt_delay;
	
	dbg("Gsensor 2 level resume!!\n");
	gsensor_reset(gsensor_get_i2c_client());	
	dmt_delay=msecs_to_jiffies(atomic_read(&devdata.delay));
	if(dmt_delay<1)
		dmt_delay=1;
	//dmtprintk(KERN_INFO "schedule_delayed_work start with delay time=%lu\n",dmt_delay);
	schedule_delayed_work(&devdata.delaywork,dmt_delay);
	return 0;
	//return gsensor_reset(client);
}

static void dmt10_early_suspend(struct early_suspend *h)
{
	dbg("start\n");
	cancel_delayed_work_sync(&devdata.delaywork);	
	dbg("exit\n");
}

static void dmt10_late_resume(struct early_suspend *h)
{
	unsigned long dmt_delay;

	dbg("start\n");	
	dmt_delay=msecs_to_jiffies(atomic_read(&devdata.delay));
	if(dmt_delay<1)
		dmt_delay=1;
	//dmtprintk(KERN_INFO "schedule_delayed_work start with delay time=%lu\n",dmt_delay);
	schedule_delayed_work(&devdata.delaywork,dmt_delay);	
	dbg("exit\n");
}


/*static const struct i2c_device_id device_i2c_ids[] = {
	{DEVICE_I2C_NAME, 0},
	{}   
};

MODULE_DEVICE_TABLE(i2c, device_i2c_ids);

static struct i2c_driver device_i2c_driver = 
{
	.driver	= {
		.owner = THIS_MODULE,
		.name = DEVICE_I2C_NAME,
		},
	.class = I2C_CLASS_HWMON,
	.id_table = device_i2c_ids,
	.probe = device_i2c_probe,
	.remove	= __devexit_p(device_i2c_remove),
#ifndef CONFIG_ANDROID_POWER
	.suspend = device_i2c_suspend,
	.resume	= device_i2c_resume,
#endif	
	
};
*/


static int device_open(struct inode *inode, struct file *filp)
{
	return 0; 
}
/*
static ssize_t device_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	return 0;
}

static ssize_t device_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	s16 xyz[SENSOR_DATA_SIZE];
	int i;

	device_i2c_read_xyz(devdata.client, (s16 *)&xyz);
	//offset compensation 
	for(i = 0; i < SENSOR_DATA_SIZE; i++)
		xyz[i] -= offset.v[i];
	
	if(copy_to_user(buf, &xyz, count)) 
		return -EFAULT;
		
	return count;
}
*/
static long device_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0, ret = 0, i;
	int intBuf[SENSOR_DATA_SIZE];
	s16 xyz[SENSOR_DATA_SIZE];
	// check type
	if (_IOC_TYPE(cmd) != IOCTL_MAGIC) return -ENOTTY;

	// check user space pointer is valid
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err) return -EFAULT;
	
	switch(cmd) 
	{
		case SENSOR_RESET:
			mutex_lock(&devdata.DMT_mutex);
			gsensor_reset(devdata.client);
			mutex_unlock(&devdata.DMT_mutex);
			return ret;

		case SENSOR_CALIBRATION:
			mutex_lock(&devdata.DMT_mutex);
			// get orientation info
			//if(copy_from_user(&intBuf, (int*)arg, sizeof(intBuf))) return -EFAULT;
			gsensor_calibrate();
			mutex_unlock(&devdata.DMT_mutex);
			dmtprintk("Sensor_calibration:%d %d %d\n",offset.u.x,offset.u.y,offset.u.z);
			// save file
			gsensor_write_offset_to_file();
			
			// return the offset
			for(i = 0; i < SENSOR_DATA_SIZE; ++i)
				intBuf[i] = offset.v[i];

			ret = copy_to_user((int *)arg, &intBuf, sizeof(intBuf));
			return ret;
		
		case SENSOR_GET_OFFSET:
			// get data from file 
			mutex_lock(&devdata.DMT_mutex);
			gsensor_read_offset_from_file();
			mutex_unlock(&devdata.DMT_mutex);
			for(i = 0; i < SENSOR_DATA_SIZE; ++i)
				intBuf[i] = offset.v[i];

			ret = copy_to_user((int *)arg, &intBuf, sizeof(intBuf));
			return ret;

		case SENSOR_SET_OFFSET:
			ret = copy_from_user(&intBuf, (int *)arg, sizeof(intBuf));
			mutex_lock(&devdata.DMT_mutex);
			gsensor_set_offset(intBuf);
			mutex_unlock(&devdata.DMT_mutex);
			// write in to file
			gsensor_write_offset_to_file();
			return ret;
		
		case SENSOR_READ_ACCEL_XYZ:
			mutex_lock(&devdata.DMT_mutex);
			device_i2c_read_xyz(devdata.client, (s16 *)&xyz);
			mutex_unlock(&devdata.DMT_mutex);
			for(i = 0; i < SENSOR_DATA_SIZE; ++i)
				intBuf[i] = xyz[i] - offset.v[i];
			
		  	ret = copy_to_user((int*)arg, &intBuf, sizeof(intBuf));
			//PRINT_X_Y_Z(intBuf[0], intBuf[1], intBuf[2]);
			return ret;
		case SENSOR_SETYPR:
			if(copy_from_user(&intBuf, (int*)arg, sizeof(intBuf))) 
			{
				dmtprintk("%s:copy_from_user(&intBuf, (int*)arg, sizeof(intBuf)) ERROR, -EFAULT\n",__func__);			
				return -EFAULT;
			}
			input_report_abs(devdata.input, ABS_X, intBuf[0]);
			input_report_abs(devdata.input, ABS_Y, intBuf[1]);
			input_report_abs(devdata.input, ABS_Z, intBuf[2]);
			input_sync(devdata.input);
			dmtprintk(KERN_INFO "%s:SENSOR_SETYPR OK! x=%d,y=%d,z=%d\n",__func__,intBuf[0],intBuf[1],intBuf[2]);
			return 1;
		case SENSOR_GET_OPEN_STATUS:
			dmtprintk(KERN_INFO "%s:Going into DMT_GetOpenStatus()\n",__func__);
			DMT_GetOpenStatus();
			dmtprintk(KERN_INFO "%s:DMT_GetOpenStatus() finished\n",__func__);
			return 1;
			break;
		case SENSOR_GET_CLOSE_STATUS:
			dmtprintk(KERN_INFO "%s:Going into DMT_GetCloseStatus()\n",__func__);
			DMT_GetCloseStatus();	
			dmtprintk(KERN_INFO "%s:DMT_GetCloseStatus() finished\n",__func__);
			return 1;
			break;		
		case SENSOR_GET_DELAY:
		  	ret = copy_to_user((int*)arg, &interval, sizeof(interval));
			return 1;
			break;
		
		default:  /* redundant, as cmd was checked against MAXNR */
			return -ENOTTY;
	}
	
	return 0;
}
	
static int device_close(struct inode *inode, struct file *filp)
{
	return 0;
}

/***** I2C I/O function ***********************************************/
static int device_i2c_rxdata( struct i2c_client *client, unsigned char *rxData, int length)
{
	struct i2c_msg msgs[] = 
	{
		{.addr = client->addr, .flags = 0, .len = 1, .buf = rxData,}, 
		{.addr = client->addr, .flags = I2C_M_RD, .len = length, .buf = rxData,},
	};
	//unsigned char addr = rxData[0];
	if (i2c_transfer(client->adapter, msgs, 2) < 0) {
		dev_err(&client->dev, "%s: transfer failed.", __func__);
		return -EIO;
	}
	//DMT_DATA(&client->dev, "RxData: len=%02x, addr=%02x, data=%02x\n",
		//length, addr, rxData[0]);

	return 0;
}

static int device_i2c_txdata( struct i2c_client *client, unsigned char *txData, int length)
{
	struct i2c_msg msg[] = 
	{
		{.addr = client->addr, .flags = 0, .len = length, .buf = txData,}, 
	};

	if (i2c_transfer(client->adapter, msg, 1) < 0) {
		dev_err(&client->dev, "%s: transfer failed.", __func__);
		return -EIO;
	}
	//DMT_DATA(&client->dev, "TxData: len=%02x, addr=%02x data=%02x\n",
		//length, txData[0], txData[1]);
	return 0;
}
/* 1g = 128 becomes 1g = 1024 */
static inline void device_i2c_correct_accel_sign(s16 *val)
{
	*val<<= 3;
}

void device_i2c_merge_register_values(struct i2c_client *client, s16 *val, u8 msb, u8 lsb)
{
	*val = (((u16)msb) << 8) | (u16)lsb; 
	device_i2c_correct_accel_sign(val);
}

void device_i2c_read_xyz(struct i2c_client *client, s16 *xyz_p)
{	
	u8 buffer[11];
	s16 xyzTmp[SENSOR_DATA_SIZE];
	int i, j;
	/* get xyz high/low bytes, 0x12 */
	buffer[0] = REG_STADR;
	device_i2c_rxdata(client, buffer, 10);
    
	/* merge to 10-bits value */
	for(i = 0; i < SENSOR_DATA_SIZE; ++i){
		xyz_p[i] = 0;
		device_i2c_merge_register_values(client, (xyzTmp + i), buffer[2*(i+1)+1], buffer[2*(i+1)]);
	/* transfer to the default layout */
		for(j = 0; j < 3; j++)
			xyz_p[i] += sensorlayout[i][j] * xyzTmp[j];
	}
	//dmtprintk(KERN_INFO "@DMT@ xyz_p: %04d , %04d , %04d\n", xyz_p[0], xyz_p[1], xyz_p[2]);
}

static void DMT_work_func(struct work_struct *fakework)
{
	int i;
	static int firsttime=0;
	s16 xyz[SENSOR_DATA_SIZE];
	s16 txyz[SENSOR_DATA_SIZE];
	
	unsigned long t=atomic_read(&devdata.delay);
  	unsigned long dmt_delay = msecs_to_jiffies(t);

	dbg("t=%lu , dmt_delay=%lu\n", t, dmt_delay);
  	mutex_lock(&devdata.DMT_mutex);
	if(!firsttime)
	{
		gsensor_read_offset_from_file();	
	 	firsttime=1;
	}	
  	device_i2c_read_xyz(devdata.client, (s16 *)&xyz);
  	mutex_unlock(&devdata.DMT_mutex);
  	dbg("rxyz:%x,%x,%x\n", xyz[0], xyz[1], xyz[2]);
  	for(i = 0; i < SENSOR_DATA_SIZE; ++i)
     		xyz[i] -= offset.v[i];

	//PRINT_X_Y_Z(xyz[0], xyz[1], xyz[2]);
	//PRINT_X_Y_Z(offset.u.x, offset.u.y, offset.u.z);
	// x
	txyz[0] = xyz[l_sensorconfig.xyz_axis[0][0]]*l_sensorconfig.xyz_axis[0][1];
	// y
	txyz[1] = xyz[l_sensorconfig.xyz_axis[1][0]]*l_sensorconfig.xyz_axis[1][1];
	// z
	txyz[2] = xyz[l_sensorconfig.xyz_axis[2][0]]*l_sensorconfig.xyz_axis[2][1];
	input_report_abs(devdata.input, ABS_X, txyz[0]);
	input_report_abs(devdata.input, ABS_Y, txyz[1]);
	input_report_abs(devdata.input, ABS_Z, txyz[2]);

	/*input_report_abs(devdata.input, ABS_X, xyz[0]);
	input_report_abs(devdata.input, ABS_Y, -xyz[1]);
	input_report_abs(devdata.input, ABS_Z, -xyz[2]);*/
	input_sync(devdata.input);
	l_sensorconfig.test_pass = 1;
		
	if(dmt_delay<1)
		dmt_delay=1;
	schedule_delayed_work(&devdata.delaywork, dmt_delay);
}

static int mma10_open(struct inode *node, struct file *fle)
{
    dbg("open...\n");
	return 0;
}

static int mma10_close(struct inode *node, struct file *fle)
{
    dbg("close...\n");
	return 0;
}

static long mma10_ioctl(/*struct inode *inode,*/ struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	//unsigned char data[6];
	short delay = 0;
	short enable = 0;
	unsigned int uval = 0;

	/* cmd mapping */
	switch(cmd)
	{

		case WMT_IOCTL_SENSOR_GET_DRVID:
			uval = DMARD10_DRVID;
			if (copy_to_user((unsigned int*)arg, &uval, sizeof(unsigned int)))
			{
				return -EFAULT;
			}
			dbg("dmard10_driver_id:%d\n",uval);
			break;
		default:
			err = -1;
			break;
	}
errioctl:
	return err;
}

static const struct file_operations d10_fops = {
	.owner = THIS_MODULE,
	.open = mma10_open,
	.release = mma10_close,
	.unlocked_ioctl = mma10_ioctl,
};


static struct miscdevice d10_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = GSENSOR_DEV_NODE,
	.fops = &d10_fops,
};

static int sensor_writeproc( struct file   *file,
                           const char    *buffer,
                           unsigned long count,
                           void          *data )
{

	//int inputval = -1;
	int enable, sample = -1;
	char tembuf[8];
	//unsigned int amsr = 0;
	int test = 0;

	//mutex_lock(&sense_data_mutex);
	memset(tembuf, 0, sizeof(tembuf));
	// get sensor level and set sensor level
	if (sscanf(buffer, "isdbg=%d\n", &l_sensorconfig.isdbg))
	{
		// only set the dbg flag
	} else if (sscanf(buffer, "samp=%d\n", &sample))
	{
		if (sample > 0)
		{
			if (sample != l_sensorconfig.sensor_samp)
			{
				// should do sth
			}			
			//printk(KERN_ALERT "sensor samp=%d(amsr:%d) has been set.\n", sample, amsr);
		} else {
			klog("Wrong sample argumnet of sensor.\n");
		}
	} else if (sscanf(buffer, "enable=%d\n", &enable))
	{
		if ((enable < 0) || (enable > 1))
		{
			klog("The argument to enable/disable g-sensor should be 0 or 1  !!!\n");
		} else if (enable != l_sensorconfig.sensor_enable)
		{
			//mma_enable_disable(enable);
			l_sensorconfig.sensor_enable = enable;
		}
	} else 	if (sscanf(buffer, "sensor_test=%d\n", &test))
	{ // for test begin
		l_sensorconfig.test_pass = 0;
		atomic_set(&devdata.enable,1);
		DMT_sysfs_update_active_status(1);
	} else if (sscanf(buffer, "sensor_testend=%d\n", &test))
	{	// Don nothing only to be compatible the before testing program	
		atomic_set(&devdata.enable,0);
		DMT_sysfs_update_active_status(0);
	}
	//mutex_unlock(&sense_data_mutex);
	return count;
}

static int sensor_readproc(char *page, char **start, off_t off,
			  int count, int *eof, void *data)
{
	int len = 0;

	len = sprintf(page, 
			"test_pass=%d\nisdbg=%d\nrate=%d\nenable=%d\n",
				l_sensorconfig.test_pass,
				l_sensorconfig.isdbg,
				l_sensorconfig.sensor_samp,
				l_sensorconfig.sensor_enable
				);
	return len;
}


static int __devinit dmard10_probe(struct platform_device *pdev)
{
	int i;
	int err = 0;

	mutex_init(&devdata.DMT_mutex);
	atomic_set(&devdata.enable,0);
	atomic_set(&devdata.delay,100);
	INIT_DELAYED_WORK(&devdata.delaywork, DMT_work_func);
	dmtprintk(KERN_INFO "DMT: INIT_DELAYED_WORK\n");

	err=input_init();
	if(err)
		dmtprintk(KERN_INFO "%s:input_init fail, error code= %d\n",__func__,err);

	for(i = 0; i < SENSOR_DATA_SIZE; ++i)
		offset.v[i] = 0;
	/*if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
  		dmtprintk(KERN_INFO "%s, functionality check failed\n", __func__);
		return -1;
  	}

	gsensor_reset(client);
	*/
	//register ctrl dev
	err = misc_register(&d10_device);
	if (err !=0) {
		errlog("Can't register d10_device!\n");
		return -1;
	}
	// register rd/wr proc
	l_sensorconfig.sensor_proc = create_proc_entry(GSENSOR_PROC_NAME, 0666, NULL/*&proc_root*/);
	if (l_sensorconfig.sensor_proc != NULL)
	{
		l_sensorconfig.sensor_proc->write_proc = sensor_writeproc;
		l_sensorconfig.sensor_proc->read_proc = sensor_readproc;
	}

	return 0;
}

static int __devexit dmard10_remove(struct platform_device *pdev)
{
	if (l_sensorconfig.sensor_proc != NULL)
	{
		remove_proc_entry(GSENSOR_PROC_NAME, NULL);
		l_sensorconfig.sensor_proc = NULL;
	}
	misc_deregister(&d10_device);
	return 0;
}

static int get_axisset(void)
{
	char varbuf[64];
	int n;
	int varlen;

	memset(varbuf, 0, sizeof(varbuf));
	varlen = sizeof(varbuf);
	if (wmt_getsyspara("wmt.io.dm10sensor", varbuf, &varlen)) {
		errlog("Can't get gsensor config in u-boot!!!!\n");
		//return -1;
	} else {
		n = sscanf(varbuf, "%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d",
				&l_sensorconfig.op,
				&l_sensorconfig.int_gpio,
				&l_sensorconfig.samp,
				&(l_sensorconfig.xyz_axis[0][0]),
				&(l_sensorconfig.xyz_axis[0][1]),
				&(l_sensorconfig.xyz_axis[1][0]),
				&(l_sensorconfig.xyz_axis[1][1]),
				&(l_sensorconfig.xyz_axis[2][0]),
				&(l_sensorconfig.xyz_axis[2][1]),
				&(l_sensorconfig.offset[0]),
				&(l_sensorconfig.offset[1]),
				&(l_sensorconfig.offset[2])
			);
		if (n != 12) {
			errlog("gsensor format is error in u-boot!!!\n");
			return -1;
		}
		l_sensorconfig.sensor_samp = l_sensorconfig.samp;

		dbg("get the sensor config: %d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d\n",
			l_sensorconfig.op,
			l_sensorconfig.int_gpio,
			l_sensorconfig.samp,
			l_sensorconfig.xyz_axis[0][0],
			l_sensorconfig.xyz_axis[0][1],
			l_sensorconfig.xyz_axis[1][0],
			l_sensorconfig.xyz_axis[1][1],
			l_sensorconfig.xyz_axis[2][0],
			l_sensorconfig.xyz_axis[2][1],
			l_sensorconfig.offset[0],
			l_sensorconfig.offset[1],
			l_sensorconfig.offset[2]
		);
	}
	return 0;
}

static void dmard10_platform_release(struct device *device)
{
	dbg("...\n");
    return;
}


static struct platform_device dmard10_device = {
    .name           = GSENSOR_I2C_NAME,
    .id             = 0,
    .dev            = {
    	.release = dmard10_platform_release,
    },
};

static struct platform_driver dmard10_driver = {
	.probe = dmard10_probe,
	.remove = dmard10_remove,
	.suspend	= dmard10_suspend,
	.resume		= dmard10_resume,
	.driver = {
		   .name = GSENSOR_I2C_NAME,
		   },
};


static int __init device_init(void)
{
	int err=-1;	
	struct device *device;
	int ret = 0;

	// parse g-sensor u-boot arg
	ret = get_axisset(); 

	if (gsensor_i2c_register_device() < 0)
		return -1;
	if (gsensor_reset(gsensor_get_i2c_client()))
	{
		dbg("Failed to init dmard10!\n");
		gsensor_i2c_unregister_device();
		return -1;
	}
	ret = alloc_chrdev_region(&devdata.devno, 0, 1, GSENSOR_DEVICE_NAME);
  	if(ret)
	{
		errlog("can't allocate chrdev\n");
		return ret;
	}
	dmtprintk(KERN_INFO "%s, register chrdev(%d, %d)\n", __func__, MAJOR(devdata.devno), MINOR(devdata.devno));
	
	cdev_init(&devdata.cdev, &dmt_g_sensor_fops);  
	devdata.cdev.owner = THIS_MODULE;
  	ret = cdev_add(&devdata.cdev, devdata.devno, 1);
  	if(ret < 0)
	{
		dmtprintk(KERN_INFO "%s, add character device error, ret %d\n", __func__, ret);
		return ret;
	}
	devdata.class = class_create(THIS_MODULE, ACCELEMETER_CLASS_NAME);
	if(IS_ERR(devdata.class))
	{
   		dmtprintk(KERN_INFO "%s, create class, error\n", __func__);
		return ret;
  	}
	device=device_create(devdata.class, NULL, devdata.devno, NULL, GSENSOR_DEVICE_NAME);

	// create the platform device
	l_dev_class = class_create(THIS_MODULE, GSENSOR_I2C_NAME);
	if (IS_ERR(l_dev_class)){
		ret = PTR_ERR(l_dev_class);
		printk(KERN_ERR "Can't class_create gsensor device !!\n");
		return ret;
	}
    if((ret = platform_device_register(&dmard10_device)))
    {
    	klog("Can't register dmard10 platform devcie!!!\n");
    	return ret;
    }
    if ((ret = platform_driver_register(&dmard10_driver)) != 0)
    {
    	errlog("Can't register dmard10 platform driver!!!\n");
    	return ret;
    }	
	err = create_device_attributes(device,DMT_attributes);
	l_device = device;
#ifdef CONFIG_HAS_EARLYSUSPEND
        l_sensorconfig.earlysuspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
        l_sensorconfig.earlysuspend.suspend = dmt10_early_suspend;
        l_sensorconfig.earlysuspend.resume = dmt10_late_resume;
        register_early_suspend(&l_sensorconfig.earlysuspend);
#endif
	return 0;
	//return i2c_add_driver(&device_i2c_driver);
}


static void __exit device_exit(void)
{
	unregister_early_suspend(&l_sensorconfig.earlysuspend);
	remove_device_attributes(l_device,DMT_attributes);
	input_unregister_device(devdata.input);
	input_free_device(devdata.input);
	platform_driver_unregister(&dmard10_driver);
    platform_device_unregister(&dmard10_device);
	gsensor_i2c_unregister_device();
	class_destroy(l_dev_class);
	cdev_del(&devdata.cdev);
	unregister_chrdev_region(devdata.devno, 1);
	device_destroy(devdata.class, devdata.devno);
	class_destroy(devdata.class);
	//i2c_del_driver(&device_i2c_driver);
}

void gsensor_write_offset_to_file(void)
{
	char data[18];
	unsigned int orgfs;
	struct file *fp;

	sprintf(data,"%5d %5d %5d",offset.u.x,offset.u.y,offset.u.z);

	orgfs = get_fs();
	/* Set segment descriptor associated to kernel space */
	set_fs(KERNEL_DS);

	fp = filp_open(OffsetFileName, O_RDWR | O_CREAT, 0/*0777*/);
	if(IS_ERR(fp))
	{
		dmtprintk(KERN_INFO "filp_open %s error!!.\n",OffsetFileName);
	}
	else
	{
		dmtprintk(KERN_INFO "filp_open %s SUCCESS!!.\n",OffsetFileName);
		fp->f_op->write(fp,data,18, &fp->f_pos);
 		filp_close(fp,NULL);
	}

	set_fs(orgfs);
}

void gsensor_read_offset_from_file(void)
{
	unsigned int orgfs;
	char data[18];
	struct file *fp;
	int ux,uy,uz;
	orgfs = get_fs();
	/* Set segment descriptor associated to kernel space */
	set_fs(KERNEL_DS);

	fp = filp_open(OffsetFileName, O_RDWR , 0);
	dmtprintk(KERN_INFO "%s,try file open !\n",__func__);
	if(IS_ERR(fp))
	{
		dmtprintk(KERN_INFO "%s:Sorry,file open ERROR !\n",__func__);
		offset.u.x=0;offset.u.y=0;offset.u.z=0;
#if AUTO_CALIBRATION
		/* get acceleration average reading */
		gsensor_calibrate();
		gsensor_write_offset_to_file();
#endif
	}
	else{
		dmtprintk("filp_open %s SUCCESS!!.\n",OffsetFileName);
		fp->f_op->read(fp,data,18, &fp->f_pos);
		dmtprintk("filp_read result %s\n",data);
		sscanf(data,"%d %d %d",&ux,&uy,&uz);
		offset.u.x=ux;
		offset.u.y=uy;
		offset.u.z=uz;
		filp_close(fp,NULL);
	}
	set_fs(orgfs);
}
//*********************************************************************************************************
MODULE_AUTHOR("DMT_RD");
MODULE_DESCRIPTION("DMT Gsensor Driver");
MODULE_LICENSE("GPL");

module_init(device_init);
module_exit(device_exit);
