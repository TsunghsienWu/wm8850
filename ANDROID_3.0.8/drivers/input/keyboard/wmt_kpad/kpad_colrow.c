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

//#include <linux/module.h>
#include <linux/init.h> 
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/errno.h>
#include <asm/mach-types.h>
#include <mach/hardware.h>
#include <mach/kpad.h>
#include <linux/suspend.h>

#include "kpad_colrow.h"
#include "wmt_keypadall.h"

/////////////////////////////////////////////////////
#define WMT_KPAD_ROW0_INDEX 0
#define WMT_KPAD_ROW1_INDEX 1
#define WMT_KPAD_ROW2_INDEX 2
#define WMT_KPAD_ROW3_INDEX 3
#define WMT_KPAD_ROW4_INDEX 4

#define WMT_KPAD_MAXROW_INDEX 4

#define WMT_KPAD_COL0_INDEX 5
#define WMT_KPAD_COL1_INDEX 6
#define WMT_KPAD_COL2_INDEX 7
#define WMT_KPAD_COL3_INDEX 8


#define WMT_KPAD_MAX_INDEX  (WMT_KPAD_COL3_INDEX+1)
#define WMT_KPAD_MAX_PINNUM 7
//////////////////////////////////////////////////

#define DIRECT_EN (BIT0 | BIT1 | BIT2 | BIT3)

#define KPAD_RUNNING 0
#define KPAD_SUSPEND  1


#define KEYPAD_PIN_SHARING_SEL_VAL GPIO_PIN_SHARING_SEL_4BYTE_VAL
/* ROW0 ~ ROW3 */
#define KEYPAD_GPIO_CTRL_VAL GPIO_CTRL_GP26_KPAD_BYTE_VAL
#define KEYPAD_GPIO_OC_VAL GPIO_OC_GP26_KPAD_BYTE_VAL
#define KEYPAD_GPIO_ID_VAL GPIO_ID_GP26_KPAD_BYTE_VAL
#define KEYPAD_GPIO_PULL_CTRL_VAL GPIO_PULL_CTRL_GP26_KPAD_BYTE_VAL
#define KEYPAD_GPIO_PULL_EN_VAL GPIO_PULL_EN_GP26_KPAD_BYTE_VAL

#define KPADCOL0_INT_REQ_TYPE_ADDR				(__GPIO_BASE + 0x320 )
#define KPADCOL1_INT_REQ_TYPE_ADDR				(__GPIO_BASE + 0x321 )
#define KPADCOL2_INT_REQ_TYPE_ADDR				(__GPIO_BASE + 0x322 )

/* COL0 ~ COL3 */
#define KPADCOL03_INT_REQ_TYPE_ADDR VOUT20_INT_REQ_TYPE_ADDR

#define KPAD_COLROW_MAXNUM	8


/////////////////////////////////////////////
struct wmt_kpad_colrow_map
{
	int used; //0-->useless;1-->useful
	unsigned int* pkey;
	struct gpio_device gpd;
	struct wmt_kpad_colrow_map* head;
	int index;
};

struct wmt_kpad_colrow_device
{
	struct wmt_kpad_colrow_map crmap[WMT_KPAD_MAX_INDEX]; // for col0,col1,col2,row0,row1,row2
	int irq_type; // falling,raising, zero, high
	int dev_index;
	struct keypadall_device* kpad_dev;
	int wifi_index;
	int hold_index;
	unsigned int gpismask;
	struct wmt_kpad_colrow_map* prw03; //row0~row3  kpad interrut
	struct wmt_kpad_colrow_map* pgpio; //row4 && col  share interrupt with gpio
};

static struct wmt_kpad_s l_kpad = {
	.ref	= 0,
	.res	= NULL,
	.regs   = NULL,
	.irq	= 0,
	.ints   = { 0, 0, 0, 0, 0 },
};

static unsigned int l_colrow_keycode[WMT_KPAD_MAX_INDEX];

static int kpad_pm_state = KPAD_RUNNING;

///////////////////////////////////////////////////////
static struct wmt_kpad_colrow_device l_crdev = {
	.irq_type = WMT_KPADROWCOL_FALLING,
	.dev_index = -1,
	.gpismask = 0,
	.prw03 = NULL,
	.pgpio = NULL,
	.crmap = {
		[WMT_KPAD_ROW0_INDEX] = {.used =0,
			   .pkey = NULL,
			   .head = NULL,
			   .index = WMT_KPAD_ROW0_INDEX,
			   .gpd = {
			   		.isbit = BIT0,
			   		.gpbit = BIT0,
			   		.gpedreg = GPIO_CTRL_GP26_KPAD_BYTE_ADDR,
			   		.gpiosreg = GPIO_OC_GP26_KPAD_BYTE_ADDR,
			   		.gpivreg = GPIO_ID_GP26_KPAD_BYTE_ADDR,
			   		.gppdreg = GPIO_PULL_CTRL_GP26_KPAD_BYTE_ADDR,
			   		.gpepreg = GPIO_PULL_EN_GP26_KPAD_BYTE_ADDR,
			           },
			   }, //row0
		[WMT_KPAD_ROW1_INDEX] = {.used =0,
			   .pkey = NULL,
			   .head = NULL,
			   .index = WMT_KPAD_ROW1_INDEX,
			   .gpd = {
			   		.isbit = BIT1,
			   		.gpbit = BIT1,
			   		.gpedreg = GPIO_CTRL_GP26_KPAD_BYTE_ADDR,
			   		.gpiosreg = GPIO_OC_GP26_KPAD_BYTE_ADDR,
			   		.gpivreg = GPIO_ID_GP26_KPAD_BYTE_ADDR,
			   		.gppdreg = GPIO_PULL_CTRL_GP26_KPAD_BYTE_ADDR,
			   		.gpepreg = GPIO_PULL_EN_GP26_KPAD_BYTE_ADDR,
			           },
			   }, //row1
		[WMT_KPAD_ROW2_INDEX] = {.used =0,
			   .pkey = NULL,
			   .head = NULL,
			   .index = WMT_KPAD_ROW2_INDEX,
			   .gpd = {
			   		.isbit = BIT2,
			   		.gpbit = BIT2,
			   		.gpedreg = GPIO_CTRL_GP26_KPAD_BYTE_ADDR,
			   		.gpiosreg = GPIO_OC_GP26_KPAD_BYTE_ADDR,
			   		.gpivreg = GPIO_ID_GP26_KPAD_BYTE_ADDR,
			   		.gppdreg = GPIO_PULL_CTRL_GP26_KPAD_BYTE_ADDR,
			   		.gpepreg = GPIO_PULL_EN_GP26_KPAD_BYTE_ADDR,
			           },
			   }, //row2
		[WMT_KPAD_ROW3_INDEX] = {.used =0,
			   .pkey = NULL,
			   .head = NULL,
			   .index = WMT_KPAD_ROW3_INDEX,
			   .gpd = {
			   		.isbit = BIT3,
			   		.gpbit = BIT3,
			   		.gpedreg = GPIO_CTRL_GP26_KPAD_BYTE_ADDR,
			   		.gpiosreg = GPIO_OC_GP26_KPAD_BYTE_ADDR,
			   		.gpivreg = GPIO_ID_GP26_KPAD_BYTE_ADDR,
			   		.gppdreg = GPIO_PULL_CTRL_GP26_KPAD_BYTE_ADDR,
			   		.gpepreg = GPIO_PULL_EN_GP26_KPAD_BYTE_ADDR,
			           },
			   }, //row3
		[WMT_KPAD_ROW4_INDEX] = {.used =0,
			   .pkey = NULL,
			   .head = NULL,
			   .index = WMT_KPAD_ROW4_INDEX,
			   .gpd = {
			   		.spreg = GPIO_PIN_SHARING_SEL_4BYTE_ADDR,
			   		.spbit = (BIT6 | BIT24),
			   		.isreg = GPIO4_INT_REQ_STS_ADDR,
			   		.isbit = BIT3,
			   		.icreg = KPADROW4_INT_REQ_TYPE_ADDR,
			   		.itybit = BIT0,
			   		.iedbit = BIT7,
			   		.gpbit = BIT1,
			   		.gpivreg = GPIO_ID_GP24_SD2KPAD_BYTE_ADDR,
			   		.gpedreg = GPIO_CTRL_GP24_SD2KPAD_BYTE_ADDR,
			   		.gpiosreg = GPIO_OC_GP24_SD2KPAD_BYTE_ADDR,
			   		.gpepreg = GPIO_PULL_EN_GP24_SD2KPAD_BYTE_ADDR,
			   		.gppdreg = GPIO_PULL_CTRL_GP24_SD2KPAD_BYTE_ADDR,
			
			   		  },
			   }, //row4
		[WMT_KPAD_COL0_INDEX] = {.used =0,
			   .pkey = NULL,
			   .head = NULL,
			   .index = WMT_KPAD_COL0_INDEX,

			   .gpd = {
			   		.isreg = GPIO4_INT_REQ_STS_ADDR,//GPIO2_INT_REQ_STS_ADDR,
			   		.isbit = BIT0,
			   		.icreg = KPADCOL0_INT_REQ_TYPE_ADDR,
			   		.itybit = BIT0,
			   		.iedbit = BIT7,
			   		.gpbit = BIT4,
			   		.gpivreg = GPIO_ID_GP26_KPAD_BYTE_ADDR,
			   		.gpedreg = GPIO_CTRL_GP26_KPAD_BYTE_ADDR,
			   		.gpiosreg = GPIO_OC_GP26_KPAD_BYTE_ADDR,
			   		.gpepreg = GPIO_PULL_EN_GP26_KPAD_BYTE_ADDR,
			   		.gppdreg = GPIO_PULL_CTRL_GP26_KPAD_BYTE_ADDR,			
			   		  },
			   }, //col0
		[WMT_KPAD_COL1_INDEX] = {.used =0,
			   .pkey = NULL,
			   .head = NULL,
			   .index = WMT_KPAD_COL1_INDEX,

			   .gpd = {
			   		.isreg = GPIO4_INT_REQ_STS_ADDR,//GPIO2_INT_REQ_STS_ADDR,
			   		.isbit = BIT1,
			   		.icreg = KPADCOL1_INT_REQ_TYPE_ADDR,
			   		.itybit = BIT0,
			   		.iedbit = BIT7,
			   		.gpbit = BIT5,
			   		.gpivreg = GPIO_ID_GP26_KPAD_BYTE_ADDR,
			   		.gpovreg = GPIO_OD_GP26_KPAD_BYTE_ADDR,
			   		.gpedreg = GPIO_CTRL_GP26_KPAD_BYTE_ADDR,
			   		.gpiosreg = GPIO_OC_GP26_KPAD_BYTE_ADDR,
			   		.gpepreg = GPIO_PULL_EN_GP26_KPAD_BYTE_ADDR,
			   		.gppdreg = GPIO_PULL_CTRL_GP26_KPAD_BYTE_ADDR,			
			   		  },
			   }, //col 1
		[WMT_KPAD_COL2_INDEX] = {.used =0,
			   .pkey = NULL,
			   .head = NULL,
			   .index = WMT_KPAD_COL2_INDEX,

			   .gpd = {
			   		.isreg = GPIO4_INT_REQ_STS_ADDR,//GPIO2_INT_REQ_STS_ADDR,
			   		.isbit = BIT2,
			   		.icreg = KPADCOL2_INT_REQ_TYPE_ADDR,
			   		.itybit = BIT0,
			   		.iedbit = BIT7,
			   		.gpbit = BIT6,
			   		.gpivreg = GPIO_ID_GP26_KPAD_BYTE_ADDR,
			   		.gpedreg = GPIO_CTRL_GP26_KPAD_BYTE_ADDR,
			   		.gpiosreg = GPIO_OC_GP26_KPAD_BYTE_ADDR,
			   		.gpepreg = GPIO_PULL_EN_GP26_KPAD_BYTE_ADDR,
			   		.gppdreg = GPIO_PULL_CTRL_GP26_KPAD_BYTE_ADDR,			
			   		  },
			   }, //col 2
		/*[WMT_KPAD_COL3_INDEX] = {
			   .used =0,
			   .pkey = NULL,
			   .head = NULL,
			   .index = WMT_KPAD_COL3_INDEX,
			   .gpd = {
			   		.isreg = GPIO2_INT_REQ_STS_ADDR,
			   		.isbit = BIT3,
			   		.icreg = KPADCOL3_INT_REQ_TYPE_ADDR,
			   		.itybit = BIT0,
			   		.iedbit = BIT7,
			   		.gpbit = BIT7,
			   		.gpivreg = GPIO_ID_GP26_KPAD_BYTE_ADDR,
			   		.gpedreg = GPIO_CTRL_GP26_KPAD_BYTE_ADDR,
			   		.gpiosreg = GPIO_OC_GP26_KPAD_BYTE_ADDR,
			   		.gpepreg = GPIO_PULL_EN_GP26_KPAD_BYTE_ADDR,
			   		.gppdreg = GPIO_PULL_CTRL_GP26_KPAD_BYTE_ADDR,			
			   		  },
			   }, //col 3
			   */
	},
};

///////////////////////////////////////////////////////////////
static int kpad_row03_enable_int(void* kpad_dev);
static int kpad_row03_disable_int(void* kpad_dev);
static int kpad_gpio_enable_int(void* kpad_dev);
static int kpad_gpio_disable_int(void* kpad_dev);
static irqreturn_t kpad_colrow_interrupt(int irq, void *dev_id);
static irqreturn_t kpad_gpio_interrupt(int irq, void *dev_id);
static void kpad_dump_regs(void);


///////////////////////////////////////////////////////////////
unsigned int* kpad_colrow_getkeytbl(void)
{
	return l_colrow_keycode;
}

int kpad_colrow_setkey(char pintype, int pinnum, int key, int index)
{
	//int i = 0;
	int devin = -1;
	
	if (pinnum < 0 || (pinnum > WMT_KPAD_MAX_PINNUM))
	{
		errlog("Wrong io num(%d)!!!\n", pinnum);
		return -1;
	}
	switch (pintype)
	{
		case 'c':
			//l_colrow_keycode[index] = key;
			devin = pinnum+WMT_KPAD_COL0_INDEX;
		/*	if (pinnum <=3)
			{//col0-3
				l_crdev.crmap[devin].head = l_crdev.prw03;
				l_crdev.prw03 = l_crdev.crmap+devin;			
			} else {
				//col4..
				l_crdev.crmap[devin].head = l_crdev.pgpio;
				l_crdev.pgpio = l_crdev.crmap+devin;
		//	}	*/
			l_crdev.crmap[devin].head = l_crdev.pgpio;
			l_crdev.pgpio = l_crdev.crmap+devin;
			dbg("c%d_%x\n",pinnum,key);
			break;			
		case 'r':			
			//l_colrow_keycode[index] = key;
			devin = pinnum+WMT_KPAD_ROW0_INDEX;
			if (pinnum <=3)
			{//row0-3
				l_crdev.crmap[devin].head = l_crdev.prw03;
				l_crdev.prw03 = l_crdev.crmap+devin;			
			} else {
				//row4..
				l_crdev.crmap[devin].head = l_crdev.pgpio;
				l_crdev.pgpio = l_crdev.crmap+devin;
			}	
			dbg("r%d_%x\n",pinnum, key);
			break;
		default:
			errlog("Wrong io pin type(%c) !!!\n", pintype);
			return -1;
			break;
	};
	if (devin >= 0)
	{
		l_colrow_keycode[index] = key;
		l_crdev.crmap[devin].used = 1;
		l_crdev.crmap[devin].pkey = (l_colrow_keycode + index);
		if (DISABLE_WIFI_KEY == key)
		{
			dbg("wifi button supported.\n");
			l_crdev.wifi_index = devin;
		} else if (HOLD_INPUT_KEY == key)
		{
			dbg("hold button supported.\n");
			l_crdev.hold_index = devin;
		}
	}
	dbg("devin=%d\n", devin);
	return 0;
}

static int kpad_gpio_hw_init(void)
{
    struct wmt_kpad_colrow_map* phead = NULL;

	// row4 and kpacol gpio
	phead = l_crdev.pgpio;
	while (phead != NULL)
	{
		// disable int
		REG8_VAL(phead->gpd.icreg) &= ~phead->gpd.iedbit;

		// int type
		REG8_VAL(phead->gpd.icreg) |= (phead->gpd.itybit<<1);
		REG8_VAL(phead->gpd.icreg) &= ~(phead->gpd.itybit| (phead->gpd.itybit<<2));

		// gpio enable
		REG8_VAL(phead->gpd.gpedreg) |= phead->gpd.gpbit; //&= ~(phead->gpd.gpbit);
		// as input
		REG8_VAL(phead->gpd.gpiosreg) &= ~(phead->gpd.gpbit);
		// enable/disable pull/down & pull down		
		REG8_VAL(phead->gpd.gppdreg) |= phead->gpd.gpbit;
		REG8_VAL(phead->gpd.gpepreg) |= phead->gpd.gpbit;
		// clear int status
		REG8_VAL(phead->gpd.isreg) = phead->gpd.isbit;
		
		phead = phead->head;
		
	};
	return 0;

}

int kpad_dbg_gpio(void)
{
    struct wmt_kpad_colrow_map* phead = NULL;

	// row4 and kpacol gpio
	phead = l_crdev.pgpio;
	while (phead != NULL)
	{
		if (WMT_KPAD_COL1_INDEX == phead->index)
		{
			dbg("dbg col1 gpio,should be high!\n");
			// disable int
			/*REG8_VAL(phead->gpd.icreg) &= ~phead->gpd.iedbit;

			// int type
			REG8_VAL(phead->gpd.icreg) |= (phead->gpd.itybit<<1);
			REG8_VAL(phead->gpd.icreg) &= ~(phead->gpd.itybit| (phead->gpd.itybit<<2));
			*/

			// gpio enable/disable
			REG8_VAL(phead->gpd.gpedreg) |= (phead->gpd.gpbit);
			// as output
			//while(1)
			{
			REG8_VAL(phead->gpd.gpovreg) |= (phead->gpd.gpbit);
			REG8_VAL(phead->gpd.gpiosreg) |= (phead->gpd.gpbit);
			dbg("gpio enable reg:0x%x(0x%x)\n", phead->gpd.gpedreg, REG8_VAL(phead->gpd.gpedreg)&(phead->gpd.gpbit));
			dbg("gpio out reg:0x%x(0x%x)\n", phead->gpd.gpovreg, REG8_VAL(phead->gpd.gpovreg)&(phead->gpd.gpbit));
			dbg("gpio in/out reg:0x%x(0x%x)\n", phead->gpd.gpiosreg, REG8_VAL(phead->gpd.gpiosreg)&(phead->gpd.gpbit));
			};
			// enable/disable pull/down & pull down		
			REG8_VAL(phead->gpd.gppdreg) |= phead->gpd.gpbit;
			REG8_VAL(phead->gpd.gpepreg) |= phead->gpd.gpbit;
			// clear int status
			//REG8_VAL(phead->gpd.isreg) = phead->gpd.isbit;

			// set as input interrupt
			//REG8_VAL(phead->gpd.gpedreg) &= ~(phead->gpd.gpbit);
			// as input
			REG8_VAL(phead->gpd.gpiosreg) &= ~(phead->gpd.gpbit);
		}
		
		phead = phead->head;
		
	};
	return 0;

}

static int kpad_colrow_hw_init(void)
{
    unsigned int status;
    int i = 0;
    struct wmt_kpad_colrow_map* phead = NULL;
    int dir_en = 0;


	//Set ROW4~7 share pin to KPAD mode
	//for (i=WMT_KPAD_ROW4_INDEX;i<=WMT_KPAD_MAXROW_INDEX;i++)
	{
		//if (l_crdev.crmap[i].used)
		{
			REG32_VAL(l_crdev.crmap[WMT_KPAD_ROW4_INDEX].gpd.spreg) &= ~(l_crdev.crmap[WMT_KPAD_ROW4_INDEX].gpd.spbit);		    
dbg("share pin ok\n");
			//break;
		}
	}
	
	// init every pin gpio
	phead = l_crdev.prw03;
	while (phead != NULL)
	{
		// gpio enable/disable
		REG8_VAL(phead->gpd.gpedreg) &= ~(phead->gpd.gpbit);
		// enable/disable pull/down & pull down		
		REG8_VAL(phead->gpd.gppdreg) |= phead->gpd.gpbit;
		REG8_VAL(phead->gpd.gpepreg) |= phead->gpd.gpbit;
		dir_en |= phead->gpd.gpbit;
		dbg("index_%d:\n",phead->index);
		dbg("gpedreg:0x%04x,gpbit:0x%02x\n",
				phead->gpd.gpedreg-__GPIO_BASE,phead->gpd.gpbit);
		phead = phead->head;		
	};
      
    /*
	 * Turn on keypad clocks.
	 */
    auto_pll_divisor(DEV_KEYPAD, CLK_ENABLE, 0, 0);


	/* Clean keypad matrix input control registers. */
	l_kpad.regs->kpmcr = 0;

	/*
	 * Simply disable keypad direct input function first.
	 * Also clear all direct input enable bits.
	 */
	l_kpad.regs->kpdcr = 0;
    status = l_kpad.regs->kpdsr;

    /*
	 * Simply clean any exist keypad matrix status.
	 */
	status = l_kpad.regs->kpstr;
	l_kpad.regs->kpstr |= status;
	if (l_kpad.regs->kpstr != 0)
		errlog("[kpad] clear status failed!\n");

	/*
	 * Set keypad debounce time to be about 125 ms.
	 */
	l_kpad.regs->kpmir = KPMIR_DI(0x0FFF) | KPMIR_SI(0x01);
    l_kpad.regs->kpdir = KPDIR_DI(0x0FFF);

    /*
	 * Enable keypad direct input with interrupt enabled and
	 * automatic scan on activity.
	 */
	/*Active High*/
    //kpad.regs->kpicr &= ~KPICR_IRIMASK;
    /*Active Low*/
    l_kpad.regs->kpicr |= KPICR_IRIMASK;
    
    /*Ignore Multiple Key press disable*/
	//l_kpad.regs->kpdcr |= KPDCR_EN | KPDCR_IEN | KPDCR_ASA | KPDCR_DEN(DIRECT_EN) ;//|KPDCR_IMK;
	l_kpad.regs->kpdcr |= KPDCR_EN | KPDCR_IEN | KPDCR_ASA | KPDCR_DEN(dir_en) ;//|KPDCR_IMK;

	dbg("row03 init ok! dir_en =0x%x\n", dir_en);

	return 0;
}

static void kpad_dump_regs(void)
{
	// gpio reg
	dbg("KEYPAD_PIN_SHARING_SEL_VAL:0x%08x\n",KEYPAD_PIN_SHARING_SEL_VAL);
	dbg("KEYPAD_GPIO_CTRL_VAL      :0x%02x\n",KEYPAD_GPIO_CTRL_VAL);
	dbg("KEYPAD_GPIO_PULL_CTRL_VAL :0x%08x\n",KEYPAD_GPIO_PULL_CTRL_VAL);
	dbg("KEYPAD_GPIO_PULL_EN_VAL   :0x%08x\n",KEYPAD_GPIO_PULL_EN_VAL);
	// kpa reg
	dbg("kpmcr:0x%08x\n", l_kpad.regs->kpmcr);
	dbg("kpdcr:0x%08x\n", l_kpad.regs->kpdcr);
	dbg("kpicr:0x%08x\n", l_kpad.regs->kpicr);
	dbg("kpstr:0x%08x\n", l_kpad.regs->kpstr);
	dbg("kpmar:0x%08x\n", l_kpad.regs->kpmar);
	dbg("kpdsr:0x%08x\n", l_kpad.regs->kpdsr);
	dbg("kpmmr:0x%08x\n", l_kpad.regs->kpmmr);
	dbg("kprir:0x%08x\n", l_kpad.regs->kprir);
	dbg("kpmr0:0x%08x\n", l_kpad.regs->kpmr0);
	dbg("kpmr1:0x%08x\n", l_kpad.regs->kpmr1);
	dbg("kpmr2:0x%08x\n", l_kpad.regs->kpmr2);
	dbg("kpmr3:0x%08x\n", l_kpad.regs->kpmr3);
	dbg("kpmir:0x%08x\n", l_kpad.regs->kpmir);
	dbg("kpdir:0x%08x\n", l_kpad.regs->kpdir);
}



#ifdef CONFIG_CPU_FREQ
/*
 * Well, the debounce time is not very critical while zac2_clk
 * rescaling, but we still do it.
 */

/* kpad_clock_notifier()
 *
 * When changing the processor core clock frequency, it is necessary
 * to adjust the KPMIR register.
 *
 * Returns: 0 on success, -1 on error
 */
static int kpad_clock_notifier(struct notifier_block *nb, unsigned long event,
	void *data)
{
// sparksun: to avoid compiling error if enable cpufreq
#if 0
	switch (event) {
	case CPUFREQ_PRECHANGE:
		/*
		 * Disable input.
		 */
		kpad.regs->kpmcr &= ~KPMCR_EN;
		break;

	case CPUFREQ_POSTCHANGE:
		/*
		 * Adjust debounce time then enable input.
		 */
		kpad.regs->kpmir = KPMIR_DI((125 * wm8510_ahb_khz()) / \
			(262144)) | KPMIR_SI(0xFF);
		kpad.regs->kpmcr |= KPMCR_EN;
		break;
	}
#endif
	return 0;
}

/*
 * Notify callback while issusing zac2_clk rescale.
 */
static struct notifier_block kpad_clock_nblock = {
	.notifier_call  = kpad_clock_notifier,
	.priority = 1
};
#endif

static int kpad_enable_int(void* kpad_dev)
{
	kpad_gpio_enable_int(kpad_dev);
	kpad_row03_enable_int(kpad_dev);
	return 0;
}

static int kpad_disable_int(void* kpad_dev)
{
	kpad_gpio_disable_int(kpad_dev);
	kpad_row03_disable_int(kpad_dev);
	return 0;
}

static int kpad_colrow_probe(void* kpad_dev)
{
	int ret = 0;
	struct keypadall_device* pdev = (struct keypadall_device*)kpad_dev;
	
	// init hardware
	kpad_gpio_disable_int(kpad_dev);
	kpad_gpio_hw_init();
	kpad_colrow_hw_init();
	kpad_row03_disable_int(kpad_dev);
	
	// register int
	ret = request_irq(IRQ_KPAD, kpad_colrow_interrupt, IRQF_DISABLED, "row03keypad", kpad_dev);

	if (ret) {
		errlog("Can't allocate irq %d\n", IRQ_KPAD);
		ret = -1;
		goto err_reirq_rowkpad;
	}

	if (l_crdev.gpismask != 0)
	{
		ret = request_irq(IRQ_GPIO, kpad_gpio_interrupt, IRQF_SHARED, "gpiokpad", kpad_dev);
		if (ret) {
			errlog("Can't allocate irq %d ret = %d\n", IRQ_GPIO,ret);	
			ret = -1;
			goto err_reirq_gpkpad;
		}
		//kpad_gpio_enable_int(kpad_dev);
		//kpad_row03_enable_int(kpad_dev);
	}
	
/*#ifdef CONFIG_CPU_FREQ
	ret = cpufreq_register_notifier(&kpad_clock_nblock, \
		CPUFREQ_TRANSITION_NOTIFIER);

	if (ret) {
		errlog( "Unable to register CPU frequency " \
			"change notifier (%d)\n", ret);
	}
#endif
*/
	//dbg("ok\n");
	dbg("load init:\n");
	kpad_dump_regs();
	return 0;
err_reirq_gpkpad:
	free_irq(IRQ_KPAD, kpad_dev);
err_reirq_rowkpad:
	return ret;
	
	return 0;
}
/*
static int kpad_gp_enable_int(void* kpad_dev)
{
	kpad_row03_enable_int(kpad_dev);
	kpad_gpio_enable_int(kpad_dev);
	return 0;
}
*/
static int kpad_colrow_remove(void* kpad_dev)
{
	// nothing
	dbg("....\n");
	l_kpad.regs->kpmcr = 0;
	// free irq
	free_irq(IRQ_KPAD, kpad_dev);
	if (l_crdev.gpismask)
	{
		free_irq(IRQ_GPIO, kpad_dev);
	}
	/*Disable clock*/
    auto_pll_divisor(DEV_KEYPAD, CLK_DISABLE, 0, 0);
	return 0;
}

static int kpad_row03_enable_int(void* kpad_dev)
{
	// enable row0-3 int
	l_kpad.regs->kpdcr |= (KPDCR_EN | KPDCR_IEN) ;
	//enable_irq(l_kpad.irq);
    return 0;
}

static int kpad_gpio_enable_int(void* kpad_dev)
{
	struct wmt_kpad_colrow_map* phead = NULL;

	    // enable row4 & col0-3 & gpio
    phead = l_crdev.pgpio;
    while (phead!=NULL)
    {
    dbg("enbale gpio %d int\n", phead->index);
    	REG8_VAL(phead->gpd.icreg) |= phead->gpd.iedbit;
    	phead = phead->head;
    };
    return 0;
}

static int kpad_row03_disable_int(void* kpad_dev)
{
	// disable row0-3
	//disable_irq_nosync(l_kpad.irq);
	l_kpad.regs->kpdcr &= ~(KPDCR_EN | KPDCR_IEN);	
    
	return 0;
}

static int kpad_gpio_disable_int(void* kpad_dev)
{
	struct wmt_kpad_colrow_map* phead = NULL;

	// disable row4 & col0-3 & gpio
	phead = l_crdev.pgpio;
    while (phead!=NULL)
    {
    	dbg("enter...\n");
    	REG8_VAL(phead->gpd.icreg) &= ~phead->gpd.iedbit;
    	phead = phead->head;
    	dbg("end...\n");
    };
    return 0;
}
static int kpad_colrow_suspend(void* kpad_dev, pm_message_t state)
{
	kpad_pm_state = KPAD_SUSPEND;

/*	switch (state.event)
	{
		case PM_EVENT_SUSPEND:             
		//Disable clock
        	auto_pll_divisor(DEV_KEYPAD, CLK_DISABLE, 0, 0);
// disble irq
			kpad_gpio_disable_int(kpad_dev);
			break;
	case PM_EVENT_FREEZE:
	case PM_EVENT_PRETHAW:
        
	default:
		break;

	};
*/
	//Disable clock
	//auto_pll_divisor(DEV_KEYPAD, CLK_DISABLE, 0, 0);
// disble irq
	kpad_gpio_disable_int(kpad_dev);
	kpad_row03_disable_int(kpad_dev);



    dbg("suspend ok...\n");
	return 0;
}


#define KEYPAD_GPIO_MASK (BIT0|BIT1|BIT2|BIT3)

static int kpad_colrow_resume(void* kpad_dev)
{
	dbg("enter...\n");
	kpad_disable_int(kpad_dev);
	kpad_colrow_hw_init();
	kpad_gpio_disable_int(kpad_dev);
	kpad_gpio_hw_init();
	//kpad_gpio_enable_int(kpad_dev);
	//REG32_VAL(__GPIO_BASE+0x430) |= (BIT0|BIT1);
	//enable_irq(l_kpad.irq);
	//KEYPAD_GPIO_CTRL_VAL &= ~KEYPAD_GPIO_MASK; 
	kpad_pm_state = KPAD_RUNNING;
	dbg("resume init:\n");
	kpad_dump_regs();
	return 0;
}

static int kpad_gpio_doup(void)
{
	int i = l_crdev.dev_index;
	unsigned int keycode = 0;
	
	if (( i>= 0) && (l_crdev.crmap[i].used != 0))
	{
		if (kpadall_islongkey(*l_crdev.crmap[i].pkey))
		{
			l_crdev.kpad_dev->longpresscnt++;
		}
		// whether the key up
		if (REG8_VAL(l_crdev.crmap[i].gpd.gpivreg)&l_crdev.crmap[i].gpd.gpbit) {
			/*if ((HOLD_INPUT_KEY != *l_crdev.crmap[i].pkey) &&
				(DISABLE_WIFI_KEY!= *l_crdev.crmap[i].pkey))*/
			{
				if (kpadall_islongkey(*l_crdev.crmap[i].pkey))
				{
					if (l_crdev.kpad_dev->longpresscnt < WMT_KPAD_LONG_PRESS_COUNT)
					{
						// report the normal key
						dbg("report the normal key,not long key:%d\n", *l_crdev.crmap[i].pkey);
						kpadall_report_key(*l_crdev.crmap[i].pkey, 1);
						kpadall_report_key(*l_crdev.crmap[i].pkey, 0);
					} else if (WMT_KPAD_LONG_PRESS_COUNT == l_crdev.kpad_dev->longpresscnt)
					{
						keycode = kpadall_normalkey_2_longkey(*l_crdev.crmap[i].pkey);
						kpadall_report_key(keycode, 1);
						kpadall_report_key(keycode, 0);
						dbg("report long key:%d\n", keycode);
					}
					l_crdev.kpad_dev->longpresscnt = 0;					
				} else {
					dbg("report key:%d(0x%x)-->index:%d  up***\n", *l_crdev.crmap[i].pkey,
									*l_crdev.crmap[i].pkey, i);
					kpadall_report_key(*l_crdev.crmap[i].pkey, 0);
				}
				kpadall_set_norkeydown(0);
				// enable irq
				//kpad_gpio_enable_int(&l_crdev);
			}
			
			/*spin_lock(&l_crdev.kpad_dev->slock);
			l_crdev.kpad_dev->down = 0;
			spin_unlock(&l_crdev.kpad_dev->slock);
			*/
		} else {
			if (kpadall_islongkey(*l_crdev.crmap[i].pkey))
			{
				if (WMT_KPAD_LONG_PRESS_COUNT == l_crdev.kpad_dev->longpresscnt)
				{
					keycode = kpadall_normalkey_2_longkey(*l_crdev.crmap[i].pkey);
					kpadall_report_key(keycode, 1);
					kpadall_report_key(keycode, 0);
					kpadall_set_norkeydown(0);
					dbg("report long key:%d\n", keycode);
				}
			}
			l_crdev.kpad_dev->handle_up();
		}
	}
	return 0;
}

static int kpad_row03_doup(void)
{
	int i = l_crdev.dev_index;
	unsigned int keycode = 0;

	dbg("begin...(i=%d)\n",i);
	if (( i>= 0) && (l_crdev.crmap[i].used != 0))
	{
		
		if (kpadall_islongkey(*l_crdev.crmap[i].pkey))
		{
			l_crdev.kpad_dev->longpresscnt++;
		}
		// whether the key up
		dbg("To check pen up\n");
		if (REG8_VAL(l_crdev.crmap[i].gpd.gpivreg)&l_crdev.crmap[i].gpd.gpbit) {
			if ((HOLD_INPUT_KEY != *l_crdev.crmap[i].pkey) &&
				(DISABLE_WIFI_KEY!= *l_crdev.crmap[i].pkey))
			{
				if (kpadall_islongkey(*l_crdev.crmap[i].pkey))
				{
					if (l_crdev.kpad_dev->longpresscnt < WMT_KPAD_LONG_PRESS_COUNT)
					{
						// report the normal key
						dbg("report the normal key,not long key:%d\n", *l_crdev.crmap[i].pkey);
						kpadall_report_key(*l_crdev.crmap[i].pkey, 1);
						kpadall_report_key(*l_crdev.crmap[i].pkey, 0);
					} else if (WMT_KPAD_LONG_PRESS_COUNT == l_crdev.kpad_dev->longpresscnt)
					{
						keycode = kpadall_normalkey_2_longkey(*l_crdev.crmap[i].pkey);
						kpadall_report_key(keycode, 1);
						kpadall_report_key(keycode, 0);
						dbg("report long key:%d\n", keycode);
					}
					l_crdev.kpad_dev->longpresscnt = 0;					
				} else {
					dbg("report key:%d(0x%x)-->index:%d  up***\n", *l_crdev.crmap[i].pkey,
									*l_crdev.crmap[i].pkey, i);
					kpadall_report_key(*l_crdev.crmap[i].pkey, 0);
				}
				kpadall_set_norkeydown(0);
			}
			// set gpio as interrut
			REG8_VAL(l_crdev.crmap[i].gpd.gpedreg) &= ~l_crdev.crmap[i].gpd.gpbit;
			l_kpad.regs->kpdcr |= KPDCR_DEN(l_crdev.crmap[i].gpd.isbit);
			kpad_row03_enable_int(l_crdev.kpad_dev);
			//enable_irq(l_kpad.irq);
			/*spin_lock(&l_crdev.kpad_dev->slock);
			l_crdev.kpad_dev->down = 0;
			spin_unlock(&l_crdev.kpad_dev->slock);
			*/
		} else {
			if (kpadall_islongkey(*l_crdev.crmap[i].pkey))
			{
				if (WMT_KPAD_LONG_PRESS_COUNT == l_crdev.kpad_dev->longpresscnt)
				{
					keycode = kpadall_normalkey_2_longkey(*l_crdev.crmap[i].pkey);
					kpadall_report_key(keycode, 1);
					kpadall_report_key(keycode, 0);
					kpadall_set_norkeydown(0);
					dbg("report long key:%d\n", keycode);
				}
			}
			dbg("wait to next checking\n");
			kpadall_get_current_dev()->handle_up();
		}
	}
	return 0;
}

static irqreturn_t
kpad_colrow_interrupt(int irq, void *dev_id)
{
	unsigned int status;
    unsigned int rvalue;
	int i = 0;
	struct keypadall_device* kpad_dev =  (struct keypadall_device*)dev_id;
	struct wmt_kpad_colrow_map* phead = NULL;
	
	
	dbg("enter...\n");
	// disble irq
	disable_irq_nosync(l_kpad.irq);
	kpad_row03_disable_int(kpad_dev);	
	// Get keypad interrupt status and clean interrput source.
	status = l_kpad.regs->kpstr;
	l_kpad.regs->kpstr |= status;

	if (l_kpad.regs->kpstr != 0)
		errlog("[kpad] status clear failed! \n");

	// Fixed:received unexpected kpad  interrupt,when resume
	if (kpad_pm_state != KPAD_RUNNING)
	{
		dbg("Shouldn't be here!\n");
		//return IRQ_HANDLED;
		goto end_rowinterrupt;
	}
	if (status & KPSTR_DIA) 
	{
        rvalue = l_kpad.regs->kpdsr;
		// different the gpio pin && report the key		
		//for (i = 0; i <= WMT_KPAD_MAX_INDEX; i++)
		if (rvalue & Dir_Vaild_Scan) 
		{
			phead = l_crdev.prw03;
			while (phead != NULL)
			{
				if ((rvalue & phead->gpd.isbit) == phead->gpd.isbit)
				{
					dbg("ocurr kpad int,index=%d, rvalue = 0x%x\n",phead->index, rvalue);					
					// report the key
					/*spin_lock(&kpad_dev->slock);
					spin_unlock(&kpad_dev->slock);
					*/
					if (!kpadall_if_hand_key())
					{
						dbg("can't handle the kpad int\n");
						kpad_row03_enable_int(kpad_dev);
						break; // now can't handle the down key
					}
					// disable row and set as input gpio
					l_kpad.regs->kpdcr &= ~KPDCR_DEN(phead->gpd.isbit);
					REG8_VAL(phead->gpd.gpedreg) |= phead->gpd.gpbit; 
					REG8_VAL(phead->gpd.gpiosreg) &= ~phead->gpd.gpbit;
					i = phead->index;
					kpadall_set_current_dev(kpad_dev);
					if ((HOLD_INPUT_KEY != *l_crdev.crmap[i].pkey) &&
					   (DISABLE_WIFI_KEY!= *l_crdev.crmap[i].pkey))
					{
						if (kpadall_islongkey(*l_crdev.crmap[i].pkey))
						{
							l_crdev.kpad_dev->longpresscnt = 0;
						} else {
							dbg("report key:%d(0x%x)-->index:%d  down***\n", *l_crdev.crmap[i].pkey,
									*l_crdev.crmap[i].pkey, i);
							kpadall_report_key(*l_crdev.crmap[i].pkey, 1);							
						}
						kpadall_set_norkeydown(1);
					} else {
						// for hold key or wifi disable key
						dbg("report hold/wifi disable key   ***\n");
						kpadall_report_key(*l_crdev.crmap[i].pkey, 1);
						kpadall_report_key(*l_crdev.crmap[i].pkey, 0);
						if (HOLD_INPUT_KEY == *l_crdev.crmap[i].pkey)
						{
							kpadall_set_holddown(1);
						} else if (DISABLE_WIFI_KEY == *l_crdev.crmap[i].pkey)
						{
							kpadall_set_wifibtndown(1);
							//l_crdev.wifi_index = i;
						}
					}
					l_crdev.dev_index = i;
					dbg("l_crdev.dev_index=%d\n", l_crdev.dev_index);
					if (DISABLE_WIFI_KEY == *l_crdev.crmap[i].pkey)
					{
						kpadall_handle_wifibtn();						
					} else if (HOLD_INPUT_KEY == *l_crdev.crmap[i].pkey)
					{
						kpadall_handle_hold();
					} else {
						kpad_dev->do_up = kpad_row03_doup;
						dbg("to handle up\n");
						kpad_dev->handle_up();	
						
					}
					break;
					
				}
				phead = phead->head;
			};
		}
	} else {
		errlog("kpad status error,KPSTR=0x%.8x\n", status);
	}
end_rowinterrupt:
	// enable irq
	//kpad_row03_enable_int(kpad_dev);
	enable_irq(l_kpad.irq);
	dbg("exit...\n");
	return IRQ_HANDLED;
}

static irqreturn_t
kpad_gpio_interrupt(int irq, void *dev_id)
{
	int i = 0;
	struct keypadall_device* kpad_dev =  (struct keypadall_device*)dev_id;
	struct wmt_kpad_colrow_map* phead = NULL;
//	unsigned int isreg = l_crdev.crmap[WMT_KPAD_ROW4_INDEX].gpd.isreg;
	int status = 0;

	phead = l_crdev.pgpio;

	while(phead != NULL)
	{
		status = REG32_VAL(phead->gpd.isreg);
		dbg("gpio int 0x%x, index = %d, isreg = 0x%x\n", status, phead->index, phead->gpd.isreg);
		if(status & phead->gpd.isbit)
		{
			dbg("find keypad int!\n");
			break;
		}
		phead = phead->head;
	}

	//whether it's kpad gpio interrupt
	if (phead == NULL)
	{
		// no kpad gpio int
		dbg("no gpio kapd int\n");
		return IRQ_NONE;
	}
	// disble irq
	kpad_gpio_disable_int(kpad_dev);	

	// Fixed:received unexpected kpad  interrupt,when resume
	if (kpad_pm_state != KPAD_RUNNING)
	{
		dbg("Shouldn't be here!\n");
		//kpad_gpio_enable_int(kpad_dev);
		return IRQ_HANDLED;
	}
	// whose interrupt
	dbg("ocurr kpad int\n");					
	// report the key
	/*spin_lock(&kpad_dev->slock);
	spin_unlock(&kpad_dev->slock);
	*/
	// clear int status
	REG8_VAL(phead->gpd.isreg) = phead->gpd.isbit;
	if (!kpadall_if_hand_key())
	{
		kpad_gpio_enable_int(kpad_dev);
		dbg("Can't handl the key interrut for other is down!\n");
		return IRQ_HANDLED;
	}			
	i = phead->index;
	kpadall_set_current_dev(kpad_dev);
	if ((HOLD_INPUT_KEY != *l_crdev.crmap[i].pkey) && // home and enable wifi
	   (DISABLE_WIFI_KEY!= *l_crdev.crmap[i].pkey))
	{
		if (kpadall_islongkey(*l_crdev.crmap[i].pkey))
		{
			l_crdev.kpad_dev->longpresscnt = 0;
		} else {
			dbg("report key:%d(0x%x)-->index:%d  down***\n", *l_crdev.crmap[i].pkey,
					*l_crdev.crmap[i].pkey, i);
			kpadall_report_key(*l_crdev.crmap[i].pkey, 1);							
		}
		kpadall_set_norkeydown(1);
	} else {
		// for hold key or wifi disable key
		dbg("report hold/wifi key %x  ***\n", *l_crdev.crmap[i].pkey);
		kpadall_report_key(*l_crdev.crmap[i].pkey, 1);
		kpadall_report_key(*l_crdev.crmap[i].pkey, 0);
		if (HOLD_INPUT_KEY == *l_crdev.crmap[i].pkey)
		{
			kpadall_set_holddown(1);
		} else if (DISABLE_WIFI_KEY == *l_crdev.crmap[i].pkey)
		{
			kpadall_set_wifibtndown(1);
		}
	}
	l_crdev.dev_index = i;
	kpadall_set_current_dev(kpad_dev);
	if (DISABLE_WIFI_KEY == *l_crdev.crmap[i].pkey)
	{
		kpadall_handle_wifibtn();						
	} else if (HOLD_INPUT_KEY == *l_crdev.crmap[i].pkey)
	{
		kpadall_handle_hold();
	} else {
		kpad_dev->do_up = kpad_gpio_doup;
		kpad_dev->handle_up();
		
	}
	kpad_gpio_enable_int(kpad_dev);

//	status = REG32_VAL(phead->gpd.isreg);
//	dbg("gpio int 0x%x, index = %d\n", status, phead->index);
	return IRQ_HANDLED;
}

//return:1--wifibtn up,0--it is still down.
static int colrow_is_wifibtn_up(void)
{
	int i = l_crdev.wifi_index;
	
	if(REG8_VAL(l_crdev.crmap[i].gpd.gpivreg)&l_crdev.crmap[i].gpd.gpbit)
	{
		return 1;
	}
	return 0;
}

//return:1--hold key up,0--it is still down.
static int colrow_is_hold_up(void)
{
	int i = l_crdev.hold_index;

	//dbg("hold_index=%d\n", i);
	//dbg("gpio invalreg:0x%x(%x,bit:%x)\n",l_crdev.crmap[i].gpd.gpivreg,
		//REG8_VAL(l_crdev.crmap[i].gpd.gpivreg),
		//l_crdev.crmap[i].gpd.gpbit);
	if(REG8_VAL(l_crdev.crmap[i].gpd.gpivreg)&l_crdev.crmap[i].gpd.gpbit)
	{
		return 1;
	}
	return 0;
}

static int colrow_hold_up_end(void)
{
	int i = l_crdev.hold_index;

	if (i <= WMT_KPAD_ROW3_INDEX)
	{
		// set gpio as interrut
		REG8_VAL(l_crdev.crmap[i].gpd.gpedreg) &= ~l_crdev.crmap[i].gpd.gpbit;
		l_kpad.regs->kpdcr |= KPDCR_DEN(l_crdev.crmap[i].gpd.isbit);
		kpad_row03_enable_int(l_crdev.kpad_dev);		
	}
	return 0;
}

int kpad_colrow_register(struct keypadall_device* kpad_dev)
{
	struct wmt_kpad_colrow_map* phead = NULL;

	// probe
	kpad_dev->probe = kpad_colrow_probe;
	// remove
	kpad_dev->remove = kpad_colrow_remove;
	// enable all int
	kpad_dev->enable_int = kpad_enable_int;
	// disable int
	//kpad_dev->disable_int = kpad_row03_disable_int;
	// suspend
	kpad_dev->suspend = kpad_colrow_suspend;
	// resume
	kpad_dev->resume = kpad_colrow_resume;
	// interrupt function
	/*kpad_dev->inthandler[0] = kpad_colrow_interrupt;
	kpad_dev->irq[0] = IRQ_KPAD;	
	kpad_dev->irq_register_type[0] = IRQF_DISABLED;
	kpad_dev->inthandler[1] = kpad_gpio_interrupt;
	kpad_dev->irq[1] = IRQ_GPIO;	
	kpad_dev->irq_register_type[1] = IRQF_SHARED;
	*/
	// do up
	//kpad_dev->do_rowup = kpad_row03_doup;
	//kpad_dev->init_up = kpadall_init_up;
	kpad_dev->handle_up = kpadall_handle_up;
	//kpad_dev->do_gpup = kpad_gpio_doup;
	//kpad_dev->remove_up = kpadall_remove_up;
	kpad_dev->up_timeout = WMT_KPAD_UP_TIME; // 200
	if (kpadall_support_wifibtn())
	{
		kpad_dev->wifibtn_is_up = colrow_is_wifibtn_up;
		kpad_dev->wifi_timeout = WMT_KPAD_WIFI_TIME;
	}
	if (kpadall_support_hold())
	{
		//kpad_dev->report_hold_sta = report_hold_stat;
		kpad_dev->hold_is_up = colrow_is_hold_up;
		kpad_dev->hold_timeout = WMT_KPAD_UP_TIME;
		kpad_dev->hold_up_end = colrow_hold_up_end;
	}
	/*if (kpad_dev->hold_support != 0)
	{
		kpadall_set_current_dev(kpad_dev);
	}*/	
	l_crdev.kpad_dev = kpad_dev;
			
	l_kpad.irq = IRQ_KPAD;
	l_kpad.regs = (struct kpad_regs_s *)KPAD_BASE_ADDR;

	// the kpad gpio int status mask
	phead = l_crdev.pgpio;
	l_crdev.gpismask = 0;
	while (phead != NULL)
	{
		dbg("gpd.isbit = %d, phead->index = %d\n", phead->gpd.isbit, phead->index);
		l_crdev.gpismask |= phead->gpd.isbit;
		phead = phead->head;
	};
	dbg("l_crdev.gpismask=%x\n", l_crdev.gpismask);

	return 0;
}

int early_suspend()
{
	struct wmt_kpad_colrow_map* phead = NULL;

	//Disable menu and back of row
	dbg(" early keypad to suspend.....\n");
	phead = l_crdev.prw03;
	while(phead != NULL)
	{
		if(*phead->pkey == 0x9e || *phead->pkey == 0x8b || *phead->pkey == 0x66) {
			l_kpad.regs->kpdcr &= ~((1<<phead->index)<<16);
		}
		phead = phead->head;
	}

	//Disable menu and back of row4 & col0-3 & gpio
	phead = l_crdev.pgpio;
    while (phead!=NULL)
    {
		if(*phead->pkey == 0x9e || *phead->pkey == 0x8b || *phead->pkey == 0x66) {
			REG8_VAL(phead->gpd.icreg) &= ~phead->gpd.iedbit;
		}
		phead = phead->head;
    };
	return 0;
}

int late_resume()
{
	struct wmt_kpad_colrow_map* phead = NULL;
	int i = 0;

	//Disable menu and back of row
	dbg("early keypad to resume......\n");
	phead = l_crdev.prw03;
	while(phead != NULL)
	{
		if(*phead->pkey == 0x9e || *phead->pkey == 0x8b || *phead->pkey == 0x66) {
			l_kpad.regs->kpdcr |= ((1 <<phead->index)<<16);
		}
		phead = phead->head;
	}

	//Disable menu and back of row4 & col0-3 & gpio
	phead = l_crdev.pgpio;
    while (phead!=NULL)
    {
		if(*phead->pkey == 0x9e || *phead->pkey == 0x8b || *phead->pkey == 0x66) {
			REG8_VAL(phead->gpd.icreg) |= phead->gpd.iedbit;
		}
		phead = phead->head;
    };
	return 0;
}


