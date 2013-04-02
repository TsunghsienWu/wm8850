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
 
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <mach/hardware.h>

#include <linux/wmt_battery.h>

#include "bq27x00_battery.h"


#undef dbg
#define dbg(fmt, args...) if(debug_mode) printk(KERN_ALERT "[%s]: " fmt, __FUNCTION__ , ## args)

#undef errlog
#undef klog
#define errlog(fmt, args...) printk(KERN_ERR "[%s]: " fmt, __FUNCTION__, ## args)
#define klog(fmt, args...) printk(KERN_ALERT "[%s]: " fmt, __FUNCTION__, ## args)


////////////////////////////////////////////////
#define BATTERY_PROC_NAME "battery_config"
#define BATTERY_CALIBRATION_PRO_NAME "battery_calibration"


#define CHARGE_BY_ADAPTER  0
#define CHARGE_BY_USB      1

#define ADAPTER_PLUG_OUT   0
#define ADAPTER_PLUG_IN    1




static int batt_operation = 1;
static int debug_mode = 0;

static struct proc_dir_entry* bat_proc = NULL;
static struct proc_dir_entry* bat_module_change_proc = NULL;

static struct timer_list polling_timer;

static DEFINE_MUTEX(bat_lock);
static struct work_struct bat_work;
//static struct work_struct ac_work;
struct mutex work_lock;
//struct mutex ac_work_lock;

static struct battery_session{
	int low;
	int dcin;
	int status;
	int health;
	int is_batterypower;
	int capacity;
};
static struct battery_session bat_sesn = {0,1,POWER_SUPPLY_STATUS_UNKNOWN,POWER_SUPPLY_HEALTH_GOOD,true,100};
static struct battery_session bat_sesn_old = {0,1,POWER_SUPPLY_STATUS_UNKNOWN,POWER_SUPPLY_HEALTH_GOOD,true,100};

static struct wmt_battery_info *pdata;
//static unsigned int bat_temp_capacity = 50;
static int sus_update_cnt = 0;

int polling_interval= 3600;
static int bat_susp_resu_flag = 0; //0: normal, 1--suspend then resume
//int bat_capacity_prev = 100;
static int l_testval = 300;

extern int wmt_getsyspara(char *varname, char *varval, int *varlen);
extern int wmt_setsyspara(char *varname, char *varval);


static unsigned int wmt_bat_is_batterypower(void);

struct wmt_batgpio_set{ 
     unsigned int  name;        
     unsigned int  active;   
     unsigned int  bitmap;   
     unsigned int  ctradd;   
     unsigned int  icaddr;   
     unsigned int  idaddr;   
     unsigned int  ipcaddr;  
     unsigned int  ipdaddr;  
};                      

static struct wmt_batgpio_set dcin;    
static struct wmt_batgpio_set batstate;
static struct wmt_batgpio_set batlow;  


/** charge type: 0-adapter, 1-usb */
static int batt_charge_type = CHARGE_BY_ADAPTER;

static void reinit_gpio(void)
{
	REG32_VAL(dcin.ctradd) = REG32_VAL(dcin.ctradd) | dcin.bitmap;
	REG32_VAL(dcin.icaddr) = REG32_VAL(dcin.icaddr) & (~dcin.bitmap);
	REG32_VAL(dcin.ipcaddr) = REG32_VAL(dcin.ipcaddr) | dcin.bitmap;
	if(dcin.active & 0x10){
		REG32_VAL(dcin.ipdaddr) = REG32_VAL(dcin.ipdaddr) | dcin.bitmap;
	}else{
		REG32_VAL(dcin.ipdaddr) = REG32_VAL(dcin.ipdaddr) & (~dcin.bitmap);
	}

	REG32_VAL(batstate.ctradd) = REG32_VAL(batstate.ctradd) | batstate.bitmap;
	REG32_VAL(batstate.icaddr) = REG32_VAL(batstate.icaddr) & (~batstate.bitmap);
	REG32_VAL(batstate.ipcaddr) = REG32_VAL(batstate.ipcaddr) | batstate.bitmap;
	if(batstate.active & 0x10){
		REG32_VAL(batstate.ipdaddr) = REG32_VAL(batstate.ipdaddr) | batstate.bitmap;
	}else{
		REG32_VAL(batstate.ipdaddr) = REG32_VAL(batstate.ipdaddr) & (~batstate.bitmap);
	}

	REG32_VAL(batlow.ctradd) = REG32_VAL(batlow.ctradd) | batlow.bitmap;
	REG32_VAL(batlow.icaddr) = REG32_VAL(batlow.icaddr) & (~batlow.bitmap);
	REG32_VAL(batlow.ipcaddr) = REG32_VAL(batlow.ipcaddr) | batlow.bitmap;
	if(batlow.active & 0x10){
		REG32_VAL(batlow.ipdaddr) = REG32_VAL(batlow.ipdaddr) | batlow.bitmap;
	}else{
	    REG32_VAL(batlow.ipdaddr) = REG32_VAL(batlow.ipdaddr) & (~batlow.bitmap);
	}

}

static int gpio_ini(void)
{
    char buf[800];                                                                                                  
    char varname[] = "wmt.gpi.bat";                                                                                
    int varlen = 800;
    
	if (wmt_getsyspara(varname, buf, &varlen))
	{
		printk(KERN_ERR "Can't load battery driver for no setting wmt.gpi.bat !!!\n");
		return -1;
	} else {                                                                                                                               
        sscanf(buf,"[%x:%x:%x:%x:%x:%x:%x:%x][%x:%x:%x:%x:%x:%x:%x:%x][%x:%x:%x:%x:%x:%x:%x:%x]",              
            &dcin.name,                                                                               
            &dcin.active,                                                                             
            &dcin.bitmap,                                                                             
            &dcin.ctradd,                                                                             
            &dcin.icaddr,                                                                             
            &dcin.idaddr,                                                                             
            &dcin.ipcaddr,                                                                            
            &dcin.ipdaddr,                                                                            
            &batstate.name,                                                                           
            &batstate.active,                                                                         
            &batstate.bitmap,                                                                         
            &batstate.ctradd,                                                                         
            &batstate.icaddr,                                                                         
            &batstate.idaddr,                                                                         
            &batstate.ipcaddr,                                                                        
            &batstate.ipdaddr,
            &batlow.name,                                                                             
            &batlow.active,                                                                           
            &batlow.bitmap,                                                                           
            &batlow.ctradd,                                                                           
            &batlow.icaddr,                                                                           
            &batlow.idaddr,                                                                           
            &batlow.ipcaddr,                                                                          
            &batlow.ipdaddr);                                                                       
        //REG32_VAL(0xd813001c) &= ~0x0003;//disable all general purpose wake-up but low power alarm

		dcin.ctradd += WMT_MMAP_OFFSET;
		dcin.icaddr += WMT_MMAP_OFFSET;
		dcin.idaddr += WMT_MMAP_OFFSET;
		dcin.ipcaddr += WMT_MMAP_OFFSET;
		dcin.ipdaddr += WMT_MMAP_OFFSET;

		batstate.ctradd += WMT_MMAP_OFFSET;
		batstate.icaddr += WMT_MMAP_OFFSET;
		batstate.idaddr += WMT_MMAP_OFFSET;
		batstate.ipcaddr += WMT_MMAP_OFFSET;
		batstate.ipdaddr += WMT_MMAP_OFFSET;

		batlow.ctradd += WMT_MMAP_OFFSET;
		batlow.icaddr += WMT_MMAP_OFFSET;
		batlow.idaddr += WMT_MMAP_OFFSET;
		batlow.ipcaddr += WMT_MMAP_OFFSET;
		batlow.ipdaddr += WMT_MMAP_OFFSET;
	    if((dcin.name != 0) || (batstate.name != 1) || (batlow.name != 2) ){
			batt_operation = 0;
			return 0;
		}

		REG32_VAL(dcin.ctradd) = REG32_VAL(dcin.ctradd) | dcin.bitmap;
		REG32_VAL(dcin.icaddr) = REG32_VAL(dcin.icaddr) & (~dcin.bitmap);
		REG32_VAL(dcin.ipcaddr) = REG32_VAL(dcin.ipcaddr) | dcin.bitmap;
		if(dcin.active & 0x10){
			REG32_VAL(dcin.ipdaddr) = REG32_VAL(dcin.ipdaddr) | dcin.bitmap;
		}else{
			REG32_VAL(dcin.ipdaddr) = REG32_VAL(dcin.ipdaddr) & (~dcin.bitmap);
		}

		REG32_VAL(batstate.ctradd) = REG32_VAL(batstate.ctradd) | batstate.bitmap;
		REG32_VAL(batstate.icaddr) = REG32_VAL(batstate.icaddr) & (~batstate.bitmap);
		REG32_VAL(batstate.ipcaddr) = REG32_VAL(batstate.ipcaddr) | batstate.bitmap;
		if(batstate.active & 0x10){
			REG32_VAL(batstate.ipdaddr) = REG32_VAL(batstate.ipdaddr) | batstate.bitmap;
		}else{
			REG32_VAL(batstate.ipdaddr) = REG32_VAL(batstate.ipdaddr) & (~batstate.bitmap);
		}

		REG32_VAL(batlow.ctradd) = REG32_VAL(batlow.ctradd) | batlow.bitmap;
		REG32_VAL(batlow.icaddr) = REG32_VAL(batlow.icaddr) & (~batlow.bitmap);
		REG32_VAL(batlow.ipcaddr) = REG32_VAL(batlow.ipcaddr) | batlow.bitmap;
		if(batlow.active & 0x10){
			REG32_VAL(batlow.ipdaddr) = REG32_VAL(batlow.ipdaddr) | batlow.bitmap;
		}else{
		    REG32_VAL(batlow.ipdaddr) = REG32_VAL(batlow.ipdaddr) & (~batlow.bitmap);
		}
    }

	return 0;
}

static int wmt_batt_initPara(void)
{
	/*char buf[200];
	char varname[] = "wmt.io.bat";
	int varlen = 200;

	printk("Bow: wmt_batt_initPara  \n");

	if (wmt_getsyspara("wmt.io.batt.fullmax", buf, &varlen) == 0)
	{
		sscanf(buf, "%d", &judge_charge_full);
	}
	*/




	return 0;
}

static unsigned long wmt_read_voltage(struct power_supply *bat_ps)
{
	/*printk("Bow: read sbat...\n");*/
	return 4000;
	/*
	return wmt_read_aux_adc(bat_ps->dev->parent->driver_data,
					pdata->batt_aux) * pdata->batt_mult /
					pdata->batt_div;
	*/				
}

static unsigned long wmt_read_temp(struct power_supply *bat_ps)
{
	/*printk("Bow: read temp...\n");*/
	return 50;
}

//static unsigned long wmt_read_dcin_DT(struct power_supply *bat_ps)
//return:1--external adpater plug-in,0--external adapter plug-out
static int wmt_read_dcin_DT(struct power_supply *bat_ps)
{
	return bq27x00_is_charging();
}

//return:1--external adpater plug-in,0--external adapter plug-out
int wmt_read_dcin_state(void)
{
/*	int status;

	//printk("Bow: read battlow...\n");
    // WAKE_UP1 
	status = (REG32_VAL(dcin.idaddr) & dcin.bitmap) ? ADAPTER_PLUG_IN: ADAPTER_PLUG_OUT;
	if(!(dcin.active & 0x01)){
        status = !status;
	}
	return status;
	*/
	return bq27x00_is_charging();
}
//EXPORT_SYMBOL(wmt_read_dcin_state);

static void wmt_gpio8(int high)
{
	GPIO_CTRL_GP1_BYTE_VAL |=0x1;
	GPIO_OC_GP1_BYTE_VAL |=0x1;
	if(high)
		GPIO_OD_GP1_BYTE_VAL |=0x1;
	else
		GPIO_OD_GP1_BYTE_VAL &=0xfe;
	
}

static unsigned long wmt_read_status(struct power_supply *bat_ps)
{
	int status;
	int full = 0;

	dbg("Bow: read status...\n");
	full = (REG32_VAL(batstate.idaddr)& batstate.bitmap);
	dbg("full falg is  %x\n", full);
	dbg("plug_inout flag is %x\n", REG32_VAL(dcin.idaddr)&dcin.bitmap);

	if (!wmt_bat_is_batterypower())
	{
		// external adapter
	    if(!(batstate.active & 0x01))
		{			
			if (full)
			{
				status = POWER_SUPPLY_STATUS_FULL;
				dbg(" Full...\n");
			} else {
				status = POWER_SUPPLY_STATUS_CHARGING;
				dbg(" Charging...\n");
			}		
		} else {
			if (!full)
			{
				status = POWER_SUPPLY_STATUS_FULL;
				dbg(" Full...\n");
			} else {
				status = POWER_SUPPLY_STATUS_CHARGING;
				dbg(" Charging...\n");
			}
		}
		
	} else
	{
		// battery 
		status = POWER_SUPPLY_STATUS_DISCHARGING;
		dbg(" Discharging...\n");
	}
	
	return status;
}

static unsigned long wmt_read_health(struct power_supply *bat_ps)
{
	return POWER_SUPPLY_HEALTH_GOOD;
}

static int wmt_bat_get_property(struct power_supply *bat_ps,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	int ret = 0;

	/*if (psp != POWER_SUPPLY_PROP_PRESENT && di->cache.flags < 0)
		return -ENODEV;
	*/
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = wmt_read_status(bat_ps);//bq27x00_battery_status();
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = bq27x00_battery_voltage();
		if (ret>=0) val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = bat_sesn.is_batterypower;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = bq27x00_battery_current();
		if (ret>=0) val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (POWER_SUPPLY_STATUS_FULL == wmt_read_status(bat_ps))
		{
			ret = 100;
		} else {
			ret = bq27x00_get_capacity();
		}
		if (ret>=0) val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = bq27x00_battery_temperature();
		if (ret>=0) val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		ret = bq27x00_battery_time_to_empty();
		if (ret>=0) val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
		ret = bq27x00_battery_time_to_empty_avg();
		if (ret>=0) val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		ret = bq27x00_battery_time_to_full();
		if (ret>=0) val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		ret = POWER_SUPPLY_TECHNOLOGY_LION;
		if (ret>=0) val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		ret = bq27x00_battery_charge_now();
		if (ret>=0) val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret  = bq27x00_battery_charge_full();
		if (ret>=0) val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		ret = bq27x00_battery_charge_design();
		if (ret>=0) val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		ret = bq27x00_battery_cycle_count();
		if (ret>=0) val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		ret = bq27x00_battery_energy();
		if (ret>=0) val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
	    /*printk("Bow:POWER_SUPPLY_PROP_HEALTH...\n");*/
		val->intval = bat_sesn.health;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}


/*
static void wmt_bat_external_power_changed(struct power_supply *bat_ps)
{
	//printk("wmt_bat_external_power_changed======================\n");

	schedule_work(&bat_work);
}
*/

// return 1--battery power, 0--external power
static unsigned int wmt_bat_is_batterypower(void)
{
	//return ((bat_sesn.dcin !=0 ) ? 0 : 1);
	return bq27x00_is_charging()?0:1;
}


static void wmt_bat_update(struct power_supply *bat_ps)
{
	int update = 0;
	
	mutex_lock(&work_lock);
	update = bq27x00_update();

	if(batt_operation){
		bat_sesn.dcin= wmt_read_dcin_DT(bat_ps);
		bat_sesn.status = wmt_read_status(bat_ps);
		bat_sesn.health = wmt_read_health(bat_ps);
		bat_sesn.is_batterypower = wmt_bat_is_batterypower();
		//bat_sesn.low = wmt_read_batlow_DT(bat_ps);
		bat_sesn.capacity = bq27x00_get_capacity();
		dbg("capacity=%d\n",bq27x00_get_capacity());
		dbg("temparature=%d\n",bq27x00_battery_temperature());
		dbg("flags=0x%x\n", bq27x00_get_flag());
	}	
    
    if(update){
		power_supply_changed(bat_ps);
    }
	
	mutex_unlock(&work_lock);
}


static enum power_supply_property wmt_bat_props[] = 
{
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_HEALTH,
};

/*{
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_TECHNOLOGY,
//#if 0	
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MIN,
//#endif
   // POWER_SUPPLY_PROP_ONLINE,
    POWER_SUPPLY_PROP_CAPACITY,
    POWER_SUPPLY_PROP_HEALTH,
	
};*/

static struct power_supply bat_ps = {
	.name					= "wmt-battery",
	.type					= POWER_SUPPLY_TYPE_BATTERY,
	.get_property			= wmt_bat_get_property,
	//.external_power_changed = wmt_bat_external_power_changed,
	.properties 			= wmt_bat_props,
	.num_properties 		= ARRAY_SIZE(wmt_bat_props),
	.use_for_apm			= 1,
};

static void wmt_battery_work(struct work_struct *work)
{
    //printk("wmt_bat_work\n");
	/*bat_sesn.dcin= wmt_read_dcin_DT(NULL);
	if(bat_sesn.dcin){
		if(wmt_read_status(NULL)==POWER_SUPPLY_STATUS_CHARGING){
			wmt_gpio8(0);
		}else{
			wmt_gpio8(1);
		}
	}else{
		wmt_gpio8(1);
	}
	*/
	
	wmt_bat_update(&bat_ps);
}


static void polling_timer_func(unsigned long unused)
{
	/*printk("Bow: polling...\n");*/
	schedule_work(&bat_work);
	//schedule_work(&ac_work);
	polling_interval = 3600;
	mod_timer(&polling_timer,
		  jiffies + msecs_to_jiffies(polling_interval));

	dbg("polling, polling_interval:%d\n", polling_interval);
	/*printk("Bow: polling ^^^\n");*/
}

#ifdef CONFIG_PM
static int wmt_battery_suspend(struct platform_device *dev, pm_message_t state)
{
	/*flush_scheduled_work();*/
	del_timer(&polling_timer);
	/*mutex_lock(&work_lock);
	bat_susp_resu_flag = 1;
	sus_update_cnt = 0;
	mutex_unlock(&work_lock);
	*/
	return 0;
}

static int wmt_battery_resume(struct platform_device *dev)
{
	//if(batt_operation){
		//gpio_ini();
	//}
	//schedule_work(&bat_work);
	//schedule_work(&ac_work);
	reinit_gpio();
	setup_timer(&polling_timer, polling_timer_func, 0);
	//polling_interval = 500;
	mod_timer(&polling_timer,
		  jiffies + msecs_to_jiffies(polling_interval));
	return 0;
}
#else
#define wmt_battery_suspend NULL
#define wmt_battery_resume NULL
#endif

/*****************************************************************/
/*                              AC                               */
/*****************************************************************/
static int wmt_ac_get_property(struct power_supply *ac_ps,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	switch (psp) {
		case POWER_SUPPLY_PROP_ONLINE:
					val->intval = (wmt_bat_is_batterypower()? 0 : 1);
			break;		

		default:
			return -EINVAL;
	}
	return 0;
}

static enum power_supply_property wmt_ac_props[] = {
    POWER_SUPPLY_PROP_ONLINE,
};

static struct power_supply ac_ps = {
	.name					= "wmt-ac",
	.type					= POWER_SUPPLY_TYPE_MAINS,
	.get_property			= wmt_ac_get_property,
	//.external_power_changed = wmt_ac_external_power_changed,
	.properties 			= wmt_ac_props,
	.num_properties 		= ARRAY_SIZE(wmt_ac_props),
	.use_for_apm			= 1,
};


/*****************************************************************/
/*                         AC     End                            */
/*****************************************************************/

static int __devinit wmt_battery_probe(struct platform_device *dev)
{
	int ret = 0;

	pdata = dev->dev.platform_data;
	if (dev->id != -1)
		return -EINVAL;

	mutex_init(&work_lock);
	//mutex_init(&ac_work_lock);

	if (!pdata) {
		dev_err(&dev->dev, "Please use wmt_bat_set_pdata\n");
		return -EINVAL;
	}

	INIT_WORK(&bat_work, wmt_battery_work);
	//INIT_WORK(&ac_work, wmt_ac_work);
	if (!pdata->batt_name) {
		dev_info(&dev->dev, "Please consider setting proper battery "
				"name in platform definition file, falling "
				"back to name \"wmt-batt\"\n");
		bat_ps.name = "wmt-batt";
	} else
		bat_ps.name = pdata->batt_name;

	//bq27x00_battery_i2c_init();

	if (power_supply_register(&dev->dev, &bat_ps) != 0)
	{
		goto err;
	}
	if (power_supply_register(&dev->dev, &ac_ps) != 0) 
	{
		goto err;
	}

     /*************     AC     ************/
	
	//printk("Bow: Probe...0\n");
	setup_timer(&polling_timer, polling_timer_func, 0);
	mod_timer(&polling_timer,
			  jiffies + msecs_to_jiffies(polling_interval));	

	return 0;
err:
	//bq27x00_battery_i2c_exit();
	return ret;
}

static int __devexit wmt_battery_remove(struct platform_device *dev)
{

	flush_scheduled_work();
	del_timer_sync(&polling_timer);
	
	/*printk("Bow: remove...\n");*/
	power_supply_unregister(&bat_ps);
	power_supply_unregister(&ac_ps);
	//bq27x00_battery_i2c_exit();

	return 0;
}

static struct wmt_battery_info wmt_battery_pdata = {
        .charge_gpio    = 5,
        .max_voltage    = 4200,
        .min_voltage    = 3400,
        .batt_mult      = 1000,
        .batt_div       = 414,
        .temp_mult      = 1,
        .temp_div       = 1,
        .batt_tech      = POWER_SUPPLY_TECHNOLOGY_LION,
        .batt_name      = "wmt-battery",
};

void wmt_battery_release(struct device *dev)
{
	printk(KERN_ALERT "%s ...\n", __FUNCTION__);
}

static struct platform_device wmt_battery_device = {
	.name           = "wmt-battery",
	.id             = -1,
	.dev            = {
		.platform_data = &wmt_battery_pdata,
		.release = wmt_battery_release,
	
	},
};


static struct platform_driver wmt_battery_driver = {
	.driver	= {
		.name	= "wmt-battery",
		.owner	= THIS_MODULE,
	},
	.probe		= wmt_battery_probe, 
	.remove		= __devexit_p(wmt_battery_remove),
	.suspend	= wmt_battery_suspend,
	.resume		= wmt_battery_resume,
};

static int batt_writeproc( struct file   *file,
                           const char    *buffer,
                           unsigned long count,
                           void          *data )
{
	unsigned int volt[11]; //v10-v0:the voltage of every 10% capacity. Its unit is 10mv.
	unsigned int vc,vp; // vc--the current voltage,vp--the adc value related to vc
	int i,j;
	unsigned cmin,swap;
	char buff[200];
	char varname[] = "wmt.io.bat";
	
	if (sscanf(buffer, "dbg=%d", &debug_mode))
	{
		dbg("dbg=%d\n", debug_mode);
	} else if (sscanf(buffer, "temp=%d", &l_testval))
	{
		dbg("temperature testval=%d\n",l_testval);
	}
	/*
    else if(sscanf(buffer, "verbose_print=%d", &verbose_print)){
        dbg("verbose_print=%d\n", verbose_print);
	}*/
	else if (sscanf(buffer, "btcurve=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%x\n",
	           volt,volt+1,volt+2,volt+3,volt+4,volt+5,volt+6,volt+7,volt+8,volt+9,
	           volt+10,&vc,&vp))
	{
		//volt[10]--100% capacity, volt[9]--90% capacity,...,volt[0]--0% capacity
		// sort the input data
		for (i=0;i<10;i++)
		{
			cmin = i;
			for (j=i+1;j<=10;j++)
			{
				if (volt[cmin] > volt[j])
				{
					cmin = j;
				}
			};
			if (cmin != i)
			{
				swap = volt[cmin];
				volt[cmin] = volt[i];
				volt[i] = swap;				
			}
		};
		klog("sort result:\n");
		for (i=0;i<=10;i++)
		{
			klog("%d,", volt[i]);
		}; // only for dbg
		klog("\n");		
	}
	return count;
}


static int batt_readproc(char *page, char **start, off_t off,
			  int count, int *eof, void *data)
{
	int len = 0;
	int power_source; // 0--battery;1--external adapter.

	mutex_lock(&work_lock);
	bat_sesn.dcin= wmt_read_dcin_DT(NULL);
	if (wmt_bat_is_batterypower())
	{
		power_source = 0;
	} else {
		power_source = 1;
	}
	mutex_unlock(&work_lock);
	len = sprintf(page, "supply_power=%d\ndebug_mode=%d\n",
					power_source, debug_mode);
	dbg("supply_power=%d\n", power_source);
	return len;
}

static int mc_readproc(char *page, char **start, off_t off,
			  int count, int *eof, void *data)
{
	int len = 0;
	
	unsigned short adc_val=3800;
	int capacity;
	int cap_add = 0;

	mutex_lock(&work_lock);	
    adc_val = bq27x00_battery_voltage();
	capacity = bq27x00_get_capacity();
	mutex_unlock(&work_lock);
	dbg("vol:%d,capacity:%d\n",adc_val,capacity);
	len = sprintf(page, "adc:%d,cap1:%d,cap2:%d",
					adc_val,capacity,cap_add);
	return len;
}

static int mc_writeproc( struct file   *file,
                           const char    *buffer,
                           unsigned long count,
                           void          *data )
{
	return count;
}


extern int (*other_battery_dcin_state)(void);
static int __init wmt_battery_init(void)
{
	int ret;
	
	/*if(wmt_batt_initPara())
	{
		return -1;
	}
	*/
	if(batt_operation){
		if (gpio_ini())
		{
			return -1;
		}
	}
	if (bq27x00_battery_i2c_init())
	{
		return -2;
	}
	
	ret = platform_device_register(&wmt_battery_device);
	if (ret != 0) {
		/*DPRINTK("End1 ret = %x\n",ret);*/
		return -ENODEV;
	}
	ret = platform_driver_register(&wmt_battery_driver);

	bat_proc = create_proc_entry(BATTERY_PROC_NAME, 0666, NULL/*&proc_root*/);
    if (bat_proc != NULL)
    {
    	bat_proc->read_proc = batt_readproc;
    	bat_proc->write_proc = batt_writeproc;
    }
    bat_module_change_proc = create_proc_entry(BATTERY_CALIBRATION_PRO_NAME, 0666, NULL/*&proc_root*/);
    if (bat_module_change_proc != NULL)
    {
    	bat_module_change_proc->read_proc = mc_readproc;
    	bat_module_change_proc->write_proc = mc_writeproc;
    }

	other_battery_dcin_state = bq27x00_is_charging;

	return ret;
}

static void __exit wmt_battery_exit(void)
{
	if (bat_proc != NULL)
	{
		remove_proc_entry(BATTERY_PROC_NAME, NULL);
	}
	platform_driver_unregister(&wmt_battery_driver);
	platform_device_unregister(&wmt_battery_device);
	bq27x00_battery_i2c_exit();
}


module_init(wmt_battery_init);
module_exit(wmt_battery_exit);

MODULE_AUTHOR("WonderMedia Technologies, Inc.");
MODULE_DESCRIPTION("WONDERMEDIA battery driver");
MODULE_LICENSE("GPL");

