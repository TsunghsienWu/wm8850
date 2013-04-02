/*++ 
 * linux/drivers/video/wmt/lcd.c
 * WonderMedia video post processor (VPP) driver
 *
 * Copyright c 2012  WonderMedia  Technologies, Inc.
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

#define LCD_C
// #define DEBUG
/*----------------------- DEPENDENCE -----------------------------------------*/
#include "../lcd.h"
#include "../vout.h"
#include "../../../char/wmt-pwm.h"

/*----------------------- PRIVATE MACRO --------------------------------------*/
/* #define  LCD_XXXX  xxxx    *//*Example*/

/*----------------------- PRIVATE CONSTANTS ----------------------------------*/
/* #define LCD_XXXX    1     *//*Example*/

/*----------------------- PRIVATE TYPE  --------------------------------------*/
/* typedef  xxxx lcd_xxx_t; *//*Example*/
typedef struct {
	lcd_parm_t* (*get_parm)(int arg);
} lcd_device_t;

/*----------EXPORTED PRIVATE VARIABLES are defined in lcd.h  -------------*/
/*----------------------- INTERNAL PRIVATE VARIABLES - -----------------------*/
/* int  lcd_xxx;        *//*Example*/
lcd_device_t lcd_device_array[LCD_PANEL_MAX];
int lcd_panel_on = 1;
int lcd_pwm_enable;
lcd_panel_t lcd_panel_id = LCD_PANEL_MAX;
int lcd_panel_bpp = 24;

extern vout_dev_t lcd_vout_dev_ops;

/*--------------------- INTERNAL PRIVATE FUNCTIONS ---------------------------*/
/* void lcd_xxx(void); *//*Example*/
#ifdef CONFIG_PWM_WMT
extern void pwm_set_enable(int no,int enable);
extern void pwm_set_freq(int no,unsigned int freq);
extern void pwm_set_level(int no,unsigned int level);

extern void pwm_set_scalar(int no,unsigned int scalar);
extern void pwm_set_period(int no,unsigned int period);
extern void pwm_set_duty(int no,unsigned int duty) ;
#endif

/*----------------------- Function Body --------------------------------------*/
/*----------------------- Backlight --------------------------------------*/
void lcd_set_parm(int id,int bpp)
{
	lcd_panel_id = id;
	lcd_panel_bpp = bpp;
}

vout_dev_t *lcd_get_dev(void)
{
	if( lcd_panel_id >= LCD_PANEL_MAX )
		return 0;

	return &lcd_vout_dev_ops;
}

#ifndef CFG_LOADER
void lcd_blt_enable(int no,int enable)
{
	if( lcd_pwm_enable ) return;
#ifdef CONFIG_LCD_BACKLIGHT_WMT
	pwm_set_enable(no,enable);
#endif	
	return;
} /* End of vt8430_blt_enable */

void lcd_blt_set_level(int no,int level)
{
	if( lcd_pwm_enable ) return;
#ifdef CONFIG_LCD_BACKLIGHT_WMT
	pwm_set_level(no,level);
#endif	
	return;
}

void lcd_blt_set_freq(int no,unsigned int freq)
{
	if( lcd_pwm_enable ) return;
#ifdef CONFIG_LCD_BACKLIGHT_WMT
	pwm_set_freq(no,freq);
#endif	
	return;
}

void lcd_blt_set_pwm(int no,unsigned int scalar,unsigned int period,unsigned int duty)
{
	lcd_pwm_enable = 1;
	lcd_blt_id = no;
	
#ifdef CONFIG_PWM_WMT
	if(scalar - 1 < 0)
		scalar = 0;
	if(period - 1 < 0)
		period = 0;
	if(duty -1 < 0)
		duty = 0;

	if(pwm_get_scalar(no)!= scalar - 1)
		pwm_set_scalar(lcd_blt_id, scalar -1);

	if(pwm_get_period(no) != period - 1)
		pwm_set_period(lcd_blt_id, period - 1);

	if(pwm_get_duty(no) != duty - 1)
		pwm_set_duty(lcd_blt_id, duty - 1);
#endif					
}

/*----------------------- LCD --------------------------------------*/
static int __init lcd_arg_panel_id
(
	char *str			/*!<; // argument string */
)
{
	sscanf(str,"%d",(int *) &lcd_panel_id);
	if( lcd_panel_id >= LCD_PANEL_MAX ){
		lcd_panel_id = LCD_PANEL_MAX;
	}
	DBGMSG(KERN_INFO "set lcd panel id = %d\n",lcd_panel_id);	
  	return 1;
} /* End of lcd_arg_panel_id */

__setup("lcdid=", lcd_arg_panel_id);
#endif
int lcd_panel_register
(
	int no,						/*!<; //[IN] device number */
	void (*get_parm)(int mode)	/*!<; //[IN] get info function pointer */
)
{
	lcd_device_t *p;

	if( no >= LCD_PANEL_MAX ){
		DBGMSG(KERN_ERR "*E* lcd device no max is %d !\n",LCD_PANEL_MAX);
		return -1;
	}

	p = &lcd_device_array[no];
	if( p->get_parm ){
		DBGMSG(KERN_ERR "*E* lcd device %d exist !\n",no);
		return -1;
	}
	p->get_parm = (void *) get_parm;
//	printk("lcd_device_register %d 0x%x\n",no,p->get_parm);
	return 0;
} /* End of lcd_device_register */

lcd_parm_t *lcd_get_parm(lcd_panel_t id,unsigned int arg)
{
	lcd_device_t *p;

	p = &lcd_device_array[id];
	if( p && p->get_parm ){
		return p->get_parm(arg);
	}
	return 0;
}

/*----------------------- vout device plugin --------------------------------------*/
void lcd_set_power_down(int enable)
{
	lcd_blt_enable(lcd_blt_id,!enable);
}

int lcd_set_mode(unsigned int *option)
{
	vout_t *vo;
	
	DBGMSG("option %d,%d\n",option[0],option[1]);

	vo = lcd_vout_dev_ops.vout;
	if( option ){
		unsigned int capability;

		if( lcd_panel_id == 0 ){
			p_lcd = lcd_get_oem_parm(vo->resx,vo->resy);
		}
		else if( (p_lcd = lcd_get_parm(lcd_panel_id,lcd_panel_bpp)) ){
			MSG("[LCD] %s (id %d,bpp %d)\n",p_lcd->name,lcd_panel_id,lcd_panel_bpp);
		}
		else {
			DBG_ERR("lcd %d not support\n",lcd_panel_id);
			return -1;
		}
		
		if( p_lcd->initial ){
			p_lcd->initial();
		}
		lcd_blt_set_level(lcd_blt_id,lcd_blt_level);
		lcd_blt_set_freq(lcd_blt_id,lcd_blt_freq);
		lcd_blt_enable(lcd_blt_id,1);
		
		capability = p_lcd->capability;
		govrh_set_dvo_clock_delay(vo->govr,(capability & LCD_CAP_CLK_HI)? 0:1, 0);
		govrh_set_dvo_sync_polar(vo->govr,(capability & LCD_CAP_HSYNC_HI)? 0:1,(capability & LCD_CAP_VSYNC_HI)? 0:1);
		switch( lcd_panel_bpp ){
			case 15:
				govrh_IGS_set_mode(vo->govr,0,1,1);
				break;
			case 16:
				govrh_IGS_set_mode(vo->govr,0,3,1);
				break;
			case 18:
				govrh_IGS_set_mode(vo->govr,0,2,1);
				break;
			case 24:
				govrh_IGS_set_mode(vo->govr,0,0,0);
				break;
			default:
				break;
		}
//		g_vpp.vo_enable = 1;
	}
	else {
		lcd_blt_enable(lcd_blt_id,0);
		if( p_lcd && p_lcd->uninitial ){
			p_lcd->uninitial();
		}
		p_lcd = 0;
	}
	return 0;
}

int lcd_check_plugin(int hotplug)
{
	return (p_lcd)? 1:0;
}

int lcd_get_edid(char *buf)
{
	return 1;
}
	
int lcd_config(vout_info_t *info)
{
	info->resx = p_lcd->timing.hpixel;
	info->resy = p_lcd->timing.vpixel;
//	info->p_timing = &(p_lcd->timing);
	info->fps = p_lcd->fps;
	return 0;
}

int lcd_init(vout_t *vo)
{
	// vo_set_lcd_id(LCD_CHILIN_LW0700AT9003);
	if( lcd_panel_id >= LCD_PANEL_MAX ){
		return -1;
	}
	
	if( lcd_panel_id == 0 ){
		p_lcd = lcd_get_oem_parm(vo->resx,vo->resy);
	}
	else {
		p_lcd = lcd_get_parm(lcd_panel_id,24);
	}

	if( p_lcd == 0 ) 
		return -1;

	// set default parameters
	vo->resx = p_lcd->timing.hpixel;
	vo->resy = p_lcd->timing.vpixel;
	vo->pixclk = p_lcd->timing.pixel_clock;
	return 0;
}

vout_dev_t lcd_vout_dev_ops = {
	.name = "LCD",
	.mode = VOUT_INF_DVI,

	.init = lcd_init,
	.set_power_down = lcd_set_power_down,
	.set_mode = lcd_set_mode,
	.config = lcd_config,
	.check_plugin = lcd_check_plugin,
	.get_edid = lcd_get_edid,
};


int lcd_module_init(void)
{	
    lcd_blt_id = VPP_BLT_PWM_NUM;
	lcd_blt_level = 75;
	lcd_blt_freq = 20000;

	vout_device_register(&lcd_vout_dev_ops);
	return 0;
} /* End of lcd_module_init */
module_init(lcd_module_init);
/*--------------------End of Function Body -----------------------------------*/
#undef LCD_C
