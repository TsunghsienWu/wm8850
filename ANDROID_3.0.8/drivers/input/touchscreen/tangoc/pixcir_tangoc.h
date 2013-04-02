#ifndef _LINUX_PIXCIR_I2C_TS_H_
#define _LINUX_PIXCIR_I2C_TS_H_

#define  TOUCH_KEY
#define  NUM_KEYS           4
#define  PEN_DOWN           1
#define  PEN_UP             0
#define  I2C_BUS1		    0x01
#define  PIXCIR_ADDR		0x5c
#define	 BOOTLOADER_ADDR	0x5d


#define	 CALIBRATION_FLAG	1
#define	 BOOTLOADER		    7
#define  RESET_TP		    9

#define	 ENABLE_IRQ		    10
#define	 DISABLE_IRQ		11
#define	 BOOTLOADER_STU		12
#define  ATTB_VALUE		    13
#define  DEV_PIXCIR "TangoC44-touch"

struct pixcir_data {
    u16 addr;
    const char *name;

	struct input_dev *input_dev;
	struct delayed_work read_work;
	struct workqueue_struct *workqueue;
    struct kobject *kobj;
 #ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
    int earlysus;

    int xresl;
    int yresl;

    int irq;
    int igp_idx;
    int igp_bit;
    
    int rgp_idx;
    int rgp_bit;

    int xch;
    int ych;
    int swap;

    int penup;
    int dbg;
#ifdef TOUCH_KEY
    int tkey_pressed;
    int tkey_idx;
#endif  
};

extern int wmt_setsyspara(char *varname, unsigned char *varval);
extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);
extern int wmt_i2c_xfer_continue_if_4(struct i2c_msg *msg, unsigned int num,int bus_id);

//#define PIXCIR_DEBUG

#undef dbg
#ifdef PIXCIR_DEBUG
	#define dbg(fmt,args...)  printk("DBG:%s_%d:"fmt,__FUNCTION__,__LINE__,##args)
#else
	#define dbg(fmt,args...)  
#endif

#undef dbg_err
#define dbg_err(fmt,args...)  printk("ERR:%s_%d:"fmt,__FUNCTION__,__LINE__,##args)


#endif
