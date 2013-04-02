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
#include <linux/console.h>
#define procfs_name "usb_serial"
extern struct proc_dir_entry proc_root;

/**
 * This structure hold information about the /proc file
 *
 */
struct proc_dir_entry *Our_Proc_Usbtty;


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

/*
 *this funciton just for facilitate debugging
 *command formater: echo "digital strings" > /proc/usb_serail
 * 
*/
static int procfile_write(struct file *file, const char __user *buffer,
			   unsigned long count, void *data)
{
	char tmp[128];
	char printbuf[128];
	int num;
	int option;

	if(buffer && !copy_from_user(tmp, buffer, sizeof(tmp))) {

  	  num = sscanf(tmp, "%d %s", &option, printbuf);

    printk("your input: option:%d,printfbuf:%s,count:%d\n",option, printbuf,count);
    switch(option) {
    case 1:
		  printk("begin to register console\n");
      register_console(&Gserial_console);
      printk("register_console successful\n");
      break;
    case 2:
		  printk("enter:%s,tmp:%s\n",__func__,tmp);
		  printk("begin to console setup\n");
		  Gserial_console_setup(NULL,NULL);
		  ttyGs_console_write(NULL,tmp, strlen(tmp));
		  printk("console write finished:%d\n",strlen(tmp));
      break;
    case 3:
    	printk("your input:%s\n",printbuf);
    	break;
    case 4:
    	printk("connect usb line\n");
    	usb_gadget_connect(gp_gadget);
    	break;
    case 5:
    	printk("disconnect usb line\n");
    	usb_gadget_disconnect(gp_gadget);
    	break;
    case 6:
    	printk("begint to unregister console\n");
    	//TODO:
    	unregister_console(&Gserial_console);
    	break;
    default:
    	printk("command formater: num string\n");
    	printk("for example: echo \"3 xiaojsj\" > /proc/usb_serial\n");
    }
		return strlen(tmp);
	}else{
		printk("copy_from_user failed or buffer is null\n");
	}
}			   

static int usb_serial_proc_init(void)
{
	Our_Proc_Usbtty = create_proc_entry(procfs_name, 0644, NULL);
	
	if (Our_Proc_Usbtty == NULL) {
		//remove_proc_entry(procfs_name, &proc_root);
		printk(KERN_ALERT "Error: Could not initialize /proc/%s\n",
		       procfs_name);
		return -ENOMEM;
	}

	Our_Proc_Usbtty->read_proc = procfile_read;
	Our_Proc_Usbtty->write_proc = procfile_write;
	//Our_Proc_Usbtty->owner 	 = THIS_MODULE;
	Our_Proc_Usbtty->mode 	 = S_IFREG | S_IRUGO;
	Our_Proc_Usbtty->uid 	 = 0;
	Our_Proc_Usbtty->gid 	 = 0;
	Our_Proc_Usbtty->size 	 = 37;   //size what is its mean ?

	return 0;	/* everything is ok */
}

static void usb_serial_uninit(void)
{

	//remove_proc_entry(procfs_name, &proc_root);
}


//module_init(usb_serial_proc_init);
//module_exit(usb_serial_uninit);