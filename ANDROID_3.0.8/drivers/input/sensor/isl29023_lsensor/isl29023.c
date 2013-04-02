/*
 * isl29023.c - Intersil ISL29023  ALS & Proximity Driver
 *
 * By Intersil Corp
 * Michael DiGioia
 *
 * Based on isl29011.c
 *	by Mike DiGioia <mdigioia@intersil.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/hwmon.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/sysfs.h>
#include <linux/pm_runtime.h>
#include <linux/input-polldev.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/earlysuspend.h>

#include "sensor.h"

/* Insmod parameters */
//I2C_CLIENT_INSMOD_1(isl29023);

#define MODULE_NAME	"isl29023"

/* ICS932S401 registers */
#define ISL29023_REG_VENDOR_REV                 0x06
#define ISL29023_VENDOR                         1
#define ISL29023_VENDOR_MASK                    0x0F
#define ISL29023_REV                            4
#define ISL29023_REV_SHIFT                      4
#define ISL29023_REG_DEVICE                     0x44
#define ISL29023_DEVICE                        	44 


#define REG_CMD_1		0x00
#define REG_CMD_2		0x01
#define REG_DATA_LSB		0x02
#define REG_DATA_MSB		0x03
#define ISL_MOD_MASK		0xE0
#define ISL_MOD_POWERDOWN	0
#define ISL_MOD_ALS_ONCE	1
#define ISL_MOD_IR_ONCE		2
#define ISL_MOD_RESERVED	4
#define ISL_MOD_ALS_CONT	5
#define ISL_MOD_IR_CONT		6
#define IR_CURRENT_MASK		0xC0
#define IR_FREQ_MASK		0x30
#define SENSOR_RANGE_MASK	0x03
#define ISL_RES_MASK		0x0C

static int last_mod;

struct isl_device {
	struct input_polled_dev* input_poll_dev;
	struct i2c_client* client;
	int resolution;
	int range;
	int isdbg;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend earlysuspend;
#endif

};

static struct isl_device* l_sensorconfig = NULL;
static struct kobject *android_lsensor_kobj = NULL;
static int l_enable = 1; // 0:don't report data

static DEFINE_MUTEX(mutex);

static int isl_set_range(struct i2c_client *client, int range)
{
	int ret_val;

	ret_val = i2c_smbus_read_byte_data(client, REG_CMD_2);
	if (ret_val < 0)
		return -EINVAL;
	ret_val &= ~SENSOR_RANGE_MASK;	/*reset the bit */
	ret_val |= range;
	ret_val = i2c_smbus_write_byte_data(client, REG_CMD_2, ret_val);

 printk(KERN_INFO MODULE_NAME ": %s isl29023 set_range call, \n", __func__);
	if (ret_val < 0)
		return ret_val;
	return range;
}

static int isl_set_mod(struct i2c_client *client, int mod)
{
	int ret, val, freq;

	switch (mod) {
	case ISL_MOD_POWERDOWN:
	case ISL_MOD_RESERVED:
		goto setmod;
	case ISL_MOD_ALS_ONCE:
	case ISL_MOD_ALS_CONT:
		freq = 0;
		break;
	case ISL_MOD_IR_ONCE:
	case ISL_MOD_IR_CONT:
		freq = 1;
		break;
	default:
		return -EINVAL;
	}
	/* set IR frequency */
	val = i2c_smbus_read_byte_data(client, REG_CMD_2);
	if (val < 0)
		return -EINVAL;
	val &= ~IR_FREQ_MASK;
	if (freq)
		val |= IR_FREQ_MASK;
	ret = i2c_smbus_write_byte_data(client, REG_CMD_2, val);
	if (ret < 0)
		return -EINVAL;

setmod:
	/* set operation mod */
	val = i2c_smbus_read_byte_data(client, REG_CMD_1);
	if (val < 0)
		return -EINVAL;
	val &= ~ISL_MOD_MASK;
	val |= (mod << 5);
	ret = i2c_smbus_write_byte_data(client, REG_CMD_1, val);
	if (ret < 0)
		return -EINVAL;

	if (mod != ISL_MOD_POWERDOWN)
		last_mod = mod;

	return mod;
}

static int isl_get_res(struct i2c_client *client)
{
	int val;

 printk(KERN_INFO MODULE_NAME ": %s isl29023 get_res call, \n", __func__);
    val = i2c_smbus_read_word_data(client, 0)>>8 & 0xff;

	if (val < 0)
		return -EINVAL;

	val &= ISL_RES_MASK;
	val >>= 2;

	switch (val) {
	case 0:
		return 65536;
	case 1:
		return 4096;
	case 2:
		return 256;
	case 3:
		return 16;
	default:
		return -EINVAL;
	}
}

static int isl_get_mod(struct i2c_client *client)
{
	int val;

	val = i2c_smbus_read_byte_data(client, REG_CMD_1);
	if (val < 0)
		return -EINVAL;
	return val >> 5;
}

static int isl_get_range(struct i2c_client* client)
{
	switch (i2c_smbus_read_word_data(client, 0)>>8 & 0xff & 0x3) {
		case 0: return  1000;
		case 1: return  4000;
		case 2: return 16000;
		case 3: return 64000;
		default: return -EINVAL;
	}
}

static ssize_t
isl_sensing_range_show(struct device *dev,
		       struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	int val;

	mutex_lock(&mutex);
	pm_runtime_get_sync(dev);
	val = i2c_smbus_read_byte_data(client, REG_CMD_2);
	pm_runtime_put_sync(dev);
	mutex_unlock(&mutex);

	dev_dbg(dev, "%s: range: 0x%.2x\n", __func__, val);

	if (val < 0)
		return val;
	return sprintf(buf, "%d000\n", 1 << (2 * (val & 3)));
}

static ssize_t
ir_current_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	int val;

	mutex_lock(&mutex);
	pm_runtime_get_sync(dev);
	val = i2c_smbus_read_byte_data(client, REG_CMD_2);
	pm_runtime_put_sync(dev);
	mutex_unlock(&mutex);

	dev_dbg(dev, "%s: IR current: 0x%.2x\n", __func__, val);

	if (val < 0)
		return -EINVAL;
	val >>= 6;

	switch (val) {
	case 0:
		val = 100;
		break;
	case 1:
		val = 50;
		break;
	case 2:
		val = 25;
		break;
	case 3:
		val = 0;
		break;
	default:
		return -EINVAL;
	}

	if (val)
		val = sprintf(buf, "%d\n", val);
	else
		val = sprintf(buf, "%s\n", "12.5");
	return val;
}

static ssize_t
isl_sensing_mod_show(struct device *dev,
		     struct device_attribute *attr, char *buf)
{
//	struct i2c_client *client = to_i2c_client(dev);

	dev_dbg(dev, "%s: mod: 0x%.2x\n", __func__, last_mod);

	switch (last_mod) {
	case ISL_MOD_POWERDOWN:
		return sprintf(buf, "%s\n", "0-Power-down");
	case ISL_MOD_ALS_ONCE:
		return sprintf(buf, "%s\n", "1-ALS once");
	case ISL_MOD_IR_ONCE:
		return sprintf(buf, "%s\n", "2-IR once");
	case ISL_MOD_RESERVED:
		return sprintf(buf, "%s\n", "4-Reserved");
	case ISL_MOD_ALS_CONT:
		return sprintf(buf, "%s\n", "5-ALS continuous");
	case ISL_MOD_IR_CONT:
		return sprintf(buf, "%s\n", "6-IR continuous");
	default:
		return -EINVAL;
	}
}

static ssize_t
isl_output_data_show(struct device *dev,
		     struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	int ret_val, mod;
	unsigned long int output = 0;
	int temp;

	mutex_lock(&mutex);
	pm_runtime_get_sync(dev);

	temp = i2c_smbus_read_byte_data(client, REG_DATA_MSB);
	if (temp < 0)
		goto err_exit;
	ret_val = i2c_smbus_read_byte_data(client, REG_DATA_LSB);
	if (ret_val < 0)
		goto err_exit;
	ret_val |= temp << 8;

	dev_dbg(dev, "%s: Data: %04x\n", __func__, ret_val);

	mod = isl_get_mod(client);
	switch (last_mod) {
	case ISL_MOD_ALS_CONT:
	case ISL_MOD_ALS_ONCE:
	case ISL_MOD_IR_ONCE:
	case ISL_MOD_IR_CONT:
		output = ret_val;
		break;
	default:
		goto err_exit;
	}
	pm_runtime_put_sync(dev);
	mutex_unlock(&mutex);
	return sprintf(buf, "%ld\n", output);

err_exit:
	pm_runtime_put_sync(dev);
	mutex_unlock(&mutex);
	return -EINVAL;
}

static int isl_get_lux_data(struct i2c_client* client)
{
	struct isl_device* idev = i2c_get_clientdata(client);

	__u16 resH, resL;
	//int range;
	resL = i2c_smbus_read_word_data(client, 1)>>8;
	resH = i2c_smbus_read_word_data(client, 2)&0xff00;
	if ((resL < 0) || (resH < 0))
	{
		errlog("Error to read lux_data!\n");
		return -1;
	}
	return (resH | resL) * idev->range / idev->resolution;
}
static ssize_t
isl_lux_show(struct device *dev,
		     struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	__u16 resH, resL;// L1, L2, H1, H2, thresL, thresH;
	char cmd2;
	int res, data, tmp, range, resolution;

	mutex_lock(&mutex);
	pm_runtime_get_sync(dev);

//	cmd2 = i2c_smbus_read_word_data(client, 0)>>8 & 0xff;  // 01h
	resL = i2c_smbus_read_word_data(client, 1)>>8;         // 02h
	resH = i2c_smbus_read_word_data(client, 2)&0xff00;     // 03h
//	L1 = i2c_smbus_read_word_data(client, 3)>>8;           // 04h
//	L2 = i2c_smbus_read_word_data(client, 4)&0xff00;       // 05h
//	H1 = i2c_smbus_read_word_data(client, 5)>>8;           // 06h
//	H2 = i2c_smbus_read_word_data(client, 6)&0xff00;       // 07h

	res = resH | resL;
//	thresL = L2 | L1; 
//	thresH = H2 | H1; 

    cmd2 = i2c_smbus_read_word_data(client, 0)>>8 & 0xff;
	resolution = isl_get_res(client);  //resolution
	pm_runtime_put_sync(dev);
	mutex_unlock(&mutex);
	tmp = cmd2 & 0x3;            //range
	switch (tmp) {    
		case 0:
			range = 1000;
			break;
		case 1:
			range = 4000;
			break;
		case 2:
			range = 16000;
			break;
		case 3:
			range = 64000;
			break;
		default:
			return -EINVAL;
	}
	data = res * range / resolution;

//	printk("Data = 0x%04x [%d]\n", data, data);
//	printk("CMD2 = 0x%x\n", cmd2);
//	printk("Threshold Low  = 0x%04x\n", thresL);
//	printk("Threshold High = 0x%04x\n", thresH);

	return sprintf(buf, "%u\n", data);
}

static ssize_t
isl_cmd2_show(struct device *dev,
		     struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned long cmd2;
	mutex_lock(&mutex);
	pm_runtime_get_sync(dev);
    cmd2 = i2c_smbus_read_word_data(client, 0)>>8 & 0xff;
	pm_runtime_put_sync(dev);
	mutex_unlock(&mutex);
	dbg(" cmd2: 0x%02x\n", cmd2);
	switch (cmd2) {
	case 0:
		return sprintf(buf, "%s\n", "[cmd2 = 0] n = 16, range = 1000");
	case 1:
		return sprintf(buf, "%s\n", "[cmd2 = 1] n = 16, range = 4000");
	case 2:
		return sprintf(buf, "%s\n", "[cmd2 = 2] n = 16, range = 16000");
	case 3:
		return sprintf(buf, "%s\n", "[cmd2 = 3] n = 16, range = 64000");

	case 4:
		return sprintf(buf, "%s\n", "[cmd2 = 4] n = 12, range = 1000");
	case 5:
		return sprintf(buf, "%s\n", "[cmd2 = 5] n = 12, range = 4000");
	case 6:
		return sprintf(buf, "%s\n", "[cmd2 = 6] n = 12, range = 16000");
	case 7:
		return sprintf(buf, "%s\n", "[cmd2 = 7] n = 12, range = 64000");

	case 8:
		return sprintf(buf, "%s\n", "[cmd2 = 8] n = 8, range = 1000");
	case 9:
		return sprintf(buf, "%s\n", "[cmd2 = 9] n = 8, range = 4000");
	case 10:
		return sprintf(buf, "%s\n", "[cmd2 = 10] n = 8, range = 16000");
	case 11:
		return sprintf(buf, "%s\n", "[cmd2 = 11] n = 8, range = 64000");

	case 12:
		return sprintf(buf, "%s\n", "[cmd2 = 12] n = 4, range = 1000");
	case 13:
		return sprintf(buf, "%s\n", "[cmd2 = 13] n = 4, range = 4000");
	case 14:
		return sprintf(buf, "%s\n", "[cmd2 = 14] n = 4, range = 16000");
	case 15:
		return sprintf(buf, "%s\n", "[cmd2 = 15] n = 4, range = 64000");

	default:
		return -EINVAL;
	}
}


static ssize_t
isl_sensing_range_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned int ret_val;
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	switch (val) {
	case 1000:
		val = 0;
		break;
	case 4000:
		val = 1;
		break;
	case 16000:
		val = 2;
		break;
	case 64000:
		val = 3;
		break;
	default:
		return -EINVAL;
	}

	mutex_lock(&mutex);
	pm_runtime_get_sync(dev);
	ret_val = isl_set_range(client, val);
	pm_runtime_put_sync(dev);
	mutex_unlock(&mutex);

	if (ret_val < 0)
		return ret_val;
	return count;
}

static ssize_t
ir_current_store(struct device *dev,
		 struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned int ret_val;
	unsigned long val;

	if (!strncmp(buf, "12.5", 4))
		val = 3;
	else {
		if (strict_strtoul(buf, 10, &val))
			return -EINVAL;
		switch (val) {
		case 100:
			val = 0;
			break;
		case 50:
			val = 1;
			break;
		case 25:
			val = 2;
			break;
		default:
			return -EINVAL;
		}
	}

	mutex_lock(&mutex);
	pm_runtime_get_sync(dev);

	ret_val = i2c_smbus_read_byte_data(client, REG_CMD_2);
	if (ret_val < 0)
		goto err_exit;

	ret_val &= ~IR_CURRENT_MASK;	/*reset the bit before setting them */
	ret_val |= (val << 6);

	ret_val = i2c_smbus_write_byte_data(client, REG_CMD_2, ret_val);
	if (ret_val < 0)
		goto err_exit;

	pm_runtime_put_sync(dev);
	mutex_unlock(&mutex);

	return count;

err_exit:
	pm_runtime_put_sync(dev);
	mutex_unlock(&mutex);
	return -EINVAL;
}

static ssize_t
isl_sensing_mod_store(struct device *dev,
		      struct device_attribute *attr,
		      const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	int ret_val;
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	if (val > 7)
		return -EINVAL;

	mutex_lock(&mutex);
	pm_runtime_get_sync(dev);
	ret_val = isl_set_mod(client, val);
	pm_runtime_put_sync(dev);
	mutex_unlock(&mutex);

	if (ret_val < 0)
		return ret_val;
	return count;
}

static ssize_t
isl_cmd2_store(struct device *dev,
		      struct device_attribute *attr,
		      const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct isl_device* idev = i2c_get_clientdata(client);
	int res;
	unsigned long val;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	if (val > 15 || val < 0)
		return -EINVAL;

	mutex_lock(&mutex);
	pm_runtime_get_sync(dev);
	res = i2c_smbus_write_byte_data(client, REG_CMD_2, val);
	if (res < 0)
		printk("Warning - write failed\n");

	idev->resolution = isl_get_res(client);
	idev->range = isl_get_range(client);
	pm_runtime_put_sync(dev);
	mutex_unlock(&mutex);

	return count;
}

static DEVICE_ATTR(range, S_IRUGO | S_IWUSR,
		   isl_sensing_range_show, isl_sensing_range_store);
static DEVICE_ATTR(mod, S_IRUGO | S_IWUSR,
		   isl_sensing_mod_show, isl_sensing_mod_store);
static DEVICE_ATTR(ir_current, S_IRUGO | S_IWUSR,
		   ir_current_show, ir_current_store);
static DEVICE_ATTR(output, S_IRUGO, isl_output_data_show, NULL);
static DEVICE_ATTR(cmd2, S_IRUGO | S_IWUSR,
		   isl_cmd2_show, isl_cmd2_store);
static DEVICE_ATTR(lux, S_IRUGO,
		   isl_lux_show, NULL);

static struct attribute *mid_att_isl[] = {
	&dev_attr_range.attr,
	&dev_attr_mod.attr,
	&dev_attr_ir_current.attr,
	&dev_attr_output.attr,
	&dev_attr_lux.attr,
	&dev_attr_cmd2.attr,
	NULL
};

static struct attribute_group m_isl_gr = {
	.name = "isl29023",
	.attrs = mid_att_isl
};

static int isl_set_default_config(struct i2c_client *client)
{
	struct isl_device* idev = i2c_get_clientdata(client);

	int ret=0;
/* We don't know what it does ... */
//	ret = i2c_smbus_write_byte_data(client, REG_CMD_1, 0xE0);
//	ret = i2c_smbus_write_byte_data(client, REG_CMD_2, 0xC3);
/* Set default to ALS continuous */
	ret = i2c_smbus_write_byte_data(client, REG_CMD_1, 0xA0);
	if (ret < 0)
		return -EINVAL;
/* Range: 0~16000, number of clock cycles: 65536 */
	ret = i2c_smbus_write_byte_data(client, REG_CMD_2, 0x02); // vivienne
	if (ret < 0)
		return -EINVAL;
	idev->resolution = isl_get_res(client);
	idev->range = isl_get_range(client);;
    dbg("isl29023 set_default_config call, \n");

	return 0;
}

/* Return 0 if detection is successful, -ENODEV otherwise */
static int isl29023_detect(struct i2c_client *client/*, int kind,
                          struct i2c_board_info *info*/)
{
	struct i2c_adapter *adapter = client->adapter;
	int vendor, device, revision;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;
	//printk(KERN_INFO MODULE_NAME ": %s isl29023 detact call, kind:%d type:%s addr:%x \n", __func__, kind, info->type, info->addr);

	/* if (kind <= 0)*/ {
	        

	        vendor = i2c_smbus_read_word_data(client,
	                                          ISL29023_REG_VENDOR_REV);
	        dbg("read vendor=%d(0x%x)\n", vendor,vendor);
	        if (0x0FFFF == vendor)
	        {
	        	dbg("find isl29023!\n");
	        	return 0;
	        } else {
	        	return -ENODEV;
	        }
	        vendor >>= 8;
	        revision = vendor >> ISL29023_REV_SHIFT;
	        vendor &= ISL29023_VENDOR_MASK;
	        if (vendor != ISL29023_VENDOR)
	        {
	        	dbg("real_vendor=0x%x,tvendor=0x%x\n",vendor,ISL29023_VENDOR);
	        	return -ENODEV;
	        }

	        device = i2c_smbus_read_word_data(client,
	                                          ISL29023_REG_DEVICE);
	        dbg("device=%x\n", device);
	        device >>= 8;
	        if (device != ISL29023_DEVICE)
	        {
	        	dbg("real_device=0x%x, tdevice=0x%x\n", device, ISL29023_DEVICE);
	        	return -ENODEV;
	        }

	        if (revision != ISL29023_REV)
	        {
	              dbg("Unknown revision %d\n",
	                         revision);
	        }
	} /*else
	        dev_dbg(&adapter->dev, "detection forced\n");*/

	// strlcpy(info->type, "isl29023", I2C_NAME_SIZE);

        return 0;
}

int isl_input_open(struct input_dev* input)
{
	return 0;
}

void isl_input_close(struct input_dev* input)
{
}

static void isl_input_lux_poll(struct input_polled_dev *dev)
{
	struct isl_device* idev = dev->private;
	struct input_dev* input = idev->input_poll_dev->input;
	struct i2c_client* client = idev->client;

	if (l_enable != 0)
	{
		mutex_lock(&mutex);
		//pm_runtime_get_sync(dev);
		input_report_abs(input, ABS_MISC, isl_get_lux_data(client));
		input_sync(input);
		//pm_runtime_put_sync(dev);
		mutex_unlock(&mutex);
	}
}

static struct i2c_device_id isl29023_id[] = {
	{"isl29023", 0},
	{}
};

static int isl29023_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	dev_dbg(dev, "suspend\n");

	mutex_lock(&mutex);
	pm_runtime_get_sync(dev);
	isl_set_mod(client, ISL_MOD_POWERDOWN);
	pm_runtime_put_sync(dev);
	mutex_unlock(&mutex);

 printk(KERN_INFO MODULE_NAME ": %s isl29023 suspend call, \n", __func__);
	return 0;
}

static int isl29023_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	dev_dbg(dev, "resume\n");

	mutex_lock(&mutex);
	pm_runtime_get_sync(dev);
	isl_set_mod(client, last_mod);
	pm_runtime_put_sync(dev);
	mutex_unlock(&mutex);

 printk(KERN_INFO MODULE_NAME ": %s isl29023 resume call, \n", __func__);
	return 0;
}

MODULE_DEVICE_TABLE(i2c, isl29023_id);

/*static const struct dev_pm_ops isl29023_pm_ops = {
	.runtime_suspend = isl29023_runtime_suspend,
	.runtime_resume = isl29023_runtime_resume,
};

static struct i2c_board_info isl_info = {
	I2C_BOARD_INFO("isl29023", 0x44),
};

static struct i2c_driver isl29023_driver = {
	.driver = {
		   .name = "isl29023",
		   .pm = &isl29023_pm_ops,
		   },
	.probe = isl29023_probe,
	.remove = isl29023_remove,
	.id_table = isl29023_id,
	.detect         = isl29023_detect,
	//.address_data   = &addr_data,
};*/

static int mmad_open(struct inode *inode, struct file *file)
{
	dbg("Open the l-sensor node...\n");
	return 0;
}

static int mmad_release(struct inode *inode, struct file *file)
{
	dbg("Close the l-sensor node...\n");
	return 0;
}

static ssize_t mmad_read(struct file *fl, char __user *buf, size_t cnt, loff_t *lf)
{
	int lux_data = 0;
	
	mutex_lock(&mutex);
	lux_data = isl_get_lux_data(l_sensorconfig->client);
	mutex_unlock(&mutex);
	if (lux_data < 0)
	{
		errlog("Failed to read lux data!\n");
		return -1;
	}
	copy_to_user(buf, &lux_data, sizeof(lux_data));
	return sizeof(lux_data);
}

static long
mmad_ioctl(/*struct inode *inode,*/ struct file *file, unsigned int cmd,
	   unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	//char rwbuf[5];
	short enable;  //amsr = -1;
	unsigned int uval;

	dbg("l-sensor ioctr...\n");
	//memset(rwbuf, 0, sizeof(rwbuf));
	switch (cmd) {
		case LIGHT_IOCTL_SET_ENABLE:
			// enable/disable sensor
			if (copy_from_user(&enable, argp, sizeof(short)))
			{
				printk(KERN_ERR "Can't get enable flag!!!\n");
				return -EFAULT;
			}
			dbg("enable=%d\n",enable);
			if ((enable >=0) && (enable <=1))
			{
				dbg("driver: disable/enable(%d) gsensor.\n", enable);
				
				//l_sensorconfig.sensor_enable = enable;
				dbg("Should to implement d/e the light sensor!\n");
				l_enable = enable;
				
			} else {
				printk(KERN_ERR "Wrong enable argument in %s !!!\n", __FUNCTION__);
				return -EINVAL;
			}
			break;
		case WMT_IOCTL_SENSOR_GET_DRVID:
			uval = ISL29023_DRVID;
			if (copy_to_user((unsigned int*)arg, &uval, sizeof(unsigned int)))
			{
				return -EFAULT;
			}
			dbg("Isl29023_driver_id:%d\n",uval);
		default:
			break;
	}

	return 0;
}


static struct file_operations mmad_fops = {
	.owner = THIS_MODULE,
	.open = mmad_open,
	.release = mmad_release,
	.read = mmad_read,
	.unlocked_ioctl = mmad_ioctl,
};


static struct miscdevice mmad_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "lsensor_ctrl",
	.fops = &mmad_fops,
};

static void isl29023_early_suspend(struct early_suspend *h)
{
	struct i2c_client *client = l_sensorconfig->client;

	dbg("start\n");
	mutex_lock(&mutex);
	//pm_runtime_get_sync(dev);
	isl_set_mod(client, ISL_MOD_POWERDOWN);
	//pm_runtime_put_sync(dev);
	mutex_unlock(&mutex);
	dbg("exit\n");
}

static void isl29023_late_resume(struct early_suspend *h)
{
	struct i2c_client *client = l_sensorconfig->client;

	dbg("start\n");
	mutex_lock(&mutex);
	//pm_runtime_get_sync(dev);
	isl_set_mod(client, last_mod);
	isl_set_default_config(client);
	//pm_runtime_put_sync(dev);
	mutex_unlock(&mutex);	
	dbg("exit\n");
}


static int
isl29023_probe(struct i2c_client *client/*, const struct i2c_device_id *id*/)
{
	int res=0;

	struct isl_device* idev = kzalloc(sizeof(struct isl_device), GFP_KERNEL);
	if(!idev)
		return -ENOMEM;

	l_sensorconfig = idev;
	android_lsensor_kobj = kobject_create_and_add("android_lsensor", NULL);
	if (android_lsensor_kobj == NULL) {
		errlog(
		       "lsensor_sysfs_init:"\
		       "subsystem_register failed\n");
		res = -ENOMEM;
		goto err_kobjetc_create;
	}
	res = sysfs_create_group(android_lsensor_kobj, &m_isl_gr);
	if (res) {
		//pr_warn("isl29023: device create file failed!!\n");
 		printk(KERN_INFO MODULE_NAME ": %s isl29023 device create file failed\n", __func__);
		res = -EINVAL;
		goto err_sysfs_create;
	}

/* last mod is ALS continuous */
	last_mod = 5;
	//pm_runtime_enable(&client->dev);	
	idev->input_poll_dev = input_allocate_polled_device();
	if(!idev->input_poll_dev)
	{
		res = -ENOMEM;
		goto err_input_allocate_device;
	}
	idev->client = client;
	idev->input_poll_dev->private = idev;
	idev->input_poll_dev->poll = isl_input_lux_poll;
	idev->input_poll_dev->poll_interval = 100;//50;
	idev->input_poll_dev->input->open = isl_input_open;
	idev->input_poll_dev->input->close = isl_input_close;
	idev->input_poll_dev->input->name = "lsensor_lux";
	idev->input_poll_dev->input->id.bustype = BUS_I2C;
	idev->input_poll_dev->input->dev.parent = &client->dev;
	input_set_drvdata(idev->input_poll_dev->input, idev);
	input_set_capability(idev->input_poll_dev->input, EV_ABS, ABS_MISC);
	input_set_abs_params(idev->input_poll_dev->input, ABS_MISC, 0, 16000, 0, 0);
	i2c_set_clientdata(client, idev);
	/* set default config after set_clientdata */
	res = isl_set_default_config(client);	
	res = misc_register(&mmad_device);
	if (res) {
		errlog("mmad_device register failed\n");
		goto err_misc_register;
	}
	res = input_register_polled_device(idev->input_poll_dev);
	if(res < 0)
		goto err_input_register_device;
	// suspend/resume register
#ifdef CONFIG_HAS_EARLYSUSPEND
        idev->earlysuspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
        idev->earlysuspend.suspend = isl29023_early_suspend;
        idev->earlysuspend.resume = isl29023_late_resume;
        register_early_suspend(&(idev->earlysuspend));
#endif

	dbg("isl29023 probe succeed!\n");
	return 0;
err_input_register_device:
	misc_deregister(&mmad_device);
	input_free_polled_device(idev->input_poll_dev);
err_misc_register:
err_input_allocate_device:
	//__pm_runtime_disable(&client->dev, false);
err_sysfs_create:
	kobject_del(android_lsensor_kobj);
err_kobjetc_create:
	kfree(idev);
	return res;
}

static int isl29023_remove(struct i2c_client *client)
{
	struct isl_device* idev = i2c_get_clientdata(client);

	unregister_early_suspend(&(idev->earlysuspend));
	misc_deregister(&mmad_device);
	input_unregister_polled_device(idev->input_poll_dev);
	input_free_polled_device(idev->input_poll_dev);
	sysfs_remove_group(android_lsensor_kobj, &m_isl_gr);
	kobject_del(android_lsensor_kobj);
	//__pm_runtime_disable(&client->dev, false);
	kfree(idev);
 printk(KERN_INFO MODULE_NAME ": %s isl29023 remove call, \n", __func__);
	return 0;
}

static int __init sensor_isl29023_init(void)
{
	 printk(KERN_INFO MODULE_NAME ": %s isl29023 init call, \n", __func__);
	/*
	 * Force device to initialize: i2c-15 0x44
	 * If i2c_new_device is not called, even isl29023_detect will not run
	 * TODO: rework to automatically initialize the device
	 */
	//i2c_new_device(i2c_get_adapter(15), &isl_info);
	//return i2c_add_driver(&isl29023_driver);
	if (sensor_i2c_register_device())
	{
		errlog("Can't register light i2c device!\n");
		return -1;
	}
	if (isl29023_detect(sensor_get_i2c_client()))
	{
		errlog("Can't find light sensor isl29023!\n");
		goto detect_fail;
	}
	if(isl29023_probe(sensor_get_i2c_client()))
	{
		errlog("Erro for probe!\n");
		goto detect_fail;
	}
	return 0;

detect_fail:
	sensor_i2c_unregister_device();
	return -1;
}

static void __exit sensor_isl29023_exit(void)
{
 	printk(KERN_INFO MODULE_NAME ": %s isl29023 exit call \n", __func__);
 	isl29023_remove(sensor_get_i2c_client());
 	sensor_i2c_unregister_device();
	//i2c_del_driver(&isl29023_driver);
}

module_init(sensor_isl29023_init);
module_exit(sensor_isl29023_exit);

MODULE_AUTHOR("mdigioia");
MODULE_ALIAS("isl29023 ALS");
MODULE_DESCRIPTION("Intersil isl29023 ALS Driver");
MODULE_LICENSE("GPL v2");

