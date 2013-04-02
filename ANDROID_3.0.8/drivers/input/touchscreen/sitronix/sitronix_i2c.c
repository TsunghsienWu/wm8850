/* drivers/input/touchscreen/sis_i2c.c
 *
 * Copyright (C) 2009 SiS, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/linkage.h>
#include <linux/slab.h>
#include <mach/gpio-cs.h>
#include <asm/uaccess.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include "sitronix_i2c.h"

struct sitronix_data *pContext=NULL;

#ifdef TOUCH_KEY 

#define MENU_IDX    0
#define HOME_IDX    1
#define BACK_IDX    2
#define SEARCH_IDX  3
#define NUM_KEYS    4

static int virtual_keys[NUM_KEYS] ={
        KEY_BACK,
        KEY_HOME,
        KEY_MENU,
        KEY_SEARCH
};
#endif

#define I2C_BUS1 1


#ifdef CONFIG_HAS_EARLYSUSPEND
static void sitronix_early_suspend(struct early_suspend *h);
static void sitronix_late_resume(struct early_suspend *h);
#endif

static int sitronix_read(struct sitronix_data *sitronix, u8 *rxdata, int length)
{
	int ret;
	struct i2c_msg msg[2];

    msg[0].addr = sitronix->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = rxdata;
	
	msg[1].addr = sitronix->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = length;
	msg[1].buf = rxdata;

	ret = wmt_i2c_xfer_continue_if_4(msg, 2, I2C_BUS1);
	if (ret <= 0)
		dbg_err("msg i2c read error: %d\n", ret);
	
	return ret;
}

#ifdef SITRONIX_DEBUG
static int sitronix_write(struct sitronix_data *sitronix, u8 *txdata, int length)
{
	int ret;
	struct i2c_msg msg[1];

    msg[0].addr = sitronix->addr;
	msg[0].flags = 0;
	msg[0].len = length;
	msg[0].buf = txdata;

	ret = wmt_i2c_xfer_continue_if_4(msg, 1, I2C_BUS1);
	if (ret <= 0)
		dbg_err("msg i2c read error: %d\n", ret);
	
	return ret;
}

static int sitronix_get_fw_revision(struct sitronix_data *sitronix)
{
	int ret = 0;
	uint8_t buffer[4]={FIRMWARE_REVISION_3,0};

	ret = sitronix_read(sitronix, buffer, 4);
	if (ret < 0){
		dbg_err("read fw revision error (%d)\n", ret);
		return ret;
	}
    
	memcpy(sitronix->fw_revision, buffer, 4);
	printk("Fw Revision (hex): %x%x%x%x\n", buffer[0], buffer[1], buffer[2], buffer[3]);
	
	return 0;
}

static int sitronix_get_max_touches(struct sitronix_data *sitronix)
{
	int ret = 0;
	uint8_t buffer[1]={MAX_NUM_TOUCHES};

	ret = sitronix_read(sitronix, buffer, 1);
	if (ret < 0){
		dbg_err("read max touches error (%d)\n", ret);
		return ret;
	}
    
	sitronix->max_touches = buffer[0];
	printk("max touches = %d \n",sitronix->max_touches);
    
	return 0;
}

static int sitronix_get_protocol(struct sitronix_data *sitronix)
{
	int ret = 0;
	uint8_t buffer[1]={I2C_PROTOCOL};

	ret = sitronix_read(sitronix, buffer, 1);
	if (ret < 0){
		dbg_err("read i2c protocol error (%d)\n", ret);
		return ret;
	}
    
	sitronix->touch_protocol_type = buffer[0] & I2C_PROTOCOL_BMSK;
	sitronix->sensing_mode = (buffer[0] & (ONE_D_SENSING_CONTROL_BMSK << ONE_D_SENSING_CONTROL_SHFT)) >> ONE_D_SENSING_CONTROL_SHFT;
	printk("i2c protocol = %d ,sensing mode = %d \n", sitronix->touch_protocol_type, sitronix->sensing_mode);

	return 0;
}

static int sitronix_get_resolution(struct sitronix_data *sitronix)
{
	int ret = 0;
	uint8_t buffer[3]={XY_RESOLUTION_HIGH};

	ret = sitronix_read(sitronix, buffer, 3);
	if (ret < 0){
		dbg_err("read resolution error (%d)\n", ret);
		return ret;
	}
    
	sitronix->resolution_x = ((buffer[0] & (X_RES_H_BMSK << X_RES_H_SHFT)) << 4) | buffer[1];
	sitronix->resolution_y = ((buffer[0] & Y_RES_H_BMSK) << 8) | buffer[2];
	printk("Resolution: %d x %d\n", sitronix->resolution_x, sitronix->resolution_y);
	
	return 0;
}

static int sitronix_get_chip_id(struct sitronix_data *sitronix)
{
	int ret = 0;
	uint8_t buffer[3]={CHIP_ID};

	ret = sitronix_read(sitronix, buffer, 3);
	if (ret < 0){
		dbg_err("read Chip ID error (%d)\n", ret);
		return ret;
	}
    
	if(buffer[0] == 0){
		if(buffer[1] + buffer[2] > 32)
			sitronix->chip_id = 2;
		else
			sitronix->chip_id = 0;
	}else
		sitronix->chip_id = buffer[0];

    sitronix->Num_X = buffer[1];
	sitronix->Num_Y = buffer[2];
	printk("Chip ID = %d, Num_X = %d, Num_Y = %d\n", sitronix->chip_id, sitronix->Num_X, sitronix->Num_Y);

	return 0;
}

static int sitronix_get_device_status(struct sitronix_data *sitronix)
{
	int ret = 0;
	uint8_t buf[3]={FIRMWARE_VERSION,0};

	ret = sitronix_read(sitronix, buf, 3);
	if (ret < 0){
		dbg_err("read resolution error (%d)\n", ret);
		return ret;
	}
    
	printk("Firmware version:%02x, Status Reg:%02x,Ctrl Reg:%02x\n", buf[0], buf[1],buf[2]);
	return 0;

}

static int sitronix_get_device_info(struct sitronix_data *sitronix)
{
	int ret = 0;

    ret = sitronix_get_resolution(sitronix);
	if(ret < 0) return ret;
    
	ret = sitronix_get_chip_id(sitronix);
	if(ret < 0) return ret;
    
	ret = sitronix_get_fw_revision(sitronix);
	if(ret < 0) return ret;
    
	ret = sitronix_get_protocol(sitronix);
	if(ret < 0) return ret;
    
	ret = sitronix_get_max_touches(sitronix);
	if(ret < 0) return ret;
    
    ret = sitronix_get_device_status(sitronix);
    if(ret < 0) return ret;
    
	if((sitronix->fw_revision[0] == 0) && (sitronix->fw_revision[1] == 0)){
		if(sitronix->touch_protocol_type == SITRONIX_RESERVED_TYPE_0){
			sitronix->touch_protocol_type = SITRONIX_B_TYPE;
			printk("i2c protocol (revised) = %d \n", sitronix->touch_protocol_type);
		}
	}
    
	if(sitronix->touch_protocol_type == SITRONIX_A_TYPE)
		sitronix->pixel_length = PIXEL_DATA_LENGTH_A;
	else if(sitronix->touch_protocol_type == SITRONIX_B_TYPE){
		sitronix->pixel_length = PIXEL_DATA_LENGTH_B;
		sitronix->max_touches = 2;
		printk("max touches (revised) = %d \n", sitronix->max_touches);
	}

	return 0;
}
#endif

static void sitronix_read_work(struct work_struct *work)
{
	struct sitronix_data *sitronix= container_of(work, struct sitronix_data, read_work);
    int ret = -1;
	u8 buf[22] = {FINGERS,0};
	u8 i = 0, fingers = 0, tskey = 0;
	u16 px = 0, py = 0;
    u16 x = 0, y = 0;

    ret = sitronix_read(sitronix, buf,sizeof(buf));
    if(ret <= 0){
        dbg_err("get raw data failed!\n");
        goto err_exit;
    }
    
    fingers = buf[0]&0x0f;
    if( fingers ){
        /* Report co-ordinates to the multi-touch stack */
        for(i=0; i < fingers; i++){
            if(sitronix->swap){
                y = ((buf[i*4+2]<<4)&0x0700)|buf[i*4+3];
                x = ((buf[i*4+2]<<8)&0x0700)|buf[i*4+4];
            }else{
                x = ((buf[i*4+2]<<4)&0x0700)|buf[i*4+3];
                y = ((buf[i*4+2]<<8)&0x0700)|buf[i*4+4];
            }

            if(!(buf[i*4+2]&0x80)) continue; /*check valid bit */

            
            if(x > sitronix->xresl ) x = sitronix->xresl ;
            if(y > sitronix->yresl ) y = sitronix->yresl ;
            
            px = x;
            py = y;
            if(sitronix->xch) px = sitronix->xresl - x;
            if(sitronix->ych) py = sitronix->yresl - y;         
            
    		input_report_abs(sitronix->input_dev, ABS_MT_POSITION_X, px);
    		input_report_abs(sitronix->input_dev, ABS_MT_POSITION_Y, py);
    		input_report_abs(sitronix->input_dev, ABS_MT_TRACKING_ID, i+1);
    		input_mt_sync(sitronix->input_dev);
            sitronix->penup = 0;
            if(sitronix->dbg) printk("F%d,raw data: x=%-4d, y=%-4d; report data: px=%-4d, py=%-4d\n", i, x, y, px, py);
    	}
    	input_sync(sitronix->input_dev);

    }
    else if(!sitronix->penup){
        dbg("pen up.\n");
        sitronix->penup = 1;
        input_mt_sync(sitronix->input_dev);
        input_sync(sitronix->input_dev);
    }

    /* virtual keys */
    tskey = buf[1];
    if(tskey){
        if(!sitronix->tkey_idx){
            sitronix->tkey_idx = tskey;
            input_report_key(sitronix->input_dev,virtual_keys[sitronix->tkey_idx>>1] , 1);
            input_sync(sitronix->input_dev);
            dbg("virtual key down, idx=%d\n",sitronix->tkey_idx);
        }
    }else{
        if(sitronix->tkey_idx){   
            dbg("virtual key up , idx=%d\n",sitronix->tkey_idx);
            input_report_key(sitronix->input_dev,virtual_keys[sitronix->tkey_idx>>1] , 0);
            input_sync(sitronix->input_dev);
            sitronix->tkey_idx = tskey;            
        }
    }

err_exit:

    gpio_enable_irq(sitronix->igp_idx, sitronix->igp_bit);
    return;
}


static irqreturn_t sitronix_isr_handler(int irq, void *dev)
{
    struct sitronix_data *sitronix = dev;

    if (!gpio_irq_isenable(sitronix->igp_idx, sitronix->igp_bit) ||
        !gpio_irq_state(sitronix->igp_idx, sitronix->igp_bit))
		return IRQ_NONE;

    gpio_disable_irq(sitronix->igp_idx, sitronix->igp_bit);    
	if(!sitronix->earlysus) queue_work(sitronix->workqueue, &sitronix->read_work);
		
	return IRQ_HANDLED;
}

static void sitronix_reset(struct sitronix_data *sitronix)
{
    gpio_enable(sitronix->rgp_idx, sitronix->rgp_bit, OUTPUT);
    gpio_set_value(sitronix->rgp_idx, sitronix->rgp_bit, HIGH);
    mdelay(5);
    gpio_set_value(sitronix->rgp_idx, sitronix->rgp_bit, LOW);
    mdelay(5);
    gpio_set_value(sitronix->rgp_idx, sitronix->rgp_bit, HIGH);
    mdelay(5);

    return;
}
    
static int sitronix_auto_clb(struct sitronix_data *sitronix)
{
    return 1;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sitronix_early_suspend(struct early_suspend *handler)
{
    struct sitronix_data *sitronix = container_of(handler, struct sitronix_data, early_suspend);
    sitronix->earlysus = 1;
	gpio_disable_irq(sitronix->igp_idx, sitronix->igp_bit);
    return;
}

static void sitronix_late_resume(struct early_suspend *handler)
{
    struct sitronix_data *sitronix = container_of(handler, struct sitronix_data, early_suspend);
    
    sitronix->earlysus = 0;
    sitronix_reset(sitronix);
    msleep(200);

    gpio_enable(sitronix->igp_idx, sitronix->igp_bit,INPUT);
    gpio_pull_enable(sitronix->igp_idx, sitronix->igp_bit, PULL_UP);
    gpio_setup_irq(sitronix->igp_idx, sitronix->igp_bit, IRQ_FALLING);
    
    return;
}
#endif  //CONFIG_HAS_EARLYSUSPEND


static ssize_t cat_dbg(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "dbg \n");
}

static ssize_t echo_dbg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct sitronix_data *sitronix = pContext;

	sscanf(buf,"%d",&sitronix->dbg);
        
	return count;
}
static DEVICE_ATTR(dbg, S_IRUGO | S_IWUSR, cat_dbg, echo_dbg);

static ssize_t cat_clb(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "calibrate --echo 1 >clb \n");
}

static ssize_t echo_clb(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int cal ;
    struct sitronix_data *sitronix = pContext;
    
    sscanf(buf, "%d", &cal);
	if(cal){
        if(sitronix_auto_clb(sitronix) <= 0) printk("Auto calibrate failed.\n");
    }
        
	return count;
}
static DEVICE_ATTR(clb, S_IRUGO | S_IWUSR, cat_clb, echo_clb);

static struct attribute *sitronix_attributes[] = {
	&dev_attr_clb.attr,
    &dev_attr_dbg.attr,
	NULL
};

static const struct attribute_group sitronix_group = {
	.attrs = sitronix_attributes,
};

static int sitronix_sysfs_create_group(struct sitronix_data *sitronix, const struct attribute_group *group)
{
    int err;

    sitronix->kobj = kobject_create_and_add("wmtts", NULL) ;
    if(!sitronix->kobj){
        dbg_err("kobj create failed.\n");
        return -ENOMEM;
    }
    
    /* Register sysfs hooks */
	err = sysfs_create_group(sitronix->kobj, group);
	if (err < 0){
        kobject_del(sitronix->kobj);
		dbg_err("Create sysfs group failed!\n");
		return -ENOMEM;
	}

    return 0;
}

static void sitronix_sysfs_remove_group(struct sitronix_data *sitronix, const struct attribute_group *group)
{
    sysfs_remove_group(sitronix->kobj, group);
    kobject_del(sitronix->kobj);  
    return;
}

static int sitronix_probe(struct platform_device *pdev)
{
	int i;
    int err = 0;
	struct sitronix_data *sitronix = platform_get_drvdata(pdev);
    
	INIT_WORK(&sitronix->read_work, sitronix_read_work);
    sitronix->workqueue = create_singlethread_workqueue(sitronix->name);
	if (!sitronix->workqueue) {
		err = -ESRCH;
		goto exit_create_singlethread;
	}
    
    err = sitronix_sysfs_create_group(sitronix, &sitronix_group);
    if(err < 0){
        dbg("create sysfs group failed.\n");
        goto exit_create_group;
    }
    
	sitronix->input_dev = input_allocate_device();
	if (!sitronix->input_dev) {
		err = -ENOMEM;
		dbg("failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}
	
	sitronix->input_dev->name = sitronix->name;
	sitronix->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	set_bit(INPUT_PROP_DIRECT, sitronix->input_dev->propbit);

	input_set_abs_params(sitronix->input_dev,
			     ABS_MT_POSITION_X, 0, sitronix->xresl, 0, 0);
	input_set_abs_params(sitronix->input_dev,
			     ABS_MT_POSITION_Y, 0, sitronix->yresl, 0, 0);
    input_set_abs_params(sitronix->input_dev,
                 ABS_MT_TRACKING_ID, 0, 20, 0, 0);
#ifdef TOUCH_KEY 	
	for (i = 0; i <NUM_KEYS; i++)
		set_bit(virtual_keys[i], sitronix->input_dev->keybit);

	sitronix->input_dev->keycode = virtual_keys;
	sitronix->input_dev->keycodesize = sizeof(unsigned int);
	sitronix->input_dev->keycodemax = NUM_KEYS;
#endif

	err = input_register_device(sitronix->input_dev);
	if (err) {
		dbg_err("sitronix_ts_probe: failed to register input device.\n");
		goto exit_input_register_device_failed;
	}
        
#ifdef CONFIG_HAS_EARLYSUSPEND
	sitronix->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	sitronix->early_suspend.suspend = sitronix_early_suspend;
	sitronix->early_suspend.resume = sitronix_late_resume;
	register_early_suspend(&sitronix->early_suspend);
#endif
    
	if(request_irq(sitronix->irq, sitronix_isr_handler, IRQF_SHARED, sitronix->name, sitronix) < 0){
		dbg_err("Could not allocate irq for ts_sitronix !\n");
		err = -1;
		goto exit_register_irq;
	}	
	
    gpio_enable(sitronix->igp_idx, sitronix->igp_bit, INPUT);
    gpio_pull_enable(sitronix->igp_idx, sitronix->igp_bit, PULL_UP);
    gpio_setup_irq(sitronix->igp_idx, sitronix->igp_bit, IRQ_FALLING);
    
    sitronix_reset(sitronix);
    msleep(200);
#ifdef SITRONIX_DEBUG    
    sitronix_get_device_info(sitronix);
#endif
    
    return 0;
    
exit_register_irq:
	unregister_early_suspend(&sitronix->early_suspend);
exit_input_register_device_failed:
	input_free_device(sitronix->input_dev);
exit_input_dev_alloc_failed:
    sitronix_sysfs_remove_group(sitronix, &sitronix_group);
exit_create_group:
	cancel_work_sync(&sitronix->read_work);
	destroy_workqueue(sitronix->workqueue);  
exit_create_singlethread:
    //kfree(sitronix);
	return err;
}

static int sitronix_remove(struct platform_device *pdev)
{
    struct sitronix_data *sitronix = platform_get_drvdata(pdev);

    cancel_work_sync(&sitronix->read_work);
    flush_workqueue(sitronix->workqueue);
	destroy_workqueue(sitronix->workqueue);
    
    free_irq(sitronix->irq, sitronix);
    gpio_disable_irq(sitronix->igp_idx, sitronix->igp_bit);
    
    unregister_early_suspend(&sitronix->early_suspend);
	input_unregister_device(sitronix->input_dev);

    sitronix_sysfs_remove_group(sitronix, &sitronix_group);
    //kfree(sitronix);
    
	dbg("remove...\n");
	return 0;
}

static void sitronix_release(struct device *device)
{
    return;
}

static struct platform_device sitronix_device = {
	.name  	    = DEV_SITRONIX,
	.id       	= 0,
	.dev    	= {.release = sitronix_release},
};

static struct platform_driver sitronix_driver = {
	.driver = {
	           .name   	= DEV_SITRONIX,
		       .owner	= THIS_MODULE,
	 },
	.probe    = sitronix_probe,
	.remove   = sitronix_remove,
};

static int check_touch_env(struct sitronix_data *sitronix)
{
	int ret = 0;
	int len = 96;
	int Enable;
    char retval[96] = {0};
	char *p=NULL;

    // Get u-boot parameter
	if(wmt_getsyspara("wmt.io.touch", retval, &len)) return -EIO;

	sscanf(retval,"%d:",&Enable);
	//check touch enable
	if(Enable == 0) return -ENODEV;
	
	p = strchr(retval,':');
	p++;

    if(strncmp(p,"st1536",6)) return -ENODEV;
    
	sitronix->name = DEV_SITRONIX;
    sitronix->addr = SITRONIX_ADDR;
	p = strchr(p,':');
	p++;
	sscanf(p,"%d:%d:%d:%d:%d:%d",
        &sitronix->xresl, &sitronix->yresl,&sitronix->igp_idx,&sitronix->igp_bit,&sitronix->rgp_idx, &sitronix->rgp_bit);

    sitronix->irq = IRQ_GPIO;
	printk("%s reslx=%d, resly=%d, Interrupt GP%d, bit%d, Reset GP%d, bit%d\n", sitronix->name, 
        sitronix->xresl, sitronix->yresl, sitronix->igp_idx,sitronix->igp_bit,sitronix->rgp_idx,sitronix->rgp_bit);

    memset(retval, 0x00, sizeof(retval));
    ret = wmt_getsyspara("wmt.io.st1536", retval, &len);
    if(!ret) {
        sscanf(retval, "%d:%d:%d",&sitronix->swap, &sitronix->xch, &sitronix->ych);
    }else{     
        sitronix->swap = 0;
        sitronix->xch = 1;
        sitronix->ych = 0;        
    }    
    
    sitronix->penup = 1;
  
	return 0;
}

static int __init sitronix_init(void)
{
	int ret = -ENOMEM;
    struct sitronix_data *sitronix=NULL;

	sitronix = kzalloc(sizeof(struct sitronix_data), GFP_KERNEL);
    if(!sitronix){
        dbg_err("mem alloc failed.\n");
        return -ENOMEM;
    }
    
	pContext = sitronix;
	ret = check_touch_env(sitronix);
    if(ret < 0)
        goto exit_free_mem;

	ret = platform_device_register(&sitronix_device);
	if(ret){
		dbg_err("register platform drivver failed!\n");
		goto exit_free_mem;
	}    
    platform_set_drvdata(&sitronix_device, sitronix);
	
	ret = platform_driver_register(&sitronix_driver);
	if(ret){
		dbg_err("register platform device failed!\n");
		goto exit_unregister_pdev;
	}
	
	return ret;
	
exit_unregister_pdev:
	platform_device_unregister(&sitronix_device);
exit_free_mem:
    kfree(sitronix);
    pContext = NULL;
	return ret;
}

static void sitronix_exit(void)
{
    if(!pContext) return;
    
	platform_driver_unregister(&sitronix_driver);
	platform_device_unregister(&sitronix_device);
    kfree(pContext);
    
	return;
}

late_initcall(sitronix_init);
module_exit(sitronix_exit);

MODULE_DESCRIPTION("Sitronix Multi-Touch Driver");
MODULE_LICENSE("GPL");

