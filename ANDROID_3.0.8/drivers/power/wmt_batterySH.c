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
//#include <linux/wm97xx.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <mach/gpio-cs.h>
#include <linux/proc_fs.h>
//#include <linux/wm97xx_batt.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <mach/hardware.h>
//#include <mach/wmt_spi.h>

#include <linux/wmt_battery.h>

////////////////////////////////////////////////
#define BATTERY_PROC_NAME "battery_config"
#define BATTERY_CALIBRATION_PRO_NAME "battery_calibration"

#define BATTERY_READ_VT1603_I2C      1
#define BATTERY_READ_UNDIRECT_SPI   2
#define BATTERY_READ_VT1603_SPI      3

#define CHARGE_BY_ADAPTER  0
#define CHARGE_BY_USB      1

#define ADAPTER_PLUG_OUT   0
#define ADAPTER_PLUG_IN    1

#define JUDGE_HOP_VOLTAGE_TIMES    4

typedef enum 
{
    BM_VOLUME        = 0,
    BM_BRIGHTNESS    = 1,
    BM_WIFI          = 2,
    BM_VIDEO         = 3,
    BM_USBDEVICE     = 4,
    BM_HDMI          = 5,
    BM_COUNT		
} BatteryModule;

//FIXME:default brightness is on
//Different module's(volume,brightness,wifi,video,usb,hdmi) default value.
int module_usage[BM_COUNT]={0, 100, 0, 0, 0, 0};

typedef struct {
   int voltage_effnorm;
   int voltage_effmax;
}module_effect;

///////////////////////////////////////////////////////////     
static struct battery_module_calibration{
    module_effect brightness_ef[10];//brghtness effect
    module_effect wifi_ef[10];//wifi effect
    module_effect adapter_ef[10];//adapter effect
}module_calibration;

static struct battery_config{
    int update_time ;
    int wmt_operation;
    int ADC_USED;
    int ADC_TYP; // 1--read capability from vt1603 by SPI;3--Read capability from vt1603 by I2C
    int charge_max;         
    int charge_min;   
    int discharge_vp[11]; //battery capacity curve:v0--v10 
}bat_config={5000,0,0,1,0xB78,0x972,
				0xF5A,0xEE2,0xE92,0xE55,0xE1C,0xDDB,0xDB5,0xD9D,0xD95,0xD6A,0xCD1};

static int batt_operation = 0;
//static int ac_dcin = 1;
//static int ac_dcin_old = 1;
static int judge_charge_full = 0; // 1: Judge full by the charging capativity

static int debug_mode = 0; //report fake capacity  
static int verbose_print = 0; //Whether show debug info
static struct proc_dir_entry* bat_proc = NULL;
static struct proc_dir_entry* bat_module_change_proc = NULL;

static bool mBrightnessHasSet = false;
/*static bool isFromAdapterPlugOut = false;*/
static bool mIsForOldVersion = false;//android4.1:false, android4.0.3:true.

/* Debug macros */
#undef dbg
#if 1
	#define dbg(format, arg...) if (verbose_print) printk(KERN_ALERT "[%s]: " format, __func__, ## arg)
#else
	#define dbg(format, arg...)
#endif

#undef klog
#define klog(format, arg...) printk(KERN_ALERT "[%s]: " format, __func__, ## arg)

int adc_repair(int adc);
int adc2cap(int adc);

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

int polling_interval= 1000;
static int bat_susp_resu_flag = 0; //0: normal, 1--suspend then resume
int bat_capacity_prev = 100;

extern int wmt_getsyspara(char *varname, char *varval, int *varlen);
extern int wmt_setsyspara(char *varname, char *varval);
extern int android_usb_connect_state(void);

//#ifdef CONFIG_BATTERY_VT1603
//extern unsigned short vt1603_get_bat_info(void);
extern unsigned short vt1603_get_bat_info(int dcin_status);
//#endif

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
struct gpio_operation_t {
	unsigned int id;
	unsigned int act;
	unsigned int bitmap;
	unsigned int ctl;
	unsigned int oc;
	unsigned int od;
};
#define GPIO_PHY_BASE_ADDR (GPIO_BASE_ADDR-WMT_MMAP_OFFSET)

static struct wmt_batgpio_set dcin;    
static struct wmt_batgpio_set batstate;
static struct wmt_batgpio_set batlow;  
static struct gpio_operation_t s_power_state_pin;


/** charge type: 0-adapter, 1-usb */
static int batt_charge_type = CHARGE_BY_ADAPTER;

int wmt_batt_charge_type(void)
{
	return batt_charge_type;
}
EXPORT_SYMBOL(wmt_batt_charge_type);


static int gpio_ini(void)
{
    //gpio_enable(GP2,6,OUTPUT);
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

//--> Initialize power state Pin
	memset(&s_power_state_pin, 0, sizeof(struct gpio_operation_t));
	if( wmt_getsyspara("wmt.gpo.powerstate",buf,&varlen) == 0) 
	{
		sscanf(buf,"%d:%d:%x:%x:%x:%x",&s_power_state_pin.id, &s_power_state_pin.act, &s_power_state_pin.bitmap, &s_power_state_pin.ctl, \
			&s_power_state_pin.oc, &s_power_state_pin.od);
		if ((s_power_state_pin.ctl&0xffff0000) == GPIO_PHY_BASE_ADDR)
			s_power_state_pin.ctl += WMT_MMAP_OFFSET;
		if ((s_power_state_pin.oc&0xffff0000) == GPIO_PHY_BASE_ADDR)
			s_power_state_pin.oc += WMT_MMAP_OFFSET;
		if ((s_power_state_pin.od&0xffff0000) == GPIO_PHY_BASE_ADDR)
			s_power_state_pin.od += WMT_MMAP_OFFSET;

		printk("[power_state] id = 0x%x, act = 0x%x, bitmap = 0x%x, ctl = 0x%x, oc = 0x%x, od = 0x%x\n", 
			s_power_state_pin.id, s_power_state_pin.act, s_power_state_pin.bitmap, s_power_state_pin.ctl,
			s_power_state_pin.oc, s_power_state_pin.od);
	}
	else
		printk("[%s] Not define power_state pin\n", __func__);


	return 0;
}

static int wmt_batt_initPara(void)
{
	char buf[200];
	char varname[] = "wmt.io.bat";
	int varlen = 200;

	printk("Bow: wmt_batt_initPara  \n");

	if (wmt_getsyspara("wmt.io.batt.fullmax", buf, &varlen) == 0)
	{
		sscanf(buf, "%d", &judge_charge_full);
	}

	if (wmt_getsyspara(varname, buf, &varlen) == 0){
		sscanf(buf,"%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x",
				&bat_config.ADC_TYP,
			    &bat_config.wmt_operation,
			    &bat_config.update_time,
			    &bat_config.charge_max,
			    &bat_config.charge_min,
			    &bat_config.discharge_vp[10],
			    &bat_config.discharge_vp[9],
			    &bat_config.discharge_vp[8],
			    &bat_config.discharge_vp[7],
   			    &bat_config.discharge_vp[6],
			    &bat_config.discharge_vp[5],
			    &bat_config.discharge_vp[4],
			    &bat_config.discharge_vp[3],
			    &bat_config.discharge_vp[2],
			    &bat_config.discharge_vp[1],
			    &bat_config.discharge_vp[0]);
		switch( bat_config.wmt_operation){
			case 0: /* for RDK */
				batt_operation = 0;
				break;
			case 1: /* for droid book */
				batt_operation = 1;
				bat_config.ADC_USED= 0;
				break;
			case 2: /* for MID */
				batt_operation = 1;
				bat_config.ADC_USED= 1;
				break;
			default:
				batt_operation = 1;
				bat_config.ADC_USED= 0;
				break;
		}
	}else{
		bat_config.charge_max = 0xE07;         
		bat_config.charge_min = 0xC3A;       
		bat_config.discharge_vp[10] = 0xeb3; 
		bat_config.discharge_vp[9] = 0xe71;   
		bat_config.discharge_vp[8] = 0xe47;   
		bat_config.discharge_vp[7] = 0xe1c;   
		bat_config.discharge_vp[6] = 0xdf0;   
		bat_config.discharge_vp[5] = 0xdc7;  
		bat_config.discharge_vp[4] = 0xda0;  
		bat_config.discharge_vp[3] = 0xd6c;  
		bat_config.discharge_vp[2] = 0xd30;  
		bat_config.discharge_vp[1] = 0xceb;  
		bat_config.discharge_vp[0] = 0xcb0;   
		bat_config.ADC_USED = 0;
		batt_operation = 1;
	}

	memset(&module_calibration, 0, sizeof(struct battery_module_calibration));
	
    if(wmt_getsyspara("wmt.io.bateff.brightness", buf, &varlen) == 0){
		sscanf(buf,"%x-%x:%x-%x:%x-%x:%x-%x:%x-%x:%x-%x:%x-%x:%x-%x:%x-%x:%x-%x",
			&module_calibration.brightness_ef[9].voltage_effnorm,
			&module_calibration.brightness_ef[9].voltage_effmax,
			&module_calibration.brightness_ef[8].voltage_effnorm,
			&module_calibration.brightness_ef[8].voltage_effmax,			
			&module_calibration.brightness_ef[7].voltage_effnorm,
			&module_calibration.brightness_ef[7].voltage_effmax,			
			&module_calibration.brightness_ef[6].voltage_effnorm,
			&module_calibration.brightness_ef[6].voltage_effmax,
			&module_calibration.brightness_ef[5].voltage_effnorm,
			&module_calibration.brightness_ef[5].voltage_effmax,
			&module_calibration.brightness_ef[4].voltage_effnorm,
			&module_calibration.brightness_ef[4].voltage_effmax,
			&module_calibration.brightness_ef[3].voltage_effnorm,
			&module_calibration.brightness_ef[3].voltage_effmax,			
			&module_calibration.brightness_ef[2].voltage_effnorm,
			&module_calibration.brightness_ef[2].voltage_effmax,			
			&module_calibration.brightness_ef[1].voltage_effnorm,
			&module_calibration.brightness_ef[1].voltage_effmax,
			&module_calibration.brightness_ef[0].voltage_effnorm,
			&module_calibration.brightness_ef[0].voltage_effmax);
	}else{
			module_calibration.brightness_ef[9].voltage_effnorm = 0xeb1;
			module_calibration.brightness_ef[9].voltage_effmax  = 0xef3;
			module_calibration.brightness_ef[8].voltage_effnorm = 0xe57;
			module_calibration.brightness_ef[8].voltage_effmax  = 0xe98;			
			module_calibration.brightness_ef[7].voltage_effnorm = 0xe13;
			module_calibration.brightness_ef[7].voltage_effmax  = 0xe55;			
			module_calibration.brightness_ef[6].voltage_effnorm = 0xdcc;
			module_calibration.brightness_ef[6].voltage_effmax  = 0xe0f;
			module_calibration.brightness_ef[5].voltage_effnorm = 0xdaa;
			module_calibration.brightness_ef[5].voltage_effmax  = 0xdee;
			module_calibration.brightness_ef[4].voltage_effnorm = 0xd83;
			module_calibration.brightness_ef[4].voltage_effmax  = 0xdca;
			module_calibration.brightness_ef[3].voltage_effnorm = 0xd56;
			module_calibration.brightness_ef[3].voltage_effmax  = 0xda0;			
			module_calibration.brightness_ef[2].voltage_effnorm = 0xd24;
			module_calibration.brightness_ef[2].voltage_effmax  = 0xd71;			
			module_calibration.brightness_ef[1].voltage_effnorm = 0xce9;
			module_calibration.brightness_ef[1].voltage_effmax  = 0xd43;
			module_calibration.brightness_ef[0].voltage_effnorm = 0xcbf;
			module_calibration.brightness_ef[0].voltage_effmax  = 0xd18;
	}

    if(wmt_getsyspara("wmt.io.bateff.wifi", buf, &varlen) == 0){
		sscanf(buf,"%x-%x:%x-%x:%x-%x:%x-%x:%x-%x:%x-%x:%x-%x:%x-%x:%x-%x:%x-%x",
			&module_calibration.wifi_ef[9].voltage_effnorm,
			&module_calibration.wifi_ef[9].voltage_effmax,
			&module_calibration.wifi_ef[8].voltage_effnorm,
			&module_calibration.wifi_ef[8].voltage_effmax,			
			&module_calibration.wifi_ef[7].voltage_effnorm,
			&module_calibration.wifi_ef[7].voltage_effmax,			
			&module_calibration.wifi_ef[6].voltage_effnorm,
			&module_calibration.wifi_ef[6].voltage_effmax,
			&module_calibration.wifi_ef[5].voltage_effnorm,
			&module_calibration.wifi_ef[5].voltage_effmax,
			&module_calibration.wifi_ef[4].voltage_effnorm,
			&module_calibration.wifi_ef[4].voltage_effmax,
			&module_calibration.wifi_ef[3].voltage_effnorm,
			&module_calibration.wifi_ef[3].voltage_effmax,			
			&module_calibration.wifi_ef[2].voltage_effnorm,
			&module_calibration.wifi_ef[2].voltage_effmax,			
			&module_calibration.wifi_ef[1].voltage_effnorm,
			&module_calibration.wifi_ef[1].voltage_effmax,
			&module_calibration.wifi_ef[0].voltage_effnorm,
			&module_calibration.wifi_ef[0].voltage_effmax);
	}else{
			module_calibration.wifi_ef[9].voltage_effnorm = 0xea8;
			module_calibration.wifi_ef[9].voltage_effmax  = 0xea0;
			module_calibration.wifi_ef[8].voltage_effnorm = 0xe57;
			module_calibration.wifi_ef[8].voltage_effmax  = 0xe3d;			
			module_calibration.wifi_ef[7].voltage_effnorm = 0xe12;
			module_calibration.wifi_ef[7].voltage_effmax  = 0xdf6;			
			module_calibration.wifi_ef[6].voltage_effnorm = 0xdcc;
			module_calibration.wifi_ef[6].voltage_effmax  = 0xdaf;
			module_calibration.wifi_ef[5].voltage_effnorm = 0xda9;
			module_calibration.wifi_ef[5].voltage_effmax  = 0xda5;
			module_calibration.wifi_ef[4].voltage_effnorm = 0xd82;
			module_calibration.wifi_ef[4].voltage_effmax  = 0xd7e;
			module_calibration.wifi_ef[3].voltage_effnorm = 0xd55;
			module_calibration.wifi_ef[3].voltage_effmax  = 0xd50;			
			module_calibration.wifi_ef[2].voltage_effnorm = 0xd24;
			module_calibration.wifi_ef[2].voltage_effmax  = 0xd1d;			
			module_calibration.wifi_ef[1].voltage_effnorm = 0xce8;
			module_calibration.wifi_ef[1].voltage_effmax  = 0xccc;
			module_calibration.wifi_ef[0].voltage_effnorm = 0xcbe;
			module_calibration.wifi_ef[0].voltage_effmax  = 0xcb1;
	}
	
    if(wmt_getsyspara("wmt.io.bateff.adapter", buf, &varlen) == 0){
		sscanf(buf,"%x-%x:%x-%x:%x-%x:%x-%x:%x-%x:%x-%x:%x-%x:%x-%x:%x-%x:%x-%x",
			&module_calibration.adapter_ef[9].voltage_effnorm,
			&module_calibration.adapter_ef[9].voltage_effmax,
			&module_calibration.adapter_ef[8].voltage_effnorm,
			&module_calibration.adapter_ef[8].voltage_effmax,			
			&module_calibration.adapter_ef[7].voltage_effnorm,
			&module_calibration.adapter_ef[7].voltage_effmax,			
			&module_calibration.adapter_ef[6].voltage_effnorm,
			&module_calibration.adapter_ef[6].voltage_effmax,
			&module_calibration.adapter_ef[5].voltage_effnorm,
			&module_calibration.adapter_ef[5].voltage_effmax,
			&module_calibration.adapter_ef[4].voltage_effnorm,
			&module_calibration.adapter_ef[4].voltage_effmax,
			&module_calibration.adapter_ef[3].voltage_effnorm,
			&module_calibration.adapter_ef[3].voltage_effmax,			
			&module_calibration.adapter_ef[2].voltage_effnorm,
			&module_calibration.adapter_ef[2].voltage_effmax,			
			&module_calibration.adapter_ef[1].voltage_effnorm,
			&module_calibration.adapter_ef[1].voltage_effmax,
			&module_calibration.adapter_ef[0].voltage_effnorm,
			&module_calibration.adapter_ef[0].voltage_effmax);
	}else{
			module_calibration.adapter_ef[9].voltage_effnorm = 0xe9b;
			module_calibration.adapter_ef[9].voltage_effmax  = 0xf54;
			module_calibration.adapter_ef[8].voltage_effnorm = 0xe50;
			module_calibration.adapter_ef[8].voltage_effmax  = 0xf02;			
			module_calibration.adapter_ef[7].voltage_effnorm = 0xe0c;
			module_calibration.adapter_ef[7].voltage_effmax  = 0xeb9;			
			module_calibration.adapter_ef[6].voltage_effnorm = 0xde7;
			module_calibration.adapter_ef[6].voltage_effmax  = 0xe97;
			module_calibration.adapter_ef[5].voltage_effnorm = 0xdc5;
			module_calibration.adapter_ef[5].voltage_effmax  = 0xe76;
			module_calibration.adapter_ef[4].voltage_effnorm = 0xda4;
			module_calibration.adapter_ef[4].voltage_effmax  = 0xe55;
			module_calibration.adapter_ef[3].voltage_effnorm = 0xd7c;
			module_calibration.adapter_ef[3].voltage_effmax  = 0xe2d;			
			module_calibration.adapter_ef[2].voltage_effnorm = 0xd4e;
			module_calibration.adapter_ef[2].voltage_effmax  = 0xe06;			
			module_calibration.adapter_ef[1].voltage_effnorm = 0xd1c;
			module_calibration.adapter_ef[1].voltage_effmax  = 0xdd8;
			module_calibration.adapter_ef[0].voltage_effnorm = 0xce0;
			module_calibration.adapter_ef[0].voltage_effmax  = 0xdaa;
	}

	if(module_calibration.wifi_ef[0].voltage_effmax == 0) {
		dbg("for old version \n");
		mIsForOldVersion = true;
	}


	dbg("bat_config.ADC_TYP = 0x%x \n", bat_config.ADC_TYP);
    dbg("Bow: bat_config.wmt_operation = 0x%x \n",bat_config.wmt_operation);
    dbg("Bow: bat_config.update_time = 0x%x \n",bat_config.update_time);
    dbg("Bow: bat_config.charge_max = 0x%x \n",bat_config.charge_max);
    dbg("Bow: bat_config.charge_min = 0x%x \n",bat_config.charge_min);
    dbg("Bow: batt_discharge_max = 0x%x \n",bat_config.discharge_vp[10]);
    dbg("Bow: batt_discharge_v9 = 0x%x \n",bat_config.discharge_vp[9]);
    dbg("Bow: batt_discharge_v8 = 0x%x \n",bat_config.discharge_vp[8]);
    dbg("Bow: batt_discharge_v7 = 0x%x \n",bat_config.discharge_vp[7]);
    dbg("Bow: batt_discharge_v6 = 0x%x \n",bat_config.discharge_vp[6]);
    dbg("Bow: batt_discharge_v5 = 0x%x \n",bat_config.discharge_vp[5]);
    dbg("Bow: batt_discharge_v4 = 0x%x \n",bat_config.discharge_vp[4]);
    dbg("Bow: batt_discharge_v3 = 0x%x \n",bat_config.discharge_vp[3]);
    dbg("Bow: batt_discharge_v2 = 0x%x \n",bat_config.discharge_vp[2]);
    dbg("Bow: batt_discharge_v1 = 0x%x \n",bat_config.discharge_vp[1]);
    dbg("Bow: batt_discharge_min = 0x%x \n",bat_config.discharge_vp[0]);

	memset(&buf, 0x0, sizeof(buf));
	if (wmt_getsyspara("wmt.io.batt.chargetype", buf, &varlen) == 0) {
		sscanf(buf, "%d", &batt_charge_type);
	}

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
	return 30;
}

//retrun:1--low battery worning,0--no worning
static int wmt_read_batlow_DT(struct power_supply *bat_ps)
{

	int status;
	/*printk("Bow: read battlow...\n");*/
	/* WAKE_UP0 */
	status = (REG32_VAL(batlow.idaddr) & batlow.bitmap) ? 1 : 0;
	if(!(batlow.active & 0x01)){
        status = !status;
	}
	return !status;
}
//static unsigned long wmt_read_dcin_DT(struct power_supply *bat_ps)
//return:1--external adpater plug-in,0--external adapter plug-out
static int wmt_read_dcin_DT(struct power_supply *bat_ps)
{

	int status;
	
	/*printk("Bow: read battlow...\n");*/
       /* WAKE_UP1 */
	status = (REG32_VAL(dcin.idaddr) & dcin.bitmap) ? ADAPTER_PLUG_IN: ADAPTER_PLUG_OUT;
	if(!(dcin.active & 0x01)){
              status = !status;
	}

	// if usb connected to pc for charging, still record battery as discharging
	if(status == ADAPTER_PLUG_IN && batt_charge_type == CHARGE_BY_USB && android_usb_connect_state())
		return ADAPTER_PLUG_OUT;
	
	return status;
}

//return:1--external adpater plug-in,0--external adapter plug-out
int (*other_battery_dcin_state)(void)=0;

int wmt_read_dcin_state(void)
{
	int status;
	if(other_battery_dcin_state!=NULL)
		status = (*other_battery_dcin_state)();
	else{

		/*printk("Bow: read battlow...\n");*/
	    /* WAKE_UP1 */
		status = (REG32_VAL(dcin.idaddr) & dcin.bitmap) ? ADAPTER_PLUG_IN: ADAPTER_PLUG_OUT;
		if(!(dcin.active & 0x01)){
	        status = !status;
		}
	}
	return status;
}
EXPORT_SYMBOL(wmt_read_dcin_state);
EXPORT_SYMBOL(other_battery_dcin_state);

static void wmt_gpio8(int high)
{
	GPIO_CTRL_GP1_BYTE_VAL |=0x1;
	GPIO_OC_GP1_BYTE_VAL |=0x1;
	if(high)
		GPIO_OD_GP1_BYTE_VAL |=0x1;
	else
		GPIO_OD_GP1_BYTE_VAL &=0xfe;
	
}


//on = 1: true on power state led,  on = 0:  true off power state led
void pull_power_state_pin(int on)
{
	unsigned int val;

	if ((s_power_state_pin.ctl == 0) || (s_power_state_pin.oc == 0) || (s_power_state_pin.od == 0)) 
		return;

	val = 1 << s_power_state_pin.bitmap;
	
	if (s_power_state_pin.act == 0)
		on = ~on;

	if (on) {
		REG8_VAL(s_power_state_pin.od) |= val;
	} else {
		REG8_VAL(s_power_state_pin.od) &= ~val;
	}

	REG8_VAL(s_power_state_pin.oc) |= val;
	REG8_VAL(s_power_state_pin.ctl) |= val; 

}

static unsigned long wmt_read_status(struct power_supply *bat_ps)
{
	int status;
	int full = 0;

	dbg("Bow: read status...\n");
	full = (REG32_VAL(batstate.idaddr)& batstate.bitmap);
	dbg("full falg is  %d\n", full);

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

static unsigned long wmt_read_present(struct power_supply *bat_ps)
{
	return wmt_bat_is_batterypower();	
}


static unsigned short hx_batt_read(int fast)
{
	unsigned short adcval = 0;
	
//#ifdef CONFIG_BATTERY_VT1603
		adcval = vt1603_get_bat_info(fast);
		dbg("vt1603 bat ADC = %d(0x%x) \n ",adcval, adcval);
//#endif
	return adcval;
}

//去除最大最小值，然后返回一个平均值
static unsigned int get_best_value(unsigned int adcVal[], int len)
{
	unsigned int maxValue = adcVal[0], minValue = adcVal[0], sum = adcVal[0];
	int i = 1;
	for(; i < len; i++)
	{
		if(adcVal[i] > maxValue) {
			maxValue = adcVal[i];
		}
		if(adcVal[i] < minValue) {
			minValue = adcVal[i];
		}
		sum += adcVal[i];
	}
	sum = sum - maxValue - minValue;
	unsigned int returnValue = sum/(len - 2);
	return returnValue;
}

static int convertValue(int cap) 
{
	if(cap<0)
		cap = 0;
	if(cap>0)
		cap = (cap/5+1)*5;
	if(cap>100)
		cap = 100;
	return cap;
}
static unsigned int wmt_read_capacity(struct power_supply *bat_ps, int fast)
{
	//开机瞬间，系统默认背光为0，此时经过修正后 会把电压拉到一个很低很低的值
	//导致系统开机后因低电被关闭(此时电量其实比较高，但是因为默认的背光把它拉低了)
	//等整个系统启动结束后，背光才会第一次被设置成我们之前设置的值(大于40)
	//此时电量才不至于被拉得很低
	//所以如果检测到背光未被设值，我们就直接将背光默认为28。
	if(!mBrightnessHasSet)
		module_usage[BM_BRIGHTNESS] = 72;
	
	int capacity=0;
	unsigned int ADC_val = 0, ADC_repair_val = 0;

	dbg(" read capacity, ");
	static int index = 0;
	static int oldCapacity = -1;
	static unsigned int adc_val[5];
	static int judgeVoltageHopNextTime = 0;
	
	
	dbg("1 old Capacity = %d  \n",oldCapacity);
	adc_val[index%5] = hx_batt_read(fast);
	//init adc_val
	if(index == 0 && oldCapacity == -1/* || isFromAdapterPlugOut*/) {
		int i;
		for(i = 0; i < 5; i++) {
			adc_val[i] = adc_val[index%5];
		}
	}
	
	if(index++%5==0/* || isFromAdapterPlugOut*/) {
		ADC_val = get_best_value(adc_val, sizeof(adc_val) / sizeof(unsigned int));
	} else {
		if(oldCapacity == -1 || (oldCapacity < 15 && wmt_read_batlow_DT(NULL)))//第二个判断条件是为了让系统第一时间检测出低电然后关机
			ADC_val = get_best_value(adc_val, sizeof(adc_val) / sizeof(unsigned int));
		else {
			capacity = convertValue(oldCapacity);
			return capacity;
		}
	}
	
	static int oldLowestAdc = -1;
	if(oldLowestAdc == -1)
		oldLowestAdc = ADC_val;
	//新获取的电压值在非充电情况下，比前一个值大了0.005V
	//0.005V有可能是AD转换后的浮动误差，也可能是调节背光等引起的电压变化
	//若浮动范围在0.005V以内，则取上一个的值（比新取的值低0.005V以内）
	if( ADC_val - oldLowestAdc > 0 && ADC_val - oldLowestAdc < 5) {
		ADC_val = oldLowestAdc;
	} else {
		oldLowestAdc = ADC_val;//ADC_val比oldLowestAdc低，或者ADC_val比oldLowestAdc高0.005V以上
	}
	

	ADC_repair_val = adc_repair(ADC_val); 
	capacity = adc2cap(ADC_repair_val);
	dbg("2 new capacity = %d  \n",capacity);
	//io低 可能是误报的（wifi打开瞬间拉低所致）;
	//在30%以内开始减5
	if(wmt_read_batlow_DT(NULL)) {
		dbg("battery low");
		if(capacity <= 30) {
			if(capacity > 25)
				capacity -= (30 - capacity);
			else 
				capacity -=5;
		}
		
		if(capacity <5) {
			oldCapacity = 0;
			return 0;//小于5会自动关机
		}
	}
	dbg("3 repair capacity = %d  \n",capacity);
	dbg("ADC_val=%d(0x%x), ADC_repair_val=%d(0x%x).......\n", ADC_val, ADC_val, ADC_repair_val, ADC_repair_val);
	
	//dbg("pmc status:%x\n", PMWS_VAL);
	/*printk("ADC value = 0x%x  \n",ADC_val);*/

	/*
	if(bat_sesn.status== POWER_SUPPLY_STATUS_DISCHARGING){
	    ADC_val = ADC_repair_val;
		// for discharging
        if(ADC_val >= bat_config.discharge_vp[10]){
			capacity = 100;
		}
		else if (ADC_val <=bat_config.discharge_vp[0]){
			capacity = 0;
		}
		else{
			for (i=10;i>=1;i--)
			{
				if ((ADC_val<=bat_config.discharge_vp[i])&&(ADC_val>bat_config.discharge_vp[i-1]))
				{
					capacity = (ADC_val-bat_config.discharge_vp[i-1])*10/(bat_config.discharge_vp[i]-bat_config.discharge_vp[i-1])+(i-1)*10;
				}
			};
		}
		
		dbg("DISCHARGING Capacity = %d \n",capacity);
	}else{
		// for charging
		capacity = ((int)ADC_val - bat_config.charge_min) * 100 / (bat_config.charge_max - bat_config.charge_min);
		dbg("adc_val=%d,bat_config.charge_max=%d,bat_config.charge_min=%d, capacity=%d, %d\n",
				ADC_val,bat_config.charge_max, bat_config.charge_min, capacity, -1);
		if (capacity <= 0)
		{
			capacity = 10;
		} else if (capacity > 90)
		{
			capacity = 90;
		}
		dbg("CHARGING Capacity = %d \n",capacity);
	}
	*/

	static int oldLowestCapacity = -1;
	if(oldLowestCapacity == -1 /*|| isFromAdapterPlugOut*/)
		oldLowestCapacity = capacity;
	//对电量作向下取值的平滑处理
	if( capacity - oldLowestCapacity > 0 && capacity - oldLowestCapacity < 8) { //手指对屏幕进行操作，电量会下降4~6%
		dbg("ping hua\n");
		capacity = oldLowestCapacity;
	} else {
		oldLowestCapacity = capacity;//ADC_val比oldLowestAdc低，或者ADC_val比oldLowestAdc高0.005V以上
	}

	if(judgeVoltageHopNextTime > JUDGE_HOP_VOLTAGE_TIMES)
		judgeVoltageHopNextTime = JUDGE_HOP_VOLTAGE_TIMES;

	//跳变大于10%的点去掉，等5秒后重新返回新的电量值。
	if(/*!isFromAdapterPlugOut &&*/ judgeVoltageHopNextTime!=JUDGE_HOP_VOLTAGE_TIMES && oldCapacity != -1 && (capacity - oldCapacity > 3 || oldCapacity - capacity > 3)) {
		dbg("judgeVoltageHopNextTime = %d\n", judgeVoltageHopNextTime);
		++judgeVoltageHopNextTime;
		capacity = convertValue(oldCapacity);
		dbg("return~  Capacity = %d  \n",oldCapacity);
		return capacity;
	}
	/*isFromAdapterPlugOut = false;*/
	judgeVoltageHopNextTime = 0;
	
	oldCapacity = capacity;
	capacity = convertValue(capacity);
	dbg("4 return~  Capacity = %d  \n",oldCapacity);
	return capacity;
	//if(bat_capacity_bak>capacity){
	//	dbg(" bat_capacity_bak %d-->%d",bat_capacity_bak,capacity);
	//	bat_capacity_bak = capacity;
	//}	
	//return bat_capacity_bak;
}


static int wmt_bat_get_property(struct power_supply *bat_ps,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{

	/*printk("Bow: get property...\n");*/
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
	    /*printk("Bow:POWER_SUPPLY_PROP_STATUS...\n");*/
		val->intval = bat_sesn.status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
	    /*printk("Bow:POWER_SUPPLY_PROP_HEALTH...\n");*/
		val->intval = bat_sesn.health;
		break;
	//case POWER_SUPPLY_PROP_ONLINE:
	    /*printk("Bow:POWER_SUPPLY_PROP_ONLINE...\n");*/
		//val->intval = bat_online;
		//break;		
	case POWER_SUPPLY_PROP_TECHNOLOGY:
	    /*printk("Bow:POWER_SUPPLY_PROP_TECHNOLOGY...\n");*/
		val->intval = pdata->batt_tech;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	    /*printk("Bow:POWER_SUPPLY_PROP_VOLTAGE_NOW...\n");*/
		if (pdata->batt_aux >= 0)
			val->intval = wmt_read_voltage(bat_ps);
		else
			return -EINVAL;
		break;
	case POWER_SUPPLY_PROP_TEMP:
	    /*printk("Bow:POWER_SUPPLY_PROP_TEMP...\n");*/
		if (pdata->temp_aux >= 0)
			val->intval = wmt_read_temp(bat_ps);
		else
			return -EINVAL;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	    /*printk("Bow:POWER_SUPPLY_PROP_VOLTAGE_MAX...\n");*/
		if (pdata->max_voltage >= 0)
			val->intval = pdata->max_voltage;
		else
			return -EINVAL;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN:
	    /*printk("Bow:POWER_SUPPLY_PROP_VOLTAGE_MIN...\n");*/
		if (pdata->min_voltage >= 0)
			val->intval = pdata->min_voltage;
		else
			return -EINVAL;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
	    /*printk("Bow:POWER_SUPPLY_PROP_PRESENT...\n");*/
		val->intval = bat_sesn.is_batterypower;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
	    /*printk("Bow:POWER_SUPPLY_PROP_CAPACITY...\n");
	    {
	        int b = bat_sesn.capacity;
		   if(bat_sesn.status== POWER_SUPPLY_STATUS_DISCHARGING)
		   {
			   if(bat_capacity_prev>bat_sesn.capacity){
			    	dbg(" bat_capacity_bak %d-->%d",bat_capacity_prev,bat_sesn.capacity);
			    	bat_capacity_prev = bat_sesn.capacity;
			   }else{
			        b = bat_capacity_prev;
			   }			      
		   }else{
		       bat_capacity_prev = 100;
		   }
		   
		   val->intval = b; 

	    }*/
		val->intval = bat_sesn.capacity;
		break;		
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		    /*printk("Bow:POWER_SUPPLY_PROP_VOLTAGE_AVG...\n");*/
	   val->intval = 3;
	   break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		    /*printk("Bow:POWER_SUPPLY_PROP_CURRENT_AVG...\n");*/
	   val->intval = 3;
	   break;
		
	default:
			/*printk("%s %d\n",__FUNCTION__,__LINE__);*/

		return -EINVAL;
	}
			/*printk("%s %d psp %d\n",__FUNCTION__,val->intval,psp);*/
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
unsigned int wmt_bat_is_batterypower(void)
{
	return ((bat_sesn.dcin !=0 ) ? 0 : 1);	
}
EXPORT_SYMBOL_GPL(wmt_bat_is_batterypower);

// return 1--battery is charing, 0--external power
static unsigned int wmt_bat_is_charging(void)
{

	return ((bat_sesn.status==POWER_SUPPLY_STATUS_CHARGING)?1:0);	
}


static int wmt_read_charging_capacity(void) {

	int adc_rval =1, adc_val =1;
	int capacity = 1, cap_add =1;
	adc_val= hx_batt_read(1);
	
    adc_rval = adc_repair(adc_val);
	capacity = adc2cap(adc_val);
	cap_add = adc2cap(adc_rval);
	return cap_add;
}

void pull_charge_elec_pin(int on);


static void wmt_bat_update(struct power_supply *bat_ps)
{
	unsigned int current_percent = 0;

	mutex_lock(&work_lock);
    if(unlikely(debug_mode)){
		bat_sesn.dcin= wmt_read_dcin_DT(bat_ps);
		bat_sesn.status = wmt_read_status(bat_ps);
		bat_sesn.health = wmt_read_health(bat_ps);
		bat_sesn.is_batterypower = wmt_bat_is_batterypower();
		bat_sesn.low = wmt_read_batlow_DT(bat_ps);

        //use fixed capacity in debug mode
        if(wmt_bat_is_batterypower()){
            if(bat_sesn.low)
			   current_percent = 10;
			else if(bat_sesn.status == POWER_SUPPLY_STATUS_FULL)
			   current_percent = 100;
			else
			   current_percent = 90;
		}else{
		    if(bat_sesn.status == POWER_SUPPLY_STATUS_FULL)
                current_percent = 100;
			else
				current_percent = 90;
		}
		bat_sesn.capacity = current_percent;
	}
	else if(batt_operation){
		bat_sesn.dcin= wmt_read_dcin_DT(bat_ps);
		bat_sesn.status = wmt_read_status(bat_ps);
		bat_sesn.health = wmt_read_health(bat_ps);
		bat_sesn.is_batterypower = wmt_bat_is_batterypower();
		bat_sesn.low = wmt_read_batlow_DT(bat_ps);

		// if usb connected to pc for charging, still recard battery as discharging
		//if (batt_charge_type == CHARGE_BY_USB && android_usb_connect_state() && bat_sesn.dcin)
		//	bat_sesn.status = POWER_SUPPLY_STATUS_DISCHARGING;
	    

		if((bat_sesn.low) && (wmt_bat_is_batterypower())){
	         /* If battery low signal occur. */ 
			 /*Setting battery capacity is empty. */ 
			 /* System will force shut down. */
			bat_sesn.capacity = wmt_read_capacity(bat_ps, 0);
			if (bat_sesn.capacity < 15)
			{
				printk(KERN_ALERT "Battery Low.......... \n");
				//bat_sesn.capacity = 0;
			} else {
				printk(KERN_ALERT "Wrong battery low signal !!!\n");
			}
		}else{		
	        if (!bat_config.ADC_USED){
			dbg("Bow: Droid book. \n");
				/*bat_sesn.status = wmt_read_status(bat_ps);*/
				if(bat_sesn.dcin){  /* Charging */
					dbg("Bow: bat_sesn.status = %d\n",bat_sesn.status);
					if(bat_sesn.status== POWER_SUPPLY_STATUS_DISCHARGING){
						bat_sesn.capacity = 100;
					}else{
						bat_sesn.capacity = 50;
					}         
				}else{ /* Discharging */
					bat_sesn.capacity = 100;
				}
			}else{
			     dbg("Bow: MID. \n");
				 if(!wmt_bat_is_batterypower()){   /* Charging */
				 	if(!wmt_bat_is_charging()){
						dbg("Battery not charing.....\n");
						//bat_sesn.capacity = bat_capacity_bak;
						//bat_sesn.capacity is full and adapter is plug in.
						bat_sesn.capacity = 100;				
						current_percent = 100;		
					}else
					{
						//bat_capacity_bak =100;
						dbg("Battery charing.....\n");
						dbg("Bow: bat_sesn.status = %d\n",bat_sesn.status);
						if ((1 == judge_charge_full) &&
							(hx_batt_read(0)>= bat_config.charge_max))
						{
							bat_sesn.status = POWER_SUPPLY_STATUS_FULL;
						}
						if(bat_sesn.status== POWER_SUPPLY_STATUS_FULL){
							bat_sesn.capacity = 100;					
							//bat_temp_capacity = 50;
							//current_percent = wmt_read_capacity(bat_ps, 0);
						}else{						
							current_percent = wmt_read_capacity(bat_ps, 0);
							bat_susp_resu_flag = 0;
							sus_update_cnt = 0;
						} 
						//return 100 when charging;
						bat_sesn.capacity = current_percent;				
						//current_percent = 100;
					}
				}else{  /* Discharging */
					dbg("Dischargin...\n");
					//FIXME: workaround for discharging
					//bat_capacity_bak = 100;
					if (bat_sesn.dcin != bat_sesn_old.dcin)
					{
						// External adapter plug-out. The first switching.
						dbg("sleep.......bat_sesn.dcin = %d ",bat_sesn.dcin);
						/*isFromAdapterPlugOut = true;*/
						msleep(800);
						msleep(1500);
						current_percent = wmt_read_capacity(bat_ps,1);
						dbg("External adapter plug-out...\n");
					} else {
				        //current_percent = wmt_read_capacity(bat_ps, 0);
				        current_percent = wmt_read_capacity(bat_ps, 1);
			        }
					if (bat_susp_resu_flag != 0)
					{ 
						// If suspend then resume, show the capability before suspend
						if (sus_update_cnt < 2)
						{
							bat_sesn.capacity = bat_sesn_old.capacity;
							sus_update_cnt++;
						} else {
							bat_susp_resu_flag = 0;
							sus_update_cnt = 0;
							bat_sesn.capacity = current_percent;							
						}
						dbg("suspend_to_resume, bat_susp_resu_flag = %d, sus_update_cnt=%d\n",
						bat_susp_resu_flag, sus_update_cnt);
					} else {
						dbg("Normal discharging....\n");						
						bat_sesn.capacity = current_percent;
					}
					
				}				 

			}

	    }
	}else{
		dbg("Bow: RDK \n");
		bat_sesn.health = wmt_read_health(bat_ps);
		bat_sesn.is_batterypower = wmt_bat_is_batterypower();
		bat_sesn.status= POWER_SUPPLY_STATUS_FULL;
		bat_sesn.capacity = 100;
	}
	
	dbg("Bow: current_percent = %d \n",current_percent);
    //dbg("Bow: bat_temp_capacity = %d \n",bat_temp_capacity);
    dbg("Bow: capacity %d \n",bat_sesn.capacity);

//bat_sesn.capacity = 50; // only for debug
    
    if((bat_sesn.low) ||
       (bat_sesn.dcin != bat_sesn_old.dcin) ||
       (bat_sesn.health != bat_sesn_old.health) ||
       (bat_sesn.is_batterypower != bat_sesn_old.is_batterypower) ||
       (bat_sesn.status != bat_sesn_old.status) ||
       (bat_sesn.capacity != bat_sesn_old.capacity) ||
       (bat_sesn.capacity <= 0)){
		power_supply_changed(bat_ps);
    }
	
	if (batt_charge_type == CHARGE_BY_ADAPTER) {	
		if(bat_sesn.status == POWER_SUPPLY_STATUS_CHARGING)
		{
			int current_cap_add = wmt_read_charging_capacity();

	     	//int charging_high_current_value =  gpio_get_value(GP2,6);
	     	if(current_cap_add >= 95)// && charging_high_current_value == 0)
	     	{
	     		pull_charge_elec_pin(1);
	          	//gpio_set_value(GP2,6, HIGH);
	     	} 
			else if(current_cap_add < 95)// && charging_high_current_value == 1)
	     	{
	     		pull_charge_elec_pin(0);
	          	//gpio_set_value(GP2,6, LOW);
	     	}
		} 
		else if(bat_sesn.status == POWER_SUPPLY_STATUS_FULL)
		{
	     	//int charging_high_current_value =  gpio_get_value(GP2,6);
	     	//if(charging_high_current_value == 1)
	     	{
	     		pull_charge_elec_pin(0);
	          	//gpio_set_value(GP2,6, LOW);
				
	     	}
		}
	}
		
    dbg("Bow: ****************************************************************************  \n");
	dbg("Bow: bat_sesn.low = %d, bat_sesn_old.low = %d  \n",bat_sesn.low,bat_sesn_old.low);
	dbg("Bow: bat_sesn.dcin = %d, bat_sesn_old.dcin = %d  \n",bat_sesn.dcin,bat_sesn_old.dcin);
	//printk("Bow: bat_sesn.health = %d, bat_sesn_old.health = %d  \n",bat_sesn.health,bat_sesn_old.health);
	//printk("Bow: bat_sesn.is_batterypower = %d, bat_sesn_old.is_batterypower = %d  \n",bat_sesn.is_batterypower,bat_sesn_old.is_batterypower);
	dbg("Bow: bat_sesn.status = %d, bat_sesn_old.status = %d  \n",bat_sesn.status,bat_sesn_old.status);
	dbg("Bow: bat_sesn.capacity = %d, bat_sesn_old.capacity = %d  \n",bat_sesn.capacity,bat_sesn_old.capacity);
	dbg("Bow: ****************************************************************************  \n");
   
	bat_sesn_old.low = bat_sesn.low;
	bat_sesn_old.dcin = bat_sesn.dcin;
    bat_sesn_old.health = bat_sesn.health;
    bat_sesn_old.is_batterypower = bat_sesn.is_batterypower;
    bat_sesn_old.status = bat_sesn.status;
    bat_sesn_old.capacity = bat_sesn.capacity;
	mutex_unlock(&work_lock);
	/*printk("Bow: update ^^^\n");*/
}


static enum power_supply_property wmt_bat_props[] = {
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
	
};

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
    /*printk("wmt_bat_work\n");*/
	bat_sesn.dcin= wmt_read_dcin_DT(NULL);
	if(bat_sesn.dcin){
		if(wmt_read_status(NULL)==POWER_SUPPLY_STATUS_CHARGING){
			pull_power_state_pin(0);
		}else{
			pull_power_state_pin(1);
		}
	}else{
		pull_power_state_pin(1);
	}
	
	wmt_bat_update(&bat_ps);
}


static void polling_timer_func(unsigned long unused)
{
	/*printk("Bow: polling...\n");*/
	schedule_work(&bat_work);
	//schedule_work(&ac_work);
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
	mutex_lock(&work_lock);
	bat_susp_resu_flag = 1;
	sus_update_cnt = 0;
	mutex_unlock(&work_lock);
	return 0;
}

static int wmt_battery_resume(struct platform_device *dev)
{
	if(batt_operation){
		gpio_ini();
	}
	//schedule_work(&bat_work);
	//schedule_work(&ac_work);
	setup_timer(&polling_timer, polling_timer_func, 0);
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
	//REG32_VAL(0xd813001c) &= ~0x3fe;//simen:disable all general purpose wake-up but low power alarm 
	/*printk("Bow: Probe...\n");*/
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


	if (power_supply_register(&dev->dev, &bat_ps) != 0)
	{
		goto err;
	}
	if (power_supply_register(&dev->dev, &ac_ps) != 0) 
	{
		goto err;
	}

     /*************     AC     ************/
	
	printk("wmt_battery_probe done.\n");
	setup_timer(&polling_timer, polling_timer_func, 0);
	mod_timer(&polling_timer,
			  jiffies + msecs_to_jiffies(polling_interval));	
	

	return 0;
err:
	return ret;
}

static int __devexit wmt_battery_remove(struct platform_device *dev)
{

	flush_scheduled_work();
	del_timer_sync(&polling_timer);
	
	/*printk("Bow: remove...\n");*/
	power_supply_unregister(&bat_ps);
	power_supply_unregister(&ac_ps);

	return 0;
}

static struct wmt_battery_info wmt_battery_pdata = {
        .charge_gpio    = 5,
        .max_voltage    = 4000,
        .min_voltage    = 3000,
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
	
	if (sscanf(buffer, "debug_mode=%d", &debug_mode))
	{
		dbg("debug_mode=%d\n", debug_mode);
	} 
    else if(sscanf(buffer, "verbose_print=%d", &verbose_print)){
        dbg("verbose_print=%d\n", verbose_print);
	}
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
		// calculate the battery capacity curve
		mutex_lock(&work_lock);
		for (i=10;i>=0;i--)
		{
			bat_config.discharge_vp[i]=volt[i]*vp/vc;
		};		
		mutex_unlock(&work_lock);
		// update battery capicity u-boot param 
		sprintf(buff, "%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x",
					 bat_config.ADC_TYP,
			         bat_config.wmt_operation,
			         bat_config.update_time,
			         bat_config.charge_max,
			         bat_config.charge_min,
			         bat_config.discharge_vp[10],
			         bat_config.discharge_vp[9],
			         bat_config.discharge_vp[8],
			         bat_config.discharge_vp[7],
			         bat_config.discharge_vp[6],
			         bat_config.discharge_vp[5],
			         bat_config.discharge_vp[4],
			         bat_config.discharge_vp[3],
			         bat_config.discharge_vp[2],
			         bat_config.discharge_vp[1],
			         bat_config.discharge_vp[0]);	    
	    wmt_setsyspara(varname, buff);
	    klog("%s=%s\n", varname, buff);
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
	len = sprintf(page, "supply_power=%d\ndebug_mode=%d\nverbose_print=%d\n",
					power_source, debug_mode, verbose_print);
	dbg("supply_power=%d\n", power_source);
	return len;
}


static int mc_writeproc( struct file   *file,
                           const char    *buffer,
                           unsigned long count,
                           void          *data )
{
	BatteryModule bm;
	int usage;
	
	if (sscanf(buffer, "MODULE_CHANGE:%d-%d",&bm,&usage))
	{
		if(bm<BM_VOLUME||bm>BM_COUNT-1){
			printk("bm %d error,min %d max %d\n",bm,BM_VOLUME,BM_COUNT-1);
			return 0;
		}
		if(usage>100||usage<0){
			printk("usage %d error\n",usage);
			return 0;
		}
		if(!mBrightnessHasSet && bm == BM_BRIGHTNESS) {
			mBrightnessHasSet = true;
		}
		printk("mc_write:%d--%d\n",bm,usage);
		// calculate the battery capacity curve
		mutex_lock(&work_lock);
		module_usage[bm]=usage;
		mutex_unlock(&work_lock);

	}
	return count;
}

int getEffect(int adc, int usage, module_effect* effects, int size){
	int expectAdc, effect, i;
	for(i = 0; i < size; i++){
        effect =  (effects[i].voltage_effmax - effects[i].voltage_effnorm) * usage / 100;
		expectAdc = effects[i].voltage_effnorm + effect;
		if(adc <= effects[i].voltage_effmax)
		{
		    //printk("i is %d, adc is %d , effects_norm = %d, effects_max = %d, usage = %d, effect = %d\n",
				   //i, adc, effects[i].voltage_effnorm, effects[i].voltage_effmax, usage, effect);
			return effect;
		}
	}
	//printk("adc is %d , no match in effects, usage = %d, \n", adc, usage);
	if(size > 0)
	   return (effects[size-1].voltage_effmax - effects[size-1].voltage_effnorm) * usage / 100;
	return 0;
}

//for debug
int getEffectLevel(int adc, int usage, module_effect* effects, int size){
	int expectAdc, effect, i;
	for(i = 0; i < size; i++){
        effect =  (effects[i].voltage_effmax - effects[i].voltage_effnorm) * usage / 100;
		expectAdc = effects[i].voltage_effnorm + effect;
		if(adc <= effects[i].voltage_effmax)
		{
			return i;
		}
	}
	//printk("adc is %d , no match in effects\n", adc);
	if(size > 0)
	   return size - 1;
	return -1;
}


int adc_repair(int adc){
	int j;
	int adc_add;
	adc_add = adc;
	
    if(!wmt_bat_is_batterypower()){
		adc_add -= getEffect(adc, 100, module_calibration.adapter_ef, sizeof(module_calibration.adapter_ef)/sizeof(module_calibration.adapter_ef[0]));
		return adc_add;
	}    

	for(j=BM_VOLUME;j<BM_COUNT;j++){
		if(j==BM_BRIGHTNESS){
			//adc_add = adc_add+(module_usage[j]*(90+(3400-adc)*10/500)/100);
            int usage = 100 - module_usage[j];
			if(usage < 0) usage = 0; if(usage > 100) usage = 100;
			adc_add -= getEffect(adc, usage, module_calibration.brightness_ef, sizeof(module_calibration.brightness_ef)/sizeof(module_calibration.brightness_ef[0]));
		}else if(j==BM_WIFI) {
			//adc_add = adc_add+((module_usage[j]*40)/100);
			if(mIsForOldVersion) {
				int wifiUsage = module_usage[j];
				adc_add -= getEffect(adc, wifiUsage, module_calibration.wifi_ef, sizeof(module_calibration.wifi_ef)/sizeof(module_calibration.wifi_ef[0]));
			} else {
	            // int wifiUsage = 100 - module_usage[j];
				// adc_add -= getEffect(adc, wifiUsage, module_calibration.wifi_ef, sizeof(module_calibration.wifi_ef)/sizeof(module_calibration.wifi_ef[0]));
				
				// printk("wifi usage = %d, adc_add -= %d \n", wifiUsage, getEffect(adc, wifiUsage, module_calibration.wifi_ef, sizeof(module_calibration.wifi_ef)/sizeof(module_calibration.wifi_ef[0])));
			}
		} else if(j==BM_VIDEO) {
			adc_add += (module_usage[j]*45)/100;
		} else if(j==BM_USBDEVICE) {
			if(!mIsForOldVersion && module_usage[j] < 20 && module_usage[BM_WIFI] != 100) { //40表示usb wifi的电流，小于20表示usb wifi未打开,module_usage[BM_WIFI] != 100表示sdio wifi未打开
				adc_add -= getEffect(adc, 100, module_calibration.wifi_ef, sizeof(module_calibration.wifi_ef)/sizeof(module_calibration.wifi_ef[0]));
			}
			//printk("usb usage = %d \n", module_usage[j]);
			//if(module_usage[j] != 0) adc_add +=  (int)(200 * /*module_usage[j]*/6 * 2 / 100);
			//printk("BM_USBDEVICE = %d ,adc + %d\n", module_usage[j], (int)(200 * /*module_usage[j]*/6 * 2 / 100));
		} else if(j==BM_HDMI)
			adc_add = adc_add+((module_usage[j]>0?1:0)*15);			
		else
			adc_add = adc_add;
	}	

	//printk("adc is %d, adc_add is %d\n", adc, adc_add);
	
	return adc_add;
}

int adc2cap(int adc){
	int adc_val = adc;
	int capacity;
	int i;
    if(adc_val >= bat_config.discharge_vp[10]){
		capacity = 100;
	}
	else if (adc_val <=bat_config.discharge_vp[0]){
		capacity = 0;
	}
	else{
		for (i=10;i>=1;i--)
		{
			if ((adc_val<=bat_config.discharge_vp[i])&&(adc_val>bat_config.discharge_vp[i-1]))
			{
				capacity = (adc_val-bat_config.discharge_vp[i-1])*10/(bat_config.discharge_vp[i]-bat_config.discharge_vp[i-1])+(i-1)*10;
			}
		};
	}
	return capacity;
}

static int mc_readproc(char *page, char **start, off_t off,
			  int count, int *eof, void *data)
{
	int len = 0;
	int power_source; // 1--battery;2--external adapter.
	
	unsigned short adc_val,adc_rval = -1;
	int capacity,cap_add,wrong_repair_cap;//,i;
	BatteryModule j;
	//char * tmp;
	char usage[128];

    int bmUsage,wifiUsage;
	
	mutex_lock(&work_lock);
	adc_val= hx_batt_read(1);
	mutex_unlock(&work_lock);

	
    adc_rval = adc_repair(adc_val);
	capacity = adc2cap(adc_val);
	cap_add = adc2cap(adc_rval);
	//printk("adc %d  %d\n",adc_val,adc_repair(adc_val));

	//io低 可能是误报的（wifi打开瞬间拉低所致）;
	//在30%以内开始减5
	if(wmt_read_batlow_DT(NULL)) {
		wrong_repair_cap = capacity;
		if(wrong_repair_cap <= 30) {
			if(wrong_repair_cap > 25)
				wrong_repair_cap -= (30 - wrong_repair_cap);
			else 
				wrong_repair_cap -=5;
		}
		
		if(wrong_repair_cap <5) {
			wrong_repair_cap = 0;
		}
	}
	

	len = sprintf(page, "adc:%d,cap1:%d,cap2:%d",
					adc_val,capacity,cap_add);
					//在电量比较高的情况下，低电gpio报错了（因调节背光等所导致的瞬间低电），之后
					//的低电就不能用gpio来判断，只能靠电量，此时的wrong_repair_cap就是返回的电量
					//（在原基础上自减5%）
	for(j=BM_VOLUME;j<BM_COUNT;j++){
		sprintf(usage,",%d-%d",j,module_usage[j]);
		strcat(page,usage);
		len+=strlen(usage);
	}

    bat_sesn.dcin= wmt_read_dcin_DT(NULL);
    power_source = !wmt_bat_is_batterypower(); 

    bmUsage = 100 - module_usage[BM_BRIGHTNESS];
	if(bmUsage < 0) bmUsage = 0; if(bmUsage > 100) bmUsage = 100;

	if(mIsForOldVersion) {
		wifiUsage = module_usage[BM_WIFI];
	} else {
		wifiUsage = 100 - module_usage[BM_WIFI];
	}
			
    sprintf(usage, ",bl:%d,bf:%d,adapter:%d,adc_repair:%d, bright_repair_level:%d(%d%%),"
		           " adapter_repair_level:%d, wifi_repair_level:%d, wrCap3:%d \n",  
		           wmt_read_batlow_DT(NULL), 
                   //0!=(REG32_VAL(batstate.idaddr)& batstate.bitmap),
	               POWER_SUPPLY_STATUS_FULL == wmt_read_status(NULL),
		           power_source, adc_rval,
		           getEffectLevel(adc_val, bmUsage, module_calibration.brightness_ef, sizeof(module_calibration.brightness_ef)/sizeof(module_calibration.brightness_ef[0])),bmUsage,
		           getEffectLevel(adc_val, power_source*100, module_calibration.adapter_ef, sizeof(module_calibration.adapter_ef)/sizeof(module_calibration.adapter_ef[0])),
		           getEffectLevel(adc_val, wifiUsage, module_calibration.wifi_ef, sizeof(module_calibration.wifi_ef)/sizeof(module_calibration.wifi_ef[0])),
				   wrong_repair_cap);
	
	strcat(page, usage);
	len+=strlen(usage);
	//printk("page %s %d\n",page,len);
	
	//dbg("supply_power=%d\n", power_source);
	return len;
}

static bool wmt_batt_is_adjust(void)
{
	char varname[]="wmt.battery.adjust";
	char buf[4];
	int adjust = 0;
	int varlen = 3;

	if (wmt_getsyspara(varname, buf, &varlen) == 0)
	{
		sscanf(buf, "%d", &adjust);
		return (adjust==1)?true:false;
	} 
	return true;
}

static int __init wmt_battery_init(void)
{
	int ret;
	if(wmt_batt_initPara())
	{
		return -1;
	}	

	if (!wmt_batt_is_adjust())
	{
		return -1;
	}
	if(batt_operation){
		if (gpio_ini())
		{
			return -1;
		}
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

	memset(module_usage,0,sizeof(module_usage));
	return ret;
}

static void __exit wmt_battery_exit(void)
{
	if (bat_proc != NULL)
	{
		remove_proc_entry(BATTERY_PROC_NAME, NULL);
	}
	if (bat_module_change_proc != NULL)
	{
		remove_proc_entry(BATTERY_CALIBRATION_PRO_NAME, NULL);
	}
	platform_driver_unregister(&wmt_battery_driver);
	platform_device_unregister(&wmt_battery_device);
}


module_init(wmt_battery_init);
module_exit(wmt_battery_exit);

MODULE_AUTHOR("WonderMedia Technologies, Inc.");
MODULE_DESCRIPTION("WONDERMEDIA battery driver");
MODULE_LICENSE("GPL");

