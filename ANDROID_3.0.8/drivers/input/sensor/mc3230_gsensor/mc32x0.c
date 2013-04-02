/*
 * Copyright (C) 2011 MCUBE, Inc.
 *
 * Initial Code:
 *	Tan Liang
 */


/*! \file mc32x0_driver.c
    \brief This file contains all function implementations for the mc32x0 in linux
    
    Details.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <asm/uaccess.h>
#include <linux/unistd.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/earlysuspend.h>
#include <linux/input.h>
#include <mach/hardware.h>

#include "mc32x0_driver.h"

/////////////////////////////////////////////////////////////////////////
#undef dbg
//#define dbg(fmt, args...) printk(KERN_ALERT "[%s]: " fmt, __FUNCTION__ , ## args)

#define dbg(fmt, args...) if (l_sensorconfig.isdbg) printk(KERN_ALERT "[%s]: " fmt, __FUNCTION__, ## args)


#undef errlog
#undef klog
#define errlog(fmt, args...) printk(KERN_ERR "[%s]: " fmt, __FUNCTION__, ## args)
#define klog(fmt, args...) printk(KERN_ALERT "[%s]: " fmt, __FUNCTION__, ## args)

#define GSENSOR_PROC_NAME "gsensor_config"
#define GSENSOR_NAME "mc3230"

//#define MC32X0_IOC_MAGIC 'M'

//add accel calibrate IO
typedef struct {
	unsigned short	x;		/**< X axis */
	unsigned short	y;		/**< Y axis */
	unsigned short	z;		/**< Z axis */
} GSENSOR_VECTOR3D;

typedef struct{
	int x;
	int y;
	int z;
}SENSOR_DATA;


#define MC32X0_IOC_MAGIC   0xA1

#define MC32X0_SET_RANGE				_IOWR(MC32X0_IOC_MAGIC,4, unsigned char)
#define MC32X0_GET_RANGE				_IOWR(MC32X0_IOC_MAGIC,5, unsigned char)
#define MC32X0_SET_MODE					_IOWR(MC32X0_IOC_MAGIC,6, unsigned char)
#define MC32X0_GET_MODE					_IOWR(MC32X0_IOC_MAGIC,7, unsigned char)
#define MC32X0_SET_BANDWIDTH			_IOWR(MC32X0_IOC_MAGIC,8, unsigned char)
#define MC32X0_GET_BANDWIDTH			_IOWR(MC32X0_IOC_MAGIC,9, unsigned char)
#define MC32X0_READ_ACCEL_X				_IOWR(MC32X0_IOC_MAGIC,10,short)
#define MC32X0_READ_ACCEL_Y				_IOWR(MC32X0_IOC_MAGIC,11,short)
#define MC32X0_READ_ACCEL_Z				_IOWR(MC32X0_IOC_MAGIC,12,short)
#define MC32X0_GET_INTERRUPT_STATUS		_IOWR(MC32X0_IOC_MAGIC,13,unsigned char)
#define MC32X0_READ_ACCEL_XYZ			_IOWR(MC32X0_IOC_MAGIC,14,short)

/*#define GSENSOR_IOCTL_INIT                  _IO(MC32X0_IOC_MAGIC,  0x15)
#define GSENSOR_IOCTL_READ_CHIPINFO         _IOR(MC32X0_IOC_MAGIC, 0x16, int)
#define GSENSOR_IOCTL_READ_SENSORDATA       _IOR(MC32X0_IOC_MAGIC, 0x17, int)
#define GSENSOR_IOCTL_READ_OFFSET			_IOR(MC32X0_IOC_MAGIC, 0x18, GSENSOR_VECTOR3D)
#define GSENSOR_IOCTL_READ_GAIN				_IOR(MC32X0_IOC_MAGIC, 0x19, GSENSOR_VECTOR3D)
#define GSENSOR_IOCTL_READ_RAW_DATA			_IOR(MC32X0_IOC_MAGIC, 0x20, int)
*/
#define GSENSOR_IOCTL_READ_SENSORDATA       _IOR(MC32X0_IOC_MAGIC, 0x17, int)
#define GSENSOR_IOCTL_SET_CALI				_IOW(MC32X0_IOC_MAGIC, 0x21, SENSOR_DATA)
#define GSENSOR_IOCTL_GET_CALI				_IOW(MC32X0_IOC_MAGIC, 0x22, SENSOR_DATA)
#define GSENSOR_IOCTL_CLR_CALI				_IO(MC32X0_IOC_MAGIC, 0x23)
#define GSENSOR_MCUBE_IOCTL_READ_RBM_DATA		_IOR(MC32X0_IOC_MAGIC, 0x24, SENSOR_DATA)
#define GSENSOR_MCUBE_IOCTL_SET_RBM_MODE		_IO(MC32X0_IOC_MAGIC, 0x25)
#define GSENSOR_MCUBE_IOCTL_CLEAR_RBM_MODE		_IO(MC32X0_IOC_MAGIC, 0x26)
#define GSENSOR_MCUBE_IOCTL_SET_CALI			_IOW(MC32X0_IOC_MAGIC, 0x27, SENSOR_DATA)
#define GSENSOR_MCUBE_IOCTL_REGISTER_MAP		_IO(MC32X0_IOC_MAGIC, 0x28)
#define GSENSOR_IOCTL_SET_CALI_MODE   			_IOW(MC32X0_IOC_MAGIC, 0x29,int)
#define GSENSOR_IOCTL_READ_RAW_DATA			_IOR(MC32X0_IOC_MAGIC, 0x30, int)


#define MC32X0_IOC_MAXNR				50
#define AKMIO				0xA1

/* IOCTLs for APPs */

// for offset calibration
#define WMTGSENSOR_IOCTL_MAGIC  0x09
#define WMT_IOCTL_SENSOR_CAL_OFFSET  		_IOW(WMTGSENSOR_IOCTL_MAGIC,  1, int)
#define ECS_IOCTL_APP_SET_AFLAG		 _IOW(WMTGSENSOR_IOCTL_MAGIC, 0x02, short)
#define ECS_IOCTL_APP_SET_DELAY		 _IOW(WMTGSENSOR_IOCTL_MAGIC, 0x03, short)
#define WMT_IOCTL_SENSOR_GET_DRVID	 _IOW(WMTGSENSOR_IOCTL_MAGIC, 0x04, unsigned int)
#define MC3230_DRVID 1

//////////////////////////////////////////////////////////////////
#define SENSOR_DELAY_FASTEST      0
#define SENSOR_DELAY_GAME        20
#define SENSOR_DELAY_UI          60
#define SENSOR_DELAY_NORMAL     200

#define MC32X0_AXIS_X		   0
#define MC32X0_AXIS_Y		   1
#define MC32X0_AXIS_Z		   2
#define MC32X0_AXES_NUM 	   3
#define MC32X0_DATA_LEN 	   6
#define MC32X0_DEV_NAME 	   "MC32X0"
#define GRAVITY_EARTH_1000 		9807
#define IS_MC3230 1
#define IS_MC3210 2

static int mcube_read_cali_file(void /*struct i2c_client *client*/);


struct mc32x0_data{
	mc32x0_t mc32x0;
	// for control
	int int_gpio; //0-3
	int op;
	int samp;
	int xyz_axis[3][3]; // (axis,direction)
	struct proc_dir_entry* sensor_proc;
	struct input_dev *input_dev;
	//struct work_struct work;
	struct delayed_work work; // for polling
	struct workqueue_struct *queue;
	int isdbg;
	int sensor_samp; // 
	int sensor_enable;  // 0 --> disable sensor, 1 --> enable sensor
	int test_pass;
	//int offset[3];
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend earlysuspend;
#endif
	s16 offset[MC32X0_AXES_NUM+1];	/*+1: for 4-byte alignment*/
	s16 data[MC32X0_AXES_NUM+1]; 
	s16 cali_sw[MC32X0_AXES_NUM+1];


};

static struct mutex sense_data_mutex;
static struct class* l_dev_class = NULL;

static struct mc32x0_data l_sensorconfig = {
	.op = 0,
	.int_gpio = 3,
	.samp = 16,
	.xyz_axis = {
		{ABS_X, -1},
		{ABS_Y, 1},
		{ABS_Z, -1},
		},
	.sensor_proc = NULL,
	.isdbg = 0,
	.sensor_samp = 1,  // 1 sample/second
	.sensor_enable = 1, // enable sensor
	.test_pass = 0, // for test program
	//.offset={0,0,0},
};

#define MC3230_REG_CHIP_ID 		0x18
#define MC3230_REG_X_OUT			0x0 //RO
#define MC3230_REG_Y_OUT			0x1 //RO
#define MC3230_REG_Z_OUT			0x2 //RO
#define MC3230_REG_STAT 			0x04
#define MC3230_REG_SLEEP_COUNTER 0x05
#define MC3230_REG_INTMOD			0x06
#define MC3230_REG_SYSMOD			0x07
#define MC3230_REG_RATE_SAMP 		0x08

#define MC32X0_XOUT_EX_L_REG				0x0d

#define MC3230_REG_RBM_DATA		0x0D
#define MC3230_REG_PRODUCT_CODE	0x3b


#define CALIB_PATH				"/data/data/com.mcube.acc/files/mcube-calib.txt"
#define DATA_PATH			   "/sdcard/mcube-register-map.txt"


static GSENSOR_VECTOR3D gsensor_gain;

static struct file * fd_file = NULL;
static int load_cali_flg = 0;
static mm_segment_t oldfs;
//add by Liang for storage offset data
static unsigned char offset_buf[9]; 
static signed int offset_data[3];
static int G_RAW_DATA[3];
static signed int gain_data[3];
static signed int enable_RBM_calibration = 0;
static unsigned char mc32x0_type;

////////////////////////////////////////////////////////////////////
extern int wmt_i2c_xfer_continue_if_4(struct i2c_msg *msg, unsigned int num, int bus_id);
extern int wmt_i2c_xfer_continue_if_0(struct i2c_msg *msg, unsigned int num);
static void MC32X0_rbm(/*struct i2c_client *client, */int enable);

unsigned int sample_rate_2_memsec(unsigned int rate)
{
	return (1000/rate);
}

/*	i2c delay routine for eeprom	*/
static  void mc32x0_i2c_delay(unsigned int msec)
{
	mdelay(msec);
}

/*	i2c write routine for mc32x0	*/
static  int mc32x0_i2c_write(unsigned char reg_addr, unsigned char *data, unsigned char len)
{
	int ret = 0;
	u8 buf[2];
	struct i2c_msg msg[1];

	msg[0].addr = MC32X0_I2C_ADDR;
    msg[0].flags = 0 ;
    msg[0].flags &= ~(I2C_M_RD);
	while (len--)
	{
		buf[0] = reg_addr;
		//memcpy(buf+1, data, len);
		buf[1] = *data;	
	    msg[0].len = 2;
	    msg[0].buf = buf;
		if ((ret = wmt_i2c_xfer_continue_if_4(msg,1,0)) <= 0)
		//if ((ret =  wmt_i2c_xfer_continue_if_0(msg, 1))<=0)
		{
			errlog("write error!\n");
			return -1;
		}
		reg_addr++;
		data++;
	};
	return 0;

/*
	s32 dummy;
	unsigned char buffer[2];
	if( mc32x0_client == NULL )	//	No global client pointer
		return -1;

	while(len--)
	{
		buffer[0] = reg_addr;
		buffer[1] = *data;
		dummy = i2c_master_send(mc32x0_client, (char*)buffer, 2);

		reg_addr++;
		data++;
		if(dummy < 0)
			return -1;
	}
	return 0;
*/
}

/*	i2c read routine for mc32x0	*/
static int mc32x0_i2c_read(unsigned char reg_addr, unsigned char *data, unsigned char len) 
{
	int ret = 0;	
	
	struct i2c_msg msg[] = 
	{
		{.addr = MC32X0_I2C_ADDR, .flags = 0, .len = 1, .buf = &reg_addr,}, 
		{.addr = MC32X0_I2C_ADDR, .flags = I2C_M_RD, .len = len, .buf = data,},
	};
	ret = wmt_i2c_xfer_continue_if_4(msg, 2,0);
	//ret =  wmt_i2c_xfer_continue_if_0(msg, 2);

	if (ret <= 0)
	{
		errlog("read error!\n");
	}
	return (ret>0)?0:-1;


/*
	s32 dummy;
       
		dummy = i2c_master_send(mc32x0_client, (char*)&reg_addr, 1);
		if(dummy < 0)
			return -1;
		dummy = i2c_master_recv(mc32x0_client, (char*)data, len);
		if(dummy < 0)
			return -1;
		
	
	return 0;
	*/
}


/*	read command for MC32X0 device file	*/
static ssize_t mc32x0_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{	
	return 0;
}

/*	write command for MC32X0 device file	*/
static ssize_t mc32x0_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
	dbg("write...\n");
	return 0;
}

static int mc32x0_start(void)
{
	mutex_lock(&sense_data_mutex);
	mc32x0_set_image();
	mc32x0_set_mode(MC32X0_WAKE);
	mutex_unlock(&sense_data_mutex);
	return 0;
}

/*	open command for MC32X0 device file	*/
static int mc32x0_open(struct inode *node, struct file *fle)
{
	/*mutex_lock(&sense_data_mutex);
	mc32x0_set_image();
	mc32x0_set_mode(MC32X0_WAKE);
	mutex_unlock(&sense_data_mutex);
	*/

    dbg("open...\n");
	return 0;
}

/*	release command for MC32X0 device file	*/
static int mc32x0_close(struct inode *node, struct file *fle)
{
    dbg("close...\n");
    /*mutex_lock(&sense_data_mutex);
	mc32x0_set_mode(MC32X0_STANDBY);
	mutex_unlock(&sense_data_mutex);
	*/
	return 0;
}

static int MC32X0_WriteCalibration(/*struct i2c_client *client,*/ int dat[MC32X0_AXES_NUM])
{
	int err;
	u8 buf[9],data;
	s16 tmp, x_gain, y_gain, z_gain ;
	s32 x_off, y_off, z_off;
#if 1  //modify by zwx

	dbg("UPDATE dat: (%+3d %+3d %+3d)\n", 
	dat[MC32X0_AXIS_X], dat[MC32X0_AXIS_Y], dat[MC32X0_AXIS_Z]);

	/*calculate the real offset expected by caller*/
	//cali_temp[MC32X0_AXIS_X] = dat[MC32X0_AXIS_X];
	//cali_temp[MC32X0_AXIS_Y] = dat[MC32X0_AXIS_Y];
	//cali_temp[MC32X0_AXIS_Z] = dat[MC32X0_AXIS_Z];
	//cali[MC32X0_AXIS_Z]= cali[MC32X0_AXIS_Z]-gsensor_gain.z;


#endif	
// read register 0x21~0x28
#if 0 //zwx
	if ((err = mc3230_read_block(client, 0x21, buf, 3))) 
	{
		errlog("error: %d\n", err);
		return err;
	}
	if ((err = mc3230_read_block(client, 0x24, &buf[3], 3))) 
	{
		errlog("error: %d\n", err);
		return err;
	}
	if ((err = mc3230_read_block(client, 0x27, &buf[6], 3))) 
	{
		errlog("error: %d\n", err);
		return err;

	}
#endif
	/*buf[0] = 0x21;
	err = mc3230_rx_data(client, &buf[0], 3);
	buf[3] = 0x24;
	err = mc3230_rx_data(client, &buf[3], 3);
	buf[6] = 0x27;
	err = mc3230_rx_data(client, &buf[6], 3);
	*/
	err = mc32x0_i2c_read(0x21, buf,9);
#if 1
	// get x,y,z offset
	tmp = ((buf[1] & 0x3f) << 8) + buf[0];
		if (tmp & 0x2000)
			tmp |= 0xc000;
		x_off = tmp;
					
	tmp = ((buf[3] & 0x3f) << 8) + buf[2];
		if (tmp & 0x2000)
			tmp |= 0xc000;
		y_off = tmp;
					
	tmp = ((buf[5] & 0x3f) << 8) + buf[4];
		if (tmp & 0x2000)
			tmp |= 0xc000;
		z_off = tmp;
					
	// get x,y,z gain
	x_gain = ((buf[1] >> 7) << 8) + buf[6];
	y_gain = ((buf[3] >> 7) << 8) + buf[7];
	z_gain = ((buf[5] >> 7) << 8) + buf[8];
								
	// prepare new offset
	x_off = x_off + 16 * dat[MC32X0_AXIS_X] * 256 * 128 / 3 / gsensor_gain.x / (40 + x_gain);
	y_off = y_off + 16 * dat[MC32X0_AXIS_Y] * 256 * 128 / 3 / gsensor_gain.y / (40 + y_gain);
	z_off = z_off + 16 * dat[MC32X0_AXIS_Z] * 256 * 128 / 3 / gsensor_gain.z / (40 + z_gain);

	//storege the cerrunt offset data with DOT format
	offset_data[0] = x_off;
	offset_data[1] = y_off;
	offset_data[2] = z_off;

	//storege the cerrunt Gain data with GOT format
	gain_data[0] = 256*8*128/3/(40+x_gain);
	gain_data[1] = 256*8*128/3/(40+y_gain);
	gain_data[2] = 256*8*128/3/(40+z_gain);
	dbg("%d %d ======================\n\n ",gain_data[0],x_gain);
#endif
	buf[0]=0x43;
	//mc3230_write_block(client, 0x07, buf, 1);
	//mc3230_write_reg(client,0x07,0x43);
	mc32x0_write_reg(0x07,buf,1);
					
	buf[0] = x_off & 0xff;
	buf[1] = ((x_off >> 8) & 0x3f) | (x_gain & 0x0100 ? 0x80 : 0);
	buf[2] = y_off & 0xff;
	buf[3] = ((y_off >> 8) & 0x3f) | (y_gain & 0x0100 ? 0x80 : 0);
	buf[4] = z_off & 0xff;
	buf[5] = ((z_off >> 8) & 0x3f) | (z_gain & 0x0100 ? 0x80 : 0);


	
	//mc3230_write_block(client, 0x21, buf, 6);
	mc32x0_write_reg(0x21,buf,6);

	//buf[0]=0x41;
	//mc3230_write_block(client, 0x07, buf, 1);	

	//mc3230_write_reg(client,0x07,0x41);
	data = 0x41;
	mc32x0_write_reg(0x07,&data,1);

    return err;

}

static int MC32X0_ResetCalibration(/*struct i2c_client *client*/void)
{
	//struct mc3230_data *mc3230 = i2c_get_clientdata(client);
	s16 tmp;
	char data;

	//mc3230_write_reg(client,0x07,0x43);
	data = 0x43;
	mc32x0_write_reg(0x07,&data,1);

	//mc3230_write_block(client, 0x21, offset_buf, 6);
	mc32x0_write_reg(0x21, offset_buf, 6);

	//mc3230_write_reg(client,0x07,0x41);
	data = 0x41;
	mc32x0_write_reg(0x07,&data,1);

	msleep(20);

	tmp = ((offset_buf[1] & 0x3f) << 8) + offset_buf[0];  // add by Liang for set offset_buf as OTP value 
	if (tmp & 0x2000)
	tmp |= 0xc000;
	offset_data[0] = tmp;
			
	tmp = ((offset_buf[3] & 0x3f) << 8) + offset_buf[2];  // add by Liang for set offset_buf as OTP value 
	if (tmp & 0x2000)
		tmp |= 0xc000;
	offset_data[1] = tmp;
			
	tmp = ((offset_buf[5] & 0x3f) << 8) + offset_buf[4];  // add by Liang for set offset_buf as OTP value 
	if (tmp & 0x2000)
	tmp |= 0xc000;
	offset_data[2] = tmp;	

	//memset(mc3230->cali_sw, 0x00, sizeof(mc3230->cali_sw));
	return 0;  

}

static int MC32X0_ReadOffset(/*struct i2c_client *client,*/ s16 ofs[MC32X0_AXES_NUM])
{    
/*	int err;
	u8 off_data[6];
	

	if ( mc32x0_type == IS_MC3210 )
	{
		//if ((err = mc3230_read_block(client, MC32X0_XOUT_EX_L_REG, off_data, MC32X0_DATA_LEN))) 
		if (err = mc32x0_read_reg(MC32X0_XOUT_EX_L_REG, off_data, MC32X0_DATA_LEN))
    		{
    			errlog("error: %d\n", err);
    			return err;
    		}
		ofs[MC32X0_AXIS_X] = ((s16)(off_data[0]))|((s16)(off_data[1])<<8);
		ofs[MC32X0_AXIS_Y] = ((s16)(off_data[2]))|((s16)(off_data[3])<<8);
		ofs[MC32X0_AXIS_Z] = ((s16)(off_data[4]))|((s16)(off_data[5])<<8);
	}
	else if (mc32x0_type == IS_MC3230) 
	{
		if ((err = mc3230_read_block(client, 0, off_data, 3))) 
    		{
    			errlog("error: %d\n", err);
    			return err;
    		}
		ofs[MC32X0_AXIS_X] = (s8)off_data[0];
		ofs[MC32X0_AXIS_Y] = (s8)off_data[1];
		ofs[MC32X0_AXIS_Z] = (s8)off_data[2];			
	}

	dbg("MC32X0_ReadOffset %d %d %d \n",ofs[MC32X0_AXIS_X] ,ofs[MC32X0_AXIS_Y],ofs[MC32X0_AXIS_Z]);
*/
    return 0;  
}


static int MC32X0_ReadCalibration(/*struct i2c_client *client,*/ int dat[MC32X0_AXES_NUM])
{
	
    struct mc32x0_data *mc3230 = &l_sensorconfig;//i2c_get_clientdata(client);
    int err;
	
    if ((err = MC32X0_ReadOffset(/*client,*/ mc3230->offset))) {
        errlog("read offset fail, %d\n", err);
        return err;
    }    
    
    dat[MC32X0_AXIS_X] = mc3230->offset[MC32X0_AXIS_X];
    dat[MC32X0_AXIS_Y] = mc3230->offset[MC32X0_AXIS_Y];
    dat[MC32X0_AXIS_Z] = mc3230->offset[MC32X0_AXIS_Z];  
	//modify by zwx
	//dbg("MC32X0_ReadCalibration %d %d %d \n",dat[mc3230->cvt.map[MC32X0_AXIS_X]] ,dat[mc3230->cvt.map[MC32X0_AXIS_Y]],dat[mc3230->cvt.map[MC32X0_AXIS_Z]]);
                                      
    return 0;
}

static int MC32X0_ReadData(/*struct i2c_client *client,*/ s16 buffer[MC32X0_AXES_NUM])
{
	s8 buf[3];
	char rbm_buf[6];
	int ret;
	//int err = 0;

	/*if(NULL == client)
	{
		err = -EINVAL;
		return err;
	}
	*/

	if ( enable_RBM_calibration == 0)
	{
		//err = hwmsen_read_block(client, addr, buf, 0x06);
	}
	else if (enable_RBM_calibration == 1)
	{		
		memset(rbm_buf, 0, 6);
        	rbm_buf[0] = MC3230_REG_RBM_DATA;
        	//ret = mc3230_rx_data(client, &rbm_buf[0], 6);
        	ret = mc32x0_read_reg(MC3230_REG_RBM_DATA,rbm_buf,6);
        	dbg("calibration:enable_RBM_calibration == 1");
	}

	if ( enable_RBM_calibration == 0)
	{

	    do {
	        memset(buf, 0, 3);
	        buf[0] = MC3230_REG_X_OUT;
	        //ret = mc3230_rx_data(client, &buf[0], 3);
	        ret = mc32x0_read_reg(MC3230_REG_X_OUT,buf,3);
	        if (ret < 0)
	            return ret;
	    	} while (0);
		
			buffer[0]=(s16)buf[0];
			buffer[1]=(s16)buf[1];
			buffer[2]=(s16)buf[2];
		
		dbg("0x%02x 0x%02x 0x%02x \n",buffer[0],buffer[1],buffer[2]);
	}
	else if (enable_RBM_calibration == 1)
	{
		buffer[MC32X0_AXIS_X] = (s16)((rbm_buf[0]) | (rbm_buf[1] << 8));
		buffer[MC32X0_AXIS_Y] = (s16)((rbm_buf[2]) | (rbm_buf[3] << 8));
		buffer[MC32X0_AXIS_Z] = (s16)((rbm_buf[4]) | (rbm_buf[5] << 8));

		dbg("%s RBM<<<<<[%08d %08d %08d]\n", __func__,buffer[MC32X0_AXIS_X], buffer[MC32X0_AXIS_Y], buffer[MC32X0_AXIS_Z]);
if(gain_data[0] == 0)
{
		buffer[MC32X0_AXIS_X] = 0;
		buffer[MC32X0_AXIS_Y] = 0;
		buffer[MC32X0_AXIS_Z] = 0;
	return 0;
}
		buffer[MC32X0_AXIS_X] = (buffer[MC32X0_AXIS_X] + offset_data[0]/2)*gsensor_gain.x/gain_data[0];
		buffer[MC32X0_AXIS_Y] = (buffer[MC32X0_AXIS_Y] + offset_data[1]/2)*gsensor_gain.y/gain_data[1];
		buffer[MC32X0_AXIS_Z] = (buffer[MC32X0_AXIS_Z] + offset_data[2]/2)*gsensor_gain.z/gain_data[2];
		
		dbg("%s offset_data <<<<<[%d %d %d]\n", __func__,offset_data[0], offset_data[1], offset_data[2]);

		dbg("%s gsensor_gain <<<<<[%d %d %d]\n", __func__,gsensor_gain.x, gsensor_gain.y, gsensor_gain.z);
		
		dbg("%s gain_data <<<<<[%d %d %d]\n", __func__,gain_data[0], gain_data[1], gain_data[2]);

		dbg("%s RBM->RAW <<<<<[%d %d %d]\n", __func__,buffer[MC32X0_AXIS_X], buffer[MC32X0_AXIS_Y], buffer[MC32X0_AXIS_Z]);
	}
	
	return 0;
}


static int MC32X0_ReadRawData(/*struct i2c_client *client,*/ char * buf)
{
	//struct mc32x0_data *obj = &l_sensorconfig;//(struct mc3230_data*)i2c_get_clientdata(client);
	int res = 0;
	s16 raw_buf[3];

	/*if (!buf || !client)
	{
		return EINVAL;
	}

	if(obj->status == MC3230_CLOSE)
	{
		res = mc3230_start(client, 0);
		if(res)
		{
			errlog("Power on mc32x0 error %d!\n", res);
		}
	}*/
	
	if(res = MC32X0_ReadData(/*client,*/ raw_buf) != 0)
	{     
		errlog("I2C error: ret value=%d", res);
		return EIO;
	}
	else
	{
	
	klog("UPDATE dat: (%+3d %+3d %+3d)\n", 
	raw_buf[MC32X0_AXIS_X], raw_buf[MC32X0_AXIS_Y], raw_buf[MC32X0_AXIS_Z]);

	G_RAW_DATA[MC32X0_AXIS_X] = raw_buf[0];
	G_RAW_DATA[MC32X0_AXIS_Y] = raw_buf[1];
	G_RAW_DATA[MC32X0_AXIS_Z] = raw_buf[2];
	G_RAW_DATA[MC32X0_AXIS_Z]= G_RAW_DATA[MC32X0_AXIS_Z]+gsensor_gain.z*l_sensorconfig.xyz_axis[2][1]*(-1);
	
	//printk("%s %d\n",__FUNCTION__, __LINE__);
		sprintf(buf, "%04x %04x %04x", G_RAW_DATA[MC32X0_AXIS_X], 
			G_RAW_DATA[MC32X0_AXIS_Y], G_RAW_DATA[MC32X0_AXIS_Z]);
		klog("G_RAW_DATA: (%+3d %+3d %+3d)\n", 
	G_RAW_DATA[MC32X0_AXIS_X], G_RAW_DATA[MC32X0_AXIS_Y], G_RAW_DATA[MC32X0_AXIS_Z]);
	}
	return 0;
}

struct file *openFile(char *path,int flag,int mode) 
{ 
	struct file *fp; 
	 
	fp=filp_open(path, flag, mode); 
	if (IS_ERR(fp) || !fp->f_op) 
	{
		dbg("Calibration File filp_open return NULL\n");
		return NULL; 
	}
	else 
	{

		return fp; 
	}
} 
 
int readFile(struct file *fp,char *buf,int readlen) 
{ 
	if (fp->f_op && fp->f_op->read) 
		return fp->f_op->read(fp,buf,readlen, &fp->f_pos); 
	else 
		return -1; 
} 

int writeFile(struct file *fp,char *buf,int writelen) 
{ 
	if (fp->f_op && fp->f_op->write) 
		return fp->f_op->write(fp,buf,writelen, &fp->f_pos); 
	else 
		return -1; 
}
 
int closeFile(struct file *fp) 
{ 
	filp_close(fp,NULL); 
	return 0; 
} 

void initKernelEnv(void) 
{ 
	oldfs = get_fs(); 
	set_fs(KERNEL_DS);
	dbg("initKernelEnv\n");
} 


static int mcube_read_cali_file(void/*struct i2c_client *client*/)
{
	int cali_data[3];
	int err =0;
	char buf[64];
	
	initKernelEnv();
	fd_file = openFile(CALIB_PATH,O_RDONLY,0); 
	if (fd_file == NULL) 
	{
		dbg("fail to open\n");
		cali_data[0] = 0;
		cali_data[1] = 0;
		cali_data[2] = 0;
		return 1;
	}
	else
	{
		memset(buf,0,64); 
		if ((err = readFile(fd_file,buf,128))>0) 
		{
			dbg("buf:%s\n",buf); 
		}
		else 
		{
			dbg("read file error %d\n",err); 
		}
		//printk("%s %d\n",__func__,__LINE__);

		set_fs(oldfs); 
		closeFile(fd_file); 

		sscanf(buf, "%d %d %d",&cali_data[MC32X0_AXIS_X], &cali_data[MC32X0_AXIS_Y], &cali_data[MC32X0_AXIS_Z]);
		klog("cali_data: %d %d %d\n", cali_data[MC32X0_AXIS_X], cali_data[MC32X0_AXIS_Y], cali_data[MC32X0_AXIS_Z]); 	
				
		//cali_data1[MC32X0_AXIS_X] = cali_data[MC32X0_AXIS_X] * gsensor_gain.x / GRAVITY_EARTH_1000;
		//cali_data1[MC32X0_AXIS_Y] = cali_data[MC32X0_AXIS_Y] * gsensor_gain.y / GRAVITY_EARTH_1000;
		//cali_data1[MC32X0_AXIS_Z] = cali_data[MC32X0_AXIS_Z] * gsensor_gain.z / GRAVITY_EARTH_1000;
		//cali_data[MC32X0_AXIS_X]=-cali_data[MC32X0_AXIS_X];
		//cali_data[MC32X0_AXIS_Y]=-cali_data[MC32X0_AXIS_Y];
		//cali_data[MC32X0_AXIS_Z]=-cali_data[MC32X0_AXIS_Z];

		//klog("cali_data1: %d %d %d\n", cali_data1[MC32X0_AXIS_X], cali_data1[MC32X0_AXIS_Y], cali_data1[MC32X0_AXIS_Z]); 	
		//printk("%s %d\n",__func__,__LINE__);	  
		MC32X0_WriteCalibration(/*client,*/ cali_data);
	}
	return 0;
}


static int mcube_write_log_data(/*struct i2c_client *client,*/ u8 data[0x40])
{
	//struct mc32x0_i2c_data *obj = i2c_get_clientdata(client);
	int cali_data[3],cali_data1[3];
	s16 rbm_data[3], raw_data[3];
	int err =0;
	char buf[66*50];
	int n=0,i=0;

	initKernelEnv();
	fd_file = openFile(DATA_PATH ,O_RDWR | O_CREAT,0); 
	if (fd_file == NULL) 
	{
		dbg("mcube_write_log_data fail to open\n");	
	}
	else
	{
		rbm_data[MC32X0_AXIS_X] = (s16)((data[0x0d]) | (data[0x0e] << 8));
		rbm_data[MC32X0_AXIS_Y] = (s16)((data[0x0f]) | (data[0x10] << 8));
		rbm_data[MC32X0_AXIS_Z] = (s16)((data[0x11]) | (data[0x12] << 8));

		raw_data[MC32X0_AXIS_X] = (rbm_data[MC32X0_AXIS_X] + offset_data[0]/2)*gsensor_gain.x/gain_data[0];
		raw_data[MC32X0_AXIS_Y] = (rbm_data[MC32X0_AXIS_Y] + offset_data[1]/2)*gsensor_gain.y/gain_data[1];
		raw_data[MC32X0_AXIS_Z] = (rbm_data[MC32X0_AXIS_Z] + offset_data[2]/2)*gsensor_gain.z/gain_data[2];

		
		memset(buf,0,66*50); 
		n += sprintf(buf+n, "G-sensor RAW X = %d  Y = %d  Z = %d\n", raw_data[0] ,raw_data[1] ,raw_data[2]);
		n += sprintf(buf+n, "G-sensor RBM X = %d  Y = %d  Z = %d\n", rbm_data[0] ,rbm_data[1] ,rbm_data[2]);
		for(i=0; i<64; i++)
		{
		n += sprintf(buf+n, "mCube register map Register[0x%x] = 0x%x\n",i,data[i]);
		}
		msleep(50);		
		if ((err = writeFile(fd_file,buf,n))>0) 
		{
			dbg("buf:%s\n",buf); 
		}
		else 
		{
			dbg("write file error %d\n",err); 
		}

		set_fs(oldfs); 
		closeFile(fd_file); 
	}
	return 0;
}


static int MC32X0_Read_Reg_Map(/*struct i2c_client *client*/ void)
{
	//struct mc32x0_i2c_data *priv = i2c_get_clientdata(client);        
	u8 data[128];
	//u8 addr = 0x00;
	int err = 0;
	int i;

	/*if(NULL == client)
	{
		err = -EINVAL;
		return err;
	}*/


	
/********************************************/
	
/*	err = hwmsen_read_block(client, addr, data, 0x06);
	addr +=6;
	err = hwmsen_read_block(client, addr, data+6, 0x06);
	addr +=6;
	err = hwmsen_read_block(client, addr, data+12, 0x06);
	addr +=6;
	err = hwmsen_read_block(client, addr, data+18, 0x06);
	addr +=6;
	err = hwmsen_read_block(client, addr, data+24, 0x06);
	addr +=6;
	err = hwmsen_read_block(client, addr, data+30, 0x06);
	addr +=6;
	err = hwmsen_read_block(client, addr, data+36, 0x06);
	addr +=6;
	err = hwmsen_read_block(client, addr, data+42, 0x06);
	addr +=6;
	err = hwmsen_read_block(client, addr, data+48, 0x06);
	addr +=6;
	err = hwmsen_read_block(client, addr, data+54, 0x06);
	addr +=6;
	err = hwmsen_read_block(client, addr, data+60, 0x06);
*/
	err = mc32x0_read_reg(0x00, data, 0x40);

	for(i = 0; i<64; i++)
	{
		klog("mcube register map Register[0x%x] = 0x%x\n", i ,data[i]);
	}

	msleep(50);	
	
	mcube_write_log_data(/*client,*/ data);

	msleep(50);
     	
	return err;
}


/*	ioctl command for MC32X0 device file	*/
static long mc32x0_ioctl(/*struct inode *inode,*/ struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	unsigned char data[6];
	short delay = 0;
	short enable = 0;

	char strbuf[256];
	void __user *tmpdata;
	SENSOR_DATA sensor_data;
	int cali[3];
	struct mc32x0_data* this = &l_sensorconfig;
	unsigned int uval = 0;
	
	if (WMT_IOCTL_SENSOR_CAL_OFFSET == cmd)
	{
		return 0;// now do nothing
	}
	/* //check cmd 
	if(_IOC_TYPE(cmd) != MC32X0_IOC_MAGIC)	
	{
		errlog("cmd magic type error\n");
		return -ENOTTY;
	}
	if(_IOC_NR(cmd) > MC32X0_IOC_MAXNR)
	{
		errlog("cmd number error\n");
		return -ENOTTY;
	}

	if(_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE,(void __user*)arg, _IOC_SIZE(cmd));
	else if(_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user*)arg, _IOC_SIZE(cmd));
	if(err)
	{
		errlog("cmd access_ok error\n");
		return -EFAULT;
	}

	*/
	/* cmd mapping */
	mutex_lock(&sense_data_mutex);
	switch(cmd)
	{
	case MC32X0_SET_RANGE:
		if(copy_from_user(data,(unsigned char*)arg,1)!=0)
		{
			errlog("copy_from_user error\n");
			err = -EFAULT;
		} else {
			err = mc32x0_set_range(*data);
		}
		break;

	case MC32X0_GET_RANGE:
		err = mc32x0_get_range(data);
		if(copy_to_user((unsigned char*)arg,data,1)!=0)
		{
			errlog("copy_to_user error\n");
			err = -EFAULT;
		}
		break;

	case MC32X0_SET_MODE:
		if(copy_from_user(data,(unsigned char*)arg,1)!=0)
		{
			errlog("copy_from_user error\n");
			err = -EFAULT;
		} else {
			err = mc32x0_set_mode(*data);
		}
		break;

	case MC32X0_GET_MODE:
		err = mc32x0_get_mode(data);
		if(copy_to_user((unsigned char*)arg,data,1)!=0)
		{
			errlog("copy_to_user error\n");
			err = -EFAULT;
		}
		break;

	case MC32X0_SET_BANDWIDTH:
		if(copy_from_user(data,(unsigned char*)arg,1)!=0)
		{
			errlog("copy_from_user error\n");
			err = -EFAULT;
		} else {
			err = mc32x0_set_bandwidth(*data);
		}
		break;

	case MC32X0_GET_BANDWIDTH:
		err = mc32x0_get_bandwidth(data);
		if(copy_to_user((unsigned char*)arg,data,1)!=0)
		{
			errlog("copy_to_user error\n");
			err = -EFAULT;
		}
		break;

	case MC32X0_READ_ACCEL_X:
		err = mc32x0_read_accel_x((short*)data);
		if(copy_to_user((short*)arg,(short*)data,2)!=0)
		{
			errlog("copy_to_user error\n");
			err = -EFAULT;
		}
		break;

	case MC32X0_READ_ACCEL_Y:
		err = mc32x0_read_accel_y((short*)data);
		if(copy_to_user((short*)arg,(short*)data,2)!=0)
		{
			errlog("copy_to_user error\n");
			err = -EFAULT;
		}
		break;

	case MC32X0_READ_ACCEL_Z:
		err = mc32x0_read_accel_z((short*)data);
		if(copy_to_user((short*)arg,(short*)data,2)!=0)
		{
			errlog("copy_to_user error\n");
			err = -EFAULT;
		}
		break;

	case MC32X0_READ_ACCEL_XYZ:
		err = mc32x0_read_accel_xyz((mc32x0acc_t*)data);
		if(copy_to_user((mc32x0acc_t*)arg,(mc32x0acc_t*)data,6)!=0)
		{
			errlog("copy_to error\n");
			err = -EFAULT;
		}
		break;	

	case MC32X0_GET_INTERRUPT_STATUS:
		err = mc32x0_get_interrupt_status(data);
		if(copy_to_user((unsigned char*)arg,data,1)!=0)
		{
			errlog("copy_to_user error\n");
			err = -EFAULT;
		}
		break;
	case ECS_IOCTL_APP_SET_DELAY:
		// set the rate of g-sensor
		if (copy_from_user(&delay,(short*)arg, sizeof(short)))
		{
			errlog("Can't get set delay!!!\n");
			err = -EFAULT;
			goto errioctl;
		}
		dbg("Get delay=%d \n", delay);
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
	case WMT_IOCTL_SENSOR_GET_DRVID:
		uval = MC3230_DRVID;
		if (copy_to_user((unsigned int*)arg, &uval, sizeof(unsigned int)))
		{
			return -EFAULT;
		}
		dbg("mc32x0_driver_id:%d\n",uval);
		break;
	case ECS_IOCTL_APP_SET_AFLAG:
		// enable/disable sensor
		if (copy_from_user(&enable, (short*)arg, sizeof(short)))
		{
			errlog("Can't get enable flag!!!\n");
			err = -EFAULT;
			goto errioctl;				
		}
		if ((enable >=0) && (enable <=1))
		{
			dbg("driver: disable/enable(%d) gsensor. l_sensorconfig.sensor_samp=%d\n", enable, l_sensorconfig.sensor_samp);
			
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
	case GSENSOR_MCUBE_IOCTL_SET_CALI:
			dbg("fwq GSENSOR_MCUBE_IOCTL_SET_CALI!!\n");
			tmpdata = (void __user*)arg;
			if(tmpdata == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			if(copy_from_user(&sensor_data, tmpdata, sizeof(sensor_data)))
			{
				err = -EFAULT;
				break;	  
			}
			//if(atomic_read(&this->suspend))
			//{
			//	errlog("Perform calibration in suspend state!!\n");
			//	err = -EINVAL;
			//}
			else
			{
				//this->cali_sw[MC32X0_AXIS_X] += sensor_data.x;
				//this->cali_sw[MC32X0_AXIS_Y] += sensor_data.y;
				//this->cali_sw[MC32X0_AXIS_Z] += sensor_data.z;
				
				cali[0] = sensor_data.x;
				cali[1] = sensor_data.y;
				cali[2] = sensor_data.z;	

			  	dbg("GSENSOR_MCUBE_IOCTL_SET_CALI %d  %d  %d  %d  %d  %d!!\n", cali[0], cali[1],cali[2] ,sensor_data.x, sensor_data.y ,sensor_data.z);
				
				err = MC32X0_WriteCalibration(/*client,*/ cali);			 
			}
			break;
		case GSENSOR_IOCTL_CLR_CALI:
			dbg("fwq GSENSOR_IOCTL_CLR_CALI!!\n");
			err = MC32X0_ResetCalibration();
			break;

		case GSENSOR_IOCTL_GET_CALI:
			dbg("fwq mc32x0 GSENSOR_IOCTL_GET_CALI\n");
			tmpdata = (void __user*)arg;
			if(tmpdata == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			/*if((err = MC32X0_ReadCalibration(client, cali)))
			{
				dbg("fwq mc32x0 MC32X0_ReadCalibration error!!!!\n");
				break;
			}*/

			sensor_data.x = this->cali_sw[MC32X0_AXIS_X];
			sensor_data.y = this->cali_sw[MC32X0_AXIS_Y];
			sensor_data.z = this->cali_sw[MC32X0_AXIS_Z];
			if(copy_to_user(tmpdata, &sensor_data, sizeof(sensor_data)))
			{
				err = -EFAULT;
				break;
			}		
			break;	
		// add by liang ****
		//add in Sensors_io.h
		//#define GSENSOR_IOCTL_SET_CALI_MODE   _IOW(GSENSOR, 0x0e, int)
		case GSENSOR_IOCTL_SET_CALI_MODE:
			dbg("fwq mc32x0 GSENSOR_IOCTL_SET_CALI_MODE\n");
			break;

		case GSENSOR_MCUBE_IOCTL_READ_RBM_DATA:
			dbg("fwq GSENSOR_MCUBE_IOCTL_READ_RBM_DATA\n");
			tmpdata = (void __user *) arg;
			if(tmpdata == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			//*MC32X0_ReadRBMData(client, (char *)&strbuf);
			if(copy_to_user(tmpdata, &strbuf, strlen(strbuf)+1))
			{
				err = -EFAULT;
				break;	  
			}
			break;

		case GSENSOR_MCUBE_IOCTL_SET_RBM_MODE:
			dbg("fwq GSENSOR_MCUBE_IOCTL_SET_RBM_MODE\n");

			MC32X0_rbm(/*client,*/1);

			break;

		case GSENSOR_MCUBE_IOCTL_CLEAR_RBM_MODE:
			dbg("fwq GSENSOR_MCUBE_IOCTL_SET_RBM_MODE\n");

			MC32X0_rbm(/*client,*/0);

			break;

		case GSENSOR_MCUBE_IOCTL_REGISTER_MAP:
			dbg("fwq GSENSOR_MCUBE_IOCTL_REGISTER_MAP\n");

			MC32X0_Read_Reg_Map();

			break;
		case GSENSOR_IOCTL_READ_SENSORDATA:	
		case GSENSOR_IOCTL_READ_RAW_DATA:
			dbg("fwq GSENSOR_IOCTL_READ_RAW_DATA\n");
			tmpdata = (void __user*)arg;

			MC32X0_ReadRawData(/*client,*/ strbuf);
			if(copy_to_user(tmpdata, strbuf, 64))
			{
				err = -EFAULT;
				break;
			}	
			
			break;

	default:
		break;
	}
errioctl:
	mutex_unlock(&sense_data_mutex);
	return err;
}


static const struct file_operations mc32x0_fops = {
	.owner = THIS_MODULE,
	.read = mc32x0_read,
	.write = mc32x0_write,
	.open = mc32x0_open,
	.release = mc32x0_close,
	.unlocked_ioctl = mc32x0_ioctl,
};


static struct miscdevice mcube_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sensor_ctrl",
	.fops = &mc32x0_fops,
};


static int is_mc32x0(void)
{
	u8 tempvalue;
	u8 buf[2];
	int ret = 0;

	/* read chip id */
	tempvalue = MC32X0_CHIP_ID;

	//i2c_master_send(client, (char*)&tempvalue, 1);
	//i2c_master_recv(client, (char*)&tempvalue, 1);
	ret = mc32x0_i2c_read(tempvalue, buf,1);

	if(buf[0] != 0x01)
	{
		errlog("Can't find mc3230(mCube)! ret=%d,buf[0]=%d\n", ret, buf[0]);
		return -1;		
	}
	klog("Find mc3230(mCube g-sensor).\n");
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
			dbg("The argument to enable/disable g-sensor should be 0 or 1  !!!\n");
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
	//mc32x0acc_t acc;
	s16 acc[MC32X0_AXES_NUM];
	int tx,ty,tz,txyz;
	int rxyz[3];
	static int suf=0; // To report every merging value
	int ret = 0;

dbg("l_sensorconfig.sensor_enable=%d",l_sensorconfig.sensor_enable);
	if (!l_sensorconfig.sensor_enable)
	{
		queue_delayed_work(l_sensorconfig.queue, &l_sensorconfig.work, msecs_to_jiffies(sample_rate_2_memsec(l_sensorconfig.sensor_samp)));
		return;
	}
	if( load_cali_flg > 0)
	{
		ret =mcube_read_cali_file(/*client*/);
		if(ret == 0)
			load_cali_flg = ret;
		else 
			load_cali_flg --;
		dbg("load_cali %d\n",ret); 
	}  
	// read x,y,z
	mutex_lock(&sense_data_mutex);
	//mc32x0_read_accel_xyz(&acc);
	MC32X0_ReadData(acc);
	mutex_unlock(&sense_data_mutex);
	// report data
	rxyz[0] = acc[0];
	rxyz[1] = acc[1];
	rxyz[2] = acc[2];
	tx = rxyz[l_sensorconfig.xyz_axis[0][0]]*l_sensorconfig.xyz_axis[0][1] + l_sensorconfig.offset[0];
	ty = rxyz[l_sensorconfig.xyz_axis[1][0]]*l_sensorconfig.xyz_axis[1][1] + l_sensorconfig.offset[1];
	tz = rxyz[l_sensorconfig.xyz_axis[2][0]]*l_sensorconfig.xyz_axis[2][1] + l_sensorconfig.offset[2];
	suf++;
	if (suf > 5)
	{
		suf = 0;
	}
	txyz = (tx&0x00FF) | ((ty&0xFF)<<8) | ((tz&0xFF)<<16) | ((suf&0xFF)<<24);
	input_report_abs(l_sensorconfig.input_dev, ABS_X, txyz);
	//input_report_abs(l_sensorconfig.input_dev, ABS_Y, ty);
	//input_report_abs(l_sensorconfig.input_dev, ABS_Z, tz);
	input_sync(l_sensorconfig.input_dev);
	l_sensorconfig.test_pass = 1; // for testing
	dbg("gsensor rpt:%2x,%2x,%2x,%x\n", (u8)tx,(u8)ty,(u8)tz,txyz);
	// next to read	
	queue_delayed_work(l_sensorconfig.queue, &l_sensorconfig.work, msecs_to_jiffies(sample_rate_2_memsec(l_sensorconfig.sensor_samp)));
}


static void MC32X0_rbm(/*struct i2c_client *client, */int enable)
{
	//int err; 
	char data;

	if(enable == 1 )
	{
#if 0
		buf1[0] = 0x43; 
		err = mc3230_write_block(client, 0x07, buf1, 0x01);

		buf1[0] = 0x02; 
		err = mc3230_write_block(client, 0x14, buf1, 0x01);

		buf1[0] = 0x41; 
		err = mc3230_write_block(client, 0x07, buf1, 0x01);
#endif
		//err = mc3230_write_reg(client,0x07,0x43);
		//err = mc3230_write_reg(client,0x14,0x02);
		//err = mc3230_write_reg(client,0x07,0x41);

		data = 0x43;
		mc32x0_write_reg(0x07,&data,1);
		data = 0x02;
		mc32x0_write_reg(0x14,&data,1);
		data = 0x41;
		mc32x0_write_reg(0x07,&data,1);
		

		enable_RBM_calibration =1;
		
		dbg("set rbm!!\n");

		msleep(10);
	}
	else if(enable == 0 )  
	{
#if 0
		buf1[0] = 0x43; 
		err = mc3230_write_block(client, 0x07, buf1, 0x01);

		buf1[0] = 0x00; 
		err = mc3230_write_block(client, 0x14, buf1, 0x01);

		buf1[0] = 0x41; 
		err = mc3230_write_block(client, 0x07, buf1, 0x01);
#endif	
		//err = mc3230_write_reg(client,0x07,0x43);
		//err = mc3230_write_reg(client,0x14,0x00);
		//err = mc3230_write_reg(client,0x07,0x41);

		data = 0x43;
		mc32x0_write_reg(0x07,&data,1);
		data = 0x00;
		mc32x0_write_reg(0x14,&data,1);
		data = 0x41;
		mc32x0_write_reg(0x07,&data,1);
		enable_RBM_calibration =0;

		dbg("clear rbm!!\n");

		msleep(10);
	}
}

static int mc3230_reg_init(void/*struct i2c_client *client*/)
{
	int ret = 0;
	//int pcode = 0;
	char data;

	//mcprintkfunc("-------------------------mc3230 init------------------------\n");	
	

	
	//pcode = mc3230_read_reg(client,MC3230_REG_PRODUCT_CODE);
	mc32x0_read_reg(MC3230_REG_PRODUCT_CODE,&data,1);
	if( data ==0x19 )
	{
		mc32x0_type = IS_MC3230;
	}
	else if ( data ==0x90 )
	{
		mc32x0_type = IS_MC3210;
	}
	

	if ( mc32x0_type == IS_MC3230 )
	{
		//ret = mc3230_write_reg(client, 0x20, 0x32);
		data = 0x32;
		mc32x0_write_reg(0x20,&data,1);
	}
	else if ( mc32x0_type == IS_MC3210 )
	{
		//ret = mc3230_write_reg(client, 0x20, 0x3F);
		data = 0x3F;
		mc32x0_write_reg(0x20,&data,1);
	}


	if ( mc32x0_type == IS_MC3230 )
	{
		gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = 86;
	}
	else if ( mc32x0_type == IS_MC3210 )
	{
		gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = 1024;
	}

	MC32X0_rbm(/*client,*/0);



//	mc3230_active(client,1); 
//	mcprintkreg("mc3230 0x07:%x\n",mc3230_read_reg(client,MC3230_REG_SYSMOD));
//	enable_irq(client->irq);
//	msleep(50);
	return ret;
}

static int mc32x0_normal_init(void)
{
	int err = 0;
	s32 x_off, y_off, z_off;
	s16 tmp, x_gain, y_gain, z_gain ;

	mcube_mc32x0_init(&(l_sensorconfig.mc32x0));
	memset(offset_buf, 0, 9);
	offset_buf[0] = 0x21;
	//err = mc3230_rx_data(client, offset_buf, 9);
	err = mc32x0_i2c_read(0x21, offset_buf,9);
	if(err)
	{
		errlog("error to read: %d\n", err);
		return err;
	}

	tmp = ((offset_buf[1] & 0x3f) << 8) + offset_buf[0];
		if (tmp & 0x2000)
			tmp |= 0xc000;
		x_off = tmp;
					
	tmp = ((offset_buf[3] & 0x3f) << 8) + offset_buf[2];
		if (tmp & 0x2000)
			tmp |= 0xc000;
		y_off = tmp;
					
	tmp = ((offset_buf[5] & 0x3f) << 8) + offset_buf[4];
		if (tmp & 0x2000)
			tmp |= 0xc000;
		z_off = tmp;
					
	// get x,y,z gain
	x_gain = ((offset_buf[1] >> 7) << 8) + offset_buf[6];
	y_gain = ((offset_buf[3] >> 7) << 8) + offset_buf[7];
	z_gain = ((offset_buf[5] >> 7) << 8) + offset_buf[8];
							

	//storege the cerrunt offset data with DOT format
	offset_data[0] = x_off;
	offset_data[1] = y_off;
	offset_data[2] = z_off;

	//storege the cerrunt Gain data with GOT format
	gain_data[0] = 256*8*128/3/(40+x_gain);
	gain_data[1] = 256*8*128/3/(40+y_gain);
	gain_data[2] = 256*8*128/3/(40+z_gain);
	printk("offser gain = %d %d %d %d %d %d======================\n\n ",
		gain_data[0],gain_data[1],gain_data[2],offset_data[0],offset_data[1],offset_data[2]);

	mc3230_reg_init();
	return 0;

}

static int mc32x0_probe(struct platform_device *pdev)
{
	int err = 0;
	s32 x_off, y_off, z_off;
	s16 tmp, x_gain, y_gain, z_gain ;

	load_cali_flg = 30;
	//register ctrl dev
	err = misc_register(&mcube_device);
	if (err !=0) {
		errlog("Can't register mcube_device!\n");
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
	input_set_abs_params(l_sensorconfig.input_dev, ABS_X, 0, 0x05ffffff, 0, 0);
	/* y-axis acceleration */
	//input_set_abs_params(l_sensorconfig.input_dev, ABS_Y, -512, 512, 0, 0);
	/* z-axis acceleration */
	//input_set_abs_params(l_sensorconfig.input_dev, ABS_Z, -512, 512, 0, 0);

	l_sensorconfig.input_dev->name = "g-sensor";

	err = input_register_device(l_sensorconfig.input_dev);

	if (err) {
		errlog("Unable to register input device: %s\n",
		       l_sensorconfig.input_dev->name);
		goto exit_input_register_device_failed;
	}


	l_sensorconfig.mc32x0.bus_write = mc32x0_i2c_write;
	l_sensorconfig.mc32x0.bus_read = mc32x0_i2c_read;
	
	mcube_mc32x0_init(&(l_sensorconfig.mc32x0));
	memset(offset_buf, 0, 9);
	offset_buf[0] = 0x21;
	//err = mc3230_rx_data(client, offset_buf, 9);
	err = mc32x0_i2c_read(0x21, offset_buf,9);
	if(err)
	{
		errlog("error: %d\n", err);
		return err;
	}

	tmp = ((offset_buf[1] & 0x3f) << 8) + offset_buf[0];
		if (tmp & 0x2000)
			tmp |= 0xc000;
		x_off = tmp;
					
	tmp = ((offset_buf[3] & 0x3f) << 8) + offset_buf[2];
		if (tmp & 0x2000)
			tmp |= 0xc000;
		y_off = tmp;
					
	tmp = ((offset_buf[5] & 0x3f) << 8) + offset_buf[4];
		if (tmp & 0x2000)
			tmp |= 0xc000;
		z_off = tmp;
					
	// get x,y,z gain
	x_gain = ((offset_buf[1] >> 7) << 8) + offset_buf[6];
	y_gain = ((offset_buf[3] >> 7) << 8) + offset_buf[7];
	z_gain = ((offset_buf[5] >> 7) << 8) + offset_buf[8];
							

	//storege the cerrunt offset data with DOT format
	offset_data[0] = x_off;
	offset_data[1] = y_off;
	offset_data[2] = z_off;

	//storege the cerrunt Gain data with GOT format
	gain_data[0] = 256*8*128/3/(40+x_gain);
	gain_data[1] = 256*8*128/3/(40+y_gain);
	gain_data[2] = 256*8*128/3/(40+z_gain);
	printk("offser gain = %d %d %d %d %d %d======================\n\n ",
		gain_data[0],gain_data[1],gain_data[2],offset_data[0],offset_data[1],offset_data[2]);

	mc3230_reg_init();
	return 0;
exit_input_register_device_failed:
	input_free_device(l_sensorconfig.input_dev);	
exit_input_dev_alloc_failed:
	// release proc
	remove_proc_entry(GSENSOR_PROC_NAME, NULL);
	l_sensorconfig.sensor_proc = NULL;
	// unregister the ctrl dev
	misc_deregister(&mcube_device);

	return err;
}

static int mc32x0_remove(struct platform_device *pdev)
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
	misc_deregister(&mcube_device);
	input_unregister_device(l_sensorconfig.input_dev);
	return 0;
}

extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);
static int get_axisset(void* param)
{
	char varbuf[64];
	int n;
	int varlen;

	memset(varbuf, 0, sizeof(varbuf));
	varlen = sizeof(varbuf);
	if (wmt_getsyspara("wmt.io.mc3230sensor", varbuf, &varlen)) {
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
			printk(KERN_ERR "gsensor format is error in u-boot!!!\n");
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

static void mc32x0_platform_release(struct device *device)
{
    return;
}


static struct platform_device mc32x0_device = {
    .name           = GSENSOR_NAME,
    .id             = 0,
    .dev            = {
    	.release = mc32x0_platform_release,
    },
};

static void mc32x0_early_suspend(struct early_suspend *h)
{
	dbg("start\n");
	cancel_delayed_work_sync(&l_sensorconfig.work);
	dbg("exit\n");
}

static void mc32x0_late_resume(struct early_suspend *h)
{
	dbg("start\n");
	mc32x0_normal_init();
	mc32x0_start();
	load_cali_flg = 10;
	queue_delayed_work(l_sensorconfig.queue, &l_sensorconfig.work, msecs_to_jiffies(sample_rate_2_memsec(l_sensorconfig.sensor_samp)));
	dbg("exit\n");
}


static int mc32x0_i2c_suspend(struct platform_device *pdev, pm_message_t state)
{
	dbg("...\n");
	
	return 0;
}

static int mc32x0_i2c_resume(struct platform_device *pdev)
{
	klog("...\n");
	
	return 0;
}


static struct platform_driver mc32x0_driver = {
	.probe = mc32x0_probe,
	.remove = mc32x0_remove,
	.suspend	= mc32x0_i2c_suspend,
	.resume		= mc32x0_i2c_resume,
	.driver = {
		   .name = GSENSOR_NAME,
		   },
};


static int __init MC32X0_init(void)
{
	int ret = 0;
	
	// find the device
	if(is_mc32x0() != 0)
	{
		return -1;
	}	
	// parse g-sensor u-boot arg
	ret = get_axisset(NULL); 
	/*if ((ret != 0) || !l_sensorconfig.op)
		return -EINVAL;*/
	// create the platform device
	l_dev_class = class_create(THIS_MODULE, GSENSOR_NAME);
	if (IS_ERR(l_dev_class)){
		ret = PTR_ERR(l_dev_class);
		printk(KERN_ERR "Can't class_create gsensor device !!\n");
		return ret;
	}
    if((ret = platform_device_register(&mc32x0_device)))
    {
    	klog("Can't register mc3230 platform devcie!!!\n");
    	return ret;
    }
    if ((ret = platform_driver_register(&mc32x0_driver)) != 0)
    {
    	errlog("Can't register mc3230 platform driver!!!\n");
    	return ret;
    }
    //klog("mc3230 g-sensor driver load!\n");
#ifdef CONFIG_HAS_EARLYSUSPEND
        l_sensorconfig.earlysuspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
        l_sensorconfig.earlysuspend.suspend = mc32x0_early_suspend;
        l_sensorconfig.earlysuspend.resume = mc32x0_late_resume;
        register_early_suspend(&l_sensorconfig.earlysuspend);
#endif

    mc32x0_start();
    dbg("l_sensorconfig.sensor_samp=%d\n", l_sensorconfig.sensor_samp);
    queue_delayed_work(l_sensorconfig.queue, &l_sensorconfig.work, msecs_to_jiffies(sample_rate_2_memsec(l_sensorconfig.sensor_samp)));
	return 0;
}

static void __exit MC32X0_exit(void)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&l_sensorconfig.earlysuspend);
#endif
    platform_driver_unregister(&mc32x0_driver);
    platform_device_unregister(&mc32x0_device);
	class_destroy(l_dev_class);
}


MODULE_DESCRIPTION("MC32X0 driver");
MODULE_LICENSE("GPL");

module_init(MC32X0_init);
module_exit(MC32X0_exit);
