#ifndef __LINUX_cyttsp2_TS_H__
#define __LINUX_cyttsp2_TS_H__


#define DEV_cyttsp2          "cyttsp2-touch"
#define I2C_BUS1      		0x01

#define SUPPORT_POINT_NUM   2
struct ts_event {
	int x[SUPPORT_POINT_NUM];
	int y[SUPPORT_POINT_NUM];
    int tpoint;
};

#define CY_NUM_TCH_ID               10
#define GET_NUM_TOUCHES(x)          ((x) & 0x1F)
#define IS_LARGE_AREA(x)            (((x) & 0x20) >> 5)
#define IS_BAD_PKT(x)               ((x) & 0x20)
#define IS_VALID_APP(x)             ((x) & 0x01)
#define IS_OPERATIONAL_ERR(x)       ((x) & 0x3F)
#define GET_HSTMODE(reg)            ((reg & 0x70) >> 4)
#define GET_BOOTLOADERMODE(reg)     ((reg & 0x10) >> 4)

#define CY_GET_EVENTID(reg)         ((reg & 0x60) >> 5)
#define CY_GET_TRACKID(reg)         (reg & 0x1F)
#define CY_NUM_DAT                  

#undef abs
#define abs(x) (((x)>0)?(x):(-(x)))

struct cyttsp2_data {
    int id;
    u16 addr;
    const char *name;

	struct input_dev *input_dev;
	struct delayed_work read_work;
	struct workqueue_struct *workqueue;
    struct mutex ts_mutex;
    struct kobject *kobj;
 #ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
    int earlysus;

    int xresl;
    int yresl;

    int pre_x;
    int pre_y;

    int irq;
    int igp_idx;
    int igp_bit;
    
    int rgp_idx;
    int rgp_bit;

    int nt;

    int t_cnt;
    int dbg;

};

/* Bootloader File 0 offset */
#define CY_BL_FILE0			0x00

/* Bootloader command directive */
#define CY_BL_CMD			0xFF

/* Bootloader Initiate Bootload */
#define CY_BL_INIT_LOAD			0x38

/* Bootloader Write a Block */
#define CY_BL_WRITE_BLK			0x39

/* Bootloader Terminate Bootload */
#define CY_BL_TERMINATE			0x3B

/* Bootloader Exit and Verify Checksum command */
#define CY_BL_EXIT			0xA5

/* Bootloader default keys */
#define CY_BL_KEY0			0x00
#define CY_BL_KEY1			0x01
#define CY_BL_KEY2			0x02
#define CY_BL_KEY3			0x03
#define CY_BL_KEY4			0x04
#define CY_BL_KEY5			0x05
#define CY_BL_KEY6			0x06
#define CY_BL_KEY7			0x07

/* Active Power state scanning/processing refresh interval */
#define CY_ACT_INTRVL_DFLT		0x00

/* touch timeout for the Active power */
#define CY_TCH_TMOUT_DFLT		0xFF

/* Low Power state scanning/processing refresh interval */
#define CY_LP_INTRVL_DFLT		0x0A

#define CY_IDLE_STATE		0
#define CY_ACTIVE_STATE		1
#define CY_LOW_PWR_STATE		2
#define CY_SLEEP_STATE		3

/* device mode bits */
#define CY_OP_MODE		0x00
#define CY_SYSINFO_MODE		0x10

/* power mode select bits */
#define CY_SOFT_RESET_MODE		0x01	/* return to Bootloader mode */
#define CY_DEEP_SLEEP_MODE		0x02
#define CY_LOW_PWR_MODE		0x04

#define CY_NUM_KEY			8




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

//#define CY_DEBUG

#undef dbg
#ifdef CY_DEBUG
	#define dbg(fmt,args...)  printk("DBG:%s_%d:"fmt,__FUNCTION__,__LINE__,##args)
#else
	#define dbg(fmt,args...)  
#endif

#undef dbg_err
#define dbg_err(fmt,args...)  printk("ERR:%s_%d:"fmt,__FUNCTION__,__LINE__,##args)

#endif
