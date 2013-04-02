/*++ 
 * linux/drivers/media/video/wmt_v4l2/cmos/cmos-dev-gc.c
 * WonderMedia v4l cmos device driver
*
 * Copyright c 2010  WonderMedia  Technologies, Inc.
 *
 * This program is free software: you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 2 of the License, or 
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details. 
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
 * WonderMedia Technologies, Inc.
 * 4F, 533, Chung-Cheng Road, Hsin-Tien, Taipei 231, R.O.C
--*/
#define CMOS_DEV_MODULES_C
#include "cmos-dev-modules.h"
#include "../wmt-vid.h"
#include <linux/module.h>
#include <linux/delay.h>
#include "modules/cmos-modules.h"

int cmos_exit_ext_device(wmt_cmos_product_id cmosid)
{
	if(CMOS_ID_OV_2659==cmosid)
	{
		wmt_vid_i2c_write16addr(0x30, 0x0100, 0x0);
		wmt_vid_i2c_write16addr(0x30, 0x3000, 0x0c);
		wmt_vid_i2c_write16addr(0x30, 0x3001, 0x0);
		wmt_vid_i2c_write16addr(0x30, 0x3002, 0x1f);
	}
	else if(CMOS_ID_OV_3660==cmosid)
	{
		//wmt_vid_i2c_write16addr(0x3c, 0x3008, 0x86);
		//wmt_vid_i2c_write16addr(0x3c, 0x301d, 0x7f);
		//wmt_vid_i2c_write16addr(0x3c, 0x301e, 0xfc);
		wmt_vid_i2c_write16addr(0x3c, 0x3017, 0x80);
		wmt_vid_i2c_write16addr(0x3c, 0x3018, 0x03);
	}
	else if(CMOS_ID_OV_5640==cmosid)
	{
		wmt_vid_i2c_write16addr(0x3c, 0x3017, 0x80);
		wmt_vid_i2c_write16addr(0x3c, 0x3018, 0x03);
	}
	else if(CMOS_ID_GC_2035==cmosid)
	{
			wmt_vid_i2c_write(0x3c, 0xfe, 0x00);
			wmt_vid_i2c_write(0x3c, 0xf1, 0x00);
			wmt_vid_i2c_write(0x3c, 0xf2, 0x00);
	}
	return 0;
}

static void cmos_init_8bit_addr(unsigned char* array_addr, unsigned int array_size, unsigned char i2c_addr)
{
	unsigned int addr,data,i;

	for(i=0;i<array_size;i+=2)
	{
        addr = array_addr[i];
        data = array_addr[i+1];
		wmt_vid_i2c_write(i2c_addr,addr,data);
	}
}

static void cmos_init_16bit_addr(unsigned int * array_addr, unsigned int array_size, unsigned int i2c_addr)
{
	unsigned int addr,data,i;

	for(i=0;i<array_size;i+=2)
	{
        addr = array_addr[i];
        data = array_addr[i+1];
		wmt_vid_i2c_write16addr(i2c_addr,addr,data);
	}
}

static void cmos_init_16bit_addr_16bit_data(unsigned int * array_addr, unsigned int array_size, unsigned int i2c_addr)
{
	unsigned int addr,data,i;

	for(i=0;i<array_size;i+=2)
	{
        addr = array_addr[i];
        data = array_addr[i+1];
		wmt_vid_i2c_write16addr16data(i2c_addr,addr,data);
	}
}

int cmos_init_gc0307(cmos_init_arg_t  *init_arg)
{
	printk("%s %d %d \n", __FUNCTION__, init_arg->width, init_arg->height);
	cmos_init_8bit_addr(gc0307_640x480, sizeof(gc0307_640x480)/sizeof(unsigned char), 0x21);
  
	if ((init_arg->cmos_v_flip == 0) &&(init_arg->cmos_h_flip == 0))
	{
		wmt_vid_i2c_write( 0x21, 0x0f, 0xb2);
		wmt_vid_i2c_write( 0x21, 0x45, 0x27);
		wmt_vid_i2c_write( 0x21, 0x47, 0x2c);
	}else if ((init_arg->cmos_v_flip == 0) &&(init_arg->cmos_h_flip == 1)){
	
		wmt_vid_i2c_write( 0x21, 0x0f, 0xa2);
		wmt_vid_i2c_write( 0x21, 0x45, 0x26);
		wmt_vid_i2c_write( 0x21, 0x47, 0x28);
	}else if ((init_arg->cmos_v_flip == 1) &&(init_arg->cmos_h_flip == 0)){
	
		wmt_vid_i2c_write( 0x21, 0x0f, 0x92);
		wmt_vid_i2c_write( 0x21, 0x45, 0x25);
		wmt_vid_i2c_write( 0x21, 0x47, 0x24);
	}else if ((init_arg->cmos_v_flip == 1) &&(init_arg->cmos_h_flip == 1)){
	
		wmt_vid_i2c_write( 0x21, 0x0f, 0x82);
		wmt_vid_i2c_write( 0x21, 0x45, 0x24);
		wmt_vid_i2c_write( 0x21, 0x47, 0x20);
	}
      return 0;

}

int cmos_init_gc0308(cmos_init_arg_t  *init_arg)
{
	unsigned char data;

	printk("%s %d %d \n", __FUNCTION__, init_arg->width, init_arg->height);

	cmos_init_8bit_addr(gc0308_640x480, sizeof(gc0308_640x480)/sizeof(unsigned char), 0x21);

	if((init_arg->width == 320) && ( init_arg->height == 240))
	{
		cmos_init_8bit_addr(gc0308_320x240, sizeof(gc0308_320x240)/sizeof(unsigned char), 0x21);
	}

	data = wmt_vid_i2c_read(0x21,0x14) ;
	data &= 0xfc;
	data |= 0x10;
	if (init_arg->cmos_v_flip)
		data |= 0x2;
	if (init_arg->cmos_h_flip)
		data |= 0x1;
	wmt_vid_i2c_write( 0x21, 0x14, data);
	return 0;
}

int cmos_init_gc0309(cmos_init_arg_t  *init_arg)
{
	unsigned char data;
	printk("%s %d %d \n", __FUNCTION__, init_arg->width, init_arg->height);
	wmt_vid_i2c_write( 0x21, 0xfe, 0x00);
	wmt_vid_i2c_write( 0x21, 0xfe, 0x80);

	msleep(50);
	cmos_init_8bit_addr(gc0309_640x480, sizeof(gc0309_640x480)/sizeof(unsigned char), 0x21);
	if((init_arg->width == 320) && ( init_arg->height == 240))
	{
		cmos_init_8bit_addr(gc0309_320x240, sizeof(gc0309_320x240)/sizeof(unsigned char), 0x21);
	}

	data = wmt_vid_i2c_read(0x21,0x14) ;
	data &= 0xfc;
	data |= 0x10;
	if (init_arg->cmos_v_flip)
		data |= 0x2;
	if (init_arg->cmos_h_flip)
		data |= 0x1;
	wmt_vid_i2c_write( 0x21, 0x14, data);
	return 0;
}

int cmos_init_gc0311(cmos_init_arg_t  *init_arg)
{
	unsigned char data;
#if 0
	REG32_VAL(0xd81100c0) |= 1<<12;//high , turn off  the CMOS power
	msleep(200);
	REG32_VAL(0xd81100c0) &= ~(1<<12);//high , turn off  the CMOS power
	msleep(200);
	REG32_VAL(0xd81100c0) |= 1<<12;//high , turn off  the CMOS power
	msleep(200);
	REG32_VAL(0xd81100c0) &= ~(1<<12);//high , turn off  the CMOS power
	msleep(200);
	REG32_VAL(0xd81100c0) |= 1<<12;//high , turn off  the CMOS power
	msleep(200);
	REG32_VAL(0xd81100c0) &= ~(1<<12);//high , turn off  the CMOS power
	msleep(200);
#endif
	printk("%s %d %d \n", __FUNCTION__, init_arg->width, init_arg->height);
	wmt_vid_i2c_write( 0x33, 0xfe, 0x80);
	msleep(200);
	cmos_init_8bit_addr(gc0311_640x480, sizeof(gc0311_640x480)/sizeof(unsigned char), 0x33);
	if((init_arg->width == 320) && ( init_arg->height == 240))
	{
		cmos_init_8bit_addr(gc0311_320x240, sizeof(gc0311_320x240)/sizeof(unsigned char), 0x33);
	}
#if 0
	data = wmt_vid_i2c_read(0x33,0x17) ;
	printk(KERN_ALERT " ******************* by flashchen the register of 17 first is %d  \n", data);
	data &= 0xfc;
	if (init_arg->cmos_v_flip)
		data |= 0x2;
	if (init_arg->cmos_h_flip)
		data |= 0x1;
	wmt_vid_i2c_write( 0x33, 0x17, data);
#else
	data=0x14;
	if (init_arg->cmos_v_flip)
		data |= 0x2;
	if (init_arg->cmos_h_flip)
		data |= 0x1;
	wmt_vid_i2c_write( 0x33, 0x17, data);
#endif
	return 0;
}

int cmos_init_gc0329(cmos_init_arg_t  *init_arg)
{
	unsigned char data;
	printk("%s %d %d \n", __FUNCTION__, init_arg->width, init_arg->height);
	cmos_init_8bit_addr(gc0329_640x480, sizeof(gc0329_640x480)/sizeof(unsigned char), 0x31);
	if((init_arg->width == 320) && ( init_arg->height == 240))
	{
		cmos_init_8bit_addr(gc0329_320x240, sizeof(gc0329_320x240)/sizeof(unsigned char), 0x31);
	}

	wmt_vid_i2c_write( 0x31, 0xfe, 0x00);
	wmt_vid_i2c_write( 0x31, 0xfc, 0x16);
	msleep(50);
	data = wmt_vid_i2c_read(0x31,0x17) ;
	msleep(50);
	data &= 0xfc;
	if (init_arg->cmos_v_flip)
		data |= 0x2;
	if (init_arg->cmos_h_flip)
		data |= 0x1;
    wmt_vid_i2c_write( 0x31, 0x17, data);
    return 0;
}

int cmos_init_gc2015(cmos_init_arg_t  *init_arg)
{
	unsigned char data;
	printk("%s %d %d \n", __FUNCTION__, init_arg->width, init_arg->height);
	if(320==init_arg->width && 240==init_arg->height)
	{
		cmos_init_8bit_addr(gc2015_320x240, sizeof(gc2015_320x240)/sizeof(unsigned char), 0x30);
	}
	else if(640==init_arg->width && 480==init_arg->height)
	{
		if(init_arg->mode_switch)
		{
			cmos_init_8bit_addr(gc2015_640x480_key, sizeof(gc2015_640x480_key)/sizeof(unsigned char), 0x30);
			init_arg->mode_switch=0;
		}
		else//init
		{
			cmos_init_8bit_addr(gc2015_640x480, sizeof(gc2015_640x480)/sizeof(unsigned char), 0x30);
		}
	}
#if 0
	else if(800==init_arg->width && 600==init_arg->height)
	{
		cmos_init_8bit_addr(gc2015_800x600, sizeof(gc2015_800x600)/sizeof(unsigned char), 0x30);
	}
#endif
	else if(1600==init_arg->width && 1200==init_arg->height)
	{
		if(init_arg->mode_switch)
			init_arg->mode_switch=0;
		cmos_init_8bit_addr(gc2015_1600x1200_key, sizeof(gc2015_1600x1200_key)/sizeof(unsigned char), 0x30);
	}

	data = wmt_vid_i2c_read(0x30,0x29) ;
	data &= 0xfc;
	if (init_arg->cmos_v_flip)
		data |= 0x2;
	if (init_arg->cmos_h_flip)
		data |= 0x1;
    wmt_vid_i2c_write(0x30, 0x29, data);
    return 0;
}

void gc2035_shutter(void)
{
	unsigned int value;
	unsigned int pid=0,shutter,temp_reg;

	wmt_vid_i2c_write(0x3c, 0xfe, 0x00);
	wmt_vid_i2c_write(0x3c, 0xb6, 0x02);
	value = wmt_vid_i2c_read(0x3c,0x03) ;
	pid |= (value << 8);
	value = wmt_vid_i2c_read(0x3c,0x04) ;
	pid |= (value & 0xff);
	shutter=pid;
	temp_reg= shutter /2;	// 2
	if(temp_reg < 1) temp_reg = 1;
	wmt_vid_i2c_write(0x3c, 0x03,((temp_reg>>8)&0xff));
	wmt_vid_i2c_write(0x3c, 0x04, (temp_reg&0xff));
 
}

int cmos_init_gc2035(cmos_init_arg_t  *init_arg)
{
	unsigned char data;
	printk("%s %d %d \n", __FUNCTION__, init_arg->width, init_arg->height);
	if(320==init_arg->width && 240==init_arg->height)
	{
		msleep(50);
		cmos_init_8bit_addr(gc2035_640x480, sizeof(gc2035_640x480)/sizeof(unsigned char), 0x3c);
		cmos_init_8bit_addr(gc2035_320x240, sizeof(gc2035_320x240)/sizeof(unsigned char), 0x3c);
	}
	else if(176==init_arg->width && 144==init_arg->height)
	{
		msleep(50);
		cmos_init_8bit_addr(gc2035_640x480, sizeof(gc2035_640x480)/sizeof(unsigned char), 0x3c);
		cmos_init_8bit_addr(gc2035_176x144, sizeof(gc2035_176x144)/sizeof(unsigned char), 0x3c);
	}
	else if(1280==init_arg->width && 720==init_arg->height)
	{
		msleep(50);
		cmos_init_8bit_addr(gc2035_640x480, sizeof(gc2035_640x480)/sizeof(unsigned char), 0x3c);
		cmos_init_8bit_addr(gc2035_1280x720, sizeof(gc2035_1280x720)/sizeof(unsigned char), 0x3c);
	}
	else if(640==init_arg->width && 480==init_arg->height)
	{
		if(init_arg->mode_switch)
		{
			cmos_init_8bit_addr(gc2035_640x480_key, sizeof(gc2035_640x480_key)/sizeof(unsigned char), 0x3c);
			msleep(50);
			wmt_vid_i2c_write(0x3c, 0xfe, 0x00);
			wmt_vid_i2c_write(0x3c, 0xb6, 0x03);
			init_arg->mode_switch=0;
		}
		else//init
		{
			msleep(50);
			cmos_init_8bit_addr(gc2035_640x480, sizeof(gc2035_640x480)/sizeof(unsigned char), 0x3c);
		}
	}
	else if(1600==init_arg->width && 1200==init_arg->height)
	{
		if(init_arg->mode_switch)
			init_arg->mode_switch=0;

		gc2035_shutter();
		cmos_init_8bit_addr(gc2035_1600x1200_key, sizeof(gc2035_1600x1200_key)/sizeof(unsigned char), 0x3c);
	}
#if 1
	data = 0x14;
	if (init_arg->cmos_v_flip)
		data |= 0x2;
	if (init_arg->cmos_h_flip)
		data |= 0x1;
    wmt_vid_i2c_write(0x3c, 0x17, data);
#endif
    return 0;
}

int cmos_init_sp0838(cmos_init_arg_t  *init_arg)
{
	unsigned char data;
	printk("%s %d %d \n", __FUNCTION__, init_arg->width, init_arg->height);
	cmos_init_8bit_addr(sp0838_640x480, sizeof(sp0838_640x480)/sizeof(unsigned char), 0x18);
	if(320==init_arg->width && 240==init_arg->height)
	{
		cmos_init_8bit_addr(sp0838_320x240, sizeof(sp0838_320x240)/sizeof(unsigned char), 0x18);
	}

    wmt_vid_i2c_write(0x18,0xfd,0x00);
	data = wmt_vid_i2c_read(0x18,0x31) ;
	data &= 0x9f;
	if (init_arg->cmos_v_flip)
		data |= 0x40;
	if (init_arg->cmos_h_flip)
		data |= 0x20;
    wmt_vid_i2c_write( 0x18, 0x31, data);
    return 0;
}

int cmos_init_sp0a18(cmos_init_arg_t  *init_arg)
{
	unsigned char data;
	printk("%s %d %d \n", __FUNCTION__, init_arg->width, init_arg->height);
	cmos_init_8bit_addr(sp0a18_640x480, sizeof(sp0a18_640x480)/sizeof(unsigned char), 0x21);

	if(320==init_arg->width && 240==init_arg->height)
	{
		cmos_init_8bit_addr(sp0838_320x240, sizeof(sp0838_320x240)/sizeof(unsigned char), 0x21);
	}

    wmt_vid_i2c_write(0x21,0xfd,0x00);
	data = wmt_vid_i2c_read(0x21,0x31) ;
	data &= 0x9f;
	if (init_arg->cmos_v_flip)
		data |= 0x40;
	if (init_arg->cmos_h_flip)
		data |= 0x20;
    wmt_vid_i2c_write( 0x21, 0x31, data);
    return 0;
}

int cmos_init_ov7675(cmos_init_arg_t  *init_arg)
{
	unsigned char data;
	printk("%s %d %d \n", __FUNCTION__, init_arg->width, init_arg->height);
	if( (init_arg->width == 320) && (init_arg->height == 240) ) {
		cmos_init_8bit_addr(ov7675_320x240, sizeof(ov7675_320x240)/sizeof(unsigned char), 0x21);
	}
	else
	{
		cmos_init_8bit_addr(ov7675_640x480, sizeof(ov7675_640x480)/sizeof(unsigned char), 0x21);
	}
	    
	data = 0x0;
	if (init_arg->cmos_v_flip)
		data |= 0x10;
	if (init_arg->cmos_h_flip)
		data |= 0x20;
	wmt_vid_i2c_write( 0x21, 0x1e, data);

	return 0;
} 

int cmos_init_ov2640(cmos_init_arg_t  *init_arg)
{
	printk("%s %d %d \n", __FUNCTION__, init_arg->width, init_arg->height);
    if( (init_arg->width == 800) && (init_arg->height == 600) ) {
		cmos_init_8bit_addr(ov2640_800_600_cfg_0, sizeof(ov2640_800_600_cfg_0)/sizeof(unsigned char), 0x21);
		cmos_init_8bit_addr(ov2640_800_600_cfg_1, sizeof(ov2640_800_600_cfg_1)/sizeof(unsigned char), 0x21);
		cmos_init_8bit_addr(ov2640_800_600_cfg_2, sizeof(ov2640_800_600_cfg_2)/sizeof(unsigned char), 0x21);
	}
    return 0;
} 

int cmos_init_ov2643(cmos_init_arg_t  *init_arg)
{
	printk("%s %d %d \n", __FUNCTION__, init_arg->width, init_arg->height);
	if(1280==init_arg->width && 720==init_arg->height)
    {
		cmos_init_8bit_addr(ov2643_1280x720, sizeof(ov2643_1280x720)/sizeof(unsigned char), 0x30);
	}
    return 0;
} 

int cmos_init_ov2659(cmos_init_arg_t  *init_arg)
{
	unsigned char data;
	printk("%s %d %d \n", __FUNCTION__, init_arg->width, init_arg->height);

    //wmt_vid_i2c_write16addr(0x30, 0x0103, 0x01);
	//msleep(200);

	if(320==init_arg->width && 240==init_arg->height)
	{
		cmos_init_16bit_addr(ov2659_320x240, sizeof(ov2659_320x240)/sizeof(unsigned int), 0x30);
	}
	else if(176==init_arg->width && 144==init_arg->height)
	{
		cmos_init_16bit_addr(ov2659_176x144, sizeof(ov2659_176x144)/sizeof(unsigned int), 0x30);
	}
	else if(640==init_arg->width && 480==init_arg->height)
	{
		if(init_arg->mode_switch)
		{
			//cmos_init_16bit_addr(ov2659_640x480, sizeof(ov2659_640x480)/sizeof(unsigned int), 0x30);
			cmos_init_16bit_addr(ov2659_640x480_key, sizeof(ov2659_640x480_key)/sizeof(unsigned int), 0x30);
			init_arg->mode_switch=0;
		}
		else//init
			cmos_init_16bit_addr(ov2659_640x480, sizeof(ov2659_640x480)/sizeof(unsigned int), 0x30);
	}
	else if(176==init_arg->width && 144==init_arg->height)
	{
		cmos_init_16bit_addr(ov2659_640x480, sizeof(ov2659_640x480)/sizeof(unsigned int), 0x30);
	}
#if 0// not support any more
	else if(800==init_arg->width && 600==init_arg->height)
	{
		cmos_init_16bit_addr(ov2659_800x600, sizeof(ov2659_800x600)/sizeof(unsigned int), 0x30);
	}
	else if(1280==init_arg->width && 720==init_arg->height)
	{
		cmos_init_16bit_addr(ov2659_1280x720, sizeof(ov2659_1280x720)/sizeof(unsigned int), 0x30);
	}
#endif
	else if(1600==init_arg->width && 1200==init_arg->height)
	{
		if(init_arg->mode_switch)
			init_arg->mode_switch=0;
		//cmos_init_16bit_addr(ov2659_640x480, sizeof(ov2659_640x480)/sizeof(unsigned int), 0x30);
		cmos_init_16bit_addr(ov2659_1600x1200_key, sizeof(ov2659_1600x1200_key)/sizeof(unsigned int), 0x30);
	}
	//binding filter
#if 0//the following is wrong, use data in .h file
	data = wmt_vid_i2c_read16addr(0x30,0x3a05) ;
	data &= ~0x80;
    wmt_vid_i2c_write16addr(0x30, 0x3a05, data);
	
	data = wmt_vid_i2c_read16addr(0x30,0x3a00) ;
	data |= 0x20;
    wmt_vid_i2c_write16addr(0x30, 0x3a00, data);
    wmt_vid_i2c_write16addr(0x30, 0x3a08, 0x00);
    //wmt_vid_i2c_write16addr(0x30, 0x3a09, 0x86);
    wmt_vid_i2c_write16addr(0x30, 0x3a09, 0xb8);
    wmt_vid_i2c_write16addr(0x30, 0x3a0e, 0x06);
#endif
	//hsync
	//data = wmt_vid_i2c_read16addr(0x30,0x4708) ;
	//data &= ~0x04;
    //wmt_vid_i2c_write16addr(0x30, 0x4708, data);

	//driver capability
	data = wmt_vid_i2c_read16addr(0x30,0x3011) ;
	data |= 0xc0;
    wmt_vid_i2c_write16addr(0x30, 0x3011, data);

	data = wmt_vid_i2c_read16addr(0x30,0x3820) ;
	data &= 0xf9;
	if (init_arg->cmos_v_flip)
		data |= 0x6;
    wmt_vid_i2c_write16addr(0x30, 0x3820, data);

	data = wmt_vid_i2c_read16addr(0x30,0x3821) ;
	data &= 0xf9;
	if (init_arg->cmos_h_flip)
		data |= 0x6;
    wmt_vid_i2c_write16addr(0x30, 0x3821, data);
    return 0;
}


#if 0
//ov3640 is 16bit addres
int ov3640_autofocus()
{
	wmt_vid_i2c_write16addr(0x3c,0x3f01,0x01);
	wmt_vid_i2c_write16addr(0x3c,0x3f00,0x03);
	return 0;
}
#endif

int cmos_init_ov3640(cmos_init_arg_t  *init_arg)
{
	printk("%s %d %d \n", __FUNCTION__, init_arg->width, init_arg->height);
	if(640==init_arg->width && 480==init_arg->height)
	{
		cmos_init_16bit_addr(ov3640_640x480, sizeof(ov3640_640x480)/sizeof(unsigned int), 0x3c);
	}
#if 0
	else if(800==width && 600==height)
	else if(1600==width && 1200==height)
#endif
    return 0;
}

void ov3660_shutter(void)
{
	uint32_t temp;
	uint16_t temp1;
	uint16_t temp2;
	uint16_t temp3;

	wmt_vid_i2c_write16addr(0x3c,0x3503,0x07);
	wmt_vid_i2c_write16addr(0x3c,0x3a00,0x38);
	//wmt_vid_i2c_write16addr(0x3c,0x3406,0x01);
	
	temp1 = wmt_vid_i2c_read16addr(0x3c,0x3500);
	temp2 = wmt_vid_i2c_read16addr(0x3c,0x3501);
	temp3 = wmt_vid_i2c_read16addr(0x3c,0x3502);

	temp = ((temp1<<12) | (temp2<<4) | (temp3>>4));
	temp = temp*1/2;
	temp = temp*16;

	temp1 = temp&0x00ff;
	temp2 = (temp&0x00ff00)>>8;
	temp3 = (temp&0xff0000)>>16;
	
	wmt_vid_i2c_write16addr(0x3c,0x3500,temp3);
	wmt_vid_i2c_write16addr(0x3c,0x3501,temp2);
	wmt_vid_i2c_write16addr(0x3c,0x3502,temp1);
}
int cmos_init_ov3660(cmos_init_arg_t  *init_arg)
{
	printk("%s %d %d \n", __FUNCTION__, init_arg->width, init_arg->height);

	if(320==init_arg->width && 240==init_arg->height)
	{
		cmos_init_16bit_addr(ov3660_320x240, sizeof(ov3660_320x240)/sizeof(unsigned int), 0x3c);
	}
	else if(176==init_arg->width && 144==init_arg->height)
	{
		cmos_init_16bit_addr(ov3660_176x144, sizeof(ov3660_176x144)/sizeof(unsigned int), 0x3c);
	}
	else if(640==init_arg->width && 480==init_arg->height)
	{
		if(init_arg->mode_switch)
		{
			cmos_init_16bit_addr(ov3660_640x480_key, sizeof(ov3660_640x480_key)/sizeof(unsigned int), 0x3c);
			init_arg->mode_switch=0;
		}
		else//init
		{
			wmt_vid_i2c_write16addr(0x3c,0x3008,0x82);
			msleep(5);
			cmos_init_16bit_addr(ov3660_640x480, sizeof(ov3660_640x480)/sizeof(unsigned int), 0x3c);
		}
	}
	else if(176==init_arg->width && 144==init_arg->height)
	{
			wmt_vid_i2c_write16addr(0x3c,0x3008,0x82);
			msleep(5);
			cmos_init_16bit_addr(ov3660_640x480, sizeof(ov3660_640x480)/sizeof(unsigned int), 0x3c);
	}
#if 0// no longer support
	else if(1024==init_arg->width && 768==init_arg->height)
	{
		cmos_init_16bit_addr(ov3660_1024x768, sizeof(ov3660_1024x768)/sizeof(unsigned int), 0x3c);
	}
#endif
	else if(2048==init_arg->width && 1536==init_arg->height)
	{
		if(init_arg->mode_switch)
			init_arg->mode_switch=0;
		ov3660_shutter();
		//cmos_init_16bit_addr(ov3660_640x480, sizeof(ov3660_640x480)/sizeof(unsigned int), 0x3c);
		cmos_init_16bit_addr(ov3660_2048x1536_key, sizeof(ov3660_2048x1536_key)/sizeof(unsigned int), 0x3c);
	}
		//cmos_init_16bit_addr(ov3660_color, sizeof(ov3660_color)/sizeof(unsigned int), 0x3c);
#if 0
	else if(800==width && 600==height)
	else if(1600==width && 1200==height)
#endif
    return 0;
}

int cmos_init_ov5640(cmos_init_arg_t  *init_arg)
{
	unsigned char data;
	printk("%s %d %d \n", __FUNCTION__, init_arg->width, init_arg->height);

	if(320==init_arg->width && 240==init_arg->height)
	{
		cmos_init_16bit_addr(ov5640_320x240, sizeof(ov5640_320x240)/sizeof(unsigned int), 0x3c);
	}
	else if(640==init_arg->width && 480==init_arg->height)
	{
		if(init_arg->mode_switch)
		{
			cmos_init_16bit_addr(ov5640_640x480_key, sizeof(ov5640_640x480_key)/sizeof(unsigned int), 0x3c);
			init_arg->mode_switch=0;
		}
		else//init
		{
			//wmt_vid_i2c_write16addr(0x3c,0x3008,0x82);
			//msleep(5);
			cmos_init_16bit_addr(ov5640_640x480, sizeof(ov5640_640x480)/sizeof(unsigned int), 0x3c);
			cmos_init_16bit_addr(ov5640_640x480_key, sizeof(ov5640_640x480_key)/sizeof(unsigned int), 0x3c);
		}
	}
	else if(176==init_arg->width && 144==init_arg->height)
	{
			//wmt_vid_i2c_write16addr(0x3c,0x3008,0x82);
			//msleep(5);
			cmos_init_16bit_addr(ov5640_640x480, sizeof(ov5640_640x480)/sizeof(unsigned int), 0x3c);
			cmos_init_16bit_addr(ov5640_640x480_key, sizeof(ov5640_640x480_key)/sizeof(unsigned int), 0x3c);
	}
#if 0// no longer support
	else if(1024==init_arg->width && 768==init_arg->height)
	{
		cmos_init_16bit_addr(ov3660_1024x768, sizeof(ov3660_1024x768)/sizeof(unsigned int), 0x3c);
	}
#endif
	else if((2592==init_arg->width && 1936==init_arg->height))
	{
		if(init_arg->mode_switch)
			init_arg->mode_switch=0;
		//cmos_init_16bit_addr(ov3660_640x480, sizeof(ov3660_640x480)/sizeof(unsigned int), 0x3c);
		cmos_init_16bit_addr(ov5640_2592x1944_key, sizeof(ov5640_2592x1944_key)/sizeof(unsigned int), 0x3c);
	}
		//cmos_init_16bit_addr(ov3660_color, sizeof(ov3660_color)/sizeof(unsigned int), 0x3c);
#if 0
	else if(800==width && 600==height)
	else if(1600==width && 1200==height)
#endif

	data = wmt_vid_i2c_read16addr(0x3c,0x3820) ;
	data &= 0xf9;
	if (init_arg->cmos_v_flip)
		data |= 0x6;
    wmt_vid_i2c_write16addr(0x3c, 0x3820, data);

	data = wmt_vid_i2c_read16addr(0x3c,0x3821) ;
	data &= 0xf9;
	if (init_arg->cmos_h_flip)
		data |= 0x6;
    wmt_vid_i2c_write16addr(0x3c, 0x3821, data);
    
    return 0;
}


int cmos_init_bg0328(cmos_init_arg_t  *init_arg)
{
	unsigned char data;
	printk("%s %d %d \n", __FUNCTION__, init_arg->width, init_arg->height);
	cmos_init_8bit_addr(bg0328_640x480, sizeof(bg0328_640x480)/sizeof(unsigned char), 0x52);

	if((init_arg->width == 320) && ( init_arg->height == 240))
	{
		cmos_init_8bit_addr(bg0328_320x240, sizeof(bg0328_320x240)/sizeof(unsigned char), 0x52);
	}
  
	wmt_vid_i2c_write( 0x52, 0xf0, 0x00);
	data = wmt_vid_i2c_read(0x52,0x21) ;
	data &= 0xfc;
	if (init_arg->cmos_v_flip)
		data |= 0x1;
	if (init_arg->cmos_h_flip)
		data |= 0x2;
	wmt_vid_i2c_write( 0x52, 0x21, data);
	return 0;
}

int cmos_init_hi704(cmos_init_arg_t  *init_arg)
{
	unsigned char data;
	printk("%s %d %d \n", __FUNCTION__, init_arg->width, init_arg->height);

	if(320==init_arg->width && 240==init_arg->height)
	{
		cmos_init_8bit_addr(hi704_320x240, sizeof(hi704_320x240)/sizeof(unsigned char), 0x30);
	}
	else if(176==init_arg->width && 144==init_arg->height)
	{
		cmos_init_8bit_addr(hi704_176x144, sizeof(hi704_176x144)/sizeof(unsigned char), 0x30);
	}
	else if(640==init_arg->width && 480==init_arg->height)
	{
		cmos_init_8bit_addr(hi704_640x480, sizeof(hi704_640x480)/sizeof(unsigned char), 0x30);
	}
	else
	{
		return -1;
	}
#if 1
    wmt_vid_i2c_write(0x30,0x03,0x00);
	data = wmt_vid_i2c_read(0x30,0x11) ;
	data &= 0xfc;
	if (init_arg->cmos_v_flip)
		data |= 0x2;
	if (init_arg->cmos_h_flip)
		data |= 0x1;
	wmt_vid_i2c_write(0x30, 0x11, data);
#endif
	return 0;
}

int cmos_init_yacd511(cmos_init_arg_t  *init_arg)
{
	unsigned char data;
	printk("%s %d %d \n", __FUNCTION__, init_arg->width, init_arg->height);

	if(320==init_arg->width && 240==init_arg->height)
	{
		cmos_init_8bit_addr(yacd511_320x240, sizeof(yacd511_320x240)/sizeof(unsigned char), 0x20);
	}
	else if(640==init_arg->width && 480==init_arg->height)
	{
		if(init_arg->mode_switch)
		{
			cmos_init_8bit_addr(yacd511_640x480_key, sizeof(yacd511_640x480_key)/sizeof(unsigned char), 0x20);
			init_arg->mode_switch=0;
		}
		else//init
		{
			cmos_init_8bit_addr(yacd511_640x480, sizeof(yacd511_640x480)/sizeof(unsigned char), 0x20);
		}
	}
	else if(1600==init_arg->width && 1200==init_arg->height)
	{
		if(init_arg->mode_switch)
			init_arg->mode_switch=0;
		cmos_init_8bit_addr(yacd511_1600x1200_key, sizeof(yacd511_1600x1200_key)/sizeof(unsigned char), 0x20);
	}
	else
	{
		return -1;
	}

    wmt_vid_i2c_write(0x20,0x03,0x00);
	data = wmt_vid_i2c_read(0x20,0x11) ;
	data &= 0xfc;
	if (init_arg->cmos_v_flip)
		data |= 0x2;
	if (init_arg->cmos_h_flip)
		data |= 0x1;
	wmt_vid_i2c_write( 0x20, 0x11, data);
	return 0;
}

// nt99250 is 16bit addres
int cmos_init_nt99250(cmos_init_arg_t  *init_arg)
{
	unsigned char data;
	printk("%s %d %d \n", __FUNCTION__, init_arg->width, init_arg->height);
	if(640==init_arg->width && 480==init_arg->height)
	{
		cmos_init_16bit_addr(nt99250_640x480, sizeof(nt99250_640x480)/sizeof(unsigned int), 0x36);
	}
	else if(800==init_arg->width && 600==init_arg->height)
	{
		cmos_init_16bit_addr(nt99250_800x600, sizeof(nt99250_800x600)/sizeof(unsigned int), 0x36);
	}
	else if(1600==init_arg->width && 1200==init_arg->height)
	{
		cmos_init_16bit_addr(nt99250_1600x1200, sizeof(nt99250_800x600)/sizeof(unsigned int), 0x36);
	}

	data = wmt_vid_i2c_read16addr(0x36,0x3022) ;
	data &= 0xfc;
	if (init_arg->cmos_v_flip)
		data |= 0x1;
	if (init_arg->cmos_h_flip)
		data |= 0x2;
    wmt_vid_i2c_write16addr(0x36, 0x3022, data);
    return 0;
}

int cmos_init_siv121d(cmos_init_arg_t  *init_arg)
{
	unsigned char data;
	printk("%s %d %d \n", __FUNCTION__, init_arg->width, init_arg->height);

	if(320==init_arg->width && 240==init_arg->height)
	{
		cmos_init_8bit_addr(siv121d_320x240, sizeof(siv121d_320x240)/sizeof(unsigned char), 0x33);
	}
	else if(640==init_arg->width && 480==init_arg->height)
	{
			wmt_vid_i2c_write(0x33,0x00, 0x01);
			wmt_vid_i2c_write(0x33,0x03, 0x0a);
			msleep(20);
			wmt_vid_i2c_write(0x33,0x00, 0x01);
			wmt_vid_i2c_write(0x33,0x03, 0x0a);
			msleep(20);

			cmos_init_8bit_addr(siv121d_640x480, sizeof(siv121d_640x480)/sizeof(unsigned char), 0x33);
	}
	else
	{
		return -1;
	}

    wmt_vid_i2c_write(0x33,0x03,0x01);
	data = wmt_vid_i2c_read(0x33,0x04) ;
	data &= 0xfc;
	if (init_arg->cmos_v_flip)
		data |= 0x2;
	if (init_arg->cmos_h_flip)
		data |= 0x1;
	wmt_vid_i2c_write( 0x33, 0x04, data);

	return 0;
}


int cmos_init_sid130b(cmos_init_arg_t  *init_arg)
{
	unsigned char data;
	printk("%s %d %d \n", __FUNCTION__, init_arg->width, init_arg->height);

	if(320==init_arg->width && 240==init_arg->height)
	{
		cmos_init_8bit_addr(sid130b_init, sizeof(sid130b_init)/sizeof(unsigned char), 0x37);
		cmos_init_8bit_addr(sid130b_320x240, sizeof(sid130b_320x240)/sizeof(unsigned char), 0x37);
	}
	else if(640==init_arg->width && 480==init_arg->height)
	{
		if(init_arg->mode_switch)
		{
			cmos_init_8bit_addr(sid130b_640x480_key, sizeof(sid130b_640x480_key)/sizeof(unsigned char), 0x37);
			init_arg->mode_switch=0;
		}
		else//init
		{
			cmos_init_8bit_addr(sid130b_init, sizeof(sid130b_init)/sizeof(unsigned char), 0x37);
			cmos_init_8bit_addr(sid130b_640x480_key, sizeof(sid130b_640x480_key)/sizeof(unsigned char), 0x37);
			//cmos_init_8bit_addr(sid130b_640x480, sizeof(sid130b_640x480)/sizeof(unsigned char), 0x37);
		}
	}
	else if(1600==init_arg->width && 1200==init_arg->height)
	{
		if(init_arg->mode_switch)
			init_arg->mode_switch=0;
		cmos_init_8bit_addr(sid130b_1600x1200_key, sizeof(sid130b_1600x1200_key)/sizeof(unsigned char), 0x37);
	}
	else
	{
		return -1;
	}

    wmt_vid_i2c_write(0x37,0x00,0x00);
	data = wmt_vid_i2c_read(0x37,0x04) ;
	data &= 0xfc;
	if (init_arg->cmos_v_flip)
		data |= 0x2;
	if (init_arg->cmos_h_flip)
		data |= 0x1;
	wmt_vid_i2c_write( 0x37, 0x04, data);
	return 0;
}

void init_samsungs5k5ca(cmos_init_arg_t  *init_arg)
{

	unsigned char data;
		{
			cmos_init_16bit_addr_16bit_data(samsungs5k5ca_init0, sizeof(samsungs5k5ca_init0)/sizeof(unsigned int), 0x3c);
			msleep(100);
			cmos_init_16bit_addr_16bit_data(samsungs5k5ca_init1, sizeof(samsungs5k5ca_init1)/sizeof(unsigned int), 0x3c);
			msleep(100);
			cmos_init_16bit_addr_16bit_data(samsungs5k5ca_init2, sizeof(samsungs5k5ca_init2)/sizeof(unsigned int), 0x3c);
			
#if 1
	msleep(100);
	wmt_vid_i2c_write16addr16data(0x3c,0xfcfc,0xd000);
	wmt_vid_i2c_write16addr16data(0x3c,0x0028,0x7000);
	wmt_vid_i2c_write16addr16data(0x3c,0x002a,0x0296);
	data = 0;
	if (init_arg->cmos_v_flip)
		data |= 0x2;
	if (init_arg->cmos_h_flip)
		data |= 0x1;
	wmt_vid_i2c_write16addr16data(0x3c,0x0f12,data);
	wmt_vid_i2c_write16addr16data(0x3c,0x0f12,data);

	wmt_vid_i2c_write16addr16data(0x3c,0xfcfc,0xd000);
	wmt_vid_i2c_write16addr16data(0x3c,0x0028,0x7000);
	wmt_vid_i2c_write16addr16data(0x3c,0x002a,0x02c6);
	data = 0;
	if (init_arg->cmos_v_flip)
		data |= 0x2;
	if (init_arg->cmos_h_flip)
		data |= 0x1;
	wmt_vid_i2c_write16addr16data(0x3c,0x0f12,data);
	wmt_vid_i2c_write16addr16data(0x3c,0x0f12,data);

	wmt_vid_i2c_write16addr16data(0x3c,0xfcfc,0xd000);
	wmt_vid_i2c_write16addr16data(0x3c,0x0028,0x7000);
	wmt_vid_i2c_write16addr16data(0x3c,0x002a,0x02f6);
	data = 0;
	if (init_arg->cmos_v_flip)
		data |= 0x2;
	if (init_arg->cmos_h_flip)
		data |= 0x1;
	wmt_vid_i2c_write16addr16data(0x3c,0x0f12,data);
	wmt_vid_i2c_write16addr16data(0x3c,0x0f12,data);


	wmt_vid_i2c_write16addr16data(0x3c,0xfcfc,0xd000);
	wmt_vid_i2c_write16addr16data(0x3c,0x0028,0x7000);
	wmt_vid_i2c_write16addr16data(0x3c,0x002a,0x0326);
	data = 0;
	if (init_arg->cmos_v_flip)
		data |= 0x2;
	if (init_arg->cmos_h_flip)
		data |= 0x1;
	wmt_vid_i2c_write16addr16data(0x3c,0x0f12,data);
	wmt_vid_i2c_write16addr16data(0x3c,0x0f12,data);


#endif

			msleep(100);
			cmos_init_16bit_addr_16bit_data(samsungs5k5ca_init3, sizeof(samsungs5k5ca_init3)/sizeof(unsigned int), 0x3c);
			msleep(100);
			cmos_init_16bit_addr_16bit_data(samsungs5k5ca_init4, sizeof(samsungs5k5ca_init4)/sizeof(unsigned int), 0x3c);
		}
}

int cmos_init_samsungs5k5ca(cmos_init_arg_t  *init_arg)
{
	unsigned char data;
	printk("%s %d %d \n", __FUNCTION__, init_arg->width, init_arg->height);
	if(320==init_arg->width && 240==init_arg->height)
	{
		init_samsungs5k5ca(init_arg);
		cmos_init_16bit_addr_16bit_data(samsungs5k5ca_320x240, sizeof(samsungs5k5ca_320x240)/sizeof(unsigned int), 0x3c);
	}
	else if(176==init_arg->width && 144==init_arg->height)
	{
		init_samsungs5k5ca(init_arg);
		cmos_init_16bit_addr_16bit_data(samsungs5k5ca_176x144, sizeof(samsungs5k5ca_176x144)/sizeof(unsigned int), 0x3c);
	}
	else if(640==init_arg->width && 480==init_arg->height)
	{
		if(init_arg->mode_switch)
		{
			cmos_init_16bit_addr_16bit_data(samsungs5k5ca_640x480_key, sizeof(samsungs5k5ca_640x480_key)/sizeof(unsigned int), 0x3c);
			init_arg->mode_switch=0;
		}
		else
		{
			init_samsungs5k5ca(init_arg);
		}
	}
	else if(2048==init_arg->width && 1536==init_arg->height)
	{
		if(init_arg->mode_switch)
			init_arg->mode_switch=0;
		//cmos_init_16bit_addr_16bit_data(samsungs5k5ca_2048x1536_key, sizeof(samsungs5k5ca_2048x1536_key)/sizeof(unsigned int), 0x3c);
		cmos_init_16bit_addr_16bit_data(samsungs5k5ca_2048x1536_key_night, sizeof(samsungs5k5ca_2048x1536_key_night)/sizeof(unsigned int), 0x3c);
	}
	else
	{
		return -1;
	}

#if 0
	msleep(100);
	wmt_vid_i2c_write16addr16data(0x3c,0xfcfc,0xd000);
	wmt_vid_i2c_write16addr16data(0x3c,0x0028,0x7000);
	wmt_vid_i2c_write16addr16data(0x3c,0x002a,0x0296);
	data = 0;
	if (init_arg->cmos_v_flip)
		data |= 0x2;
	if (init_arg->cmos_h_flip)
		data |= 0x1;
	wmt_vid_i2c_write16addr16data(0x3c,0x0f12,data);
	wmt_vid_i2c_write16addr16data(0x3c,0x0f12,data);
#endif
	return 0;
}

static int do_cmos_init_ext_device(cmos_init_arg_t  *init_arg, wmt_cmos_product_id cmosid)
{
	unsigned int data=0x0;
#if 0
	int i;
	for(i=0;i<0x80;i++)
	{
		data = wmt_vid_i2c_read(i,0x0) ;
		printk(KERN_ALERT "********** by flashchen iic test ********* 00 reg of addr %x is %x \n",i,data);
	}
#endif
	
	while(1)
	{
		switch(cmosid)
		{
		case CMOS_ID_NONE:
		case CMOS_ID_GC_0307:
			data = wmt_vid_i2c_read(0x21,0) ;
			if(data==0x99)
			{
				cmos_init_gc0307(init_arg);
				return CMOS_ID_GC_0307;
			}

		case CMOS_ID_GC_0308:
			data = wmt_vid_i2c_read(0x21,0) ;
			if(data ==0x9b)
			{
				cmos_init_gc0308(init_arg);
				return CMOS_ID_GC_0308;
			}

		case CMOS_ID_GC_0309:
			data = wmt_vid_i2c_read(0x21,0) ;
			if(data ==0xa0)
			{
				cmos_init_gc0309(init_arg);
				return CMOS_ID_GC_0309;
			}

		case CMOS_ID_GC_0311:
			data = wmt_vid_i2c_read(0x33,0xf0) ;
			if(data ==0xbb)
			{
				cmos_init_gc0311(init_arg);
				return CMOS_ID_GC_0311;
			}

		case CMOS_ID_GC_0329:
			wmt_vid_i2c_write( 0x31, 0xfe, 0x00);
			wmt_vid_i2c_write( 0x31, 0xfc, 0x16);
			data = wmt_vid_i2c_read(0x31,0) ;
			if(0xc0==data)
			{
				cmos_init_gc0329(init_arg);
				return CMOS_ID_GC_0329;
			}

		case CMOS_ID_GC_2015:
			data = wmt_vid_i2c_read(0x30,0) ;
			data <<=8;
			data += wmt_vid_i2c_read(0x30,1) ;
			if(0x2005==data)
			{
				cmos_init_gc2015(init_arg);
				return CMOS_ID_GC_2015;
			}

		case CMOS_ID_GC_2035:
			data = wmt_vid_i2c_read(0x3c,0xf0) ;
			data <<=8;
			data += wmt_vid_i2c_read(0x3c,0xf1) ;
			if(0x2035==data)
			{
				cmos_init_gc2035(init_arg);
				return CMOS_ID_GC_2035;
			}

		case CMOS_ID_SP_0838:
			wmt_vid_i2c_write(0x18,0xfd,0x0) ;
			data = wmt_vid_i2c_read(0x18,2) ;
			if(0x027==data)
			{
				cmos_init_sp0838(init_arg);
				return CMOS_ID_SP_0838;
			}

		case CMOS_ID_SP_0A18:
			wmt_vid_i2c_write(0x21,0xfd,0x0) ;
			data = wmt_vid_i2c_read(0x21,2) ;
			if(0x0a==data)
			{
				cmos_init_sp0a18(init_arg);
				return CMOS_ID_SP_0A18;
			}

		case CMOS_ID_OV_7675:
			data = wmt_vid_i2c_read(0x21 ,0xa) ;
			if (data==0x76){
				cmos_init_ov7675(init_arg);
				return CMOS_ID_OV_7675;
			}

		case CMOS_ID_OV_2640:
			wmt_vid_i2c_write(0x21,0xff,1 );
			data = wmt_vid_i2c_read(0x21 ,0xa) ;
			if (data==0x26){
				cmos_init_ov2640(init_arg);
				return CMOS_ID_OV_2640;
			}

		case CMOS_ID_OV_2643:
			wmt_vid_i2c_write(0x30,0xff,1 );
			data = wmt_vid_i2c_read(0x30 ,0xa) ;
			data <<=8;
			data += wmt_vid_i2c_read(0x30 ,0xb) ;
			if (data==0x2643){
				cmos_init_ov2643(init_arg);
				return CMOS_ID_OV_2643;
			}

		case CMOS_ID_OV_2659:
			data=wmt_vid_i2c_read16addr(0x30,0x300a);
			data <<=8;
			data+=wmt_vid_i2c_read16addr(0x30,0x300b);
			if(0x2656==data)
			{
				cmos_init_ov2659(init_arg);
				return CMOS_ID_OV_2659;
			}

		case CMOS_ID_OV_3640:
			data=wmt_vid_i2c_read16addr(0x3c,0x300a);
			data <<=8;
			data+=wmt_vid_i2c_read16addr(0x3c,0x300b);
			if(0x364c==data)
			{
				cmos_init_ov3640(init_arg);
				return CMOS_ID_OV_3640;
			}

		case CMOS_ID_OV_3660:
			data=wmt_vid_i2c_read16addr(0x3c,0x300a);
			data <<=8;
			data+=wmt_vid_i2c_read16addr(0x3c,0x300b);
			if(0x3660==data)
			{
				cmos_init_ov3660(init_arg);
				return CMOS_ID_OV_3660;
			}

		case CMOS_ID_OV_5640:
			data=wmt_vid_i2c_read16addr(0x3c,0x300a);
			data <<=8;
			data+=wmt_vid_i2c_read16addr(0x3c,0x300b);
			if(0x5640==data)
			{
				cmos_init_ov5640(init_arg);
				return CMOS_ID_OV_5640;
			}

		case CMOS_ID_BG_0328:
			data = wmt_vid_i2c_read(0x52,0) ;
			data <<=8;
			data += wmt_vid_i2c_read(0x52,1) ;
			if(0x0328==data)
			{
				cmos_init_bg0328(init_arg);
				return CMOS_ID_BG_0328;
			}

		case CMOS_ID_HI_704:
			wmt_vid_i2c_write(0x30,0x03,0x0) ;
			data = wmt_vid_i2c_read(0x30,0x04) ;
			if(0x96==data)
			{
				cmos_init_hi704(init_arg);
				return CMOS_ID_HI_704;
			}

		case CMOS_ID_YACD_511:
			wmt_vid_i2c_write(0x20,0x03,0x0) ;
			data = wmt_vid_i2c_read(0x20,0x04) ;
			if(0x92==data)
			{
				cmos_init_yacd511(init_arg);
				return CMOS_ID_YACD_511;
			}

		case CMOS_ID_NT_99250:
			data=wmt_vid_i2c_read16addr(0x36,0x3000);
			data <<=8;
			data+=wmt_vid_i2c_read16addr(0x36,0x3001);
			if(0x2501==data)
			{
				cmos_init_nt99250(init_arg);
				return CMOS_ID_NT_99250;
			}

		case CMOS_ID_SIV_121D:
			wmt_vid_i2c_write(0x33,0x00,0x0) ;
			data = wmt_vid_i2c_read(0x33,0x01) ;
			if(0xde==data)
			{
				cmos_init_siv121d(init_arg);
				return CMOS_ID_SIV_121D;
			}

		case CMOS_ID_SID_130B:
			wmt_vid_i2c_write(0x37,0x00,0x0) ;
			data = wmt_vid_i2c_read(0x37,0x01) ;
			if(0x1b==data)
			{
				cmos_init_sid130b(init_arg);
				return CMOS_ID_SID_130B;
			}
		case CMOS_ID_SAMSUNG_S5K5CA:
			wmt_vid_i2c_write16addr16data(0x3c,0xfcfc,0x0000);
			data = wmt_vid_i2c_read16addr16data(0x3c,0x0040);
			if(0x05ca==data)
			{
				cmos_init_samsungs5k5ca(init_arg);
				return CMOS_ID_SAMSUNG_S5K5CA;
			}

		}

		if(CMOS_ID_NONE!=cmosid)
			cmosid=CMOS_ID_NONE;
		else
			break;
	}

	printk("don't find any cmos device init cmos device NG\n");
	return CMOS_ID_NONE;//-1
	
}


int cmos_init_ext_device(cmos_init_arg_t  *init_arg, wmt_cmos_product_id cmosid)
{
	int ret;
	wmt_vid_i2c_xfer_dbg(0);		//diable wmt_vid_i2c_xfer] fail msg
	ret = do_cmos_init_ext_device(init_arg, cmosid);
	wmt_vid_i2c_xfer_dbg(1);		//restore wmt_vid_i2c_xfer] msg.
	return ret;
}

int updateSceneMode(wmt_cmos_product_id id,int value)
{
	switch(id)
	{
		case CMOS_ID_OV_3660:
			switch(value)
			{
				case 0://auto
					wmt_vid_i2c_write16addr(0x3c, 0x3a00, 0x3c);
					wmt_vid_i2c_write16addr(0x3c, 0x3a02, 0x04);
					wmt_vid_i2c_write16addr(0x3c, 0x3a03, 0x98);
					wmt_vid_i2c_write16addr(0x3c, 0x3a14, 0x04);
					wmt_vid_i2c_write16addr(0x3c, 0x3a15, 0x98);
					break;
				case 1://night node
					wmt_vid_i2c_write16addr(0x3c, 0x3a00, 0x3c);
					wmt_vid_i2c_write16addr(0x3c, 0x3a02, 0x12);
					wmt_vid_i2c_write16addr(0x3c, 0x3a03, 0x60);
					wmt_vid_i2c_write16addr(0x3c, 0x3a14, 0x12);
					wmt_vid_i2c_write16addr(0x3c, 0x3a15, 0x60);
					break;
			}
			break;

		case CMOS_ID_OV_2659:
			switch(value)
			{
				case 0://auto
					wmt_vid_i2c_write16addr(0x30, 0x3a00, 0x3c);
					wmt_vid_i2c_write16addr(0x30, 0x3a02, 0x03);
					wmt_vid_i2c_write16addr(0x30, 0x3a03, 0x9c);
					wmt_vid_i2c_write16addr(0x30, 0x3a14, 0x03);
					wmt_vid_i2c_write16addr(0x30, 0x3a15, 0x9c);
					break;
				case 1://night node
					wmt_vid_i2c_write16addr(0x30, 0x3a00, 0x3c);
					wmt_vid_i2c_write16addr(0x30, 0x3a02, 0x0e);
					wmt_vid_i2c_write16addr(0x30, 0x3a03, 0x70);
					wmt_vid_i2c_write16addr(0x30, 0x3a14, 0x0e);
					wmt_vid_i2c_write16addr(0x30, 0x3a15, 0x70);
					break;
			}
			break;

		case CMOS_ID_SIV_121D:
			switch(value)
			{
				case 0://auto
					wmt_vid_i2c_write(0x33,0x00,0x02) ;
					wmt_vid_i2c_write(0x33,0x40,0x40) ;
					break;
				case 1://night node
					wmt_vid_i2c_write(0x33,0x00,0x02) ;
					wmt_vid_i2c_write(0x33,0x40,0x50) ;
					break;
			}
			break;

		case CMOS_ID_SID_130B:
			switch(value)
			{
				case 0://auto
					wmt_vid_i2c_write(0x37,0x00,0x01) ;
					wmt_vid_i2c_write(0x37,0x11,0x0a) ;
					wmt_vid_i2c_write(0x37,0x00,0x04) ;
					wmt_vid_i2c_write(0x37,0xb6,0x00) ;
					wmt_vid_i2c_write(0x37,0xff,0xff) ;
					break;
				case 1://night node
					wmt_vid_i2c_write(0x37,0x00,0x01) ;
					wmt_vid_i2c_write(0x37,0x11,0x14) ;
					wmt_vid_i2c_write(0x37,0x00,0x04) ;
					wmt_vid_i2c_write(0x37,0xb6,0x10) ;
					wmt_vid_i2c_write(0x37,0xff,0xff) ;
					break;
			}
			break;

		case CMOS_ID_HI_704:
			switch(value)
			{
				case 0://auto
wmt_vid_i2c_write(0x30,0x03, 0x20);
wmt_vid_i2c_write(0x30,0x03, 0x20);
wmt_vid_i2c_write(0x30,0x83, 0x00);
wmt_vid_i2c_write(0x30,0x84, 0xaf);
wmt_vid_i2c_write(0x30,0x85, 0x1a);
wmt_vid_i2c_write(0x30,0x86, 0x00);
wmt_vid_i2c_write(0x30,0x87, 0xf1);
wmt_vid_i2c_write(0x30,0x88, 0x02);
wmt_vid_i2c_write(0x30,0x89, 0x47);
wmt_vid_i2c_write(0x30,0x8a, 0xac);
wmt_vid_i2c_write(0x30,0x8B, 0x3a);
wmt_vid_i2c_write(0x30,0x8C, 0x5e);
wmt_vid_i2c_write(0x30,0x8D, 0x30);
wmt_vid_i2c_write(0x30,0x8E, 0x7b);
wmt_vid_i2c_write(0x30,0x9c, 0x06);
wmt_vid_i2c_write(0x30,0x9d, 0x97);
wmt_vid_i2c_write(0x30,0x9e, 0x00);
wmt_vid_i2c_write(0x30,0x9f, 0xf1);
					break;
				case 1://night node
wmt_vid_i2c_write(0x30,0x03, 0x20);
wmt_vid_i2c_write(0x30,0x83, 0x00);
wmt_vid_i2c_write(0x30,0x84, 0xaf);
wmt_vid_i2c_write(0x30,0x85, 0x1a);
wmt_vid_i2c_write(0x30,0x86, 0x00);
wmt_vid_i2c_write(0x30,0x87, 0xf1);
wmt_vid_i2c_write(0x30,0x88, 0x03);
wmt_vid_i2c_write(0x30,0x89, 0xa5);
wmt_vid_i2c_write(0x30,0x8a, 0xe0);
wmt_vid_i2c_write(0x30,0x8B, 0x3a);
wmt_vid_i2c_write(0x30,0x8C, 0x5e);
wmt_vid_i2c_write(0x30,0x8D, 0x30);
wmt_vid_i2c_write(0x30,0x8E, 0x7b);
wmt_vid_i2c_write(0x30,0x9c, 0x06);
wmt_vid_i2c_write(0x30,0x9d, 0x97);
wmt_vid_i2c_write(0x30,0x9e, 0x00);
wmt_vid_i2c_write(0x30,0x9f, 0xf1);
					break;
			}
			break;


		case CMOS_ID_YACD_511:
			switch(value)
			{
				case 0://auto
					wmt_vid_i2c_write(0x20,0x03,0x20) ;
					wmt_vid_i2c_write(0x20,0x10,0x1c) ;
					wmt_vid_i2c_write(0x20,0x18,0x38) ;
					wmt_vid_i2c_write(0x20,0x88,0x05) ;
					wmt_vid_i2c_write(0x20,0x89,0xb8) ;
					wmt_vid_i2c_write(0x20,0x8a,0xd8) ;
					wmt_vid_i2c_write(0x20,0x10,0x9c) ;
					wmt_vid_i2c_write(0x20,0x18,0x30) ;
					wmt_vid_i2c_write(0x20,0xff,0xff) ;
					break;
				case 1://night node
					wmt_vid_i2c_write(0x20,0x03,0x20) ;
					wmt_vid_i2c_write(0x20,0x10,0x1c) ;
					wmt_vid_i2c_write(0x20,0x18,0x38) ;
					wmt_vid_i2c_write(0x20,0x88,0x08) ;
					wmt_vid_i2c_write(0x20,0x89,0x02) ;
					wmt_vid_i2c_write(0x20,0x8a,0xc8) ;
					wmt_vid_i2c_write(0x20,0x10,0x9c) ;
					wmt_vid_i2c_write(0x20,0x18,0x30) ;
					wmt_vid_i2c_write(0x20,0xff,0xff) ;
					break;
			}
			break;

		case CMOS_ID_GC_0308:
			switch(value)
			{
				case 0://auto
					wmt_vid_i2c_write( 0x21, 0xec, 0x20);
					break;
				case 1://night node
					wmt_vid_i2c_write( 0x21, 0xec, 0x30);
					break;
			}
			break;

		case CMOS_ID_GC_2035:
			switch(value)
			{
				case 0://auto
					wmt_vid_i2c_write(0x3c, 0xfe, 0x01);
					wmt_vid_i2c_write(0x3c, 0x3e, 0x40);
					wmt_vid_i2c_write(0x3c, 0xfe, 0x00);
					break;
				case 1://night node
					wmt_vid_i2c_write(0x3c, 0xfe, 0x01);
					wmt_vid_i2c_write(0x3c, 0x3e, 0x60);
					wmt_vid_i2c_write(0x3c, 0xfe, 0x00);
					break;
			}
			break;

		case CMOS_ID_SAMSUNG_S5K5CA:
			switch(value)
			{
				case 0://auto
			cmos_init_16bit_addr_16bit_data(samsungs5k5ca_640x480_key, sizeof(samsungs5k5ca_640x480_key)/sizeof(unsigned int), 0x3c);
					break;
				case 1://night node
			cmos_init_16bit_addr_16bit_data(samsungs5k5ca_night, sizeof(samsungs5k5ca_night)/sizeof(unsigned int), 0x3c);
					break;
			}
			break;

	}
	return 0;
}

int updateWhiteBalance(wmt_cmos_product_id id,int value)
{
	switch(id)
	{
		case CMOS_ID_OV_3660:
			switch(value)
			{
				case 0://auto
					wmt_vid_i2c_write16addr(0x3c, 0x3212, 0x03);
					wmt_vid_i2c_write16addr(0x3c, 0x3406, 0x00);
					wmt_vid_i2c_write16addr(0x3c, 0x3212, 0x13);
					wmt_vid_i2c_write16addr(0x3c, 0x3212, 0xa3);
					break;
				case 1://INCANDESCENT
					wmt_vid_i2c_write16addr(0x3c, 0x3212, 0x03);
					wmt_vid_i2c_write16addr(0x3c, 0x3406, 0x01);
					wmt_vid_i2c_write16addr(0x3c, 0x3400, 0x04);
					wmt_vid_i2c_write16addr(0x3c, 0x3401, 0x05);
					wmt_vid_i2c_write16addr(0x3c, 0x3402, 0x04);
					wmt_vid_i2c_write16addr(0x3c, 0x3403, 0x00);
					wmt_vid_i2c_write16addr(0x3c, 0x3404, 0x08);
					wmt_vid_i2c_write16addr(0x3c, 0x3405, 0x4d);
					wmt_vid_i2c_write16addr(0x3c, 0x3212, 0x13);
					wmt_vid_i2c_write16addr(0x3c, 0x3212, 0xa3);
					break;
				case 2://FLUORESCENT
					wmt_vid_i2c_write16addr(0x3c, 0x3212, 0x03);
					wmt_vid_i2c_write16addr(0x3c, 0x3406, 0x01);
					wmt_vid_i2c_write16addr(0x3c, 0x3400, 0x06);
					wmt_vid_i2c_write16addr(0x3c, 0x3401, 0x57);
					wmt_vid_i2c_write16addr(0x3c, 0x3402, 0x04);
					wmt_vid_i2c_write16addr(0x3c, 0x3403, 0x00);
					wmt_vid_i2c_write16addr(0x3c, 0x3404, 0x07);
					wmt_vid_i2c_write16addr(0x3c, 0x3405, 0x6f);
					wmt_vid_i2c_write16addr(0x3c, 0x3212, 0x13);
					wmt_vid_i2c_write16addr(0x3c, 0x3212, 0xa3);
					break;
				case 3://DAYLIGHT
					wmt_vid_i2c_write16addr(0x3c, 0x3212, 0x03);
					wmt_vid_i2c_write16addr(0x3c, 0x3406, 0x01);
					wmt_vid_i2c_write16addr(0x3c, 0x3400, 0x07);
					wmt_vid_i2c_write16addr(0x3c, 0x3401, 0x29);
					wmt_vid_i2c_write16addr(0x3c, 0x3402, 0x04);
					wmt_vid_i2c_write16addr(0x3c, 0x3403, 0x00);
					wmt_vid_i2c_write16addr(0x3c, 0x3404, 0x04);
					wmt_vid_i2c_write16addr(0x3c, 0x3405, 0x5d);
					wmt_vid_i2c_write16addr(0x3c, 0x3212, 0x13);
					wmt_vid_i2c_write16addr(0x3c, 0x3212, 0xa3);
					break;
				case 4://CLOUDY
					wmt_vid_i2c_write16addr(0x3c, 0x3212, 0x03);
					wmt_vid_i2c_write16addr(0x3c, 0x3406, 0x01);
					wmt_vid_i2c_write16addr(0x3c, 0x3400, 0x07);
					wmt_vid_i2c_write16addr(0x3c, 0x3401, 0xff);
					wmt_vid_i2c_write16addr(0x3c, 0x3402, 0x04);
					wmt_vid_i2c_write16addr(0x3c, 0x3403, 0x00);
					wmt_vid_i2c_write16addr(0x3c, 0x3404, 0x04);
					wmt_vid_i2c_write16addr(0x3c, 0x3405, 0x00);
					wmt_vid_i2c_write16addr(0x3c, 0x3212, 0x13);
					wmt_vid_i2c_write16addr(0x3c, 0x3212, 0xa3);
					break;
			}
			break;

		case CMOS_ID_OV_2659:
			switch(value)
			{
				case 0://auto
					wmt_vid_i2c_write16addr(0x30, 0x3406, 0x00);
					break;
				case 1://INCANDESCENT
					wmt_vid_i2c_write16addr(0x30, 0x3208, 0x00);
					wmt_vid_i2c_write16addr(0x30, 0x3406, 0x01);
					wmt_vid_i2c_write16addr(0x30, 0x3400, 0x04);
					wmt_vid_i2c_write16addr(0x30, 0x3401, 0xe0);
					wmt_vid_i2c_write16addr(0x30, 0x3402, 0x04);
					wmt_vid_i2c_write16addr(0x30, 0x3403, 0x00);
					wmt_vid_i2c_write16addr(0x30, 0x3404, 0x05);
					wmt_vid_i2c_write16addr(0x30, 0x3405, 0xa0);
					wmt_vid_i2c_write16addr(0x30, 0x3208, 0x10);
					wmt_vid_i2c_write16addr(0x30, 0x3208, 0xa0);
					break;
				case 2://FLUORESCENT
					wmt_vid_i2c_write16addr(0x30, 0x3208, 0x00);
					wmt_vid_i2c_write16addr(0x30, 0x3406, 0x01);
					wmt_vid_i2c_write16addr(0x30, 0x3400, 0x04);
					wmt_vid_i2c_write16addr(0x30, 0x3401, 0x00);
					wmt_vid_i2c_write16addr(0x30, 0x3402, 0x04);
					wmt_vid_i2c_write16addr(0x30, 0x3403, 0x00);
					wmt_vid_i2c_write16addr(0x30, 0x3404, 0x06);
					wmt_vid_i2c_write16addr(0x30, 0x3405, 0x50);
					wmt_vid_i2c_write16addr(0x30, 0x3208, 0x10);
					wmt_vid_i2c_write16addr(0x30, 0x3208, 0xa0);
					break;
				case 3://DAYLIGHT
					wmt_vid_i2c_write16addr(0x30, 0x3208, 0x00);
					wmt_vid_i2c_write16addr(0x30, 0x3406, 0x01);
					wmt_vid_i2c_write16addr(0x30, 0x3400, 0x06);
					wmt_vid_i2c_write16addr(0x30, 0x3401, 0x10);
					wmt_vid_i2c_write16addr(0x30, 0x3402, 0x04);
					wmt_vid_i2c_write16addr(0x30, 0x3403, 0x00);
					wmt_vid_i2c_write16addr(0x30, 0x3404, 0x04);
					wmt_vid_i2c_write16addr(0x30, 0x3405, 0x48);
					wmt_vid_i2c_write16addr(0x30, 0x3208, 0x10);
					wmt_vid_i2c_write16addr(0x30, 0x3208, 0xa0);
					break;
				case 4://CLOUDY
					wmt_vid_i2c_write16addr(0x30, 0x3208, 0x00);
					wmt_vid_i2c_write16addr(0x30, 0x3406, 0x01);
					wmt_vid_i2c_write16addr(0x30, 0x3400, 0x06);
					wmt_vid_i2c_write16addr(0x30, 0x3401, 0x30);
					wmt_vid_i2c_write16addr(0x30, 0x3402, 0x04);
					wmt_vid_i2c_write16addr(0x30, 0x3403, 0x00);
					wmt_vid_i2c_write16addr(0x30, 0x3404, 0x04);
					wmt_vid_i2c_write16addr(0x30, 0x3405, 0x30);
					wmt_vid_i2c_write16addr(0x30, 0x3208, 0x10);
					wmt_vid_i2c_write16addr(0x30, 0x3208, 0xa0);
					break;
			}
			break;

		case CMOS_ID_SIV_121D:
			switch(value)
			{
				case 0://auto
					wmt_vid_i2c_write(0x33,0x00,0x03) ;
					wmt_vid_i2c_write(0x33,0x10,0xd0) ;
					break;
				case 1://INCANDESCENT
					wmt_vid_i2c_write(0x33,0x00,0x03) ;
					wmt_vid_i2c_write(0x33,0x10,0xd5) ;
					wmt_vid_i2c_write(0x33,0x72,0x80) ;
					wmt_vid_i2c_write(0x33,0x73,0xe0) ;
					break;
				case 2://FLUORESCENT
					wmt_vid_i2c_write(0x33,0x00,0x03) ;
					wmt_vid_i2c_write(0x33,0x10,0xd5) ;
					wmt_vid_i2c_write(0x33,0x72,0x78) ;
					wmt_vid_i2c_write(0x33,0x73,0xa0) ;
					break;
				case 3://DAYLIGHT
					wmt_vid_i2c_write(0x33,0x00,0x03) ;
					wmt_vid_i2c_write(0x33,0x10,0xd5) ;
					wmt_vid_i2c_write(0x33,0x72,0xd8) ;
					wmt_vid_i2c_write(0x33,0x73,0x90) ;
					break;
				case 4://CLOUDY
					wmt_vid_i2c_write(0x33,0x00,0x03) ;
					wmt_vid_i2c_write(0x33,0x10,0xd5) ;
					wmt_vid_i2c_write(0x33,0x72,0xb4) ;
					wmt_vid_i2c_write(0x33,0x73,0x74) ;
					break;
			}
			break;

		case CMOS_ID_SID_130B:
			switch(value)
			{
				case 0://auto
					wmt_vid_i2c_write(0x37,0x00,0x02) ;
					wmt_vid_i2c_write(0x37,0x10,0xd3) ;
					wmt_vid_i2c_write(0x37,0xff,0xff) ;
					break;
				case 1://INCANDESCENT
					wmt_vid_i2c_write(0x37,0x00,0x02) ;
					wmt_vid_i2c_write(0x37,0x10,0x00) ;
					wmt_vid_i2c_write(0x37,0x50,0xaa) ;
					wmt_vid_i2c_write(0x37,0x51,0xbe) ;
					wmt_vid_i2c_write(0x37,0xff,0xff) ;
					break;
				case 2://FLUORESCENT
					wmt_vid_i2c_write(0x37,0x00,0x02) ;
					wmt_vid_i2c_write(0x37,0x10,0x00) ;
					wmt_vid_i2c_write(0x37,0x50,0xc2) ;
					wmt_vid_i2c_write(0x37,0x51,0x9e) ;
					wmt_vid_i2c_write(0x37,0xff,0xff) ;
					break;
				case 3://DAYLIGHT
					wmt_vid_i2c_write(0x37,0x00,0x02) ;
					wmt_vid_i2c_write(0x37,0x10,0x00) ;
					wmt_vid_i2c_write(0x37,0x50,0xaa) ;
					wmt_vid_i2c_write(0x37,0x51,0x90) ;
					wmt_vid_i2c_write(0x37,0xff,0xff) ;
					break;
				case 4://CLOUDY
					wmt_vid_i2c_write(0x37,0x00,0x02) ;
					wmt_vid_i2c_write(0x37,0x10,0x00) ;
					wmt_vid_i2c_write(0x37,0x50,0xd0) ;
					wmt_vid_i2c_write(0x37,0x51,0x88) ;
					wmt_vid_i2c_write(0x37,0xff,0xff) ;
					break;
			}
			break;
		
		case CMOS_ID_HI_704:
			switch(value)
			{
				case 0://auto
		wmt_vid_i2c_write(0x30,0x03, 0x22);			
		wmt_vid_i2c_write(0x30,0x83, 0x52);
		wmt_vid_i2c_write(0x30,0x84, 0x1b);
		wmt_vid_i2c_write(0x30,0x85, 0x50);
		wmt_vid_i2c_write(0x30,0x86, 0x25);				
					break;
				case 1://INCANDESCENT
		wmt_vid_i2c_write(0x30,0x03, 0x22);
		wmt_vid_i2c_write(0x30,0x11, 0x28);		  
		wmt_vid_i2c_write(0x30,0x80, 0x29);
		wmt_vid_i2c_write(0x30,0x82, 0x54);
		wmt_vid_i2c_write(0x30,0x83, 0x2e);
		wmt_vid_i2c_write(0x30,0x84, 0x23);
		wmt_vid_i2c_write(0x30,0x85, 0x58);
		wmt_vid_i2c_write(0x30,0x86, 0x4f);
					break;
				case 2://FLUORESCENT
		wmt_vid_i2c_write(0x30,0x03, 0x22);
		wmt_vid_i2c_write(0x30,0x11, 0x28);
		wmt_vid_i2c_write(0x30,0x80, 0x41);
		wmt_vid_i2c_write(0x30,0x82, 0x42);
		wmt_vid_i2c_write(0x30,0x83, 0x44);
		wmt_vid_i2c_write(0x30,0x84, 0x34);
		wmt_vid_i2c_write(0x30,0x85, 0x46);
		wmt_vid_i2c_write(0x30,0x86, 0x3a);
					break;
				case 3://DAYLIGHT
		wmt_vid_i2c_write(0x30,0x03, 0x22);
		wmt_vid_i2c_write(0x30,0x11, 0x28); 	  
		wmt_vid_i2c_write(0x30,0x80, 0x59);
		wmt_vid_i2c_write(0x30,0x82, 0x29);
		wmt_vid_i2c_write(0x30,0x83, 0x60);
		wmt_vid_i2c_write(0x30,0x84, 0x50);
		wmt_vid_i2c_write(0x30,0x85, 0x2f);
		wmt_vid_i2c_write(0x30,0x86, 0x23);
					break;
				case 4://CLOUDY
		wmt_vid_i2c_write(0x30,0x03, 0x22);
		wmt_vid_i2c_write(0x30,0x11, 0x28);
		wmt_vid_i2c_write(0x30,0x80, 0x71);
		wmt_vid_i2c_write(0x30,0x82, 0x2b);
		wmt_vid_i2c_write(0x30,0x83, 0x72);
		wmt_vid_i2c_write(0x30,0x84, 0x70);
		wmt_vid_i2c_write(0x30,0x85, 0x2b);
		wmt_vid_i2c_write(0x30,0x86, 0x28);
					break;
			}
			break;

		case CMOS_ID_YACD_511:
			switch(value)
			{
				case 0://auto
					wmt_vid_i2c_write(0x20,0x03,0x22) ;
					wmt_vid_i2c_write(0x20,0x80,0x2d) ;
					wmt_vid_i2c_write(0x20,0x82,0x42) ;
					wmt_vid_i2c_write(0x20,0x83,0x50) ;
					wmt_vid_i2c_write(0x20,0x84,0x10) ;
					wmt_vid_i2c_write(0x20,0x85,0x60) ;
					wmt_vid_i2c_write(0x20,0x86,0x23) ;
					wmt_vid_i2c_write(0x20,0xff,0xff) ;
					break;
				case 1://INCANDESCENT
					wmt_vid_i2c_write(0x20,0x03,0x22) ;
					wmt_vid_i2c_write(0x20,0x80,0x2a) ;
					wmt_vid_i2c_write(0x20,0x82,0x3f) ;
					wmt_vid_i2c_write(0x20,0x83,0x35) ;
					wmt_vid_i2c_write(0x20,0x84,0x28) ;
					wmt_vid_i2c_write(0x20,0x85,0x45) ;
					wmt_vid_i2c_write(0x20,0x86,0x3b) ;
					wmt_vid_i2c_write(0x20,0xff,0xff) ;
					break;
				case 2://FLUORESCENT
					wmt_vid_i2c_write(0x20,0x03,0x22) ;
					wmt_vid_i2c_write(0x20,0x80,0x20) ;
					wmt_vid_i2c_write(0x20,0x82,0x4d) ;
					wmt_vid_i2c_write(0x20,0x83,0x25) ;
					wmt_vid_i2c_write(0x20,0x84,0x1b) ;
					wmt_vid_i2c_write(0x20,0x85,0x55) ;
					wmt_vid_i2c_write(0x20,0x86,0x48) ;
					wmt_vid_i2c_write(0x20,0xff,0xff) ;
					break;
				case 3://DAYLIGHT
					wmt_vid_i2c_write(0x20,0x03,0x22) ;
					wmt_vid_i2c_write(0x20,0x80,0x3d) ;
					wmt_vid_i2c_write(0x20,0x82,0x2e) ;
					wmt_vid_i2c_write(0x20,0x83,0x40) ;
					wmt_vid_i2c_write(0x20,0x84,0x33) ;
					wmt_vid_i2c_write(0x20,0x85,0x33) ;
					wmt_vid_i2c_write(0x20,0x86,0x28) ;
					wmt_vid_i2c_write(0x20,0xff,0xff) ;
					break;
				case 4://CLOUDY
					wmt_vid_i2c_write(0x20,0x03,0x22) ;
					wmt_vid_i2c_write(0x20,0x80,0x50) ;
					wmt_vid_i2c_write(0x20,0x82,0x25) ;
					wmt_vid_i2c_write(0x20,0x83,0x55) ;
					wmt_vid_i2c_write(0x20,0x84,0x4b) ;
					wmt_vid_i2c_write(0x20,0x85,0x28) ;
					wmt_vid_i2c_write(0x20,0x86,0x20) ;
					wmt_vid_i2c_write(0x20,0xff,0xff) ;
					break;
			}
			break;
		
		case CMOS_ID_GC_0308:
			switch(value)
			{
				case 0://auto
					wmt_vid_i2c_write( 0x21, 0x5a, 0x56);
					wmt_vid_i2c_write( 0x21, 0x5b, 0x40);
					wmt_vid_i2c_write( 0x21, 0x5c, 0x4a);
					wmt_vid_i2c_write( 0x21, 0x22, 0x57);
					break;
				case 1://INCANDESCENT
					wmt_vid_i2c_write( 0x21, 0x22, 0x55);
					wmt_vid_i2c_write( 0x21, 0x5a, 0x48);
					wmt_vid_i2c_write( 0x21, 0x5b, 0x40);
					wmt_vid_i2c_write( 0x21, 0x5c, 0x5c);
					break;
				case 2://FLUORESCENT
					wmt_vid_i2c_write( 0x21, 0x22, 0x55);
					wmt_vid_i2c_write( 0x21, 0x5a, 0x40);
					wmt_vid_i2c_write( 0x21, 0x5b, 0x42);
					wmt_vid_i2c_write( 0x21, 0x5c, 0x50);
					break;
				case 3://DAYLIGHT
					wmt_vid_i2c_write( 0x21, 0x22, 0x55);
					wmt_vid_i2c_write( 0x21, 0x5a, 0x74);
					wmt_vid_i2c_write( 0x21, 0x5b, 0x52);
					wmt_vid_i2c_write( 0x21, 0x5c, 0x40);
					break;
				case 4://CLOUDY
					wmt_vid_i2c_write( 0x21, 0x22, 0x55);
					wmt_vid_i2c_write( 0x21, 0x5a, 0x8c);
					wmt_vid_i2c_write( 0x21, 0x5b, 0x50);
					wmt_vid_i2c_write( 0x21, 0x5c, 0x40);
					break;
			}
			break;

		case CMOS_ID_GC_2035:
			switch(value)
			{
				case 0://auto
					wmt_vid_i2c_write(0x3c, 0xfe, 0x00);
					wmt_vid_i2c_write(0x3c, 0xb3, 0x61);
					wmt_vid_i2c_write(0x3c, 0xb4, 0x40);
					wmt_vid_i2c_write(0x3c, 0xb5, 0x61);
					wmt_vid_i2c_write(0x3c, 0x82, 0xfe);
					break;
				case 1://INCANDESCENT
					wmt_vid_i2c_write(0x3c, 0xfe, 0x00);
					wmt_vid_i2c_write(0x3c, 0x82, 0xfc);
					wmt_vid_i2c_write(0x3c, 0xb3, 0xa0);
					wmt_vid_i2c_write(0x3c, 0xb4, 0x45);
					wmt_vid_i2c_write(0x3c, 0xb5, 0x40);
					break;
				case 2://FLUORESCENT
					wmt_vid_i2c_write(0x3c, 0xfe, 0x00);
					wmt_vid_i2c_write(0x3c, 0x82, 0xfc);
					wmt_vid_i2c_write(0x3c, 0xb3, 0x50);
					wmt_vid_i2c_write(0x3c, 0xb4, 0x40);
					wmt_vid_i2c_write(0x3c, 0xb5, 0xa8);
					break;
				case 3://DAYLIGHT
					wmt_vid_i2c_write(0x3c, 0xfe, 0x00);
					wmt_vid_i2c_write(0x3c, 0x82, 0xfc);
					wmt_vid_i2c_write(0x3c, 0xb3, 0x58);
					wmt_vid_i2c_write(0x3c, 0xb4, 0x40);
					wmt_vid_i2c_write(0x3c, 0xb5, 0x60);
					break;
				case 4://CLOUDY
					wmt_vid_i2c_write(0x3c, 0xfe, 0x00);
					wmt_vid_i2c_write(0x3c, 0x82, 0xfc);
					wmt_vid_i2c_write(0x3c, 0xb3, 0x58);
					wmt_vid_i2c_write(0x3c, 0xb4, 0x40);
					wmt_vid_i2c_write(0x3c, 0xb5, 0x50);
					break;
			}
			break;
#if 1
		case CMOS_ID_SAMSUNG_S5K5CA:
			switch(value)
			{
				case 0://auto
wmt_vid_i2c_write16addr16data(0x3c,0x002A, 0x04D2);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x067F);
					break;
				case 1://INCANDESCENT
wmt_vid_i2c_write16addr16data(0x3c,0x002A, 0x04D2);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x0677);
wmt_vid_i2c_write16addr16data(0x3c,0x002A, 0x04A0);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x0126);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x0001);
wmt_vid_i2c_write16addr16data(0x3c,0x002A, 0x04A4);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x0100);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x0001);
wmt_vid_i2c_write16addr16data(0x3c,0x002A, 0x04A8);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x01C5);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x0001);
					break;
				case 2://FLUORESCENT
wmt_vid_i2c_write16addr16data(0x3c,0x002A, 0x04D2);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x0677);
wmt_vid_i2c_write16addr16data(0x3c,0x002A, 0x04A0);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x0145);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x0001);
wmt_vid_i2c_write16addr16data(0x3c,0x002A, 0x04A4);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x0100);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x0001);
wmt_vid_i2c_write16addr16data(0x3c,0x002A, 0x04A8);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x01EB);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x0001);
					break;
				case 3://DAYLIGHT
wmt_vid_i2c_write16addr16data(0x3c,0x002A, 0x04D2);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x0677);
wmt_vid_i2c_write16addr16data(0x3c,0x002A, 0x04A0);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x0166);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x0001);
wmt_vid_i2c_write16addr16data(0x3c,0x002A, 0x04A4);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x0100);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x0001);
wmt_vid_i2c_write16addr16data(0x3c,0x002A, 0x04A8);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x0145);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x0001);
					break;
				case 4://CLOUDY
wmt_vid_i2c_write16addr16data(0x3c,0x002A, 0x04D2);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x0677);
wmt_vid_i2c_write16addr16data(0x3c,0x002A, 0x04A0);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x0128);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x0001);
wmt_vid_i2c_write16addr16data(0x3c,0x002A, 0x04A4);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x0100);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x0001);
wmt_vid_i2c_write16addr16data(0x3c,0x002A, 0x04A8);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x01AB);
wmt_vid_i2c_write16addr16data(0x3c,0x0F12, 0x0001);
					break;
			}
			break;
#endif

	}
	return 0;
}

int updateAntiBanding(wmt_cmos_product_id id,int value)
{
	switch(id)
	{
		case CMOS_ID_GC_0309:
			switch(value)
			{
				case 0://50Hz
					wmt_vid_i2c_write(0x21,0x01,0xfa);
					wmt_vid_i2c_write(0x21,0x02,0x70);
					wmt_vid_i2c_write(0x21,0x0f,0x01);
					wmt_vid_i2c_write(0x21,0xe2,0x00);
					wmt_vid_i2c_write(0x21,0xe3,0x64);
					wmt_vid_i2c_write(0x21,0xe4,0x02);
					wmt_vid_i2c_write(0x21,0xe5,0x58);
					wmt_vid_i2c_write(0x21,0xe6,0x03);
					wmt_vid_i2c_write(0x21,0xe7,0x20);
					wmt_vid_i2c_write(0x21,0xe8,0x04);
					wmt_vid_i2c_write(0x21,0xe9,0xb0);
					wmt_vid_i2c_write(0x21,0xea,0x09);
					wmt_vid_i2c_write(0x21,0xeb,0xc4);
					break;
				case 1://60Hz
					wmt_vid_i2c_write(0x21,0x01,0x2c);
					wmt_vid_i2c_write(0x21,0x02,0x98);
					wmt_vid_i2c_write(0x21,0x0f,0x02);
					wmt_vid_i2c_write(0x21,0xe2,0x00);
					wmt_vid_i2c_write(0x21,0xe3,0x50);
					wmt_vid_i2c_write(0x21,0xe4,0x02);
					wmt_vid_i2c_write(0x21,0xe5,0x80);
					wmt_vid_i2c_write(0x21,0xe6,0x03);
					wmt_vid_i2c_write(0x21,0xe7,0xc0);
					wmt_vid_i2c_write(0x21,0xe8,0x05);
					wmt_vid_i2c_write(0x21,0xe9,0x00);
					wmt_vid_i2c_write(0x21,0xea,0x09);
					wmt_vid_i2c_write(0x21,0xeb,0x60);
					break;
			}
			break;

	}
	return 0;
}
#undef CMOS_DEV_MODULES_C
