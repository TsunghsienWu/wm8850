#ifndef __NOVATEK_TOUCH_H__
#define __NOVATEK_TOUCH_H__

//define default resolution of the touchscreen
#define TOUCH_MAX_HEIGHT 	1344			
#define TOUCH_MAX_WIDTH		2368

//define default resolution of LCM
#define SCREEN_MAX_HEIGHT   600
#define SCREEN_MAX_WIDTH    1024

#define DRIVER_SEND_CFG
#define NT11002   0
#define NT11003   1
#define NT11004   2

#define DEV_NOVATEK     "novatek-touch"
#define DEV_NT11002     "ntk02-touch"
#define DEV_NT11003     "ntk03-touch"
#define DEV_NT11004     "ntk04-touch"

#define NT11002_ADDR    0x01
#define NT11003_ADDR    0x01
#define NT11004_ADDR    0x01
#define I2C_BUS1        0x01


#define IC_DEFINE        NT11003
#define READ_COOR_ADDR   0x00
#if IC_DEFINE == NT11002
#define IIC_BYTENUM     4
#define MAX_FINGER_NUM	5   //    10	
#elif IC_DEFINE == NT11003
#define IIC_BYTENUM     6
#define MAX_FINGER_NUM	10   //    10	
#elif IC_DEFINE == NT11004
#define IIC_BYTENUM     6
#define MAX_FINGER_NUM	2   //    1
#endif

//Set IIC Protocol 
#define OLD_FORMAT      0
#define NEW_FORMAT      1
#define IIC_FORMAT      OLD_FORMAT
//Set Suspend type 
#define SI_MODE         0
#define PORTRESUME      1
#define SUSPEND_TYPE    PORTRESUME

#define TOUCHKEY_EVENT
#define MAX_KEY_NUM     4
 #define MENU           24
 #define HOME           23
 #define BACK           22
 #define SEARCH         21
///////////////////////////////////////////////////////////////////////////////////
struct novatek_data {
    int id;
	u8 addr;
    const char *name;
	struct input_dev *input_dev;
	struct work_struct work;
    struct workqueue_struct *workqueue;
	struct early_suspend esuspend;

    int igp_idx;
    int igp_bit;

    int rgp_idx;
    int rgp_bit;

    int reslx;
    int resly;

    int tw;
    int th;
    
    int nt;
    int nbyte;
    
    int irq;
    int penup;
    int earlysus;
	
};

//#define NTK_DEBUG

#undef dbg
#ifdef NTK_DEBUG
	#define dbg(fmt,args...)  printk("DBG:%s_%d:"fmt,__FUNCTION__,__LINE__,##args)
#else
	#define dbg(fmt,args...)  
#endif

#undef dbg_err
#define dbg_err(fmt,args...)  printk("ERR:%s_%d:"fmt,__FUNCTION__,__LINE__,##args)

#define DEV_NAME 				    "wmtts"
#define DEV_MAJOR					11

#define TS_IOC_MAGIC  				't'
#define TS_IOCTL_CAL_START    		_IO(TS_IOC_MAGIC,   1)
#define TS_IOCTL_CAL_DONE     		_IOW(TS_IOC_MAGIC,  2, int*)
#define TS_IOCTL_GET_RAWDATA  		_IOR(TS_IOC_MAGIC,  3, int*)
#define TS_IOCTL_CAL_QUIT			_IOW(TS_IOC_MAGIC,  4, int*)
#define TS_IOCTL_CAL_CAP	        _IOW(TS_IOC_MAGIC,  5, int*)
#define TS_IOC_MAXNR          		5

extern int wmt_setsyspara(char *varname, unsigned char *varval);
extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);
extern int wmt_i2c_xfer_continue_if_4(struct i2c_msg *msg, unsigned int num,int bus_id);

#endif
