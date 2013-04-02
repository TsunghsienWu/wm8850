#ifndef _LINUX_SIT_I2C_H
#define _LINUX_SIT_I2C_H

#define SITRONIX_ADDR 0x60
#define DEV_SITRONIX "sitronix-touch"
#define SITRONIX_MAX_SUPPORTED_POINT 5
#define TOUCH_KEY 

struct sitronix_data {
    u16 addr;
    const char *name;

	struct input_dev *input_dev;
	struct work_struct read_work;
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
    //int tkey_pressed;
    int tkey_idx;
#endif  
	u8 fw_revision[4];
	int resolution_x;
	int resolution_y;
	u8 max_touches;
	u8 touch_protocol_type;
	u8 chip_id;

    u8 Num_X;
	u8 Num_Y;
	u8 sensing_mode;
    u8 pixel_length;
};

typedef enum{
	FIRMWARE_VERSION,
	STATUS_REG,
	DEVICE_CONTROL_REG,
	TIMEOUT_TO_IDLE_REG,
	XY_RESOLUTION_HIGH,
	X_RESOLUTION_LOW,
	Y_RESOLUTION_LOW,
	FIRMWARE_REVISION_3 = 0x0C,
	FIRMWARE_REVISION_2,
	FIRMWARE_REVISION_1,
	FIRMWARE_REVISION_0,
	FINGERS,
	KEYS_REG,
	XY0_COORD_H,
	X0_COORD_L,
	Y0_COORD_L,
	I2C_PROTOCOL = 0x3E,
	MAX_NUM_TOUCHES,
	DATA_0_HIGH,
	DATA_0_LOW,
	CHIP_ID = 0xF4,

	PAGE_REG = 0xff,
}RegisterOffset;


typedef enum{
	XY_COORD_H,
	X_COORD_L,
	Y_COORD_L,
	PIXEL_DATA_LENGTH_B,
	PIXEL_DATA_LENGTH_A,
}PIXEL_DATA_FORMAT;

#define X_RES_H_SHFT 4
#define X_RES_H_BMSK 0xf
#define Y_RES_H_SHFT 0
#define Y_RES_H_BMSK 0xf
#define FINGERS_SHFT 0
#define FINGERS_BMSK 0xf
#define X_COORD_VALID_SHFT 7
#define X_COORD_VALID_BMSK 0x1
#define X_COORD_H_SHFT 4
#define X_COORD_H_BMSK 0x7
#define Y_COORD_H_SHFT 0
#define Y_COORD_H_BMSK 0x7

typedef enum{
	SITRONIX_RESERVED_TYPE_0,
	SITRONIX_A_TYPE,
	SITRONIX_B_TYPE,
}I2C_PROTOCOL_TYPE;

#define I2C_PROTOCOL_SHFT 0x0
#define I2C_PROTOCOL_BMSK 0x3

typedef enum{
	SENSING_BOTH,
	SENSING_X_ONLY,
	SENSING_Y_ONLY,
	SENSING_BOTH_NOT,
}ONE_D_SENSING_CONTROL_MODE;

#define ONE_D_SENSING_CONTROL_SHFT 0x2
#define ONE_D_SENSING_CONTROL_BMSK 0x3

extern int wmt_setsyspara(char *varname, unsigned char *varval);
extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);
extern int wmt_i2c_xfer_continue_if_4(struct i2c_msg *msg, unsigned int num,int bus_id);

//#define SITRONIX_DEBUG

#undef dbg
#ifdef SITRONIX_DEBUG
	#define dbg(fmt,args...)  printk("DBG:%s_%d:"fmt,__FUNCTION__,__LINE__,##args)
#else
	#define dbg(fmt,args...)  
#endif

#undef dbg_err
#define dbg_err(fmt,args...)  printk("ERR:%s_%d:"fmt,__FUNCTION__,__LINE__,##args)

#endif /* _LINUX_SIS_I2C_H */
