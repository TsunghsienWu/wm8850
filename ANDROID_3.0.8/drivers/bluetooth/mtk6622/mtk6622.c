/*++
Copyright (c) 2011-2013  WonderMedia Technologies, Inc. All Rights Reserved.

This PROPRIETARY SOFTWARE is the property of WonderMedia
Technologies, Inc. and may contain trade secrets and/or other confidential
information of WonderMedia Technologies, Inc. This file shall not be
disclosed to any third party, in whole or in part, without prior written consent of
WonderMedia.  

THIS PROPRIETARY SOFTWARE AND ANY RELATED
DOCUMENTATION ARE PROVIDED AS IS, WITH ALL FAULTS, AND
WITHOUT WARRANTY OF ANY KIND EITHER EXPRESS OR IMPLIED,
AND WonderMedia TECHNOLOGIES, INC. DISCLAIMS ALL EXPRESS
OR IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE, QUIET ENJOYMENT OR
NON-INFRINGEMENT.  
--*/

#include <linux/module.h>
#include <linux/init.h>

#include <linux/platform_device.h>

#include <mach/hardware.h>

#include "../../char/wmt-pwm.h"



#define pwm_write_reg(addr,val,wait)	\
	REG32_VAL(addr) = val;	\
	while(REG32_VAL(PWM_STS_REG_ADDR)&=wait);
void pwm_set_scalar(int no,unsigned int scalar)
{
	unsigned int addr;

	addr = PWM_SCALAR_REG_ADDR + (0x10 * no);
	pwm_write_reg(addr,scalar,PWM_SCALAR_UPDATE << (8*no));
}

void pwm_set_period(int no,unsigned int period)
{
	unsigned int addr;

	addr = PWM_PERIOD_REG_ADDR + (0x10 * no);
	pwm_write_reg(addr,period,PWM_PERIOD_UPDATE << (8*no));
}

void pwm_set_duty(int no,unsigned int duty)
{
	unsigned int addr;

	addr = PWM_DUTY_REG_ADDR + (0x10 * no);
	pwm_write_reg(addr,duty,PWM_DUTY_UPDATE << (8*no));
}

unsigned int pwm_get_control(int no)
{
	unsigned int addr;

	addr = PWM_CTRL_REG_ADDR + (0x10 * no);
	return (REG32_VAL(addr) & 0xF);
}


unsigned int pwm_get_period(int no)
{
	unsigned int addr;

	addr = PWM_PERIOD_REG_ADDR + (0x10 * no);
	return (REG32_VAL(addr) & 0xFFF);
}

unsigned int pwm_get_scalar(int no)
{
	unsigned int addr;

	addr = PWM_SCALAR_REG_ADDR + (0x10 * no);
	return (REG32_VAL(addr) & 0xFFF);
}


unsigned int pwm_get_duty(int no)
{
	unsigned int addr;

	addr = PWM_DUTY_REG_ADDR + (0x10 * no);
	return (REG32_VAL(addr) & 0xFFF);
}

void pwm_set_gpio(int no,int enable)
{
	unsigned int pwm_pin;

	pwm_pin = (no==0)? 0x08:0x04;
	if( enable ) { 
		REG8_VAL(GPIO_OD_GP31_BYTE_ADDR) &= ~pwm_pin;
		REG8_VAL(GPIO_CTRL_GP31_BYTE_ADDR) &= ~pwm_pin;
	} else {
		REG8_VAL(GPIO_OD_GP31_BYTE_ADDR) |= pwm_pin;
		
		REG8_VAL(GPIO_OC_GP31_BYTE_ADDR) |= pwm_pin;
		REG8_VAL(GPIO_CTRL_GP31_BYTE_ADDR) |= pwm_pin;
	}
	/* set to PWM mode */
	if( no == 1 )
		REG32_VAL(GPIO_PIN_SHARING_SEL_4BYTE_ADDR) |= 0x10;

}


void pwm_set_control(int no,unsigned int ctrl)
{
	unsigned int addr;


	addr = PWM_CTRL_REG_ADDR + (0x10 * no);
	pwm_write_reg(addr,ctrl,PWM_CTRL_UPDATE << (8*no));
} /* End of pwm_proc */


static void  rtc_gpio_enable_32k(int which)
{
	int no=which;
	unsigned int scalar=15;
	unsigned int period=101;
	unsigned int duty=50;
	printk("rtc_gpio_enable pwm1 32k\n");
	
	if(pwm_get_scalar(no)!= scalar - 1)
		pwm_set_scalar(no, scalar -1);

	if(pwm_get_period(no) != period - 1)
		pwm_set_period(no, period - 1);

	if(pwm_get_duty(no) != duty - 1)
		pwm_set_duty(no, duty - 1);

	pwm_set_control(no, 0x7);

	pwm_set_gpio(no,1);

	
}
static void wmt_gpio25(int high)
{
	printk("wmt_gpio25\n");
	GPIO_CTRL_GPIO_27_24_BYTE_VAL |=0x2;
	GPIO_OC_GPIO_27_20_2BYTE_VAL |=0x200;
	if(high)
		GPIO_OD_GPIO_27_20_2BYTE_VAL |=0x200;
	else
		GPIO_OD_GPIO_27_20_2BYTE_VAL &=0xfcff;
	
}

static int atsmb_resume(struct platform_device *pdev)
{
	rtc_gpio_enable_32k(1);
	wmt_gpio25(1);
	return 0;
}

static int atsmb_suspend(struct platform_device *pdev)
{
	return 0;
}
static struct platform_driver mtk6622_driver = {
	.driver.name		= "mtk6622",
	.suspend	= atsmb_suspend,
	.resume		= atsmb_resume,
};
static int __init init_function(void)
{
	printk("hello,i am init function\n");
	//(void)platform_driver_register(&mtk6622_driver);
	rtc_gpio_enable_32k(1);
	wmt_gpio25(1);
	return 0;
}

static void __exit exit_function(void)
{
	//(void)platform_driver_unregister(&mtk6622_driver);
	printk("hello,i am exit function\n");
	return ;
}

module_exit(exit_function);
module_init(init_function);
