#include <linux/i2c.h>
//#include "gsensor.h"

#ifdef CONFIG_WMT_SENSOR_MC3210
#define GSENSOR_I2C_NAME	"mc32x0"
#define GSENSOR_I2C_ADDR	0x4c
#elif defined CONFIG_WMT_SENSOR_DMARD08
#define GSENSOR_I2C_NAME	"dmard08"
#define GSENSOR_I2C_ADDR	0x1c
#elif defined CONFIG_WMT_SENSOR_MMA7660
#define GSENSOR_I2C_NAME	"mma7660"
#define GSENSOR_I2C_ADDR	0x4c
#elif defined CONFIG_WMT_SENSOR_KXTE9
#define GSENSOR_I2C_NAME	"kxte9"
#define GSENSOR_I2C_ADDR	0x0f
#endif

struct i2c_board_info gsensor_i2c_board_info = {
	.type          = GSENSOR_I2C_NAME,
	.flags         = 0x00,
	.addr          = GSENSOR_I2C_ADDR,
	.platform_data = NULL,
	.archdata      = NULL,
	.irq           = -1,
};

int gsensor_i2c_register_device (void)
{
	struct i2c_board_info *gsensor_i2c_bi;
	struct i2c_adapter *adapter = NULL;
	struct i2c_client *client   = NULL;
	gsensor_i2c_bi = &gsensor_i2c_board_info;
	adapter = i2c_get_adapter(0);/*in bus 0*/

	if (NULL == adapter) {
		printk("can not get i2c adapter, client address error\n");
		return -1;
	}
	client = i2c_new_device(adapter, gsensor_i2c_bi);
	if (client == NULL) {
		printk("allocate i2c client failed\n");
		return -1;
	}
	i2c_put_adapter(adapter);
	return 0;
}

EXPORT_SYMBOL_GPL(gsensor_i2c_register_device);