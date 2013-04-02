/*
 * @file drivers/i2c/chips/dmard06.c
 * @brief DMARD06 g-sensor Linux device driver
 * @author Domintech Technology Co., Ltd (http://www.domintech.com.tw)
 * @version 1.0
 * @date 2011/8/5
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
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <mach/hardware.h>
#include <linux/miscdevice.h>

//////////////////////////////////////////////////////////
#define AKMIO				0xA1

/* IOCTLs for AKM library */
#define ECS_IOCTL_INIT                  _IO(AKMIO, 0x01)
#define ECS_IOCTL_WRITE                 _IOW(AKMIO, 0x02, char[5])
#define ECS_IOCTL_READ                  _IOWR(AKMIO, 0x03, char[5])
#define ECS_IOCTL_RESET      	          _IO(AKMIO, 0x04)
#define ECS_IOCTL_INT_STATUS            _IO(AKMIO, 0x05)
#define ECS_IOCTL_FFD_STATUS            _IO(AKMIO, 0x06)
#define ECS_IOCTL_SET_MODE              _IOW(AKMIO, 0x07, short)
#define ECS_IOCTL_GETDATA               _IOR(AKMIO, 0x08, char[RBUFF_SIZE+1])
#define ECS_IOCTL_GET_NUMFRQ            _IOR(AKMIO, 0x09, char[2])
#define ECS_IOCTL_SET_PERST             _IO(AKMIO, 0x0A)
#define ECS_IOCTL_SET_G0RST             _IO(AKMIO, 0x0B)
#define ECS_IOCTL_SET_YPR               _IOW(AKMIO, 0x0C, short[12])
#define ECS_IOCTL_GET_OPEN_STATUS       _IOR(AKMIO, 0x0D, int)
#define ECS_IOCTL_GET_CLOSE_STATUS      _IOR(AKMIO, 0x0E, int)
#define ECS_IOCTL_GET_CALI_DATA         _IOR(AKMIO, 0x0F, char[MAX_CALI_SIZE])
#define ECS_IOCTL_GET_DELAY             _IOR(AKMIO, 0x30, short)

/* IOCTLs for APPs */
#define ECS_IOCTL_APP_SET_MODE		_IOW(AKMIO, 0x10, short)
#define ECS_IOCTL_APP_SET_MFLAG		_IOW(AKMIO, 0x11, short)
#define ECS_IOCTL_APP_GET_MFLAG		_IOW(AKMIO, 0x12, short)
//#define ECS_IOCTL_APP_SET_AFLAG		_IOW(AKMIO, 0x13, short)
#define ECS_IOCTL_APP_GET_AFLAG		_IOR(AKMIO, 0x14, short)
#define ECS_IOCTL_APP_SET_TFLAG		_IOR(AKMIO, 0x15, short)
#define ECS_IOCTL_APP_GET_TFLAG		_IOR(AKMIO, 0x16, short)
#define ECS_IOCTL_APP_RESET_PEDOMETER   _IO(AKMIO, 0x17)
//#define ECS_IOCTL_APP_SET_DELAY		_IOW(AKMIO, 0x18, short)
#define ECS_IOCTL_APP_GET_DELAY		ECS_IOCTL_GET_DELAY
#define ECS_IOCTL_APP_SET_MVFLAG	_IOW(AKMIO, 0x19, short)	/* Set raw magnetic vector flag */
#define ECS_IOCTL_APP_GET_MVFLAG	_IOR(AKMIO, 0x1A, short)	/* Get raw magnetic vector flag */

#define WMTGSENSOR_IOCTL_MAGIC  0x09
#define WMT_IOCTL_SENSOR_CAL_OFFSET  _IOW(WMTGSENSOR_IOCTL_MAGIC, 0x01, int) //offset calibration
#define ECS_IOCTL_APP_SET_AFLAG		 _IOW(WMTGSENSOR_IOCTL_MAGIC, 0x02, short)
#define ECS_IOCTL_APP_SET_DELAY		 _IOW(WMTGSENSOR_IOCTL_MAGIC, 0x03, short)
#define WMT_IOCTL_SENSOR_GET_DRVID	 _IOW(WMTGSENSOR_IOCTL_MAGIC, 0x04, unsigned int)


/* IOCTLs for pedometer */
#define ECS_IOCTL_SET_STEP_CNT          _IOW(AKMIO, 0x20, short)
//////////////////////////////////////////////////////////////////
#define SENSOR_DELAY_FASTEST      0
#define SENSOR_DELAY_GAME        20
#define SENSOR_DELAY_UI          60
#define SENSOR_DELAY_NORMAL     200

#define DMARD06_DRVID 3

/////////////////////////////////////////////////////////////////

#undef dbg
#define dbg(fmt, args...) if (l_sensorconfig.isdbg) printk(KERN_ALERT "[%s]: " fmt, __FUNCTION__, ## args)

#undef errlog
#undef klog
#define errlog(fmt, args...) printk(KERN_ERR "[%s]: " fmt, __FUNCTION__, ## args)
#define klog(fmt, args...) printk(KERN_ALERT "[%s]: " fmt, __FUNCTION__, ## args)

#define DMARD06_I2C_NAME "dmard06"
#define DMARD06_I2C_ADDR 0x1c

#define GSENSOR_PROC_NAME "gsensor_config"
#define GSENSOR_MAJOR   161
#define GSENSOR_NAME    "dmard06"
#define GSENSOR_DRIVER_NAME  "dmard06_drv"

#define GSENDMARD06_UBOOT_NAME "wmt.io.d06sensor"

#define MAX_WR_DMARD06_LEN (1+1)

#define LSG    32

static char const *const ACCELEMETER_CLASS_NAME = "accelemeter";
static char const *const DMARD06_DEVICE_NAME = "dmard06";
////////////////////////////////////////////////////////////
#define ID_REG_ADDR       0x0F
#define SWRESET_REG_ADDR  0x53
#define T_REG_ADDR        0x40
#define XYZ_REG_ADDR      0x41
#define CTR1_REG_ADDR     0x44
#define CTR2_REG_ADDR     0x45
#define CTR3_REG_ADDR     0x46
#define CTR4_REG_ADDR     0x47
#define CTR5_REG_ADDR     0x48
#define STAT_REG_ADDR     0x49



static int dmard06_init(void);
static void dmard06_exit(void);

static int dmard06_file_open(struct inode*, struct file*);
static ssize_t dmard06_file_write(struct file*, const char*, size_t, loff_t*);
static ssize_t dmard06_file_read(struct file*, char*, size_t, loff_t*);
static int dmard06_file_close(struct inode*, struct file*);

static int dmard06_i2c_suspend(struct platform_device *pdev, pm_message_t state);
static int dmard06_i2c_resume(struct platform_device *pdev);
static int dmard06_i2c_probe(void);
static int dmard06_i2c_remove(void);
static void dmard06_i2c_read_xyz(s8 *x, s8 *y, s8 *z);
static void dmard06_i2c_accel_value(s8 *val);
static int dmard06_probe(
	struct platform_device *pdev);
static int dmard06_remove(struct platform_device *pdev);
static int dmard06_i2c_xyz_read_reg(u8* index ,u8 *buffer, int length);



extern int wmt_i2c_xfer_continue_if_4(struct i2c_msg *msg, unsigned int num, int bus_id);

extern int wmt_setsyspara(char *varname, unsigned char *varval);

/////////////////////////////////////////////////////////////////
struct work_struct poll_work;
static struct mutex sense_data_mutex;


struct dmard06_config
{
	int op;
	int int_gpio; //0-3
	int samp;
	int xyz_axis[3][3]; // (axis,direction)
	int irq;
	struct proc_dir_entry* sensor_proc;
	//int sensorlevel;
	//int shake_enable; // 1--enable shake, 0--disable shake
	//int manual_rotation; // 0--landance, 90--vertical
	struct input_dev *input_dev;
	//struct work_struct work;
	struct delayed_work work; // for polling
	struct workqueue_struct *queue;
	int isdbg; // 0-- no debug log, 1--show debug log
	int sensor_samp; // 
	int sensor_enable;  // 0 --> disable sensor, 1 --> enable sensor
	int test_pass;
	spinlock_t spinlock;
	int pollcnt; // the counts of polling
	int offset[3]; // for calibration
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend earlysuspend;
#endif
};

static struct dmard06_config l_sensorconfig = {
	.op = 0,
	.int_gpio = 3,
	.samp = 16,
	.xyz_axis = {
		{ABS_X, -1},
		{ABS_Y, 1},
		{ABS_Z, -1},
		},
	.irq = 6,
	.int_gpio = 3,
	.sensor_proc = NULL,
	//.sensorlevel = SENSOR_GRAVITYGAME_MODE,
	//.shake_enable = 0, // default enable shake
	.isdbg = 0,
	.sensor_samp = 1,  // 1 sample/second
	.sensor_enable = 1, // enable sensor
	.test_pass = 0, // for test program
	.pollcnt = 0, // Don't report the x,y,z when the driver is loaded until 2~3 seconds
	.offset = {0, 0, 0},
};

static struct class* l_dev_class = NULL;
static struct device *l_clsdevice = NULL;


struct raw_data
{
 	short x;
  	short y;
  	short z;
};

struct dev_data 
{
	dev_t devno;
  	struct cdev cdev;
  	struct class *class;
	struct i2c_client *client;
};

static struct dev_data dev;

struct file_operations dmard06_fops = 
{
	.owner = THIS_MODULE,
	.read = dmard06_file_read,
	.write = dmard06_file_write,
	.open = dmard06_file_open,
	.release = dmard06_file_close,
};

static int dmard06_file_open(struct inode *inode, struct file *filp)
{
	dbg("open...\n");
	
	return 0; 
}

static ssize_t dmard06_file_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	dbg("write...\n");
	
	return 0;
}

unsigned int sample_rate_2_memsec(unsigned int rate)
{
	return (1000/rate);
}

static int dmard06_packet_rptValue(int x, int y, int z)
{
	return ((0xFF&z) | ((0xFF&y)<<8) | ((0xFF&x)<<16));	
}


static void dmard06_work_func(struct work_struct *work)
{
	u8 buffer[3];
	//buffer[0] = 0x41;
	u8 index = 0x41;
	s8 x,y,z;
	int xyz,tx,ty,tz;

	mutex_lock(&sense_data_mutex);
	//read data
	dmard06_i2c_xyz_read_reg(&index, buffer, 3);
	mutex_unlock(&sense_data_mutex);
	// check whether it's valid
	// report the data
	x = (s8)buffer[0];   
	y = (s8)buffer[1];
	z = (s8)buffer[2];   
	dmard06_i2c_accel_value(&x);   
	dmard06_i2c_accel_value(&y);
	dmard06_i2c_accel_value(&z);
	tx = x*l_sensorconfig.xyz_axis[0][1]+l_sensorconfig.offset[0];
	ty = y*l_sensorconfig.xyz_axis[1][1]+l_sensorconfig.offset[1];
	tz = z*l_sensorconfig.xyz_axis[2][1]+l_sensorconfig.offset[2];
	xyz = dmard06_packet_rptValue(tx, ty, tz);
	input_report_abs(l_sensorconfig.input_dev, ABS_X, xyz);

	//input_report_abs(l_sensorconfig.input_dev, l_sensorconfig.xyz_axis[0][0], 
	//		x*l_sensorconfig.xyz_axis[0][1]+l_sensorconfig.offset[0]);
	//input_report_abs(l_sensorconfig.input_dev, l_sensorconfig.xyz_axis[1][0], 
	//		y*l_sensorconfig.xyz_axis[1][1]+l_sensorconfig.offset[1]);
	//input_report_abs(l_sensorconfig.input_dev, l_sensorconfig.xyz_axis[2][0], 
			//z*l_sensorconfig.xyz_axis[2][1]+l_sensorconfig.offset[2]);
	input_sync(l_sensorconfig.input_dev);
	dbg("x=%2x(tx=%2x),y=%2x(ty=%2x),z=%2x(tz=%2x),xyz=%x", 
		(char)x, (char)tx, (char)y, (char)ty, (char)z, (char)tz, xyz);
	
	// for next polling
	queue_delayed_work(l_sensorconfig.queue, &l_sensorconfig.work, msecs_to_jiffies(sample_rate_2_memsec(l_sensorconfig.sensor_samp)));
	//klog("%d=%d,%d=%d,%d=%d\n", l_sensorconfig.xyz_axis[0][0], x*l_sensorconfig.xyz_axis[0][1],
	//							l_sensorconfig.xyz_axis[1][0], y*l_sensorconfig.xyz_axis[1][1],
	//							l_sensorconfig.xyz_axis[2][0], z*l_sensorconfig.xyz_axis[2][1]);
	//klog("the polling period:%d\n", msecs_to_jiffies(sample_rate_2_memsec(l_sensorconfig.sensor_samp)));
	
}


static ssize_t dmard06_file_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{	
	int ret;
	s8 x, y, z;
	struct raw_data rdata;
	
	dbg("read...\n");
	mutex_lock(&sense_data_mutex);
	dmard06_i2c_read_xyz(&x, &y, &z);
	rdata.x = x;
	rdata.y = y;
	rdata.z = z;

	ret = copy_to_user(buf, &rdata, count);
	mutex_unlock(&sense_data_mutex);
	
	return count;
}

static int dmard06_file_close(struct inode *inode, struct file *filp)
{
	dbg("close...\n");
	
	return 0;
}

static void dmard06_platform_release(struct device *device)
{
    return;
}


static struct platform_device dmard06_device = {
    .name           = GSENSOR_NAME,
    .id             = 0,
    .dev            = {
    	.release = dmard06_platform_release,
    },
};

static struct platform_driver dmard06_driver = {
	.probe = dmard06_probe,
	.remove = dmard06_remove,
	.suspend	= dmard06_i2c_suspend,
	.resume		= dmard06_i2c_resume,
	.driver = {
		   .name = GSENSOR_NAME,
		   },
};

static int dmard06_i2c_xyz_write_reg(u8* index ,u8 *buffer, int length)
{
	int ret = 0;
	u8 buf[MAX_WR_DMARD06_LEN];
	struct i2c_msg msg[1];

	buf[0] = *index;
	memcpy(buf+1, buffer, length);
	msg[0].addr = DMARD06_I2C_ADDR;
    msg[0].flags = 0 ;
    msg[0].flags &= ~(I2C_M_RD);
    msg[0].len = length+1;
    msg[0].buf = buf;
	if ((ret = wmt_i2c_xfer_continue_if_4(msg,1,0)) <= 0)
	{
		errlog("write error!\n");
	}
	return ret;
}

static int dmard06_i2c_xyz_read_reg(u8* index ,u8 *buffer, int length)
{
	int ret = 0;	
	
	struct i2c_msg msg[] = 
	{
		{.addr = DMARD06_I2C_ADDR, .flags = 0, .len = 1, .buf = index,}, 
		{.addr = DMARD06_I2C_ADDR, .flags = I2C_M_RD, .len = length, .buf = buffer,},
	};
	ret = wmt_i2c_xfer_continue_if_4(msg, 2,0);
	if (ret <= 0)
	{
		errlog("read error!\n");
	}
	return ret;
}

static void dmard06_i2c_read_xyz(s8 *x, s8 *y, s8 *z)
{
		
	u8 buffer[3];
	//buffer[0] = 0x41;
	u8 index = 0x41;
	
	dmard06_i2c_xyz_read_reg(&index, buffer, 3);
	*x = (s8)buffer[0];   
	*y = (s8)buffer[1];
	*z = (s8)buffer[2];   
	dmard06_i2c_accel_value(x);   
	dmard06_i2c_accel_value(y);
	dmard06_i2c_accel_value(z);
	if (ABS_X == l_sensorconfig.xyz_axis[0][0])
	{
		*x = l_sensorconfig.xyz_axis[0][1]*(*x);
		*y = l_sensorconfig.xyz_axis[1][1]*(*y);
	} else {
		*x = l_sensorconfig.xyz_axis[0][1]*(*y);
		*y = l_sensorconfig.xyz_axis[1][1]*(*x);
	}
	*z = l_sensorconfig.xyz_axis[2][1]*(*z);
	
	dbg("dmrd06:x=%x,y=%x,z=%x\n", *x, *y, *z);
}

static void dmard06_i2c_accel_value(s8 *val)
{
	*val >>= 1;
}

static int dmard06_CalOffset(int side)
{
	u8 buffer[3];
	//buffer[0] = 0x41;
	u8 index = 0x41;
	s8 x,y,z;

	//mutex_lock(&sense_data_mutex);
	//read data
	dmard06_i2c_xyz_read_reg(&index, buffer, 3);
	//mutex_unlock(&sense_data_mutex);
	// check whether it's valid
	// report the data
	x = (s8)buffer[0];   
	y = (s8)buffer[1];
	z = (s8)buffer[2];   
	dmard06_i2c_accel_value(&x);   
	dmard06_i2c_accel_value(&y);
	dmard06_i2c_accel_value(&z);
	l_sensorconfig.offset[0] = 0 - x*l_sensorconfig.xyz_axis[0][1];
	l_sensorconfig.offset[1] = 0 - y*l_sensorconfig.xyz_axis[1][1];
	l_sensorconfig.offset[2] = LSG - z*l_sensorconfig.xyz_axis[2][1];
	return 0;

}

static int dmard06_i2c_suspend(struct platform_device *pdev, pm_message_t state)
{
	dbg("...\n");
	//cancel_delayed_work_sync(&l_sensorconfig.work);
	
	return 0;
}

static int  is_dmard06(void)
{
	int err = 0;
	u8 cAddress = 0, cData = 0;
	char buf[4];

	cAddress = 0x53;
	//i2c_master_send( client, (char*)&cAddress, 1);
	//i2c_master_recv( client, (char*)&cData, 1);
	if (dmard06_i2c_xyz_read_reg(&cAddress, &cData,1) <= 0)
	{
		errlog("Error to read SW_RESET register!\n");
	}
	dbg("i2c Read 0x53 = %x \n", cData);

	cAddress = 0x0f;
	//i2c_master_send( client, (char*)&cAddress, 1);
	//i2c_master_recv( client, (char*)&cData, 1);
	if (dmard06_i2c_xyz_read_reg(&cAddress, &cData,1) <= 0)
	{
		errlog("Can't find dmard06!\n");
		return -1;
	}
	dbg("i2c Read 0x0f = %d \n", cData);

	if(( cData&0x00FF) == 0x0006)
	{
		klog("Find DMARD06!\n");
	}
	else
	{
		errlog("ID isn't 0x06.(0x%x) !\n",cData);
		return -1;
	}		
	
	return 0;
}

static int dmard06_i2c_remove(void)
{
	
	return 0;
}

static int dmard06_i2c_resume(struct platform_device *pdev)
{
	dbg("...\n");
	//queue_delayed_work(l_sensorconfig.queue, &l_sensorconfig.work, msecs_to_jiffies(sample_rate_2_memsec(l_sensorconfig.sensor_samp)));

	
	return 0;
}

static int sensor_writeproc( struct file   *file,
                           const char    *buffer,
                           unsigned long count,
                           void          *data )
{

	int inputval = -1;
	int enable, sample = -1;
	char tembuf[8];
	unsigned int amsr = 0;
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
			}			
			//printk(KERN_ALERT "sensor samp=%d(amsr:%d) has been set.\n", sample, amsr);
		} else {
			printk(KERN_ALERT "Wrong sample argumnet of sensor.\n");
		}
	} else if (sscanf(buffer, "enable=%d\n", &enable))
	{
		if ((enable < 0) || (enable > 1))
		{
			printk(KERN_ERR "The argument to enable/disable g-sensor should be 0 or 1  !!!\n");
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


extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);
static int get_axisset(void* param)
{
	char varbuf[64];
	int n;
	int varlen;

	memset(varbuf, 0, sizeof(varbuf));
	varlen = sizeof(varbuf);
	if (wmt_getsyspara("wmt.io.d06sensor", varbuf, &varlen)) {
		printk(KERN_DEBUG "Can't get gsensor config in u-boot!!!!\n");
		return -1;
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
				&l_sensorconfig.offset[0],
				&l_sensorconfig.offset[1],
				&l_sensorconfig.offset[2]);
		if (n != 12) {
			printk(KERN_ERR "gsensor format is error in u-boot!!!\n");
			return -1;
		}
		l_sensorconfig.sensor_samp = l_sensorconfig.samp;

		dbg("get the sensor config: %d:%d:%d:%d:%d:%d:%d:%d:%d\n",
			l_sensorconfig.op,
			l_sensorconfig.int_gpio,
			l_sensorconfig.samp,
			l_sensorconfig.xyz_axis[0][0],
			l_sensorconfig.xyz_axis[0][1],
			l_sensorconfig.xyz_axis[1][0],
			l_sensorconfig.xyz_axis[1][1],
			l_sensorconfig.xyz_axis[2][0],
			l_sensorconfig.xyz_axis[2][1]
		);
	}
	return 0;
}

// To contol the g-sensor for UI 
static int mmad_open(struct inode *inode, struct file *file)
{
	dbg("Open the g-sensor node...\n");
	return 0;
}

static int mmad_release(struct inode *inode, struct file *file)
{
	dbg("Close the g-sensor node...\n");
	return 0;
}


static int
mmad_ioctl(/*struct inode *inode,*/ struct file *file, unsigned int cmd,
	   unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	char rwbuf[5];
	short delay, enable,  amsr = -1;
	unsigned int sample;
	int ret = 0;
	int side;
	char varbuff[80];
	unsigned int uval = 0;

	dbg("g-sensor ioctr...\n");
	memset(rwbuf, 0, sizeof(rwbuf));
	mutex_lock(&sense_data_mutex);
	switch (cmd) {
		case ECS_IOCTL_APP_SET_DELAY:
			// set the rate of g-sensor
			if (copy_from_user(&delay, argp, sizeof(short)))
			{
				printk(KERN_ALERT "Can't get set delay!!!\n");
				ret = -EFAULT;
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
				ret = -EFAULT;
				goto errioctl;	
			}
			break;
		case ECS_IOCTL_APP_SET_AFLAG:
			// enable/disable sensor
			if (copy_from_user(&enable, argp, sizeof(short)))
			{
				printk(KERN_ERR "Can't get enable flag!!!\n");
				ret = -EFAULT;
				goto errioctl;				
			}
			if ((enable >=0) && (enable <=1))
			{
				dbg("driver: disable/enable(%d) gsensor.\n", enable);
				
				if (enable != l_sensorconfig.sensor_enable)
				{
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
				printk(KERN_ERR "Wrong enable argument in %s !!!\n", __FUNCTION__);
				ret = -EFAULT;
				goto errioctl;
			}
			break;
		case WMT_IOCTL_SENSOR_GET_DRVID:
			uval = DMARD06_DRVID;
			if (copy_to_user((unsigned int*)arg, &uval, sizeof(unsigned int)))
			{
				ret = -EFAULT;
				goto errioctl;
			}
			dbg("dmard06_driver_id:%d\n",uval);
			break;
		case WMT_IOCTL_SENSOR_CAL_OFFSET:
			klog("-->WMT_IOCTL_SENSOR_CAL_OFFSET\n");
			if(copy_from_user(&side, (int*)argp, sizeof(int)))
			{
				ret = -EFAULT;
				goto errioctl;
			}
			dbg("side=%d\n",side);
			if (dmard06_CalOffset(side) != 0)
			{
				ret = -EFAULT;
				goto errioctl;
			}
			// save the param
			sprintf(varbuff, "%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d",
					l_sensorconfig.op,
					l_sensorconfig.int_gpio,
					10,//l_sensorconfig.samp,
					(l_sensorconfig.xyz_axis[0][0]),
					(l_sensorconfig.xyz_axis[0][1]),
					(l_sensorconfig.xyz_axis[1][0]),
					(l_sensorconfig.xyz_axis[1][1]),
					(l_sensorconfig.xyz_axis[2][0]),
					(l_sensorconfig.xyz_axis[2][1]),
					l_sensorconfig.offset[0],
					l_sensorconfig.offset[1],
					l_sensorconfig.offset[2]
				);
			wmt_setsyspara(GSENDMARD06_UBOOT_NAME, varbuff);
			ret = 0;
			break;
		default:
			break;
	}


	/*switch (cmd) {
	case ECS_IOCTL_READ:
		if (copy_to_user(argp, &rwbuf, sizeof(rwbuf)))
			return -EFAULT;
		break;
	default:
		break;
	}*/
errioctl:
	mutex_unlock(&sense_data_mutex);
	return ret;
}


static struct file_operations mmad_fops = {
	.owner = THIS_MODULE,
	.open = mmad_open,
	.release = mmad_release,
	.unlocked_ioctl = mmad_ioctl,
};


static struct miscdevice mmad_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sensor_ctrl",
	.fops = &mmad_fops,
};

static int dmard06_probe(
	struct platform_device *pdev)
{
	int err = 0;

	//register ctrl dev
	err = misc_register(&mmad_device);
	if (err != 0)
	{
		errlog("Can't register mma_device!\n");
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
	l_sensorconfig.queue = create_singlethread_workqueue("sensor-data-report");
	//INIT_WORK(&l_sensorconfig.work, mma_work_func);
	INIT_DELAYED_WORK(&l_sensorconfig.work, dmard06_work_func);
	mutex_init(&sense_data_mutex);
	l_sensorconfig.input_dev = input_allocate_device();
	if (!l_sensorconfig.input_dev) {
		err = -ENOMEM;
		errlog("Failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}
	//set_bit(EV_KEY, l_sensorconfig.input_dev->evbit);
	//set_bit(EV_ABS, l_sensorconfig.input_dev->evbit);
	l_sensorconfig.input_dev->evbit[0] = BIT(EV_ABS) | BIT_MASK(EV_KEY);
	//set_bit(KEY_NEXTSONG, l_sensorconfig.input_dev->keybit);
	
	/* yaw */
	//input_set_abs_params(l_sensorconfig.input_dev, ABS_RX, 0, 360*100, 0, 0);
	/* pitch */
	//input_set_abs_params(l_sensorconfig.input_dev, ABS_RY, -180*100, 180*100, 0, 0);
	/* roll */
	//input_set_abs_params(l_sensorconfig.input_dev, ABS_RZ, -90*100, 90*100, 0, 0);
	/* x-axis acceleration */
	input_set_abs_params(l_sensorconfig.input_dev, ABS_X, -128, 128, 0, 0);
	/* y-axis acceleration */
	input_set_abs_params(l_sensorconfig.input_dev, ABS_Y, -128, 128, 0, 0);
	/* z-axis acceleration */
	input_set_abs_params(l_sensorconfig.input_dev, ABS_Z, -128, 128, 0, 0);

	l_sensorconfig.input_dev->name = "g-sensor";

	err = input_register_device(l_sensorconfig.input_dev);

	if (err) {
		errlog("Unable to register input device: %s\n",
		       l_sensorconfig.input_dev->name);
		goto exit_input_register_device_failed;
	}

	return 0;
exit_input_register_device_failed:
	input_free_device(l_sensorconfig.input_dev);	
exit_input_dev_alloc_failed:
	// release proc
	remove_proc_entry(GSENSOR_PROC_NAME, NULL);
	l_sensorconfig.sensor_proc = NULL;
	// unregister the ctrl dev
	misc_deregister(&mmad_device);
	return err;
}

static int dmard06_remove(struct platform_device *pdev)
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
	misc_deregister(&mmad_device);
	return 0;
}

static void dmard06_early_suspend(struct early_suspend *h)
{
	dbg("start\n");
	cancel_delayed_work_sync(&l_sensorconfig.work);
	dbg("exit\n");
}

static void dmard06_late_resume(struct early_suspend *h)
{
	dbg("start\n");
	// init 
	queue_delayed_work(l_sensorconfig.queue, &l_sensorconfig.work, msecs_to_jiffies(sample_rate_2_memsec(l_sensorconfig.sensor_samp)));
	dbg("exit\n");
}


static int __init dmard06_init(void)
{
	int ret = 0;

	// detech the device
	if (is_dmard06() != 0)
	{
		return -1;
	}
	// parse g-sensor u-boot arg
	ret = get_axisset(NULL); 
	/*if ((ret != 0) || !l_sensorconfig.op)
		return -EINVAL;
	*/

	// Create device node
	if (register_chrdev (GSENSOR_MAJOR, GSENSOR_NAME, &dmard06_fops)) {
		printk (KERN_ERR "unable to get major %d\n", GSENSOR_MAJOR);
		return -EIO;
	}	

	l_dev_class = class_create(THIS_MODULE, GSENSOR_NAME);
	if (IS_ERR(l_dev_class)){
		ret = PTR_ERR(l_dev_class);
		printk(KERN_ERR "Can't class_create gsensor device !!\n");
		return ret;
	}
	l_clsdevice = device_create(l_dev_class, NULL, MKDEV(GSENSOR_MAJOR, 0), NULL, GSENSOR_NAME);
	if (IS_ERR(l_clsdevice)){
		ret = PTR_ERR(l_clsdevice);
		printk(KERN_ERR "Failed to create device %s !!!",GSENSOR_NAME);
		return ret;
	}
	INIT_WORK(&poll_work, dmard06_work_func);


    if((ret = platform_device_register(&dmard06_device)))
    {
    	printk(KERN_ERR "%s Can't register mma7660 platform devcie!!!\n", __FUNCTION__);
    	return ret;
    }
    if ((ret = platform_driver_register(&dmard06_driver)) != 0)
    {
    	printk(KERN_ERR "%s Can't register mma7660 platform driver!!!\n", __FUNCTION__);
    	return ret;
    }
#ifdef CONFIG_HAS_EARLYSUSPEND
    l_sensorconfig.earlysuspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
    l_sensorconfig.earlysuspend.suspend = dmard06_early_suspend;
    l_sensorconfig.earlysuspend.resume = dmard06_late_resume;
    register_early_suspend(&l_sensorconfig.earlysuspend);
#endif

    klog("dmard06 g-sensor driver load!\n");
    queue_delayed_work(l_sensorconfig.queue, &l_sensorconfig.work, msecs_to_jiffies(sample_rate_2_memsec(l_sensorconfig.sensor_samp)));

	return 0;
}

static void __exit dmard06_exit(void)
{
	unregister_early_suspend(&l_sensorconfig.earlysuspend);
    platform_driver_unregister(&dmard06_driver);
    platform_device_unregister(&dmard06_device);
    device_destroy(l_dev_class, MKDEV(GSENSOR_MAJOR, 0));
	unregister_chrdev(GSENSOR_MAJOR, GSENSOR_NAME);
	class_destroy(l_dev_class);
	
}

MODULE_AUTHOR("DMT_RD");
MODULE_DESCRIPTION("DMARD06 g-sensor Driver");
MODULE_LICENSE("GPL");

module_init(dmard06_init);
module_exit(dmard06_exit);
