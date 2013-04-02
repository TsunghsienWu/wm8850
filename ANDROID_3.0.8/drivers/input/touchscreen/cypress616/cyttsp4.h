#ifndef __LINUX_cyttsp4_TS_H__
#define __LINUX_cyttsp4_TS_H__


#define DEV_cyttsp4          "cyttsp4-touch"
#define I2C_BUS1      0x01

#define SUPPORT_POINT_NUM   10
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
#define CY_NUM_DAT                  6

/* GEN4/SOLO Operational interface definitions */
struct cyttsp4_touch {
	u8 xh;
	u8 xl;
	u8 yh;
	u8 yl;
	u8 z;
	u8 t;
	u8 size;
} __attribute__((packed));

struct cyttsp4_xydata {
	u8 hst_mode;
	u8 reserved;
	u8 cmd;
	u8 dat[CY_NUM_DAT];
	u8 rep_len;
	u8 rep_stat;
	u8 tt_stat;
	struct cyttsp4_touch tch[CY_NUM_TCH_ID];
} __attribute__((packed));

struct cyttsp4_data {
    int id;
    u16 addr;
    const char *name;

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

    int xresl;
    int yresl;

    int tw;
    int th;

    int irq;
    int igp_idx;
    int igp_bit;
    
    int rgp_idx;
    int rgp_bit;

    int nt;
    int swap;

    int upg;
    int xch;
    int dbg;

};


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
