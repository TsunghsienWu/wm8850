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
#include <mach/gpio_customize_ease.h>

//TODO: control this flag by /proc/gpio/debug_switch
#define gpio_DBG 0
//#define IF_GPIO_DEBUG_ENABLE if(gpio_DBG)   
#define printf printk

extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);

//####################################################### gpio API begin ########################################################

/*
* set gpio to input or output mode
* mode: 1 = input, 0 = output
* return 0 is successfull, other is failed
*/
int gpio_enable_any(GPIO_CTRL *gp_ctrl, GPIO_IN_OUT mode)
{
	REG8_VAL(gp_ctrl->gpio_enable.address) |= 1<< gp_ctrl->bitmap; //enable gpio mode
	REG8_VAL(gp_ctrl->gpio_pull_enable.address) &= ~(1<<gp_ctrl->bitmap); //disable default pull down

	if(mode == INPUT)
		REG8_VAL(gp_ctrl->gpio_out_enable.address) &= ~(1<<gp_ctrl->bitmap); //set as input
	else 
		REG8_VAL(gp_ctrl->gpio_out_enable.address) |= 1<<gp_ctrl->bitmap;  //set as output
	return 0;
}
EXPORT_SYMBOL(gpio_enable_any);

/*
* disable gpio mode
*/
int gpio_disable_any(GPIO_CTRL *gp_ctrl)
{
	REG8_VAL(gp_ctrl->gpio_enable.address) &= ~(1<<gp_ctrl->bitmap);
	return 0;
}
EXPORT_SYMBOL(gpio_disable_any);

int gpio_set_value_any(GPIO_CTRL *gp_ctrl,GPIO_OUTPUT value)
{
	/*set gpio output data register*/
	if(value == HIGH)
		REG8_VAL(gp_ctrl->gpio_out_data.address) |= (1<<gp_ctrl->bitmap);
	else
		REG8_VAL(gp_ctrl->gpio_out_data.address) &= ~(1<<gp_ctrl->bitmap);
	return 0;
}
EXPORT_SYMBOL(gpio_set_value_any);

int gpio_get_value_any(GPIO_CTRL *gp_ctrl)
{
	/* read GPIO Input Data Register */
	if(REG8_VAL(gp_ctrl->gpio_input_data.address) & (1<<gp_ctrl->bitmap))
		return HIGH;
	else
		return LOW;
}
EXPORT_SYMBOL(gpio_get_value_any);

/*
* mode: 0=pull down, mode=1 pull up
*/
int gpio_pull_enable_any(GPIO_CTRL *gp_ctrl,GPIO_PULL pull_mode)
{
	unsigned int offset =0;
	/*set pull enable register*/
	REG8_VAL(gp_ctrl->gpio_pull_enable.address) |= (1<<gp_ctrl->bitmap);
	/*set pull control register*/
	if(gp_ctrl->gpio_pull_ctrl.bit)
		offset = gp_ctrl->gpio_pull_ctrl.bit;
	else
		offset = gp_ctrl->bitmap;
	if(pull_mode == PULL_UP)
	{
		REG8_VAL(gp_ctrl->gpio_pull_ctrl.address) |= (1<<offset);
	} else {
		REG8_VAL(gp_ctrl->gpio_pull_ctrl.address) &= ~(1<<offset);
	}
	return 0;
}
EXPORT_SYMBOL(gpio_pull_enable_any);

int gpio_pull_disable_any(GPIO_CTRL *gp_ctrl)
{
	REG8_VAL(gp_ctrl->gpio_pull_enable.address) &= ~(1<<gp_ctrl->bitmap);
	return 0;
}
EXPORT_SYMBOL(gpio_pull_disable_any);

/*
*
* enable: 0=gpio irq disable, 1=gpio irq enable
*/
int enable_gpio_int_any(GPIO_CTRL *gp_ctrl, GPIO_INT_ENA enable)
{
	/*When this bit is active(a one), it indicates the corresponding interrupt function is enabled.*/
	if(enable)
		REG8_VAL(gp_ctrl->gpio_int_ctl.address) |= (1<<7);
	else
		REG8_VAL(gp_ctrl->gpio_int_ctl.address) &= ~(1<<7);
	return 0;
}
EXPORT_SYMBOL(enable_gpio_int_any);

int set_gpio_irq_triger_mode_any(GPIO_CTRL *gp_ctrl, GPIO_IRQ_TRIGER_MODE trigger_mode)
{
	REG8_VAL(gp_ctrl->gpio_int_ctl.address) &= ~0x07;
	switch(trigger_mode) {
	case IRQ_LOW:
		REG8_VAL(gp_ctrl->gpio_int_ctl.address) &= ~0x07;
		break;
	case IRQ_HIGH:
		REG8_VAL(gp_ctrl->gpio_int_ctl.address) |= 0x01;
		break;
	case IRQ_FALLING:
		REG8_VAL(gp_ctrl->gpio_int_ctl.address) |= 0x02;
		break;
	case IRQ_RISING:
		REG8_VAL(gp_ctrl->gpio_int_ctl.address) |= 0x03;
		break;
	case IRQ_BOTHEDGE:
		REG8_VAL(gp_ctrl->gpio_int_ctl.address) |= 0x04;
		break;
	default:
		//printk("gpio irq trigger mode set error\n");
		break;
	}
}
EXPORT_SYMBOL(set_gpio_irq_triger_mode_any);

int gpio_irq_state_any(GPIO_CTRL *gp_ctrl)
{
	if(REG8_VAL(gp_ctrl->gpio_int_status.address) & (1<<gp_ctrl->bitmap))
		return 0x01;
	else
		return 0x00;
}
EXPORT_SYMBOL(gpio_irq_state_any);

/*
*When this bit is active (a one), it indicates that an interrupt request has been configured and
*detected on the associated signal .  Once this bit is active (a one), software must write a one 
*to this bit to clear it inactive (a zero), otherwise this bit and the associated GPIO Interrupt 
*Request will remain active (a one).
*When this bit is inactive (a zero), it indicates that no interrupt has been configured and 
*detected on the associated signal
*/
int gpio_clean_irq_status_any(GPIO_CTRL *gp_ctrl)
{
	REG8_VAL((gp_ctrl->gpio_int_status.address)) |= (1<<gp_ctrl->bitmap);
}
EXPORT_SYMBOL(gpio_clean_irq_status_any);

int gpio_irq_isEnable_any(GPIO_CTRL *gp_ctrl)
{
	if(REG8_VAL(gp_ctrl->gpio_int_ctl.address) & (1<<0x07))
		return 0x01;
	else
		return 0x00;
}
EXPORT_SYMBOL(gpio_irq_isEnable_any);

//####################################################### gpio API end #########################################################


int get_uboot_parameter(char *varname, unsigned char *varval, int *varlen)
{
	return wmt_getsyspara(varname, varval, varlen);
}
EXPORT_SYMBOL(get_uboot_parameter);

/*
* input parameter is unsigned char(8 bit)
*/

int count_1_numbs(unsigned char value)
{
	int count = 0x00;
	while(value){
		if(value & 0x01)
			count++;
        value >>= 1;
	}
	//printf("count 1 numbers is:%d\n",count);
	return count;
}

void printf_gpio_ctrl(GPIO_CTRL *gp_ctrl)
{
	printf("enabe:%x\n out_enable:%x\n out_data:%x\n input_data:%x\n int_ctrl:%x\n int_status:%x\n pull_enable:%x\n pull_ctrl:%x\n",
			gp_ctrl->gpio_enable.address,
			gp_ctrl->gpio_out_enable.address,
			gp_ctrl->gpio_out_data.address,
			gp_ctrl->gpio_input_data.address,
			gp_ctrl->gpio_int_ctl.address,
			gp_ctrl->gpio_int_status.address,
			gp_ctrl->gpio_pull_enable.address,
			gp_ctrl->gpio_pull_ctrl.address);
	printf("gp_ctrl->name:%s\n",gp_ctrl->name);
	
#if 0	
	printf("enabe:%x\n out_enable:%x\n out_data:%x\n input_data:%x\n int_ctrl:%x\n int_status:%x\n pull_enable:%x\n pull_ctrl:%x\n",
			gp_ctrl->gpio_enable.bit,
			gp_ctrl->gpio_out_enable.bit,
			gp_ctrl->gpio_out_data.bit,
			gp_ctrl->gpio_input_data.bit,
			gp_ctrl->gpio_int_ctl.bit,
			gp_ctrl->gpio_int_status.bit,
			gp_ctrl->gpio_pull_enable.bit,
			gp_ctrl->gpio_pull_ctrl.bit);
#endif
}
EXPORT_SYMBOL(printf_gpio_ctrl);
/*input parameter is offset address*/
unsigned int phy_2_virtual(unsigned int phyAddr){
	if((phyAddr&0xFE000000) == 0xFE000000)
		return phyAddr;
	else
		return phyAddr + GPIO_BASE_ADDR;
}

/*
* uboot formater:
* name: bitmask:bitmap:gpio_enable_register:gpio_out_enable:gpio_out_data:gpio_input_data:gpio_int_ctl:gpio_int_status
* :gpio_pull_enable:gpio_pull_ctrl
*/
int parser_uboot_parameter(unsigned char *buf, GPIO_CTRL *gp_ctrl)
{
	unsigned char bitmask;
	int offset,count;
	//unsigned int buftmp[8] = {0};//store register address value
	unsigned int buftmp[8];//store register address value
	unsigned int * pbuf = buftmp;//pointer to buftmp
	char *data = buf; //point to input buf
	int i;
	
	memset(pbuf,0,sizeof(buftmp));
	/*point to the last register address*/
	GPIO_REGISTER * p_register = (GPIO_REGISTER *)(&(gp_ctrl->gpio_pull_ctrl.address));
	/*fetch bitmask*/
	sscanf(data,"%x:%n",&bitmask,&offset);
	count = count_1_numbs(bitmask);
	data += offset;
	gp_ctrl->bitmask = bitmask;
	
	/*fetch bitmap*/
	sscanf(data,"%x:%n",&gp_ctrl->bitmap,&offset);
	data += offset;
	
	/*fetch gpio register address*/
	while (count-- && (1 == sscanf(data,"%x:%n",pbuf++,&offset)))
	{	
		//if(gpio_DBG)
		//NOTE: if comment the following sentence, will lead to value of gp_ctrl be modified, why ????
		printf("count:%x,offset:%x\n",count,offset);
		data += offset;
	}

	if(gpio_DBG)
		printf("get register address,GPIO_BASE_ADDR:%x\n",GPIO_BASE_ADDR);
	for(i=0; i<8; i++) {
		if(buftmp[i])
			buftmp[i] = phy_2_virtual(buftmp[i]);
		if(gpio_DBG)
		printf("%d:%x\n",i,buftmp[i]);
	}
	
	pbuf--; // pointer to the last filled value
	/*begin to fill corresponding gpio register*/
	while(bitmask){
		if(bitmask & 0x001) {
			if(gpio_DBG)
				printf("0fill register addres:0x%x\n",*pbuf);
			p_register->address = *pbuf;
				pbuf--;
		}	
		p_register--;
		bitmask >>= 1;
		if(gpio_DBG)
			printf("bitmask:%x\n",bitmask);
	}
	return 0x0;
}

/*
* uboot formater:
* name: bitmask:enable_offset:out_enable_offset:out_data_offset:input_data_offset:int_ctl_offset:int_status_offset
* :pull_enable_offset:pull_ctrl_offset
*/
int parser_uboot_parameter_bit_offset(unsigned char *buf, GPIO_CTRL *gp_ctrl)
{
	unsigned char bitmask;
	int offset,count;
	unsigned int buftmp[8];//store bit offset
	unsigned int * pbuf = buftmp;//pointer to buftmp
	char *data = buf; //point to input buf
	int i;
	GPIO_REGISTER * p_register;
	
	memset(pbuf,0,sizeof(buftmp));
	/*point to the last register address*/
	p_register = (GPIO_REGISTER *)(&(gp_ctrl->gpio_pull_ctrl));
	/*fetch bitmask*/
	sscanf(data,"%x:%n",&bitmask,&offset);
	count = count_1_numbs(bitmask);
	data += offset;
		
	/*fetch register bit offset*/
	while (count-- && (1 == sscanf(data,"%x:%n",pbuf++,&offset)))
	{
		if(gpio_DBG)
			printf("count:%x,offset:%x\n",count,offset);
		data += offset;
	}

	if(gpio_DBG)
	{
		printf("get bit offset:\n");
		for(i=0; i<8; i++) {
			printf("%d:%x\n",i,buftmp[i]);
		}
	}
	
	pbuf--; // pointer to the last filled value
	/*begin to fill corresponding bit offset*/
	while(bitmask){
		if(bitmask & 0x001) {
			if(gpio_DBG)
				printf("fill bit offset:0x%x\n",*pbuf);
			p_register->bit = *pbuf;
			pbuf--;
		}	
		p_register--;
		bitmask >>= 1;
		if(gpio_DBG)
			printf("bitmask:%x\n",bitmask);
	}
	return 0x0;
}

int parse_gpio_ctrl_string(unsigned char *buf, GPIO_CTRL *gp_ctrl,unsigned char * name)
{ 
	int retval = 0x00;
	retval = parser_uboot_parameter(buf, gp_ctrl);
	strcpy(gp_ctrl->name, name);
	printf_gpio_ctrl(gp_ctrl);
	return retval;
}
EXPORT_SYMBOL(parse_gpio_ctrl_string);

/*
* fetch uboot parameter by uboot_env specify
* return value:
*/
int get_value_of_gpio_ctrl(unsigned char * uboot_env, GPIO_CTRL const *gp_ctrl)
{
	int retval=0;
	int varlen=127;
	unsigned char buf[200]={0};
	unsigned char buf_strcat[100]={0};
	memset(gp_ctrl,0,sizeof(GPIO_CTRL));
	retval = wmt_getsyspara(uboot_env,buf,&varlen);
	if(!retval)
	{
		strcpy(gp_ctrl->name,uboot_env);
		retval = parser_uboot_parameter(buf, gp_ctrl);
		
		memset(buf,0,sizeof(buf));
		varlen = 127;
		
		strcpy(buf_strcat,uboot_env);
		strcat(buf_strcat,".bitoffset");
		
		printk("buf_strcat:%s\n",buf_strcat);
		retval = wmt_getsyspara(buf_strcat,buf,&varlen);
		if(!retval) {
			retval = parser_uboot_parameter_bit_offset(buf, gp_ctrl);
		}else {
			printk("uboot parameter:%s do not exit\n",buf_strcat);
			retval = 0;
		}
		printf_gpio_ctrl(gp_ctrl);
	}else{
		printk("read uboot var:%s do not exit\n",uboot_env);
		retval = 0x5a;
	}

	return retval;
}
EXPORT_SYMBOL(get_value_of_gpio_ctrl);

