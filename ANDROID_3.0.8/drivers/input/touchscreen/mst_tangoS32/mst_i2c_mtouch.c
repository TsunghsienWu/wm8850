/* MST multitouch controller driver.
 *
 * Copyright(c) 2010 Master Touch Inc.
 *
 * Author: David <matt@0xlab.org>
 *
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <mach/hardware.h>
#include <mach/gpio-cs.h>

#include "mst_i2c_mtouch.h"

#define MST_DRIVER_NAME 	"mst-touch"

#define COORD_INTERPRET(MSB_BYTE, LSB_BYTE) \
		(MSB_BYTE << 8 | LSB_BYTE)

//#define MST_DEBUG

#undef dbg
#ifdef MST_DEBUG
	#define dbg(fmt,args...)  printk("DBG:%s_%d:"fmt,__FUNCTION__,__LINE__,##args)
#else
	#define dbg(fmt,args...)  
#endif

#undef dbg_err
#define dbg_err(fmt,args...)  printk("ERR:%s_%d:"fmt,__FUNCTION__,__LINE__,##args)

struct mst_data{
	int 	x, y;
	int 	x2, y2;
    //int   w, p;
    //int   w2, p2;
    int 	px, py;
	int 	px2, py2;
    
	struct input_dev *dev;
	struct mutex lock;

    int gpidx;
    int gpbit;
    
	int irq;
    int capcal;
    
	int nt;
	int x_resl;
	int y_resl;
	struct delayed_work work;
    
    int ealrysus;
#ifdef CONFIG_HAS_EARLYSUSPEND
    struct early_suspend earlysuspend;
#endif

};

struct mst_data mst_context;

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mst_early_suspend(struct early_suspend *h);
static void mst_late_resume(struct early_suspend *h);
#endif

static inline int mst_pen_isup(struct mst_data *mst)
{
	return gpio_get_value(mst->gpidx,mst->gpbit);
}

extern int wmt_i2c_xfer_continue_if_4(struct i2c_msg *msg, unsigned int num, int bus_id);

static int __mst_reg_write(struct mst_data *mst, u8 reg, u8 value)
{
	int ret = 0;
	struct i2c_msg msg[1];
	unsigned char buf[2];

	buf[0] = reg;
	buf[1] = value;

	msg[0].addr = MST_I2C_ADDR;
	msg[0].flags = 0 ;
	msg[0].flags &= ~(I2C_M_RD);
	msg[0].len = 2;
	msg[0].buf = buf;

	ret = wmt_i2c_xfer_continue_if_4(msg, 1,MST_I2C_BUS_ID);
	if(ret <= 0)
		dbg("IIC xfer failed,errno=%d\n",ret);

    	return ret;
}

static int reg_write(struct mst_data *mst, u8 reg, u8 val)
{
	int timeout = 20;

	mutex_lock(&mst->lock);
	while(--timeout && __mst_reg_write(mst, reg, val)<=0)
		udelay(100);
	mutex_unlock(&mst->lock);

	if(!timeout)
		dbg("IIC reg write error!\n");

	return timeout;
}

inline static int __mst_reg_read(struct mst_data *mst, u8 reg, u8* value)
{
	int ret = 0;
	struct i2c_msg msg[2];
	unsigned char buf[2];

	buf[0] = reg;
	buf[1] = 0x0;

	msg[0].addr = MST_I2C_ADDR;
	msg[0].flags = 0 ;
	msg[0].len = 1;
	msg[0].buf = buf;

	msg[1].addr = MST_I2C_ADDR;
	msg[1].flags = 0 ;
	msg[1].flags |= (I2C_M_RD);
	msg[1].len = 1;
	msg[1].buf = value;

	ret = wmt_i2c_xfer_continue_if_4(msg, 2,MST_I2C_BUS_ID);
	if(ret <= 0)
		dbg("IIC xfer failed,errno=%d\n",ret);

    	return ret;

}

inline static u8 reg_read(struct mst_data *mst, u8 reg)
{
	int32_t timeout = 20;
	u8 value = 0;
	
	mutex_lock(&mst->lock);
	while(--timeout && __mst_reg_read(mst, reg, &value)<=0);
		udelay(100);
	mutex_unlock(&mst->lock);
	
	if(!timeout)
		dbg("IIC reg read error!\n");

	return (value & 0xff);
}

static int reg_set_bit_mask(struct mst_data *mst,
			    u8 reg, u8 mask, u8 val)
{
	int ret = 0;
	u8 tmp;

	val &= mask;

	mutex_lock(&mst->lock);

	ret = __mst_reg_read(mst, reg,&tmp);
	if(ret <= 0)
		goto exit;
	
	tmp &= ~mask;
	tmp |= val;
	ret = __mst_reg_write(mst, reg, tmp);
exit:
	if(ret <= 0)
		dbg_err("Master IIC read/write Failed!\n");
	mutex_unlock(&mst->lock);

	return ret;
}

static int mst_calibrate(struct mst_data *mst)
{
	int ret = 0;
	
	/* auto calibrate */
    printk("Warning: Don't touch the screen when auto calibrating..\n");
	ret = reg_write(mst,MST_SPECOP,0x03);
	msleep(1500);

	return ret?0:-1;
}

static const char *pwr_mod_str[] = {

	[MST_PWR_ACT] = "act",
	[MST_PWR_SLEEP] = "sleep",
	[MST_PWR_DSLEEP] = "dsleep",
	[MST_PWR_FREEZE] = "freeze",
};

static ssize_t cat_pwr_mod(struct device *dev,
                struct device_attribute *attr, char *buf)
{
	struct mst_data *mst = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", pwr_mod_str[(reg_read(mst, MST_PWR_MODE) & 0x3)]);
}
static ssize_t echo_pwr_mod(struct device *dev, struct device_attribute *attr,
               const char *buf, size_t count)
{
	struct mst_data *mst = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < ARRAY_SIZE(pwr_mod_str); i++) {
		if (!strncmp(buf, pwr_mod_str[i], strlen(pwr_mod_str[i]))) {
			reg_set_bit_mask(mst, MST_PWR_MODE, 0x3, i);
		}
	}
	return count;
}

static DEVICE_ATTR(pwr_mod, S_IRUGO | S_IWUSR, cat_pwr_mod, echo_pwr_mod);

static const char *int_mod_str[] = {

	[MST_INT_PERIOD] = "period",
	[MST_INT_FMOV] = "fmov",
	[MST_INT_FTOUCH] = "ftouch",
};

static ssize_t cat_int_mod(struct device *dev,
                struct device_attribute *attr, char *buf)
{
	struct mst_data *mst = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", int_mod_str[(reg_read(mst, MST_INT_MODE) & 0x3)]);
}
static ssize_t echo_int_mod(struct device *dev, struct device_attribute *attr,
               const char *buf, size_t count)
{
	struct mst_data *mst = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < ARRAY_SIZE(int_mod_str); i++) {
		if (!strncmp(buf, int_mod_str[i], strlen(int_mod_str[i])))
			reg_set_bit_mask(mst, MST_INT_MODE, 0x3, i);
	}
	return count;
}

static DEVICE_ATTR(int_mod, S_IRUGO | S_IWUSR, cat_int_mod, echo_int_mod);

static const char *int_trig_str[] = {

	[MST_INT_TRIG_LOW] = "low-act",
	[MST_INT_TRIG_HI] = "hi-act",
};

static ssize_t cat_int_trig(struct device *dev,
                struct device_attribute *attr, char *buf)
{
	struct mst_data *mst = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n",
			int_trig_str[((reg_read(mst, MST_INT_MODE) >> 2)& 0x1)]);
}
static ssize_t echo_int_trig(struct device *dev, struct device_attribute *attr,
               const char *buf, size_t count)
{
	struct mst_data *mst = dev_get_drvdata(dev);
	u8 tmp;

	if (!strncmp(buf, int_trig_str[MST_INT_TRIG_LOW],
								strlen(int_trig_str[MST_INT_TRIG_LOW]))) {
		tmp = reg_read(mst, MST_INT_MODE);
		tmp &= ~(MST_INT_TRIG_LOW_EN);
		reg_write(mst, MST_INT_MODE, tmp);
	}
	else if (!strncmp(buf, int_trig_str[MST_INT_TRIG_HI],
								strlen(int_trig_str[MST_INT_TRIG_HI]))){
		tmp = reg_read(mst, MST_INT_MODE);
		tmp |= MST_INT_TRIG_LOW_EN;
		reg_write(mst, MST_INT_MODE, tmp);
	}
	else {
		dbg_err("trigger mode not support.\n");
	}
	return count;
}

static DEVICE_ATTR(int_trig, S_IRUGO | S_IWUSR, cat_int_trig, echo_int_trig);

static ssize_t cat_pwr_asleep(struct device *dev,
                struct device_attribute *attr, char *buf)
{
	struct mst_data *mst = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n",((reg_read(mst, MST_PWR_MODE) >> 2)& 0x1));
}
static ssize_t echo_pwr_asleep(struct device *dev, struct device_attribute *attr,
               const char *buf, size_t count)
{
	struct mst_data *mst = dev_get_drvdata(dev);
	unsigned int asleep_mod = simple_strtoul(buf, NULL, 10);
	u8 tmp;

	asleep_mod = !!asleep_mod;

	if (asleep_mod) {
		tmp = reg_read(mst, MST_PWR_MODE);
		tmp |= MST_PWR_ASLEEP_EN;
		reg_write(mst, MST_PWR_MODE, tmp);
	}
	else{
		tmp = reg_read(mst, MST_PWR_MODE);
		tmp &= ~(MST_PWR_ASLEEP_EN);
		reg_write(mst, MST_PWR_MODE, tmp);
	}
	return count;
}

static DEVICE_ATTR(pwr_asleep, S_IRUGO | S_IWUSR, cat_pwr_asleep, echo_pwr_asleep);

static ssize_t cat_int_width(struct device *dev,
                struct device_attribute *attr, char *buf)
{
	struct mst_data *mst = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n",reg_read(mst, MST_INT_WIDTH));
}

static ssize_t echo_int_width(struct device *dev, struct device_attribute *attr,
               const char *buf, size_t count)
{
	struct mst_data *mst = dev_get_drvdata(dev);
	unsigned int int_width = simple_strtoul(buf, NULL, 10);

	int_width &= 0xff;

	reg_write(mst, MST_INT_WIDTH, int_width);
	return count;
}

static DEVICE_ATTR(int_width, S_IRUGO | S_IWUSR, cat_int_width, echo_int_width);

static ssize_t cat_autocal(struct device *dev,
                struct device_attribute *attr, char *buf)
{
	//struct mst_data *mst = dev_get_drvdata(dev);

	return sprintf(buf, "autocal \n");
}

static ssize_t echo_autocal(struct device *dev, struct device_attribute *attr,
               const char *buf, size_t count)
{
    int ret = 0;
	struct mst_data *mst = dev_get_drvdata(dev);
	unsigned int cal = simple_strtoul(buf, NULL, 10);

	if(cal){
        ret = mst_calibrate(mst);
        if(ret) printk("Autocal Failed.\n");
    }else{
        printk("autocal --echo not zero digital to this node.\n");
    }
        
	return count;
}

static DEVICE_ATTR(autocal, S_IRUGO | S_IWUSR, cat_autocal, echo_autocal);

static struct attribute *mst_attributes[] = {
	&dev_attr_pwr_mod.attr,
	&dev_attr_pwr_asleep.attr,
	&dev_attr_int_mod.attr,
	&dev_attr_int_trig.attr,
	&dev_attr_int_width.attr,
	&dev_attr_autocal.attr,
	NULL
};

static const struct attribute_group mst_group = {
	.attrs = mst_attributes,
};


static inline void mst_report(struct mst_data *mst)
{
	if(mst->x||mst->y||mst->x2||mst->y2){

        if((mst->px != 0 && mst->py != 0)  && (abs(mst->px-mst->x) > MST_JITTER_THRESHOLD ||abs(mst->py-mst->y) > MST_JITTER_THRESHOLD)){
            //printk("x threshold %d, y threhold %d\n", abs(mst->px-mst->x), abs(mst->py-mst->y) );
            mst->px = 0;
            mst->py = 0;
            goto p2;
            
        }
            
		input_report_abs(mst->dev, ABS_MT_POSITION_Y, mst->x);
		input_report_abs(mst->dev, ABS_MT_POSITION_X, mst->x_resl-mst->y);
		input_mt_sync(mst->dev);

        mst->px = mst->x;
        mst->py = mst->y;
p2:
		if(mst->nt==2){

            if((mst->px2 != 0 && mst->py2 != 0) && (abs(mst->px2-mst->x2) > MST_JITTER_THRESHOLD || abs(mst->py2-mst->y2) > MST_JITTER_THRESHOLD)){
                //printk("x2 threshold %d, y2 threhold %d\n", abs(mst->px2-mst->x2), abs(mst->py2-mst->y2) );
                mst->px2 = 0;
                mst->py2 = 0;
                goto end;
            }
            
			input_report_abs(mst->dev, ABS_MT_POSITION_Y, mst->x2);
			input_report_abs(mst->dev, ABS_MT_POSITION_X, mst->x_resl-mst->y2);
			input_mt_sync(mst->dev);

            mst->px2 = mst->x2;
            mst->py2 = mst->y2;
		}
end:	
		input_sync(mst->dev);
	}
	
	return;
}

static void mst_i2c_work(struct work_struct *work)
{
	struct mst_data *mst =
			container_of((struct delayed_work*)work, struct mst_data, work);

	mst->nt = reg_read(mst, MST_TOUCH);
	
	if(mst_pen_isup(mst)||mst->nt==0 ||mst->ealrysus){
		dbg("========== pen is up ============\n");
		input_mt_sync(mst->dev);
		input_sync(mst->dev);

        if(!mst->ealrysus)
		    gpio_enable_irq(mst->gpidx, mst->gpbit);

        mst->px = 0;
        mst->py = 0;
        
        mst->px2 = 0;
        mst->px2 = 0;
        
		return;
	}

	if (mst->nt==2) {
		
		mst->x = COORD_INTERPRET(reg_read(mst, MST_POS_X_HI),
				reg_read(mst, MST_POS_X_LOW));
		mst->y = COORD_INTERPRET(reg_read(mst, MST_POS_Y_HI),
				reg_read(mst, MST_POS_Y_LOW));
		//mst->p = COORD_INTERPRET(reg_read(mst, MST_STRNGTH_HI),
		//		reg_read(mst, MST_STRNGTH_LOW));
		//mst->w = reg_read(mst, MST_AREA_X);

		mst->x2 = COORD_INTERPRET(reg_read(mst, MST_POS_X2_HI),
				reg_read(mst, MST_POS_X2_LOW));
		mst->y2 = COORD_INTERPRET(reg_read(mst, MST_POS_Y2_HI),
				reg_read(mst, MST_POS_Y2_LOW));
		//mst->p2 = COORD_INTERPRET(reg_read(mst, MST_STRNGTH_HI),
		//		reg_read(mst, MST_STRNGTH_LOW));
		//mst->w2 = reg_read(mst, MST_AREA_X2);

		dbg(" x=%-4d,y=%-4d,w=%-4d,   x2=%-4d,y2=%-4d,w2=%-4d, ntouch=%d\n",
			mst->x, mst->y, mst->w, mst->x2, mst->y2, mst->w2,mst->nt);

		mst_report(mst);

	}
	else if(mst->nt==1){
		
		mst->x = COORD_INTERPRET(reg_read(mst, MST_POS_X_HI),
				reg_read(mst, MST_POS_X_LOW));
		mst->y = COORD_INTERPRET(reg_read(mst, MST_POS_Y_HI),
				reg_read(mst, MST_POS_Y_LOW));
		//mst->p = COORD_INTERPRET(reg_read(mst, MST_STRNGTH_HI),
		//		reg_read(mst, MST_STRNGTH_LOW));
		//mst->w = reg_read(mst, MST_AREA_X);
		dbg("  x=%-4d,y=%-4d,w=%-4d, ntouch=%d\n",mst->x, mst->y, mst->w,mst->nt);
		mst_report(mst);
	}
	else{
		dbg("Mst loop error!\n");

	}

	schedule_delayed_work(&mst->work, 18*HZ/1000);
	return;
	
}

static irqreturn_t mst_irq_handler(int irq, void *_mst)
{
	struct mst_data *mst = _mst;

	if(!gpio_irq_isenable(mst->gpidx, mst->gpbit) ||
        !gpio_irq_state(mst->gpidx, mst->gpbit))
        return IRQ_NONE;
    
    gpio_disable_irq(mst->gpidx, mst->gpbit);
    gpio_irq_clean(mst->gpidx, mst->gpbit);
    
	udelay(20);
	if(mst_pen_isup(mst)){
        gpio_enable_irq(mst->gpidx, mst->gpbit);
		dbg("get touch interrupt ,but check pen is up ?!\n");
		return IRQ_HANDLED;
	}

	dbg("======== pen is down ========\n");
	schedule_delayed_work(&mst->work,0);
	
	return IRQ_HANDLED;

}


static int mst_probe(struct platform_device *pdev)
{
	struct mst_data *mst;
	struct input_dev *input_dev;
	int err = 0;
	char tmp=0;
	
	mst = &mst_context;
	dev_set_drvdata(&pdev->dev, mst);
	mutex_init(&mst->lock);

	/* allocate input device */
	input_dev = input_allocate_device();
	if (!input_dev) {
		dbg_err( "Failed to allocate input device \n");
		return -ENODEV;
    }

#ifdef CONFIG_HAS_EARLYSUSPEND
    mst->earlysuspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
    mst->earlysuspend.suspend = mst_early_suspend;
    mst->earlysuspend.resume = mst_late_resume;
    register_early_suspend(&mst->earlysuspend);
#endif

	mst->dev = input_dev;
	input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) |BIT_MASK(EV_ABS);
    input_dev->propbit[0] = BIT_MASK(INPUT_PROP_DIRECT);

	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
						0, mst->x_resl, 0, 0);

	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
						0, mst->y_resl, 0, 0);

	input_dev->name = pdev->name;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &pdev->dev;

	err = input_register_device(input_dev);
	if (err){
		dbg_err("register input device failed!\n");
		goto exit_input;
	}

	INIT_DELAYED_WORK(&mst->work, mst_i2c_work);

	
	/* auto calibrate */
    if(mst->capcal){
        printk("\n\nMST TangoS32 Auto Calibrating.\n\n");
	    reg_write(mst,MST_SPECOP,0x03);
	    msleep(1000);
    }

	/* setup INT mode */
	reg_write(mst, MST_INT_MODE, 0x0a);

	tmp = reg_read(mst, MST_INT_MODE);
	tmp &= ~(MST_INT_TRIG_LOW_EN);
	reg_write(mst, MST_INT_MODE, tmp);

	err = request_irq(mst->irq, mst_irq_handler,IRQF_SHARED, MST_DRIVER_NAME, mst);
	if(err){
		dbg_err("Master request irq failed!");
		goto exit_input;
	}
    gpio_enable(mst->gpidx, mst->gpbit, INPUT);
    gpio_pull_enable(mst->gpidx, mst->gpbit, PULL_UP);
    gpio_setup_irq(mst->gpidx, mst->gpbit,IRQ_FALLING);

	/* Register sysfs hooks */
	err = sysfs_create_group(&pdev->dev.kobj, &mst_group);
	if (err < 0){
		dbg_err("Create sysfs failed!\n");
		goto exit_input;
	}

	return 0;

exit_input:
	input_unregister_device(mst->dev);
	
	return err;
}

static int __devexit mst_remove(struct platform_device *pdev)
{
	struct mst_data *mst = dev_get_drvdata(&pdev->dev);

	gpio_disable_irq(mst->gpidx, mst->gpbit);
	free_irq(mst->irq, mst);

	cancel_delayed_work_sync(&mst->work);
	
	input_unregister_device(mst->dev);
	sysfs_remove_group(&pdev->dev.kobj, &mst_group);
    
#ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&mst->earlysuspend);
#endif

	mutex_destroy(&mst->lock);
	
	return 0;
}

#ifdef CONFIG_PM

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mst_early_suspend(struct early_suspend *h)
{
	struct mst_data *mst = &mst_context;

	//reg_set_bit_mask(mst, MST_PWR_MODE, MST_PWR_DSLEEP, 0x03);
    gpio_disable_irq(mst->gpidx, mst->gpbit);
    mst->ealrysus = 1;
    cancel_delayed_work_sync(&mst->work);
    
	return ;

}

static void mst_late_resume(struct early_suspend *h)
{
	int tmp=0;
	struct mst_data *mst = &mst_context;

	//reg_set_bit_mask(mst, MST_PWR_MODE, MST_PWR_ACT, 0x03);

	/* setup INT mode */
	reg_write(mst, MST_INT_MODE, 0x0a);
	
	tmp = reg_read(mst, MST_INT_MODE);
	tmp &= ~(MST_INT_TRIG_LOW_EN);
	reg_write(mst, MST_INT_MODE, tmp);

    mst->ealrysus = 0;
	gpio_enable(mst->gpidx, mst->gpbit, INPUT);
    gpio_pull_enable(mst->gpidx, mst->gpbit, PULL_UP);
    gpio_setup_irq(mst->gpidx, mst->gpbit,IRQ_FALLING);

    return;
}
#endif

static int mst_suspend(struct platform_device *pdev, pm_message_t state)
{
#ifndef CONFIG_HAS_EARLYSUSPEND 
	struct mst_data *mst = dev_get_drvdata(&pdev->dev);

	//reg_set_bit_mask(mst, MST_PWR_MODE, MST_PWR_DSLEEP, 0x03);
    gpio_disable_irq(mst->gpidx, mst->gpbit);
    cancel_delayed_work_sync(&mst->work);
#endif
   
	return 0;
}

static int mst_resume(struct platform_device *pdev)
{
#ifndef CONFIG_HAS_EARLYSUSPEND 
	int tmp=0;
	struct mst_data *mst = dev_get_drvdata(&pdev->dev);

	//reg_set_bit_mask(mst, MST_PWR_MODE, MST_PWR_ACT, 0x03);

	/* setup INT mode */
	reg_write(mst, MST_INT_MODE, 0x0a);
	
	tmp = reg_read(mst, MST_INT_MODE);
	tmp &= ~(MST_INT_TRIG_LOW_EN);
	reg_write(mst, MST_INT_MODE, tmp);
	
	gpio_enable(mst->gpidx, mst->gpbit, INPUT);
    gpio_pull_enable(mst->gpidx, mst->gpbit, PULL_UP);
    gpio_setup_irq(mst->gpidx, mst->gpbit,IRQ_FALLING);
#endif

	return 0;
}
#else
#define mst_suspend NULL
#define mst_resume NULL
#endif

static void mst_release(struct device *device)
{
    return;
}

static struct platform_device mst_device = {
	.name  	    = MST_DRIVER_NAME,
	.id       	= 0,
	.dev    	= {.release = mst_release},
};

static struct platform_driver mst_driver = {
	.driver = {
	           .name   	= MST_DRIVER_NAME,
		       .owner	= THIS_MODULE,
	 },
	.probe    = mst_probe,
	.remove   = mst_remove,
	.suspend  = mst_suspend,
	.resume   = mst_resume,
};


/////////////////////////////////////////////
//          char device register reference
/////////////////////////////////////////////
static struct class* l_dev_class = NULL;
static struct device *l_clsdevice = NULL;

#define MST_DEV_NAME 				 "wmtts"
#define MST_MAJOR					11

#define TS_IOC_MAGIC  				't'
#define TS_IOCTL_CAL_START    		_IO(TS_IOC_MAGIC,   1)
#define TS_IOCTL_CAL_DONE     		_IOW(TS_IOC_MAGIC,  2, int*)
#define TS_IOCTL_GET_RAWDATA  		_IOR(TS_IOC_MAGIC,  3, int*)
#define TS_IOCTL_CAL_QUIT			_IOW(TS_IOC_MAGIC,  4, int*)
#define TS_IOCTL_CAL_CAP	        _IOW(TS_IOC_MAGIC,  5, int*)
#define TS_IOC_MAXNR          			5

static int mst_open(struct inode *inode, struct file *filp)
{
	dbg("wmt ts driver opening...\n");
    return 0;
}

static int mst_close(struct inode *inode, struct file *filp)
{
	dbg("wmt ts driver closing...\n");
	return 0;
}

static long mst_ioctl(struct file *dev, unsigned int cmd, unsigned long arg)
{

	if (_IOC_TYPE(cmd) != TS_IOC_MAGIC){ 
		dbg("CMD ERROR!");
		return -ENOTTY;
	}
	
	if (_IOC_NR(cmd) > TS_IOC_MAXNR){ 
		dbg("NO SUCH IO CMD!\n");
		return -ENOTTY;
	}

	switch (cmd) {
		case TS_IOCTL_CAL_START:
			printk("MST: TS_IOCTL_CAL_START\n");
			
			return 0;
			
		case TS_IOCTL_CAL_DONE:
			printk("MST: TS_IOCTL_CAL_DONE\n");

			return 0;
			
		case TS_IOCTL_CAL_QUIT:
			printk("MST: TS_IOCTL_CAL_QUIT\n");
			
			return 0;
			
		case TS_IOCTL_GET_RAWDATA:
			printk("MST: TS_IOCTL_GET_RAWDATA\n");

			return 0;
		case TS_IOCTL_CAL_CAP:
			printk("MST: TS_IOCTL_CAL_CAP\n");
			if(mst_calibrate(&mst_context)){
				dbg("MST: calibrate failed!\n");
				return -EINVAL;
			}

			return 0;
			
	}
	
	return -EINVAL;
}

static ssize_t mst_read(struct file *filp, char *buf, size_t count, loff_t *l)
{
	return 0;
}


static struct file_operations mst_fops = {
	.read               = mst_read,
	.unlocked_ioctl     = mst_ioctl,
	.open       	    = mst_open,
	.release      	    = mst_close,
};

static int mst_register_chdev(void)
{
	int ret =0;
	
	// Create device node
	if (register_chrdev (MST_MAJOR, MST_DEV_NAME, &mst_fops)) {
		printk (KERN_ERR "wmt touch: unable to get major %d\n", MST_MAJOR);
		return -EIO;
	}	
	
	l_dev_class = class_create(THIS_MODULE, MST_DEV_NAME);
	if (IS_ERR(l_dev_class)){
		unregister_chrdev(MST_MAJOR, MST_DEV_NAME);
		ret = PTR_ERR(l_dev_class);
		printk(KERN_ERR "Can't class_create touch device !!\n");
		return ret;
	}
	
	l_clsdevice = device_create(l_dev_class, NULL, MKDEV(MST_MAJOR, 0), NULL, MST_DEV_NAME);
	if (IS_ERR(l_clsdevice)){
		unregister_chrdev(MST_MAJOR, MST_DEV_NAME);
		class_destroy(l_dev_class);
		ret = PTR_ERR(l_clsdevice);
		printk(KERN_ERR "Failed to create device %s !!!",MST_DEV_NAME);
		return ret;
	}

	return 0;
}


static int mst_unregister_chdev(void)
{	
	device_destroy(l_dev_class, MKDEV(MST_MAJOR, 0));
	unregister_chrdev(MST_MAJOR, MST_DEV_NAME);
	class_destroy(l_dev_class);

	return 0;
}

extern int wmt_setsyspara(char *varname, unsigned char *varval);
extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);

static int check_touch_env(struct mst_data *mst)
{
	int ret = 0;
	int len = 96;
	int Enable;
    char retval[96] = {0};
	char *p=NULL;

    	// Get u-boot parameter
	ret = wmt_getsyspara("wmt.io.touch", retval, &len);
	if(ret){
		//printk("MST:Read wmt.io.touch Failed.\n");
		return -EIO;
	}
	sscanf(retval,"%d:",&Enable);
	//check touch enable
	if(Enable == 0){
		//printk("Touch Screen Is Disabled.\n");
		return -ENODEV;
	}
	
	p = strchr(retval,':');
	p++;
	if(strncmp(p,"mst_tangos32",12)){//check touch ID
		//printk("===== MST Touch Disabled!=====\n");
		return -ENODEV; 
	}
	
	p = strchr(p,':');
	p++;
	sscanf(p,"%d:%d:%d:%d",&mst->x_resl, &mst->y_resl,&mst->gpidx, &mst->gpbit);
	
	mst->irq = IRQ_GPIO;
	printk("MST TangoS32 Touch p.x = %d, p.y = %d, Interrupt GP%d, bit%d\n", mst->x_resl, mst->y_resl, mst->gpidx,mst->gpbit);

    memset(retval, 0, sizeof(retval));
    ret = wmt_getsyspara("wmt.io.capcal", retval, &len);
    if(ret){
        mst->capcal = 1;
        wmt_setsyspara("wmt.io.capcal", "1");
    }
    
	if(mst->irq < 0){
		printk("MST: map gpio irq error ?!\n");
		return -ENODEV;
	}

	return 0;
}

static int __init mst_init(void)
{
	int ret = 0;

	memset(&mst_context,0,sizeof(struct mst_data));
	
	if(check_touch_env(&mst_context))
		return -EIO;

	ret = platform_device_register(&mst_device);
	if(ret){
		dbg_err("register platform drivver failed!\n");

		return ret;
	}
	
	ret = platform_driver_register(&mst_driver);
	if(ret){
		dbg_err("register platform device failed!\n");
		goto exit_unregister_pdev;
	}

	ret = mst_register_chdev();
	if(ret){
		dbg_err("register char devcie failed!\n");
		goto exit_unregister_pdrv;
	}
	
	return ret;
	
exit_unregister_pdrv:
	platform_driver_unregister(&mst_driver);
exit_unregister_pdev:
	platform_device_unregister(&mst_device);

	return ret;
	
}

static void mst_exit(void)
{
	platform_driver_unregister(&mst_driver);
	platform_device_unregister(&mst_device);
	mst_unregister_chdev();

	return;
}

//module_init(mst_init);
late_initcall(mst_init);
module_exit(mst_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MasterTouch");
