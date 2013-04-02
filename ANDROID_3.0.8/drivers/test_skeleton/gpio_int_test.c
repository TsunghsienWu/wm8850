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
#include <linux/kernel.h>
#include <linux/delay.h>
#include <mach/hardware.h>//for REG8_VAL
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/spinlock.h>


#include "gpio_customize_ease.h"
/*
* NOTE: all of gpio pin interruption is shared interruption,
* the first thing in interruption handler function is check this interruption is 
* myself or not, if it is not, we should return IRQ_NONE immmediately, then kernel will
* call next irq handler in handler chain speedly
* 
*  interuption is low active
*/
irqreturn_t gpio_irq_handler(int i, void *arg)
{
	GPIO_CTRL *gp_ctrl = (GPIO_CTRL *)arg;
	int pin_state;
	/*if this gpio pin int is not enable or int status bit is not toggled, it is not our interruption*/
	if(!gpio_irq_isEnable(gp_ctrl) || !gpio_irq_state(gp_ctrl))
		return IRQ_NONE;
	/*get value on gpin int pin*/
	pin_state = gpio_get_value(gp_ctrl);
	
	/*be here, be sure that this int is myself, but we should disable int before handler this interruption*/
	enable_gpio_int(gp_ctrl, INT_DIS);
	
	/*because this gpio pin is low active, so when it generate int, it should be low on this pin
	, or it maybe fake int*/
	if(pin_state == 0) {
		printk("this is my interruption!!!\n");
		printk("dummy handler\n");
	}else {
		printk("this is fake interruption!!\n");
	}
	/*clean corresponding int status bit, or it will generate int continuously for ever*/
	gpio_clean_irq_status(gp_ctrl);
	/*we should reopen this gpio pin int in order to capture the future/comming interruption*/
	enable_gpio_int(gp_ctrl, INT_EN);
	return IRQ_HANDLED;
}

int test_gpio_interruption(GPIO_CTRL *gp_ctrl)
{
	int iRet=0;
	/*init gpio interruption pin: input, low trigger, pull up, disable int*/
	gpio_enable(gp_ctrl, INPUT);
	set_gpio_irq_triger_mode(gp_ctrl, IRQ_LOW);
	gpio_pull_enable(gp_ctrl,PULL_UP);
	enable_gpio_int(gp_ctrl,INT_DIS);
	/*register interruption handler funciton*/
	iRet = request_irq(5, gpio_irq_handler, IRQF_SHARED, "gpio_int_test", gp_ctrl);
	if (iRet){
		printk("Request IRQ failed!ERRNO:%d.", iRet);
		return -1;
	}
	printk("register gpio interruption succesful\n");
	/*clear int status register*/
	gpio_clean_irq_status(gp_ctrl);
	/*enable interruption*/
	enable_gpio_int(gp_ctrl,INT_EN);
	printk("enable gpio int\n");
}