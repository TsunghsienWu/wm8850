#ifndef _LINUX_SIS_I2C_H
#define _LINUX_SIS_I2C_H

#define TOUCH_KEY
#define I2C_BUS1 0X01

struct sis9210_data {
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

    int dbg;
#ifdef TOUCH_KEY
    int vkey_pressed;
    int vkey_idx;
#endif  
};


// For SiS9200 i2c data format
// Define if for SMBus Tx/Rx in X86.
// Undef it for I2C Tx/Rx in Embedded System.
#define TOUCH_KEY
#define DEV_SIS92X "sis9210-touch"
#define SIS92X_ADDR	0x5


#define TIMER_NS    12500000

#ifdef _FOR_LEGACY_SIS81X
#define MAX_FINGERS			2
#else   // SiS9200
#define MAX_FINGERS			10
#endif


#define SIS_CMD_NORMAL							   0x0
#define SIS_CMD_RECALIBRATE		                   0x87
#define MAX_READ_BYTE_COUNT						   16
#define MSK_BUTTON_POINT						   0xf0
#define MSK_TOUCHNUM							   0x0f
#define MSK_HAS_CRC								   0x10
#define MSK_DATAFMT								   0xe0
#define MSK_PSTATE								   0x0f
#define MSK_PID	                                   0xf0
#define TOUCHDOWN								   0x0
#define TOUCHUP									   0x1
#define RES_FMT									   0x00
#define FIX_FMT									   0x40
#define BUTTON_TOUCH_SERIAL				           0x70

//POWER MODE
#define SIS_CMD_POWERMODE          0x90
#define	MSK_

#ifdef _SMBUS_INTERFACE
#define CMD_BASE	0
#else
#define CMD_BASE	1	//i2c
#endif

#define PKTINFO									   CMD_BASE + 1

#define FORMAT_MODE								   1
#define P1TSTATE								   2
#define BUTTON_STATE					           (CMD_BASE + 1)

#define NO_TOUCH								   0x02
#define SINGLE_TOUCH							   0x09
#define MULTI_TOUCH								   0x0e
#define LAST_ONE								   0x07
#define LAST_TWO								   0x0c
#define BUTTON_TOUCH							   0x05
#define BUTTON_TOUCH_ONE_POINT			           0x0a
#define BUTTON_TOUCH_MULTI_TOUCH		           0x0f


#define CRCCNT(x) ((x + 0x1) & (~0x1))

extern int wmt_setsyspara(char *varname, unsigned char *varval);
extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);
extern int wmt_i2c_xfer_continue_if_4(struct i2c_msg *msg, unsigned int num,int bus_id);

//#define SIS92X_DEBUG

#undef dbg
#ifdef SIS92X_DEBUG
	#define dbg(fmt,args...)  printk("DBG:%s_%d:"fmt,__FUNCTION__,__LINE__,##args)
#else
	#define dbg(fmt,args...)  
#endif

#undef dbg_err
#define dbg_err(fmt,args...)  printk("ERR:%s_%d:"fmt,__FUNCTION__,__LINE__,##args)

#endif /* _LINUX_SIS_I2C_H */
