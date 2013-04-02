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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h> // for msleep
#include <mach/hardware.h>//for REG32_VAL
#include <asm/atomic.h> // for atomic_inc atomis_dec added by rubbitxiao


//added begin by rubbit
extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);
extern int wmt_setsyspara(char *varname, char *varval);                      

//if you use API in the file ,you should #include <mach/wmt_misc.h>

void detect_wifi_module(void * pbool)
{
	int retval = -1;
	int varlen = 127;
	char buf[200] = {0};
	char *wifi;
	char *bt;
	char *gps;
	char *tmp;

	printk("detect_wifi_module\n");
  retval = wmt_getsyspara("wmt.wifi.bluetooth.gps", buf, &varlen);
  if(!retval)
	{
		//sscanf(buf, "%s:%s:%s",wifi,bt,gps);
		if(strlen(buf) == 0) {
			printk("uboot enviroment variant(wmt.wifi.bluetooth.gp) is not connect\n");
			*(bool*)pbool = false;
			return ;
		}
		tmp = buf;
		printk("buf:%s\n",buf);
		wifi = strsep(&tmp,":");
		bt = strsep(&tmp,":");
		gps = strsep(&tmp,":");
		
		printk("wifi:%s, bt:%s, gps:%s\n",wifi,bt,gps);

		if(!strncmp(wifi,"mtk_6620",8)) {
			*(bool*)pbool = true;
			printk("detect mtk_6620 modules:%d\n",*(bool*)pbool);
		} else {
			*(bool*)pbool = false;
			printk("wifi_module:%s---%d\n",wifi,*(bool*)pbool);
		}
		return ;
	}
	printk("have not set uboot enviroment variant:wmt.wifi.bluetooth.gps\n");
	return;
}

int bt_is_mtk6622 = -1;

int is_mtk6622(void)
{
	if(bt_is_mtk6622==-1){
		int retval = -1;
		int varlen = 127;
		char buf[200] = {0};
		retval = wmt_getsyspara("wmt.bt.param", buf, &varlen);
		bt_is_mtk6622 = 0;
		if(!retval)
		{
			if(!strcmp(buf,"mtk_6622"))
				bt_is_mtk6622 = 1;
		}
	}
	return bt_is_mtk6622;
}
                                                                             
                                                                            
struct wifi_gpio {                                                           
	int name;                                                                  
	int active;                                                                
	unsigned int bmp;                                                          
	unsigned int ctraddr;                                                      
	unsigned int ocaddr;                                                       
	unsigned int odaddr;                                                       
};                                                                           
                                                                             
static struct wifi_gpio l_wifi_gpio = {                                      
	.name = 6,                                                                 
	.active = 0,                                                               
	.bmp = (1<<0x6),                                                           
	.ctraddr = 0xD8110040,                                                     
	.ocaddr = 0xD8110080,                                                      
	.odaddr = 0xD81100C0,                                                      
};                                                                           
                                                                             
static void excute_gpio_op(int on)                                           
{                                                                            
                                                                             
	unsigned int vibtime = 0;                                                                                      
	REG32_VAL(l_wifi_gpio.ctraddr) |= l_wifi_gpio.bmp;                         
	REG32_VAL(l_wifi_gpio.ocaddr) |= l_wifi_gpio.bmp;                          
                                                                                                                                                           
	if(on){                                                                    
		if (l_wifi_gpio.active == 0)                                             
			REG32_VAL(l_wifi_gpio.odaddr) &= ~l_wifi_gpio.bmp; /* low active */    
		else                                                                     
			REG32_VAL(l_wifi_gpio.odaddr) |= l_wifi_gpio.bmp; /* high active */    
	}else{                                                                     
		if (l_wifi_gpio.active == 0)                                             
			REG32_VAL(l_wifi_gpio.odaddr) |= l_wifi_gpio.bmp;                      
		else                                                                     
			REG32_VAL(l_wifi_gpio.odaddr) &= ~l_wifi_gpio.bmp;                     
	}                                                                          
                                                                             
                                                                             
}                                                                            

atomic_t gVwifiPower = ATOMIC_INIT(0);
                                                                             
void wifi_power_ctrl(int open)                                               
{  
	int ret=0;                                                                          
	int varlen = 127;                                                          
    int retval;                                                              
    char buf[200]={0};                                                       
	printk("wifi_power_ctrl %d\n",open);                                       
    
  if(open)
  {
  	if( atomic_inc_return(&gVwifiPower) > 1 )
  	{
  			printk("gVwifiPower:%d\n",atomic_read(&gVwifiPower));
  			return;
  	}
  	
  }else
  {
  	if( atomic_dec_return(&gVwifiPower) > 0 )
  	{
  		printk("gVwifiPower:%d\n",atomic_read(&gVwifiPower));  		
  		return;
  	}
  }                                                                            
                                                                             
  retval = wmt_getsyspara("wmt.gpo.wifi", buf, &varlen);                   
  if(!retval)                                                              
	{                                                                          
		sscanf(buf, "%x:%x:%x:%x:%x:%x",                                         
					       &(l_wifi_gpio.name),                                        
					       &(l_wifi_gpio.active),                                      
					       &(l_wifi_gpio.bmp),                                         
					       &(l_wifi_gpio.ctraddr),                                     
					       &(l_wifi_gpio.ocaddr),                                      
					       &(l_wifi_gpio.odaddr));    
			                                        
		l_wifi_gpio.bmp = 1 << l_wifi_gpio.bmp;          
			                                                                                                                                               
		printk("wifi power up:%s\n", buf);   
		printk("name:%x,active:%x,bmp:%x,ctr:%x,oca:%x,oda:%x\n",
			l_wifi_gpio.name,l_wifi_gpio.active,l_wifi_gpio.bmp,
			l_wifi_gpio.ctraddr,l_wifi_gpio.ocaddr,l_wifi_gpio.odaddr);                                  
	}                                                                          
	//convert physical to virtual address                                                
	l_wifi_gpio.ctraddr|=(GPIO_BASE_ADDR&0xFE000000);                          
	l_wifi_gpio.ocaddr|=(GPIO_BASE_ADDR&0xFE000000);                           
	l_wifi_gpio.odaddr|=(GPIO_BASE_ADDR&0xFE000000);		                       
                                                                             
	                                                                           
  excute_gpio_op(open);                                                    
	if(open){                                                                  
		//wait 1 sec to hw init                                                  
		msleep(1000);                                                            
	}                                                                          
		                                                                         
} 

EXPORT_SYMBOL(is_mtk6622);
EXPORT_SYMBOL(wifi_power_ctrl);                                                                           
