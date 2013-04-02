/*
 * Copyright (C) 2011 MCUBE, Inc.
 *
 * Initial Code:
 *	Tan Liang
 */





#include <linux/delay.h>

#include "mc32x0_driver.h"


mc32x0_t *p_mc32x0;				/**< pointer to MC32X0 device structure  */


/** API Initialization routine
 \param *mc32x0 pointer to MC32X0 structured type
 \return result of communication routines 
 */

int mcube_mc32x0_init(mc32x0_t *mc32x0) 
{
	int comres=0;
	unsigned char data;

	p_mc32x0 = mc32x0;																				/* assign mc32x0 ptr */
	p_mc32x0->dev_addr = MC32X0_I2C_ADDR;															/* preset  I2C_addr */
	comres += p_mc32x0->MC32X0_BUS_READ_FUNC(p_mc32x0->dev_addr, MC32X0_CHIP_ID, &data, 1);			/* read Chip Id */
	
	p_mc32x0->chip_id = data;

	// init other reg
	data = 0x63;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, 0x1b, &data, 1);
	data = 0x43;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, 0x1b, &data, 1);
	msleep(5);

	data = 0x43;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, 0x07, &data, 1);
	data = 0x80;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, 0x1c, &data, 1);
	data = 0x80;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, 0x17, &data, 1);
	msleep(5);
	data = 0x00;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, 0x1c, &data, 1);
	data = 0x00;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, 0x17, &data, 1);

	return comres;
}

int mc32x0_set_image (void) 
{
	int comres;
	unsigned char data;
	if (p_mc32x0==0)
		return E_NULL_PTR;
	
#ifdef MCUBE_2G_10BIT_TAP        
	data = MC32X0_MODE_DEF;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Mode_Feature_REG, &data, 1 );

	data = 0x00;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Sleep_Count_REG, &data, 1 );

	data = 0x00;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Sample_Rate_REG, &data, 1 );	
	
	data = 0x80;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Tap_Detection_Enable_REG, &data, 1 );
	
	data = 0x05;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_TAP_Dwell_Reject_REG, &data, 1 );
	
	data = 0x33;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_RANGE_Control_REG, &data, 1 );
	
	data = 0x07;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_TAP_Threshold_REG, &data, 1 );
	
	data = 0x04;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Interrupt_Enable_REG, &data, 1 );
    	
#endif

#ifdef MCUBE_2G_10BIT
	data = MC32X0_MODE_DEF;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Mode_Feature_REG, &data, 1 );

	data = 0x00;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Sleep_Count_REG, &data, 1 );

	data = 0x00;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Sample_Rate_REG, &data, 1 );	

	data = 0x00;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Tap_Detection_Enable_REG, &data, 1 );

	data = 0x33;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_RANGE_Control_REG, &data, 1 );

	data = 0x00;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Interrupt_Enable_REG, &data, 1 );

#endif

#ifdef MCUBE_8G_14BIT_TAP        
	data = MC32X0_MODE_DEF;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Mode_Feature_REG, &data, 1 );

	data = 0x00;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Sleep_Count_REG, &data, 1 );

	data = 0x00;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Sample_Rate_REG, &data, 1 );	
	
	data = 0x80;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Tap_Detection_Enable_REG, &data, 1 );
	
	data = 0x05;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_TAP_Dwell_Reject_REG, &data, 1 );
	
	data = 0x3F;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_RANGE_Control_REG, &data, 1 );
	
	data = 0x07;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_TAP_Threshold_REG, &data, 1 );
	
	data = 0x04;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Interrupt_Enable_REG, &data, 1 );
    	
#endif

#ifdef MCUBE_8G_14BIT
	data = MC32X0_MODE_DEF;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Mode_Feature_REG, &data, 1 );

	data = 0x00;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Sleep_Count_REG, &data, 1 );

	data = 0x00;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Sample_Rate_REG, &data, 1 );	

	data = 0x00;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Tap_Detection_Enable_REG, &data, 1 );

	data = 0x3F;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_RANGE_Control_REG, &data, 1 );

	data = 0x00;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Interrupt_Enable_REG, &data, 1 );

#endif



#ifdef MCUBE_1_5G_8BIT
	data = MC32X0_MODE_DEF;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Mode_Feature_REG, &data, 1 );

	data = 0x00;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Sleep_Count_REG, &data, 1 );	

	data = 0x00;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Sample_Rate_REG, &data, 1 );

	data = 0x02;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_RANGE_Control_REG, &data, 1 );

	data = 0x00;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Tap_Detection_Enable_REG, &data, 1 );

	data = 0x00;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Interrupt_Enable_REG, &data, 1 );

#endif


#ifdef MCUBE_1_5G_8BIT_TAP
	data = MC32X0_MODE_DEF;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Mode_Feature_REG, &data, 1 );

	data = 0x00;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Sleep_Count_REG, &data, 1 );	

	data = 0x00;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Sample_Rate_REG, &data, 1 );

	data = 0x80;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Tap_Detection_Enable_REG, &data, 1 );	

	data = 0x02;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_RANGE_Control_REG, &data, 1 );

	data = 0x03;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_TAP_Dwell_Reject_REG, &data, 1 );

	data = 0x07;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_TAP_Threshold_REG, &data, 1 );

	data = 0x04;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Interrupt_Enable_REG, &data, 1 );

#endif

	
#ifdef  MCUBE_1_5G_6BIT
	
	data = MC32X0_MODE_DEF;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Mode_Feature_REG, &data, 1 );
	
	data = 0x00;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Sleep_Count_REG, &data, 1 );	
	
	data = 0x00;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Sample_Rate_REG, &data, 1 );
	
	data = 0x00;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_RANGE_Control_REG, &data, 1 );
	
	data = 0x00;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Tap_Detection_Enable_REG, &data, 1 );
	
	data = 0x00;
	comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_Interrupt_Enable_REG, &data, 1 );
	
		
#endif



	return comres;
}


int mc32x0_get_offset(unsigned char *offset) 
{
	return 0;
}	


int mc32x0_set_offset(unsigned char offset) 
{
	return 0;
}




/**	set mc32x0s range 
 \param range 
 
 \see MC32X0_RANGE_2G		
 \see MC32X0_RANGE_4G			
 \see MC32X0_RANGE_8G			
*/
int mc32x0_set_range(char range) 
{			
   int comres = 0;
   unsigned char data;

   if (p_mc32x0==0)
	    return E_NULL_PTR;

   if (range<3) {	
	   comres = p_mc32x0->MC32X0_BUS_READ_FUNC(p_mc32x0->dev_addr, MC32X0_RANGE__REG, &data, 1);
	   data = MC32X0_SET_BITSLICE(data, MC32X0_RANGE, range);
	   comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_RANGE__REG, &data, 1);

   }
   return comres;

}


/* readout select range from MC32X0 
   \param *range pointer to range setting
   \return result of bus communication function
   \see MC32X0_RANGE_2G, MC32X0_RANGE_4G, MC32X0_RANGE_8G		
   \see mc32x0_set_range()
*/
int mc32x0_get_range(unsigned char *range) 
{

	int comres = 0;
	unsigned char data;

	if (p_mc32x0==0)
		return E_NULL_PTR;
	comres = p_mc32x0->MC32X0_BUS_READ_FUNC(p_mc32x0->dev_addr, MC32X0_RANGE__REG, &data, 1);
	data = MC32X0_GET_BITSLICE(data, MC32X0_RANGE);

	*range = data;

	
	return comres;

}



int mc32x0_set_mode(unsigned char mode) {
	
	int comres=0;
	unsigned char data;

	if (p_mc32x0==0)
		return E_NULL_PTR;

	if (mode<4) {
		data  = MC32X0_MODE_DEF;
		data  = MC32X0_SET_BITSLICE(data, MC32X0_MODE, mode);		  
        comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_MODE__REG, &data, 1 );

	  	p_mc32x0->mode = mode;
	} 
	return comres;
	
}



int mc32x0_get_mode(unsigned char *mode) 
{
    if (p_mc32x0==0)
    	return E_NULL_PTR;	
		*mode =  p_mc32x0->mode;
	  return 0;
}


int mc32x0_set_bandwidth(char bw) 
{
	int comres = 0;
	unsigned char data;


	if (p_mc32x0==0)
		return E_NULL_PTR;

	if (bw<7) {

  	  comres = p_mc32x0->MC32X0_BUS_READ_FUNC(p_mc32x0->dev_addr, MC32X0_BANDWIDTH__REG, &data, 1 );
	  data = MC32X0_SET_BITSLICE(data, MC32X0_BANDWIDTH, bw);
	  comres += p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, MC32X0_BANDWIDTH__REG, &data, 1 );

	}

	return comres;


}

int mc32x0_get_bandwidth(unsigned char *bw) {
	int comres = 1;
	if (p_mc32x0==0)
		return E_NULL_PTR;

	comres = p_mc32x0->MC32X0_BUS_READ_FUNC(p_mc32x0->dev_addr, MC32X0_BANDWIDTH__REG, bw, 1 );		

	*bw = MC32X0_GET_BITSLICE(*bw, MC32X0_BANDWIDTH);
	
	return comres;

}


int mc32x0_read_accel_x(short *a_x) 
{
	int comres;
	unsigned char data[2];
	
	
	if (p_mc32x0==0)
		return E_NULL_PTR;

	comres = p_mc32x0->MC32X0_BUS_READ_FUNC(p_mc32x0->dev_addr, MC32X0_XOUT_EX_L_REG, &data[0],2);
	
	*a_x = ((short)data[0])|(((short)data[1])<<8);

	return comres;
	
}



int mc32x0_read_accel_y(short *a_y) 
{
	int comres;
	unsigned char data[2];	


	if (p_mc32x0==0)
		return E_NULL_PTR;

	comres = p_mc32x0->MC32X0_BUS_READ_FUNC(p_mc32x0->dev_addr, MC32X0_YOUT_EX_L_REG, &data[0],2);
	
	*a_y = ((short)data[0])|(((short)data[1])<<8);

	return comres;
}


int mc32x0_read_accel_z(short *a_z)
{
	int comres;
	unsigned char data[2];	

	if (p_mc32x0==0)
		return E_NULL_PTR;

	comres = p_mc32x0->MC32X0_BUS_READ_FUNC(p_mc32x0->dev_addr, MC32X0_ZOUT_EX_L_REG, &data[0],2);
	
	*a_z = ((short)data[0])|(((short)data[1])<<8);

	return comres;
}


int mc32x0_read_accel_xyz(mc32x0acc_t * acc)
{
	int comres;
	unsigned char data[6];


	if (p_mc32x0==0)
		return E_NULL_PTR;
	
#ifdef MC32X0_HIGH_END
	comres = p_mc32x0->MC32X0_BUS_READ_FUNC(p_mc32x0->dev_addr, MC32X0_XOUT_EX_L_REG, &data[0],6);
	
	acc->x = ((signed short)data[0])|(((signed short)data[1])<<8);
	acc->y = ((signed short)data[2])|(((signed short)data[3])<<8);
	acc->z = ((signed short)data[4])|(((signed short)data[5])<<8);
#endif

#ifdef MC32X0_LOW_END
		comres = p_mc32x0->MC32X0_BUS_READ_FUNC(p_mc32x0->dev_addr, MC32X0_XOUT_REG, &data[0],3);
		
#ifndef MCUBE_1_5G_6BIT		
				acc->x = (signed char)data[0];
				acc->y = (signed char)data[1];
				acc->z = (signed char)data[2];
#else 
				acc->x = (signed short)GET_REAL_VALUE(data[0],6);
				acc->y = (signed short)GET_REAL_VALUE(data[1],6);
				acc->z = (signed short)GET_REAL_VALUE(data[2],6);
#endif
		
#endif



	
	return comres;
	
}



int mc32x0_get_interrupt_status(unsigned char * ist) 
{

	int comres=0;	
	if (p_mc32x0==0)
		return E_NULL_PTR;
	comres = p_mc32x0->MC32X0_BUS_READ_FUNC(p_mc32x0->dev_addr, MC32X0_Tilt_Status_REG, ist, 1);
	return comres;
}



int mc32x0_read_reg(unsigned char addr, unsigned char *data, unsigned char len)
{

	int comres;
	if (p_mc32x0==0)
		return E_NULL_PTR;

	comres = p_mc32x0->MC32X0_BUS_READ_FUNC(p_mc32x0->dev_addr, addr, data, len);
	return comres;

}


int mc32x0_write_reg(unsigned char addr, unsigned char *data, unsigned char len) 
{

	int comres;

	if (p_mc32x0==0)
		return E_NULL_PTR;

	comres = p_mc32x0->MC32X0_BUS_WRITE_FUNC(p_mc32x0->dev_addr, addr, data, len);

	return comres;

}

