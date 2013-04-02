#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/earlysuspend.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <mach/hardware.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/wait.h>
#include <asm/uaccess.h>
#include <mach/gpio-cs.h>
#include <linux/slab.h>
#include "cyttsp2.h"

struct cyttsp2_data *pContext=NULL;

static int cyttsp2_i2c_rxdata(struct cyttsp2_data *cyttsp2, u8 *rxdata, int length)
{
	int ret;
	struct i2c_msg msg[2];

    msg[0].addr = cyttsp2->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = rxdata;
	
	msg[1].addr = cyttsp2->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = length;
	msg[1].buf = rxdata;

	ret = wmt_i2c_xfer_continue_if_4(msg, 2, I2C_BUS1);
	if (ret <= 0)
		dbg_err("msg i2c read error: %d\n", ret);
	
	return ret;
}


static int cyttsp2_i2c_txdata(struct cyttsp2_data *cyttsp2 ,char *txdata, int length)
{
	int ret;
	struct i2c_msg msg[1];

    msg[0].addr = cyttsp2->addr;
	msg[0].flags = 0;
	msg[0].len = length;
	msg[0].buf = txdata;

	ret = wmt_i2c_xfer_continue_if_4(msg, 1, I2C_BUS1);
	if (ret <= 0)
		dbg_err("msg i2c read error: %d\n", ret);
	
	return ret;
}
#define BUF_MAX  16
#define BUF_LEN  8
struct dual_avg_buf {
    int num;
    u32 data[BUF_MAX];
};

static struct dual_avg_buf x1_buf,y1_buf,x2_buf,y2_buf;

static inline void dual_buf_fill(struct dual_avg_buf *dual_buf, u32 data, int len)
{
    dual_buf->data[dual_buf->num % len] = data;
    dual_buf->num++;

    return ;
}

static inline u32 dual_buf_avg(struct dual_avg_buf *dual_buf, int len)
{
    int i, num;
    u32 avg = 0;
    int max, min;

    num = (dual_buf->num < len)? dual_buf->num : len;
	
    if(num == 1)
		return dual_buf->data[0];
    if(num == 2)
		return (dual_buf->data[0]+dual_buf->data[1])/2;

    max = dual_buf->data[0];
    min = dual_buf->data[0];
    for (i = 0; i < num; i++){
        avg += dual_buf->data[i];
		
	  if(dual_buf->data[i] > max)
	  	max = dual_buf->data[i];

	  if(dual_buf->data[i] < min)
	  	min = dual_buf->data[i];
    }

    return (avg-max-min )/ (num-2);
}

static void dual_buf_init(struct dual_avg_buf *dual_buf)
{
    memset(dual_buf, 0x00, sizeof(struct dual_avg_buf));
    return ;
}
static void cyttsp2_read_work(struct work_struct *work)
{
    int retval;
	u8 xy_data[20]={0x00};
    u8 clr_cmd[2] = {0x07,0x01};
	u8 num_tch = 0;
    struct cyttsp2_data *cyttsp2 = container_of(work, struct cyttsp2_data, read_work.work);


	retval = cyttsp2_i2c_rxdata(cyttsp2,xy_data,sizeof(xy_data));
	if (retval < 0) {
		pr_err("%s: cyttsp2 read i2c data failed.\n", __func__);
		goto _cyttsp2_worker_exit;
	}
    
    cyttsp2_i2c_txdata(cyttsp2,clr_cmd,sizeof(clr_cmd));
    
	/* determine number of currently active touches */
	num_tch = xy_data[0x11];

	/* extract xy_data for all currently reported touches */
	if (num_tch == 1) {
		int x = (xy_data[9] <<8) | xy_data[10];
		int y = (xy_data[11] <<8) | xy_data[12];
        int px = cyttsp2->xresl -y;
        int py = x;         
                
		input_report_abs(cyttsp2->input_dev, ABS_MT_POSITION_X, px);
		input_report_abs(cyttsp2->input_dev, ABS_MT_POSITION_Y, py);
		input_mt_sync(cyttsp2->input_dev);
        input_sync(cyttsp2->input_dev);
        cyttsp2->t_cnt = 0;
		if(cyttsp2->dbg) printk("num_tch=%d, x=%-4d y=%-4d; px=%-4d,py=%-4d\n", num_tch,  x, y, px, py);		
	}
    else if(num_tch == 2){
		int x1 = (xy_data[9] <<8) | xy_data[10];
		int y1 = (xy_data[11] <<8) | xy_data[12];
        
        int x2 = (xy_data[9+4] <<8) | xy_data[10+4];
		int y2 = (xy_data[11+4] <<8) | xy_data[12+4];
        
        int px1 = cyttsp2->xresl -y1;
        int py1 = x1;

        int px2 = cyttsp2->xresl -y2;
        int py2 = x2;

        if(!cyttsp2->t_cnt ) {
            cyttsp2->t_cnt++;
            dual_buf_init(&x1_buf);
            dual_buf_init(&y1_buf);
            dual_buf_init(&x2_buf);
            dual_buf_init(&y2_buf); 
        }

        dual_buf_fill(&x1_buf, px1, BUF_LEN);
        dual_buf_fill(&y1_buf, py1, BUF_LEN);
        dual_buf_fill(&x2_buf, px2, BUF_LEN);
        dual_buf_fill(&y2_buf, py2, BUF_LEN); 

        px1 = dual_buf_avg(&x1_buf, BUF_LEN);
        py1 = dual_buf_avg(&y1_buf, BUF_LEN);
        px2 = dual_buf_avg(&x2_buf, BUF_LEN);
        py2 = dual_buf_avg(&y2_buf, BUF_LEN);
                
		input_report_abs(cyttsp2->input_dev, ABS_MT_POSITION_X, px1);
		input_report_abs(cyttsp2->input_dev, ABS_MT_POSITION_Y, py1);
		input_mt_sync(cyttsp2->input_dev);
        
        input_report_abs(cyttsp2->input_dev, ABS_MT_POSITION_X, px2);
		input_report_abs(cyttsp2->input_dev, ABS_MT_POSITION_Y, py2);
		input_mt_sync(cyttsp2->input_dev);
        
		input_sync(cyttsp2->input_dev);
    }
    else {
        //printk("get zero touch.\n");
		input_mt_sync(cyttsp2->input_dev);
		input_sync(cyttsp2->input_dev);

        dual_buf_init(&x1_buf);
        dual_buf_init(&y1_buf);
        dual_buf_init(&x2_buf);
        dual_buf_init(&y2_buf); 
        cyttsp2->t_cnt = 0;
        if(!cyttsp2->earlysus) gpio_enable_irq(cyttsp2->igp_idx, cyttsp2->igp_bit);

        return;
	}  
    
_cyttsp2_worker_exit:    
    if(!cyttsp2->earlysus) queue_delayed_work(cyttsp2->workqueue, &cyttsp2->read_work, 20*HZ/1000);
    return;
}

static irqreturn_t cyttsp2_interrupt(int irq, void *dev)
{
    struct cyttsp2_data *cyttsp2 = dev;

    if (!gpio_irq_isenable(cyttsp2->igp_idx, cyttsp2->igp_bit) ||
        !gpio_irq_state(cyttsp2->igp_idx, cyttsp2->igp_bit))
		return IRQ_NONE;
    
    gpio_disable_irq(cyttsp2->igp_idx, cyttsp2->igp_bit);
	if(!cyttsp2->earlysus) queue_delayed_work(cyttsp2->workqueue, &cyttsp2->read_work,0);
		
	return IRQ_HANDLED;
}

static void cyttsp2_reset(struct cyttsp2_data *cyttsp2)
{
    gpio_enable(cyttsp2->igp_idx, cyttsp2->igp_bit, INPUT);
    gpio_pull_enable(cyttsp2->igp_idx, cyttsp2->igp_bit, PULL_UP);
    gpio_setup_irq(cyttsp2->igp_idx, cyttsp2->igp_bit, IRQ_FALLING);

    gpio_enable(cyttsp2->rgp_idx, cyttsp2->rgp_bit, OUTPUT);
	gpio_set_value(cyttsp2->rgp_idx, cyttsp2->rgp_bit,0);

	msleep(3);
	gpio_set_value(cyttsp2->rgp_idx, cyttsp2->rgp_bit,1);
	msleep(3);
	gpio_set_value(cyttsp2->rgp_idx, cyttsp2->rgp_bit,0);
	msleep(80);

    return;
}
	
static int cyttsp2_init_panel(struct cyttsp2_data *cyttsp2)
{
	int ret=0;
    u8 pwr_cmd[2] = {0x04,0x09};

    cyttsp2->addr = 0x05;
   
    /* disable auto power mode */    
    ret = cyttsp2_i2c_txdata(cyttsp2, pwr_cmd, sizeof(pwr_cmd));
    if(ret <= 0)
        printk("soft reset failed.\n");
    
	return ret;	
}     

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cyttsp2_early_suspend(struct early_suspend *handler)
{
    struct cyttsp2_data *cyttsp2 = container_of(handler, struct cyttsp2_data, early_suspend);
    cyttsp2->earlysus = 1;
	gpio_disable_irq(cyttsp2->igp_idx, cyttsp2->igp_bit);
    return;
}

static void cyttsp2_late_resume(struct early_suspend *handler)
{
    struct cyttsp2_data *cyttsp2 = container_of(handler, struct cyttsp2_data, early_suspend);

    cyttsp2_reset(cyttsp2);
    cyttsp2_init_panel(cyttsp2);
    cyttsp2->earlysus = 0;    
    gpio_enable_irq(cyttsp2->igp_idx, cyttsp2->igp_bit);
    return;
}
#endif  //CONFIG_HAS_EARLYSUSPEND

static ssize_t cat_dbg(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "dbg \n");
}

static ssize_t echo_dbg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct cyttsp2_data *cyttsp2 = pContext;

	sscanf(buf, "%d",&cyttsp2->dbg);
        
	return count;
}
static DEVICE_ATTR(dbg, S_IRUGO | S_IWUSR, cat_dbg, echo_dbg);

static struct attribute *cyttsp2_attributes[] = {
    &dev_attr_dbg.attr,
	NULL
};

static const struct attribute_group cyttsp2_group = {
	.attrs = cyttsp2_attributes,
};

static int cyttsp2_sysfs_create_group(struct cyttsp2_data *cyttsp2, const struct attribute_group *group)
{
    int err;

    cyttsp2->kobj = kobject_create_and_add("wmtts", NULL) ;
    if(!cyttsp2->kobj){
        dbg_err("kobj create failed.\n");
        return -ENOMEM;
    }
    
    /* Register sysfs hooks */
	err = sysfs_create_group(cyttsp2->kobj, group);
	if (err < 0){
        kobject_del(cyttsp2->kobj);
		dbg_err("Create sysfs group failed!\n");
		return -ENOMEM;
	}

    return 0;
}

static void cyttsp2_sysfs_remove_group(struct cyttsp2_data *cyttsp2, const struct attribute_group *group)
{
    sysfs_remove_group(cyttsp2->kobj, group);
    kobject_del(cyttsp2->kobj);  
    return;
}
static int cyttsp2_probe(struct platform_device *pdev)
{
	int err = 0;
	struct cyttsp2_data *cyttsp2 = platform_get_drvdata(pdev);
    
	INIT_DELAYED_WORK(&cyttsp2->read_work, cyttsp2_read_work);
    err = cyttsp2_sysfs_create_group(cyttsp2, &cyttsp2_group);
    if(err < 0){
        dbg_err("sysfs create failed.\n");
        return err;
    }
    
    cyttsp2->workqueue = create_singlethread_workqueue(cyttsp2->name);
	if (!cyttsp2->workqueue) {
		err = -ESRCH;
		goto exit_create_singlethread;
	}
    
	cyttsp2->input_dev = input_allocate_device();
	if (!cyttsp2->input_dev) {
		err = -ENOMEM;
		dbg("failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}
	
	cyttsp2->input_dev->name = cyttsp2->name;
	cyttsp2->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	set_bit(INPUT_PROP_DIRECT, cyttsp2->input_dev->propbit);

	input_set_abs_params(cyttsp2->input_dev, ABS_MT_POSITION_X, 0, cyttsp2->xresl, 0, 0);
	input_set_abs_params(cyttsp2->input_dev, ABS_MT_POSITION_Y, 0, cyttsp2->yresl, 0, 0);

	err = input_register_device(cyttsp2->input_dev);
	if (err) {
		dbg_err("cyttsp2_ts_probe: failed to register input device.\n");
		goto exit_input_register_device_failed;
	}
        
#ifdef CONFIG_HAS_EARLYSUSPEND
	cyttsp2->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	cyttsp2->early_suspend.suspend = cyttsp2_early_suspend;
	cyttsp2->early_suspend.resume = cyttsp2_late_resume;
	register_early_suspend(&cyttsp2->early_suspend);
#endif
    
	if(request_irq(cyttsp2->irq, cyttsp2_interrupt, IRQF_SHARED, cyttsp2->name, cyttsp2) < 0){
		dbg_err("Could not allocate irq for ts_cyttsp2 !\n");
		err = -1;
		goto exit_register_irq;
	}	
	cyttsp2->earlysus = 1;
    
    cyttsp2_reset(cyttsp2);
    cyttsp2_init_panel(cyttsp2);

    cyttsp2->earlysus = 0;
    gpio_enable_irq(cyttsp2->igp_idx, cyttsp2->igp_bit);
    
    return 0;
    
exit_register_irq:
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&cyttsp2->early_suspend);
#endif
exit_input_register_device_failed:
	input_free_device(cyttsp2->input_dev);
exit_input_dev_alloc_failed:
	cancel_delayed_work_sync(&cyttsp2->read_work);
	destroy_workqueue(cyttsp2->workqueue);  
exit_create_singlethread:
    cyttsp2_sysfs_remove_group(cyttsp2,&cyttsp2_group);
	return err;
}

static int cyttsp2_remove(struct platform_device *pdev)
{
    struct cyttsp2_data *cyttsp2 = dev_get_drvdata(&pdev->dev);

    cancel_delayed_work_sync(&cyttsp2->read_work);
	destroy_workqueue(cyttsp2->workqueue);
    cyttsp2_sysfs_remove_group(cyttsp2,&cyttsp2_group);
    
    free_irq(cyttsp2->irq, cyttsp2);
    gpio_disable_irq(cyttsp2->igp_idx, cyttsp2->igp_bit);
#ifdef CONFIG_HAS_EARLYSUSPEND    
    unregister_early_suspend(&cyttsp2->early_suspend);
#endif
	input_unregister_device(cyttsp2->input_dev);
    
	dbg("remove...\n");
	return 0;
}

static void cyttsp2_release(struct device *device)
{
    return;
}

static struct platform_device cyttsp2_device = {
	.name  	    = DEV_cyttsp2,
	.id       	= 0,
	.dev    	= {.release = cyttsp2_release},
};

static struct platform_driver cyttsp2_driver = {
	.driver = {
	           .name   	= DEV_cyttsp2,
		       .owner	= THIS_MODULE,
	 },
	.probe    = cyttsp2_probe,
	.remove   = cyttsp2_remove,
};


static int check_touch_env(struct cyttsp2_data *cyttsp2)
{
	int ret = 0;
	int len = 96;
	int Enable;
    char retval[96] = {0};
	char *p=NULL;

    // Get u-boot parameter
	ret = wmt_getsyspara("wmt.io.touch", retval, &len);
	if(ret) return -EIO;
    
	sscanf(retval,"%d:",&Enable);
	//check touch enable
	if(Enable == 0) return -ENODEV;
	
	p = strchr(retval,':');
	p++;
	if(strncmp(p,"cyp120",6)==0){//check touch ID
        cyttsp2->name = DEV_cyttsp2;
    }else{
        printk("cyttsp2 touch disabled.\n");
        return -ENODEV;
    }
	
	p = strchr(p,':');
	p++;
	sscanf(p,"%d:%d:%d:%d:%d:%d",
        &cyttsp2->xresl, &cyttsp2->yresl,&cyttsp2->igp_idx,&cyttsp2->igp_bit,&cyttsp2->rgp_idx, &cyttsp2->rgp_bit);

    cyttsp2->irq = IRQ_GPIO;
	printk("%s xresl=%d, yresl=%d, Interrupt GP%d, bit%d, Reset GP%d, bit%d\n", cyttsp2->name, 
        cyttsp2->xresl, cyttsp2->yresl, cyttsp2->igp_idx,cyttsp2->igp_bit,cyttsp2->rgp_idx,cyttsp2->rgp_bit);

	return 0;
}

static int __init cyttsp2_init(void)
{
	int ret = -ENOMEM;
    struct cyttsp2_data *cyttsp2=NULL;

    cyttsp2 = kzalloc(sizeof(struct cyttsp2_data), GFP_KERNEL);
    if(!cyttsp2){
        dbg_err("mem alloc failed.\n");
        return -ENOMEM;
    }

    pContext = cyttsp2;
    ret = check_touch_env(cyttsp2);
    if(ret < 0)
        goto exit_free_mem;

	ret = platform_device_register(&cyttsp2_device);
	if(ret){
		dbg_err("register platform drivver failed!\n");
		goto exit_free_mem;
	}
    platform_set_drvdata(&cyttsp2_device, cyttsp2);
    
	ret = platform_driver_register(&cyttsp2_driver);
	if(ret){
		dbg_err("register platform device failed!\n");
		goto exit_unregister_pdev;
	}
    
	return ret;
    
exit_unregister_pdev:
	platform_device_unregister(&cyttsp2_device);
exit_free_mem:
    kfree(cyttsp2);
    pContext = NULL;
	return ret;
}

static void cyttsp2_exit(void)
{
	if(!pContext) return;
    
	platform_driver_unregister(&cyttsp2_driver);
	platform_device_unregister(&cyttsp2_device);
    kfree(pContext);

	return;
}

late_initcall(cyttsp2_init);
module_exit(cyttsp2_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("FocalTech.Touch");
