/*++
linux/arch/arm/mach-wmt/irq.c

IRQ settings for WMT

Copyright (c) 2009 WonderMedia Technologies, Inc.

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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/ptrace.h>
#include <linux/syscore_ops.h>
#include <mach/hardware.h>
#include <linux/irq.h>
#include <asm/mach/irq.h>

#include "generic.h"

struct wmt_irqcfg_t {
	/* IRQ number */
	unsigned char irq;
	/* Routing and enable */
	unsigned char icdc;
};

/*
 * An interrupt initialising table, put GPIOs on the fisrt
 * group. Also route all interrupts to zac_irq decode block.
 * Change the table for future tuning.
 */
/* FIXME:remove it later, james */
static struct wmt_irqcfg_t wmt_init_irqcfgs[] __initdata =
{
	{ IRQ_GPIO,				ICDC_IRQ }, /*5*/
	{ IRQ_SDC1,				ICDC_IRQ }, /*1*/
	{ IRQ_SDC1_DMA,			ICDC_IRQ }, /*2*/
	{ IRQ_PMC_AXI_PWR,		ICDC_IRQ }, /*4*/
	{ IRQ_I2C2,				ICDC_IRQ }, /*7*/
	{ IRQ_KPAD,				ICDC_IRQ }, /*8*/
	{ IRQ_VDMA,				ICDC_IRQ }, /*9*/
	{ IRQ_ETH0,				ICDC_IRQ }, /*10*/
	{ IRQ_SDC2,				ICDC_IRQ }, /*11*/
	{ IRQ_SDC2_DMA,			ICDC_IRQ }, /*12*/
	{ IRQ_SDC3,				ICDC_IRQ }, /*13*/
	{ IRQ_SDC3_DMA,			ICDC_IRQ }, /*14*/
	{ IRQ_I2C3,				ICDC_IRQ }, /*15*/
	{ IRQ_APBB,				ICDC_IRQ }, /*16*/
	{ IRQ_DMA_CH_0,			ICDC_IRQ }, /*17*/
	{ IRQ_I2C1,				ICDC_IRQ }, /*18*/
	{ IRQ_I2C0,				ICDC_IRQ }, /*19*/
	{ IRQ_SDC0,				ICDC_IRQ }, /*20*/
	{ IRQ_SDC0_DMA,			ICDC_IRQ }, /*21*/
	{ IRQ_PMC_WAKEUP,		ICDC_IRQ }, /*22*/
	{ IRQ_PCM,				ICDC_IRQ }, /*23*/
	{ IRQ_SPI0,				ICDC_IRQ }, /*24*/
	{ IRQ_SPI1,				ICDC_IRQ }, /*25*/
	{ IRQ_UHDC,				ICDC_IRQ }, /*26*/
	{ IRQ_DMA_CH_1,			ICDC_IRQ }, /*27*/
	{ IRQ_NFC,				ICDC_IRQ }, /*28*/
	{ IRQ_NFC_DMA,			ICDC_IRQ }, /*29*/
	{ IRQ_EBMC,				ICDC_IRQ }, /*30*/
	{ IRQ_UART0,			ICDC_IRQ }, /*32*/
	{ IRQ_UART1,			ICDC_IRQ }, /*33*/
	{ IRQ_DMA_CH_2,			ICDC_IRQ }, /*34*/
	{ IRQ_OST7,				ICDC_IRQ }, /*35*/
	{ IRQ_OST0,				ICDC_IRQ }, /*36*/
	{ IRQ_OST1,				ICDC_IRQ }, /*37*/
	{ IRQ_OST2,				ICDC_IRQ }, /*38*/
	{ IRQ_OST3,				ICDC_IRQ }, /*39*/
	{ IRQ_DMA_CH_3,			ICDC_IRQ }, /*40*/
	{ IRQ_DMA_CH_4,			ICDC_IRQ }, /*41*/
	{ IRQ_OST4,				ICDC_IRQ }, /*42*/
	{ IRQ_OST5,				ICDC_IRQ }, /*43*/
	{ IRQ_OST6,				ICDC_IRQ }, /*44*/
	{ IRQ_DMA_CH_5,			ICDC_IRQ }, /*45*/
	{ IRQ_DMA_CH_6,			ICDC_IRQ }, /*46*/
	{ IRQ_UART2,			ICDC_IRQ }, /*47*/
	{ IRQ_RTC1,				ICDC_IRQ }, /*48*/
	{ IRQ_RTC2,				ICDC_IRQ }, /*49*/
	{ IRQ_UART3,			ICDC_IRQ }, /*50*/
	{ IRQ_DMA_CH_7,			ICDC_IRQ }, /*51*/
	{ IRQ_PMC_MDM_RDY,		ICDC_IRQ }, /*52*/
	{ IRQ_PMC_EBM_WAKEREQ,	ICDC_IRQ }, /*53*/
	{ IRQ_PMC_MDM_WAKE_AP,	ICDC_IRQ }, /*54*/
	{ IRQ_CIR,				ICDC_IRQ }, /*55*/
	{ IRQ_IC1_IRQ0,			ICDC_IRQ }, /*56*/
	{ IRQ_IC1_IRQ1,			ICDC_IRQ }, /*57*/
	{ IRQ_IC1_IRQ2,			ICDC_IRQ }, /*58*/
	{ IRQ_IC1_IRQ3,			ICDC_IRQ }, /*59*/
	{ IRQ_IC1_IRQ4,			ICDC_IRQ }, /*60*/
	{ IRQ_IC1_IRQ5,			ICDC_IRQ }, /*61*/
	{ IRQ_IC1_IRQ6,			ICDC_IRQ }, /*62*/
	{ IRQ_IC1_IRQ7,			ICDC_IRQ }, /*63*/
	{ IRQ_JDEC,				ICDC_IRQ }, /*64*/
	{ IRQ_SAE,				ICDC_IRQ }, /*66*/
	{ IRQ_VPP_IRQ0,			ICDC_IRQ }, /*67*/
	{ IRQ_VPP_IRQ1,			ICDC_IRQ }, /*68*/
	{ IRQ_VPP_IRQ2,			ICDC_IRQ }, /*69*/
	{ IRQ_MSVD,				ICDC_IRQ }, /*70*/
	{ IRQ_DZ_0,				ICDC_IRQ }, /*71*/
	{ IRQ_DZ_1,				ICDC_IRQ }, /*72*/
	{ IRQ_DZ_2,				ICDC_IRQ }, /*73*/
	{ IRQ_DZ_3,				ICDC_IRQ }, /*74*/
	{ IRQ_DZ_4,				ICDC_IRQ }, /*75*/
	{ IRQ_DZ_5,				ICDC_IRQ }, /*76*/
	{ IRQ_DZ_6,				ICDC_IRQ }, /*77*/
	{ IRQ_DZ_7,				ICDC_IRQ }, /*78*/
	{ IRQ_VPP_IRQ3,			ICDC_IRQ }, /*79*/
	{ IRQ_VPP_IRQ4,			ICDC_IRQ }, /*80*/
	{ IRQ_VPP_IRQ5,			ICDC_IRQ }, /*81*/
	{ IRQ_VPP_IRQ6,			ICDC_IRQ }, /*82*/
	{ IRQ_VPP_IRQ7,			ICDC_IRQ }, /*83*/
	{ IRQ_VPP_IRQ8,			ICDC_IRQ }, /*84*/
	{ IRQ_VPP_IRQ9,			ICDC_IRQ }, /*85*/
	{ IRQ_VPP_IRQ10,		ICDC_IRQ }, /*86*/
	{ IRQ_H264,				ICDC_IRQ }, /*87*/
	{ IRQ_VP8DEC,			ICDC_IRQ }, /*88*/
	{ IRQ_MALI_PMU,			ICDC_IRQ }, /*89*/
	{ IRQ_MALI_GPMMU,		ICDC_IRQ }, /*90*/
	{ IRQ_VPP_IRQ25,		ICDC_IRQ }, /*91*/
	{ IRQ_DMA_CH_8,			ICDC_IRQ }, /*92*/
	{ IRQ_DMA_CH_9,			ICDC_IRQ }, /*93*/
	{ IRQ_DMA_CH_10,		ICDC_IRQ }, /*94*/
	{ IRQ_DMA_CH_11,		ICDC_IRQ }, /*95*/
	{ IRQ_DMA_CH_12,		ICDC_IRQ }, /*96*/
	{ IRQ_DMA_CH_13,		ICDC_IRQ }, /*97*/
	{ IRQ_DMA_CH_14,		ICDC_IRQ }, /*98*/
	{ IRQ_DMA_CH_15,		ICDC_IRQ }, /*99*/
	{ IRQ_MALI_GP,			ICDC_IRQ }, /*100*/
	{ IRQ_MALI_PPMMU0,		ICDC_IRQ }, /*101*/
	{ IRQ_MALI_PP0,			ICDC_IRQ }, /*102*/
	{ IRQ_VPP_IRQ19,		ICDC_IRQ }, /*103*/
	{ IRQ_VPP_IRQ20,		ICDC_IRQ }, /*104*/
	{ IRQ_L220_L2,			ICDC_IRQ }, /*105*/
	{ IRQ_VPP_IRQ21,		ICDC_IRQ }, /*106*/
	{ IRQ_VPP_IRQ22,		ICDC_IRQ }, /*107*/
	{ IRQ_VPP_IRQ23,		ICDC_IRQ }, /*108*/
	{ IRQ_VPP_IRQ24,		ICDC_IRQ }, /*109*/
	{ IRQ_DZ_8,				ICDC_IRQ }, /*110*/
	{ IRQ_VPP_IRQ11,		ICDC_IRQ }, /*111*/
	{ IRQ_VPP_IRQ12,		ICDC_IRQ }, /*112*/
	{ IRQ_VPP_IRQ13,		ICDC_IRQ }, /*113*/
	{ IRQ_VPP_IRQ14,		ICDC_IRQ }, /*114*/
	{ IRQ_VPP_IRQ15,		ICDC_IRQ }, /*115*/
	{ IRQ_VPP_IRQ16,		ICDC_IRQ }, /*116*/
	{ IRQ_VPP_IRQ17,		ICDC_IRQ }, /*117*/
	{ IRQ_VPP_IRQ18,		ICDC_IRQ }, /*118*/
	{ IRQ_L220_ECNTR,		ICDC_IRQ }, /*119*/
	{ IRQ_L220_PARRT,		ICDC_IRQ }, /*120*/
	{ IRQ_L220_PARRD,		ICDC_IRQ }, /*121*/
	{ IRQ_L220_ERRWT,		ICDC_IRQ }, /*122*/
	{ IRQ_L220_ERRWD,		ICDC_IRQ }, /*123*/
	{ IRQ_L220_ERRRT,		ICDC_IRQ }, /*124*/
	{ IRQ_L220_ERRRD,		ICDC_IRQ }, /*125*/
	{ IRQ_L220_SLVERR,		ICDC_IRQ }, /*126*/
	{ IRQ_L220_DECERR,		ICDC_IRQ }, /*127*/
};

#define NR_WMT_IRQS  (sizeof(wmt_init_irqcfgs)/sizeof(struct wmt_irqcfg_t))

static struct wmt_irq_state_s {
	unsigned int  saved;
	unsigned int  girc;                     /* GPIO Interrupt Request Control register */
	unsigned int  girt;                     /* GPIO Interrupt Request Type register    */
	unsigned int  icpc[ICPC_NUM];           /* 8 decode block                          */
	unsigned char icdc[ICDC_NUM_WMT];       /* 64 interrupt destination control        */
} wmt_irq_state;

/*
 * Follows are handlers for normal irq_chip
 */
static void wmt_mask_irq(struct irq_data *data)
{
	if (data->irq < 64)
		ICDC0_VAL(data->irq) &= ~ICDC_ENABLE;
	else
		ICDC1_VAL(data->irq - 64) &= ~ICDC_ENABLE;
}

static void wmt_unmask_irq(struct irq_data *data)
{
	if (data->irq < 64)
		ICDC0_VAL(data->irq) |= ICDC_ENABLE;
	else
		ICDC1_VAL(data->irq-64) |= ICDC_ENABLE;
}

static void wmt_ack_irq(struct irq_data *data)
{
#if 0
	if ( irq < 36 || irq > 39 )	/* 36~39 is OST irq */
		printk("wmt_irq_ack, irq=%u\n", irq);
#endif
}

static struct irq_chip wmt_normal_chip = {
	.name   = "normal",
	.irq_ack    = wmt_ack_irq,
	.irq_mask   = wmt_mask_irq,
	.irq_unmask = wmt_unmask_irq,
};

static struct resource wmt_irq_resource = {
	.name  = "irqs",
	.start = 0xFE140000,
	.end   = 0xFE15ffff,
};

#ifdef CONFIG_PM
static int wmt_irq_suspend(void)
{
	int i;
	struct wmt_irq_state_s *st = &wmt_irq_state;

	/* printk("%s state=%d\n", __FUNCTION__, state); */

	st->saved = 1;

	/*
	 * Save interrupt priority.
	 */
	for (i = 0; i < ICPC_NUM; i++)
		st->icpc[i] = ICPC_VAL(i);

	/*
	 * Save interrupt routing and mask.
	 */
	for (i = 0; i < ICDC_NUM_WMT; i++) {
		if (i < 64)
			st->icdc[i] = ICDC0_VAL(i);
		else
			st->icdc[i] = ICDC1_VAL(i-64);
	}

	return 0;
}

static int wmt_irq_resume(void)
{
	int i;
	struct wmt_irq_state_s *st = &wmt_irq_state;

	if (st->saved) {
		/*
		 * Restore interrupt priority.
		 */
		for (i = 0; i < ICPC_NUM; i++)
			ICPC_VAL(i) = st->icpc[i];

		/*
		 * Restore interrupt routing and mask.
		 */
		for (i = 0; i < ICDC_NUM_WMT; i++) {
			if (i < 64)
				ICDC0_VAL(i) = st->icdc[i];
			else
				ICDC1_VAL(i-64) = st->icdc[i];
		}
	}
	return 0;
}

static struct syscore_ops wmt_irq_syscore_ops = {
	.suspend	= wmt_irq_suspend,
	.resume		= wmt_irq_resume,
};

static int __init wmt_irq_init_devicefs(void)
{
	register_syscore_ops(&wmt_irq_syscore_ops);
	return 0;
}

device_initcall(wmt_irq_init_devicefs);
#endif  /* CONFIG_PM */

void __init wmt_init_irq(void)
{
	unsigned int i;

	/*temp solution fix me*/
	struct irq_data irq_56,irq_57,irq_58,irq_59,irq_60,irq_61,irq_62,irq_63;

	irq_56.irq = 56;
	irq_57.irq = 57;
	irq_58.irq = 58;
	irq_59.irq = 59;
	irq_60.irq = 60;
	irq_61.irq = 61;
	irq_62.irq = 62;
	irq_63.irq = 63;
	
	request_resource(&iomem_resource, &wmt_irq_resource);

	for (i = 0; i < NR_WMT_IRQS; i++) {
		/*
		 * Disable all IRQs and route them to irq.
		 */
		if (wmt_init_irqcfgs[i].irq < 64)
			ICDC0_VAL(wmt_init_irqcfgs[i].irq) = wmt_init_irqcfgs[i].icdc;
		else
			ICDC1_VAL(wmt_init_irqcfgs[i].irq-64) = wmt_init_irqcfgs[i].icdc;

		/*
		 * Install irqchip for others
		 */
		irq_set_chip(wmt_init_irqcfgs[i].irq, &wmt_normal_chip);

		/*
		 * Default we choose level triggled policy
		 */
		irq_set_handler(wmt_init_irqcfgs[i].irq, handle_level_irq);
		set_irq_flags(wmt_init_irqcfgs[i].irq, IRQF_VALID);
	}

	/*
	 * Enable irq56~irq63 for IC1
	 */
	wmt_unmask_irq(&irq_56);
	wmt_unmask_irq(&irq_57);
	wmt_unmask_irq(&irq_58);
	wmt_unmask_irq(&irq_59);
	wmt_unmask_irq(&irq_60);
	wmt_unmask_irq(&irq_61);
	wmt_unmask_irq(&irq_62);
	wmt_unmask_irq(&irq_63);
}
