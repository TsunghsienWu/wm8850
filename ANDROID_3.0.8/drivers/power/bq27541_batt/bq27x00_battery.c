/*
 * BQ27x00 battery driver
 *
 * Copyright (C) 2008 Rodolfo Giometti <giometti@linux.it>
 * Copyright (C) 2008 Eurotech S.p.A. <info@eurotech.it>
 * Copyright (C) 2010-2011 Lars-Peter Clausen <lars@metafoo.de>
 * Copyright (C) 2011 Pali Rohár <pali.rohar@gmail.com>
 *
 * Based on a previous work by Copyright (C) 2008 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

/*
 * Datasheets:
 * http://focus.ti.com/docs/prod/folders/print/bq27000.html
 * http://focus.ti.com/docs/prod/folders/print/bq27500.html
 */

#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <asm/unaligned.h>

#include "bq27x00_battery.h"
#include "batt_client.h"

//#define DEBUG_WMT_BATT
#undef dbg
#ifdef DEBUG_WMT_BATT
#define dbg(fmt, args...) printk(KERN_ALERT "[%s]: " fmt, __FUNCTION__ , ## args)
#else
#define dbg(fmt, args...) 
#endif

#undef errlog
#undef klog
#define errlog(fmt, args...) printk(KERN_ERR "[%s]: " fmt, __FUNCTION__, ## args)
#define klog(fmt, args...) printk(KERN_ALERT "[%s]: " fmt, __FUNCTION__, ## args)


#define DRIVER_VERSION			"1.2.0"

#define BQ27x00_REG_TEMP		0x06
#define BQ27x00_REG_VOLT		0x08
#define BQ27x00_REG_AI			0x14
#define BQ27x00_REG_FLAGS		0x0A
#define BQ27x00_REG_TTE			0x16
#define BQ27x00_REG_TTF			0x18
#define BQ27x00_REG_TTECP		0x26
#define BQ27x00_REG_NAC			0x0C /* Nominal available capaciy */
#define BQ27x00_REG_LMD			0x12 /* Last measured discharge */
#define BQ27x00_REG_CYCT		0x2A /* Cycle count total */
#define BQ27x00_REG_AE			0x22 /* Available enery */

#define BQ27000_REG_RSOC		0x0B /* Relative State-of-Charge */
#define BQ27000_REG_ILMD		0x76 /* Initial last measured discharge */
#define BQ27000_FLAG_CHGS		BIT(7)
#define BQ27000_FLAG_FC			BIT(5)

#define BQ27500_REG_SOC			0x2C
#define BQ27500_REG_DCAP		0x3C /* Design capacity */
#define BQ27500_FLAG_DSC		BIT(0)
#define BQ27500_FLAG_FC			BIT(9)

#define BQ27000_RS			20 /* Resistor sense */

struct bq27x00_device_info;
struct bq27x00_access_methods {
	int (*read)(struct bq27x00_device_info *di, u8 reg, bool single);
};

enum bq27x00_chip { BQ27000, BQ27500 };

struct bq27x00_reg_cache {
	int temperature;
	int time_to_empty;
	int time_to_empty_avg;
	int time_to_full;
	int charge_full;
	int cycle_count;
	int capacity;
	int flags;

	int current_now;
};

struct bq27x00_device_info {
	struct device 		*dev;
	int			id;
	enum bq27x00_chip	chip;

	struct bq27x00_reg_cache cache;
	int charge_design_full;

	unsigned long last_update;
	struct delayed_work work;

	struct power_supply	bat;

	struct bq27x00_access_methods bus;

	struct mutex lock;
};

static enum power_supply_property bq27x00_battery_props[] = {
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
};

static unsigned int poll_interval = 360;

struct bq27x00_device_info *l_di = NULL;


/*
 * Common code for BQ27x00 devices
 */

static inline int bq27x00_read(struct bq27x00_device_info *di, u8 reg,
		bool single)
{
	return di->bus.read(di, reg, single);
}

/*
 * Return the battery Relative State-of-Charge
 * Or < 0 if something fails.
 */
int bq27x00_battery_read_rsoc(struct bq27x00_device_info *di)
{
	int rsoc;

	if (di->chip == BQ27500)
		rsoc = bq27x00_read(di, BQ27500_REG_SOC, false);
	else
		rsoc = bq27x00_read(di, BQ27000_REG_RSOC, true);

	if (rsoc < 0)
		dev_err(di->dev, "error reading relative State-of-Charge\n");

	return rsoc;
}

/*
 * Return a battery charge value in µAh
 * Or < 0 if something fails.
 */
int bq27x00_battery_read_charge(struct bq27x00_device_info *di, u8 reg)
{
	int charge;

	charge = bq27x00_read(di, reg, false);
	if (charge < 0) {
		dev_err(di->dev, "error reading nominal available capacity\n");
		return charge;
	}

	if (di->chip == BQ27500)
		charge *= 1000;
	else
		charge = charge * 3570 / BQ27000_RS;

	return charge;
}

/*
 * Return the battery Nominal available capaciy in µAh
 * Or < 0 if something fails.
 */
int bq27x00_battery_read_nac(struct bq27x00_device_info *di)
{
	return bq27x00_battery_read_charge(di, BQ27x00_REG_NAC);
}

int bq27x00_battery_charge_now(void)
{
	return bq27x00_battery_read_nac(l_di);
}

int bq27x00_battery_charge_full(void)
{
	return l_di->cache.charge_full;
}

int bq27x00_battery_charge_design(void)
{
	return l_di->charge_design_full;
}

int bq27x00_battery_cycle_count(void)
{
	return l_di->cache.cycle_count;
}

/*
 * Return the battery Last measured discharge in µAh
 * Or < 0 if something fails.
 */
int bq27x00_battery_read_lmd(struct bq27x00_device_info *di)
{
	return bq27x00_battery_read_charge(di, BQ27x00_REG_LMD);
}

/*
 * Return the battery Initial last measured discharge in µAh
 * Or < 0 if something fails.
 */
int bq27x00_battery_read_ilmd(struct bq27x00_device_info *di)
{
	int ilmd;

	if (di->chip == BQ27500)
		ilmd = bq27x00_read(di, BQ27500_REG_DCAP, false);
	else
		ilmd = bq27x00_read(di, BQ27000_REG_ILMD, true);

	if (ilmd < 0) {
		dev_err(di->dev, "error reading initial last measured discharge\n");
		return ilmd;
	}

	if (di->chip == BQ27500)
		ilmd *= 1000;
	else
		ilmd = ilmd * 256 * 3570 / BQ27000_RS;

	return ilmd;
}

/*
 * Return the battery Cycle count total
 * Or < 0 if something fails.
 */
int bq27x00_battery_read_cyct(struct bq27x00_device_info *di)
{
	int cyct;

	cyct = bq27x00_read(di, BQ27x00_REG_CYCT, false);
	if (cyct < 0)
		dev_err(di->dev, "error reading cycle count total\n");

	return cyct;
}

/*
 * Read a time register.
 * Return < 0 if something fails.
 */
int bq27x00_battery_read_time(struct bq27x00_device_info *di, u8 reg)
{
	int tval;

	tval = bq27x00_read(di, reg, false);
	if (tval < 0) {
		dev_err(di->dev, "error reading register %02x: %d\n", reg, tval);
		return tval;
	}

	if (tval == 65535)
		return -ENODATA;

	return tval * 60;
}

static void bq27x00_cache_cpy(struct bq27x00_reg_cache* des,struct bq27x00_reg_cache*src)
{
	des->capacity = src->capacity;
	des->charge_full = src->charge_full;
	des->current_now = src->current_now;
	des->cycle_count = src->cycle_count;
	des->flags = src->flags;
	des->temperature = src->temperature;
	des->time_to_empty = src->time_to_empty;
	des->time_to_empty_avg = src->time_to_empty_avg;
	des->time_to_full = src->time_to_full;
}

// return 1:charging,0:discharging
int bq27x00_is_charging(void)
{ 
	return ((l_di->cache.flags & 0x01)?0:1);
}
EXPORT_SYMBOL(bq27x00_is_charging);

int bq27x00_get_capacity(void)
{
	return (l_di->cache.capacity<=5)?0:l_di->cache.capacity;
}

int bq27x00_get_flag(void)
{
	return l_di->cache.flags;
}

// return 0:the state aren't changed,1:some states are changed.
int bq27x00_update(void /*struct bq27x00_device_info *di*/)
{
	struct bq27x00_device_info *di = l_di;
	struct bq27x00_reg_cache cache = {0, };
	int ret = 0;
	bool is_bq27500 = di->chip == BQ27500;
	
	cache.flags = bq27x00_read(di, BQ27x00_REG_FLAGS, is_bq27500);
	//dbg("cache.flags=%d\n", cache.flags);
	if (cache.flags >= 0) {
		cache.capacity = bq27x00_battery_read_rsoc(di);
		cache.temperature = bq27x00_read(di, BQ27x00_REG_TEMP, false);
		cache.time_to_empty = bq27x00_battery_read_time(di, BQ27x00_REG_TTE);
		cache.time_to_empty_avg = bq27x00_battery_read_time(di, BQ27x00_REG_TTECP);
		cache.time_to_full = bq27x00_battery_read_time(di, BQ27x00_REG_TTF);
		cache.charge_full = bq27x00_battery_read_lmd(di);
		cache.cycle_count = bq27x00_battery_read_cyct(di);

		//dbg("cache.capacity=%d\n",cache.capacity);
		//dbg("cache.charge_full=%d\n",cache.charge_full);
		//dbg("cache.cycle_count=%d\n", cache.cycle_count);
		//dbg("cache.temperature=%d\n", cache.temperature);
		if (!is_bq27500)
			cache.current_now = bq27x00_read(di, BQ27x00_REG_AI, false);

		/* We only have to read charge design full once */
		if (di->charge_design_full <= 0)
		{
			di->charge_design_full = bq27x00_battery_read_ilmd(di);
			dbg("charge_design_full=%d\n", di->charge_design_full);
		}
	}

	/* Ignore current_now which is a snapshot of the current battery state
	 * and is likely to be different even between two consecutive reads */
	if (memcmp(&di->cache, &cache, sizeof(cache) - sizeof(int)) != 0) {
		//di->cache = cache;
		bq27x00_cache_cpy(&di->cache, &cache);
		//power_supply_changed(&di->bat);
		ret = 1;
	}
	di->last_update = jiffies;
	return ret;
}

static void bq27x00_battery_poll(struct work_struct *work)
{
//	struct bq27x00_device_info *di =
	//	container_of(work, struct bq27x00_device_info, work.work);

//	bq27x00_update(l_di);

	if (poll_interval > 0) {
		/* The timer does not have to be accurate. */
		//set_timer_slack(&di->work.timer, poll_interval * HZ / 4);
		schedule_delayed_work(&l_di->work, msecs_to_jiffies(3600));
		dbg("For next update!\n");
	}
}


/*
 * Return the battery temperature in tenths of degree Celsius
 * Or < 0 if something fails.
 */
int bq27x00_battery_temperature(void)
//(struct bq27x00_device_info *di,	union power_supply_propval *val)
{
	int temp;
	
	struct bq27x00_device_info *di = l_di;
	if (di->cache.temperature < 0)
		return di->cache.temperature;

	if (di->chip == BQ27500)
		temp = di->cache.temperature - 2731;
	else
		temp = ((di->cache.temperature * 5) - 5463) / 2;

	return temp;
}

int bq27x00_battery_time_to_empty(void)
{
	return l_di->cache.time_to_empty;
}

int bq27x00_battery_time_to_empty_avg(void)
{
	return l_di->cache.time_to_empty_avg;
}

int bq27x00_battery_time_to_full(void)
{
	return l_di->cache.time_to_full;
}

/*
 * Return the battery average current in µA
 * Note that current can be negative signed as well
 * Or 0 if something fails.
 */
int bq27x00_battery_current(void)
//(struct bq27x00_device_info *di, union power_supply_propval *val)
{
	int curr;
	struct bq27x00_device_info *di = l_di;

	if (di->chip == BQ27500)
	    curr = bq27x00_read(di, BQ27x00_REG_AI, false);
	else
	    curr = di->cache.current_now;

	//if (curr < 0)
		//return curr;

	if (di->chip == BQ27500) {
		/* bq27500 returns signed value */
		curr = (int)((s16)curr) * 1000;
	} else {
		if (di->cache.flags & BQ27000_FLAG_CHGS) {
			dev_dbg(di->dev, "negative current!\n");
			curr = -curr;
		}

		curr = curr * 3570 / BQ27000_RS;
	}

	return curr;
}

int bq27x00_battery_status(void)
//(struct bq27x00_device_info *di,	union power_supply_propval *val)
{
	int status;
	struct bq27x00_device_info *di = l_di;

	if (di->chip == BQ27500) {
		if (di->cache.flags & BQ27500_FLAG_FC)
			status = POWER_SUPPLY_STATUS_FULL;
		else if (di->cache.flags & BQ27500_FLAG_DSC)
			status = POWER_SUPPLY_STATUS_DISCHARGING;
		else
			status = POWER_SUPPLY_STATUS_CHARGING;
	} else {
		if (di->cache.flags & BQ27000_FLAG_FC)
			status = POWER_SUPPLY_STATUS_FULL;
		else if (di->cache.flags & BQ27000_FLAG_CHGS)
			status = POWER_SUPPLY_STATUS_CHARGING;
		else if (power_supply_am_i_supplied(&di->bat))
			status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		else
			status = POWER_SUPPLY_STATUS_DISCHARGING;
	}

	//val->intval = status;

	return status;
}

/*
 * Return the battery Voltage in milivolts
 * Or < 0 if something fails.
 */
int bq27x00_battery_voltage(void)
//(struct bq27x00_device_info *di,union power_supply_propval *val)
{
	int volt;
	struct bq27x00_device_info *di = l_di;

	volt = bq27x00_read(di, BQ27x00_REG_VOLT, false);
	//if (volt < 0)
		//return volt;

	//val->intval = volt * 1000;

	return volt;
}

/*
 * Return the battery Available energy in µWh
 * Or < 0 if something fails.
 */
int bq27x00_battery_energy(void)
//(struct bq27x00_device_info *di,	union power_supply_propval *val)
{
	int ae;
	struct bq27x00_device_info *di = l_di;

	ae = bq27x00_read(di, BQ27x00_REG_AE, false);
	/*if (ae < 0) {
		dev_err(di->dev, "error reading available energy\n");
		return ae;
	}*/

	if (di->chip == BQ27500)
		ae *= 1000;
	else
		ae = ae * 29200 / BQ27000_RS;

	//val->intval = ae;

	return ae;
}


int bq27x00_simple_value(int value)
//(int value,	union power_supply_propval *val)
{
	//struct bq27x00_device_info *di = l_di;
	if (value < 0)
		return value;

	//val->intval = value;

	return value;
}

#define to_bq27x00_device_info(x) container_of((x), \
				struct bq27x00_device_info, bat);

int bq27x00_battery_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	//int ret = 0;
	//struct bq27x00_device_info *di = l_di;//to_bq27x00_device_info(psy);

	/*mutex_lock(&di->lock);
	if (time_is_before_jiffies(di->last_update + 5 * HZ)) {
		cancel_delayed_work_sync(&di->work);
		//bq27x00_battery_poll(&di->work.work);
	}
	mutex_unlock(&di->lock);
	*/
/*
	if (psp != POWER_SUPPLY_PROP_PRESENT && di->cache.flags < 0)
		return -ENODEV;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		ret = bq27x00_battery_status(di, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = bq27x00_battery_voltage(di, val);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = di->cache.flags < 0 ? 0 : 1;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = bq27x00_battery_current(di, val);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		ret = bq27x00_simple_value(di->cache.capacity, val);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = bq27x00_battery_temperature(di, val);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		ret = bq27x00_simple_value(di->cache.time_to_empty, val);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
		ret = bq27x00_simple_value(di->cache.time_to_empty_avg, val);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		ret = bq27x00_simple_value(di->cache.time_to_full, val);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		ret = bq27x00_simple_value(bq27x00_battery_read_nac(di), val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = bq27x00_simple_value(di->cache.charge_full, val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		ret = bq27x00_simple_value(di->charge_design_full, val);
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		ret = bq27x00_simple_value(di->cache.cycle_count, val);
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		ret = bq27x00_battery_energy(di, val);
		break;
	default:
		return -EINVAL;
	}
*/
	return 0;
}

static void bq27x00_external_power_changed(struct power_supply *psy)
{
	struct bq27x00_device_info *di = to_bq27x00_device_info(psy);

	cancel_delayed_work_sync(&di->work);
	schedule_delayed_work(&di->work, 0);
}

static int bq27x00_powersupply_init(struct bq27x00_device_info *di)
{
	int ret;

	di->bat.type = POWER_SUPPLY_TYPE_BATTERY;
	di->bat.properties = bq27x00_battery_props;
	di->bat.num_properties = ARRAY_SIZE(bq27x00_battery_props);
	di->bat.get_property = bq27x00_battery_get_property;
	//di->bat.external_power_changed = bq27x00_external_power_changed;

	INIT_DELAYED_WORK(&di->work, bq27x00_battery_poll);
	mutex_init(&di->lock);

	ret = power_supply_register(di->dev, &di->bat);
	if (ret) {
		dev_err(di->dev, "failed to register battery: %d\n", ret);
		return ret;
	}

	dev_info(di->dev, "support ver. %s enabled\n", DRIVER_VERSION);

	//bq27x00_update(di);
	

	return 0;
}

static void bq27x00_powersupply_unregister(struct bq27x00_device_info *di)
{
	cancel_delayed_work_sync(&di->work);

	power_supply_unregister(&di->bat);

	mutex_destroy(&di->lock);
}


/* If the system has several batteries we need a different name for each
 * of them...
 */
static DEFINE_IDR(battery_id);
static DEFINE_MUTEX(battery_mutex);

static int bq27x00_read_i2c(struct bq27x00_device_info *di, u8 reg, bool single)
{
	struct i2c_client *client = to_i2c_client(di->dev);
	struct i2c_msg msg[2];
	unsigned char data[2];
	int ret;

	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].buf = &reg;
	msg[0].len = sizeof(reg);
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = data;
	if (single)
		msg[1].len = 1;
	else
		msg[1].len = 2;

	ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0)
		return ret;

	if (!single)
		ret = get_unaligned_le16(data);
	else
		ret = data[0];

	return ret;
}

static int bq27x00_battery_probe(struct i2c_client *client/*,const struct i2c_device_id *id*/)
{
	char *name;
	struct bq27x00_device_info *di;
	int num;
	int retval = 0;
	bool is_bq27500;
	int i = 0;

	/* Get new ID for the new battery device */
	retval = idr_pre_get(&battery_id, GFP_KERNEL);
	if (retval == 0)
		return -ENOMEM;
	mutex_lock(&battery_mutex);
	retval = idr_get_new(&battery_id, client, &num);
	mutex_unlock(&battery_mutex);
	if (retval < 0)
		return retval;

	//name = kasprintf(GFP_KERNEL, "%s-%d", id->name*, num);
	name = kasprintf(GFP_KERNEL, "%s-%d", "bq27x00", num);
	if (!name) {
		errlog("failed to allocate device name\n");
		retval = -ENOMEM;
		goto batt_failed_1;
	}

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di) {
		errlog("failed to allocate device info data\n");
		retval = -ENOMEM;
		goto batt_failed_2;
	}

	di->id = num;
	di->dev = &client->dev;
	di->chip = BQ27500;//id->driver_data;
	di->bat.name = name;
	di->bus.read = &bq27x00_read_i2c;
	dbg("id=%d,chip=%d\n", di->id, di->chip);

	//if (bq27x00_powersupply_init(di))
		//goto batt_failed_3;

	i2c_set_clientdata(client, di);
	l_di = di;
	is_bq27500 = (di->chip == BQ27500);

	for (i = 0; i < 10; i++)
	{
		if (bq27x00_read(di, BQ27x00_REG_FLAGS, is_bq27500) < 0)
		{
			msleep(50);			
		} else {
			break;
		}
	};
	if (10 == i)
	{
		errlog("Can't find bq27500 battery!\n");
		retval = -2;
		goto batt_failed_3;
	}

	return 0;

batt_failed_3:
	kfree(di);
batt_failed_2:
	kfree(name);
batt_failed_1:
	mutex_lock(&battery_mutex);
	idr_remove(&battery_id, num);
	mutex_unlock(&battery_mutex);
	l_di = NULL;

	return retval;
}

static int bq27x00_battery_remove(struct i2c_client *client)
{
	struct bq27x00_device_info *di = i2c_get_clientdata(client);

	//bq27x00_powersupply_unregister(di);

	kfree(di->bat.name);

	mutex_lock(&battery_mutex);
	idr_remove(&battery_id, di->id);
	mutex_unlock(&battery_mutex);

	kfree(di);
	l_di = NULL;

	return 0;
}

int bq27x00_battery_i2c_init(void)
{
	int ret = 0;
	
	// register i2c device
	if (batt_i2c_register_device())
	{
		errlog("Can't register battery device!\n");
		return -1;
	}
	// probe the device
	ret = bq27x00_battery_probe(batt_get_i2c_client());
	//int ret = i2c_add_driver(&bq27x00_battery_driver);
	if (ret)
	{
		errlog("Unable to register BQ27x00 i2c driver\n");
		batt_i2c_unregister_device();
	}

	return ret;
}

void bq27x00_battery_i2c_exit(void)
{
	//i2c_del_driver(&bq27x00_battery_driver);
	bq27x00_battery_remove(batt_get_i2c_client());
	batt_i2c_unregister_device();
}





/*
 * Module stuff
 */

/*static int __init bq27x00_battery_init(void)
{
	int ret;

	ret = bq27x00_battery_i2c_init();

	return ret;
}
//module_init(bq27x00_battery_init);

 void __exit bq27x00_battery_exit(void)
{
	bq27x00_battery_i2c_exit();
}
*/
//module_exit(bq27x00_battery_exit);

//MODULE_AUTHOR("Rodolfo Giometti <giometti@linux.it>");
//MODULE_DESCRIPTION("BQ27x00 battery monitor driver");
//MODULE_LICENSE("GPL");
