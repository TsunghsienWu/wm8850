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
#include "sis9210_i2c.h"

struct sis9210_data *pContext;

#ifdef TOUCH_KEY 

#define NUM_KEYS    4
static int virtual_keys[NUM_KEYS] ={
        KEY_MENU,
        KEY_HOME,
        KEY_BACK,
        KEY_SEARCH
};
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sis9210_early_suspend(struct early_suspend *h);
static void sis9210_late_resume(struct early_suspend *h);
#endif

static int sis9210_read(struct sis9210_data *sis9210, u8 *rxdata, int length)
{
	int ret;
	struct i2c_msg msg[2];

    msg[0].addr = sis9210->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = rxdata;
	
	msg[1].addr = sis9210->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = length;
	msg[1].buf = rxdata;

	ret = wmt_i2c_xfer_continue_if_4(msg, 2, I2C_BUS1);
	if (ret <= 0)
		dbg_err("msg i2c read error: %d\n", ret);
	
	return ret;
}

#if 0
static int sis9210_write(struct sis9210_data *sis9210, u8 *txdata, int length)
{
	int ret;
	struct i2c_msg msg[1];

    msg[0].addr = sis9210->addr;
	msg[0].flags = 0;
	msg[0].len = length;
	msg[0].buf = txdata;

	ret = wmt_i2c_xfer_continue_if_4(msg, 1, I2C_BUS1);
	if (ret <= 0)
		dbg_err("msg i2c read error: %d\n", ret);
	
	return ret;
}
#endif
static int sis9210_ReadPacket(struct sis9210_data *sis9210, u8 cmd, u8 *buf)
{
    int ret = -1,len=0;
	u8 offset = 0;
	bool ReadNext= false;
	u8 ByteCount = 0;
	u8 fingers = 0;
    u8 pbuf[MAX_READ_BYTE_COUNT]={0};

    pbuf[0] = cmd;
    
	do{
        //int i=0;
        ret = sis9210_read(sis9210, pbuf, MAX_READ_BYTE_COUNT);
#if 0        
        for(i=0;i<MAX_READ_BYTE_COUNT;i++)
            printk("%02x ",pbuf[i]);
        printk("\n");
#endif        
        len = pbuf[0] & 0xff;
        if (len > MAX_READ_BYTE_COUNT ||ret <= 0){
            dbg("data size error, len=%d.\n",len);
            return -1;
        }

        // FOR SiS9200
		switch (len)
		{
			case NO_TOUCH: 		//ByteCount:2,NoTouch
			case SINGLE_TOUCH:  //ByteCount:9,Single Point
			case LAST_ONE:		//ByteCount:7,Last one point
			case BUTTON_TOUCH:  //ByteCount:5,ButtonTouch
			case BUTTON_TOUCH_ONE_POINT: //ByteCount:10,ButtonTouch + Single Point  //CY
				ReadNext= false;//only read once packet
				break;

			case BUTTON_TOUCH_MULTI_TOUCH: //ByteCount:15,ButtonTouch + Multi Touch  //CY
			case MULTI_TOUCH:	//ByteCount:14,Multi Touch
				fingers = (pbuf[PKTINFO] & MSK_TOUCHNUM); //get total fingers' number
				if ((fingers <= 0) || (fingers > MAX_FINGERS))
        		{
        		    dbg("finger number error.\n");
        		    return -1;
        		}

        		ByteCount = 2 + (fingers * 5 ) + CRCCNT(fingers);   // Total byte count
				if (len == BUTTON_TOUCH_MULTI_TOUCH) // for button touch event
				{									 // add one bytecount,BS
				  ByteCount += 1;
				}
        		ByteCount = ByteCount - len;    // Byte counts that remain to be received
        		ReadNext= ByteCount > 0 ? true : false;		//whether are remained packets needed to read ?
        		break;

        	case LAST_TWO:  //ByteCount:12,Last two point
				ByteCount = ByteCount - len;
				ReadNext= ByteCount > 0 ? true : false;
				break;

			default:    // I2C,SMBus Read fail or Data incorrect
         	    printk(KERN_INFO "Unknow bytecount = %d\n", len);
        		return -1;
        		break;
		}

        memcpy(&buf[offset], &pbuf[CMD_BASE], len);
        offset += len;
    }
    while (ReadNext);

    return len;
}


static void sis9210_read_work(struct work_struct *work)
{
    int ret = -1;
	u8 buf[64] = {0};
	u8 i = 0, fingers = 0;
    //u8 crc = 0,
	u8 px = 0, py = 0,  idx= 0;
    u16 id, x, y;
    //u16 bPressure, bWidth;
    int vkey,vkey_idx= 0;
	struct sis9210_data *sis9210= container_of(work, struct sis9210_data, read_work.work);
    

    if(gpio_get_value(sis9210->igp_idx, sis9210->igp_bit) == HIGH){
        dbg("pen up.\n");
        sis9210->vkey_idx = -1;
        input_mt_sync(sis9210->input_dev);
        input_sync(sis9210->input_dev);
        gpio_enable_irq(sis9210->igp_idx, sis9210->igp_bit);
        return;
    }

    /* I2C or SMBUS block data read */
    ret = sis9210_ReadPacket(sis9210, SIS_CMD_NORMAL, buf);
	if (ret < 0){
	    dbg(KERN_INFO "read packet failed: ret = %d\n",ret);
		goto reapt_read;
	}else if ((ret > 2) && (!(buf[1] & MSK_HAS_CRC))){
	    dbg_err(KERN_INFO "command type error\n");
		goto reapt_read;
	}

    // FOR SiS9200
	/* Parser and Get the sis9200 data */
	fingers = (buf[FORMAT_MODE] & MSK_TOUCHNUM);
	fingers = (fingers > MAX_FINGERS ? 0 : fingers);

	if ((buf[FORMAT_MODE] & MSK_BUTTON_POINT) == BUTTON_TOUCH_SERIAL){	 
		if (fingers > 1) 
            vkey_idx = 2; // when fingers is >= 2 , BS is placed at the same position
		else 
            vkey_idx = fingers;
        
        vkey = buf[BUTTON_STATE + vkey_idx * 5];
		if(sis9210->vkey_idx < 0){
            for(i=0; i< 4; i++){
                if(vkey&(0x01<<i)){
                    dbg("vkey down, idx=%d\n",i);
                    sis9210->vkey_idx = i;
                    input_report_key(sis9210->input_dev, virtual_keys[sis9210->vkey_idx], 1);
                    input_sync(sis9210->input_dev);
                }                
            }
        }
	}else{
        if(sis9210->vkey_idx >= 0){
            dbg("vkey up, idx=%d\n",sis9210->vkey_idx);
            input_report_key(sis9210->input_dev, virtual_keys[sis9210->vkey_idx], 0);
            input_sync(sis9210->input_dev);
            sis9210->vkey_idx = -1;
        }
    }

    if( fingers ){
    	for (i = 0; i < fingers; i++){
            idx = 2 + (i * 5) + 2 * (i >> 1);    // Calc point status
    		if (((buf[FORMAT_MODE] & MSK_BUTTON_POINT) == BUTTON_TOUCH_SERIAL) && i > 1){
    		  idx += 1; 					// for button event and above 3 points
    		}
            
    	    px = idx + 1;                   // Calc point x_coord
    	    py = px + 2;                    // Calc point y_coord

    		//bPressure = (buf[idx] & MSK_PSTATE) == TOUCHDOWN ? 1 : 0;
    		//bWidth = (buf[idx] & MSK_PSTATE) == TOUCHDOWN ? 1 : 0;
    		id = (buf[idx] & MSK_PID) >> 4;
    		//x = (((buf[px] & 0xff) << 8) | (buf[px + 1] & 0xff));
            //y = (((buf[py] & 0xff) << 8) | (buf[py + 1] & 0xff));
            
            y = (((buf[px] & 0xff) << 8) | (buf[px + 1] & 0xff));
            x = 4095 - (((buf[py] & 0xff) << 8) | (buf[py + 1] & 0xff));
            if(sis9210->dbg) printk("F%d,Tid=%d, x=%-4d, y=%-4d\n",i,id, x, y);

            input_report_abs(sis9210->input_dev, ABS_MT_POSITION_X, x);
    		input_report_abs(sis9210->input_dev, ABS_MT_POSITION_Y, y);
            input_report_abs(sis9210->input_dev, ABS_MT_TRACKING_ID, id);
    		input_mt_sync(sis9210->input_dev);
    	}
        input_sync(sis9210->input_dev);
    }
   
reapt_read:
    if(!sis9210->earlysus) queue_delayed_work(sis9210->workqueue, &sis9210->read_work, 16*HZ/1000);
    return;
}


static irqreturn_t sis9210_isr_handler(int irq, void *dev)
{
    struct sis9210_data *sis9210 = dev;

    if (!gpio_irq_isenable(sis9210->igp_idx, sis9210->igp_bit) ||
        !gpio_irq_state(sis9210->igp_idx, sis9210->igp_bit))
		return IRQ_NONE;
    
    gpio_disable_irq(sis9210->igp_idx, sis9210->igp_bit);    
	if(!sis9210->earlysus) queue_delayed_work(sis9210->workqueue, &sis9210->read_work, 0);
		
	return IRQ_HANDLED;
}

static void sis9210_reset(struct sis9210_data *sis9210)
{
    gpio_enable(sis9210->rgp_idx, sis9210->rgp_bit, OUTPUT);   
    gpio_set_value(sis9210->rgp_idx, sis9210->rgp_bit, HIGH);
    mdelay(5);
    gpio_set_value(sis9210->rgp_idx, sis9210->rgp_bit, LOW);
    mdelay(5);
    gpio_set_value(sis9210->rgp_idx, sis9210->rgp_bit, HIGH);
    mdelay(5);

    return;
}

    
static int sis9210_auto_clb(struct sis9210_data *sis9210)
{
# if 0
    int ret;
    u8 cmd[2]={0x3a,0x03};
    
    ret = sis9210_write(sis9210, cmd, sizeof(cmd));
    if(ret <= 0)
        dbg_err("Sis92xx initialize failed.\n");
    mdelay(500);
#endif    
    return 2;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sis9210_early_suspend(struct early_suspend *handler)
{
    struct sis9210_data *sis9210 = container_of(handler, struct sis9210_data, early_suspend);
    sis9210->earlysus = 1;
	gpio_disable_irq(sis9210->igp_idx, sis9210->igp_bit);
    return;
}

static void sis9210_late_resume(struct early_suspend *handler)
{
    struct sis9210_data *sis9210 = container_of(handler, struct sis9210_data, early_suspend);

    sis9210->earlysus = 0;
    sis9210_reset(sis9210);
    msleep(200);

    gpio_enable(sis9210->igp_idx, sis9210->igp_bit,INPUT);
    gpio_pull_enable(sis9210->igp_idx, sis9210->igp_bit, PULL_UP);
    gpio_setup_irq(sis9210->igp_idx, sis9210->igp_bit, IRQ_FALLING);
    
    return;
}
#endif  //CONFIG_HAS_EARLYSUSPEND


static ssize_t cat_dbg(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "dbg \n");
}

static ssize_t echo_dbg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct sis9210_data *sis9210 = pContext;

	sscanf(buf,"%d",&sis9210->dbg);
        
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
    struct sis9210_data *sis9210 = pContext;
    
    sscanf(buf, "%d", &cal);
	if(cal){
        if(sis9210_auto_clb(sis9210) <= 0) printk("Auto calibrate failed.\n");
    }
        
	return count;
}
static DEVICE_ATTR(clb, S_IRUGO | S_IWUSR, cat_clb, echo_clb);

static struct attribute *sis9210_attributes[] = {
	&dev_attr_clb.attr,
    &dev_attr_dbg.attr,
	NULL
};

static const struct attribute_group sis9210_group = {
	.attrs = sis9210_attributes,
};

static int sis9210_sysfs_create_group(struct sis9210_data *sis9210, const struct attribute_group *group)
{
    int err;

    sis9210->kobj = kobject_create_and_add("wmtts", NULL) ;
    if(!sis9210->kobj){
        dbg_err("kobj create failed.\n");
        return -ENOMEM;
    }
    
    /* Register sysfs hooks */
	err = sysfs_create_group(sis9210->kobj, group);
	if (err < 0){
        kobject_del(sis9210->kobj);
		dbg_err("Create sysfs group failed!\n");
		return -ENOMEM;
	}

    return 0;
}

static void sis9210_sysfs_remove_group(struct sis9210_data *sis9210, const struct attribute_group *group)
{
    sysfs_remove_group(sis9210->kobj, group);
    kobject_del(sis9210->kobj);  
    return;
}

static int sis9210_probe(struct platform_device *pdev)
{
	int i;
    int err = 0;
	struct sis9210_data *sis9210 = platform_get_drvdata(pdev);
    
	INIT_DELAYED_WORK(&sis9210->read_work, sis9210_read_work);
    sis9210->workqueue = create_singlethread_workqueue(sis9210->name);
	if (!sis9210->workqueue) {
		err = -ESRCH;
		goto exit_create_singlethread;
	}
    
    err = sis9210_sysfs_create_group(sis9210, &sis9210_group);
    if(err < 0){
        dbg("create sysfs group failed.\n");
        goto exit_create_group;
    }
    
	sis9210->input_dev = input_allocate_device();
	if (!sis9210->input_dev) {
		err = -ENOMEM;
		dbg("failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}
	
	sis9210->input_dev->name = sis9210->name;
	sis9210->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	set_bit(INPUT_PROP_DIRECT, sis9210->input_dev->propbit);

	input_set_abs_params(sis9210->input_dev,
			     ABS_MT_POSITION_X, 0, 4095,/*sis9210->xresl*/ 0, 0);
	input_set_abs_params(sis9210->input_dev,
			     ABS_MT_POSITION_Y, 0, 4095,/*sis9210->yresl,*/ 0, 0);
    input_set_abs_params(sis9210->input_dev,
                 ABS_MT_TRACKING_ID, 0, 20, 0, 0);
#ifdef TOUCH_KEY 	
	for (i = 0; i <NUM_KEYS; i++)
		set_bit(virtual_keys[i], sis9210->input_dev->keybit);

	sis9210->input_dev->keycode = virtual_keys;
	sis9210->input_dev->keycodesize = sizeof(unsigned int);
	sis9210->input_dev->keycodemax = NUM_KEYS;
#endif

	err = input_register_device(sis9210->input_dev);
	if (err) {
		dbg_err("sis9210_ts_probe: failed to register input device.\n");
		goto exit_input_register_device_failed;
	}
        
#ifdef CONFIG_HAS_EARLYSUSPEND
	sis9210->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	sis9210->early_suspend.suspend = sis9210_early_suspend;
	sis9210->early_suspend.resume = sis9210_late_resume;
	register_early_suspend(&sis9210->early_suspend);
#endif
    
	if(request_irq(sis9210->irq, sis9210_isr_handler, IRQF_SHARED, sis9210->name, sis9210) < 0){
		dbg_err("Could not allocate irq for ts_sis9210 !\n");
		err = -1;
		goto exit_register_irq;
	}	
	
    gpio_enable(sis9210->igp_idx, sis9210->igp_bit, INPUT);
    gpio_pull_enable(sis9210->igp_idx, sis9210->igp_bit, PULL_UP);
    gpio_setup_irq(sis9210->igp_idx, sis9210->igp_bit, IRQ_FALLING);
    
    sis9210_reset(sis9210);
    
    return 0;
    
exit_register_irq:
	unregister_early_suspend(&sis9210->early_suspend);
exit_input_register_device_failed:
	input_free_device(sis9210->input_dev);
exit_input_dev_alloc_failed:
    sis9210_sysfs_remove_group(sis9210, &sis9210_group);
exit_create_group:
	cancel_delayed_work_sync(&sis9210->read_work);
	destroy_workqueue(sis9210->workqueue);  
exit_create_singlethread:
    //kfree(sis9210);
	return err;
}

static int sis9210_remove(struct platform_device *pdev)
{
    struct sis9210_data *sis9210 = platform_get_drvdata(pdev);

    cancel_delayed_work_sync(&sis9210->read_work);
    flush_workqueue(sis9210->workqueue);
	destroy_workqueue(sis9210->workqueue);
    
    free_irq(sis9210->irq, sis9210);
    gpio_disable_irq(sis9210->igp_idx, sis9210->igp_bit);
    
    unregister_early_suspend(&sis9210->early_suspend);
	input_unregister_device(sis9210->input_dev);

    sis9210_sysfs_remove_group(sis9210, &sis9210_group);
    //kfree(sis9210);
    
	dbg("remove...\n");
	return 0;
}

static void sis9210_release(struct device *device)
{
    return;
}

static struct platform_device sis9210_device = {
	.name  	    = DEV_SIS92X,
	.id       	= 0,
	.dev    	= {.release = sis9210_release},
};

static struct platform_driver sis9210_driver = {
	.driver = {
	           .name   	= DEV_SIS92X,
		       .owner	= THIS_MODULE,
	 },
	.probe    = sis9210_probe,
	.remove   = sis9210_remove,
};

static int check_touch_env(struct sis9210_data *sis9210)
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

	if(strncmp(p,"sis9210",7)) return -ENODEV;
    
	sis9210->name = DEV_SIS92X;
    	sis9210->addr = SIS92X_ADDR;
	p = strchr(p,':');
	p++;
	sscanf(p,"%d:%d:%d:%d:%d:%d",
        &sis9210->xresl, &sis9210->yresl,&sis9210->igp_idx,&sis9210->igp_bit,&sis9210->rgp_idx, &sis9210->rgp_bit);

	sis9210->irq = IRQ_GPIO;
	printk("%s reslx=%d, resly=%d, Interrupt GP%d, bit%d, Reset GP%d, bit%d\n", sis9210->name, 
        sis9210->xresl, sis9210->yresl, sis9210->igp_idx,sis9210->igp_bit,sis9210->rgp_idx,sis9210->rgp_bit);
    
	sis9210->vkey_idx = -1;
  
	return 0;
}

static int __init sis9210_init(void)
{
	int ret = -ENOMEM;
	struct sis9210_data *sis9210=NULL;

	sis9210 = kzalloc(sizeof(struct sis9210_data), GFP_KERNEL);
    if(!sis9210){
        dbg_err("mem alloc failed.\n");
        return -ENOMEM;
    }

    pContext = sis9210;
	ret = check_touch_env(sis9210);
    if(ret < 0)
        goto exit_free_mem;

	ret = platform_device_register(&sis9210_device);
	if(ret){
		dbg_err("register platform drivver failed!\n");
		goto exit_free_mem;
	}
	platform_set_drvdata(&sis9210_device, sis9210);
	
	ret = platform_driver_register(&sis9210_driver);
	if(ret){
		dbg_err("register platform device failed!\n");
		goto exit_unregister_pdev;
	}
	
	return ret;
	
exit_unregister_pdev:
	platform_device_unregister(&sis9210_device);
exit_free_mem:
    kfree(sis9210);
    pContext = NULL;
	return ret;
}

static void sis9210_exit(void)
{
    if(!pContext) return;
    
	platform_driver_unregister(&sis9210_driver);
	platform_device_unregister(&sis9210_device);
    kfree(pContext);

	return;
}

late_initcall(sis9210_init);
module_exit(sis9210_exit);

MODULE_DESCRIPTION("SiS 9210       Touchscreen Driver");
MODULE_LICENSE("GPL");

