/*
 * @file drivers/i2c/dmard08.c
 * @brief DMARD08 g-sensor Linux device driver
 * @author Domintech Technology Co., Ltd (http://www.domintech.com.tw)
 * @version 1.22
 * @date 2011/12/01
 *
 * @section LICENSE
 *
 *  Copyright 2011 Domintech Technology Co., Ltd
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
 *
 */
#include <linux/module.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/cdev.h>
#include <linux/earlysuspend.h>
#include <linux/wakelock.h>
#include <asm/uaccess.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/serio.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <mach/hardware.h>
#include <asm/io.h>
#include <linux/miscdevice.h>
#include <mach/gpio.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
// ****Add by Steve Huang*********2011-11-18********
#include <linux/syscalls.h>
#include "gsensor.h"
//#include "cyclequeue.h"
// ************************************************

#define SENSOR_DATA_SIZE 3   

static struct mutex sense_data_mutex;
static struct class* l_dev_class = NULL;

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


// ****Add by Steve Huang*********2011-11-18********
/*void gsensor_write_offset_to_file(void);
void gsensor_read_offset_from_file(void);
char OffsetFileName[] = "/data/misc/dmt/offset.txt";*/
//**************************************************


extern int gsensor_i2c_register_device (void);
extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);

/*static int dmard08_i2c_suspend(struct i2c_client *client, pm_message_t mesg);
static int dmard08_i2c_resume(struct i2c_client *client);*/
//static int __devinit dmard08_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
//static int __devexit dmard08_i2c_remove(struct i2c_client *client);
void dmard08_i2c_read_xyz(struct i2c_client *client, s16 *xyz);
static inline void dmard08_i2c_correct_accel_sign(s16 *val); //check output is correct
void dmard08_i2c_merge_register_values(struct i2c_client *client, s16 *val, u8 msb, u8 lsb); //merge the register values

struct raw_data {
	short x;
	short y;
	short z;
};

struct raw_data rdata;
//static struct raw_data offset;

struct dev_data 
{
	dev_t devno;
	struct cdev cdev;
  	struct class *class;
	struct i2c_client *client;
};
//static struct dev_data dev;


unsigned int sample_rate_2_memsec(unsigned int rate)
{
	return (1000/rate);
}


/*void gsensor_read_accel_avg(int num_avg, raw_data *avg_p) // marked by eason check again!!
{
   	long xyz_acc[SENSOR_DATA_SIZE];   
  	s16 xyz[SENSOR_DATA_SIZE];
  	int i, j;
	
	//initialize the accumulation buffer
  	for(i = 0; i < SENSOR_DATA_SIZE; ++i) 
		xyz_acc[i] = 0;

	for(i = 0; i < num_avg; i++) 
	{      
		device_i2c_read_xyz(l_sensorconfig.client, (s16 *)&xyz);
		for(j = 0; j < SENSOR_DATA_SIZE; j++) 
			xyz_acc[j] += xyz[j];
  	}

	// calculate averages
  	for(i = 0; i < SENSOR_DATA_SIZE; i++) 
		avg_p->v[i] = (s16) (xyz_acc[i] / num_avg);
}*/

/*void gsensor_calibrate(int side) //marked by eason check again
{	
	raw_data avg;
	int avg_num = 16;
	
	//IN_FUNC_MSG;
	// get acceleration average reading
	gsensor_read_accel_avg(avg_num, &avg);
	// calculate and set the offset
	gsensor_calculate_offset(side, avg); 
}*/

/*void ce_on(void) //marked by eason check again
{
	int gppdat;
	gppdat = __raw_readl(S3C64XX_GPPDAT);
	gppdat |= (1 << 0);

	__raw_writel(gppdat,S3C64XX_GPPDAT);
}

void ce_off(void)
{
	int gppdat;
	gppdat = __raw_readl(S3C64XX_GPPDAT);
	gppdat &= ~(1 << 0);

	__raw_writel(gppdat,S3C64XX_GPPDAT);
}
 
void config_ce_pin(void)
{
	unsigned int value;
	//D08's CE (pin#12) is connected to S3C64XX AP processor's port P0
	//Below codes set port P0 as digital output 
	value = readl(S3C64XX_GPPCON);
	value &= ~ (0x3);
	value |= 1 ;  //Output =01 , Input = 00 , Ext. Interrupt = 10
	writel(value, S3C64XX_GPPCON);  //save S3C64XX_GPPCON change
}

void gsensor_reset(void)
{
	ce_off();
	msleep(300); 
	ce_on();
}*/

/*void gsensor_set_offset(int val[3])  //marked by eason check again
{
	int i;
	IN_FUNC_MSG;
	for(i = 0; i < SENSOR_DATA_SIZE; ++i)
		offset.v[i] = (s16) val[i];
}*/

/*
static const struct i2c_device_id dmard08_i2c_ids[] = 
{
	{GSENSOR_I2C_NAME, 0},
	{}   
};

MODULE_DEVICE_TABLE(i2c, dmard08_i2c_ids);


static struct i2c_driver dmard08_i2c_driver = 
{
	.driver	= {
		.owner = THIS_MODULE,
		.name = GSENSOR_I2C_NAME,
		},
	.class = I2C_CLASS_HWMON,
	.probe = dmard08_i2c_probe,
	.remove	= __devexit_p(dmard08_i2c_remove),
	//.suspend = dmard08_i2c_suspend,
	//.resume	= dmard08_i2c_resume,
	.id_table = dmard08_i2c_ids,
};
*/

static int dmard08_i2c_xyz_read_reg(struct i2c_client *client,u8 *buffer, int length) //OK
{
	
	struct i2c_msg msg[] = 
	{
		{.addr = client->addr, .flags = 0, .len = 1, .buf = buffer,}, 
		{.addr = client->addr, .flags = I2C_M_RD, .len = length, .buf = buffer,},
	};
	return i2c_transfer(client->adapter, msg, 2);
}

static int dmard08_i2c_xyz_write_reg(struct i2c_client *client,u8 *buffer, int length) //write reg OK
{
	struct i2c_msg msg[] = 
	{
		{.addr = client->addr, .flags = 0, .len = length, .buf = buffer,},
	};
	return i2c_transfer(client->adapter, msg, 1);
}

//static void dmard08_i2c_read_xyz(struct i2c_client, s16 *x, s16 *y, s16 *z) //add by eason
void dmard08_i2c_read_xyz(struct i2c_client *client, s16 *xyz_p)
{
//	s16 xTmp,yTmp,zTmp;		//added by eason
	s16 xyzTmp[SENSOR_DATA_SIZE];
	int i;
/*get xyz high/low bytes, 0x02~0x07*/
	u8 buffer[6];
	buffer[0] = 0x2;
	mutex_lock(&sense_data_mutex);
	dmard08_i2c_xyz_read_reg(client, buffer, 6);
	mutex_unlock(&sense_data_mutex);
   
	//merge to 11-bits value
	for(i = 0; i < SENSOR_DATA_SIZE; ++i){
		dmard08_i2c_merge_register_values(client, (xyzTmp + i), buffer[2*i], buffer[2*i + 1]);
	}
	//transfer to the default layout
	for(i = 0; i < SENSOR_DATA_SIZE; ++i)
	{
		xyz_p[i] = xyzTmp[i]; // add by eason
/*		xyz_p[i] = 0;
		for(j = 0; j < 3; j++)
			xyz_p[i] += sensorlayout[i][j] * xyzTmp[j]; */
	}
	dbg("%x,%x,%x,",xyz_p[0], xyz_p[1], xyz_p[2]);
	//printk("@DMT@ dmard08_i2c_read_xyz: X-axis: %d ,Y-axis: %d ,Z-axis: %d\n", xyz_p[0], xyz_p[1], xyz_p[2]);
}

void dmard08_i2c_merge_register_values(struct i2c_client *client, s16 *val, u8 msb, u8 lsb)
{
	
	*val = (((u16)msb) << 3) | (u16)lsb;
	dmard08_i2c_correct_accel_sign(val);
}

static inline void dmard08_i2c_correct_accel_sign(s16 *val)
{
	
	*val<<= (sizeof(s16) * BITS_PER_BYTE - 11);  
	*val>>= (sizeof(s16) * BITS_PER_BYTE - 11);  
}

/*
static int dmard08_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	dbg("...\n");	
	return 0;
}
*/

//static int __devinit dmard08_i2c_probe(struct i2c_client *client,const struct i2c_device_id *id)
static int __devinit dmard08_hw_init(struct i2c_client *client/*,const struct i2c_device_id *id*/)
{
	char cAddress = 0 , cData = 0;
	u8 buffer[2];

	//for(i = 0; i < SENSOR_DATA_SIZE; ++i) //marked by eason check again
	//	offset.v[i] = 0;

	
	if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
  		dbg("I2C_FUNC_I2C not support\n");
    	return -1;
  	}
	//config_ce_pin(); //how used?
	//gsensor_reset(); //how used?
	/* check SW RESET */
	cAddress = 0x08;
	i2c_master_send( client, (char*)&cAddress, 1);
	i2c_master_recv( client, (char*)&cData, 1);
	dbg( "i2c Read 0x08 = %d \n", cData);
	if( cData == 0x00)
	{
		cAddress = 0x09;
		i2c_master_send( client, (char*)&cAddress, 1);
		i2c_master_recv( client, (char*)&cData, 1);	
		dbg( "i2c Read 0x09 = %d \n", cData);
		if( cData == 0x00)
		{
			cAddress = 0x0a;
			i2c_master_send( client, (char*)&cAddress, 1);
			i2c_master_recv( client, (char*)&cData, 1);	
			dbg( "i2c Read 0x0a = %d \n", cData);
			if( cData == 0x88)
			{
				cAddress = 0x0b;
				i2c_master_send( client, (char*)&cAddress, 1);
				i2c_master_recv( client, (char*)&cData, 1);	
				dbg( "i2c Read 0x0b = %d \n", cData);
				if( cData == 0x08)
				{
					dbg( "DMT_DEVICE_NAME registered I2C driver!\n");
					l_sensorconfig.client = client;
				}
				else
				{
					dbg( "err : i2c Read 0x0B = %d!\n",cData);
					l_sensorconfig.client = NULL;
					return -1;
				}
			}
			else
			{
				dbg( "err : i2c Read 0x0A = %d!\n",cData);
				l_sensorconfig.client = NULL;
				return -1;
			}
		}
		else
		{
			dbg( "err : i2c Read 0x09 = %d!\n",cData);
			l_sensorconfig.client = NULL;
			return -1;
		}
	}
	else
	{
		dbg( "err : i2c Read 0x08 = %d!\n",cData);
		l_sensorconfig.client = NULL;
		
		return -1;
	}
	
	/* set sampling period if samp = 1, set the sampling frequency = 684 
		 otherwise set the sample frequency = 342 (default) added by eason 2012/3/7*/
	if (l_sensorconfig.samp == 1) {
		buffer[0] = 0x08;
		buffer[1] = 0x04;
		dmard08_i2c_xyz_write_reg(client, buffer, 2);
	} 
		
	/*check sensorlayout[i][j] //eason
	for(i = 0; i < 3; ++i)
	{
		for(j = 0; j < 3; j++)
			printk("%d",sensorlayout[i][j]);
		printk("\n");
	} */
	
	return 0;
}

static int __devexit dmard08_i2c_remove(struct i2c_client *client) //OK
{
	dbg("...\n");
	
	return 0;
}

/*
static int dmard08_i2c_resume(struct i2c_client *client) //OK
{
	dbg("...\n");
	
	return 0;
}
*/
static int get_axisset(void)
{
	char varbuf[64];
	int n;
	int varlen;

	memset(varbuf, 0, sizeof(varbuf));
	varlen = sizeof(varbuf);
	if (wmt_getsyspara("wmt.io.dm08sensor", varbuf, &varlen)) {
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

static void dmard08_platform_release(struct device *device)
{
	dbg("...\n");
    return;
}


static struct platform_device dmard08_device = {
    .name           = GSENSOR_I2C_NAME,
    .id             = 0,
    .dev            = {
    	.release = dmard08_platform_release,
    },
};

static int dmard08_suspend(struct platform_device *pdev, pm_message_t state)
{
	dbg("...\n");
	//cancel_delayed_work_sync(&l_sensorconfig.work);
	
	return 0;
}


static int dmard08_open(struct inode *node, struct file *fle)
{
    dbg("open...\n");
	return 0;
}

/*	release command for dmard08 device file	*/
static int dmard08_close(struct inode *node, struct file *fle)
{
    dbg("close...\n");
	return 0;
}

/*	ioctl command for dmard08 device file	*/
static long dmard08_ioctl(/*struct inode *inode,*/ struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	//unsigned char data[6];
	short delay = 0;
	short enable = 0;
	unsigned int uval = 0;

	if (WMT_IOCTL_SENSOR_CAL_OFFSET == cmd)
	{
		return 0;// now do nothing
	}
	
	/* cmd mapping */
	mutex_lock(&sense_data_mutex);
	switch(cmd)
	{

	case ECS_IOCTL_APP_SET_DELAY:
		// set the rate of g-sensor
		dbg("ECS_IOCTL_APP_SET_DELAY\n");
		if (copy_from_user(&delay,(short*)arg, sizeof(short)))
		{
			errlog("Can't get set delay!!!\n");
			err = -EFAULT;
			goto errioctl;
		}
		klog("set delay=%d \n", delay);
		//klog("before change sensor sample:%d...\n", l_sensorconfig.sensor_samp);
		if ((delay >=0) && (delay < 20))
		{
			delay = 20;
		} else if (delay > 200) 
		{
			delay = 200;
		}
		if (delay > 0)
		{
			l_sensorconfig.sensor_samp = 1000/delay;
		} else {
			errlog("error delay argument(delay=%d)!!!\n",delay);
			err = -EFAULT;
			goto errioctl;	
		}
		break;
	case ECS_IOCTL_APP_SET_AFLAG:
		dbg("ECS_IOCTL_APP_SET_AFLAG\n");
		// enable/disable sensor
		if (copy_from_user(&enable, (short*)arg, sizeof(short)))
		{
			errlog("Can't get enable flag!!!\n");
			err = -EFAULT;
			goto errioctl;				
		}
		if ((enable >=0) && (enable <=1))
		{
			dbg("driver: disable/enable(%d) gsensor.\n", enable);
			
			if (enable != l_sensorconfig.sensor_enable)
			{
				// do sth ???
				//mma_enable_disable(enable);
				/*if (enable != 0)
				{
					queue_delayed_work(l_sensorconfig.queue, &l_sensorconfig.work, msecs_to_jiffies(sample_rate_2_memsec(l_sensorconfig.sensor_samp)));
				} else {
					cancel_delayed_work_sync(&l_sensorconfig.work);
					flush_workqueue(l_sensorconfig.queue);
				}*/
				l_sensorconfig.sensor_enable = enable;
				
			}			
		} else {
			errlog("Wrong enable argument!!!\n");
			err = -EFAULT;
			goto errioctl;
		}
		break;
	case WMT_IOCTL_SENSOR_GET_DRVID:
		uval = DMARD08_DRVID;
		if (copy_to_user((unsigned int*)arg, &uval, sizeof(unsigned int)))
		{
			return -EFAULT;
		}
		dbg("dmard08_driver_id:%d\n",uval);
		break;
	default:
		break;
	}
errioctl:
	mutex_unlock(&sense_data_mutex);
	return err;
}

/*
static ssize_t dmard08_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
	struct que_data data;
	short xyz_temp[3];
	
	// read data from cycle queue
	while (clque_is_empty()) msleep(10);
	clque_out(&data);
	xyz_temp[0] = data.data[0];
	xyz_temp[1] = data.data[1];
	xyz_temp[2] = data.data[2];
	
	
	if(copy_to_user(buf, &xyz_temp, sizeof(xyz_temp))) 
		return -EFAULT;
	dbg("x=%x,y=%x,z=%x\n",xyz_temp[0], xyz_temp[1], xyz_temp[2]);
	return sizeof(xyz_temp);
}
*/
/*
static ssize_t dmard08_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
	dbg("write...\n");
	return 0;
}
*/

static const struct file_operations d08_fops = {
	.owner = THIS_MODULE,
	.open = dmard08_open,
	.release = dmard08_close,
	//.read = dmard08_read,
	//.wirte = dmard08_write,
	.unlocked_ioctl = dmard08_ioctl,
};

static struct miscdevice d08_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = GSENSOR_DEV_NODE,
	.fops = &d08_fops,
};


static int dmard08_resume(struct platform_device *pdev)
{
	char buffer[2];
	dbg("...\n");

	if (l_sensorconfig.samp == 1) {
		buffer[0] = 0x08;
		buffer[1] = 0x04;
		dmard08_i2c_xyz_write_reg(l_sensorconfig.client, buffer, 2);
	} 	
	return 0;
}

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

	mutex_lock(&sense_data_mutex);
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
	} else if (sscanf(buffer, "sensor_testend=%d\n", &test))
	{	// Don nothing only to be compatible the before testing program		
	}
	mutex_unlock(&sense_data_mutex);
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

static void read_work_func(struct work_struct *work)
{
	s16 xyz[SENSOR_DATA_SIZE];
	s16 txyz[SENSOR_DATA_SIZE];

	if (! l_sensorconfig.sensor_enable)
	{
		// no report data
		queue_delayed_work(l_sensorconfig.queue, &l_sensorconfig.work, msecs_to_jiffies(sample_rate_2_memsec(l_sensorconfig.sensor_samp)));
		return;
	}
	// read data to one cycle que
	//dbg("read...\n");
	dmard08_i2c_read_xyz(l_sensorconfig.client, (s16 *)xyz);

	// x
	txyz[0] = xyz[l_sensorconfig.xyz_axis[0][0]]*l_sensorconfig.xyz_axis[0][1]+l_sensorconfig.offset[0];
	// y
	txyz[1] = xyz[l_sensorconfig.xyz_axis[1][0]]*l_sensorconfig.xyz_axis[1][1]+l_sensorconfig.offset[1];
	// z
	txyz[2] = xyz[l_sensorconfig.xyz_axis[2][0]]*l_sensorconfig.xyz_axis[2][1]+l_sensorconfig.offset[2];

	input_report_abs(l_sensorconfig.input_dev, ABS_X, txyz[0]);
	input_report_abs(l_sensorconfig.input_dev, ABS_Y, txyz[1]);
	input_report_abs(l_sensorconfig.input_dev, ABS_Z, txyz[2]);
	input_sync(l_sensorconfig.input_dev);
	l_sensorconfig.test_pass = 1; // for testing
	// read next
	queue_delayed_work(l_sensorconfig.queue, &l_sensorconfig.work, msecs_to_jiffies(sample_rate_2_memsec(l_sensorconfig.sensor_samp)));
}


static int dmard08_probe(struct platform_device *pdev)
{
	int err = 0;

	//register ctrl dev
	err = misc_register(&d08_device);
	if (err !=0) {
		errlog("Can't register d08_device!\n");
		return -1;
	}
	// register rd/wr proc
	l_sensorconfig.sensor_proc = create_proc_entry(GSENSOR_PROC_NAME, 0666, NULL/*&proc_root*/);
	if (l_sensorconfig.sensor_proc != NULL)
	{
		l_sensorconfig.sensor_proc->write_proc = sensor_writeproc;
		l_sensorconfig.sensor_proc->read_proc = sensor_readproc;
	}
	// init work queue
	l_sensorconfig.queue = create_singlethread_workqueue("sensor-report");
	INIT_DELAYED_WORK(&l_sensorconfig.work, read_work_func);
	mutex_init(&sense_data_mutex);
		// init input device
	l_sensorconfig.input_dev = input_allocate_device();
	if (!l_sensorconfig.input_dev) {
		err = -ENOMEM;
		errlog("Failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}
	l_sensorconfig.input_dev->evbit[0] = BIT(EV_ABS) | BIT_MASK(EV_KEY);
	/* x-axis acceleration */
	input_set_abs_params(l_sensorconfig.input_dev, ABS_X, -1024, 1024, 0, 0);
	/* y-axis acceleration */
	input_set_abs_params(l_sensorconfig.input_dev, ABS_Y, -1024, 1024, 0, 0);
	/* z-axis acceleration */
	input_set_abs_params(l_sensorconfig.input_dev, ABS_Z, -1024, 1024, 0, 0);

	l_sensorconfig.input_dev->name = GSENSOR_INPUT_NAME;

	err = input_register_device(l_sensorconfig.input_dev);

	if (err) {
		errlog("Unable to register input device: %s\n",
		       l_sensorconfig.input_dev->name);
		goto exit_input_register_device_failed;
	}

	return 0;
exit_input_register_device_failed:
	// free inut
	input_free_device(l_sensorconfig.input_dev);
exit_input_dev_alloc_failed:
	// free queue
	destroy_workqueue(l_sensorconfig.queue);
	l_sensorconfig.queue = NULL;	
	// free proc
	if (l_sensorconfig.sensor_proc != NULL)
	{
		remove_proc_entry(GSENSOR_PROC_NAME, NULL);
		l_sensorconfig.sensor_proc = NULL;
	}	
	// free work	
	// unregister ctrl dev
	misc_deregister(&d08_device);
	return err;
}

static int dmard08_remove(struct platform_device *pdev)
{
	if (NULL != l_sensorconfig.queue)
	{
		cancel_delayed_work_sync(&l_sensorconfig.work);
		flush_workqueue(l_sensorconfig.queue);
		destroy_workqueue(l_sensorconfig.queue);
		l_sensorconfig.queue = NULL;
	}
	if (l_sensorconfig.sensor_proc != NULL)
	{
		remove_proc_entry(GSENSOR_PROC_NAME, NULL);
		l_sensorconfig.sensor_proc = NULL;
	}
	misc_deregister(&d08_device);
	input_unregister_device(l_sensorconfig.input_dev);
	return 0;
}


static struct platform_driver dmard08_driver = {
	.probe = dmard08_probe,
	.remove = dmard08_remove,
	.suspend	= dmard08_suspend,
	.resume		= dmard08_resume,
	.driver = {
		   .name = GSENSOR_I2C_NAME,
		   },
};

static void dmard08_early_suspend(struct early_suspend *h)
{
	dbg("start\n");
	cancel_delayed_work_sync(&l_sensorconfig.work);
	dbg("exit\n");
}

static void dmard08_late_resume(struct early_suspend *h)
{
	dbg("start\n");
	// init 
	queue_delayed_work(l_sensorconfig.queue, &l_sensorconfig.work, msecs_to_jiffies(sample_rate_2_memsec(l_sensorconfig.sensor_samp)));
	dbg("exit\n");
}


static int __init dmard08_init(void) //OK
{
	int ret = 0;

	// parse g-sensor u-boot arg
	ret = get_axisset(); 
	/*if ((ret != 0) || !l_sensorconfig.op)
	{
		dbg("Can't load gsensor dmar08 driver for error u-boot arg!\n");
		return -EINVAL;
	}*/
	if (gsensor_i2c_register_device() < 0)
		return -1;
	// find the device
	/*if(i2c_add_driver(&dmard08_i2c_driver) != 0)
	{
		ret = -1;
		dbg("Can't find gsensor dmard08!\n");
		goto err_i2c_add_driver;
	}*/
	if(dmard08_hw_init(gsensor_get_i2c_client()))
	{
		ret = -1;
		dbg("Can't find gsensor dmard08!\n");
		goto err_i2c_add_driver;
	}
	

	// create the platform device
	l_dev_class = class_create(THIS_MODULE, GSENSOR_I2C_NAME);
	if (IS_ERR(l_dev_class)){
		ret = PTR_ERR(l_dev_class);
		printk(KERN_ERR "Can't class_create gsensor device !!\n");
		return ret;
	}
    if((ret = platform_device_register(&dmard08_device)))
    {
    	klog("Can't register mc3230 platform devcie!!!\n");
    	return ret;
    }
    if ((ret = platform_driver_register(&dmard08_driver)) != 0)
    {
    	errlog("Can't register mc3230 platform driver!!!\n");
    	return ret;
    }
#ifdef CONFIG_HAS_EARLYSUSPEND
        l_sensorconfig.earlysuspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
        l_sensorconfig.earlysuspend.suspend = dmard08_early_suspend;
        l_sensorconfig.earlysuspend.resume = dmard08_late_resume;
        register_early_suspend(&l_sensorconfig.earlysuspend);
#endif

    queue_delayed_work(l_sensorconfig.queue, &l_sensorconfig.work, msecs_to_jiffies(sample_rate_2_memsec(l_sensorconfig.sensor_samp)));

	return 0;

err_i2c_add_driver:
	gsensor_i2c_unregister_device();
	return ret;
}

static void __exit dmard08_exit(void) //OK
{
    platform_driver_unregister(&dmard08_driver);
    platform_device_unregister(&dmard08_device);
	class_destroy(l_dev_class);
	gsensor_i2c_unregister_device();
}

//*********************************************************************************************************
// 2011-11-30
// Add by Steve Huang 
// function definition
/*
void gsensor_write_offset_to_file(void)
{
	char data[18];
	unsigned int orgfs;
	long lfile=-1;

	//sprintf(data,"%5d %5d %5d",offset.u.x,offset.u.y,offset.u.z); //marked by eason check again

	orgfs = get_fs();
// Set segment descriptor associated to kernel space
	set_fs(KERNEL_DS);
	
        lfile=sys_open(OffsetFileName,O_WRONLY|O_CREAT, 0777);
	if (lfile < 0)
	{
 	 printk("sys_open %s error!!. %ld\n",OffsetFileName,lfile);
	}
	else
	{
   	 sys_write(lfile, data,18);
	 sys_close(lfile);
	}
	set_fs(orgfs);

 return;
}

void gsensor_read_offset_from_file(void)
{
	unsigned int orgfs;
	char data[18];
	long lfile=-1;
	orgfs = get_fs();
// Set segment descriptor associated to kernel space
	set_fs(KERNEL_DS);

	lfile=sys_open(OffsetFileName, O_RDONLY, 0);
	if (lfile < 0)
	{
 	 printk("sys_open %s error!!. %ld\n",OffsetFileName,lfile);
	 if(lfile==-2)
	 {
           lfile=sys_open(OffsetFileName,O_WRONLY|O_CREAT, 0777);
	   if(lfile >=0)
	   {
	    strcpy(data,"00000 00000 00000");
 	    printk("sys_open %s OK!!. %ld\n",OffsetFileName,lfile);
	    sys_write(lfile,data,18);
	    sys_read(lfile, data, 18);
	    sys_close(lfile);
	   }
	  else
 	   printk("sys_open %s error!!. %ld\n",OffsetFileName,lfile);
	 }  
	 
	}
	else
	{
	 sys_read(lfile, data, 18);
	 sys_close(lfile);
	}
	//sscanf(data,"%hd %hd %hd",&offset.u.x,&offset.u.y,&offset.u.z); //marked by eason check again
	set_fs(orgfs);

}
*/
//*********************************************************************************************************
MODULE_AUTHOR("DMT_RD");
MODULE_DESCRIPTION("DMARD08 g-sensor Driver");
MODULE_LICENSE("GPL");

module_init(dmard08_init);
module_exit(dmard08_exit);

