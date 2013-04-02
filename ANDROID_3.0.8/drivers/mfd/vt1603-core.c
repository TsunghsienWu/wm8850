/*++ 
 * WonderMedia core driver for VT1603/VT1609
 *
 * Copyright c 2010  WonderMedia  Technologies, Inc.
 *
 * This program is free software: you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 2 of the License, or 
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details. 
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * WonderMedia Technologies, Inc.
 * 4F, 533, Chung-Cheng Road, Hsin-Tien, Taipei 231, R.O.C
--*/

#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/mfd/core.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>

#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/mfd/vt1603/core.h>


/*----------------------------------------------------------------------*/

/* Export Functions */

int vt1603_reg_write(struct vt1603 *vt1603, u8 reg, u8 val)
{
	return vt1603->reg_write(vt1603, reg, val);
}

int vt1603_reg_read(struct vt1603 *vt1603, u8 reg, u8 *val)
{
	return vt1603->reg_read(vt1603, reg, val);
}

void vt1603_regs_dump(struct vt1603 *vt1603)
{
	int reg = 0, ret;
	u8 val;
	printk(KERN_INFO "\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
	for(reg = 0; reg < 0xe8; reg++){
		ret = vt1603->reg_read(vt1603, reg, &val);
		if (ret) {
			printk(KERN_INFO "\nvt1603_hw_read[r:%d] error\n", reg);
			goto out;
		}
		if(reg%4 == 0) printk("\n");
		printk("reg[%02x]=%02x  ", reg, val);
	}
out:
	printk(KERN_INFO "\n<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n\n");
}

/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/

static int vt1603_mfd_init(struct vt1603 *vt1603)
{
	int ret;
	struct mfd_cell cell[] = {
		{
			.name = "vt1603-codec",
			.platform_data = vt1603,
			.pdata_size = sizeof(*vt1603),
		},
		{
			.name = "vt1603-touch",
			.platform_data = vt1603,
			.pdata_size = sizeof(*vt1603),
		},
		{
			.name = "vt1603-batt",
			.platform_data = vt1603,
			.pdata_size = sizeof(*vt1603),
		},
	};

	ret = mfd_add_devices(vt1603->dev, -1,
			      cell, ARRAY_SIZE(cell),
			      NULL, 0);
	if (ret != 0) {
		pr_err("vt1603_mfd_init errcode: %d\n", ret);
		goto err;
	}
	return 0;

err:
	mfd_remove_devices(vt1603->dev);
	kfree(vt1603);
	return ret;
}

static void vt1603_mfd_release(struct vt1603 *vt1603)
{
	mfd_remove_devices(vt1603->dev);
	kfree(vt1603);
}

/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/

#if defined(CONFIG_I2C) && defined(CONFIG_VT1603_IOCTRL_I2C)
static int vt1603_i2c_write(struct vt1603 *vt1603, u8 reg, u8 val)
{
	int ret;
	struct i2c_client *i2c = vt1603->control_data;
	struct i2c_msg xfer[1];
	u8 buf[2];

	/*
	 * [MSG1]: fill the register address data
	 * fill the data Tx buffer
	 */
	xfer[0].addr = i2c->addr;
	xfer[0].len = 2;
	xfer[0].flags = 0 ;
	xfer[0].flags &= ~(I2C_M_RD);
	xfer[0].buf = buf;
	buf[0] = reg; 
	buf[1] = val;
	ret = i2c_transfer(i2c->adapter, xfer, ARRAY_SIZE(xfer));

	/* i2c_transfer returns number of messages transferred */
	if (ret != ARRAY_SIZE(xfer)) {
		pr_err("vt1603_i2c_read[r:%d, v:%d] errcode[%d]\n", reg, val, ret);
		if (ret < 0)
			return ret;
		else
			return -EIO;
	} else
		return 0;
}

static int vt1603_i2c_read(struct vt1603 *vt1603, u8 reg, u8 *val)
{
	int ret;
	struct i2c_client *i2c = vt1603->control_data;
	struct i2c_msg xfer[2];

	/* [MSG1] fill the register address data */
	xfer[0].addr = i2c->addr;
	xfer[0].len = 1;
	xfer[0].flags = 2;	/* Read the register val */
	xfer[0].buf = &reg;
	/* [MSG2] fill the data rx buffer */
	xfer[1].addr = i2c->addr;
	xfer[1].flags = I2C_M_RD;	/* Read the register val */
	xfer[1].len = 1;	/* only n bytes */
	xfer[1].buf = val;
	ret = i2c_transfer(i2c->adapter, xfer, ARRAY_SIZE(xfer));

	/* i2c_transfer returns number of messages transferred */
	if (ret != ARRAY_SIZE(xfer)) {
		pr_err("vt1603_i2c_read[r:%d] errcode[%d]\n", reg, ret);
		if (ret < 0)
			return ret;
		else
			return -EIO;
	} else {
		return 0;
	}
}

static int __devinit vt1603_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct vt1603 *vt1603;

	if (i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C) == 0) {
		pr_err("vt1603_i2c_probe: can't talk I2C?\n");
		return -EIO;
	}

	vt1603 = kzalloc(sizeof(struct vt1603), GFP_KERNEL);
	if (vt1603 == NULL)
		return -ENOMEM;

	vt1603->dev = &i2c->dev;
	vt1603->control_data = i2c;
	vt1603->reg_read = vt1603_i2c_read;
	vt1603->reg_write = vt1603_i2c_write;
	vt1603->type = id->driver_data;
	i2c_set_clientdata(i2c, vt1603);
	dev_set_drvdata(vt1603->dev, vt1603);

	return vt1603_mfd_init(vt1603);
}

static int __devexit vt1603_i2c_remove(struct i2c_client *i2c)
{
	struct vt1603 *vt1603 = i2c_get_clientdata(i2c);
	vt1603_mfd_release(vt1603);
	return 0;
}

static const struct i2c_device_id vt1603_i2c_id[] = {
	{ "vt1603", VT1603 },
	{ "vt1609", VT1609 },
};
MODULE_DEVICE_TABLE(i2c, vt1603_i2c_id);

/* corgi i2c codec control layer */
static struct i2c_driver vt1603_i2c_driver = {
	.driver = {
		.name = "VT1603",
		.owner = THIS_MODULE,
	},
	.probe =    vt1603_i2c_probe,
	.remove =   __devexit_p(vt1603_i2c_remove),
	.id_table = vt1603_i2c_id,
};	
#endif

/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/

#if defined(CONFIG_SPI_MASTER) && defined(CONFIG_VT1603_IOCTRL_SPI)
static int vt1603_spi_write_then_read(struct spi_device *spi,
		const u8 *txbuf, unsigned n_tx,
		u8 *rxbuf, unsigned n_rx)
{
	static DEFINE_MUTEX(lock);
	static u8 buf[32];
	
	int		 status;
	struct spi_message	message;
	struct spi_transfer	x;
	u8		 *local_buf;

	/* Use preallocated DMA-safe buffer.  We can't avoid copying here,
	 * (as a pure convenience thing), but we can keep heap costs
	 * out of the hot path ...
	 */
	if ((n_tx + n_rx) > ARRAY_SIZE(buf))
		return -EINVAL;
	
	spi_message_init(&message);
	memset(&x, 0, sizeof x);
	x.len = n_tx + n_rx;
	spi_message_add_tail(&x, &message);
	
	/* ... unless someone else is using the pre-allocated buffer */
	if (!mutex_trylock(&lock)) {
		local_buf = kmalloc(ARRAY_SIZE(buf), GFP_KERNEL);
		if (!local_buf)
			return -ENOMEM;
	}
	else {
		memset(buf, 0x00, sizeof(buf));
		local_buf = buf;
	}
	
	memcpy(local_buf, txbuf, n_tx);
	x.tx_buf = local_buf;
	x.rx_buf = local_buf;
	
	/* do the i/o */
	status = spi_sync(spi, &message);
	if (status == 0)
		memcpy(rxbuf, x.rx_buf + n_tx, n_rx);
	
	if (x.tx_buf == buf)
		mutex_unlock(&lock);
	else
		kfree(local_buf);
	
	return status;
}

static int vt1603_spi_write(struct vt1603 *vt1603, u8 reg, u8 val)
{
	int ret = 0;
	u8 xfer[3] = { 0 };
	struct spi_device *spi = vt1603->control_data;

	xfer[0] = ((reg & 0xff) | 0x80); 
	xfer[1] = ((reg & 0xff) >> 7);
	xfer[2] = val & 0xff;
	ret = spi_write(spi, xfer, ARRAY_SIZE(xfer));
	if (ret != 0)
		pr_err("vt1603_spi_write[r:%d, v:%d] errcode[%d]\n", reg, val, ret);
	
	return ret;
}

static int vt1603_spi_read(struct vt1603 *vt1603, u8 reg, u8 *val)
{
	u8 addr[3] = { 0 };
	u8 data[3] = { 0 };
	int ret = 0;
	struct spi_device *spi = vt1603->control_data;

	addr[0] = ((reg & 0xff) & (~ 0x80)); 
	addr[1] = ((reg & 0xff) >> 7);
	addr[2] = 0xff;
	ret = vt1603_spi_write_then_read(spi, addr, 2, data, 3);
	if(ret != 0)
		pr_err("vt1603_spi_read[r:%d] errcode[%d]\n", reg, ret);

	*val = data[2];
	return ret;
}

static int vt1603_spi_probe(struct spi_device *spi )
{
	struct vt1603 *vt1603;
	const struct spi_device_id *id = spi_get_device_id(spi);

	vt1603 = kzalloc(sizeof(struct vt1603), GFP_KERNEL);
	if (vt1603 == NULL)
		return -ENOMEM;

	vt1603->dev = &spi->dev;
	vt1603->control_data = spi;
	vt1603->reg_read = vt1603_spi_read;
	vt1603->reg_write = vt1603_spi_write;
	vt1603->type = id->driver_data;
	
	spi_set_drvdata(spi, vt1603);
	dev_set_drvdata(vt1603->dev, vt1603);

	return vt1603_mfd_init(vt1603);
}

static int vt1603_spi_remove(struct spi_device *spi)
{
	struct vt1603 *vt1603 = spi_get_drvdata(spi);
	vt1603_mfd_release(vt1603);
	return 0;
}

static const struct spi_device_id vt1603_spi_id[] = {
	{ "vt1603", VT1603},
	{ "vt1609", VT1609},
};
MODULE_DEVICE_TABLE(spi, vt1603_spi_id);

static struct spi_driver vt1603_spi_driver = {
	.driver = {
		.name = "VT1603",
		.owner = THIS_MODULE,
	},
	.probe = vt1603_spi_probe,
	.remove = __devexit_p(vt1603_spi_remove),
	.id_table = vt1603_spi_id,
};
#endif

/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/

static int __init vt1603_init(void)
{
	int ret = -1;
#if defined(CONFIG_I2C) && defined(CONFIG_VT1603_IOCTRL_I2C)
	ret = i2c_add_driver(&vt1603_i2c_driver);
	if (ret != 0)
		pr_err("Failed to register vt1603 I2C driver: %d\n", ret);
#endif
#if defined(CONFIG_SPI_MASTER) && defined(CONFIG_VT1603_IOCTRL_SPI)
	ret = spi_register_driver(&vt1603_spi_driver);
	if (ret != 0)
		pr_err("Failed to register vt1603 SPI driver: %d\n", ret);
#endif
	return ret;
}
module_init(vt1603_init);

static void __exit vt1603_exit(void)
{
#if defined(CONFIG_I2C) && defined(CONFIG_VT1603_IOCTRL_I2C)
	i2c_del_driver(&vt1603_i2c_driver);
#endif
#if defined(CONFIG_SPI_MASTER) && defined(CONFIG_VT1603_IOCTRL_SPI)
	spi_unregister_driver(&vt1603_spi_driver);
#endif
}
module_exit(vt1603_exit);


MODULE_AUTHOR("WonderMedia Technologies, Inc.");
MODULE_LICENSE("GPL");