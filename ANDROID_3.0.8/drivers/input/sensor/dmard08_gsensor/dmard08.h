/*
 * @file include/linux/dmard08.h
 * @brief DMT g-sensor Linux device driver
 * @author Domintech Technology Co., Ltd (http://www.domintech.com.tw)
 * @version 1.2
 * @date 2011/11/14
 *
 * @section LICENSE
 *
 *  Copyright 2011 Domintech Technology Co., Ltd
 *
 * 	This software is licensed under the terms of the GNU General Public
 * 	License version 2, as published by the Free Software Foundation, and
 * 	may be copied, distributed, and modified under those terms.
 *
 * 	This program is distributed in the hope that it will be useful,
 * 	but WITHOUT ANY WARRANTY; without even the implied warranty of
 * 	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * 	GNU General Public License for more details.
 *
 *
 */
#ifndef DMARD08_H
#define DMARD08_H

/*
#define DEVICE_I2C_NAME "dmard08"

//#define DMT_DEBUG_DATA	1
#define DMT_DEBUG_DATA 		0

#if DMT_DEBUG_DATA
#define IN_FUNC_MSG printk(KERN_INFO "@DMT@ In %s\n", __func__)
#define PRINT_X_Y_Z(x, y, z) printk(KERN_INFO "@DMT@ X/Y/Z axis: %04d , %04d , %04d\n", (x), (y), (z))
#define PRINT_OFFSET(x, y, z) printk(KERN_INFO "@offset@  X/Y/Z axis: %04d , %04d , %04d\n",offset.x,offset.y,offset.z);
#else
#define IN_FUNC_MSG
#define PRINT_X_Y_Z(x, y, z)
#define PRINT_OFFSET(x, y, z)
#endif
*/

//g-senor layout configuration, choose one of the following configuration
#define CONFIG_GSEN_LAYOUT_PAT_1
//#define CONFIG_GSEN_LAYOUT_PAT_2
//#define CONFIG_GSEN_LAYOUT_PAT_3
//#define CONFIG_GSEN_LAYOUT_PAT_4
//#define CONFIG_GSEN_LAYOUT_PAT_5
//#define CONFIG_GSEN_LAYOUT_PAT_6
//#define CONFIG_GSEN_LAYOUT_PAT_7
//#define CONFIG_GSEN_LAYOUT_PAT_8

#define CONFIG_GSEN_CALIBRATION_GRAVITY_ON_Z_NEGATIVE 1
#define CONFIG_GSEN_CALIBRATION_GRAVITY_ON_Z_POSITIVE 2
#define CONFIG_GSEN_CALIBRATION_GRAVITY_ON_Y_NEGATIVE 3
#define CONFIG_GSEN_CALIBRATION_GRAVITY_ON_Y_POSITIVE 4
#define CONFIG_GSEN_CALIBRATION_GRAVITY_ON_X_NEGATIVE 5
#define CONFIG_GSEN_CALIBRATION_GRAVITY_ON_X_POSITIVE 6

#define DEFAULT_SENSITIVITY 256
#define IOCTL_MAGIC  0x09
#define SENSOR_DATA_SIZE 3                           

#define SENSOR_RESET    			_IO(IOCTL_MAGIC, 0)
#define SENSOR_CALIBRATION   		_IOWR(IOCTL_MAGIC,  1, int[SENSOR_DATA_SIZE])
#define SENSOR_GET_OFFSET  			_IOR(IOCTL_MAGIC,  2, int[SENSOR_DATA_SIZE])
#define SENSOR_SET_OFFSET  			_IOWR(IOCTL_MAGIC,  3, int[SENSOR_DATA_SIZE])
#define SENSOR_READ_ACCEL_XYZ  		_IOR(IOCTL_MAGIC,  4, int[SENSOR_DATA_SIZE])

#define SENSOR_MAXNR 4

#endif

