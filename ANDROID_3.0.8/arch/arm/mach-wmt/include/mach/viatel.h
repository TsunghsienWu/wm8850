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

#ifndef VIATEL_H
#define VIATEL_H

#include <linux/irq.h>
#include <linux/notifier.h>

#define GPIO_OEM_UNKNOW (-1)
#define GPIO_OEM_VALID(gpio) ((gpio == GPIO_OEM_UNKNOW) ? 0 : 1)

//////////////////////////////////////////////////////////////////////////////////
/*******************************  Gpio Config ***********************************/
//////////////////////////////////////////////////////////////////////////////////
#if defined(CONFIG_MACH_OMAP_KUNLUN)
/*Note: must redefine the GPIO pins according to your board, keep GPIO_VIATEL_UNKNOW if not used*/
#define GPIO_VIATEL_MDM_PWR_EN  126
#define GPIO_VIATEL_MDM_PWR_IND GPIO_OEM_UNKNOW
#define GPIO_VIATEL_MDM_RST     149
#define GPIO_VIATEL_MDM_RST_IND GPIO_OEM_UNKNOW
#define GPIO_VIATEL_MDM_BOOT_SEL GPIO_OEM_UNKNOW
#define GPIO_VIATEL_MDM_ETS_SEL GPIO_OEM_UNKNOW

#define GPIO_VIATEL_USB_AP_RDY 129
#define GPIO_VIATEL_USB_MDM_RDY 128
#define GPIO_VIATEL_USB_AP_WAKE_MDM 127
#define GPIO_VIATEL_USB_MDM_WAKE_AP 173
#endif

#if defined(CONFIG_SOC_JZ4770)
#include <asm/jzsoc.h>
/*Note: must redefine the GPIO pins according to your board, keep GPIO_VIATEL_UNKNOW if not used*/
#define GPIO_VIATEL_MDM_PWR_EN  GPIO_OEM_UNKNOW
#define GPIO_VIATEL_MDM_PWR_IND GPIO_OEM_UNKNOW
#define GPIO_VIATEL_MDM_RST     GPIO_EVDO_AP_BB_RST
#define GPIO_VIATEL_MDM_RST_IND GPIO_OEM_UNKNOW
#define GPIO_VIATEL_MDM_BOOT_SEL GPIO_OEM_UNKNOW
#define GPIO_VIATEL_MDM_ETS_SEL GPIO_EVDO_ETS_SEL_CON

#define GPIO_VIATEL_USB_AP_RDY GPIO_EVDO_AP_RDY_N
#define GPIO_VIATEL_USB_MDM_RDY GPIO_EVDO_CP_RDY_N
#define GPIO_VIATEL_USB_AP_WAKE_MDM GPIO_EVDO_AP_WAKE_BB_N
#define GPIO_VIATEL_USB_MDM_WAKE_AP GPIO_EVDO_CP_WAKE_AP_N
#define GPIO_VIATEL_UART_MDM_WAKE_AP GPIO_EVDO_AP_REV_GPIO5
#define GPIO_VIATEL_UART_AP_RDY GPIO_EVDO_AP_REV_GPIO3
#endif

#if defined(EVDO_DT_SUPPORT)
#include <mach/mt6575_gpio.h>
#include <mach/eint.h>

/*Note: must redefine the GPIO pins according to your board, keep GPIO_VIATEL_UNKNOW if not used*/
#define GPIO_VIATEL_MDM_PWR_EN  GPIO192
#define GPIO_VIATEL_MDM_PWR_IND GPIO_OEM_UNKNOW //GPIO193
#define GPIO_VIATEL_MDM_RST     GPIO188
#define GPIO_VIATEL_MDM_RST_IND GPIO189
#define GPIO_VIATEL_MDM_BOOT_SEL GPIO_OEM_UNKNOW //GPIO196
#define GPIO_VIATEL_MDM_ETS_SEL GPIO_OEM_UNKNOW

#define GPIO_VIATEL_USB_AP_RDY GPIO209
#define GPIO_VIATEL_USB_MDM_RDY GPIO202
#define GPIO_VIATEL_USB_AP_WAKE_MDM GPIO201
#define GPIO_VIATEL_USB_MDM_WAKE_AP GPIO207

#define GPIO_VIATEL_UART_MDM_WAKE_AP GPIO193
#define GPIO_VIATEL_UART_AP_RDY GPIO196

#endif


#define EVDO_WMT8850 1
#if defined(EVDO_WMT8850)
//#include <mach/hardware.h>
#include <mach/gpio-cs.h>

#define GPIO_VIATEL_UART_MDM_WAKE_AP 		-1		//not use
#define GPIO_VIATEL_UART_AP_RDY 			-2		//not use
#define GPIO_VIATEL_USB_AP_RDY 				1//(GP48*10+7)		//gpio 23  
#define GPIO_VIATEL_USB_MDM_RDY 			2//(GP1*10+0)//(GP26*10+6)		//gp26 bit6
#define GPIO_VIATEL_USB_AP_WAKE_MDM 		3//(GP48*10+5)		//gpio21
#define GPIO_VIATEL_USB_MDM_WAKE_AP 		4//(GP2*10+1)		//gp2 bit1

int oem_gpio_convert_init(void);
#endif
//////////////////////////////////////////////////////////////////////////////////
/****************************** Gpio Function   *********************************/
//////////////////////////////////////////////////////////////////////////////////
int oem_gpio_request(int gpio, const char *label);
void oem_gpio_free(int gpio);
/*config the gpio to be input for irq if the SOC need*/
int oem_gpio_direction_input_for_irq(int gpio);
int oem_gpio_direction_output(int gpio, int value);
int oem_gpio_get_value(int gpio);
int oem_gpio_to_irq(int gpio);
int oem_irq_to_gpio(int irq);
int oem_gpio_set_irq_type(int gpio, unsigned int type);
int oem_gpio_request_irq(int gpio, irq_handler_t handler, unsigned long flags,
	    const char *name, void *dev);
void oem_gpio_irq_mask(int gpio);
void oem_gpio_irq_unmask(int gpio);
int oem_gpio_irq_isenable(int gpio);
int oem_gpio_irq_isint(int gpio);
int oem_gpio_irq_clear(int gpio);


//////////////////////////////////////////////////////////////////////////////////
/*******************************  Sync Control **********************************/
//////////////////////////////////////////////////////////////////////////////////
/* notifer events */
#define ASC_NTF_TX_READY      0x0001 /*notifie CBP is ready to work*/
#define ASC_NTF_TX_UNREADY    0x0002 /*notifie CBP is not ready to work*/
#define ASC_NTF_RX_PREPARE    0x1001 /* notifier the device active to receive data from CBP*/
#define ASC_NTF_RX_POST       0x1002 /* notifer the device CBP stop tx data*/

#define ASC_NAME_LEN   (64)

/*used to register handle*/
struct asc_config{
    int gpio_ready;
    int gpio_wake;
    /*the level which indicate ap is ready*/
    int polar;
    char name[ASC_NAME_LEN];
};

/*Used to registe user accoring to handle*/
struct asc_infor {
    void *data;
    int (*notifier)(int, void *);
    char name[ASC_NAME_LEN];
};

#define USB_TX_HD_NAME "UsbTxHd"
#define USB_RX_HD_NAME "UsbRxHd"
#define USB_TX_USER_NAME "usb"
#define USB_RX_USER_NAME "usb"
#define RAWBULK_TX_USER_NAME "rawbulk"
#define RAWBULK_RX_USER_NAME "rawbulk"

#define UART_TX_HD_NAME "UartTxHd"
#define UART_RX_HD_NAME "UartRxHd"
#define UART_TX_USER_NAME "uart"
#define UART_RX_USER_NAME "uart"

#define ASC_PATH(hd, user) hd"."user

int asc_tx_register_handle(struct asc_config *cfg);
int asc_tx_add_user(const char *name, struct asc_infor *infor);
void asc_tx_del_user(const char *path);
int asc_tx_get_ready(const char *path, int sync);
int asc_tx_put_ready(const char *path, int sync);
int asc_tx_auto_ready(const char *name, int sync);
int asc_tx_check_ready(const char *name);
int asc_tx_set_auto_delay(const char *name, int delay);
int asc_tx_user_count(const char *path);
void asc_tx_reset(const char *name);

int asc_rx_register_handle(struct asc_config *cfg);
int asc_rx_add_user(const char *name, struct asc_infor *infor);
void asc_rx_del_user(const char *path);
int asc_rx_confirm_ready(const char *name, int ready);
void asc_rx_reset(const char *name);
int asc_rx_check_on_start(const char *name);

//////////////////////////////////////////////////////////////////////////////////
/*******************************  Power Control *********************************/
//////////////////////////////////////////////////////////////////////////////////
/* modem event notification values */
enum clock_event_nofitiers {
	MDM_EVT_NOTIFY_POWER_ON = 0,
	MDM_EVT_NOTIFY_POWER_OFF,
	MDM_EVT_NOTIFY_RESET_ON,
	MDM_EVT_NOTIFY_RESET_OFF,
	MDM_EVT_NOTIFY_NUM
};

void modem_notify_event(unsigned long event);
int modem_register_notifier(struct notifier_block *nb);
int modem_unregister_notifier(struct notifier_block *nb);
#endif
