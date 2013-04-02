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
#include "cyttsp4.h"

struct cyttsp4_data *pContext=NULL;

static int cyttsp4_i2c_rxdata(struct cyttsp4_data *cyttsp4, u8 *rxdata, int length)
{
	int ret;
	struct i2c_msg msg[2];

    msg[0].addr = cyttsp4->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = rxdata;
	
	msg[1].addr = cyttsp4->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = length;
	msg[1].buf = rxdata;

	ret = wmt_i2c_xfer_continue_if_4(msg, 2, I2C_BUS1);
	if (ret <= 0)
		dbg_err("msg i2c read error: %d\n", ret);
	
	return ret;
}


static int cyttsp4_i2c_txdata(struct cyttsp4_data *cyttsp4 ,char *txdata, int length)
{
	int ret;
	struct i2c_msg msg[1];

    msg[0].addr = cyttsp4->addr;
	msg[0].flags = 0;
	msg[0].len = length;
	msg[0].buf = txdata;

	ret = wmt_i2c_xfer_continue_if_4(msg, 1, I2C_BUS1);
	if (ret <= 0)
		dbg_err("msg i2c read error: %d\n", ret);
	
	return ret;
}

static void cyttsp4_read_work(struct work_struct *work)
{
    struct cyttsp4_data *cyttsp4 = container_of(work, struct cyttsp4_data, read_work);
	struct cyttsp4_xydata xy_data;
	u8 num_cur_tch = 0;
	int i;
	int retval;

	/*
	 * Get event data from CYTTSP device.
	 * The event data includes all data
	 * for all active touches.
	 */
	memset(&xy_data, 0, sizeof(xy_data));
	retval = cyttsp4_i2c_rxdata(cyttsp4,(u8*)&xy_data,sizeof(xy_data));
	if (retval < 0) {
		/* I2C bus failure implies bootloader running */
		pr_err("%s: cyttsp read i2c data failed.\n", __func__);
		goto _cyttsp4_worker_exit;
	}

	/* determine number of currently active touches */
	num_cur_tch = GET_NUM_TOUCHES(xy_data.tt_stat);

    if (IS_LARGE_AREA(xy_data.tt_stat) == 1) {
		/* terminate all active tracks */
		num_cur_tch = 0;
		pr_err("%s: Large area detected\n", __func__);
		goto _cyttsp4_worker_exit;
	} else if (num_cur_tch > CY_NUM_TCH_ID) {
		if (num_cur_tch == 0x1F) {
			/* terminate all active tracks */
			pr_err("%s: Num touch err detected (n=%d)\n", __func__, num_cur_tch);
			num_cur_tch = 0;
		} else {
			pr_err("%s: too many tch; set to max tch (n=%d c=%d)\n", __func__, num_cur_tch, CY_NUM_TCH_ID);
			num_cur_tch = CY_NUM_TCH_ID;
		}
	} else if (IS_BAD_PKT(xy_data.rep_stat)) {
		/* terminate all active tracks */
		num_cur_tch = 0;
		pr_err("%s: Invalid buffer detected\n", __func__);
	}

	/* extract xy_data for all currently reported touches */
	if (num_cur_tch) {
		for (i = 0; i < num_cur_tch; i++) {
			struct cyttsp4_touch *tch = &xy_data.tch[i];

			int e = CY_GET_EVENTID(tch->t);
			int t = CY_GET_TRACKID(tch->t);
			int y = (tch->xh <<8) | tch->xl;
			int x = (tch->yh <<8) | tch->yl;

            if(e== 0x03) continue;
			input_report_abs(cyttsp4->input_dev, ABS_MT_TRACKING_ID, t);
			input_report_abs(cyttsp4->input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(cyttsp4->input_dev, ABS_MT_POSITION_Y, y);
			input_mt_sync(cyttsp4->input_dev);
			if(cyttsp4->dbg) printk("t=%-1d, x=%-4d y=%-4d, e=%02X\n", t, x, y, e);
		}
		input_sync(cyttsp4->input_dev);
	}
    else {
		input_mt_sync(cyttsp4->input_dev);
		input_sync(cyttsp4->input_dev);
	}
    
_cyttsp4_worker_exit:
    gpio_enable_irq(cyttsp4->igp_idx, cyttsp4->igp_bit);
    return;
}

static irqreturn_t cyttsp4_interrupt(int irq, void *dev)
{
    struct cyttsp4_data *cyttsp4 = dev;

    if (!gpio_irq_isenable(cyttsp4->igp_idx, cyttsp4->igp_bit) ||
        !gpio_irq_state(cyttsp4->igp_idx, cyttsp4->igp_bit))
		return IRQ_NONE;
    
    gpio_disable_irq(cyttsp4->igp_idx, cyttsp4->igp_bit);
	if(!cyttsp4->earlysus) queue_work(cyttsp4->workqueue, &cyttsp4->read_work);
		
	return IRQ_HANDLED;
}

static void cyttsp4_reset(struct cyttsp4_data *cyttsp4)
{
    gpio_enable(cyttsp4->igp_idx, cyttsp4->igp_bit, INPUT);
    gpio_setup_irq(cyttsp4->igp_idx, cyttsp4->igp_bit, IRQ_FALLING);

    gpio_enable(cyttsp4->rgp_idx, cyttsp4->rgp_bit, OUTPUT);
	gpio_set_value(cyttsp4->rgp_idx, cyttsp4->rgp_bit,1);

	msleep(20);
	gpio_set_value(cyttsp4->rgp_idx, cyttsp4->rgp_bit,0);
	msleep(40);
	gpio_set_value(cyttsp4->rgp_idx, cyttsp4->rgp_bit,1);
	msleep(20);

    return;
}

static int cyttsp4_init_panel(struct cyttsp4_data *cyttsp4)
{
	int ret=-1;
    u8 ops_cmd[2] = {0x00,0x08};
	u8 blr_cmd[7] = {0x01,0x3b,0x00,0x00,0x4f,0x6d,0x17};
    
    /* exit bootloader mode */
    cyttsp4->addr = 0x69;
	ret = cyttsp4_i2c_txdata(cyttsp4, blr_cmd, sizeof(blr_cmd));
    if(ret < 0) printk("exit bootloader mode failed.\n");
    msleep(500);

    /* switch to operating mode */
    cyttsp4->addr = 0x67;
	ret = cyttsp4_i2c_txdata(cyttsp4, ops_cmd, sizeof(ops_cmd));
	if(ret < 0) printk("enter operating_mode failed!\n");
    
	return ret;	
}     

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cyttsp4_early_suspend(struct early_suspend *handler)
{
    struct cyttsp4_data *cyttsp4 = container_of(handler, struct cyttsp4_data, early_suspend);
    cyttsp4->earlysus = 1;
	gpio_disable_irq(cyttsp4->igp_idx, cyttsp4->igp_bit);
    return;
}

static void cyttsp4_late_resume(struct early_suspend *handler)
{
    struct cyttsp4_data *cyttsp4 = container_of(handler, struct cyttsp4_data, early_suspend);

    cyttsp4_reset(cyttsp4);
    msleep(50);
    cyttsp4_init_panel(cyttsp4);
    cyttsp4->earlysus = 0;    
    gpio_enable_irq(cyttsp4->igp_idx, cyttsp4->igp_bit);
    return;
}
#endif  //CONFIG_HAS_EARLYSUSPEND

static ssize_t cat_dbg(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "dbg \n");
}

static ssize_t echo_dbg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int dbg = simple_strtoul(buf, NULL, 10);
    struct cyttsp4_data *cyttsp4 = pContext;

	if(dbg){
        cyttsp4->dbg = 1;
    }else{
        cyttsp4->dbg = 0;
    }
        
	return count;
}
static DEVICE_ATTR(dbg, S_IRUGO | S_IWUSR, cat_dbg, echo_dbg);

static struct attribute *cyttsp4_attributes[] = {
    &dev_attr_dbg.attr,
	NULL
};

static const struct attribute_group cyttsp4_group = {
	.attrs = cyttsp4_attributes,
};

static int cyttsp4_sysfs_create_group(struct cyttsp4_data *cyttsp4, const struct attribute_group *group)
{
    int err;

    cyttsp4->kobj = kobject_create_and_add("wmtts", NULL) ;
    if(!cyttsp4->kobj){
        dbg_err("kobj create failed.\n");
        return -ENOMEM;
    }
    
    /* Register sysfs hooks */
	err = sysfs_create_group(cyttsp4->kobj, group);
	if (err < 0){
        kobject_del(cyttsp4->kobj);
		dbg_err("Create sysfs group failed!\n");
		return -ENOMEM;
	}

    return 0;
}

static void cyttsp4_sysfs_remove_group(struct cyttsp4_data *cyttsp4, const struct attribute_group *group)
{
    sysfs_remove_group(cyttsp4->kobj, group);
    kobject_del(cyttsp4->kobj);  
    return;
}
static int cyttsp4_probe(struct platform_device *pdev)
{
	int err = 0;
	struct cyttsp4_data *cyttsp4 = platform_get_drvdata(pdev);
    
	INIT_WORK(&cyttsp4->read_work, cyttsp4_read_work);
	mutex_init(&cyttsp4->ts_mutex);

    err = cyttsp4_sysfs_create_group(cyttsp4, &cyttsp4_group);
    if(err < 0){
        dbg_err("sysfs create failed.\n");
        return err;
    }
    
    cyttsp4->workqueue = create_singlethread_workqueue(cyttsp4->name);
	if (!cyttsp4->workqueue) {
		err = -ESRCH;
		goto exit_create_singlethread;
	}
    
	cyttsp4->input_dev = input_allocate_device();
	if (!cyttsp4->input_dev) {
		err = -ENOMEM;
		dbg("failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}
	
	cyttsp4->input_dev->name = cyttsp4->name;
	cyttsp4->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	set_bit(INPUT_PROP_DIRECT, cyttsp4->input_dev->propbit);

	input_set_abs_params(cyttsp4->input_dev, ABS_MT_POSITION_X, 0, cyttsp4->xresl, 0, 0);
	input_set_abs_params(cyttsp4->input_dev, ABS_MT_POSITION_Y, 0, cyttsp4->yresl, 0, 0);
    input_set_abs_params(cyttsp4->input_dev, ABS_MT_TRACKING_ID, 0, 10, 0, 0);

	err = input_register_device(cyttsp4->input_dev);
	if (err) {
		dbg_err("cyttsp4_ts_probe: failed to register input device.\n");
		goto exit_input_register_device_failed;
	}
        
#ifdef CONFIG_HAS_EARLYSUSPEND
	cyttsp4->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	cyttsp4->early_suspend.suspend = cyttsp4_early_suspend;
	cyttsp4->early_suspend.resume = cyttsp4_late_resume;
	register_early_suspend(&cyttsp4->early_suspend);
#endif
    
	if(request_irq(cyttsp4->irq, cyttsp4_interrupt, IRQF_SHARED, cyttsp4->name, cyttsp4) < 0){
		dbg_err("Could not allocate irq for ts_cyttsp4 !\n");
		err = -1;
		goto exit_register_irq;
	}	
	cyttsp4->earlysus = 1;
    
    cyttsp4_reset(cyttsp4);
    msleep(80);
    cyttsp4_init_panel(cyttsp4);

    cyttsp4->earlysus = 0;
    gpio_enable_irq(cyttsp4->igp_idx, cyttsp4->igp_bit);
    
    return 0;
    
exit_register_irq:
	unregister_early_suspend(&cyttsp4->early_suspend);
exit_input_register_device_failed:
	input_free_device(cyttsp4->input_dev);
exit_input_dev_alloc_failed:
	cancel_work_sync(&cyttsp4->read_work);
	destroy_workqueue(cyttsp4->workqueue);  
exit_create_singlethread:
    cyttsp4_sysfs_remove_group(cyttsp4,&cyttsp4_group);
	return err;
}

static int cyttsp4_remove(struct platform_device *pdev)
{
    struct cyttsp4_data *cyttsp4 = dev_get_drvdata(&pdev->dev);

    cancel_work_sync(&cyttsp4->read_work);
    flush_workqueue(cyttsp4->workqueue);
	destroy_workqueue(cyttsp4->workqueue);
    cyttsp4_sysfs_remove_group(cyttsp4,&cyttsp4_group);
    
    free_irq(cyttsp4->irq, cyttsp4);
    gpio_disable_irq(cyttsp4->igp_idx, cyttsp4->igp_bit);
    
    unregister_early_suspend(&cyttsp4->early_suspend);
	input_unregister_device(cyttsp4->input_dev);
    
	mutex_destroy(&cyttsp4->ts_mutex);
	dbg("remove...\n");
	return 0;
}

static void cyttsp4_release(struct device *device)
{
    return;
}

static struct platform_device cyttsp4_device = {
	.name  	    = DEV_cyttsp4,
	.id       	= 0,
	.dev    	= {.release = cyttsp4_release},
};

static struct platform_driver cyttsp4_driver = {
	.driver = {
	           .name   	= DEV_cyttsp4,
		       .owner	= THIS_MODULE,
	 },
	.probe    = cyttsp4_probe,
	.remove   = cyttsp4_remove,
};


static int check_touch_env(struct cyttsp4_data *cyttsp4)
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
	if(strncmp(p,"cypress616",10)==0){//check touch ID
        cyttsp4->name = DEV_cyttsp4;
    }else{
        printk("cyttsp4 touch disabled.\n");
        return -ENODEV;
    }
	
	p = strchr(p,':');
	p++;
	sscanf(p,"%d:%d:%d:%d:%d:%d",
        &cyttsp4->xresl, &cyttsp4->yresl,&cyttsp4->igp_idx,&cyttsp4->igp_bit,&cyttsp4->rgp_idx, &cyttsp4->rgp_bit);

    cyttsp4->irq = IRQ_GPIO;
	printk("%s xresl=%d, yresl=%d, Interrupt GP%d, bit%d, Reset GP%d, bit%d\n", cyttsp4->name, 
        cyttsp4->xresl, cyttsp4->yresl, cyttsp4->igp_idx,cyttsp4->igp_bit,cyttsp4->rgp_idx,cyttsp4->rgp_bit);

	return 0;
}

static int __init cyttsp4_init(void)
{
	int ret = -ENOMEM;
    struct cyttsp4_data *cyttsp4=NULL;
        
	cyttsp4 = kzalloc(sizeof(struct cyttsp4_data), GFP_KERNEL);
    if(!cyttsp4){
        dbg_err("mem alloc failed.\n");
        return -ENOMEM;
    }
    
    pContext = cyttsp4;
	ret = check_touch_env(cyttsp4);
    if(ret < 0)
        goto exit_free_mem;

	ret = platform_device_register(&cyttsp4_device);
	if(ret){
		dbg_err("register platform drivver failed!\n");
		goto exit_free_mem;
	}
    platform_set_drvdata(&cyttsp4_device, cyttsp4);
	
	ret = platform_driver_register(&cyttsp4_driver);
	if(ret){
		dbg_err("register platform device failed!\n");
		goto exit_unregister_pdev;
	}
    
	return ret;
    
exit_unregister_pdev:
	platform_device_unregister(&cyttsp4_device);
exit_free_mem:
    kfree(cyttsp4);
    pContext = NULL;
	return ret;
}

static void cyttsp4_exit(void)
{
    if(!pContext) return;
    
	platform_driver_unregister(&cyttsp4_driver);
	platform_device_unregister(&cyttsp4_device);
    kfree(pContext);

	return;
}

late_initcall(cyttsp4_init);
module_exit(cyttsp4_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("FocalTech.Touch");
