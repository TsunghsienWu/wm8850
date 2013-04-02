#ifndef __LINUX_FT5X0X_TS_H__
#define __LINUX_FT5X0X_TS_H__

#define DEV_FT5206          "ft5206-touch"
#define DEV_FT5301          "ft5301-touch"
#define DEV_FT5302          "ft5302-touch"
#define DEV_FT5306          "ft5306-touch"
#define DEV_FT5406          "ft5406-touch"
#define DEV_FT5606          "ft5606-touch"

#define DEV_FT5X0X          "ft5x0x-touch"

#define FT5406_I2C_ADDR     0x38
#define FT5X0X_I2C_BUS      0x01

enum FT5X0X_ID{
    FT5206 =1,
    FT5301,
    FT5302,
    FT5306,
    FT5406,
    FT5606,
    FT5X0X,
};

struct vt1603_ts_cal_info {
    int   a1;
    int   b1;
    int   c1;
    int   a2;
    int   b2;
    int   c2;
    int   delta;
};

#define SUPPORT_POINT_NUM   10
struct ts_event {
	int x[SUPPORT_POINT_NUM];
	int y[SUPPORT_POINT_NUM];
    int tid[SUPPORT_POINT_NUM];
    int tpoint;
};

#define TOUCH_KEY

#ifdef TOUCH_KEY
#define NUM_KEYS 4
struct key_pos{
    int y_lower;
    int y_upper;
};

struct ts_key{
    int axis;
    int x_lower;
    int x_upper;
    struct key_pos ypos[NUM_KEYS];
};
#endif

struct ft5x0x_data {
    int id;
    u16 addr;
    const char *name;
    u8 fw_name[64];

	struct input_dev *input_dev;
	struct ts_event event;
	struct work_struct read_work;
	struct workqueue_struct *workqueue;
    struct mutex ts_mutex;
    struct kobject *kobj;
 #ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
    int earlysus;

    int reslx;
    int resly;

    int tw;
    int th;

    int irq;
    int igp_idx;
    int igp_bit;
    
    int rgp_idx;
    int rgp_bit;

    int nt;
    int nb;
    int xch;
    int ych;
    int swap;

    int upg;
    int dbg;
#ifdef TOUCH_KEY
    int tskey_used;
    int tkey_pressed;
    int nkeys;
    int tkey_idx;
    struct ts_key tkey;
#endif    

};

enum ft5x0x_ts_regs {
	FT5X0X_REG_THGROUP					= 0x80,     /* touch threshold, related to sensitivity */
	FT5X0X_REG_THPEAK					= 0x81,
	FT5X0X_REG_THCAL					= 0x82,
	FT5X0X_REG_THWATER					= 0x83,
	FT5X0X_REG_THTEMP					= 0x84,
	FT5X0X_REG_THDIFF					= 0x85,				
	FT5X0X_REG_CTRL						= 0x86,
	FT5X0X_REG_TIMEENTERMONITOR			= 0x87,
	FT5X0X_REG_PERIODACTIVE				= 0x88,      /* report rate */
	FT5X0X_REG_PERIODMONITOR			= 0x89,
	FT5X0X_REG_HEIGHT_B					= 0x8a,
	FT5X0X_REG_MAX_FRAME				= 0x8b,
	FT5X0X_REG_DIST_MOVE				= 0x8c,
	FT5X0X_REG_DIST_POINT				= 0x8d,
	FT5X0X_REG_FEG_FRAME				= 0x8e,
	FT5X0X_REG_SINGLE_CLICK_OFFSET		= 0x8f,
	FT5X0X_REG_DOUBLE_CLICK_TIME_MIN	= 0x90,
	FT5X0X_REG_SINGLE_CLICK_TIME		= 0x91,
	FT5X0X_REG_LEFT_RIGHT_OFFSET		= 0x92,
	FT5X0X_REG_UP_DOWN_OFFSET			= 0x93,
	FT5X0X_REG_DISTANCE_LEFT_RIGHT		= 0x94,
	FT5X0X_REG_DISTANCE_UP_DOWN		    = 0x95,
	FT5X0X_REG_ZOOM_DIS_SQR				= 0x96,
	FT5X0X_REG_RADIAN_VALUE				=0x97,
	FT5X0X_REG_MAX_X_HIGH               = 0x98,
	FT5X0X_REG_MAX_X_LOW             	= 0x99,
	FT5X0X_REG_MAX_Y_HIGH            	= 0x9a,
	FT5X0X_REG_MAX_Y_LOW             	= 0x9b,
	FT5X0X_REG_K_X_HIGH            		= 0x9c,
	FT5X0X_REG_K_X_LOW             		= 0x9d,
	FT5X0X_REG_K_Y_HIGH            		= 0x9e,
	FT5X0X_REG_K_Y_LOW             		= 0x9f,
	FT5X0X_REG_AUTO_CLB_MODE			= 0xa0,
	FT5X0X_REG_LIB_VERSION_H 			= 0xa1,
	FT5X0X_REG_LIB_VERSION_L 			= 0xa2,		
	FT5X0X_REG_CIPHER					= 0xa3,
	FT5X0X_REG_MODE						= 0xa4,
	FT5X0X_REG_PMODE					= 0xa5,	  /* Power Consume Mode		*/	
	FT5X0X_REG_FIRMID					= 0xa6,   /* Firmware version */
	FT5X0X_REG_STATE					= 0xa7,
	FT5X0X_REG_FT5201ID					= 0xa8,
	FT5X0X_REG_ERR						= 0xa9,
	FT5X0X_REG_CLB						= 0xaa,
};

//FT5X0X_REG_PMODE
#define PMODE_ACTIVE        0x00
#define PMODE_MONITOR       0x01
#define PMODE_STANDBY       0x02
#define PMODE_HIBERNATE     0x03

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

//#define FT_DEBUG

#undef dbg
#ifdef FT_DEBUG
	#define dbg(fmt,args...)  printk("DBG:%s_%d:"fmt,__FUNCTION__,__LINE__,##args)
#else
	#define dbg(fmt,args...)  
#endif

#undef dbg_err
#define dbg_err(fmt,args...)  printk("ERR:%s_%d:"fmt,__FUNCTION__,__LINE__,##args)

#endif
