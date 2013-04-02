
/***********************************************************************
*drivers/input/touchsrceen/novatek_touchdriver.c
*
*Novatek  nt1100x TouchScreen driver.
*
*Copyright(c) 2012 Novatek Ltd.
*
*VERSION              DATA               AUTHOR
*   1.1                   2012-02-27         LiuPeng
************************************************************************/
/************************************************************************
*DESCRIPTION:(only apply to under andriod 3.0  edition)
*Version 1.1    
*                    1, Add new iic protocol;
*                    2, Add Level trigger and Edge_Timer trigger for int trigger type 
*                    3, Add Calclate Traget baseline command ;
*Version 1.2   
*                    1, Add ts->use_irq = TS_INT;
*			  2, Add TouchKey MaxValue;
*
*
*
************************************************************************/

#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <mach/gpio-cs.h>
#include "novatek.h"


static struct novatek_data *pContext ;

static int i2c_read_bytes(struct novatek_data *novatek, uint8_t *buf, int len)
{
	struct i2c_msg msgs[2];
	int ret=-1;
	int retries = 0;

	msgs[0].flags=!I2C_M_RD;
	msgs[0].addr=novatek->addr;
	msgs[0].len=1;
	msgs[0].buf=&buf[0];

	msgs[1].flags=I2C_M_RD;
	msgs[1].addr=novatek->addr;
	msgs[1].len=len-1;
	msgs[1].buf=&buf[1];

	while(retries<5)
	{
		ret=wmt_i2c_xfer_continue_if_4(msgs, 2,I2C_BUS1);
		if(ret == 2)break;
		retries++;
	}
	return ret;
}


int i2c_write_bytes(struct novatek_data *novatek,uint8_t *data,int len)
{
	int ret=-1;
	int retries = 0;
    struct i2c_msg msg;


	msg.flags=!I2C_M_RD;
	msg.addr=novatek->addr;
	msg.len=len;
	msg.buf=data;		
	
	while(retries<5)
	{
		ret=wmt_i2c_xfer_continue_if_4(&msg, 1,I2C_BUS1);
		if(ret == 1)break;
		retries++;
	}
	return ret;
}

/*
static int novatek_set_rawdata(struct novatek_data *ts, int x,int y)
{
    ts->xraw = x;
    ts->yraw = y;

    return 0;
}
*/
/*******************************************************
Description:
	novatek touchscreen work function.

Parameter:
	ts:	i2c client private struct.
	
return:
	Executive outcomes.0---succeed.
*******************************************************/
static void novatek_ts_work(struct work_struct *work)
{
	int ret =-1;
	u8 data[61]={0};
	u8 touchKey = 0;
	u8 index,touch_num;
	u16 position = 0;
	u8 track_id[10] = {0};
    u8 id_status[10] = {0};
	int px = 0;
	int py = 0;
	int x,y;
    int value;
	
	struct novatek_data *ts = container_of(work, struct novatek_data,work);

	data[0] = READ_COOR_ADDR;
	ret = i2c_read_bytes(ts, data, sizeof(data)/sizeof(data[0]));
	if(ret != 2){
		dbg_err("Read point data failed !\n");
		gpio_enable_irq(ts->igp_idx,ts->igp_bit);
		return;
	}

/* Checking Virtual Key Touch Event */
#ifdef TOUCHKEY_EVENT
	touchKey = data[1] >> 3;
	if((touchKey > 20) && (touchKey < 26))
		goto TouchKey_Handle;
#endif
    
	touch_num = ts->nt;
	for(index = 0; index < ts->nt; index++){
		position = 1 + ts->nbyte* index;
		if(( data[position] & 0x03 ) == 0x03 )
			touch_num--;
        else
            id_status[index] = 1;
	}

	if(touch_num){
		for(index = 0; index < touch_num; index++){
            if(id_status[index] == 1){
    		    position = 1 + ts->nbyte * index;
    		#if (IC_DEFINE == NT11003)
    			track_id[index] = (unsigned int)(data[position] >> 3) - 1;
    		#elif (IC_DEFINE == NT11002)
    			track_id[index] = (unsigned int)(data[position] >> 4) - 1;
    		#endif				

                x = (int)(data[position+1]<<4) + (int) (data[position+3]>>4);
    			y = (int)(data[position+2]<<4) + (int) (data[position+3]&0x0f);

    			x = x *ts->resly/ts->tw;	
    			y = y *ts->reslx/ts->th;
    			
    			if((x > ts->resly)||(y > ts->reslx))continue;

                px = ts->reslx -y;
                py = x;

    			ts->penup = 0;
    			//printk("x = %d,y = %d; px=%d, py=%d\n", x, y,px,py);
    			
    			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, px);
    			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, py);			
    			input_mt_sync(ts->input_dev);

            }
	    }
	    input_sync(ts->input_dev);
	    dbg("pen down...\n");			  			
    }
    else if (!ts->penup){
		input_mt_sync(ts->input_dev);
		input_sync(ts->input_dev);
		ts->penup = 1;;
		dbg("pen up...\n");
	}
	gpio_enable_irq(ts->igp_idx,ts->igp_bit);

    return;

TouchKey_Handle:
#ifdef TOUCHKEY_EVENT
	value = (unsigned int)(data[1] >> 3);
	switch(value)
	{
		case MENU:
	    	input_report_key(ts->input_dev, KEY_MENU, data[1]&0x01);
	    	break;
		case HOME:
		   	input_report_key(ts->input_dev, KEY_HOME, data[1]&0x01);
			break;			
		case BACK:
		   	input_report_key(ts->input_dev, KEY_BACK, data[1]&0x01);
			break;
		case SEARCH:
		   	input_report_key(ts->input_dev, KEY_SEARCH, data[1]&0x01);
			break;
		default:
			break;
	}
	input_sync(ts->input_dev);
#endif
	gpio_enable_irq(ts->igp_idx,ts->igp_bit);

    return;
			
}

/*******************************************************
Description:
	External interrupt service routine.

Parameter:
	irq:	interrupt number.
	dev_id: private data pointer.
	
return:
	irq execute status.
*******************************************************/
static irqreturn_t novatek_ts_isr(int irq, void *dev_id)
{
	struct novatek_data *ts = dev_id;

	if (gpio_irq_isenable(ts->igp_idx,ts->igp_bit) && 
        gpio_irq_state(ts->igp_idx,ts->igp_bit)){
		dbg("begin..\n");
		gpio_disable_irq(ts->igp_idx,ts->igp_bit);
		if(!ts->earlysus) queue_work(ts->workqueue, &ts->work);
		return IRQ_HANDLED;
	}
	
	return IRQ_NONE;
}

static int novatek_set_irq_gpio(struct novatek_data *nova)
{
    gpio_enable(nova->igp_idx,nova->igp_bit,INPUT);
    gpio_pull_enable(nova->igp_idx,nova->igp_bit,PULL_UP);
    gpio_setup_irq(nova->igp_idx,nova->igp_bit,IRQ_FALLING);

    return 0;
}

static int novatek_set_rst_gpio(struct novatek_data *nova)
{
    gpio_enable(nova->rgp_idx,nova->rgp_bit,OUTPUT);

    gpio_set_value(nova->rgp_idx,nova->rgp_bit,HIGH);
    msleep(3);
    gpio_set_value(nova->rgp_idx,nova->rgp_bit,LOW);
    msleep(3);
    gpio_set_value(nova->rgp_idx,nova->rgp_bit,HIGH);

    return 0;
}

static void novatek_early_suspend(struct early_suspend *handler)
{
    struct novatek_data *nova = container_of(handler, struct novatek_data, esuspend);
    nova->earlysus = 1;
	gpio_disable_irq(nova->igp_idx,nova->igp_bit);
    cancel_work_sync(&nova->work);
    
    return;
}

static void novatek_late_resume(struct early_suspend *handler)
{
	struct novatek_data *nova = container_of(handler, struct novatek_data, esuspend);
	nova->earlysus = 0;
	novatek_set_irq_gpio(nova);
    novatek_set_rst_gpio(nova);
    
    return;
}


static int novatek_ts_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct novatek_data *ts = NULL;
#ifdef TOUCHKEY_EVENT
    int i;
    int key_codes[MAX_KEY_NUM]={KEY_MENU,KEY_HOME,KEY_BACK,KEY_SEARCH};
#endif
								
	dbg("Install novatek touch driver.\n");
    ts = platform_get_drvdata(pdev);
    if(!ts){
        dbg_err("Memery Error.\n");
        return -EFAULT;
    }

	INIT_WORK(&ts->work,novatek_ts_work);
	ts->workqueue = create_singlethread_workqueue(ts->name);
	if (!ts->workqueue) {
		ret = -ESRCH;
		goto exit_singlethread;
	}

	ret = request_irq(ts->irq, novatek_ts_isr , IRQF_SHARED, ts->name, ts);
	if(ret != 0) {
		dbg_err("Novatek ts probe: request irq failed.\n");
		goto exit_irq_request_failed;
	}
	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		dbg_err("Failed to allocate input device\n");
		goto err_input_dev_alloc_failed;
	}
	
	ts->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	ts->input_dev->propbit[0] = BIT_MASK(INPUT_PROP_DIRECT);

#ifdef TOUCHKEY_EVENT
	for(i = 0; i < MAX_KEY_NUM; i++)
		input_set_capability(ts->input_dev,EV_KEY,key_codes[i]);	
#endif
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->reslx, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, ts->resly, 0, 0);
	ts->input_dev->name = ts->name;
	ts->input_dev->id.bustype = BUS_I2C;
	ts->input_dev->id.vendor = 0xDEAD;
	ts->input_dev->id.product = 0xBEEF;
	ts->input_dev->id.version = 20105;//screen firmware version
	ret = input_register_device(ts->input_dev);
	if (ret != 0) {
		dbg_err("Probe: unable to register %s input device\n", ts->input_dev->name);
		goto err_input_register_device_failed;
	}

	ts->esuspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->esuspend.suspend = novatek_early_suspend;
	ts->esuspend.resume = novatek_late_resume;
	register_early_suspend(&ts->esuspend);

    novatek_set_irq_gpio(ts);
    novatek_set_rst_gpio(ts);
    
	return 0;  
	
err_input_register_device_failed:
	input_free_device(ts->input_dev);
err_input_dev_alloc_failed:
	free_irq(ts->irq, ts);	
exit_irq_request_failed:
	cancel_work_sync(&ts->work);
	destroy_workqueue(ts->workqueue);
exit_singlethread:
	kfree(ts);	
	return ret;	
}

static int novatek_ts_remove(struct platform_device *pdev)
{
	struct novatek_data *ts = platform_get_drvdata(pdev);
    
    dbg("The driver is removing...\n");
    gpio_disable_irq(ts->igp_idx, ts->igp_bit);

    cancel_work_sync(&ts->work);
    destroy_workqueue(ts->workqueue);
    
    input_unregister_device(ts->input_dev);
	unregister_early_suspend(&ts->esuspend);
    platform_set_drvdata(pdev, NULL);

    free_irq(ts->irq, ts);
    kfree(ts);

	return 0;
}

static void novatek_release(struct device *device)
{
    return;
}

static struct platform_device novatek_device = {
	.name  	    = DEV_NOVATEK,
	.id       	= 0,
	.dev    	= {.release = novatek_release},
};

static struct platform_driver novatek_driver = {
	.driver = {
	           .name   	= DEV_NOVATEK,
		       .owner	= THIS_MODULE,
	 },
	.probe    = novatek_ts_probe,
	.remove   = novatek_ts_remove,

};

static int check_touch_env(struct novatek_data *novatek)
{
	int ret = 0;
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
	if(strncmp(p,"ntk02",5)==0){//check touch ID
		novatek->id = NT11002;
        novatek->name = DEV_NT11002;
        novatek->addr = NT11002_ADDR;
	}else if(strncmp(p,"ntk03",5)==0){
        novatek->id = NT11003;
        novatek->name = DEV_NT11003;
        novatek->addr = NT11003_ADDR;
    }else if(strncmp(p,"ntk04",5)==0){
        novatek->id = NT11004;
        novatek->name = DEV_NT11004;
        novatek->addr = NT11004_ADDR;
    }else{
        printk("NovaTek touch disabled.\n");
        return -ENODEV;
    }
	
	p = strchr(p,':');
	p++;
	sscanf(p,"%d:%d:%d:%d:%d:%d",&novatek->reslx, &novatek->resly,
        &novatek->igp_idx, &novatek->igp_bit,&novatek->rgp_idx, &novatek->rgp_bit);

    novatek->irq = IRQ_GPIO;
	printk("%s reslx=%d, resly=%d, Interrupt GP%d bit%d, Reset GP%d bit%d\n", novatek->name, novatek->reslx, 
        novatek->resly, novatek->igp_idx,novatek->igp_bit,novatek->rgp_idx, novatek->rgp_bit);

    ret = wmt_getsyspara("wmt.io.nova", retval, &len);
	if(ret){//default data is for 800x480
        novatek->nt = 5;
        novatek->nbyte = 6;
		novatek->tw = 800;
        novatek->th = 480;
		//return -EIO;
	}else{
        sscanf(retval,"%d:%d:%d:%d",&novatek->nt, &novatek->nbyte,&novatek->tw,&novatek->th);
    }
        
	return 0;
}

static int __init novatek_init(void)
{
	int ret = 0;
    struct novatek_data *novatek = NULL;

    novatek = kzalloc(sizeof(struct novatek_data), GFP_KERNEL);
    if(!novatek){
        dbg_err("Mem Alloc Error.\n");
        return -ENOMEM;
    }
    
	if(check_touch_env(novatek)) return -EIO;
    
    pContext = novatek;
    
	ret = platform_device_register(&novatek_device);
	if(ret){
		dbg_err("register platform drivver failed!\n");
        kfree(novatek);
		return ret;
	}
    platform_set_drvdata(&novatek_device, novatek);
	
	ret = platform_driver_register(&novatek_driver);
	if(ret){
		dbg_err("register platform device failed!\n");
		goto exit_unregister_pdev;
	}
	
	return ret;
	
exit_unregister_pdev:
	platform_device_unregister(&novatek_device);
    kfree(novatek);
	return ret;
}

static void novatek_exit(void)
{
	platform_driver_unregister(&novatek_driver);
	platform_device_unregister(&novatek_device);

	return;
}

late_initcall(novatek_init);
module_exit(novatek_exit);

MODULE_DESCRIPTION("Novatek Touchscreen Driver");
MODULE_LICENSE("GPL");
