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

typedef struct gpio_register {
	unsigned int address; //register address
	unsigned int bit;     //register bit map
}GPIO_REGISTER;

typedef enum GPIO_MODE_T{
	OUTPUT=0,
	INPUT=1,
}GPIO_IN_OUT;

typedef enum GPIO_OUTPUT_T{
	LOW		=0,
	HIGH	=1,
}GPIO_OUTPUT;
typedef enum GPIO_PULL_T{
	PULL_DOWN	=0,
		PULL_UP	 	=1,
}GPIO_PULL;

typedef enum GPIO_INT_ENA_T{
	INT_DIS = 0,
	INT_EN=1,
}GPIO_INT_ENA;

typedef enum GPIO_IRQ_T{
		IRQ_LOW        =0x00,            /* Input zero generate GPIO_IRQ signal */
		IRQ_HIGH       =0x01,            /* Input one generate GPIO_IRQ signal */
		IRQ_FALLING    =0x02,            /* Falling edge generate GPIO_IRQ signal */
		IRQ_RISING     =0x03,            /* Rising edge generate GPIO_IRQ signal */
		IRQ_BOTHEDGE   =0x04,	     	  /* Both edge generate GPIO_IRQ signal */
		IRQ_NULL       =0x05,
}GPIO_IRQ_TRIGER_MODE;
/*
* uboot formater:
* name: gpio_enable_register:gpio_out_enable:gpio_out_data:gpio_input_data:gpio_int_ctl:gpio_int_status
* :gpio_pull_enable:gpio_pull_ctrl:bitmap
*/
typedef struct _gpio_ctrl {
	char name[64];
	unsigned char bitmask;   //mask corresponding register
	unsigned int bitmap;  // offset in register
	GPIO_REGISTER gpio_enable; //gpio Enable Register   
	GPIO_REGISTER gpio_out_enable; //Output Enable Register
	GPIO_REGISTER gpio_out_data; //Output Data Register 
	GPIO_REGISTER gpio_input_data; //Input Data Register
	GPIO_REGISTER gpio_int_ctl; //interrupt enable, strigger mode set
	GPIO_REGISTER gpio_int_status; //interrupt status register
	GPIO_REGISTER gpio_pull_enable; //Pull-up/down Enable Register
	GPIO_REGISTER gpio_pull_ctrl; //Pull-up/Pull-down Control Register
}GPIO_CTRL;

/*extern API*/
extern int get_value_of_gpio_ctrl(unsigned char * uboot_env, GPIO_CTRL const *gp_ctrl);
extern int parse_gpio_ctrl_string(unsigned char *buf, GPIO_CTRL *gp_ctrl,unsigned char * name);
extern void printf_gpio_ctrl(GPIO_CTRL *gp_ctrl);
extern int get_uboot_parameter(char *varname, unsigned char *varval, int *varlen);

extern int gpio_enable_any(GPIO_CTRL *gp_ctrl, GPIO_IN_OUT mode);
extern int gpio_set_value_any(GPIO_CTRL *gp_ctrl,GPIO_OUTPUT value);
extern int gpio_get_value_any(GPIO_CTRL *gp_ctrl);
extern int gpio_pull_enable_any(GPIO_CTRL *gp_ctrl,GPIO_PULL pull_mode);

/*interruption related API*/
extern int enable_gpio_int_any(GPIO_CTRL *gp_ctrl, GPIO_INT_ENA enable);
extern int set_gpio_irq_triger_mode_any(GPIO_CTRL *gp_ctrl, GPIO_IRQ_TRIGER_MODE trigger_mode);
extern int gpio_irq_state_any(GPIO_CTRL *gp_ctrl);
extern int gpio_clean_irq_status_any(GPIO_CTRL *gp_ctrl);
extern int gpio_irq_isEnable_any(GPIO_CTRL *gp_ctrl);

/*test & debug api*/
//extern int test_gpio_interruption(GPIO_CTRL *gp_ctrl);
