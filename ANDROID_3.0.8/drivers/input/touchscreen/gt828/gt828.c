/* 
 * 
 * 2010 - 2012 Goodix Technology.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be a reference 
 * to you, when you are integrating the GOODiX's CTP IC into your system, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
 * General Public License for more details.
 * 
 * Version:1.2
 * Author:scott@goodix.com
 * Release Date:2012/06/08
 * Revision record:
 *      V1.0:2012/05/01,create file.
 *      V1.0:2012/06/08,add slot report mode.
 */

#include "gt828.h"

struct gtp_data *pContext=NULL;

#if GTP_HAVE_TOUCH_KEY
	static const u16 touch_key_array[] = GTP_KEY_TAB;
	#define GTP_MAX_KEY_NUM	 (sizeof(touch_key_array)/sizeof(touch_key_array[0]))
#endif

	
#ifdef CONFIG_HAS_EARLYSUSPEND
static void gtp_early_suspend(struct early_suspend *h);
static void gtp_late_resume(struct early_suspend *h);
#endif

extern int read_cfg_file(struct gtp_data *ts);

static int gtp_i2c_read(u8 *buf, int len)
{
    struct i2c_msg msgs[2];
    int ret=-1;

    msgs[0].flags = !I2C_M_RD;
    msgs[0].addr  = GOODIX_ADDR;
    msgs[0].len   = GTP_ADDR_LENGTH;
    msgs[0].buf   = &buf[0];

    msgs[1].flags = I2C_M_RD;
    msgs[1].addr  = GOODIX_ADDR;
    msgs[1].len   = len - GTP_ADDR_LENGTH;
    msgs[1].buf   = &buf[GTP_ADDR_LENGTH];

    ret = wmt_i2c_xfer_continue_if_4(msgs, 2, BUS_I2C1);
    if(ret < 0)
        dbg_err("IIC bus read error,ret %d\n",ret);
    
    return ret;
}

static int gtp_i2c_write(u8 *buf,int len)
{
    int ret=-1;
    struct i2c_msg msgs[1];

    msgs[0].flags = !I2C_M_RD;
    msgs[0].addr  = GOODIX_ADDR;
    msgs[0].len   = len;
    msgs[0].buf   = buf;

    ret = wmt_i2c_xfer_continue_if_4(msgs, 1, BUS_I2C1);
    if(ret < 0)
        dbg_err("IIC bus write error,ret %d\n",ret);
    
    return ret;
}

static int gtp_i2c_end_cmd(void)
{
    int ret = -1;
    u8 end_cmd_data[2]={0x80, 0x00}; 

    ret = gtp_i2c_write(end_cmd_data, 2);
    return ret;
}

static int gtp_send_cfg(struct gtp_data *ts)
{
    int ret = 0;
    int retry = 0;

    for (retry = 0; retry < 10; retry++){
        ret = gtp_i2c_write( ts->cfg_buf, ts->cfg_len);        
        gtp_i2c_end_cmd();

        if (ret > 0) break;
        
        msleep(10);
    }
    
    return ret;
}

static void gtp_work_func(struct work_struct *work)
{
    u8 point_data[2 + 2 + 5 * GTP_MAX_TOUCH + 1]={GTP_READ_COOR_ADDR >> 8, GTP_READ_COOR_ADDR & 0xFF};
    u8 check_sum = 0;
    u8 touch_num = 0;
    u8 finger = 0;
    u8 key_value = 0;
    u8* coor_data = NULL;
    int input_x = 0;
    int input_y = 0;
    //int input_w = 0;
    int idx = 0;
    int ret = -1;
    struct gtp_data *ts = container_of(work, struct gtp_data, work);

    ret = gtp_i2c_read( point_data, 10); 
    if (ret < 0){
        dbg_err("I2C transfer error. errno:%d\n ", ret);
        goto exit_work_func;
    }
	
    finger = point_data[GTP_ADDR_LENGTH];
    touch_num = (finger & 0x01) + !!(finger & 0x02) + !!(finger & 0x04) + !!(finger & 0x08) + !!(finger & 0x10);
    if (touch_num > 1){
        u8 buf[25] = {(GTP_READ_COOR_ADDR + 8) >> 8, (GTP_READ_COOR_ADDR + 8) & 0xff};

        ret = gtp_i2c_read(buf, 2 + 5 * (touch_num - 1)); 
        memcpy(&point_data[10], &buf[2], 5 * (touch_num - 1));
    }
    gtp_i2c_end_cmd();

    if((finger & 0xC0) != 0x80){
        dbg("Data not ready!");
        goto exit_work_func;
    }

    key_value = point_data[3]&0x0f; // 1, 2, 4, 8
    if ((key_value & 0x0f) == 0x0f){
        ret = gtp_send_cfg(ts);
        if (ret < 0){
            dbg("Reload config failed!\n");
        }
        goto exit_work_func;
    }

    coor_data = &point_data[4];
    check_sum = 0;
    for ( idx = 0; idx < 5 * touch_num; idx++){
        check_sum += coor_data[idx];
    }
    if (check_sum != coor_data[5 * touch_num]){
        dbg("Check sum error!");
        goto exit_work_func;
    }

#if GTP_HAVE_TOUCH_KEY
    for (idx = 0; idx < GTP_MAX_KEY_NUM; idx++){
        input_report_key(ts->input_dev, touch_key_array[idx], key_value & (0x01<<idx));   
    }
#endif

    dbg("finger:%02x, touch_num:%d.", finger,touch_num);

    if (touch_num){
        int pos = 0,x = 0,y = 0;
        for (idx = 0; idx < GTP_MAX_TOUCH; idx++){

            if (!(finger & (0x01 << idx))) continue;
            
            if(ts->swap){
                input_y = (coor_data[pos] << 8) | coor_data[pos + 1];
                input_x = (coor_data[pos + 2] << 8) | coor_data[pos + 3];
            }else{
                input_x = (coor_data[pos] << 8) | coor_data[pos + 1];
                input_y = (coor_data[pos + 2] << 8) | coor_data[pos + 3];
            }
            //input_w = coor_data[pos + 4];

            
            pos += 5;
            x = ts->xresl - input_x;
            y = input_y;  
            
            if(ts->dbg) printk("Nt=%d, ID:%d, input_X:%d, input_Y:%d, X:%d,Y:%d\n",touch_num, idx, input_x, input_y, x, y);

            input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
            input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
            input_mt_sync(ts->input_dev);
        }
        input_sync(ts->input_dev);
        ts->penup = 0;
    }

    if (!ts->penup && touch_num==0){
        dbg("Touch Release!");        
        ts->penup =1;
        input_mt_sync(ts->input_dev);
        input_sync(ts->input_dev);
    }

exit_work_func:
    gpio_enable_irq(ts->igp_idx, ts->igp_bit);

    return;
}


/*******************************************************
Function:
	External interrupt service routine.

Input:
	irq:	interrupt number.
	dev_id: private data pointer.
	
Output:
	irq execute status.
*******************************************************/
static irqreturn_t gtp_irq_handler(int irq, void *dev_id)
{
    struct gtp_data *ts = dev_id;

    if (!gpio_irq_isenable(ts->igp_idx, ts->igp_bit) ||!gpio_irq_state(ts->igp_idx, ts->igp_bit))
		return IRQ_NONE;

    gpio_disable_irq(ts->igp_idx, ts->igp_bit);
    if(!ts->earlysus) queue_work(ts->workqueue, &ts->work);

    return IRQ_HANDLED;
}

/*******************************************************
Function:
	Reset chip Function.

Input:
	ms:reset time.
	
Output:
	None.
*******************************************************/
static void gtp_reset_guitar(struct gtp_data *ts,int ms)
{
    gpio_enable(ts->rgp_idx, ts->rgp_bit, OUTPUT);
    gpio_set_value(ts->rgp_idx, ts->rgp_bit, LOW);
    msleep(ms);
    gpio_enable(ts->rgp_idx, ts->rgp_bit, INPUT);
    msleep(500);

    return;
}

/*******************************************************
Function:
	Wakeup from sleep mode Function.

Input:
	ts:	private data.
	
Output:
	Executive outcomes.0--success,non-0--fail.
*******************************************************/
static int gtp_wakeup_sleep(struct gtp_data * ts)
{
    u8 retry = 5;

    gtp_reset_guitar(ts,20);
    while(--retry && gtp_send_cfg(ts) < 0);

    if(retry) 
        return 0;
    else
        dbg_err("GTP wakeup sleep failed."); 

    return -1;
}

/*******************************************************
Function:
	GTP initialize function.

Input:
	ts:	i2c client private struct.
	
Output:
	Executive outcomes.0---succeed.
*******************************************************/
static int gtp_init_panel(struct gtp_data *ts)
{
    int ret = -1;

    ret = read_cfg_file(ts);
    if(ret){
        dbg_err("goodix read confiure file failed.\n");
        return ret;
    }
        
    ts->cfg_buf[RESOLUTION_LOC]     = (u8)(ts->xresl>>8);
    ts->cfg_buf[RESOLUTION_LOC + 1] = (u8)ts->xresl;
    ts->cfg_buf[RESOLUTION_LOC + 2] = (u8)(ts->yresl>>8);
    ts->cfg_buf[RESOLUTION_LOC + 3] = (u8)ts->yresl;
    ts->cfg_buf[TRIGGER_LOC] &= 0xf7; 

    ret = gtp_send_cfg(ts);
    if (ret < 0){
        dbg_err("Send config error.");
        return ret;
    }
    
    msleep(10);

    return 0;
}

/*******************************************************
Function:
	Read goodix touchscreen version function.

Input:
	client:	i2c client struct.
	version:address to store version info
	
Output:
	Executive outcomes.0---succeed.
*******************************************************/
static int gtp_read_version( u16* version)
{
    int ret = -1;
    u8 buf[8] = {GTP_REG_VERSION >> 8, GTP_REG_VERSION & 0xff};

    ret = gtp_i2c_read( buf, 6);
    gtp_i2c_end_cmd();
    if (ret < 0){
        dbg_err("GTP read version failed"); 
        return ret;
    }

    *version = (buf[3] << 8) | buf[4];
    printk("IC VERSION:%02x_%02x%02x\n", buf[2], buf[3], buf[4]);

    return ret;
}

static int gtp_request_gpio(struct gtp_data *ts)
{
    int ret = 0;

    gpio_enable(ts->igp_idx, ts->igp_bit, INPUT);	
    gpio_setup_irq(ts->igp_idx, ts->igp_bit, IRQ_FALLING);
    gpio_disable_irq(ts->igp_idx, ts->igp_bit);

    gtp_reset_guitar(ts,20);
    
    return ret;
}

static int gtp_request_irq(struct gtp_data *ts)
{
    int ret = -1;

    ret = request_irq(IRQ_GPIO, gtp_irq_handler, IRQF_SHARED, ts->name, ts);
    if (ret){
        dbg_err("Request IRQ failed!ERRNO:%d.", ret);
        return -1;
    }

    return 0; 
}

static int gtp_request_input(struct gtp_data *ts)
{
    int ret = -1;
#if GTP_HAVE_TOUCH_KEY
    u8 index = 0;
#endif
  
    ts->input_dev = input_allocate_device();
    if (ts->input_dev == NULL){
        dbg_err("Failed to allocate input device.");
        return -ENOMEM;
    }
   
    ts->input_dev->name = ts->name;
    ts->input_dev->phys = "Touchscreen";
    ts->input_dev->id.bustype = BUS_I2C;
    ts->input_dev->id.vendor = 0xDEAD;
    ts->input_dev->id.product = 0xBEEF;
    ts->input_dev->id.version = 10427;
	
    ts->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) ;
    set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);


#if GTP_HAVE_TOUCH_KEY
    for (index = 0; index < GTP_MAX_KEY_NUM; index++){
        input_set_capability(ts->input_dev,EV_KEY,touch_key_array[index]);	
    }
#endif

    input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->xresl, 0, 0);
    input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, ts->yresl, 0, 0);

    ret = input_register_device(ts->input_dev);
    if (ret){
        dbg_err("Register %s input device failed", ts->input_dev->name);
        return -ENODEV;
    }
    
#ifdef CONFIG_HAS_EARLYSUSPEND
    ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
    ts->early_suspend.suspend = gtp_early_suspend;
    ts->early_suspend.resume = gtp_late_resume;
    register_early_suspend(&ts->early_suspend);
#endif

    return 0;
}

static ssize_t cat_dbg(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct gtp_data *ts = pContext;

    sprintf(buf,"%d",ts->dbg);

    return strlen(buf);
}

static ssize_t echo_dbg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{  
    struct gtp_data *ts = pContext;

    sscanf(buf,"%d", &ts->dbg);
    
	return count;
}
static DEVICE_ATTR(dbg, S_IRUGO | S_IWUSR, cat_dbg, echo_dbg);

static struct attribute *goodix_attributes[] = {
    &dev_attr_dbg.attr,
	NULL
};

static const struct attribute_group goodix_group = {
	.attrs = goodix_attributes,
};

static int gtp_sysfs_create_group(struct gtp_data *ts, const struct attribute_group *group)
{
    int err;

    ts->kobj = kobject_create_and_add("wmtts", NULL) ;
    if(!ts->kobj){
        dbg_err("kobj create failed.\n");
        return -ENOMEM;
    }
    
    /* Register sysfs hooks */
	err = sysfs_create_group(ts->kobj, group);
	if (err < 0){
        kobject_del(ts->kobj);
		dbg_err("Create sysfs group failed!\n");
		return -ENOMEM;
	}

    return 0;
}

static void gtp_sysfs_remove_group(struct gtp_data *ts, const struct attribute_group *group)
{
    sysfs_remove_group(ts->kobj, group);
    kobject_del(ts->kobj);  
    return;
}

static int gtp_probe(struct platform_device *pdev)
{
    int ret = -1;
    struct gtp_data *ts = NULL;
    u16 version_info;

    ts = platform_get_drvdata(pdev);
    if(!ts){
        dbg_err("get NULL pointer.\n");
        return -EINVAL;
    }
    
    INIT_WORK(&ts->work, gtp_work_func);

    ret = gtp_sysfs_create_group(ts, &goodix_group);
    if(ret < 0){
        dbg_err("goodix create group failed.\n");
       goto exit_free_mem;
    }
        
    ts->workqueue= create_singlethread_workqueue(ts->name);
    if (!ts->workqueue){
        dbg_err("Creat workqueue failed.");
        ret = -ENOMEM;
        goto exit_free_group;
    }

    ret = gtp_request_input(ts);
    if (ret < 0){
        dbg_err("GTP request input dev failed");
        goto exit_free_workqueue;
    }
    
    ret = gtp_request_irq(ts); 
    if (ret < 0){
        dbg_err("GTP request irq failed.");
        goto exit_free_input;
    }
    
    ts->earlysus = 1;
    ret = gtp_request_gpio(ts);
    if (ret < 0){
        dbg_err("GTP request IO port failed.");
        goto exit_free_input;
    }
    
    ret = gtp_init_panel(ts);
    if (ret < 0){
        dbg_err("GTP init panel failed.");
        goto exit_free_irq;
    }

    ret = gtp_read_version(&version_info);
    if (ret < 0){
        dbg_err("Read version failed.");
        goto exit_free_irq;
    }
    
    ts->earlysus = 0;
    gpio_enable_irq(ts->igp_idx, ts->igp_bit);

    return 0;
    

exit_free_irq:
    free_irq(IRQ_GPIO, ts);
    if(ts->cfg_buf) {
        kfree(ts->cfg_buf);
        ts->cfg_buf = NULL;
    }
exit_free_input:
    input_unregister_device(ts->input_dev);
#ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&ts->early_suspend);
#endif
exit_free_workqueue:
   destroy_workqueue(ts->workqueue);
exit_free_group:
    gtp_sysfs_remove_group(ts,&goodix_group);
exit_free_mem:
    gpio_disable_irq(ts->igp_idx, ts->igp_bit);
    //kfree(ts);
    platform_set_drvdata(pdev,NULL);
    
    return ret;
    
}


static int gtp_remove(struct platform_device *pdev)
{
    struct gtp_data *ts = platform_get_drvdata(pdev);
	
    if (ts) {
	#ifdef CONFIG_HAS_EARLYSUSPEND
    	unregister_early_suspend(&ts->early_suspend);
	#endif
        gpio_disable_irq(ts->igp_idx, ts->igp_bit);
        free_irq(IRQ_GPIO, ts);
        destroy_workqueue(ts->workqueue);
        input_unregister_device(ts->input_dev);
        platform_set_drvdata(pdev, NULL);
        gtp_sysfs_remove_group(ts,&goodix_group);
        
        if(ts->cfg_buf) kfree(ts->cfg_buf);
        //kfree(ts);
    }	
  
    return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void gtp_early_suspend(struct early_suspend *h)
{
    struct gtp_data *ts;
    
    ts = container_of(h, struct gtp_data, early_suspend);
    ts->earlysus = 1;
    gpio_disable_irq(ts->igp_idx, ts->igp_bit);
    return;
}

static void gtp_late_resume(struct early_suspend *h)
{
    struct gtp_data *ts;
    int ret = -1;
    
    ts = container_of(h, struct gtp_data, early_suspend);
	ts->earlysus = 0;
    ret = gtp_wakeup_sleep(ts);
    if (ret < 0){
        dbg_err("GTP later resume failed.");
    }

    gpio_enable_irq(ts->igp_idx, ts->igp_bit);

    return;
}
#endif

static void gtp_release(struct device *device)
{
    return;
}

static struct platform_device gtp_device = {
	.name  	    = GTP_DEV_NAME,
	.id       	= 0,
	.dev    	= {.release = gtp_release},
};

static struct platform_driver gtp_driver = {
    .probe      = gtp_probe,
    .remove     = gtp_remove,
    .driver = {
        .name     = GTP_DEV_NAME,
        .owner    = THIS_MODULE,
    },
};

static int check_touch_env(struct gtp_data *ts)
{
	int len = 96;
	int Enable;
    char retval[96] = {0};
	char *p=NULL;

	if(wmt_getsyspara("wmt.io.touch", retval, &len)) 
        return -EIO;
    
	sscanf(retval,"%d:",&Enable);
	//check touch enable
	if(Enable == 0) return -ENODEV;
	
	p = strchr(retval,':');
	p++;
	if(strncmp(p,"gt813",5)==0){//check touch ID
		ts->id = GT813;
        ts->name = GT813_DEV;
	}else if(strncmp(p,"gt827_N3849B",12)==0){
        ts->id = GT827_N3849B;
        ts->name = GT827_DEV;
        sprintf(ts->cfg_name,"%s%s",GOODIX_CFG_PATH,"GT827_N3849B.cfg");
    }else if(strncmp(p,"gt828_L3816A",12)==0){
        ts->id = GT828_L3816A;
        ts->name = GT828_DEV;
        ts->swap = 1;
        sprintf(ts->cfg_name,"%s%s",GOODIX_CFG_PATH,"GT828_L3816A.cfg");
    }else if(strncmp(p,"gt828_L3765A",12)==0){
        ts->id = GT828_L3765A;
        ts->name = GT828_DEV;
        ts->swap = 0;
        sprintf(ts->cfg_name,"%s%s",GOODIX_CFG_PATH,"GT828_L3765A.cfg");
    }else{
        printk("Goodix touch disabled.\n");
        return -ENODEV;
    }
	
	p = strchr(p,':');
	p++;
	sscanf(p,"%d:%d:%d:%d:%d:%d",&ts->xresl, &ts->yresl,
        &ts->igp_idx, &ts->igp_bit,&ts->rgp_idx, &ts->rgp_bit);
    
	printk("%s reslx=%d, resly=%d, Interrupt GP%d, bit%d, Reset GP%d, bit%d\n", 
        ts->name, ts->xresl, ts->yresl, ts->igp_idx,ts->igp_bit, ts->rgp_idx, ts->rgp_bit);
    
	return 0;
}
static int __devinit gtp_init(void)
{
    int ret = -ENOMEM;
    struct gtp_data *ts = NULL;

    dbg("GTP driver install.");
    ts = kzalloc(sizeof(*ts), GFP_KERNEL);
    if (ts == NULL){
        dbg_err("Alloc GFP_KERNEL memory failed.");
        return -ENOMEM;
    }
	
    pContext = ts;
    ret = check_touch_env(ts);
    if(ret < 0){
        goto exit_release_mem;
    }    
    
    ret = platform_device_register(&gtp_device);
	if(ret){
		dbg_err("register platform drivver failed!\n");
		goto exit_release_mem;
	}
	platform_set_drvdata(&gtp_device, ts);

    ret = platform_driver_register(&gtp_driver);
	if(ret){
		dbg_err("register platform device failed!\n");
		goto exit_unregister_pdev;
	}
	
	return ret;
	
exit_unregister_pdev:
	platform_device_unregister(&gtp_device);
exit_release_mem:
    kfree(ts);
	pContext = NULL;
    return ret; 
}

static void __exit gtp_exit(void)
{
    dbg("GTP driver exited.");
    if(!pContext)return;
	
    platform_driver_unregister(&gtp_driver);
	platform_device_unregister(&gtp_device);
	kfree(pContext);
	
    return;
}

late_initcall(gtp_init);
module_exit(gtp_exit);

MODULE_DESCRIPTION("Goodix Series      Touchscreen Driver");
MODULE_LICENSE("GPL");
