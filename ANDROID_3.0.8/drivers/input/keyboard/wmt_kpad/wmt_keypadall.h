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

/********************************************************************************************
 * Brief: 
 *		Define data type and export function	
 ********************************************************************************************/
#ifndef __WMT_KEYPAD_INTEGRATE20100319_H__
#define __WMT_KEYPAD_INTEGRATE20100319_H__

/*************** Include file ******************************************************************/
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/platform_device.h>


/****************************************** Export Macros *************************************************/
#define KEYPAD_IRQ_TOTAL	3

#define VOLUME_UP_KEY		KEY_VOLUMEUP
#define VOLUME_DOWN_KEY		KEY_VOLUMEDOWN

// for hold key
#define HOLD_INPUT_KEY 195 //118 
#define UNHOLD_INPPUT_KEY 196 //119

// for wifi button
#define ENABLE_WIFI_KEY    239
#define DISABLE_WIFI_KEY   238

#define WMT_KPAD_HOLD_TIME 1300     // 1.3s

#define KEYPADALL_DEVICE_NAME "keypad"

#define WMT_KPAD_DEVID_LEN 32
#define WMT_KPAD_TYPE_LEN  32
// the number of longkey
#define WMT_KPAD_LONGKEY_TOTAL 4 

#define WMT_KPAD_UP_TIME ((HZ/100)*10) //118
#define WMT_KPAD_LONG_PRESS_COUNT  58
#define WMT_KPAD_WIFI_TIME 2345

#define WMT_KPAD_INTERRUPT_TYPE_ZERO           0
#define WMT_KPAD_INTERRUPT_TYPE_ONE            1
#define WMT_KPAD_INTERRUPT_TYPE_FALLING        2
#define WMT_KPAD_INTERRUPT_TYPE_RISING         3
#define WMT_KPAD_INTERRUPT_TYPE_FALLING_RISING 4



//#define DEBUG_KPAD
#ifdef DEBUG_KPAD
#undef dbg
#define dbg(fmt, args...) printk(KERN_ALERT " %s: " fmt, __FUNCTION__ , ## args)

//#define dbg(fmt, args...) if (kpadall_isrundbg()) printk(KERN_ALERT "[%s]: " fmt, __FUNCTION__, ## args)
//#define dbg(fmt, args...) printk(KERN_ALERT "[%s]: " fmt, __FUNCTION__, ## args)


#else
#define dbg(fmt, args...) 
#endif

#undef errlog
#undef klog
#define errlog(fmt, args...) printk(KERN_ERR "[%s]: " fmt, __FUNCTION__, ## args)
#define klog(fmt, args...) printk(KERN_ALERT "[%s]: " fmt, __FUNCTION__, ## args)


#define L_SHIFT(X) (1 <<(X))


/****************************************** Export Types **************************************************/
struct gpio_device
{
	// share pin
	unsigned int  spreg; // share pin select pin
	unsigned int  spbit; 
	// int status
	unsigned int isbit; // int status bit
	unsigned int isreg; // int status reg
	unsigned int icreg; // int config reg
	unsigned int itybit; // int type bit
	unsigned int iedbit; // int enable/disable bit
	// gpio normal
	unsigned int gpbit; // gpio pin bit
	unsigned int gpivreg; // gpio input reg
	unsigned int gpovreg; // gpio output reg
	unsigned int gpedreg; // gpio enable/disable reg
	unsigned int gpiosreg; // gpio in/out select reg
	unsigned int gpepreg; // gpio enable pull/down reg
	unsigned int gppdreg; // gpio pull/down reg
};


struct longpress_data
{
	unsigned int normalkey;
	unsigned int longkey;
};

typedef int (*do_keyup)(void);


struct keypadall_device
{
	// attribute
	char type[WMT_KPAD_TYPE_LEN];
	int irq[KEYPAD_IRQ_TOTAL]; // irq number
	unsigned int irq_register_type[KEYPAD_IRQ_TOTAL];
	unsigned int* keycode; // report keycode
	int keycodenum; // the number of key
	struct longpress_data longpressdata[WMT_KPAD_LONGKEY_TOTAL]; // long pressed key array
	struct longpress_data currlngpdata;
	int longkeynum; // the number of the long pressed key
	unsigned int matrixcol; // the number of col
	unsigned int matrixrow; // the number of row
	//unsigned int shaketime; // delay time for shaking
	unsigned int longpressnum; // the number of int for keypad down
	unsigned int longpressmaxnum; // the max number of int for keypad down
	unsigned int longpresstime; // the total time to adust longpress
//	int checklngpflag; // the flat to begin checking long press.  
//	int pressed; // 1-- key down, 0 -- key up
//	unsigned int clrpressedtime; // delay time to clear the pressed falg
//	int longkeyreported; // 1-- long key has been reported, 0 -- the key hasn't reported.
//	int holdpressed; // 1--hold key is pressed, 0--hold key up
//	int holdchecktime; // the time to check the pressed hold key
	//struct platform_device * dev;
	spinlock_t slock; // mutex lock
	// opration	
	int (*probe)(void*); // init function
	int (*remove)(void*); // remove		
	irq_handler_t inthandler[KEYPAD_IRQ_TOTAL]; // int handler
	//void (*polltimer_timeout)(unsigned long); // handle poll timer timeout
	int (*enable_int)(void*); // enable interrupt
	int (*disable_int)(void*); // disable interrupt
	int (*suspend)(void*); 
	int (*resume)(void*);
	// for shaking
//	int (*init_shake)(void*); // some initial for shaking
//	int (*handle_shake)(void*); // shaking handler
//	int (*remove_shake)(void*); // free some resource
	// for long press key
	int longpresscnt;
//	int (*init_longpress)(void*); // initial for long press
//	int (*handle_longpress)(void*); // long press handler
//	int (*remove_longpress)(void*); // free some resource
//	void (*timeout_longpress)(unsigned long timeout);
	// for handling hold key
	unsigned int hold_timeout; // unit: ms
	int (*hold_is_up)(void);
//	int (*init_hold)(void*); 
//	int (*handle_hold)(void*);
//	int (*remove_hold)(void*);	
	// for up checking
//	struct timer_list uptimer;
	//int down; // 0 --no key is pressed; 1 -- some key is pressed and wait up.	
	unsigned int up_timeout; // unit: ms
	//int (*init_up)(void);	
	int (*handle_up)(void);
	int (*do_up)(void);
	//int (*remove_up)(void);
	int (*report_hold_sta)(void);
	unsigned int wifi_timeout; // unit: ms	
	int (*wifibtn_is_up)(void); // wifi button is up
	int (*hold_up_end)(void); // To do sth after hlod key is from down to up

};

/****************************************** Export Variables **********************************************/


/****************************************** Export Functions **********************************************/

/*extern int kpadall_enable_int(void* param);
extern int kpadall_disable_int(void* param);
*/
//extern int kpadall_report_key(unsigned int key, void* param);
extern int kpadall_init_longpress(void* param);
extern int kpadall_handle_longpress(void* param);
extern int kpadall_remove_longpress(void* param);
extern int kpadall_init_shake(void* param);
extern int kpadall_handle_shake(void* param);
extern int kpadall_remove_shake(void* param);
extern int kpadall_init_hold(void);
extern int kpadall_handle_hold(void);
extern int kpadall_remove_hold(void);
extern int kpadall_isrundbg(void);
extern void kpadall_longpresstimer_timeout(unsigned long timeout);

extern unsigned int kpadall_getholdinputkey(void);
extern unsigned int kpadall_getunholdinputkey(void);
extern int kpadall_report_key(unsigned int key, int up);
extern int kpadall_init_up(void);
extern int kpadall_remove_up(void);
extern int kpadall_handle_up(void);
extern void kpadall_set_current_dev(struct keypadall_device* kpad_dev);
extern struct keypadall_device* kpadall_get_current_dev(void);
extern int kpadall_islongkey(unsigned int key);
extern unsigned int kpadall_normalkey_2_longkey(unsigned int key);
extern int kpadall_if_hand_key(void);
extern void kpadall_set_wifibtndown(int down);
extern void kpadall_set_holddown(int down);
extern void kpadall_set_norkeydown(int down);
//return:1--support hold key,0--don't support it.
extern int kpadall_support_hold(void);
//return:1--support wifit button,0--don't suport it.
extern int kpadall_support_wifibtn(void);

extern int kpadall_handle_wifibtn(void);
extern int kpad_dbg_gpio(void);

#endif

