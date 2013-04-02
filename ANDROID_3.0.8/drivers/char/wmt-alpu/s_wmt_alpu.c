/*++

	Some descriptions of such software.
	Copyright (c) 2012  WonderMedia Technologies, Inc.

	This program is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software Foundation,
	either version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
	PARTICULAR PURPOSE.  See the GNU General Public License for more details.
	You should have received a copy of the GNU General Public License along with
	this program.  If not, see <http://www.gnu.org/licenses/>.

	WonderMedia Technologies, Inc.
	2012-6-12, HowayHuo, ShenZhen
--*/
/*--- History -------------------------------------------------------------------
*Version 0.01 , HowayHuo, 2012/7/11
* First version: Porting from alpum_bypass_rev1.1
*
*------------------------------------------------------------------------------*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/random.h>


#include <mach/hardware.h>
#include <asm/uaccess.h>
/*********************************** Constant Macro **********************************/
#define ALPU_NAME          "ALPU-MP"
#define ALPU_I2C_ADDR      0x3d
#define ALPU_SUB_ADDR      0x80
#define ALPU_PROTOCOL_ADDR 0x75

#define ALPU_IOC_MAGIC           'a'
#define ALPU_SET_SUBADDR          _IOW(ALPU_IOC_MAGIC, 0, unsigned char)
#define ALPU_IOC_MAXNR           1
/********************************** Data type and local variable ***************************/
static struct i2c_client *s_alpu_client;
static int alpu_major;
struct class *class_alpu_dev;
struct device *alpu_device;
static unsigned char s_subaddr = ALPU_SUB_ADDR;
/********************************** Function declare **********************************/
//#define DEBUG

#undef DBG

#ifdef DEBUG
#define DBG(fmt, args...) printk(KERN_DEBUG "[%s]: " fmt, __FUNCTION__ , ## args)
#define INFO(fmt, args...) printk(KERN_INFO "[" ALPU_NAME "] " fmt , ## args)
#else
#define DBG(fmt, args...)
#define INFO(fmt, args...)
#endif

#define ERROR(fmt,args...) printk(KERN_ERR "[" ALPU_NAME "] " fmt , ## args)
#define WARNING(fmt,args...) printk(KERN_WARNING "[" ALPU_NAME "] " fmt , ## args)


extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);

/******************************** Function implement ****************************************/

//---------------------------------------------------------------------
// function : alpu_i2c_write_to_subaddr()
// parameter:
//           subaddr: write data to this address of ALPU-MP
//           pdata  : writed data buffer
//           len    : writed data numbers
// return   :
//           0: write success
//          -1: write fail
//---------------------------------------------------------------------
int alpu_i2c_write_to_subaddr(unsigned char subaddr, const unsigned char *pdata, int len)
{
	int ret;
	struct i2c_msg xfer[1];
	unsigned char *p_wbuf;

	if(s_alpu_client == NULL)
	{
	    ERROR("ALPU driver isn't register, alpu_i2c_write_to_subaddr() fail\n");
	    return -1;
	}

    p_wbuf = kmalloc(len + 1, GFP_KERNEL);
    if(!p_wbuf)
    {
        ERROR("alpu_i2c_write_to_subaddr() alloc memory fail\n");
        return -1;
    }

    /* [MSG1]: fill the register address data */
    *p_wbuf = subaddr;

     /* fill the Tx buffer */
    memcpy(p_wbuf + 1, pdata, len);

	xfer[0].addr = s_alpu_client->addr;
	xfer[0].flags = 0;
	xfer[0].buf = p_wbuf;
	xfer[0].len = len + 1;

	/* i2c_transfer returns number of messages transferred */
	ret = i2c_transfer(s_alpu_client->adapter, xfer, ARRAY_SIZE(xfer));
	kfree(p_wbuf);
	if (ret != ARRAY_SIZE(xfer)) {
		ERROR("alpu_i2c_write_to_subaddr err [%d]\n", ret);
		if (ret < 0)
			return ret;
		else
			return -EIO;
	}

	return 0;
}

//---------------------------------------------------------------------
// function : alpu_i2c_read_from_subaddr()
// parameter:
//           subaddr : read data from this address of ALPU-MP
//           pdata   : read data buffer
//           len     : read data numbers
// return   :
//           0: read success
//          -1: read fail
//---------------------------------------------------------------------
static int alpu_i2c_read_from_subaddr(unsigned char subaddr, unsigned char *pdata, int len)
{
	int ret;
	struct i2c_msg xfer[2];
	unsigned char wbuf[1];

	if(s_alpu_client == NULL)
	{
	    ERROR("ALPU driver isn't register, alpu_i2c_read_from_subaddr() fail\n");
	    return -1;
	}

	/* [MSG1] fill the register address data */
	wbuf[0] = subaddr;

	xfer[0].addr = s_alpu_client->addr;
	xfer[0].flags = 0;
	xfer[0].buf = wbuf;
	xfer[0].len = 1;

	/* [MSG2] fill the rx buffer */
	xfer[1].addr = s_alpu_client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = len;
	xfer[1].buf = pdata;

	/* i2c_transfer returns number of messages transferred */
	ret = i2c_transfer(s_alpu_client->adapter, xfer, ARRAY_SIZE(xfer));
	if (ret != ARRAY_SIZE(xfer)) {
		ERROR("alpu_i2c_read_from_subaddr err [%d]\n", ret);
		if (ret < 0)
			return ret;
		else
			return -EIO;
	}

	return 0;
}

//---------------------------------------------------------------------
// function : alpu_i2c_write()
// parameter:
//           pdata: writed data buffer
//           len  : writed data numbers
// return   :
//           0: write success
//          -1: write fail
//---------------------------------------------------------------------
int alpu_i2c_write(const unsigned char *pdata, int len)
{
    return alpu_i2c_write_to_subaddr(ALPU_SUB_ADDR, pdata, len);
}

//---------------------------------------------------------------------
// function : alpu_i2c_read()
// parameter:
//           pdata: read data buffer
//           len  : read data numbers
// return   :
//           0: read success
//          -1: read fail
//---------------------------------------------------------------------
int alpu_i2c_read(unsigned char *pdata, int len)
{
    return alpu_i2c_read_from_subaddr(ALPU_SUB_ADDR, pdata, len);
}

//---------------------------------------------------------------------
// function : alpu_check_exist()
// parameter: None
// return   :
//           0: ALPU-MP is exist
//          -1: ALPU-MP isn't exist
//---------------------------------------------------------------------
int alpu_check_exist(void)
{
    int ret;
    unsigned char buffer_data[8];  //Protocol Data Buffer

    //Read Protocol Test(No Stop Condition)
    ret = alpu_i2c_read_from_subaddr(ALPU_PROTOCOL_ADDR, buffer_data, 8);
    if(ret)
    {
        INFO("ALPU-MP (I2C address 0x%02x) is not found\n", ALPU_I2C_ADDR);
        //panic("ALPU-MP: illegal authorization\n");
        return -1;
    }

    return 0;
}

//--------------------------------------------------------------------
// function : alpu_bypass_test()
// parameter: None
// purpose  :  SOC generates 8 bytes data randomly and send the data to
//             ALPU-MP. ALPU-MP XOR these data with 0x01. SOC read 10 bytes
//             from ALPU-MP. If the preceding 8 bytes data is XOR right,
//             then test pass. Otherwise, test fail
// return   :
//           0: test pass
//          -1: test fail
//--------------------------------------------------------------------
int alpu_bypass_test(void)
{
    int i, j, ret;
    unsigned char alpum_tx_data[8];  //Write Data Buffer
    unsigned char alpum_rx_data[10]; //Read Data Buffer
    unsigned char alpum_ex_data[8];  //Expected Data Buffer
    unsigned char buffer_data[8];    //Protocol Data Buffer

    for(j=0; j<10; j++)
    {
        //Read Protocol Test(No Stop Condition)
        ret = alpu_i2c_read_from_subaddr(ALPU_PROTOCOL_ADDR, buffer_data, 8);
        if(ret)
        {
            printk("ALPU-MP (I2C address 0x%02x) is not found\n", ALPU_I2C_ADDR);
            printk("ALPU-MP Bypass Test Fail!!\n");
            return -1;
        }

        //Seed Generate
        get_random_bytes(alpum_tx_data, 8);

        //Writh Seed Data to ALPU-M
	    //sub_address=0x80(Bypass Mode Set)
	    ret = alpu_i2c_write(alpum_tx_data, 8);
	    if(ret)
	    {
            printk("ALPU-MP I2C Write Fail\n");
            printk("ALPU-MP Bypass Test Fail!!\n");
            return -1;
	    }

        //Read Result Data from ALPU-M
        ret = alpu_i2c_read(alpum_rx_data, 10);
        if(ret)
        {
            printk("ALPU-MP I2C Read Fail\n");
            printk("ALPU-MP Bypass Test Fail!!\n");
            return -1;
        }

        //XOR operation
	    for( i=0; i<8; i++ )
            alpum_ex_data[i] = alpum_tx_data[i] ^ 0x01;

        //Compare the encoded data and received data
        ret = 0;
	    for (i=0; i<8; i++)
        {
		    if (alpum_ex_data[i] != alpum_rx_data[i])
		    {
		        ret = -1;
                break;
		    }
	    }

        printk("\n");

        if(ret)
            printk(" ALPU-MP Bypass Test Fail!!\n");
        else
            printk(" ALPU-M Bypass Test Success!!!\n");

        //Show Result
        printk("=============================================================\n");
        printk(" Result  : %d\n", ret);
        printk(" Tx Data : "); for (i=0; i<8; i++) printk("0x%02x ", alpum_tx_data[i]); printk("\n");
        printk(" Rx Data : "); for (i=0; i<10; i++) printk("0x%02x ", alpum_rx_data[i]); printk("\n");
        printk(" Ex Data : "); for (i=0; i<8; i++) printk("0x%02x ", alpum_ex_data[i]); printk("\n");
        printk("=============================================================\n");
        printk(" Buffer Data : "); for(i = 0; i < 8; i++) printk("0x%02x ", buffer_data[i]); printk("\n");
        printk("=============================================================\n");
    }
        return ret;
}

/**************************** driver file operations struct definition *************************/
static int alpu_open(struct inode *inode, struct file *file)
{
	INFO("alpu_open\n");
	return 0;
}

static int alpu_release(struct inode *inode, struct file *file)
{
	INFO("alpu_release\n");
	return 0;
}

static ssize_t alpu_read(
	struct file *filp, /*!<; //[IN] a pointer point to struct file  */
	char __user *buf,  /*!<; // please add parameters description her*/
	size_t count,      /*!<; // please add parameters description her*/
	loff_t *f_pos	   /*!<; // please add parameters description her*/
)
{
	int ret;
    unsigned char *pdata;

	INFO("alpu_read\n");

    if(count < 1)
    {
        ERROR("alpu_read() count < 1\n");
        return -EINVAL;
    }

    pdata = kmalloc(count, GFP_KERNEL);
    if(!pdata)
    {
        ERROR("alpu_read() alloc memory fail\n");
        return -ENOMEM;
    }

    ret = alpu_i2c_read_from_subaddr(s_subaddr, pdata, count);
    if(ret)
    {
        kfree(pdata);
        return -EFAULT;
    }

	ret = copy_to_user(buf, pdata, count);
	if(ret)
	{
		ERROR("alpu_read: copy_to_user fail\n");
		kfree(pdata);
		return -EFAULT;
	}

    kfree(pdata);
	return count;
}

static ssize_t alpu_write(
	struct file *filp, /*!<; //[IN] a pointer point to struct file  */
	const char __user *buf,  /*!<; // please add parameters description her*/
	size_t count,      /*!<; // please add parameters description her*/
	loff_t *f_pos	   /*!<; // please add parameters description her*/
)
{
	int ret;
	unsigned char *pdata;

	INFO("alpu_write\n");

    if(count < 1)
    {
        ERROR("alpu_write() count < 1\n");
        return -EINVAL;
    }

    pdata = kmalloc(count, GFP_KERNEL);
    if(!pdata)
    {
        ERROR("alpu_write() alloc memory fail\n");
        return -ENOMEM;
    }

	ret = copy_from_user(pdata, buf, count);
	if(ret)
	{
		ERROR("alpu_write: copy_from_user fail\n");
		kfree(pdata);
		return -EFAULT;
	}

    ret = alpu_i2c_write_to_subaddr(s_subaddr, pdata, count);

    kfree(pdata);

	if(ret)
		return -EFAULT;

	return count;
}

static long alpu_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	int value;

	INFO("alpu_ioctl\n");

	/* check type and number, if fail return ENOTTY */
	if( _IOC_TYPE(cmd) != ALPU_IOC_MAGIC )
		return -ENOTTY;
	if( _IOC_NR(cmd) >= ALPU_IOC_MAXNR )
		return -ENOTTY;

	/* check argument area */
	if( _IOC_DIR(cmd) & _IOC_READ )
	{
		ret = !access_ok( VERIFY_WRITE, (void __user *) arg, _IOC_SIZE(cmd));
		if (ret)
			ERROR("VERIFY_WRITE failed\n");
	}
	else if ( _IOC_DIR(cmd) & _IOC_WRITE )
	{
		ret = !access_ok( VERIFY_READ, (void __user *) arg, _IOC_SIZE(cmd));
		if (ret)
			ERROR("VERIFY_READ failed\n");
	}

	if(ret)
		return -EFAULT;

	switch (cmd)
	{
		case ALPU_SET_SUBADDR:
			ret = get_user(value, (unsigned char *)arg);
			if(ret)
			{
			    ERROR("IOCTL ALPU_SET_SUBADDR fail\n");
				return ret;
			}
			INFO("IOCTL ALPU_SET_SUBADDR value = 0x%x\n", value);

            s_subaddr = value;

		break;

		default:
			ret = -ENOTTY;
	}
	return ret;
}

static const struct file_operations alpu_fops = {
	.owner		        = THIS_MODULE,
	.open		        = alpu_open,
	.release	        = alpu_release,
	.read 		        = alpu_read,
	.write 		        = alpu_write,
	.unlocked_ioctl		= alpu_ioctl,
};

/*************************** device driver struct definition *************************/
static int alpu_probe(struct platform_device *pdev)
{
	INFO("alpu_probe\n");

    return 0;
}

static int alpu_remove(struct platform_device *pdev)
{
	INFO("alpu_remove\n");

	return 0;
}


static struct platform_driver alpu_platform_driver = {
	.probe		= alpu_probe,
	.remove		= alpu_remove,
	.driver		= {
		.name	= ALPU_NAME,
		.owner	= THIS_MODULE,
	},
};

/************************** platform device struct definition **************************/
static void alpu_platform_release(
	struct device *device
)
{
	INFO("alpu_platform_release\n");
	return;
}

static struct platform_device alpu_platform_device = {
    .name           = ALPU_NAME,
    .id             = 0,
	.dev			=	{
		.release =	alpu_platform_release,
	},
	.num_resources	= 0,
	.resource		= NULL,

};

/************************** i2c device struct definition **************************/
static int __devinit alpu_i2c_probe(struct i2c_client *i2c,
    const struct i2c_device_id *id)
{
    INFO("alpu_i2c_probe\n");

    //alpu_bypass_test();

    return 0;
}

static int __devexit alpu_i2c_remove(struct i2c_client *client)
{
	INFO("alpu_i2c_remove\n");

	return 0;
}


static const struct i2c_device_id alpu_i2c_id[] = {
	{ALPU_NAME, 0},
	{ },
};
MODULE_DEVICE_TABLE(i2c, alpu_i2c_id);

static struct i2c_board_info __initdata alpu_i2c_board_info[] = {
    {
        I2C_BOARD_INFO(ALPU_NAME, ALPU_I2C_ADDR),
    },
};

static struct i2c_driver alpu_i2c_driver = {
    .driver = {
    	.name = ALPU_NAME,
    	.owner = THIS_MODULE,
    },
    .probe = alpu_i2c_probe,
    .remove = __devexit_p(alpu_i2c_remove),
    .id_table = alpu_i2c_id,
};

static int __init alpu_init(void)
{
	int ret;
	dev_t dev_no;
    struct i2c_adapter *adapter = NULL;

	INFO("ALPU driver init\n");

	char busNo[8] = {'\0', };
	int len = 8;
	wmt_getsyspara("wmt.alpu.i2c", busNo, &len);

	int num = 0;
	if(strlen(busNo) > 0)
		num = busNo[0] - '0';

    adapter = i2c_get_adapter(num);

	if (adapter == NULL) {
		ERROR("Can not get i2c adapter, client address error\n");
		return -ENODEV;
	}

	s_alpu_client = i2c_new_device(adapter, alpu_i2c_board_info);
	if (s_alpu_client == NULL) {
		ERROR("allocate i2c client failed\n");
		return -ENOMEM;
	}
    i2c_put_adapter(adapter);

	ret = i2c_add_driver(&alpu_i2c_driver);
	if (ret != 0)
	{
		ERROR("Failed to register ALPU I2C driver: %d\n", ret);
		i2c_unregister_device(s_alpu_client);
		return -EFAULT;
	}

	alpu_major = register_chrdev(0, ALPU_NAME, &alpu_fops);
	if (alpu_major < 0)
	{
		ERROR("get alpu_major failed\n");
		i2c_del_driver(&alpu_i2c_driver);
		i2c_unregister_device(s_alpu_client);
		return -EFAULT;
	}

    INFO("mknod /dev/%s c %d 0\n", ALPU_NAME, alpu_major);

	class_alpu_dev = class_create(THIS_MODULE, ALPU_NAME);
	if (IS_ERR(class_alpu_dev))
	{
		unregister_chrdev(alpu_major, ALPU_NAME);
		i2c_del_driver(&alpu_i2c_driver);
		i2c_unregister_device(s_alpu_client);

		ret = PTR_ERR(class_alpu_dev);
		ERROR("Can't create class : %s !!\n", ALPU_NAME);
		return ret;
	}

	dev_no = MKDEV(alpu_major, 0);
	alpu_device = device_create(class_alpu_dev, NULL, dev_no, NULL, ALPU_NAME);
    if (IS_ERR(alpu_device))
	{
		class_destroy(class_alpu_dev);
		unregister_chrdev(alpu_major, ALPU_NAME);
		i2c_del_driver(&alpu_i2c_driver);
		i2c_unregister_device(s_alpu_client);

    	ret = PTR_ERR(alpu_device);
    	ERROR("Failed to create device %s !!!", ALPU_NAME);
    	return ret;
    }

	ret = platform_device_register(&alpu_platform_device);
	if(ret)
	{
	    ERROR("can't register ALPU-MP device\n");

		device_destroy(class_alpu_dev, dev_no);
		class_destroy(class_alpu_dev);
		unregister_chrdev(alpu_major, ALPU_NAME);
		i2c_del_driver(&alpu_i2c_driver);
		i2c_unregister_device(s_alpu_client);

		return -ENODEV;
	}

	ret = platform_driver_register(&alpu_platform_driver);
	if(ret)
	{
		ERROR("can't register ALPU-MP driver\n");

		platform_device_unregister(&alpu_platform_device);
		device_destroy(class_alpu_dev, dev_no);
		class_destroy(class_alpu_dev);
		unregister_chrdev(alpu_major, ALPU_NAME);
		i2c_del_driver(&alpu_i2c_driver);
		i2c_unregister_device(s_alpu_client);

		return -ENODEV;
	}
	return ret;
}

static void __exit alpu_exit(void)
{
    dev_t dev_no;

	INFO("ALPU driver exit\n");

	platform_driver_unregister(&alpu_platform_driver);
    platform_device_unregister(&alpu_platform_device);
	dev_no = MKDEV(alpu_major, 0);
	device_destroy(class_alpu_dev, dev_no);
    class_destroy(class_alpu_dev);
	unregister_chrdev(alpu_major, ALPU_NAME);
	i2c_del_driver(&alpu_i2c_driver);
    if(s_alpu_client)
        i2c_unregister_device(s_alpu_client);
}

module_init(alpu_init);
module_exit(alpu_exit);

EXPORT_SYMBOL(alpu_i2c_write);
EXPORT_SYMBOL(alpu_i2c_read);
EXPORT_SYMBOL(alpu_check_exist);
EXPORT_SYMBOL(alpu_bypass_test);

MODULE_AUTHOR("WMT ShenZhen Driver Team");
MODULE_DESCRIPTION("WMT ALPU-MP Driver");
MODULE_LICENSE("GPL");
