#ifndef __GSENSOR_H__
#define __GSENSOR_H__

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define MC32X0_I2C_NAME		"mc32x0"
#define MC32X0_I2C_ADDR		0x4c

#define GSENSOR_I2C_NAME MC32X0_I2C_NAME
#define GSENSOR_I2C_ADDR MC32X0_I2C_ADDR

struct i2c_board_info gsensor_i2c_board_info = {
	.type          = GSENSOR_I2C_NAME,
	.flags         = 0x00,
	.addr          = GSENSOR_I2C_ADDR,
	.platform_data = NULL,
	.archdata      = NULL,
	.irq           = -1,
};
/*
struct gsensor_data {
	struct mutex lock;
	struct delayed_work input_work;
	struct input_dev *input_dev;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend earlysuspend;
#endif
	int hw_initialized;
	atomic_t enabled;
	int i2c_xfer_complete;
	int suspend;
};
*/

/* Function prototypes */
int gsensor_i2c_register_device (void);

#endif