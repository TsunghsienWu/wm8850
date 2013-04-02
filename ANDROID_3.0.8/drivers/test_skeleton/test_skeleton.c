/*
 * Copyright 2012 WonderMedia Technologies, Inc. All Rights Reserved. 
 *  
 * This PROPRIETARY SOFTWARE is the property of WonderMedia Technologies, Inc. 
 * and may contain trade secrets and/or other confidential information of 
 * WonderMedia Technologies, Inc. This file shall not be disclosed to any third party, 
 * in whole or in part, without prior written consent of WonderMedia. 
 *  
 * THIS PROPRIETARY SOFTWARE AND ANY RELATED DOCUMENTATION ARE PROVIDED AS IS, 
 * WITH ALL FAULTS, AND WITHOUT WARRANTY OF ANY KIND EITHER EXPRESS OR IMPLIED, 
 * AND WonderMedia TECHNOLOGIES, INC. DISCLAIMS ALL EXPRESS OR IMPLIED WARRANTIES 
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.  
 */

#include <linux/module.h>	/* Specifically, a module */
#include <linux/kernel.h>	/* We're doing kernel work */
#include <linux/proc_fs.h>	/* Necessary because we use the proc fs */
#include <linux/uaccess.h>  //for copy_from_user
#include "gpio_customize_ease.h"

#define procfs_name "test"
extern struct proc_dir_entry proc_root;



/**
 * This structure hold information about the /proc file
 *
 */
struct proc_dir_entry *Our_Proc_test;


static int procfile_read(char *buffer,
	      char **buffer_location,
	      off_t offset, int buffer_length, int *eof, void *data)
{
	int ret = 0;
	
	
	/* 
	 * We give all of our information in one go, so if the
	 * user asks us if we have more information the
	 * answer should always be no.
	 *
	 * This is important because the standard read
	 * function from the library would continue to issue
	 * the read system call until the kernel replies
	 * that it has no more information, or until its
	 * buffer is filled.
	 */
	printk("dummy implement\n");

	return ret;
}

GPIO_CTRL gp_ctrl_global;

/*
 *this funciton just for facilitate debugging
 *command formater: echo "digital strings" > /proc/usb_serail
 * setenv wmt.6620.pmu "e3:3:D8110040:D8110080:D81100c0:D81100c4:D81100c8"
 * saveenv
*/
static int procfile_write(struct file *file, const char __user *buffer,
			   unsigned long count, void *data)
{
	char tmp[128];
	char printbuf[128];
	int num;
	int option;

	
	if(buffer && !copy_from_user(tmp, buffer, sizeof(tmp))) {

		num = sscanf(tmp, "%d", &option);				
		printk("your input: option:%d count:%d\n",option,count);
		if(!strlen(gp_ctrl_global.name)) {
			printk("begin to get_value_of_gpio_ctrl\n");
			get_value_of_gpio_ctrl("wmt.6620.pmu", &gp_ctrl_global);
			printf_gpio_ctrl(&gp_ctrl_global);
			printk("exit get_value_of_gpio_ctrl\n");
		}else{
			printk("have set parsr this uboot var:%s\n",gp_ctrl_global.name);
		}
		
		switch(option) {
		case 1:
		  /*enable gpio mode and ouput high*/
		  //setenv wmt.6620.pmu "e2:3:gpio_enable_register:gpio_out_enable:gpio_out_data:gpio_pull_enable"
		  //setenv wmt.6620.pmu "e2:6:42:82:c2:482" for control sus_gpio1
		  //setenv wmt.6620.pmu "e2:4:5a:9a:da:49a" for control KPADCOL[0] as gpio
		  printk("begin to set gpio to high\n");
		  gpio_enable(&gp_ctrl_global,OUTPUT);
		  gpio_set_value(&gp_ctrl_global,HIGH);
		  /**/
		  break;
		case 2:
		  printk("begin to set gpio to low\n");
		  printf_gpio_ctrl(&gp_ctrl_global);
		  gpio_enable(&gp_ctrl_global,OUTPUT);
		  gpio_set_value(&gp_ctrl_global,LOW);
		  break;
		case 3:
			//configuration register: gpio_enable_register:gpio_out_enable:gpio_input_data:gpio_int_ctl
			//:gpio_int_status:gpio_pull_enable:gpio_pull_ctrl
			//setenv wmt.6620.pmu "df:2:71:b1:31:31a:363:4b1:4f1"
			//setenv wmt.6620.pmu.bitoffset "01:6" for bit offset of gpio_pull_ctrl is 6 
			printk("begin to init and setup gpio inerrtupion hanlder\n");
			printf_gpio_ctrl(&gp_ctrl_global);
			test_gpio_interruption(&gp_ctrl_global);
			printk("gpio interruption register finished\n");
			break;
		case 4:
			break;
		case 5:
			break;
		case 6:
			break;
		default:
			printk("test command,for example: echo 1  > /proc/test\n");
		}
		return strlen(tmp);
	}else{
		printk("copy_from_user failed or buffer is null\n");
	}
}			   

static int __init test_skeleton_proc_init(void)
{
	printk(KERN_ALERT "enter test_skeleton_proc_init\n");
	printk(KERN_ERR "enter test_skeleton_proc_init\n");
	Our_Proc_test = create_proc_entry(procfs_name, 0644, NULL);
	
	if (Our_Proc_test == NULL) {
		//remove_proc_entry(procfs_name, &proc_root);
		printk(KERN_ALERT "Error: Could not initialize /proc/%s\n",
		       procfs_name);
		return -ENOMEM;
	}

	Our_Proc_test->read_proc = procfile_read;
	Our_Proc_test->write_proc = procfile_write;
	//Our_Proc_test->owner 	 = THIS_MODULE;
	Our_Proc_test->mode 	 = S_IFREG | S_IRUGO;
	Our_Proc_test->uid 	 = 0;
	Our_Proc_test->gid 	 = 0;
	Our_Proc_test->size 	 = 37;   //size what is its mean ?

	return 0;	/* everything is ok */
}

static void __exit test_skeleton_uninit(void)
{ 

	//remove_proc_entry(procfs_name, &proc_root);
}


module_init(test_skeleton_proc_init);
module_exit(test_skeleton_uninit);
MODULE_AUTHOR("WonderMedia Technologies, Inc.");
MODULE_DESCRIPTION("Mali PM Kernel Driver");
MODULE_LICENSE("GPL");