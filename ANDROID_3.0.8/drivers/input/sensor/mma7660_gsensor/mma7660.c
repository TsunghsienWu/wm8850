#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/freezer.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <mach/hardware.h>
#include "mma7660.h"


//#define DEBUG 1

#undef dbg

//#if 0
	#define dbg(fmt, args...) if (l_sensorconfig.isdbg) 	printk(KERN_ALERT "[%s]: " fmt, __FUNCTION__, ## args) 
	//#define dbg(fmt, args...) printk(KERN_ALERT "[%s]: " fmt, __FUNCTION__, ## args) 
	
#define klog(fmt, args...) 	printk(KERN_ALERT "[%s]: " fmt, __FUNCTION__, ## args) 
	
	//#define dbg(format, arg...) printk(KERN_ALERT format, ## arg)


//#else
//	#define dbg(format, arg...)
//#endif

/////////////////////Macro constant
#define SENSOR_POLL_WAIT_TIME   1837
#define MAX_FAILURE_COUNT 10
#define MMA7660_ADDR	0x4C
#define LAND_PORT_MASK 0x1C
#define LAND_LEFT 0x1
#define LAND_RIGHT 0x2
#define PORT_INVERT 0x5
#define PORT_NORMAL 0x6

#define LANDSCAPE_LOCATION 0
#define PORTRAIT_LOCATION  1

#define SENSOR_UI_MODE 0
#define SENSOR_GRAVITYGAME_MODE 1

#define UI_SAMPLE_RATE 0xFC

#define GSENSOR_PROC_NAME "gsensor_config"
#define GSENSOR_MAJOR   161
#define GSENSOR_NAME    "mma7660"
#define GSENSOR_DRIVER_NAME  "mma7660_drv"


#define sin30_1000  500
#define	cos30_1000  866

#define DISABLE 0
#define ENABLE 1

////////////////////////the rate of g-sensor/////////////////////////////////////////////
#define SENSOR_DELAY_FASTEST      0
#define SENSOR_DELAY_GAME        20
#define SENSOR_DELAY_UI          60
#define SENSOR_DELAY_NORMAL     200

#define FASTEST_MMA_AMSR 0 // 120 samples/sec
#define GAME_MMA_AMSR    1 // 1,   (64, samples/sec)
#define UI_MMA_AMSR      3 // 2, 3,4,   (16, 8,32 samples/sec)
#define NORMAL_MMA_AMSR  5 // 5, 6, 7   (4, 2, 1 samples/sec)

/////////////////////////////////////////////////////////////////////////
static int xyz_g_table[64] = {
0,47,94,141,188,234,281,328,
375,422,469,516,563,609,656,703,
750,797,844,891,938,984,1031,1078,
1125,1172,1219,1266,1313,1359,1406,1453,
-1500,-1453,-1406,-1359,-1313,-1266,-1219,-1172,
-1125,-1078,-1031,-984,-938,-891,-844,-797,
-750,-703,-656,-609,-563,-516,-469,-422,
-375,-328,-281,-234,-188,-141,-94,-47
};

static struct platform_device *this_pdev;

static struct class* l_dev_class = NULL;
static struct device *l_clsdevice = NULL;



struct mma7660_config
{
	int op;
	int int_gpio; //0-3
	int xyz_axis[3][2]; // (axis,direction)
	int rxyz_axis[3][2];
	int irq;
	struct proc_dir_entry* sensor_proc;
	int sensorlevel;
	int shake_enable; // 1--enable shake, 0--disable shake
	int manual_rotation; // 0--landance, 90--vertical
	struct input_dev *input_dev;
	//struct work_struct work;
	struct delayed_work work; // for polling
	struct workqueue_struct *queue;
	int isdbg; // 0-- no debug log, 1--show debug log
	int sensor_samp; // 1,2,4,8,16,32,64,120
	int sensor_enable;  // 0 --> disable sensor, 1 --> enable sensor
	int test_pass;
	spinlock_t spinlock;
	int pollcnt; // the counts of polling
	int offset[3];
};

static struct mma7660_config l_sensorconfig = {
	.op = 0,
	.int_gpio = 3,
	.xyz_axis = {
		{ABS_X, -1},
		{ABS_Y, 1},
		{ABS_Z, -1},
		},
	.irq = 6,
	.int_gpio = 3,
	.sensor_proc = NULL,
	.sensorlevel = SENSOR_GRAVITYGAME_MODE,
	.shake_enable = 0, // default enable shake
	.isdbg = 0,
	.sensor_samp = 10,  // 4sample/second
	.sensor_enable = 1, // enable sensor
	.test_pass = 0, // for test program
	.pollcnt = 0, // Don't report the x,y,z when the driver is loaded until 2~3 seconds
	.offset = {0,0,0},
};


static struct timer_list l_polltimer; // for shaking
static int is_sensor_good = 0; // 1-- work well, 0 -- exception
struct work_struct poll_work;
static struct mutex sense_data_mutex;
static int revision = -1;
static int l_resumed = 0; // 1: suspend --> resume;2: suspend but not resumed; other values have no meaning

//////////////////Macro function//////////////////////////////////////////////////////

#define SET_MMA_SAMPLE(buf,samp) { \
	buf[0] = 0; \
	sensor_i2c_write(MMA7660_ADDR,7,buf,1); \
	buf[0] = samp; \
	sensor_i2c_write(MMA7660_ADDR,8,buf,1); \
	buf[0] = 0x01; \
	sensor_i2c_write(MMA7660_ADDR,7,buf,1); \
}

////////////////////////Function define/////////////////////////////////////////////////////////

static unsigned int mma_sample2AMSR(unsigned int samp);

////////////////////////Function implement/////////////////////////////////////////////////
// rate: 1,2,4,8,16,32,64,120
static unsigned int sample_rate_2_memsec(unsigned int rate)
{
	return (1000/rate);
}



static ssize_t gsensor_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	sprintf(buf, "MMA7660_%#x\n", revision);
	ret = strlen(buf) + 1;

	return ret;
}

static DEVICE_ATTR(vendor, 0444, gsensor_vendor_show, NULL);

static struct kobject *android_gsensor_kobj;
static int gsensor_sysfs_init(void)
{
	int ret ;

	android_gsensor_kobj = kobject_create_and_add("android_gsensor", NULL);
	if (android_gsensor_kobj == NULL) {
		printk(KERN_ERR
		       "mma7660 gsensor_sysfs_init:"\
		       "subsystem_register failed\n");
		ret = -ENOMEM;
		goto err;
	}

	ret = sysfs_create_file(android_gsensor_kobj, &dev_attr_vendor.attr);
	if (ret) {
		printk(KERN_ERR
		       "mma7660 gsensor_sysfs_init:"\
		       "sysfs_create_group failed\n");
		goto err4;
	}

	return 0 ;
err4:
	kobject_del(android_gsensor_kobj);
err:
	return ret ;
}

static int gsensor_sysfs_exit(void)
{
	sysfs_remove_file(android_gsensor_kobj, &dev_attr_vendor.attr);
	kobject_del(android_gsensor_kobj);
	return 0;
}

extern int wmt_i2c_xfer_continue_if_4(struct i2c_msg *msg, unsigned int num, int bus_id);

int sensor_i2c_write(unsigned int addr,unsigned int index,char *pdata,int len)
{
    struct i2c_msg msg[1];
	unsigned char buf[len+1];

	//addr = (addr >> 1);
    buf[0] = index;
	memcpy(&buf[1],pdata,len);
    msg[0].addr = addr;
    msg[0].flags = 0 ;
    msg[0].flags &= ~(I2C_M_RD);
    msg[0].len = len+1;
    msg[0].buf = buf;
//tmp    sensor_i2c_do_xfer(msg,1);
	if (wmt_i2c_xfer_continue_if_4(msg,1,0) <= 0)
	{
		klog("write error!\n");
		return -1;
	}

#ifdef DEBUG
{
	int i;

	printk("sensor_i2c_write(addr 0x%x,index 0x%x,len %d\n",addr,index,len);
	for(i=0;i<len;i+=8){
		printk("%d : 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",i,
			pdata[i],pdata[i+1],pdata[i+2],pdata[i+3],pdata[i+4],pdata[i+5],pdata[i+6],pdata[i+7]);
	}
}
#endif
    return 0;
} /* End of sensor_i2c_write */

int sensor_i2c_read(unsigned int addr,unsigned int index,char *pdata,int len)
{
	struct i2c_msg msg[2];
	unsigned char buf[len+1];
	int ret = 0;

	//addr = (addr >> 1);
	memset(buf,0x55,len+1);
	buf[0] = index;
	buf[1] = 0x0;

	msg[0].addr = addr;
	msg[0].flags = 0 ;
	msg[0].flags &= ~(I2C_M_RD);
	msg[0].len = 1;
	msg[0].buf = buf;

	msg[1].addr = addr;
	msg[1].flags = 0 ;
	msg[1].flags |= (I2C_M_RD);
	msg[1].len = len;
	msg[1].buf = buf;

	//tmp	sensor_i2c_do_xfer(msg, 2);
	ret = wmt_i2c_xfer_continue_if_4(msg, 2,0);
	if (ret < 0)
	{	
		klog("read error!\n");
		return ret;
	}

	memcpy(pdata,buf,len);
#ifdef DEBUG
{
	int i;

	printk("sensor_i2c_read(addr 0x%x,index 0x%x,len %d\n",addr,index,len);
	for(i=0;i<len;i+=8){
		printk("%d : 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",i,
			pdata[i],pdata[i+1],pdata[i+2],pdata[i+3],pdata[i+4],pdata[i+5],pdata[i+6],pdata[i+7]);
	}
}
#endif
    return 0;
} /* End of sensor_i2c_read */

void mma7660_chip_init(void)
{
	char txData[1];
	char amsr = 0;

	// the default mode is for UI
	txData[0]=0;
	sensor_i2c_write(MMA7660_ADDR,7,txData,1);
	txData[0] = 0;
	sensor_i2c_write(MMA7660_ADDR,5,txData,1);
	//txData[0] = (1 == l_sensorconfig.shake_enable) ? 0xF0:0x10;
	txData[0] = 0; // disable all int ,for polling
	sensor_i2c_write(MMA7660_ADDR,6,txData,1);
	amsr = 0;// 8 - i;
	txData[0] = 0xF8 | amsr;
	dbg("G-Sensor: SR = 0x%X\n", txData[0]);
	sensor_i2c_write(MMA7660_ADDR,8,txData,1);
	txData[0] = 0x01; // for polling
	sensor_i2c_write(MMA7660_ADDR,7,txData,1);
	return;
}

static unsigned int mma_sample2AMSR(unsigned int samp)
{
	int i = 0;
	unsigned int amsr;
	
	if (samp >= 120)
	{
		return 0;
	}
	while (samp)
	{
		samp = samp >> 1;
		i++;
	}
	amsr = 8 - i;
	return amsr;
}

static int mma_enable_disable(int enable)
{
	char buf[1];
	
	// disable all interrupt of g-sensor
	memset(buf, 0, sizeof(buf));
	if ((enable < 0) || (enable > 1))
	{
		return -1;
	}
	buf[0] = 0;
	sensor_i2c_write(MMA7660_ADDR,7,buf,1);
	if (enable != 0)
	{
		buf[0] = (1 == l_sensorconfig.shake_enable) ? 0xF0:0x10;
	} else {
		buf[0] = 0;
	}
	sensor_i2c_write(MMA7660_ADDR,6,buf,1);
	buf[0] = 0xf9;
	sensor_i2c_write(MMA7660_ADDR,7,buf,1);
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
	short delay, enable;  //amsr = -1;
	unsigned int uval = 0;

	dbg("g-sensor ioctr...\n");
	memset(rwbuf, 0, sizeof(rwbuf));
	switch (cmd) {
		case ECS_IOCTL_APP_SET_DELAY:
			// set the rate of g-sensor
			if (copy_from_user(&delay, argp, sizeof(short)))
			{
				printk(KERN_ALERT "Can't get set delay!!!\n");
				return -EFAULT;
			}
			klog("Get delay=%d\n", delay);
			//klog("before change sensor sample:%d...\n", l_sensorconfig.sensor_samp);
			if ((delay >=0) && (delay < 20))
			{
				delay = 20;
			} else if (delay > 200) 
			{
				delay = 200;
			}
			l_sensorconfig.sensor_samp = 1000/delay;			
			
			break;
		case ECS_IOCTL_APP_SET_AFLAG:
			// enable/disable sensor
			if (copy_from_user(&enable, argp, sizeof(short)))
			{
				printk(KERN_ERR "Can't get enable flag!!!\n");
				return -EFAULT;
			}
			klog("enable=%d\n",enable);
			if ((enable >=0) && (enable <=1))
			{
				dbg("driver: disable/enable(%d) gsensor.\n", enable);
				
				l_sensorconfig.sensor_enable = enable;
				
			} else {
				printk(KERN_ERR "Wrong enable argument in %s !!!\n", __FUNCTION__);
				return -EINVAL;
			}
			break;
		case WMT_IOCTL_SENSOR_GET_DRVID:
			uval = MMA7660_DRVID;
			if (copy_to_user((unsigned int*)arg, &uval, sizeof(unsigned int)))
			{
				return -EFAULT;
			}
			dbg("mma7660_driver_id:%d\n",uval);
			break;
		default:
			break;
	}



	return 0;
}


static void mma_work_func(struct work_struct *work)
{
	struct mma7660_config *data;
	unsigned char rxData[3];
	//unsigned char tiltval = 0;
	int i = 0;
	int x,y,z;

	dbg("enter...\n");
	data = dev_get_drvdata(&this_pdev->dev);
	if (!l_sensorconfig.sensor_enable)
	{
		queue_delayed_work(l_sensorconfig.queue, &l_sensorconfig.work, msecs_to_jiffies(sample_rate_2_memsec(l_sensorconfig.sensor_samp)));
		return;
	}
	mutex_lock(&sense_data_mutex);
	is_sensor_good = 1; // for watchdog
	l_sensorconfig.test_pass = 1; // for testing
	
	/*sensor_i2c_read(MMA7660_ADDR,3,rxData,1);
	tiltval = rxData[0];
	if (tiltval & 0x80) {
		if (1 == l_sensorconfig.shake_enable) {
			printk(KERN_NOTICE "shake!!!\n");
			input_report_key(data->input_dev, KEY_NEXTSONG, 1);
			input_report_key(data->input_dev, KEY_NEXTSONG, 0);
			input_sync(data->input_dev);
			
		}
	} else*/ {
		sensor_i2c_read(MMA7660_ADDR,0,rxData,3);
	/*	dbg(KERN_DEBUG "Angle: x=%d, y=%d, z=%d\n",
			xy_degree_table[rxData[0]],
			xy_degree_table[rxData[1]],
			z_degree_table[rxData[2]]);
		dbg(KERN_DEBUG "G value: x=%d, y=%d, z=%d\n",
			xyz_g_table[rxData[0]],
			xyz_g_table[rxData[1]],
			xyz_g_table[rxData[2]]);
			*/
		for (i=0; i < 3; i++)
		{
			if (rxData[i]&0x40) break;
		}
		//if (l_sensorconfig.pollcnt >= 20)
		if (3 == i)
		{
			x = xyz_g_table[rxData[l_sensorconfig.xyz_axis[0][0]]]*l_sensorconfig.xyz_axis[0][1];
			y = xyz_g_table[rxData[l_sensorconfig.xyz_axis[1][0]]]*l_sensorconfig.xyz_axis[1][1];
			z = xyz_g_table[rxData[l_sensorconfig.xyz_axis[2][0]]]*l_sensorconfig.xyz_axis[2][1];
			input_report_abs(data->input_dev, ABS_X, x);
			input_report_abs(data->input_dev, ABS_Y, y);
			input_report_abs(data->input_dev, ABS_Z, z);
			input_sync(data->input_dev);
			dbg("gx=%x,gy=%x,gz=%x\n",x,y,z);
		}
	}
	//gsensor_int_ctrl(ENABLE);
	mutex_unlock(&sense_data_mutex);
	queue_delayed_work(l_sensorconfig.queue, &l_sensorconfig.work, msecs_to_jiffies(sample_rate_2_memsec(l_sensorconfig.sensor_samp)));
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
	// disable int
	//gsensor_int_ctrl(DISABLE);
	memset(tembuf, 0, sizeof(tembuf));
	// get sensor level and set sensor level
	if (sscanf(buffer, "level=%d\n", &l_sensorconfig.sensorlevel))
	{
	} else if (sscanf(buffer, "shakenable=%d\n", &l_sensorconfig.shake_enable))
	{
		/*
		regval[0] = 0;
		sensor_i2c_write(MMA7660_ADDR,7,regval,1); // standard mode
		sensor_i2c_read(MMA7660_ADDR,6,regval,1);
		switch (l_sensorconfig.shake_enable)
		{
			case 0: // disable shake
				regval[0] &= 0x1F;
				sensor_i2c_write(MMA7660_ADDR,6, regval,1);
				dbg("Shake disable!!\n");
				break;
			case 1: // enable shake
				regval[0] |= 0xE0;
				sensor_i2c_write(MMA7660_ADDR,6, regval,1);
				dbg("Shake enable!!\n");
				break;
		};
		sensor_i2c_write(MMA7660_ADDR,7,&oldval,1);
		*/
	} /*else if (sscanf(buffer, "rotation=%d\n", &l_sensorconfig.manual_rotation))
	{
		switch (l_sensorconfig.manual_rotation)
		{
			case 90:
				// portrait
				input_report_abs(mma7660data->input_dev, ABS_X, cos30_1000);
				input_report_abs(mma7660data->input_dev, ABS_Y, 0);
				input_report_abs(mma7660data->input_dev, ABS_Z, sin30_1000);
				input_sync(mma7660data->input_dev);
				break;
			case 0:
				// landscape
				input_report_abs(mma7660data->input_dev, ABS_X, 0);
				input_report_abs(mma7660data->input_dev, ABS_Y, cos30_1000);
				input_report_abs(mma7660data->input_dev, ABS_Z, sin30_1000);
				input_sync(mma7660data->input_dev);
				break;
		};
	}*/ else if (sscanf(buffer, "isdbg=%d\n", &l_sensorconfig.isdbg))
	{
		// only set the dbg flag
	} else if (sscanf(buffer, "init=%d\n", &inputval))
	{
		mma7660_chip_init();
		dbg("Has reinit sensor !!!\n");
	} else if (sscanf(buffer, "samp=%d\n", &sample))
	{
		if (sample > 0)
		{
			if (sample != l_sensorconfig.sensor_samp)
			{
				amsr = mma_sample2AMSR(sample);
				SET_MMA_SAMPLE(tembuf, amsr);
				klog("sample:%d  ,amsr:%d \n", sample, amsr);
				l_sensorconfig.sensor_samp = sample;
			}
			printk(KERN_ALERT "sensor samp=%d(amsr:%d) has been set.\n", sample, amsr);
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
			mma_enable_disable(enable);
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



static int mma7660_init_client(struct platform_device *pdev)
{
	struct mma7660_config *data;
//	int ret;

	data = dev_get_drvdata(&pdev->dev);
	mutex_init(&sense_data_mutex);
	/*Only for polling, not interrupt*/
	l_sensorconfig.sensor_proc = create_proc_entry(GSENSOR_PROC_NAME, 0666, NULL/*&proc_root*/);
	if (l_sensorconfig.sensor_proc != NULL)
	{
		l_sensorconfig.sensor_proc->write_proc = sensor_writeproc;
		l_sensorconfig.sensor_proc->read_proc = sensor_readproc;
	}


	return 0;

//err:
//	return ret;
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

static int mma7660_probe(
	struct platform_device *pdev)
{
	int err;

	this_pdev = pdev;
	l_sensorconfig.queue = create_singlethread_workqueue("sensor-intterupt-handle");
	INIT_DELAYED_WORK(&l_sensorconfig.work, mma_work_func);
	
	l_sensorconfig.input_dev = input_allocate_device();
	if (!l_sensorconfig.input_dev) {
		err = -ENOMEM;
		printk(KERN_ERR
		       "mma7660_probe: Failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}
	l_sensorconfig.input_dev->evbit[0] = BIT(EV_ABS) | BIT_MASK(EV_KEY);
	set_bit(KEY_NEXTSONG, l_sensorconfig.input_dev->keybit);
	
	/* x-axis acceleration */
	input_set_abs_params(l_sensorconfig.input_dev, ABS_X, -65532, 65532, 0, 0);
	/* y-axis acceleration */
	input_set_abs_params(l_sensorconfig.input_dev, ABS_Y, -65532, 65532, 0, 0);
	/* z-axis acceleration */
	input_set_abs_params(l_sensorconfig.input_dev, ABS_Z, -65532, 65532, 0, 0);

	l_sensorconfig.input_dev->name = "g-sensor";

	err = input_register_device(l_sensorconfig.input_dev);

	if (err) {
		printk(KERN_ERR
		       "mma7660_probe: Unable to register input device: %s\n",
		       l_sensorconfig.input_dev->name);
		goto exit_input_register_device_failed;
	}

	err = misc_register(&mmad_device);
	if (err) {
		printk(KERN_ERR
		       "mma7660_probe: mmad_device register failed\n");
		goto exit_misc_device_register_failed;
	}
	
	dev_set_drvdata(&pdev->dev, &l_sensorconfig);
	mma7660_chip_init();
	mma7660_init_client(pdev);
	gsensor_sysfs_init();

	// satrt the polling work
	l_sensorconfig.sensor_samp = 10;
    queue_delayed_work(l_sensorconfig.queue, &l_sensorconfig.work, msecs_to_jiffies(sample_rate_2_memsec(l_sensorconfig.sensor_samp)));
	return 0;

exit_misc_device_register_failed:
exit_input_register_device_failed:
	input_free_device(l_sensorconfig.input_dev);
//exit_alloc_data_failed:
exit_input_dev_alloc_failed:
//exit_check_functionality_failed:
	return err;
}

static int mma7660_remove(struct platform_device *pdev)
{
	if (NULL != l_sensorconfig.queue)
	{
		cancel_delayed_work_sync(&l_sensorconfig.work);
		flush_workqueue(l_sensorconfig.queue);
		destroy_workqueue(l_sensorconfig.queue);
		l_sensorconfig.queue = NULL;
	}
	gsensor_sysfs_exit();
	misc_deregister(&mmad_device);
	input_unregister_device(l_sensorconfig.input_dev);
	if (l_sensorconfig.sensor_proc != NULL)
	{
		remove_proc_entry(GSENSOR_PROC_NAME, NULL);
		l_sensorconfig.sensor_proc = NULL;
	}
	free_irq(l_sensorconfig.irq, &l_sensorconfig);
	return 0;
}

static int mma7660_suspend(struct platform_device *pdev, pm_message_t state)
{
	//gsensor_int_ctrl(DISABLE);
	cancel_delayed_work_sync(&l_sensorconfig.work);
	del_timer(&l_polltimer);
	dbg("...ok\n");
	//l_resumed = 2;

	return 0;
}

static int mma7660_resume(struct platform_device *pdev)
{

	//mma7660_chip_init();
	//gsensor_int_ctrl(ENABLE);
	//mma_enable_disable(1);
	l_resumed = 1;
	mod_timer(&l_polltimer, jiffies + msecs_to_jiffies(SENSOR_POLL_WAIT_TIME));
	dbg("...ok\n");

	return 0;
}

static void mma7660_platform_release(struct device *device)
{
    return;
}


static struct platform_device mma7660_device = {
    .name           = "mma7660",
    .id             = 0,
    .dev            = {
    	.release = mma7660_platform_release,
    },
};

static struct platform_driver mma7660_driver = {
	.probe = mma7660_probe,
	.remove = mma7660_remove,
	.suspend	= mma7660_suspend,
	.resume		= mma7660_resume,
	.driver = {
		   .name = "mma7660",
		   },
};

/*
 * Brief:
 *	Get the configure of sensor from u-boot.
 * Input:
 *	no use.
 * Output:
 *	no use.
 * Return:
 *	0--success, -1--error.
 * History:
 *	Created by HangYan on 2010-4-19
 * Author:
 * 	Hang Yan in ShenZhen.
 */     
extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);
static int get_axisset(void* param)
{
	char varbuf[64];
	int n;
	int varlen;

	memset(varbuf, 0, sizeof(varbuf));
	varlen = sizeof(varbuf);
	if (wmt_getsyspara("wmt.io.mma7660gsensor", varbuf, &varlen)) {
		printk(KERN_DEBUG "Can't get gsensor config in u-boot!!!!\n");
		//return -1;
	} else {
		n = sscanf(varbuf, "%d:%d:%d:%d:%d:%d:%d:%d:%d:%d",
				&l_sensorconfig.op,
				&(l_sensorconfig.xyz_axis[0][0]),
				&(l_sensorconfig.xyz_axis[0][1]),
				&(l_sensorconfig.xyz_axis[1][0]),
				&(l_sensorconfig.xyz_axis[1][1]),
				&(l_sensorconfig.xyz_axis[2][0]),
				&(l_sensorconfig.xyz_axis[2][1]),
				&(l_sensorconfig.offset[0]),
				&(l_sensorconfig.offset[1]),
				&(l_sensorconfig.offset[2]));
		if (n != 10) {
			printk(KERN_ERR "gsensor format is error in u-boot!!!\n");
			return -1;
		}

		dbg("get the sensor config: %d:%d:%d:%d:%d:%d:%d:%d:%d:%d\n",
			l_sensorconfig.op,
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


// Add one timer to reinit sensor every 5s
static void sensor_polltimer_timeout(unsigned long timeout)
{
	schedule_work(&poll_work);
}

static void poll_work_func(struct work_struct *work)
{
	char rxData[1];
	
	//mutex_lock(&sense_data_mutex);
	if (l_sensorconfig.pollcnt != 20)
	{
		l_sensorconfig.pollcnt++;
	}
	dbg("read to work!\n");	
	spin_lock(&l_sensorconfig.spinlock);
	if (1 == l_resumed)
	{
		mma7660_chip_init();
		//gsensor_int_ctrl(ENABLE);
		//mma_enable_disable(1);
		dbg("reinit for resume...\n");
		l_resumed = 0;
		spin_unlock(&l_sensorconfig.spinlock);
		queue_delayed_work(l_sensorconfig.queue, &l_sensorconfig.work, msecs_to_jiffies(sample_rate_2_memsec(l_sensorconfig.sensor_samp)));

		return;
	}
	spin_unlock(&l_sensorconfig.spinlock);
	if (!is_sensor_good)
	{
		//mma7660_chip_init();
		// if ic  is blocked then wake g-sensor
		sensor_i2c_read(MMA7660_ADDR,3,rxData,1);
	} else {
		is_sensor_good = 0;
	}
	
	//mutex_unlock(&sense_data_mutex);
	mod_timer(&l_polltimer, jiffies + msecs_to_jiffies(SENSOR_POLL_WAIT_TIME));

}

static int mma7660_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int mma7660_release(struct inode *inode, struct file *file)
{
	return 0;
}


static struct file_operations mma7660_fops = {
	.owner		= THIS_MODULE,
	.open		= mma7660_open,
	.release	= mma7660_release,
};

static int is_mma7660(void)
{
	char txData[2];
	int i = 0;

	txData[1] = 0;
	for (i = 0; i < 3; i++)
	{
		if(sensor_i2c_write(MMA7660_ADDR,7,txData,1) == 0)
		{
			return 0;
		}
	}
	return -1;
}

static int __init mma7660_init(void)
{
    int ret = 0;

    if (is_mma7660())
	{
		printk(KERN_ERR "Can't find mma7660!!\n");
		return -1;
	}
	ret = get_axisset(NULL); // get gsensor config from u-boot
	/*if ((ret != 0) || !l_sensorconfig.op)
		return -EINVAL;*/

	
	printk(KERN_INFO "mma7660fc g-sensor driver init\n");

	spin_lock_init(&l_sensorconfig.spinlock);
    INIT_WORK(&poll_work, poll_work_func);
	// Create device node
	if (register_chrdev (GSENSOR_MAJOR, GSENSOR_NAME, &mma7660_fops)) {
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

    if((ret = platform_device_register(&mma7660_device)))
    {
    	printk(KERN_ERR "%s Can't register mma7660 platform devcie!!!\n", __FUNCTION__);
    	return ret;
    }
    if ((ret = platform_driver_register(&mma7660_driver)) != 0)
    {
    	printk(KERN_ERR "%s Can't register mma7660 platform driver!!!\n", __FUNCTION__);
    	return ret;
    }
        
	setup_timer(&l_polltimer, sensor_polltimer_timeout, 0);
	mod_timer(&l_polltimer, jiffies + msecs_to_jiffies(SENSOR_POLL_WAIT_TIME));
	is_sensor_good = 1;

	return 0;
}

static void __exit mma7660_exit(void)
{	
	del_timer(&l_polltimer);
    platform_driver_unregister(&mma7660_driver);
    platform_device_unregister(&mma7660_device);
    device_destroy(l_dev_class, MKDEV(GSENSOR_MAJOR, 0));
	unregister_chrdev(GSENSOR_MAJOR, GSENSOR_NAME);
	class_destroy(l_dev_class);

}

module_init(mma7660_init);
module_exit(mma7660_exit);
MODULE_LICENSE("GPL");
