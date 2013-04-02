#ifndef __LINUX_BQ27X00_BATTERY_H__
#define __LINUX_BQ27X00_BATTERY_H__

/**
 * struct bq27000_plaform_data - Platform data for bq27000 devices
 * @name: Name of the battery. If NULL the driver will fallback to "bq27000".
 * @read: HDQ read callback.
 *	This function should provide access to the HDQ bus the battery is
 *	connected to.
 *	The first parameter is a pointer to the battery device, the second the
 *	register to be read. The return value should either be the content of
 *	the passed register or an error value.
 */
/*struct bq27000_platform_data {
	const char *name;
	int (*read)(struct device *dev, unsigned int);
};
*/

extern int bq27x00_battery_i2c_init(void);
extern void bq27x00_battery_i2c_exit(void);
extern int bq27x00_update(void /*struct bq27x00_device_info *di*/);
extern  int bq27x00_is_charging(void);
extern int bq27x00_get_capacity(void);
extern int bq27x00_battery_status(void);
extern int bq27x00_battery_voltage(void);
extern int bq27x00_battery_current(void);
extern int bq27x00_battery_temperature(void);
extern int bq27x00_battery_time_to_empty(void);
extern int bq27x00_battery_time_to_full(void);
extern int bq27x00_battery_time_to_empty_avg(void);
extern int bq27x00_battery_charge_now(void);
extern int bq27x00_battery_charge_full(void);
extern int bq27x00_battery_charge_design(void);
extern int bq27x00_battery_cycle_count(void);
extern int bq27x00_battery_energy(void);
extern int bq27x00_get_flag(void);


#endif
