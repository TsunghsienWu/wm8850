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

#include <linux/i2c.h>
#include "sensor.h"


struct i2c_board_info sensor_i2c_board_info = {
	.type          = SENSOR_I2C_NAME,
	.flags         = 0x00,
	.addr          = SENSOR_I2C_ADDR,
	.platform_data = NULL,
	.archdata      = NULL,
	.irq           = -1,
};

static struct i2c_client *sensor_client=NULL;
int sensor_i2c_register_device (void)
{
	struct i2c_board_info *sensor_i2c_bi;
	struct i2c_adapter *adapter = NULL;
	//struct i2c_client *client   = NULL;
	sensor_i2c_bi = &sensor_i2c_board_info;
	adapter = i2c_get_adapter(2);/*in bus 2*/

	if (NULL == adapter) {
		printk("can not get i2c adapter, client address error\n");
		return -1;
	}
	sensor_client = i2c_new_device(adapter, sensor_i2c_bi);
	if (sensor_client == NULL) {
		printk("allocate i2c client failed\n");
		return -1;
	}
	i2c_put_adapter(adapter);
	return 0;
}

void sensor_i2c_unregister_device(void)
{
	if (sensor_client != NULL)
	{
		i2c_unregister_device(sensor_client);
	}
}
struct i2c_client* sensor_get_i2c_client(void)
{
	return sensor_client;
}
//EXPORT_SYMBOL_GPL(sensor_i2c_register_device);