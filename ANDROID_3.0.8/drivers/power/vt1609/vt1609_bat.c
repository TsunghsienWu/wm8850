/*++
	Copyright (c) 2008  WonderMedia Technologies, Inc.

	This program is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software Foundation,
	either version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
	PARTICULAR PURPOSE.  See the GNU General Public License for more details.
	You should have received a copy of the GNU General Public License along with
	this program.  If not, see <http://www.gnu.org/licenses/>.

	WonderMedia Technologies, Inc.
	10F, 529, Chung-Cheng Road, Hsin-Tien, Taipei 231, R.O.C.

--*/

#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <mach/hardware.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/spi/spi.h>
#include <linux/i2c.h>
#include "vt1609_bat.h"

//#define BAT_DEBUG

#undef dbg
#undef dbg_err
#ifdef BAT_DEBUG
#define dbg(fmt, args...) printk(KERN_ERR "[%s]_%d: " fmt, __func__ , __LINE__, ## args)
#else
#define dbg(fmt, args...) 
#endif

#define dbg_err(fmt, args...) printk(KERN_ERR "## VT1603A/VT1609 BAT##: " fmt,  ## args)

/*
 * vt1603_set_reg8 - set register value of vt1603
 * @bat_drv: vt1603 driver data
 * @reg: vt1603 register address
 * @val: value register will be set
 */
static int vt1603_set_reg8(struct vt1603_bat_drvdata *bat_drv, u8 reg, u8 val)
{
	int ret =0;
	
	if (bat_drv->tdev)
        ret = bat_drv->tdev->reg_write(bat_drv->tdev, reg, val);

	if(ret){
		dbg_err("vt1603 battery write error, errno%d\n", ret);
		return ret;
	}
	
	return 0;
}

/*
 * vt1603_get_reg8 - get register value of vt1603
 * @bat_drv: vt1603 driver data
 * @reg: vt1603 register address
 */
static u8 vt1603_get_reg8(struct vt1603_bat_drvdata *bat_drv, u8 reg)
{
	u8 val = 0;
	int ret = 0;
	
	if (bat_drv->tdev)	
		ret = bat_drv->tdev->reg_read(bat_drv->tdev, reg, &val);

	if (ret < 0){
		dbg_err("vt1603 battery read error, errno%d\n", ret);
		return 0;
	}
	
	return val;
}


static int vt1603_read8(struct vt1603_bat_drvdata *bat_drv, u8 reg,u8* data)
{
	int ret = 0;
	
	if (bat_drv->tdev)	
		ret = bat_drv->tdev->reg_read(bat_drv->tdev, reg, data);

	if (ret){
		dbg_err("vt1603 battery read error, errno%d\n", ret);
		return ret;
	}
	
	return 0;
}
/*
 * vt1603_setbits - write bit1 to related register's bit
 * @bat_drv: vt1603 battery driver data
 * @reg: vt1603 register address
 * @mask: bit setting mask
 */
static void vt1603_setbits(struct vt1603_bat_drvdata *bat_drv, u8 reg, u8 mask)
{
    u8 tmp = vt1603_get_reg8(bat_drv, reg) | mask;
    vt1603_set_reg8(bat_drv, reg, tmp);
}

/*
 * vt1603_clrbits - write bit0 to related register's bit
 * @bat_drv: vt1603 battery driver data
 * @reg: vt1603 register address
 * @mask:bit setting mask
 */
static void vt1603_clrbits(struct vt1603_bat_drvdata *bat_drv, u8 reg, u8 mask)
{
    u8 tmp = vt1603_get_reg8(bat_drv, reg) & (~mask);
    vt1603_set_reg8(bat_drv, reg, tmp);
}

/*
 * vt1603_reg_dump - dubug function, for dump vt1603 related registers
 * @bat_drv: vt1603 battery driver data
 */
static void vt1603_reg_dump(struct vt1603_bat_drvdata *bat_drv, u8 addr, int len)
{
    u8 i;
    for (i = addr; i < addr + len; i += 2)
        dbg("reg[%d]:0x%02X,  reg[%d]:0x%02X\n", 
            i, vt1603_get_reg8(bat_drv, i), i + 1, vt1603_get_reg8(bat_drv, i + 1));
}

/*
 * vt1603_bat - global data for battery information maintenance
 */
static struct vt1603_bat_drvdata *vt1603_bat = NULL;

#ifdef VT1603_BATTERY_EN
/*
 * vt1603_get_bat_convert_data - get battery converted data
 * @bat_drv: vt1603 battery driver data
 */
static int vt1603_get_bat_data(struct vt1603_bat_drvdata *bat_drv,int *data)
{
    int ret = 0;
    u8 data_l, data_h;
	
    ret |= vt1603_read8(bat_drv, VT1603_DATL_REG, &data_l);
    ret |= vt1603_read8(bat_drv, VT1603_DATH_REG, &data_h);
	
    *data = ADC_DATA(data_l, data_h);
	
    return ret;
}

/*
 * vt1603_work_mode_switch - switch VT1603 to battery mode
 * @bat_drv: vt1603 battery driver data
 */
static void vt1603_switch_to_bat_mode(struct vt1603_bat_drvdata *bat_drv)
{
    dbg("Enter\n");    
    vt1603_set_reg8(bat_drv, VT1603_CR_REG, 0x00);
    vt1603_set_reg8(bat_drv, 0xc6, 0x00);
    vt1603_set_reg8(bat_drv, VT1603_AMCR_REG, BIT0);
    dbg("Exit\n");
}

/*
 * vt1603_bat_tmr_isr - vt1603 battery detect timer isr
 * @bat_drvdata_addr: address of vt1603 battery driver data
 */
static void vt1603_bat_tmr_isr(unsigned long context)
{
    struct vt1603_bat_drvdata *bat_drv;

    dbg("Enter\n");
    bat_drv = (struct vt1603_bat_drvdata *)context;
    schedule_work(&bat_drv->work);
    dbg("Exit\n");
    return ;
}

/*
 * vt1603_get_pen_state - get touch panel pen state from vt1603 
 *         interrup status register
 * @bat_drv: vt1603 battery driver data
 */
static inline int vt1603_get_pen_state(struct vt1603_bat_drvdata *bat_drv)
{
    u8 state ;

    if(!bat_drv->tch_enabled)
        return TS_PENUP_STATE;
    
    state = vt1603_get_reg8(bat_drv, VT1603_INTS_REG);
    return (((state & BIT4) == 0) ? TS_PENDOWN_STATE : TS_PENUP_STATE);
}

static inline void vt1603_bat_pen_manual(struct vt1603_bat_drvdata *bat_drv)
{
    vt1603_setbits(bat_drv, VT1603_INTCR_REG, BIT7);
}

static void vt1603_bat_power_up(struct vt1603_bat_drvdata *bat_drv)
{
    if (vt1603_get_reg8(bat_drv, VT1603_PWC_REG) != 0x08)
        vt1603_set_reg8(bat_drv, VT1603_PWC_REG, 0x08);

    return ;
}

static int vt1603_bat_avg(int *data, int num)
{
    int i = 0;
    int avg = 0;
	
    if(num == 0)
	return 0;
	
    for (i = 0; i < num; i++)
        avg += data[i];

    return (avg / num);
}

static int vt1603_fifo_push(struct vt1603_fifo *Fifo, int Data)
{
	Fifo->buf[Fifo->head] = Data;
	Fifo->head++;
	if(Fifo->head >= VT1603_FIFO_LEN){
		Fifo->head = 0;
		Fifo->full = 1;
	}
	
	return 0;
}

static int vt1603_fifo_avg(struct vt1603_fifo Fifo, int *Data)
{
	int i=0;
	int Sum=0,Max=0,Min=0;

	if(!Fifo.full && !Fifo.head)//FIFO is empty
		return 0;
	
	if(!Fifo.full ){
		for(i=0; i<Fifo.head; i++)
			Sum += Fifo.buf[i];

		*Data = Sum/Fifo.head;
		return 0;
	}
	
	Max = Fifo.buf[0];
	Min = Fifo.buf[0];
	for(i=0; i<VT1603_FIFO_LEN; i++){
		Sum += Fifo.buf[i];

		if(Max < Fifo.buf[i])
			Max = Fifo.buf[i];

		if(Min > Fifo.buf[i])
			Min = Fifo.buf[i];
	}
	Sum -= Max;
	Sum -= Min;
	*Data = Sum/(VT1603_FIFO_LEN-2);

	return 0;
	
}

static struct vt1603_fifo Bat_buf;

/*
 * vt1603_bat_work - vt1603 battery workqueue routine, switch 
 *  vt1603 working mode to battery detecting
 * @work: battery work struct
 */
static void vt1603_bat_work(struct work_struct *work)
{
    int tmp = 0;
    int timeout, i = 0;
    int bat_arrary[DFLT_BAT_VAL_AVG]={0};
    struct vt1603_bat_drvdata *bat_drv;

    dbg("Enter\n");
    bat_drv = container_of(work, struct vt1603_bat_drvdata, work);
    if (unlikely(vt1603_get_pen_state(bat_drv) == TS_PENDOWN_STATE)) {
        dbg("vt1603 pendown when battery detect\n");
        goto out;
    }
	
    /* enable sar-adc power and clock        */
    vt1603_bat_power_up(bat_drv);
    /* enable pen down/up to avoid miss irq  */
    vt1603_bat_pen_manual(bat_drv);
    /* switch vt1603 to battery detect mode  */
    vt1603_switch_to_bat_mode(bat_drv);
    /* do conversion use battery manual mode */
    vt1603_setbits(bat_drv, VT1603_INTS_REG, BIT0);
    vt1603_set_reg8(bat_drv, VT1603_CR_REG, BIT4);
	
    for(i=0; i<DFLT_BAT_VAL_AVG; i++){
        timeout = 2000;
    	while(--timeout && (vt1603_get_reg8(bat_drv, VT1603_INTS_REG) & BIT0)==0)
		;

        if(timeout){
	        if(vt1603_get_bat_data(bat_drv,&bat_arrary[i]) < 0) 
                dbg_err("vt1603 get bat adc data Failed!\n");
	    }else {
		    dbg_err("wait adc end timeout ?!\n");
		    goto out;
	    }
		
		vt1603_setbits(bat_drv, VT1603_INTS_REG, BIT0);
		vt1603_set_reg8(bat_drv, VT1603_CR_REG, BIT4);//start manual ADC mode
    }
    
    tmp = vt1603_bat_avg(bat_arrary,DFLT_BAT_VAL_AVG);
    bat_drv->bat_new = tmp;
    vt1603_fifo_push(&Bat_buf, tmp);
    vt1603_fifo_avg(Bat_buf, &tmp);
    bat_drv->bat_val = tmp;
    	
    //printk(KERN_ERR"reported battery val:%d, new val:%d\n",tmp, bat_drv->bat_new);
out:
    vt1603_clrbits(bat_drv, VT1603_INTCR_REG, BIT7);
    vt1603_setbits(bat_drv, VT1603_INTS_REG, BIT0 | BIT3);
    vt1603_set_reg8(bat_drv, VT1603_CR_REG, BIT1);
    mod_timer(&bat_drv->bat_tmr, jiffies + msecs_to_jiffies(DFLT_POLLING_BAT_INTERVAL* 1000));
    dbg("Exit\n\n\n");
    return ;
}

/*
 * vt1603_bat_info_init - vt1603 battery initialization
 * @bat_drv: vt1603 battery driver data
 */
static void vt1603_bat_info_init(struct vt1603_bat_drvdata *bat_drv)
{
    Bat_buf.full = 0;
    Bat_buf.head = 0;
    bat_drv->bat_val  = 3500;
    INIT_WORK(&bat_drv->work, vt1603_bat_work);
    setup_timer(&bat_drv->bat_tmr, vt1603_bat_tmr_isr, (u32)bat_drv);
    mod_timer(&bat_drv->bat_tmr,jiffies+HZ/2);

    return;
}

/*
 * vt1603_get_bat_info - get battery status, API for wmt_battery.c
 */
unsigned short vt1603_get_bat_info(int dcin_status)
{
    if (vt1603_bat == NULL)
		return 4444;
	
    if(dcin_status){
		Bat_buf.full = 0;
		Bat_buf.head = 0;
		vt1603_bat_work(&vt1603_bat->work);
		return vt1603_bat->bat_new;
    }else
        return vt1603_bat->bat_val;
	
}
EXPORT_SYMBOL_GPL(vt1603_get_bat_info);
#endif

static int __devinit vt1603_bat_probe(struct platform_device *pdev)
{
    int ret = 0;
    struct vt1603_bat_drvdata *bat_drv = vt1603_bat;

    dbg("Enter\n");		
    bat_drv->alarm_threshold  = 0xc0;
    bat_drv->wakeup_src = BA_WAKEUP_SRC_0;
    bat_drv->tdev = dev_get_platdata(&pdev->dev);
    dev_set_drvdata(&pdev->dev, bat_drv);
    
#ifdef VT1603_BATTERY_EN
    vt1603_bat_info_init(bat_drv);
   // vt1603_bat_work(&bat_drv->work);
#endif
    vt1603_reg_dump(bat_drv, 0xc0, 12);
    //printk("VT160X SAR-ADC Battery Driver Installed!\n");
    goto out;

out:
    dbg("Exit\n");
    return ret;
}

#ifdef CONFIG_VT1603_BATTERY_ALARM
static void 
vt1603_gpio1_low_active_setup(struct vt1603_bat_drvdata *bat_drv)
{
    /* mask other module interrupts */
    vt1603_set_reg8(bat_drv, VT1603_IMASK_REG27, 0xFF);
    vt1603_set_reg8(bat_drv, VT1603_IMASK_REG28, 0xFF);
    vt1603_set_reg8(bat_drv, VT1603_IMASK_REG29, 0xFF);
    /* gpio1 low active             */
    vt1603_set_reg8(bat_drv, VT1603_IPOL_REG33, 0xFF);
    /* vt1603 gpio1 as IRQ output   */
    vt1603_set_reg8(bat_drv, VT1603_ISEL_REG36, 0x04);
    /* clear vt1603 irq             */
    vt1603_setbits(bat_drv, VT1603_INTS_REG, BIT0 | BIT1 | BIT2 | BIT3);
}

static void 
wm8650_wakeup_init(int wk_src)
{
    dbg("Enter\n");
    dbg("vt1603 battery alarm use wakeup src:0x%08x\n", wk_src);
    /* disable WAKE_UP first  */
    REG8_VAL(GPIO_CTRL_GP2_WAKEUP_SUS_BYTE_ADDR) &= ~wk_src;
    /* disable WAKE_UP output */
    REG8_VAL(GPIO_OC_GP2_WAKEUP_SUS_BYTE_ADDR) &= ~wk_src;
    /* enable pull high/low   */
    REG8_VAL(GPIO_PULL_EN_GP2_WAKEUP_SUS_BYTE_ADDR) |= wk_src;
    /* set pull high WAKE_UP  */
    REG8_VAL(GPIO_PULL_CTRL_GP2_WAKEUP_SUS_BYTE_ADDR) |= wk_src;
    /* enable WAKE_UP last    */
    REG8_VAL(GPIO_CTRL_GP2_WAKEUP_SUS_BYTE_ADDR) |= wk_src;

    dbg("Exit\n");
    return ;
}

static int
vt1603_bat_alarm_setup(struct vt1603_bat_drvdata *bat_drv, u8 threshold, int wk_src)
{
    dbg("Enter\n");

    /* register setting  */
    vt1603_set_reg8(bat_drv, VT1603_PWC_REG,  0x08);
    vt1603_set_reg8(bat_drv, VT1603_CR_REG,   0x00);
    vt1603_set_reg8(bat_drv, VT1603_AMCR_REG, 0x01);
    vt1603_set_reg8(bat_drv, VT1603_BTHD_REG, threshold & 0xFF);
    vt1603_clrbits(bat_drv,  VT1603_BAEN_REG, BIT2);
    vt1603_setbits(bat_drv,  VT1603_BAEN_REG, BIT1 | BIT0);
    vt1603_set_reg8(bat_drv, VT1603_BCLK_REG, 0x28);
    /* vt1603 gpio1 setup */
    vt1603_gpio1_low_active_setup(bat_drv);
    /* wakeup setup       */
    wm8650_wakeup_init(wk_src);

    dbg("Exit\n");
    return 0;
}

static void 
wm8650_wakeup_exit(u8 wk_src)
{
    dbg("Enter\n");
    dbg("vt1603 battery alarm use wakeup src:0x%08x\n", wk_src);
    /* disable WAKE_UP */
    REG8_VAL(GPIO_CTRL_GP2_WAKEUP_SUS_BYTE_ADDR) &= ~wk_src;
    dbg("Exit\n");
    return ;
}

static int
vt1603_bat_alarm_release(struct vt1603_bat_drvdata *bat_drv, int wk_src)
{
    dbg("Enter\n");

    wm8650_wakeup_exit(wk_src);
    vt1603_clrbits(bat_drv, VT1603_BAEN_REG, BIT1 | BIT0);

    dbg("Exit\n");
    return 0;
}
#endif

static int __devexit vt1603_bat_remove(struct platform_device *pdev)
{
    struct vt1603_bat_drvdata *bat_drv = dev_get_drvdata(&pdev->dev);

    dbg("Enter\n");
    vt1603_bat = NULL;
#ifdef VT1603_BATTERY_EN
    del_timer_sync(&bat_drv->bat_tmr);
    cancel_work_sync(&bat_drv->work);
    bat_drv->bat_val = 0;
#endif
    dev_set_drvdata(&pdev->dev, NULL);
    kfree(bat_drv);
    bat_drv = NULL;

    dbg("Exit\n");
    return 0;
}

#ifdef CONFIG_PM
static int vt1603_bat_suspend(struct platform_device *pdev, pm_message_t message)
{
    struct vt1603_bat_drvdata *bat_drv = NULL;

    dbg("Enter\n");
    bat_drv = dev_get_drvdata(&pdev->dev);
#ifdef VT1603_BATTERY_EN
    del_timer_sync(&bat_drv->bat_tmr);
#endif
#ifdef CONFIG_VT1603_BATTERY_ALARM
    vt1603_bat_alarm_setup(bat_drv, bat_drv->alarm_threshold, 
                                bat_drv->wakeup_src);
#endif
    dbg("Exit\n");
    return 0;
}

static int vt1603_bat_resume(struct platform_device *pdev)
{
    struct vt1603_bat_drvdata *bat_drv = NULL;

    dbg("Enter\n");
    bat_drv = dev_get_drvdata(&pdev->dev);
#ifdef CONFIG_VT1603_BATTERY_ALARM
    vt1603_bat_alarm_release(bat_drv, bat_drv->wakeup_src);
#endif
#ifdef VT1603_BATTERY_EN
    Bat_buf.full = 0;
    Bat_buf.head  = 0;
    mod_timer(&bat_drv->bat_tmr, jiffies + 5*HZ);
#endif
    dbg("Exit\n");
    return 0;
}

#else
#define vt1603_bat_suspend NULL
#define vt1603_bat_resume  NULL
#endif


static struct platform_driver vt1603_bat_driver = {
    .driver    = {
        		.name  = VT1603_BAT_DRIVER,
        		.owner = THIS_MODULE,
    },
    .probe     	= vt1603_bat_probe,
    .remove    	= vt1603_bat_remove,
    .suspend   	= vt1603_bat_suspend,
    .resume   	= vt1603_bat_resume,
};


static int vt1603_bat_uboot_env_check(struct vt1603_bat_drvdata *bat_drv)
{
	int ret = 0;
	int len = 96;	
	char buf[96] = { 0 };
		
	ret = wmt_getsyspara("wmt.io.bat", buf, &len);
	if(ret ||strncmp(buf,"3",1)){
		dbg_err(" VT1603A/VT1609 Battery Disabled.\n");
		return -EIO;
	}
	
	len = sizeof(buf);
	memset(buf, 0, sizeof(buf));
	ret = wmt_getsyspara("wmt.audio.i2s", buf, &len);	
    if(ret ||strncmp(buf,"vt1603",6)){
        dbg_err("Vt1603 battery invalid, for VT1603 codec disabled.\n");
        return -EINVAL;
    }

    bat_drv->tch_enabled = 0;
    
    len = sizeof(buf);
	memset(buf, 0, sizeof(buf));
	ret = wmt_getsyspara("wmt.io.touch", buf, &len);	
    if(!strncmp(buf,"1:vt1603",8) ||!strncmp(buf,"1:vt1609",8) )
        bat_drv->tch_enabled = 1;

    return 0;
}

static int __init vt1603_bat_init(void)
{
    int ret = 0;
    struct vt1603_bat_drvdata *bat_drv = NULL;
	
    bat_drv = kzalloc(sizeof(*bat_drv), GFP_KERNEL);
    if (!bat_drv) {
        dbg_err("vt160x: alloc driver data failed\n");
        ret = -ENOMEM;
        goto out;
    }
    	
    ret = vt1603_bat_uboot_env_check(bat_drv);
    if (ret) {
        dbg_err("vt1603x uboot env check failed\n");
        goto out;                
    }

    vt1603_bat = bat_drv;
	
    ret = platform_driver_register(&vt1603_bat_driver);
    if(ret){
    	dbg_err("vt160x battery driver register failed!.\n");
    	goto out;
    }
	
    return 0;
	
out:
    kfree(bat_drv);	
    return ret;
}
late_initcall(vt1603_bat_init);

static void __exit vt1603_bat_exit(void)
{
    dbg("Enter\n");
    platform_driver_unregister(&vt1603_bat_driver);
    dbg("Exit\n");
}
module_exit(vt1603_bat_exit);

MODULE_AUTHOR("WonderMedia Technologies, Inc");
MODULE_DESCRIPTION("VT1603A SAR-ADC Battery Driver");
MODULE_LICENSE("Dual BSD/GPL");
