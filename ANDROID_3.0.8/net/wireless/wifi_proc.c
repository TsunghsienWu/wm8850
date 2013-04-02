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

/*
 *  procfs1.c -  create a "file" in /proc
 *
 */

#include <linux/module.h>	/* Specifically, a module */
#include <linux/kernel.h>	/* We're doing kernel work */
#include <linux/proc_fs.h>	/* Necessary because we use the proc fs */

#define procfs_name "wifi_config"
extern struct proc_dir_entry proc_root;
/**
 * This structure hold information about the /proc file
 *
 */
struct proc_dir_entry *Our_Proc_File;
static char wifi_name[64]="no wifi"; 


void set_wifi_name(char * name){
	if(strlen(name)<=sizeof(wifi_name))
		strcpy(wifi_name,name);
	return;
}
EXPORT_SYMBOL(set_wifi_name);

/* Put data into the proc fs file.
 * 
 * Arguments
 * =========
 * 1. The buffer where the data is to be inserted, if
 *    you decide to use it.
 * 2. A pointer to a pointer to characters. This is
 *    useful if you don't want to use the buffer
 *    allocated by the kernel.
 * 3. The current position in the file
 * 4. The size of the buffer in the first argument.
 * 5. Write a "1" here to indicate EOF.
 * 6. A pointer to data (useful in case one common 
 *    read for multiple /proc/... entries)
 *
 * Usage and Return Value
 * ======================
 * A return value of zero means you have no further
 * information at this time (end of file). A negative
 * return value is an error condition.
 *
 * For More Information
 * ====================
 * The way I discovered what to do with this function
 * wasn't by reading documentation, but by reading the
 * code which used it. I just looked to see what uses
 * the get_info field of proc_dir_entry struct (I used a
 * combination of find and grep, if you're interested),
 * and I saw that  it is used in <kernel source
 * directory>/fs/proc/array.c.
 *
 * If something is unknown about the kernel, this is
 * usually the way to go. In Linux we have the great
 * advantage of having the kernel source code for
 * free - use it.
 */
int
procfile_read(char *buffer,
	      char **buffer_location,
	      off_t offset, int buffer_length, int *eof, void *data)
{
	int ret;
	
	
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
	if (offset > 0) {
		/* we have finished to read, return 0 */
		ret  = 0;
	} else {
		/* fill the buffer, return the buffer size */
		ret = sprintf(buffer, wifi_name);
	}

	return ret;
}

static int __init wifi_proc_init(void)
{
	Our_Proc_File = create_proc_entry(procfs_name, 0644, NULL);
	
	if (Our_Proc_File == NULL) {
		remove_proc_entry(procfs_name, &proc_root);
		printk(KERN_ALERT "Error: Could not initialize /proc/%s\n",
		       procfs_name);
		return -ENOMEM;
	}

	Our_Proc_File->read_proc = procfile_read;
	//Our_Proc_File->owner 	 = THIS_MODULE;
	Our_Proc_File->mode 	 = S_IFREG | S_IRUGO;
	Our_Proc_File->uid 	 = 0;
	Our_Proc_File->gid 	 = 0;
	Our_Proc_File->size 	 = 37;

	return 0;	/* everything is ok */
}

static void __exit wifi_proc_uninit(void)
{

	remove_proc_entry(procfs_name, &proc_root);
}


module_init(wifi_proc_init);
module_exit(wifi_proc_uninit);

