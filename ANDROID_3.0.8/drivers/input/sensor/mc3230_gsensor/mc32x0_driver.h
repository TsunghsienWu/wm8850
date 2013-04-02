/*
 * Copyright (C) 2011 MCUBE, Inc.
 *
 * Initial Code:
 *	Tan Liang
 */



#ifndef __MC32X0_H__
#define __MC32X0_H__



#define MC32X0_WR_FUNC_PTR int (* bus_write)(unsigned char, unsigned char *, unsigned char)

#define MC32X0_BUS_WRITE_FUNC(dev_addr, reg_addr, reg_data, wr_len)\
           bus_write(reg_addr, reg_data, wr_len)

#define MC32X0_RD_FUNC_PTR int (* bus_read)( unsigned char, unsigned char *, unsigned char)

#define MC32X0_BUS_READ_FUNC(dev_addr, reg_addr, reg_data, r_len)\
           bus_read(reg_addr, reg_data, r_len)

#define GET_REAL_VALUE(rv, bn) \
	((rv & (0x01 << (bn - 1))) ? (- (rv & ~(0xffff << (bn - 1)))) : (rv & ~(0xffff << (bn - 1))))



//#define MC32X0_HIGH_END
/*******MC3210/20 define this**********/

//#define MCUBE_2G_10BIT_TAP
//#define MCUBE_2G_10BIT
//#define MCUBE_8G_14BIT_TAP
//#define MCUBE_8G_14BIT



#define MC32X0_LOW_END
/*******MC3230 define this**********/

#define MCUBE_1_5G_8BIT
//#define MCUBE_1_5G_8BIT_TAP





/** MC32X0 I2C Address
*/

#define MC32X0_I2C_ADDR		0x4c // 0x98 >> 1



/*
	MC32X0 API error codes
*/

#define E_NULL_PTR		(char)-127

/* 
 *	
 *	register definitions 	
 *
 */

#define MC32X0_XOUT_REG						0x00
#define MC32X0_YOUT_REG						0x01
#define MC32X0_ZOUT_REG						0x02
#define MC32X0_Tilt_Status_REG				0x03
#define MC32X0_Sampling_Rate_Status_REG		0x04
#define MC32X0_Sleep_Count_REG				0x05
#define MC32X0_Interrupt_Enable_REG			0x06
#define MC32X0_Mode_Feature_REG				0x07
#define MC32X0_Sample_Rate_REG				0x08
#define MC32X0_Tap_Detection_Enable_REG		0x09
#define MC32X0_TAP_Dwell_Reject_REG			0x0a
#define MC32X0_DROP_Control_Register_REG	0x0b
#define MC32X0_SHAKE_Debounce_REG			0x0c
#define MC32X0_XOUT_EX_L_REG				0x0d
#define MC32X0_XOUT_EX_H_REG				0x0e
#define MC32X0_YOUT_EX_L_REG				0x0f
#define MC32X0_YOUT_EX_H_REG				0x10
#define MC32X0_ZOUT_EX_L_REG				0x11
#define MC32X0_ZOUT_EX_H_REG				0x12
#define MC32X0_CHIP_ID						0x18
#define MC32X0_RANGE_Control_REG			0x20
#define MC32X0_SHAKE_Threshold_REG			0x2B
#define MC32X0_UD_Z_TH_REG					0x2C
#define MC32X0_UD_X_TH_REG					0x2D
#define MC32X0_RL_Z_TH_REG					0x2E
#define MC32X0_RL_Y_TH_REG					0x2F
#define MC32X0_FB_Z_TH_REG					0x30
#define MC32X0_DROP_Threshold_REG			0x31
#define MC32X0_TAP_Threshold_REG			0x32




/** MC32X0 acceleration data 
	\brief Structure containing acceleration values for x,y and z-axis in signed short

*/

typedef struct  {
		short x, /**< holds x-axis acceleration data sign extended. Range -512 to 511. */
			  y, /**< holds y-axis acceleration data sign extended. Range -512 to 511. */
			  z; /**< holds z-axis acceleration data sign extended. Range -512 to 511. */
} mc32x0acc_t;

/* RANGE */

#define MC32X0_RANGE__POS				2
#define MC32X0_RANGE__LEN				2
#define MC32X0_RANGE__MSK				0x0c	
#define MC32X0_RANGE__REG				MC32X0_RANGE_Control_REG

/* MODE */

#define MC32X0_MODE__POS				0
#define MC32X0_MODE__LEN				2
#define MC32X0_MODE__MSK				0x03	
#define MC32X0_MODE__REG				MC32X0_Mode_Feature_REG

#define MC32X0_MODE_DEF 				0x43


/* BANDWIDTH */

#define MC32X0_BANDWIDTH__POS			4
#define MC32X0_BANDWIDTH__LEN			3
#define MC32X0_BANDWIDTH__MSK			0x70	
#define MC32X0_BANDWIDTH__REG			MC32X0_RANGE_Control_REG


#define MC32X0_GET_BITSLICE(regvar, bitname)\
			(regvar & bitname##__MSK) >> bitname##__POS


#define MC32X0_SET_BITSLICE(regvar, bitname, val)\
		  (regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK)  






#define MC32X0_RANGE_2G					0
#define MC32X0_RANGE_4G					1
#define MC32X0_RANGE_8G					2


#define MC32X0_WAKE						1
#define MC32X0_SNIFF					2
#define MC32X0_STANDBY					3


#define MC32X0_LOW_PASS_512HZ			0
#define MC32X0_LOW_PASS_256HZ			1
#define MC32X0_LOW_PASS_128HZ			2
#define MC32X0_LOW_PASS_64HZ			3
#define MC32X0_LOW_PASS_32HZ			4
#define MC32X0_LOW_PASS_16HZ			5
#define MC32X0_LOW_PASS_8HZ				6





typedef struct {	
	unsigned char mode;		/**< save current MC32X0 operation mode */
	unsigned char chip_id;	/**< save MC32X0's chip id which has to be 0x00/0x01 after calling mc32x0_init() */
	unsigned char dev_addr;   /**< initializes MC32X0's I2C device address 0x4c */
	MC32X0_WR_FUNC_PTR;		  /**< function pointer to the SPI/I2C write function */
	MC32X0_RD_FUNC_PTR;		  /**< function pointer to the SPI/I2C read function */
} mc32x0_t;



/* Function prototypes */




int mcube_mc32x0_init(mc32x0_t *mc32x0);

int mc32x0_set_image (void) ;

int mc32x0_set_range(char); 

int mc32x0_get_range(unsigned char*);

int mc32x0_set_mode(unsigned char); 

int mc32x0_get_mode(unsigned char *);

int mc32x0_set_bandwidth(char);

int mc32x0_get_bandwidth(unsigned char *);

int mc32x0_read_accel_x(short *);

int mc32x0_read_accel_y(short *);

int mc32x0_read_accel_z(short *);

int mc32x0_read_accel_xyz(mc32x0acc_t *);

int mc32x0_get_interrupt_status(unsigned char *);

int mc32x0_read_reg(unsigned char , unsigned char *, unsigned char);

int mc32x0_write_reg(unsigned char , unsigned char*, unsigned char );


#endif

