/*++
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
	10F, 529, Chung-Cheng Road, Hsin-Tien, Taipei 231, R.O.C.
--*/

#include <linux/platform_device.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/sysfs.h>
#include <linux/interrupt.h>
#include <linux/syscore_ops.h>

#include <mach/hardware.h>
#include <asm/irq.h>
#include <mach/irqs.h>
#include "wmt_ebm.h"

#define EBM_CTRL_REG 0xFE130700
#define EBM_STS_REG 0xFE130701
#define EBM_INTEN_REG 0xFE130704 /*1 byte*/
#define EBM_INTSTS_REG 0xFE130705 /*1 byte*/
#define EBM_INTTPY_REG 0xFE130706 /*2 byte*/

static DEFINE_SPINLOCK(wmt_ebm_lock);

static struct ebm_dev_reg_s *wmt_ebm_regs;
static unsigned int firmware_base_addr = 0x1C000000;/*448M*/
static unsigned int firmware_len = 0;
static unsigned int ebm_enable = 0;

static void (*mdm_rdy_callback)(void *data);
static void *mdm_rdy_callback_data;
static void (*ebm_wakereq_callback)(void *data);
static void *ebm_wakereq_callback_data;
static void (*mdm_wake_ap_callback)(void *data);
static void *mdm_wake_ap_callback_data;

extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);

void wmt_ebm_register_callback(enum ebm_callback_func_e callback_num,
				void (*callback)(void *data), void *callback_data)
{
	unsigned long flags;
	spin_lock_irqsave(&wmt_ebm_lock, flags);
	if (callback_num == MDM_RDY) {
		mdm_rdy_callback = callback;
		mdm_rdy_callback_data = callback_data;
	} else if (callback_num == EBM_WAKEREQ) {
		ebm_wakereq_callback = callback;
		ebm_wakereq_callback_data = callback_data;
	} else if (callback_num == MDM_WAKE_AP) {
		mdm_wake_ap_callback = callback;
		mdm_wake_ap_callback_data = callback_data;
	}
	spin_unlock_irqrestore(&wmt_ebm_lock, flags);
	return;
}
EXPORT_SYMBOL(wmt_ebm_register_callback);
static void ebm_wake_ack(unsigned int ack)
{
	if (ack == 1)
		GPIO_OD_GP2_WAKEUP_SUS_BYTE_VAL |= BIT6;
	else
		GPIO_OD_GP2_WAKEUP_SUS_BYTE_VAL &= ~BIT6;
	return;
}
static void ap_rdy(unsigned int ready)
{	
	if (ready == 1)
		*(volatile unsigned char *)(EBM_CTRL_REG) |= BIT3;
	else
		*(volatile unsigned char *)(EBM_CTRL_REG) &= ~BIT3;
	return;
}
void ap_wake_mdm(unsigned int wake)
{
	if (wake == 1)
		*(volatile unsigned char *)(EBM_CTRL_REG) |= BIT4;
	else
		*(volatile unsigned char *)(EBM_CTRL_REG) &= ~BIT4;
	return;
}
EXPORT_SYMBOL(ap_wake_mdm);

void mdm_reset(unsigned int reset)
{
	if (reset == 1)
		GPIO_OD_GP2_WAKEUP_SUS_BYTE_VAL |= BIT5;
	else
		GPIO_OD_GP2_WAKEUP_SUS_BYTE_VAL &= ~BIT5;
	return;
}
EXPORT_SYMBOL(mdm_reset);

unsigned int is_ebm_enable(void)
{
	return ebm_enable;
}
EXPORT_SYMBOL(is_ebm_enable);

unsigned int get_firmware_baseaddr(void)
{
	return firmware_base_addr;
}
EXPORT_SYMBOL(get_firmware_baseaddr);

unsigned int get_firmware_len(void)
{
	return firmware_len;
}
EXPORT_SYMBOL(get_firmware_len);

static int init_ebm_gpio_func(void)
{
	/*set SUS_GPIO0(EBM_RESET)*/
	GPIO_CTRL_GP2_WAKEUP_SUS_BYTE_VAL |= BIT5;
	GPIO_OC_GP2_WAKEUP_SUS_BYTE_VAL |= BIT5; 
	GPIO_OD_GP2_WAKEUP_SUS_BYTE_VAL &= ~BIT5;
	GPIO_OC_GP2_WAKEUP_SUS_BYTE_VAL |= BIT5;

#if 0
	/*set SUSGPIO3, SUS_GPIO1,WAKEUP1,SUS_GPIO2*/
	GPIO_CTRL_GP2_WAKEUP_SUS_BYTE_VAL |= BIT1 | BIT2 | BIT3 | BIT6;
	/*set SUS_GPIO1 as output*/
	GPIO_OC_GP2_WAKEUP_SUS_BYTE_VAL |= BIT6;
	/*set SUS_GPIO3,SUS_GPIO2 and WAKEUP1 as input*/
	GPIO_OC_GP2_WAKEUP_SUS_BYTE_VAL &= ~(BIT1 | BIT2 | BIT3);
#endif

	/*init EBM pin*/
	/*set SD3WP/EBM_DATA[6]*/
	GPIO_CTRL_GP13_SD3_BYTE_VAL &= ~(BIT3);

	/*
	 * MII0RXCLK/EBM_DATA[3]
	 * PHY25MHZ/AP_RDY
	 * MII0RXD0/EBM_DATA[9]
	 * MII0RXD1/EBM_DATA[14]
	 * MII0RXD2/EBM_BHE
	 * MII0RXD3/EBM_BLE
	 * MII0RXDV/EBM_A[20]
	 * MII0RXERR/EBM_A[19]
	 */
	GPIO_CTRL_GP17_MII0RX_BYTE_VAL = 0x0;

	/*
	 * SD3PWRSW/EBM_DATA[11]
	 */
	GPIO_CTRL_GP18_SDWRSW_BYTE_VAL &= ~(BIT6);

	/*
	 * MII0CRS/EBM_DATA[7]
	 * MII0COL/EBM_A[21]
	 * MII0TXEN/EBM_CLK
	 * MII0TXD3/EBM_DATA[8]
	 * MII0TXD2/EBM_DATA[12]
	 * MII0TXD1/EBM_CS
	 * MII0TXD0/EBM_WR
	 * MII0TXCLK/AP_WAKE_MDM
	 */
	GPIO_CTRL_GP19_MII0TX_BYTE_VAL = 0x0;
	
	/*
	 * MII0PHYPD/EBM_DATA[2]
	 * MII0PHYRST/EBM_ADV
	 * MII0MDC/EBM_RDY
	 * MII0MDIO/EBM_OE
	 */
	GPIO_CTRL_GP20_MII0_BYTE_VAL &= ~(BIT0 | BIT1 | BIT2 | BIT3);
	/*
	 * SD2WP/KPADROW6/EBM_DATA[0]
	 */
	GPIO_CTRL_GP24_SD2KPAD_BYTE_VAL &= ~BIT3;
	/*
	 * SD2PWRSW/KPADROW7/EBM_DATA[13]
	 */
	GPIO_CTRL_GP25_SD2WRSW_BYTE_VAL &= ~BIT1;
	/*
	 * C25MHZCLKI/PWMOUT1/SPI0SS3/EBM_A[25]
	 * C24MHZCLKI/SPI0SS2/EBM_DATA[1]
	 * CLKOUT/EBM_A[22]
	 */
	GPIO_CTRL_GP31_BYTE_VAL &= ~(BIT0 | BIT1 | BIT2);
	/*
	 * GPIO23/EBM_A[16]
	 * GPIO22/EBM_DATA[10]
	 * GPIO21/EBM_A[17]
	 * GPIO20/EBM_A[24]
	 */
	GPIO_CTRL_GPIO_23_20_BYTE_VAL &= ~(BIT7 | BIT6 | BIT5 | BIT4);
	/*
	 * GPIO27/EBM_A[23]
	 * GPIO26/EBM_DATA[15]
	 * GPIO25/EBM_DATA[5]
	 * GPIO24/EBM_DATA[4]  
	 */
	GPIO_CTRL_GPIO_27_24_BYTE_VAL &= ~(BIT0 | BIT1 | BIT2 | BIT3);
	/*
	 * UART2RXD/SPI1SS0
	 * UART2TXD/SPI1MISO
	 */
	GPIO_CTRL_GP23_UART_2_3_BYTE_VAL &= ~(BIT1 | BIT3);

	/*set PIN sharing*/
	/*
	 * 0: SPI0SS# 1: 24MHz output
	 * 0: KEYPAD or EBM depending on bit[24] 1: SDC2,
	 * 0: SPI0SS3# 1: PWMOUT1
	 */
	GPIO_PIN_SHARING_SEL_4BYTE_VAL &= ~(BIT11 | BIT6 | BIT4);
	/*0: CIRIN pin select  1: WAKEUP1/MDM_RDY pin select,0: Non-EBM mode   1: EBM mode */
	GPIO_PIN_SHARING_SEL_4BYTE_VAL |= (BIT25 | BIT24);


	/*set SUSGPIO3, SUS_GPIO1,WAKEUP1,SUS_GPIO2*/
	GPIO_CTRL_GP2_WAKEUP_SUS_BYTE_VAL |= BIT1 | BIT2 | BIT3 | BIT6;
	/*set SUS_GPIO1 as output*/
	GPIO_OC_GP2_WAKEUP_SUS_BYTE_VAL |= BIT6;
	/*set SUS_GPIO3,SUS_GPIO2 and WAKEUP1 as input*/
	GPIO_OC_GP2_WAKEUP_SUS_BYTE_VAL &= ~(BIT1 | BIT2 | BIT3);

	/*enable MDM_RDY, MDM_WAKE_AP, EBMWAKEREQ pu control*/
	GPIO_PULL_EN_GP2_WAKEUP_SUS_BYTE_VAL |= (BIT1 | BIT2 | BIT3);
	/*pull down*/
	GPIO_PULL_CTRL_GP2_WAKEUP_SUS_BYTE_VAL &= ~(BIT1 | BIT2 | BIT3);

	/*pull down SUS_GPIO1*/
	GPIO_OD_GP2_WAKEUP_SUS_BYTE_VAL &= ~BIT6;
	/*set SUS_GPIO1 as output*/
	GPIO_OC_GP2_WAKEUP_SUS_BYTE_VAL |= BIT6;
	return 0;
}
static void init_irq_type(void)
{
	/*
	 * set type
	 */
	/*MDM_RDY*/
	*(volatile unsigned short *)(EBM_INTTPY_REG) &= ~(BIT0 | BIT1 | BIT2);
	*(volatile unsigned short *)(EBM_INTTPY_REG) |= BIT2;
	/*MDM_WAKE_AP*/
	*(volatile unsigned short *)(EBM_INTTPY_REG) &= ~(BIT3 | BIT4 | BIT5);
	*(volatile unsigned short *)(EBM_INTTPY_REG) |= BIT5;
	/*EBM_WAKEREQ*/
	*(volatile unsigned short *)(EBM_INTTPY_REG) &= ~(BIT6 | BIT7 | BIT8);
	*(volatile unsigned short *)(EBM_INTTPY_REG) |= BIT8;
	/*
	 * clear interrupt status
	 */
	*(volatile unsigned char *)(EBM_INTSTS_REG) = BIT2 | BIT1 |BIT0;
	return;
}
static void enable_ebm_irq(int enable)
{
	if (enable == 1)
		*(volatile unsigned char *)(EBM_INTEN_REG) |= BIT2 | BIT1 | BIT0;
	else
		*(volatile unsigned char *)(EBM_INTEN_REG) &= ~(BIT2 | BIT1 | BIT0);
	return;
}
static void init_clock(void)
{
	auto_pll_divisor(DEV_EBM, CLK_ENABLE, 0, 0);
	auto_pll_divisor(DEV_EBM, SET_DIV, 2, 400);
	return;
}
static irqreturn_t mdm_rdy_handler(int irq, void *dev_id)
{
	unsigned long flags;
	*(volatile unsigned char *)(EBM_INTSTS_REG) = BIT0;
	spin_lock_irqsave(&wmt_ebm_lock, flags);
	if (mdm_rdy_callback)
		mdm_rdy_callback(mdm_rdy_callback_data);
	spin_unlock_irqrestore(&wmt_ebm_lock, flags);
	return IRQ_HANDLED;
}

static irqreturn_t ebm_wakereq_handler(int irq, void *dev_id)
{
	unsigned long flags;
	*(volatile unsigned char *)(EBM_INTSTS_REG) = BIT2;
	spin_lock_irqsave(&wmt_ebm_lock, flags);
	if (ebm_wakereq_callback)
		ebm_wakereq_callback(ebm_wakereq_callback_data);
	spin_unlock_irqrestore(&wmt_ebm_lock, flags);
	return IRQ_HANDLED;
}

static irqreturn_t mdm_wake_ap_handler(int irq, void *dev_id)
{
	unsigned int long flags; 
	*(volatile unsigned char *)(EBM_INTSTS_REG) = BIT1;
	spin_lock_irqsave(&wmt_ebm_lock, flags);
	if (mdm_wake_ap_callback)
		mdm_wake_ap_callback(mdm_wake_ap_callback_data);
	spin_unlock_irqrestore(&wmt_ebm_lock, flags);
	return IRQ_HANDLED;
}

#ifdef CONFIG_PM
static int wmt_ebm_suspend(void)
{
	wmt_ebm_regs->EbmcOnOff = 0x0;
	return 0;
}

static void wmt_ebm_resume(void)
{
	/*all ebm pins are in suspend domain*/
	/*
	init_clock();
	init_ebm_gpio_func();
	init_irq_type();
	enable_ebm_irq(1);
	ap_rdy(1);
	ebm_wake_ack(1);
	wmt_ebm_regs->EbmBaseAddr = firmware_base_addr;
	wmt_ebm_regs->EbmcOnOff = 0x1;
	*/
	return;
}
#else
#define wmt_ebm_suspend NULL
#define wmt_ebm_resume  NULL
#endif

static struct syscore_ops wmt_ebm_syscore_ops = {
	.suspend        = wmt_ebm_suspend,
	.resume         = wmt_ebm_resume,
};

static int wmt_ebm_probe(struct platform_device *pdev)
{
	int err = 0;
	struct resource *res;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	wmt_ebm_regs = (struct ebm_dev_reg_s *)res->start;
	/*init clock*/
	init_clock();
	/*init GPIO*/
	init_ebm_gpio_func();
	err = request_irq(IRQ_PMC_MDM_RDY, mdm_rdy_handler, IRQF_DISABLED, "mdm_rdy" , NULL);
	if(err){
		printk("PMC_MDM_RDY request irq failed!");
		goto exit_err;
	}
	err = request_irq(IRQ_PMC_EBM_WAKEREQ, ebm_wakereq_handler, IRQF_DISABLED, "ebm_wake_req" , NULL);
	if(err){
		printk("PMC_EBM_WAKEREQ request irq failed!");
		goto exit_err;
	}
	err = request_irq(IRQ_PMC_MDM_WAKE_AP, mdm_wake_ap_handler, IRQF_DISABLED, "mdm_wake_ap" , NULL);
	if(err){
		printk("PMC_MDM_WAKE_AP request irq failed!");
		goto exit_err;
	}
	init_irq_type();
	enable_ebm_irq(1);
	ap_rdy(1);
	ebm_wake_ack(1);
	wmt_ebm_regs->EbmBaseAddr = firmware_base_addr;
	wmt_ebm_regs->EbmcOnOff = 0x1;

#ifdef CONFIG_PM
	register_syscore_ops(&wmt_ebm_syscore_ops);
#endif
	return 0;
exit_err:
	return err;
}

static int wmt_ebm_remove(struct platform_device *pdev)
{
	return 0;
}

static struct resource wmt_ebm_resource[] = {
	[0] = {
		.start  = WMT_EBM_BASE_ADDR,
		.end    = (WMT_EBM_BASE_ADDR + 0xFFFF),
		.flags  = IORESOURCE_MEM,
	},
};

static struct platform_device wmt_ebm_device = {
	.name	= "wmt_ebm",
	.id	= 0,
	.num_resources  = ARRAY_SIZE(wmt_ebm_resource),
	.resource       = wmt_ebm_resource,
};

static struct platform_driver ebm_platform_driver = {
	.probe = wmt_ebm_probe,
	.remove = wmt_ebm_remove,
	/*
	.suspend = wmt_ebm_suspend,
	.resume = wmt_ebm_resume,
	*/
	.driver = {
		.name = "wmt_ebm",
		.owner = THIS_MODULE,
	},
};

static int __init wmt_ebm_init(void)
{
	int ret;
	unsigned char buf[80];
	int varlen = 80;
	int param_num = 0;
	char *varname = "wmt.ebm.param";

	ret = wmt_getsyspara(varname, buf, &varlen);
	if (ret == 0) {
		param_num = sscanf(buf, "%d:%x:%x", &ebm_enable, &firmware_base_addr, &firmware_len);
		if (param_num != 3) {
			printk("EBM: parameter's format incorrect\n");
			return 0;
		}
		if (ebm_enable == 0) {
			printk("EBM disable\n");
			return 0;
		}
	} else
		return 0;
	ret = platform_device_register(&wmt_ebm_device);
	if (ret)
		printk("wmt-ebm:register device failed\n");
	return platform_driver_register(&ebm_platform_driver);
}

static void __exit wmt_ebm_deinit(void)
{
	platform_driver_unregister(&ebm_platform_driver);
}


__initcall(wmt_ebm_init);
__exitcall(wmt_ebm_deinit);

MODULE_AUTHOR("WonderMedia Technologies, Inc.");
MODULE_DESCRIPTION("WMT EBM Driver");
MODULE_LICENSE("GPL");
