/*
 * viatel_cbp_power.c
 *
 * VIA CBP driver for Linux
 *
 * Copyright (C) 2009 VIA TELECOM Corporation, Inc.
 * Author: VIA TELECOM Corporation, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <mach/viatel.h>
#include "core.h"

#define MDM_RST_LOCK_TIME   (120) 
#define MDM_RST_HOLD_DELAY  (100) //ms
#define MDM_PWR_HOLD_DELAY  (500) //ms

//#define VIA_AP_MODEM_DEBU
#ifdef VIA_AP_MODEM_DEBUG
#undef dbg
#define dbg(format, arg...) printk("[CBP_POWER]: " format "\n" , ## arg)
#else
#undef dbg
#define dbg(format, arg...) do {} while (0)
#endif

static struct wake_lock reset_lock;

void oem_power_off_modem(void)
{
    if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_PWR_EN)){
        oem_gpio_direction_output(GPIO_VIATEL_MDM_PWR_EN, 0);
    }

    if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_RST)){
        oem_gpio_direction_output(GPIO_VIATEL_MDM_RST, 1);
        /*just hold the reset pin if no power enable pin*/
        if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_PWR_EN)){
            mdelay(MDM_RST_HOLD_DELAY);
            oem_gpio_direction_output(GPIO_VIATEL_MDM_RST, 0);
        }
    }
}
EXPORT_SYMBOL(oem_power_off_modem);

ssize_t modem_power_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int power = 0;
    int ret = 0;

    if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_PWR_IND)){
        power = !!oem_gpio_get_value(GPIO_VIATEL_MDM_PWR_IND);
    }else if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_PWR_EN)){
        printk("No MDM_PWR_IND, just detect MDM_PWR_EN\n");
        power = !!oem_gpio_get_value(GPIO_VIATEL_MDM_PWR_EN);
    }else if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_RST)){
        printk("No MDM_PWR_IND, just detect MDM_PWR_RST\n");
        power = !!oem_gpio_get_value(GPIO_VIATEL_MDM_RST);
    }
    if(power){
        ret += sprintf(buf + ret, "on\n");
    }else{
        ret += sprintf(buf + ret, "off\n");
    }
    
    return ret;
}

ssize_t modem_power_store(
	struct kobject *kobj, struct kobj_attribute *attr,
	const char *buf, size_t n)
{
    int power;

    /* power the modem */
    if ( !strncmp(buf, "on", strlen("on"))) {
        power = 1;
    }else if(!strncmp(buf, "off", strlen("off"))){
        power = 0;
    }else{
        printk("%s: input %s is invalid.\n", __FUNCTION__, buf);
        return n;
    }

    printk("Warnning: Power %s modem\n", power ? "on" : "off");
    if(power){
#if 0
        if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_RST)){
            oem_gpio_direction_output(GPIO_VIATEL_MDM_RST, 1);
            mdelay(MDM_RST_HOLD_DELAY);
        }
        if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_PWR_EN)){
            oem_gpio_direction_output(GPIO_VIATEL_MDM_PWR_EN, 0);
            mdelay(MDM_PWR_HOLD_DELAY);
        }

        if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_PWR_EN)){
            oem_gpio_direction_output(GPIO_VIATEL_MDM_PWR_EN, 1);
            mdelay(MDM_PWR_HOLD_DELAY);
        }
        
        if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_RST)){
            oem_gpio_direction_output(GPIO_VIATEL_MDM_RST, 0);
            mdelay(MDM_RST_HOLD_DELAY);
        }
        
        if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_PWR_EN)){
            oem_gpio_direction_output(GPIO_VIATEL_MDM_PWR_EN, 0);
        }
#endif

        if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_PWR_EN)){
	    if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_RST)){
                oem_gpio_direction_output(GPIO_VIATEL_MDM_RST, 0);
                mdelay(MDM_RST_HOLD_DELAY);
            }
            oem_gpio_direction_output(GPIO_VIATEL_MDM_PWR_EN, 1);
            mdelay(MDM_PWR_HOLD_DELAY);
        }
    }else{
        oem_power_off_modem();
    }
    return n;
}

ssize_t modem_reset_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int reset = 0;
    int ret = 0;
    if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_RST_IND)){
        reset = !!oem_gpio_get_value(GPIO_VIATEL_MDM_RST_IND);
    }else if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_RST)){
        reset = !!oem_gpio_get_value(GPIO_VIATEL_MDM_RST);
    }
    
    if(reset){
        ret += sprintf(buf + ret, "reset\n");
    }else{
        ret += sprintf(buf + ret, "work\n");
    }

    return ret;
}

ssize_t modem_reset_store(
	struct kobject *kobj, struct kobj_attribute *attr,
	const char *buf, size_t n)
{
    /* reset the modem */
    if ( !strncmp(buf, "reset", strlen("reset"))) {
         wake_lock_timeout(&reset_lock, MDM_RST_LOCK_TIME * HZ);
         if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_RST)){
            oem_gpio_direction_output(GPIO_VIATEL_MDM_RST, 1);
            mdelay(MDM_RST_HOLD_DELAY);
            oem_gpio_direction_output(GPIO_VIATEL_MDM_RST, 0);
            mdelay(MDM_RST_HOLD_DELAY);
         }
         printk("Warnning: reset modem\n");
    }
    
    return n;
}

ssize_t modem_ets_select_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int level = 0;
    int ret = 0;
    
    if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_ETS_SEL)){
        level = !!oem_gpio_get_value(GPIO_VIATEL_MDM_ETS_SEL);
    }

    ret += sprintf(buf, "%d\n", level);
    return ret;
}

ssize_t modem_ets_select_store(
	struct kobject *kobj, struct kobj_attribute *attr,
	const char *buf, size_t n)
{
    if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_ETS_SEL)){
        if ( !strncmp(buf, "1", strlen("1"))) {
            oem_gpio_direction_output(GPIO_VIATEL_MDM_ETS_SEL, 1);
        }else if( !strncmp(buf, "0", strlen("0"))){
            oem_gpio_direction_output(GPIO_VIATEL_MDM_ETS_SEL, 0);
        }else{
            dbg("Unknow command.\n");
        }
    }

    return n;
}

ssize_t modem_boot_select_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int ret = 0;
    int level = 0;
    
    if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_BOOT_SEL)){
        level = !!oem_gpio_get_value(GPIO_VIATEL_MDM_BOOT_SEL);
    }

    ret += sprintf(buf, "%d\n", level);
    return ret;
}

ssize_t modem_boot_select_store(
	struct kobject *kobj, struct kobj_attribute *attr,
	const char *buf, size_t n)
{
    if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_BOOT_SEL)){
        if ( !strncmp(buf, "1", strlen("1"))) {
            oem_gpio_direction_output(GPIO_VIATEL_MDM_BOOT_SEL, 1);
        }else if( !strncmp(buf, "0", strlen("0"))){
            oem_gpio_direction_output(GPIO_VIATEL_MDM_BOOT_SEL, 0);
        }else{
            dbg("Unknow command.\n");
        }
    }

    return n;
}

#define modem_attr(_name) \
static struct kobj_attribute _name##_attr = { \
    .attr = {                        \
        .name = __stringify(_name),   \
        .mode = 0644,                \
    },                               \
    .show   = modem_##_name##_show,  \
    .store  = modem_##_name##_store, \
}

modem_attr(power);
modem_attr(reset);
modem_attr(ets_select);
modem_attr(boot_select);

static struct attribute *g_attr[] = {
    &power_attr.attr,
    &reset_attr.attr,
    &ets_select_attr.attr,
    &boot_select_attr.attr,
    NULL
};

static struct attribute_group g_attr_group = {
    .attrs = g_attr,
};


static void modem_shutdown(struct platform_device *dev)
{
    oem_power_off_modem();
}

static struct platform_driver power_driver = {
	.driver.name = "modem_power",
    .shutdown = modem_shutdown,
};

static struct platform_device power_device = {
	.name = "modem_power",
};

static struct kobject *modem_kobj;
static int __init viatel_power_init(void)
{
    int ret = 0;

    modem_kobj = viatel_kobject_add("modem");
    if(!modem_kobj){
        ret = -ENOMEM;
        goto err_create_kobj;
    }

    ret = platform_device_register(&power_device);
    if (ret) {
        printk("platform_device_register failed\n");
        goto err_platform_device_register;
    }
    ret = platform_driver_register(&power_driver);
    if (ret) {
        printk("platform_driver_register failed\n");
        goto err_platform_driver_register;
    }
    
    wake_lock_init(&reset_lock, WAKE_LOCK_SUSPEND, "cbp_rst");
    if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_PWR_IND)){
        oem_gpio_direction_input_for_irq(GPIO_VIATEL_MDM_PWR_IND);
    }
    if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_RST_IND)){
        oem_gpio_direction_input_for_irq(GPIO_VIATEL_MDM_RST_IND);
    }

    //oem_gpio_direction_output(GPIO_VIATEL_MDM_RST, 0);
    //oem_gpio_direction_output(GPIO_VIATEL_MDM_PWR_EN, 1);
    ret = sysfs_create_group(modem_kobj, &g_attr_group);

    if(ret){
        printk("sysfs_create_group failed\n");
        goto err_sysfs_create_group; 
    }

    return 0;
err_sysfs_create_group:
    platform_driver_unregister(&power_driver);
err_platform_driver_register:
	platform_device_unregister(&power_device);
err_platform_device_register:
err_create_kobj:
    return ret;
}

static void  __exit viatel_power_exit(void)
{
    wake_lock_destroy(&reset_lock);
}

late_initcall_sync(viatel_power_init);
module_exit(viatel_power_exit);
