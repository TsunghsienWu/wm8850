/*++
linux/arch/arm/mach-wmt/board.c

Copyright (c) 2012 WonderMedia Technologies, Inc.

This program is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software Foundation,
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License along with
this program.  If not, see <http://www.gnu.org/licenses/>.

WonderMedia Technologies, Inc.
10F, 529, Chung-Cheng Road, Hsin-Tien, Taipei 231, R.O.C.
--*/
#define fail -1
#include <mach/hardware.h>
#include <linux/sysctl.h>

extern unsigned int wmt_i2c0_speed_mode;
extern unsigned int wmt_i2c1_speed_mode;
static char MACVeeRom[32] = {0xFF};

#if 0
static int PM_USB_PostSuspend(void)
{
	return 0;
}


static int PM_USB_PreResume(void)
{
	unsigned int tmp32;
	unsigned char tmp8;

	/* EHCI PCI configuration seting */
	/* set bus master */
	*(volatile unsigned char*) 0xd8007804 = 0x17;
	tmp8 = *(volatile unsigned char*) 0xd8007804;
	if (tmp8 != 0x17) {
		printk("PM_USB_init : 0xd8007104 = %02x\n", tmp8);
		return fail;
	}

	/* set interrupt line */
	*(volatile unsigned char*) 0xd800783c = 0x2b;
	tmp8 = *(volatile unsigned char*) 0xd800783c;
	if (tmp8 != 0x2b) {
		printk("PM_USB_init : 0xd800713c = %02x\n", tmp8);
		return fail;
	}

	/* enable AHB master cut data to 16/8/4 DW align */
	tmp32 = *(volatile unsigned char*) 0xd800784c;
	tmp32 &= 0xfffdffff;
	*(volatile unsigned char*) 0xd800784c = tmp32;
	tmp32 = *(volatile unsigned char*) 0xd800784c;
	if ((tmp32&0x00020000) != 0x00000000)
		printk("[EHCI] Programming EHCI PCI Configuration Offset Address : 4ch fail! \n");


	/* UHCI PCI configuration setting */
	/* set bus master */
	*(volatile unsigned char*) 0xd8007a04 = 0x07;
	tmp8 = *(volatile unsigned char*) 0xd8007a04;
	if (tmp8 != 0x07) {
		printk("PM_USB_init : 0xd8007304 = %02x\n", tmp8);
		return fail;
	}

	/* set interrupt line */
	*(volatile unsigned char*) 0xd8007a3c = 0x2b;
	tmp8 = *(volatile unsigned char*) 0xd8007a3c;
	if (tmp8 != 0x2b) {
		printk("PM_USB_init : 0xd800733c = %02x\n", tmp8);
		return fail;
	}

	return 0;
}
#endif

#if 0
static void PM_MAC_PreResume(void)
{
	int i = 0;
	*((volatile unsigned char*)0xd8004104) = 0x4;

	/* restore VEE */
	for (i = 0; i < 8; i++)
		*(volatile unsigned long *)(0xd800415C + i*4) = *((int *)MACVeeRom + i);

	/* reload VEE */
	*(volatile unsigned char*)0xd800417C |= 0x01;

	/* reload */
	*(volatile unsigned char*)0xd8004074 |= 0xa0;

	/* check reload VEE complete */
	while ((*(volatile unsigned char*)0xd800417C & 0x02) != 0x02);

	return;
}
static void PM_MAC_PostSuspend(void)
{
	int i = 0;

	/*save VEE*/
	for (i = 0; i < 32; i++)
		MACVeeRom[i] = *((volatile unsigned char*)0xd800415C + i);

	return;
}
#endif

static void PM_I2C_PreResume(void)
{
	printk("PM_I2C_PreResume\n");
	
	/*pre resume port 0*/
	PMCEL_VAL |= 0x0100;/*BIT 8*/
	GPIO_CTRL_GP21_I2C_BYTE_VAL &= ~(BIT0 | BIT1);
	GPIO_PULL_EN_GP21_I2C_BYTE_VAL |= (BIT0 | BIT1);
	GPIO_PULL_CTRL_GP21_I2C_BYTE_VAL |= (BIT0 | BIT1);

	
	/*
	*(volatile unsigned short *)(0xD8280000) = 0;
	*(volatile unsigned short *)(0xD828000A) = 12;
	*(volatile unsigned short *)(0xD8280008) = 0x07;
	*(volatile unsigned short *)(0xD8280000) = 0x0001;
	*/

	*(volatile unsigned short *)(I2C0_BASE_ADDR) = 0;
	*(volatile unsigned short *)(I2C0_BASE_ADDR + 0x0000000A) = 12;
	*(volatile unsigned short *)(I2C0_BASE_ADDR + 0x00000008) = 0x07;
	if (wmt_i2c0_speed_mode == 0)
		*(volatile unsigned short *)(I2C0_BASE_ADDR + 0x0000000C) = I2C_TR_STD_VALUE;
	else
		*(volatile unsigned short *)(I2C0_BASE_ADDR + 0x0000000C) = I2C_TR_FAST_VALUE;

	*(volatile unsigned short *)(I2C0_BASE_ADDR) = 0x0001;

	/* pre resume port 1 */
	PMCEL_VAL |= 0x0200; /*BIT9*/
	GPIO_CTRL_GP21_I2C_BYTE_VAL &= ~(BIT2 | BIT3);
	GPIO_PULL_EN_GP21_I2C_BYTE_VAL |= (BIT2 | BIT3);
	GPIO_PULL_CTRL_GP21_I2C_BYTE_VAL |= (BIT2 | BIT3);
	/*
	*(volatile unsigned short *)(0xD8320000) = 0;
	*(volatile unsigned short *)(0xD832000A) = 12;
	*(volatile unsigned short *)(0xD8320008) = 0x07;
	*(volatile unsigned short *)(0xD8320000) = 0x0001;
	*/
	*(volatile unsigned short *)(I2C1_BASE_ADDR) = 0;
	*(volatile unsigned short *)(I2C1_BASE_ADDR + 0x0000000A) = 12;
	*(volatile unsigned short *)(I2C1_BASE_ADDR + 0x00000008) = 0x07;
	if (wmt_i2c1_speed_mode == 0)
		*(volatile unsigned short *)(I2C1_BASE_ADDR + 0x0000000C) = I2C_TR_STD_VALUE;
	else
		*(volatile unsigned short *)(I2C1_BASE_ADDR + 0x0000000C) = I2C_TR_FAST_VALUE;
	*(volatile unsigned short *)(I2C1_BASE_ADDR) = 0x0001;
}

int PM_device_PreResume(void)
{
	int result;
	/* PM_MAC_PreResume(); */
	PM_I2C_PreResume();

	/*
	result = PM_USB_PreResume();
	if (!result)
		goto FAIL;
	*/

	return 0;
/* FAIL: */
	return result;
}


int PM_device_PostSuspend(void)
{
	/* PM_MAC_PostSuspend(); */
	return 0;
}
