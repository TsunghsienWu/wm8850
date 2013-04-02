/*
 * Driver for Pixcir I2C touchscreen controllers.
 *
 * Copyright (C) 2010-2011 Pixcir, Inc.
 *
 * pixcir.c V3.0	from v3.0 support TangoC solution and remove the previous soltutions
 *
 * pixcir.c V3.1	Add bootloader function	7
 *			Add RESET_TP		9
 * 			Add ENABLE_IRQ		10
 *			Add DISABLE_IRQ		11
 * 			Add BOOTLOADER_STU	12
 *			Add ATTB_VALUE		13
 *			Add Write/Read Interface for APP software
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
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/slab.h>  
#include <mach/gpio-cs.h>
#include <linux/earlysuspend.h>

#include <mach/hardware.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/wait.h>
#include "pixcir_tangoc.h"


struct pixcir_data *pContext = NULL;

#ifdef TOUCH_KEY 
#define MENU_IDX    0
#define HOME_IDX    1
#define BACK_IDX    2
#define SEARCH_IDX  3

static int keycodes[NUM_KEYS] ={
        KEY_MENU,
        KEY_HOME,
        KEY_BACK,
        KEY_SEARCH
};
#endif
static int pixcir_read(struct pixcir_data *pixcir, u8 *rxdata, int length)
{
	int ret;
	struct i2c_msg msg[2];

    msg[0].addr = pixcir->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = rxdata;
	
	msg[1].addr = pixcir->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = length;
	msg[1].buf = rxdata;

	ret = wmt_i2c_xfer_continue_if_4(msg, 2, I2C_BUS1);
	if (ret <= 0)
		dbg_err("msg i2c read error: %d\n", ret);
	
	return ret;
}


static int pixcir_write(struct pixcir_data *pixcir, u8 *txdata, int length)
{
	int ret;
	struct i2c_msg msg[1];

    msg[0].addr = pixcir->addr;
	msg[0].flags = 0;
	msg[0].len = length;
	msg[0].buf = txdata;

	ret = wmt_i2c_xfer_continue_if_4(msg, 1, I2C_BUS1);
	if (ret <= 0)
		dbg_err("msg i2c read error: %d\n", ret);
	
	return ret;
}

static void pixcir_read_work(struct work_struct *work)
{    
	u8 *p;
	u8 touch, button;
	u8 rdbuf[27]={0};
	int ret, i;
    int x=0,y=0,px,py,track_id,brn;
    struct pixcir_data *pixcir=container_of(work, struct pixcir_data, read_work.work);

    if(gpio_get_value(pixcir->igp_idx, pixcir->igp_bit) || pixcir->earlysus){
        dbg("INT High Level.\n");
        goto exit_penup;
    }

	ret = pixcir_read(pixcir, rdbuf, sizeof(rdbuf));
	if (ret <= 0) {
		dbg_err("pixcir_read failed, ret=%d\n",ret);
        goto exit_penup;
	}

	touch = rdbuf[0]&0x07;
	button = rdbuf[1];
	p=&rdbuf[2];
    dbg("%02x,%02x\n",touch,button);
    if (touch) {
		for(i=0; i<touch; i++) {
            brn = (*(p+4))>>3;//broken line
            track_id = (*(p+4))&0x7;
            px = (*(p+1)<<8)+(*(p));	
            py = (*(p+3)<<8)+(*(p+2));	
            p+=5;

            x = px;
            y = py;
            
            if(pixcir->swap) {
                x = py;
                y = px;
            }

            if (pixcir->xch) x = pixcir->xresl - px;
            if (pixcir->ych) y = pixcir->yresl - py;

            pixcir->penup = PEN_DOWN;
            input_report_abs(pixcir->input_dev, ABS_MT_TRACKING_ID, track_id+1);
			input_report_abs(pixcir->input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(pixcir->input_dev, ABS_MT_POSITION_Y, y);
			input_mt_sync(pixcir->input_dev);
            
			if(pixcir->dbg) printk("F%d: brn=%-2d Tid=%1d px=%-5d py=%-5d x=%-5d y=%-5d\n",i,brn,track_id,px,py,x,y);   
		}
        input_sync(pixcir->input_dev);
	} 

#ifdef TOUCH_KEY        
    if(button){
        if(!pixcir->tkey_pressed ){
            pixcir->tkey_pressed = 1;
            switch(button){
                case 1:
                    pixcir->tkey_idx = SEARCH_IDX;
                    input_report_key(pixcir->input_dev, keycodes[SEARCH_IDX], 1);
                    break;
                case 2:
                    pixcir->tkey_idx = BACK_IDX;
                    input_report_key(pixcir->input_dev, keycodes[BACK_IDX], 1);
                    break;
                case 4:
                    pixcir->tkey_idx = HOME_IDX;
                    input_report_key(pixcir->input_dev, keycodes[HOME_IDX], 1);
                    break;
                case 8:
                    pixcir->tkey_idx = MENU_IDX;
                    input_report_key(pixcir->input_dev, keycodes[MENU_IDX], 1);
                    break;
                default:
                    pixcir->tkey_idx = NUM_KEYS;
                    break;    
            }
        }
    }
    else{
        if(pixcir->tkey_pressed){
            if(pixcir->tkey_idx < NUM_KEYS ) {
                dbg("Report virtual key.\n");
                input_report_key(pixcir->input_dev, keycodes[pixcir->tkey_idx], 0);
                input_sync(pixcir->input_dev);             
            }
            pixcir->tkey_pressed = 0;
        }
    }        
#endif

    queue_delayed_work(pixcir->workqueue, &pixcir->read_work,16*HZ/1000);
    return;

exit_penup:
    if(pixcir->penup == PEN_DOWN){
        dbg("Report pen up.\n");
        input_mt_sync(pixcir->input_dev);
		input_sync(pixcir->input_dev);
        pixcir->penup = PEN_UP;
    }
#ifdef TOUCH_KEY     
    if(pixcir->tkey_idx < NUM_KEYS && pixcir->tkey_pressed ) {
        dbg("Report virtual key.\n");
        input_report_key(pixcir->input_dev, keycodes[pixcir->tkey_idx], 0);
        input_sync(pixcir->input_dev);
        pixcir->tkey_pressed = 0;
    }
#endif    
    gpio_enable_irq(pixcir->igp_idx, pixcir->igp_bit);
    return;
		
}


static irqreturn_t pixcir_isr_handler(int irq, void *dev)
{
    struct pixcir_data *pixcir = dev;

    if (!gpio_irq_isenable(pixcir->igp_idx, pixcir->igp_bit) ||
        !gpio_irq_state(pixcir->igp_idx, pixcir->igp_bit))
		return IRQ_NONE;
    
    gpio_disable_irq(pixcir->igp_idx, pixcir->igp_bit);    
	queue_delayed_work(pixcir->workqueue, &pixcir->read_work,0);
		
	return IRQ_HANDLED;
}

static void pixcir_reset(struct pixcir_data *pixcir)
{
    gpio_enable(pixcir->rgp_idx, pixcir->rgp_bit, OUTPUT);
    gpio_set_value(pixcir->rgp_idx, pixcir->rgp_bit, LOW);
    mdelay(5);
    gpio_set_value(pixcir->rgp_idx, pixcir->rgp_bit, HIGH);
    mdelay(10);
    gpio_set_value(pixcir->rgp_idx, pixcir->rgp_bit, LOW);
    mdelay(5);

    return;
}

static int pixcir_active(struct pixcir_data *pixcir)
{
    int ret;
    u8 cmd[3]={0x33,0,0x0a};
    
    ret = pixcir_write(pixcir, cmd, sizeof(cmd));
    if(ret <= 0)
        dbg_err("Pixcir initialize failed.\n");
    
    return ret;
}

static int pixcir_auto_clb(struct pixcir_data *pixcir)
{
    int ret;
    u8 cmd[2]={0x3a,0x03};
    
    ret = pixcir_write(pixcir, cmd, sizeof(cmd));
    if(ret <= 0)
        dbg_err("Pixcir initialize failed.\n");
    mdelay(500);
    return ret;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void pixcir_early_suspend(struct early_suspend *handler)
{
    struct pixcir_data *pixcir = container_of(handler, struct pixcir_data, early_suspend);
    pixcir->earlysus = 1;
	gpio_disable_irq(pixcir->igp_idx, pixcir->igp_bit);
    return;
}

static void pixcir_late_resume(struct early_suspend *handler)
{
    struct pixcir_data *pixcir = container_of(handler, struct pixcir_data, early_suspend);

    pixcir_reset(pixcir);
    msleep(200);
    pixcir_active(pixcir);
    
    pixcir->earlysus = 0;

    gpio_enable(pixcir->igp_idx, pixcir->igp_bit,INPUT);
    gpio_pull_enable(pixcir->igp_idx, pixcir->igp_bit, PULL_UP);
    gpio_setup_irq(pixcir->igp_idx, pixcir->igp_bit, IRQ_FALLING);
    
    return;
}
#endif  //CONFIG_HAS_EARLYSUSPEND


static ssize_t cat_dbg(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "dbg \n");
}

static ssize_t echo_dbg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct pixcir_data *pixcir = pContext;

	sscanf(buf,"%d",&pixcir->dbg);
        
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
    struct pixcir_data *pixcir = pContext;
    
    sscanf(buf, "%d", &cal);
	if(cal){
        if(pixcir_auto_clb(pixcir) <= 0) printk("Auto calibrate failed.\n");
    }
        
	return count;
}
static DEVICE_ATTR(clb, S_IRUGO | S_IWUSR, cat_clb, echo_clb);

static struct attribute *pixcir_attributes[] = {
	&dev_attr_clb.attr,
    &dev_attr_dbg.attr,
	NULL
};

static const struct attribute_group pixcir_group = {
	.attrs = pixcir_attributes,
};

static int pixcir_sysfs_create_group(struct pixcir_data *pixcir, const struct attribute_group *group)
{
    int err;

    pixcir->kobj = kobject_create_and_add("wmtts", NULL) ;
    if(!pixcir->kobj){
        dbg_err("kobj create failed.\n");
        return -ENOMEM;
    }
    
    /* Register sysfs hooks */
	err = sysfs_create_group(pixcir->kobj, group);
	if (err < 0){
        kobject_del(pixcir->kobj);
		dbg_err("Create sysfs group failed!\n");
		return -ENOMEM;
	}

    return 0;
}

static void pixcir_sysfs_remove_group(struct pixcir_data *pixcir, const struct attribute_group *group)
{
    sysfs_remove_group(pixcir->kobj, group);
    kobject_del(pixcir->kobj);  
    return;
}

static int pixcir_probe(struct platform_device *pdev)
{
	int i;
    int err = 0;
	struct pixcir_data *pixcir = platform_get_drvdata(pdev);
    
	INIT_DELAYED_WORK(&pixcir->read_work, pixcir_read_work);
    pixcir->workqueue = create_singlethread_workqueue(pixcir->name);
	if (!pixcir->workqueue) {
		err = -ESRCH;
		goto exit_create_singlethread;
	}
    
    err = pixcir_sysfs_create_group(pixcir, &pixcir_group);
    if(err < 0){
        dbg("create sysfs group failed.\n");
        goto exit_create_group;
    }
    
	pixcir->input_dev = input_allocate_device();
	if (!pixcir->input_dev) {
		err = -ENOMEM;
		dbg("failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}
	
	pixcir->input_dev->name = pixcir->name;
	pixcir->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	set_bit(INPUT_PROP_DIRECT, pixcir->input_dev->propbit);

	input_set_abs_params(pixcir->input_dev,
			     ABS_MT_POSITION_X, 0, pixcir->xresl, 0, 0);
	input_set_abs_params(pixcir->input_dev,
			     ABS_MT_POSITION_Y, 0, pixcir->yresl, 0, 0);
    input_set_abs_params(pixcir->input_dev,
                 ABS_MT_TRACKING_ID, 0, 20, 0, 0);
#ifdef TOUCH_KEY 	
	for (i = 0; i <NUM_KEYS; i++)
		set_bit(keycodes[i], pixcir->input_dev->keybit);

	pixcir->input_dev->keycode = keycodes;
	pixcir->input_dev->keycodesize = sizeof(unsigned int);
	pixcir->input_dev->keycodemax = NUM_KEYS;
#endif
    pixcir->earlysus = 1;
	err = input_register_device(pixcir->input_dev);
	if (err) {
		dbg_err("pixcir_ts_probe: failed to register input device.\n");
		goto exit_input_register_device_failed;
	}
        
#ifdef CONFIG_HAS_EARLYSUSPEND
	pixcir->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	pixcir->early_suspend.suspend = pixcir_early_suspend;
	pixcir->early_suspend.resume = pixcir_late_resume;
	register_early_suspend(&pixcir->early_suspend);
#endif
    
	if(request_irq(pixcir->irq, pixcir_isr_handler, IRQF_SHARED, pixcir->name, pixcir) < 0){
		dbg_err("Could not allocate irq for ts_pixcir !\n");
		err = -1;
		goto exit_register_irq;
	}	
	
    gpio_enable(pixcir->igp_idx, pixcir->igp_bit, INPUT);
    gpio_pull_enable(pixcir->igp_idx, pixcir->igp_bit, PULL_UP);
    gpio_setup_irq(pixcir->igp_idx, pixcir->igp_bit, IRQ_FALLING);
    
    pixcir_reset(pixcir);
    msleep(200);
    pixcir_active(pixcir);
    pixcir->earlysus = 0;
    
    return 0;
    
exit_register_irq:
	unregister_early_suspend(&pixcir->early_suspend);
exit_input_register_device_failed:
	input_free_device(pixcir->input_dev);
exit_input_dev_alloc_failed:
    pixcir_sysfs_remove_group(pixcir, &pixcir_group);
exit_create_group:
	cancel_delayed_work_sync(&pixcir->read_work);
	destroy_workqueue(pixcir->workqueue);  
exit_create_singlethread:
    //kfree(pixcir);
	return err;
}

static int pixcir_remove(struct platform_device *pdev)
{
    struct pixcir_data *pixcir = platform_get_drvdata(pdev);

    cancel_delayed_work_sync(&pixcir->read_work);
    flush_workqueue(pixcir->workqueue);
	destroy_workqueue(pixcir->workqueue);
    
    free_irq(pixcir->irq, pixcir);
    gpio_disable_irq(pixcir->igp_idx, pixcir->igp_bit);
    
    unregister_early_suspend(&pixcir->early_suspend);
	input_unregister_device(pixcir->input_dev);

    pixcir_sysfs_remove_group(pixcir, &pixcir_group);
    //kfree(pixcir);
    
	dbg("remove...\n");
	return 0;
}

static void pixcir_release(struct device *device)
{
    return;
}

static struct platform_device pixcir_device = {
	.name  	    = DEV_PIXCIR,
	.id       	= 0,
	.dev    	= {.release = pixcir_release},
};

static struct platform_driver pixcir_driver = {
	.driver = {
	           .name   	= DEV_PIXCIR,
		       .owner	= THIS_MODULE,
	 },
	.probe    = pixcir_probe,
	.remove   = pixcir_remove,
};

static int check_touch_env(struct pixcir_data *pixcir)
{
	//int ret = 0;
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

    if(strncmp(p,"tangoc",6)) return -ENODEV;
    
	pixcir->name = DEV_PIXCIR;
    pixcir->addr = PIXCIR_ADDR;
	p = strchr(p,':');
	p++;
	sscanf(p,"%d:%d:%d:%d:%d:%d",
        &pixcir->xresl, &pixcir->yresl,&pixcir->igp_idx,&pixcir->igp_bit,&pixcir->rgp_idx, &pixcir->rgp_bit);

    pixcir->irq = IRQ_GPIO;
	printk("%s reslx=%d, resly=%d, Interrupt GP%d, bit%d, Reset GP%d, bit%d\n", pixcir->name, 
        pixcir->xresl, pixcir->yresl, pixcir->igp_idx,pixcir->igp_bit,pixcir->rgp_idx,pixcir->rgp_bit);
    
    pixcir->swap = 1;
#if 0
    memset(retval,0x00,sizeof(retval));
    if(wmt_getsyspara("wmt.io.tangoc", retval, &len)){
        pixcir->swap = 1;
    }else{
        sscanf(p,"%d:%d:%d",&pixcir->swap,&pixcir->xch, &pixcir->ych);
    }


    memset(retval,0x00,sizeof(retval));
    ret = wmt_getsyspara("wmt.io.tskey", retval, &len);
    if(!ret){
        sscanf(retval,"%d:", &pixcir->nkeys);
        p = strchr(retval,':');
	    p++;
        for(i=0; i < pixcir->nkeys; i++ ){
            sscanf(p,"%d:%d", &pixcir->tkey.ypos[i].y_lower, &pixcir->tkey.ypos[i].y_upper);
            p = strchr(p,':');
	        p++;
            p = strchr(p,':');
	        p++;
        }
        sscanf(p,"%d:%d:%d", &pixcir->tkey.axis, &pixcir->tkey.x_lower, &pixcir->tkey.x_upper);
        pixcir->tskey_used = 1;           
    }
#endif    
	return 0;
}

static int __init pixcir_init(void)
{
	int ret = -ENOMEM;
    struct pixcir_data *pixcir=NULL;

	pixcir = kzalloc(sizeof(struct pixcir_data), GFP_KERNEL);
    if(!pixcir){
        dbg_err("mem alloc failed.\n");
        return -ENOMEM;
    }

    pContext = pixcir;
    ret = check_touch_env(pixcir);
    if(ret < 0)
        goto exit_free_mem;

	ret = platform_device_register(&pixcir_device);
	if(ret){
		dbg_err("register platform drivver failed!\n");
		goto exit_free_mem;
	}
    platform_set_drvdata(&pixcir_device, pixcir);
	
	ret = platform_driver_register(&pixcir_driver);
	if(ret){
		dbg_err("register platform device failed!\n");
		goto exit_unregister_pdev;
	}
	
	return ret;
	
exit_unregister_pdev:
	platform_device_unregister(&pixcir_device);
exit_free_mem:
    kfree(pixcir);
    pContext = NULL;
	return ret;
}

static void pixcir_exit(void)
{
    if(!pContext) return;
    
	platform_driver_unregister(&pixcir_driver);
	platform_device_unregister(&pixcir_device);
    kfree(pContext);

	return;
}

late_initcall(pixcir_init);
module_exit(pixcir_exit);

MODULE_AUTHOR("Jianchun Bian <jcbian@pixcir.com.cn>");
MODULE_DESCRIPTION("Pixcir I2C Touchscreen Driver");
MODULE_LICENSE("GPL");
