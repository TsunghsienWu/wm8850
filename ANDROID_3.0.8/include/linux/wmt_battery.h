#ifndef _LINUX_WMT_BAT_H
#define _LINUX_WMT_BAT_H


struct wmt_battery_info {
	int	batt_aux;
	int	temp_aux;
	int	charge_gpio;
	int	min_voltage;
	int	max_voltage;
	int	batt_div;
	int	batt_mult;
	int	temp_div;
	int	temp_mult;
	int	batt_tech;
	char *batt_name;
};

#endif
