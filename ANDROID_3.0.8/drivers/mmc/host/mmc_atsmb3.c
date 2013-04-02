/*++
linux/drivers/mmc/host/mmc_atsmb.c

Copyright (c) 2008  WonderMedia Technologies, Inc.

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
//#define DEBUG

//#include <linux/config.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/blkdev.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sdio.h>
#include <linux/completion.h>
#include <linux/pagemap.h>
#include <linux/dma-mapping.h>
#include <asm/dma.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/memory.h>
#include <mach/hardware.h>
#include <asm/scatterlist.h>
#include <asm/sizes.h>
#include "mmc_atsmb.h"
//#include <mach/multicard.h>
#include <mach/irqs.h>

int debug_enable = 0x00;

#if 1
#define DBG(host, fmt, args...)	\
	pr_debug("%s: %s: " fmt, mmc_hostname(host->mmc), __func__ , args)
#endif
#define ATSMB_TIMEOUT_TIME (HZ*2)

//add by jay,for modules support
static u64 wmt_sdmmc3_dma_mask = 0xffffffffUL;
static struct resource wmt_sdmmc3_resources[] = {
	[0] = {
		.start	= SD3_SDIO_MMC_BASE_ADDR,
		.end	= (SD3_SDIO_MMC_BASE_ADDR + 0x3FF),
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_SDC3,
		.end	= IRQ_SDC3,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start  = IRQ_SDC3_DMA,
		.end    = IRQ_SDC3_DMA,
		.flags  = IORESOURCE_IRQ,
	},
	/*2008/10/6 RichardHsu-s*/
	 [3] = {
	     .start    = IRQ_PMC_WAKEUP,
	     .end      = IRQ_PMC_WAKEUP,
	     .flags    = IORESOURCE_IRQ,
	 },
	 /*2008/10/6 RichardHsu-e*/
};

struct kobject *atsmb3_kobj;
struct mmc_host *mmc3_host_attr  = NULL;
//
static void atsmb3_release(struct device * dev) {}

/*#ifdef CONFIG_MMC_DEBUG*/
#define MORE_INFO
#if 0
#define DBG(x...)	printk(KERN_ALERT x)
#define DBGR(x...)	printk(KERN_ALERT x)
#else
#define DBG(x...)	do { } while (0)
#define DBGR(x...)	do { } while (0)
#endif

#if 1
void atsmb3_dump_reg(struct atsmb_host *host)
{   
    u8 CTL,CMD_IDX,RSP_TYPE,BUS_MODE,INT_MASK_0,INT_MASK_1,SD_STS_0,SD_STS_1,SD_STS_2,SD_STS_3;
    u8 RSP_0,RSP_1,RSP_2, RSP_3, RSP_4, RSP_5, RSP_6, RSP_7;
    u8 RSP_8,RSP_9,RSP_10,RSP_11,RSP_12,RSP_13,RSP_14,RSP_15;
    u8 RSP_TOUT,CLK_SEL,EXT_CTL;
    u16 BLK_LEN,BLK_CNT,SHDW_BLKLEN,TIMER_VAL;    
    u32 CMD_ARG,PDMA_GCR,PDMA_IER,PDMA_ISR,PDMA_DESPR,PDMA_RBR,PDMA_DAR,PDMA_BAR,PDMA_CPR,PDMA_CCR;
    u32 CURBLK_CNT;
    
    if(!debug_enable)
    		return;
    
    CTL = *ATSMB3_CTL;
    CMD_IDX = *ATSMB3_CMD_IDX;
    RSP_TYPE = *ATSMB3_RSP_TYPE;
    CMD_ARG = *ATSMB3_CMD_ARG;
    BUS_MODE = *ATSMB3_BUS_MODE;
    BLK_LEN = *ATSMB3_BLK_LEN;
    BLK_CNT = *ATSMB3_BLK_CNT;
    RSP_0 = *ATSMB3_RSP_0;
    RSP_1 = *ATSMB3_RSP_1;
    RSP_2 = *ATSMB3_RSP_2;
    RSP_3 = *ATSMB3_RSP_3;
    RSP_4 = *ATSMB3_RSP_4;
    RSP_5 = *ATSMB3_RSP_5;
    RSP_6 = *ATSMB3_RSP_6;
    RSP_7 = *ATSMB3_RSP_7;
    RSP_8 = *ATSMB3_RSP_8;
    RSP_9 = *ATSMB3_RSP_9;
    RSP_10 = *ATSMB3_RSP_10;
    RSP_11 = *ATSMB3_RSP_11;
    RSP_12 = *ATSMB3_RSP_12;
    RSP_13 = *ATSMB3_RSP_13;
    RSP_14 = *ATSMB3_RSP_14;
    RSP_15 = *ATSMB3_RSP_15;
    CURBLK_CNT = *ATSMB3_CURBLK_CNT;
    INT_MASK_0 = *ATSMB3_INT_MASK_0;
    INT_MASK_1 = *ATSMB3_INT_MASK_1;
    SD_STS_0 = *ATSMB3_SD_STS_0;
    SD_STS_1 = *ATSMB3_SD_STS_1;
    SD_STS_2 = *ATSMB3_SD_STS_2;
    SD_STS_3 = *ATSMB3_SD_STS_3;
    RSP_TOUT = *ATSMB3_RSP_TOUT;
    CLK_SEL = *ATSMB3_CLK_SEL;
    EXT_CTL = *ATSMB3_EXT_CTL;
    SHDW_BLKLEN = *ATSMB3_SHDW_BLKLEN;
    TIMER_VAL = *ATSMB3_TIMER_VAL;

    PDMA_GCR = *ATSMB3_PDMA_GCR;
    PDMA_IER = *ATSMB3_PDMA_IER;
    PDMA_ISR = *ATSMB3_PDMA_ISR;
    PDMA_DESPR = *ATSMB3_PDMA_DESPR;
    PDMA_RBR = *ATSMB3_PDMA_RBR;
    PDMA_DAR = *ATSMB3_PDMA_DAR;
    PDMA_BAR = *ATSMB3_PDMA_BAR;
    PDMA_CPR = *ATSMB3_PDMA_CPR;
    PDMA_CCR = *ATSMB3_PDMA_CCR;

    printk("\n+---------------------------Registers----------------------------+\n");

	printk("%16s = 0x%8x  |", "CTL", CTL);
	printk("%16s = 0x%8x\n", "CMD_IDX", CMD_IDX);

	printk("%16s = 0x%8x  |", "RSP_TYPE", RSP_TYPE);
	printk("%16s = 0x%8x\n", "CMD_ARG", CMD_ARG);

	printk("%16s = 0x%8x  |", "BUS_MODE", BUS_MODE);
	printk("%16s = 0x%8x\n", "BLK_LEN", BLK_LEN);

	printk("%16s = 0x%8x  |", "BLK_CNT", BLK_CNT);
	printk("%16s = 0x%8x\n", "RSP_0", RSP_0);

	printk("%16s = 0x%8x  |", "RSP_1", RSP_1);
	printk("%16s = 0x%8x\n", "RSP_2", RSP_2);

	printk("%16s = 0x%8x  |", "RSP_3", RSP_3);
	printk("%16s = 0x%8x\n", "RSP_4", RSP_4);

	printk("%16s = 0x%8x  |", "RSP_5", RSP_5);
	printk("%16s = 0x%8x\n", "RSP_6", RSP_6);

	printk("%16s = 0x%8x  |", "RSP_7", RSP_7);
	printk("%16s = 0x%8x\n", "RSP_8", RSP_8);

	printk("%16s = 0x%8x  |", "RSP_9", RSP_9);
	printk("%16s = 0x%8x\n", "RSP_10", RSP_10);

	printk("%16s = 0x%8x  |", "RSP_11", RSP_11);
	printk("%16s = 0x%8x\n", "RSP_12", RSP_12);

	printk("%16s = 0x%8x  |", "RSP_13", RSP_13);
	printk("%16s = 0x%8x\n", "RSP_14", RSP_14);

    printk("%16s = 0x%8x\n", "RSP_15", RSP_15);

	printk("%16s = 0x%8x  |", "CURBLK_CNT", CURBLK_CNT);
	printk("%16s = 0x%8x\n", "INT_MASK_0", INT_MASK_0);

	printk("%16s = 0x%8x  |", "INT_MASK_1", INT_MASK_1);
	printk("%16s = 0x%8x\n", "SD_STS_0", SD_STS_0);

	printk("%16s = 0x%8x  |", "SD_STS_1", SD_STS_1);
	printk("%16s = 0x%8x\n", "SD_STS_2", SD_STS_2);

	printk("%16s = 0x%8x  |", "SD_STS_3", SD_STS_3);
	printk("%16s = 0x%8x\n", "RSP_TOUT", RSP_TOUT);

	printk("%16s = 0x%8x  |", "CLK_SEL", CLK_SEL);
	printk("%16s = 0x%8x\n", "EXT_CTL", EXT_CTL);

	printk("%16s = 0x%8x  |", "SHDW_BLKLEN", SHDW_BLKLEN);
	printk("%16s = 0x%8x\n", "TIMER_VAL", TIMER_VAL);

	printk("%16s = 0x%8x  |", "PDMA_GCR", PDMA_GCR);
	printk("%16s = 0x%8x\n", "PDMA_IER", PDMA_IER);
    
	printk("%16s = 0x%8x  |", "PDMA_ISR", PDMA_ISR);
	printk("%16s = 0x%8x\n", "PDMA_DESPR", PDMA_DESPR);

    printk("%16s = 0x%8x  |", "PDMA_RBR", PDMA_RBR);
	printk("%16s = 0x%8x\n", "PDMA_DAR", PDMA_DAR);

    printk("%16s = 0x%8x  |", "PDMA_BAR", PDMA_BAR);
	printk("%16s = 0x%8x\n", "PDMA_CPR", PDMA_CPR);

    printk("%16s = 0x%8x  |", "PDMA_CCR", PDMA_CCR);
	printk("\n+----------------------------------------------------------------+\n");
}
#else
void atsmb3_dump_reg(struct atsmb_host *host) {}
#endif

unsigned int fmax3 = 515633;
unsigned int MMC3_DRIVER_VERSION;
int SD3_function = 0; /*0: normal SD/MMC card reader , 1: internal SDIO wifi*/ 
static int isMtk6620 = 0;

int SCC3_ID(void){
	unsigned short val;
    
	val = REG16_VAL(SYSTEM_CFG_CTRL_BASE_ADDR + 0x2);
	return val;
}

int get_chip_version3(void) /*2008/05/01 janshiue modify for A1 chip*/
{
	u32 tmp;
    
	tmp = REG32_VAL(SYSTEM_CFG_CTRL_BASE_ADDR);
	tmp = ((tmp & 0xF00) >> 4) + 0x90 + ((tmp & 0xFF) - 1);
	return tmp;
}

void get_driver_version3(void)
{
		if (SCC3_ID() == 0x3426) {
			if (get_chip_version3() < 0xA1)
				MMC3_DRIVER_VERSION = MMC_DRV_3426_A0;
			 else if (get_chip_version3() == 0xA1)
				MMC3_DRIVER_VERSION = MMC_DRV_3426_A1;
			 else
				MMC3_DRIVER_VERSION = MMC_DRV_3426_A2;
		} else if(SCC3_ID() == 0x3437) {
			if (get_chip_version3() < 0xA1)
				MMC3_DRIVER_VERSION = MMC_DRV_3437_A0;
			else
				MMC3_DRIVER_VERSION = MMC_DRV_3437_A1;
		} else if(SCC3_ID() == 0x3429) {
            MMC3_DRIVER_VERSION = MMC_DRV_3429;
		} else if(SCC3_ID() == 0x3451) {
			if (get_chip_version3() < 0xA1)
				MMC3_DRIVER_VERSION = MMC_DRV_3451_A0;
        } else if(SCC3_ID() == 0x3465) {
            MMC3_DRIVER_VERSION = MMC_DRV_3465;
        } else if(SCC3_ID() == 0x3445) {
            MMC3_DRIVER_VERSION = MMC_DRV_3445;
        } else if(SCC3_ID() == 0x3481) {
            MMC3_DRIVER_VERSION = MMC_DRV_3481;
        }
}
/*2008/10/6 RichardHsu-e*/

/**********************************************************************
Name  	    : atsmb3_alloc_desc
Function    : To config PDMA descriptor setting.
Calls       :
Called by   :
Parameter   :
Author 	    : Janshiue Wu
History	    :
***********************************************************************/
static inline int atsmb3_alloc_desc(struct atsmb_host *host,
									unsigned int	bytes)
{
	void	*DescPool = NULL;
	DBG("[%s] s\n",__func__);

	DescPool = dma_alloc_coherent(host->mmc->parent, bytes, &(host->DescPhyAddr), GFP_KERNEL);
	if (!DescPool) {
		DBG("can't allocal desc pool=%8X %8X\n", DescPool, host->DescPhyAddr);
		DBG("[%s] e1\n",__func__);
		return -1;
	}
	DBG("allocal desc pool=%8X %8X\n", DescPool, host->DescPhyAddr);
	host->DescVirAddr = (unsigned long *)DescPool;
	host->DescSize = bytes;
	DBG("[%s] e2\n",__func__);
	return 0;
}

/**********************************************************************
Name  	    : atsmb3_config_desc
Function    : To config PDMA descriptor setting.
Calls       :
Called by   :
Parameter   :
Author 	    : Janshiue Wu
History	    :
***********************************************************************/
static inline void atsmb3_config_desc(unsigned long *DescAddr,
									unsigned long *BufferAddr,
									unsigned long Blk_Cnt)
{

	int i = 0 ;
	unsigned long *CurDes = DescAddr;
	DBG("[%s] s\n",__func__);

	/*	the first Descriptor store for 1 Block data
	 *	(512 bytes)
	 */
	for (i = 0 ; i < 3 ; i++) {
		atsmb3_init_short_desc(CurDes, 0x80, BufferAddr, 0);
		BufferAddr += 0x20;
		CurDes += sizeof(struct SD_PDMA_DESC_S)/4;
	}
	if (Blk_Cnt > 1) {
		atsmb3_init_long_desc(CurDes, 0x80, BufferAddr, CurDes + (sizeof(struct SD_PDMA_DESC_L)/4), 0);
		BufferAddr += 0x20;
		CurDes +=  sizeof(struct SD_PDMA_DESC_L)/4;
		/*	the following Descriptor store the rest Blocks data
		 *	(Blk_Cnt - 1) * 512 bytes
		 */
		atsmb3_init_long_desc(CurDes, (Blk_Cnt - 1) * 512,
			BufferAddr, CurDes + (sizeof(struct SD_PDMA_DESC_L)/4), 1);
	} else {
		atsmb3_init_long_desc(CurDes, 0x80, BufferAddr, CurDes + (sizeof(struct SD_PDMA_DESC_L)/4), 1);
	}
	DBG("[%s] e\n",__func__);
}
/**********************************************************************
Name  	 : atsmb3_config_dma
Function    : To set des/src address, byte count to transfer, and DMA channel settings,
			 and DMA ctrl. register.
Calls		:
Called by	:
Parameter :
Author 	 : Leo Lee
History	:
***********************************************************************/
static inline void atsmb3_config_dma(unsigned long config_dir,
									unsigned long dma_mask,
									struct atsmb_host *host)
{

    DBG("[%s] s\n",__func__);
	/* Enable DMA */
	*ATSMB3_PDMA_GCR = SD_PDMA_GCR_DMA_EN;
	*ATSMB3_PDMA_GCR = SD_PDMA_GCR_SOFTRESET;
	*ATSMB3_PDMA_GCR = SD_PDMA_GCR_DMA_EN;
	/*open interrupt*/
	*ATSMB3_PDMA_IER = SD_PDMA_IER_INT_EN;
	/*Make sure host could co-work with DMA*/
	*ATSMB3_SD_STS_2 |= ATSMB_CLK_FREEZ_EN;

	/*Set timer timeout value*/
	/*If clock is 390KHz*/
	if (host->current_clock < 400000)
		*ATSMB3_TIMER_VAL = 0x200;		/*1024*512*(1/390K) second*/
	else
		*ATSMB3_TIMER_VAL = 0x1fff;		/*why not to be 0xffff?*/
	
	/*clear all DMA INT status for safety*/
	*ATSMB3_PDMA_ISR |= SD_PDMA_IER_INT_STS;

	/* hook desc */
	*ATSMB3_PDMA_DESPR = host->DescPhyAddr;
	if (config_dir == DMA_CFG_WRITE)
		*ATSMB3_PDMA_CCR &= SD_PDMA_CCR_IF_to_peripheral;
	else
		*ATSMB3_PDMA_CCR |= SD_PDMA_CCR_peripheral_to_IF;

	host->DmaIntMask = dma_mask; /*save success event*/

	*ATSMB3_PDMA_CCR |= SD_PDMA_CCR_RUN;
	DBG("[%s] e\n",__func__);
}

/**********************************************************************
Name  	 : atsmb3_prep_cmd
Function    :
Calls		:
Called by	:
Parameter :
Author 	 : Leo Lee
History	:
***********************************************************************/
static inline void atsmb3_prep_cmd(struct atsmb_host *host,
									u32 opcode,
									u32 arg,
									unsigned int flags,
									u16  blk_len,
									u16  blk_cnt,
									unsigned char int_maks_0,
									unsigned char int_mask_1,
									unsigned char cmd_type,
									unsigned char op)
{
	DBG("[%s] s\n",__func__);
    
	/*set cmd operation code and arguments.*/
	host->opcode = opcode;
	*ATSMB3_CMD_IDX = opcode; /* host->opcode is set for further use in ISR.*/
	*ATSMB3_CMD_ARG = arg;

#if 0   /* Fixme to support SPI mode, James Tian*/
    if ((flags && MMC_RSP_NONE) == MMC_RSP_NONE)
        *ATSMB3_RSP_TYPE = ATMSB_TYPE_R0;
    else if ((flags && MMC_RSP_R1) == MMC_RSP_R1)
        *ATSMB3_RSP_TYPE = ATMSB_TYPE_R1;
    else if ((flags && MMC_RSP_R1B) == MMC_RSP_R1B)
        *ATSMB3_RSP_TYPE = ATMSB_TYPE_R1b;
    else if ((flags && MMC_RSP_R2) == MMC_RSP_R2)
        *ATSMB3_RSP_TYPE = ATMSB_TYPE_R2;
    else if ((flags && MMC_RSP_R3) == MMC_RSP_R3)
        *ATSMB3_RSP_TYPE = ATMSB_TYPE_R3;
    else if ((flags && MMC_RSP_R6) == MMC_RSP_R6)
        *ATSMB3_RSP_TYPE = ((opcode != SD_SEND_IF_COND) ? ATMSB_TYPE_R6 : ATMSB_TYPE_R7);
    else
		*ATSMB3_RSP_TYPE = ATMSB_TYPE_R0;
#endif

#if 1
	/*set cmd response type*/
	switch (flags) {
	case MMC_RSP_NONE | MMC_CMD_AC:
	case MMC_RSP_NONE | MMC_CMD_BC:
		*ATSMB3_RSP_TYPE = ATMSB_TYPE_R0;
		break;
	case MMC_RSP_R1 | MMC_CMD_ADTC:
	case MMC_RSP_R1 | MMC_CMD_AC:
		*ATSMB3_RSP_TYPE = ATMSB_TYPE_R1;
		break;
	case MMC_RSP_R1B | MMC_CMD_AC:
		*ATSMB3_RSP_TYPE = ATMSB_TYPE_R1b;
		break;
	case MMC_RSP_R2 | MMC_CMD_BCR:
	case MMC_RSP_R2 | MMC_CMD_AC:
		*ATSMB3_RSP_TYPE = ATMSB_TYPE_R2;
		break;
	case MMC_RSP_R3 | MMC_CMD_BCR:
		*ATSMB3_RSP_TYPE = ATMSB_TYPE_R3;
		break;
	case MMC_RSP_R6 | MMC_CMD_BCR: /*MMC_RSP_R6 = MMC_RSP_R7.*/
		*ATSMB3_RSP_TYPE = ((opcode != SD_SEND_IF_COND) ?
			ATMSB_TYPE_R6 : ATMSB_TYPE_R7);
		break;
    case MMC_RSP_SPI_R5 | MMC_RSP_R5 | MMC_CMD_AC:
    case MMC_RSP_SPI_R5 | MMC_RSP_R5 | MMC_CMD_ADTC:
        *ATSMB3_RSP_TYPE = ATMSB_TYPE_R5;
		break;
    case MMC_RSP_SPI_R4 | MMC_RSP_R4 | MMC_CMD_BCR:
        *ATSMB3_RSP_TYPE = ATMSB_TYPE_R4;
		break;
    default:
		*ATSMB3_RSP_TYPE = ATMSB_TYPE_R0;
		break;
	}
#endif
    /*SDIO cmd 52 , 53*/
    if ((opcode == SD_IO_RW_DIRECT) ||
        (opcode == SD_IO_RW_EXTENDED)) {
        *ATSMB3_RSP_TYPE = ATMSB_TYPE_R5;
        *ATSMB3_RSP_TYPE |= BIT6;
    }
    /*SDIO cmd 5*/
    if ((opcode == SD_IO_SEND_OP_COND) && 
        ((flags & (MMC_RSP_PRESENT|
                  MMC_RSP_136|
                  MMC_RSP_CRC|
                  MMC_RSP_BUSY|
                  MMC_RSP_OPCODE)) == MMC_RSP_R4)) {
        *ATSMB3_RSP_TYPE = ATMSB_TYPE_R4;
        *ATSMB3_RSP_TYPE |= BIT6;
    }
    
	/*reset Response FIFO*/
	*ATSMB3_CTL |= 0x08;

	/* SD Host enable Clock */
    *ATSMB3_BUS_MODE |= ATSMB_CST;    

	/*Set Cmd-Rsp Timeout to be maximum value.*/
	*ATSMB3_RSP_TOUT = 0xFE;

	/*clear all status registers for safety*/
	*ATSMB3_SD_STS_0 |= 0xff;
	*ATSMB3_SD_STS_1 |= 0xff;
	*ATSMB3_SD_STS_2 |= 0xff;
	//*ATSMB3_SD_STS_2 |= 0x7f;
	*ATSMB3_SD_STS_3 |= 0xff;

	//set block length and block count for cmd requesting data
	*ATSMB3_BLK_LEN &=~(0x07ff);
	*ATSMB3_BLK_LEN |= blk_len;
	//*ATSMB3_SHDW_BLKLEN = blk_len;
	*ATSMB3_BLK_CNT = blk_cnt;


	*ATSMB3_INT_MASK_0 |= int_maks_0;
	*ATSMB3_INT_MASK_1 |= int_mask_1;

	//Set Auto stop for Multi-block access
	if(cmd_type == 3 || cmd_type == 4)
	{
		//auto stop command set.
		*ATSMB3_EXT_CTL |= 0x01;

/*
 * Enable transaction abort.
 * When CRC error occurs, CMD 12 would be automatically issued.
 * That is why we cannot enable R/W CRC error INTs.
 * If we enable CRC error INT, we would handle this INT in ISR and then issue CMD 12 via software.
 */
		*ATSMB3_BLK_LEN |= 0x0800;
	}

	/*Set read or write*/
	if (op == 1)
		*ATSMB3_CTL &=  ~(0x04);
	else if (op == 2)
		*ATSMB3_CTL |=  0x04;

	/*for Non data access command, command type is 0.*/
	*ATSMB3_CTL &= 0x0F;
	*ATSMB3_CTL |= (cmd_type<<4);
	DBG("[%s] e\n",__func__);
}

static inline void atsmb3_issue_cmd(void)
{
	wmb();
	*ATSMB3_CTL |= ATSMB_START;
	wmb();	
}
/**********************************************************************
Name  	 : atsmb3_request_end
Function    :.
Calls		:
Called by	:
Parameter :
Author 	 : Leo Lee
History	:
***********************************************************************/
static void
atsmb3_request_end(struct atsmb_host *host, struct mmc_request *mrq)
{
	DBG("[%s] s\n",__func__);
	/*
	 * Need to drop the host lock here; mmc_request_done may call
	 * back into the driver...
	 */
	spin_unlock(&host->lock);
	/*DBG("100");*/
	mmc_request_done(host->mmc, mrq);
	/*DBG("101\n");*/
	spin_lock(&host->lock);
	DBG("[%s] e\n",__func__);
}

/**********************************************************************
Name  	 : atsmb3_data_done
Function    :
Calls		:
Called by	:
Parameter :
Author 	 : Leo Lee
History	:
***********************************************************************/
void atsmb3_wait_done(void *data)
{
	struct atsmb_host *host = (struct atsmb_host *) data;
	DBG("[%s] s\n",__func__);

	WARN_ON(host->done_data == NULL);
	complete(host->done_data);
	host->done_data = NULL;
	host->done = NULL;
	DBG("[%s] e\n",__func__);
}

/*
*
* dump receive data from sdio or sdcard by rubbit
*/
void dump_sg_data(struct scatterlist *sgl, unsigned int nents)
{
		int i;
		int ret = 0;
		unsigned char buf[2048];
		memset(buf,0,sizeof(buf));
		ret = sg_copy_to_buffer(sgl, nents, buf, 2048);
		for(i=0;i<ret;i++)
		{
				printk("0x%x ",buf[i]);
				if(!(i%16))
					printk("\n");
		}
}


/**********************************************************************
Name  	 : atsmb3_start_data
Function    : If we start data, there must be only four cases.
Calls		:
Called by	:
Parameter :
Author 	 : Leo Lee
History	:
***********************************************************************/
static void atsmb3_start_data(struct atsmb_host *host)
{
	DECLARE_COMPLETION(complete);
	unsigned char cmd_type = 0;
	unsigned char op = 0;	/*0: non-operation; 1:read; 2: write*/
	unsigned char mask_0 = 0;
	unsigned char mask_1 = 0;
	unsigned long dma_mask = 0;

	struct mmc_data *data = host->data;
	struct mmc_command *cmd = host->cmd;

	struct scatterlist *sg = NULL;
	unsigned int sg_len = 0;

	unsigned int total_blks = 0;		/*total block number to transfer*/
	u32 card_addr = 0;
	unsigned long dma_len = 0;
	unsigned long total_dma_len = 0;
	dma_addr_t dma_phy = 0;			/* physical address used for DMA*/
	unsigned int dma_times = 0;  /*times dma need to transfer*/
	unsigned int dma_loop = 0;
	unsigned int sg_num = 0;
	int loop_cnt = 10000;
	unsigned int sg_transfer_len = 0; /*record each time dma transfer sg length */

	DBG("[%s] s\n",__func__);
	data->bytes_xfered = 0;
	cmd->error = 0;
	data->error = 0;

	/*for loop*/
	sg = data->sg;
	sg_len = data->sg_len;

	dma_times = (sg_len / MAX_DESC_NUM);
	if (sg_len % MAX_DESC_NUM)
		dma_times++;
	DBG("dma_times = %d  sg_len = %d sg = %x\n", dma_times, sg_len, sg);
	card_addr = cmd->arg; /*may be it is block-addressed, or byte-addressed.*/
	total_blks = data->blocks;
	dma_map_sg(&(host->mmc->class_dev), sg, sg_len,
				((data->flags)&MMC_DATA_READ) ? DMA_FROM_DEVICE : DMA_TO_DEVICE);


	for (dma_loop = 1 ; dma_loop <= dma_times; dma_loop++, sg_len -= sg_transfer_len) {
		DBG("dma_loop = %d  sg_len = %d  sg_transfer_len = %d\n", dma_loop, sg_len, sg_transfer_len);
		if (dma_loop < dma_times)
			sg_transfer_len = MAX_DESC_NUM;
		else
			sg_transfer_len = sg_len;
		DBG("sg_transfer_len = %d\n", sg_transfer_len);
		/*
		*Firstly, check and wait till card is in the transfer state.
		*For our hardware, we can not consider
		*the card has successfully tranfered its state from data/rcv to trans,
		*when auto stop INT occurs.
		*/
		loop_cnt = 10000;
		do {
            if (host->cmd->opcode == SD_IO_RW_EXTENDED)
                break;
			loop_cnt--;
			WARN_ON(loop_cnt == 0);
			host->done_data = &complete;
			host->done = &atsmb3_wait_done;
			host->soft_timeout = 1;
			atsmb3_prep_cmd(host,
							MMC_SEND_STATUS,
							(host->mmc->card->rca)<<16,
							MMC_RSP_R1 | MMC_CMD_AC,
							0,
							0,
							0,		/*mask_0*/
							ATSMB_RSP_DONE_EN
							|ATSMB_RSP_CRC_ERR_EN
							|ATSMB_RSP_TIMEOUT_EN,
							0,		/*cmd type*/
							0);		/*read or write*/
			atsmb3_issue_cmd();
			DBG("%16s = 0x%8x  |", "INT_MASK_1", *ATSMB3_INT_MASK_1);
			DBG("%16s = 0x%8x \n", "SD_STS_1", *ATSMB3_SD_STS_1);
			/*ISR would completes it.*/
			wait_for_completion_timeout(&complete, ATSMB_TIMEOUT_TIME);

			WARN_ON(host->soft_timeout == 1);
			if (host->soft_timeout == 1) {
				DBG("%s soft_timeout.\n", __func__);
				atsmb3_dump_reg(host);
			}
			if (cmd->error != MMC_ERR_NONE) {
				cmd->error = -EIO; //MMC_ERR_FAILED;
				printk("Getting Status failed.\n");
				goto end;
			}
		} while ((cmd->resp[0] & 0x1f00) != 0x900 && loop_cnt > 0); /*wait for trans state.*/
	if(debug_enable)
			printk("sdio card is ready\n");
		/*
		* Now, we can safely issue read/write command.
		* We can not consider this request as multi-block acess or single one via opcode,
		* as request is splitted into sgs.
		* Some sgs may be single one, some sgs may be multi one.
		*/

		dma_phy = sg_dma_address(sg);
		dma_len = sg_dma_len(sg);
		DBG("dma_len = %d data->blksz = %d sg_len = %d\n", dma_len, data->blksz, sg_len);
        /*SDIO read/write*/
        if (host->cmd->opcode == SD_IO_RW_EXTENDED) {
             /*Single Block read/write*/   
             if ((dma_len / (data->blksz)) == 1 && (sg_len == 1)) {
                /* read operation*/
                if (data->flags & MMC_DATA_READ) {
                    host->opcode = SD_IO_RW_EXTENDED;
        			cmd_type = 2;
        			op = 1;
        			mask_0  = 0;	/*BLOCK_XFER_DONE INT skipped, we use DMA TC INT*/
        			mask_1 = (//ATSMB_SDIO_EN
                                ATSMB_READ_CRC_ERR_EN
        						|ATSMB_DATA_TIMEOUT_EN
        						/*Data Timeout and CRC error */
        						|ATSMB_RSP_CRC_ERR_EN
        						|ATSMB_RSP_TIMEOUT_EN);
        						/*Resp Timeout and CRC error */
        			dma_mask = SD_PDMA_CCR_Evt_success;
                    DBG("[%s]SR opcode = %d type = 0x%x op = 0x%x m_0 = 0x%x m_1 = 0x%x d_m = 0x%x\n",
                        __func__,host->opcode,cmd_type,op,mask_0,mask_1,dma_mask);
                } else {
                /* write operation*/
                    host->opcode = SD_IO_RW_EXTENDED;
        			cmd_type = 1;
        			op = 2;
        			/*====That is what we want===== DMA TC INT skipped*/
        			mask_0  = ATSMB_BLOCK_XFER_DONE_EN;
        			mask_1 = (//ATSMB_SDIO_EN
                                ATSMB_WRITE_CRC_ERR_EN
        						|ATSMB_DATA_TIMEOUT_EN
        						/*Data Timeout and CRC error */
        						|ATSMB_RSP_CRC_ERR_EN
        						|ATSMB_RSP_TIMEOUT_EN);
        						/*Resp Timeout and CRC error */
        			dma_mask = 0;
                    DBG("[%s]SW opcode = %d type = 0x%x op = 0x%x m_0 = 0x%x m_1 = 0x%x d_m = 0x%x\n",
                        __func__,host->opcode,cmd_type,op,mask_0,mask_1,dma_mask);
                }
             } else {
             /*Multiple Block read/write*/
                /* read operation*/
                if (data->flags & MMC_DATA_READ) {
                    host->opcode = SD_IO_RW_EXTENDED;
        			cmd_type = 6;
        			op = 1;
        			mask_0  = 0;	/*MULTI_XFER_DONE_EN skipped*/
        			mask_1 = (//ATSMB_SDIO_EN	/*====That is what we want=====*/
                                ATSMB_READ_CRC_ERR_EN
                                |ATSMB_DATA_TIMEOUT_EN
        						/*Data Timeout and CRC error */
        						|ATSMB_RSP_CRC_ERR_EN
        						|ATSMB_RSP_TIMEOUT_EN);
        						/*Resp Timeout and CRC error */
        			dma_mask = SD_PDMA_CCR_Evt_success;
                    DBG("[%s]MR opcode = %d type = 0x%x op = 0x%x m_0 = 0x%x m_1 = 0x%x d_m = 0x%x\n",
                        __func__,host->opcode,cmd_type,op,mask_0,mask_1,dma_mask);            
                } else {
                /* write operation*/
                    host->opcode = SD_IO_RW_EXTENDED;
        			cmd_type = 5;
        			op = 2;
        			mask_0  = ATSMB_MULTI_XFER_DONE_EN;//ATSMB_BLOCK_XFER_DONE_EN;	/*MULTI_XFER_DONE INT skipped*/
        			mask_1 = (//ATSMB_SDIO_EN		/*====That is what we want=====*/
                                ATSMB_WRITE_CRC_ERR_EN
                                |ATSMB_DATA_TIMEOUT_EN
        						/*Data Timeout and CRC error */
        						|ATSMB_RSP_CRC_ERR_EN
        						|ATSMB_RSP_TIMEOUT_EN);
        						/*Resp Timeout and CRC error */
        			dma_mask = 0;
                    DBG("[%s]MR opcode = %d type = 0x%x op = 0x%x m_0 = 0x%x m_1 = 0x%x d_m = 0x%x\n",
                        __func__,host->opcode,cmd_type,op,mask_0,mask_1,dma_mask);            
                }
             }
            
        } else {
            if ((dma_len / (data->blksz)) == 1 && (sg_len == 1)) {
        		if (data->flags & MMC_DATA_READ) {/* read operation*/
        			host->opcode = MMC_READ_SINGLE_BLOCK;
        			cmd_type = 2;
        			op = 1;
        			mask_0  = 0;	/*BLOCK_XFER_DONE INT skipped, we use DMA TC INT*/
        			mask_1 = (ATSMB_READ_CRC_ERR_EN
        						|ATSMB_DATA_TIMEOUT_EN
        						/*Data Timeout and CRC error */
        						|ATSMB_RSP_CRC_ERR_EN
        						|ATSMB_RSP_TIMEOUT_EN);
        						/*Resp Timeout and CRC error */
        			dma_mask = SD_PDMA_CCR_Evt_success;
        		} else {/*write operation*/
        			host->opcode = MMC_WRITE_BLOCK;
        			cmd_type = 1;
        			op = 2;
        			/*====That is what we want===== DMA TC INT skipped*/
        			mask_0  = ATSMB_BLOCK_XFER_DONE_EN;
        			mask_1 = (ATSMB_WRITE_CRC_ERR_EN
        						|ATSMB_DATA_TIMEOUT_EN
        						/*Data Timeout and CRC error */
        						|ATSMB_RSP_CRC_ERR_EN
        						|ATSMB_RSP_TIMEOUT_EN);
        						/*Resp Timeout and CRC error */
        			dma_mask = 0;
        		}
        	} else {  /*more than one*/
        		if (data->flags&MMC_DATA_READ) {/* read operation*/
        			host->opcode = MMC_READ_MULTIPLE_BLOCK;
        			cmd_type = 4;
        			op = 1;
        			mask_0  = 0;	/*MULTI_XFER_DONE_EN skipped*/
        			mask_1 = (ATSMB_AUTO_STOP_EN	/*====That is what we want=====*/
        						|ATSMB_DATA_TIMEOUT_EN
        						/*Data Timeout and CRC error */
        						|ATSMB_RSP_CRC_ERR_EN
        						|ATSMB_RSP_TIMEOUT_EN);
        						/*Resp Timeout and CRC error */
        			dma_mask = 0;
        		} else {/*write operation*/
        			host->opcode = MMC_WRITE_MULTIPLE_BLOCK;
        			cmd_type = 3;
        			op = 2;
        			mask_0  = 0;	/*MULTI_XFER_DONE INT skipped*/
        			mask_1 = (ATSMB_AUTO_STOP_EN		/*====That is what we want=====*/
        						|ATSMB_DATA_TIMEOUT_EN
        						/*Data Timeout and CRC error */
        						|ATSMB_RSP_CRC_ERR_EN
        						|ATSMB_RSP_TIMEOUT_EN);
        						/*Resp Timeout and CRC error */
        			dma_mask = 0;
        		}
        	}
        }
		/*To controller every sg done*/
		host->done_data = &complete;
		host->done = &atsmb3_wait_done;
		/*sleep till ISR wakes us*/
		host->soft_timeout = 1;	/*If INT comes early than software timer, it would be cleared.*/

		total_dma_len = 0;
		DBG("host->DescVirAddr = %x host->DescSize=%x\n", host->DescVirAddr, host->DescSize);
		memset(host->DescVirAddr, 0, host->DescSize);
		for (sg_num = 0 ; sg_num < sg_transfer_len ; sg++, sg_num++) {

			/*
			 * Now, we can safely issue read/write command.
			 * We can not consider this request as multi-block acess or single one via opcode,
			 * as request is splitted into sgs.
			 * Some sgs may be single one, some sgs may be multi one.
			 */

			dma_phy = sg_dma_address(sg);
			dma_len = sg_dma_len(sg);
			total_dma_len = total_dma_len + dma_len;
			DBG("sg_num=%d sg_transfer_len=%d sg=%x sg_len=%d total_dma_len=%d dma_len=%d\n",
				sg_num, sg_transfer_len, sg, sg_len, total_dma_len, dma_len);
			/*2009/01/15 janshiue add*/
			if (sg_num < sg_transfer_len - 1) {
				/* means the last descporitor */
				atsmb3_init_short_desc(
				host->DescVirAddr + (sg_num * sizeof(struct SD_PDMA_DESC_S)/4),
				dma_len, (unsigned long *)dma_phy, 0);
			} else {
				atsmb3_init_short_desc(
				host->DescVirAddr + (sg_num * sizeof(struct SD_PDMA_DESC_S)/4),
				dma_len, (unsigned long *)dma_phy, 1);
			}
		/*2009/01/15 janshiue add*/

		}
		/*operate our hardware*/
		atsmb3_prep_cmd(host,
						host->opcode,
			/*arg, may be byte addressed, may be block addressed.*/
						card_addr,
						cmd->flags,
						data->blksz - 1,	/* in fact, it is useless.*/
			/* for single one, it is useless. but for multi one, */
			/* it would be used to tell auto stop function whether it is done.*/
						total_dma_len/(data->blksz),
						mask_0,
						mask_1,
						cmd_type,
						op);

		atsmb3_config_dma((op == 1) ? DMA_CFG_READ : DMA_CFG_WRITE,
						 dma_mask,
						 host);

		/*Before write command pull busy state down, can't execute next 
		command for SDIO device. because HW don't support SDIO R1b response clear
		add by Eason 2012/08/09*/
		while(!(*ATSMB3_CURBLK_CNT & BIT24))
				;
		atsmb3_issue_cmd();
		wait_for_completion_timeout(&complete,
			ATSMB_TIMEOUT_TIME*sg_transfer_len); /*ISR would completes it.*/

		/* When the address of request plus length equal card bound, 
		 * force this stop command response as pass. Eason 2012/4/20 */
		if (cmd->resp[0] == 0x80000b00) {
			/*This caes used for SD2.0 and after MMC4.1 version*/
			if (card_addr+(total_dma_len/data->blksz) == host->mmc->card->csd.capacity) {
			cmd->resp[0] = 0x00000b00;
			/*printk("card_addr = %08X, card_length = %08X,card capacity = %08X\n",
			card_addr,(total_dma_len/data->blksz),host->mmc->card->csd.capacity);
			printk("card_resp[0]=%08X, addr = %08X\n",cmd->resp[0],cmd->resp);*/
			}
		
			/* This caes used for SD1.0 and before MMC 4.1 */
			if ((card_addr/data->blksz)+(total_dma_len/data->blksz) == host->mmc->card->csd.capacity) {
			cmd->resp[0] = 0x00000b00;
			/*printk("Eason test: cmd->arg = %08X, data->blksz = %08X, length = %08X\n",
			card_addr,data->blksz,(total_dma_len/data->blksz));*/
			}
		}
		
		if (host->soft_timeout == 1) {
            atsmb3_dump_reg(host);
            //*ATSMB3_BUS_MODE |= ATSMB_SFTRST;
            *ATSMB3_PDMA_GCR = SD_PDMA_GCR_SOFTRESET;
            /*disable INT */
	        *ATSMB3_INT_MASK_0 &= ~(ATSMB_BLOCK_XFER_DONE_EN | ATSMB_MULTI_XFER_DONE_EN);
	        *ATSMB3_INT_MASK_1 &= ~(ATSMB_WRITE_CRC_ERR_EN|ATSMB_READ_CRC_ERR_EN|ATSMB_RSP_CRC_ERR_EN
		        |ATSMB_DATA_TIMEOUT_EN|ATSMB_AUTO_STOP_EN|ATSMB_RSP_TIMEOUT_EN|ATSMB_RSP_DONE_EN);

            cmd->error = -ETIMEDOUT;
            data->error = -ETIMEDOUT;
        }

        WARN_ON(host->soft_timeout == 1);

        /*check everything goes okay or not*/
		if (cmd->error != MMC_ERR_NONE
			|| data->error != MMC_ERR_NONE) {
			DBG("CMD or Data failed error=%X DescVirAddr=%8X dma_phy=%8X dma_mask = %x\n",
				cmd->error, host->DescVirAddr, dma_phy, host->DmaIntMask);
			goto end;
		}
//		card_addr += total_dma_len>>(mmc_card_blockaddr(host->mmc->card_selected) ? 9 : 0);
		card_addr += total_dma_len>>(mmc_card_blockaddr(host->mmc->card) ? 9 : 0);	//zhf: modified by James Tian
		data->bytes_xfered += total_dma_len;

		if(debug_enable) { 
				printk("issue cmd ok for host->opcode:0x%x,cmd_type:0x%x\n",host->opcode,cmd_type);
				dump_sg_data(data->sg, data->sg_len);
		}
	}

//	dma_unmap_sg(&(host->mmc->class_dev), data->sg, data->sg_len,
//			((data->flags)&MMC_DATA_READ) ? DMA_FROM_DEVICE : DMA_TO_DEVICE);
	WARN_ON(total_blks != (data->bytes_xfered / data->blksz));
	host->opcode = 0;
end:
	dma_unmap_sg(&(host->mmc->class_dev), data->sg, data->sg_len,
			((data->flags)&MMC_DATA_READ) ? DMA_FROM_DEVICE : DMA_TO_DEVICE);
	spin_lock(&host->lock);
	atsmb3_request_end(host, host->mrq);
	spin_unlock(&host->lock);
	DBG("[%s] e\n",__func__);
}


/**********************************************************************
Name  	 : atsmb3_cmd_with_data_back
Function    :
Calls		:
Called by	:
Parameter :
Author 	 : Leo Lee
History	:
***********************************************************************/
static void  atsmb3_cmd_with_data_back(struct atsmb_host *host)
{
	DECLARE_COMPLETION(complete);
	
	struct scatterlist *sg = NULL;
	unsigned int sg_len = 0;
	DBG("[%s] s\n",__func__);
	if(debug_enable)
		printk("enter:%s\n",__func__);	
	/*for loop*/
	sg = host->data->sg;
	sg_len = host->data->sg_len;
	/*To controller every sg done*/
	host->done_data = &complete;
	host->done = &atsmb3_wait_done;
	dma_map_sg(&(host->mmc->class_dev), sg, sg_len, DMA_FROM_DEVICE);

	/*2009/01/15 janshiue add*/
	memset(host->DescVirAddr, 0, host->DescSize);
	atsmb3_init_long_desc(host->DescVirAddr, sg_dma_len(sg), (unsigned long *)sg_dma_address(sg), 0, 1);
	/*2009/01/15 janshiue add*/
	/*prepare for cmd*/
	atsmb3_prep_cmd(host,					/*host*/
					host->cmd->opcode, 	/*opcode*/
					host->cmd->arg,		/*arg*/
					host->cmd->flags,		/*flags*/
					sg_dma_len(sg)-1,		/*block length*/
					0,		/*block size: It looks like useless*/
					0,		/*int_mask_0*/
					(ATSMB_RSP_CRC_ERR_EN|ATSMB_RSP_TIMEOUT_EN
					|ATSMB_READ_CRC_ERR_EN |ATSMB_DATA_TIMEOUT_EN), /*int_mask_1*/
					2, 					/*cmd_type*/
					1);					/*op*/

	atsmb3_config_dma(DMA_CFG_READ,
					 SD_PDMA_CCR_Evt_success,
					 host);
	atsmb3_issue_cmd();
	/*ISR would completes it.*/
	wait_for_completion_timeout(&complete, ATSMB_TIMEOUT_TIME); 
	
	dma_unmap_sg(&(host->mmc->class_dev), host->data->sg, host->data->sg_len,
			((host->data->flags)&MMC_DATA_READ) ? DMA_FROM_DEVICE : DMA_TO_DEVICE);
	spin_lock(&host->lock);
	atsmb3_request_end(host, host->mrq);
	spin_unlock(&host->lock);
	DBG("[%s] e\n",__func__);
	if(debug_enable)
		printk("exit:%s\n",__func__);	
}
/**********************************************************************
Name  	 : atsmb3_start_cmd
Function    :
Calls		:
Called by	:
Parameter :
Author 	 : Leo Lee
History	:
***********************************************************************/
static void atsmb3_start_cmd(struct atsmb_host *host)
{
    unsigned char int_mask_0,int_mask_1;
    int_mask_0 = 0;
    int_mask_1 = ATSMB_RSP_DONE_EN|ATSMB_RSP_CRC_ERR_EN|ATSMB_RSP_TIMEOUT_EN;

    DBG("[%s] s\n",__func__);
	if(host->cmd->opcode!=52 && debug_enable)
		printk("atsmb3_start_cmd %d\n",host->cmd->opcode);
    atsmb3_prep_cmd(host,
					host->cmd->opcode,
					host->cmd->arg,
					host->cmd->flags,
					0,		/*useless*/
					0,		/*useless*/
					int_mask_0,		/*mask_0*/
					int_mask_1,     /*mask_1*/
					0,		/*cmd type*/
					0);		/*read or write*/
	atsmb3_issue_cmd();
	DBG("[%s] e\n",__func__);
}
/**********************************************************************
Name  	 : atsmb3_fmt_check_rsp
Function    :
Calls		:
Called by	:
Parameter :
Author 	 : Leo Lee
History	:
***********************************************************************/
static inline void atsmb3_fmt_check_rsp(struct atsmb_host *host)
{

	u8 tmp_resp[4] = {0};
	int i, j, k;
	DBG("[%s] s\n",__func__);
	if (host->cmd->flags != (MMC_RSP_R2 | MMC_CMD_AC)
		&& host->cmd->flags != (MMC_RSP_R2 | MMC_CMD_BCR)) {
		for (j = 0, k = 1; j < 4; j++, k++)
			tmp_resp[j] = *(REG8_PTR(_RSP_0 + MMC_ATSMB3_BASE+k));

		host->cmd->resp[0] = (tmp_resp[0] << 24) |
			(tmp_resp[1]<<16) | (tmp_resp[2]<<8) | (tmp_resp[3]);
	} else {
		for (i = 0, k = 1; i < 4; i++) { /*R2 has 4 u32 response.*/
			for (j = 0; j < 4; j++) {
				if (k < 16)
					tmp_resp[j] = *(REG8_PTR(_RSP_0 + MMC_ATSMB3_BASE+k));
				else /* k =16*/
					tmp_resp[j] = *(ATSMB3_RSP_0);
					k++;
			 }
			host->cmd->resp[i] = (tmp_resp[0]<<24) | (tmp_resp[1]<<16) |
				(tmp_resp[2]<<8) | (tmp_resp[3]);
		}
	}

	/*
	* For the situation that we need response,
	* but response registers give us all zeros, we consider this operation timeout.
	*/
	if (host->cmd->flags != (MMC_RSP_NONE | MMC_CMD_AC)
		&& host->cmd->flags != (MMC_RSP_NONE | MMC_CMD_BC)) {
		if (host->cmd->resp[0] == 0 && host->cmd->resp[1] == 0
			&& host->cmd->resp[2] == 0 && host->cmd->resp[3] == 0) {
			host->cmd->error = -ETIMEDOUT;	//zhf: modified by James Tian
        }
	}
	DBG("[%s] e\n",__func__);
}
/**********************************************************************
Name  	 : atsmb3_get_slot_status
Function    : Our host only supports one slot.
Calls		:
Called by	:
Parameter :
returns	: 1: in slot; 0: not in slot.
Author 	 : Leo Lee
History	:
***********************************************************************/
static int atsmb3_get_slot_status(struct mmc_host *mmc)
{
//	struct atsmb_host *host = mmc_priv(mmc);
	unsigned char status_0 = 0;
//	unsigned long flags;
	unsigned long ret = 0;
	DBG("[%s] s\n",__func__);
//	spin_lock_irqsave(&host->lock, flags); // Vincent Li mark out for CONFIG_PREEMPT_RT
	status_0 = *ATSMB3_SD_STS_0;
//	spin_unlock_irqrestore(&host->lock, flags); // Vincent Li mark out for CONFIG_PREEMPT_RT
	/* after WM3400 A1 ATSMB_CARD_IN_SLOT_GPI = 1 means not in slot*/
	if (MMC3_DRIVER_VERSION >= MMC_DRV_3426_A0) {
		ret = ((status_0 & ATSMB_CARD_NOT_IN_SLOT_GPI) ? 0 : 1);
		DBG("[%s] e1\n",__func__);
		return ret;
	} else {
		ret = ((status_0 & ATSMB_CARD_NOT_IN_SLOT_GPI) ? 1 : 0);
		DBG("[%s] e2\n",__func__);
		return ret;
	}
	DBG("[%s] e3\n",__func__);	
		return 0;
}

/**********************************************************************
Name        : atsmb3_init_short_desc
Function    :
Calls       :
Called by   :
Parameter   :
Author      : Janshiue Wu
History     :
***********************************************************************/
int atsmb3_init_short_desc(unsigned long *DescAddr, unsigned int ReqCount, unsigned long *BufferAddr, int End)
{
	struct SD_PDMA_DESC_S *CurDes_S;
	DBG("[%s] s\n",__func__);
	CurDes_S = (struct SD_PDMA_DESC_S *) DescAddr;
	CurDes_S->ReqCount = ReqCount;
	CurDes_S->i = 0;
	CurDes_S->format = 0;
	CurDes_S->DataBufferAddr = BufferAddr;
	if (End) {
		CurDes_S->end = 1;
		CurDes_S->i = 1;
	}
	DBG("[%s] e\n",__func__);
	return 0;
}

/**********************************************************************
Name        : atsmb3_init_long_desc
Function    :
Calls       :
Called by   :
Parameter   :
Author      : Janshiue Wu
History     :
***********************************************************************/
int atsmb3_init_long_desc(unsigned long *DescAddr, unsigned int ReqCount,
	unsigned long *BufferAddr, unsigned long *BranchAddr, int End)
{
	struct SD_PDMA_DESC_L *CurDes_L;
	DBG("[%s] s\n",__func__);
	CurDes_L = (struct SD_PDMA_DESC_L *) DescAddr;
	CurDes_L->ReqCount = ReqCount;
	CurDes_L->i = 0;
	CurDes_L->format = 1;
	CurDes_L->DataBufferAddr = BufferAddr;
	CurDes_L->BranchAddr = BranchAddr;
	if (End) {
		CurDes_L->end = 1;
		CurDes_L->i = 1;
	}
	DBG("[%s] e\n",__func__);
	return 0;
}

/**********************************************************************
Name  	 : atsmb3_dma_isr
Function    :.
Calls		:
Called by	:
Parameter :
Author 	 : Leo Lee
History	:
***********************************************************************/
static irqreturn_t atsmb3_dma_isr(int irq, void *dev_id)
{

	struct atsmb_host *host = dev_id;
	u8 status_0, status_1, status_2, status_3;
	u32 pdma_event_code = 0;
	DBG("[%s] s\n",__func__);

    disable_irq_nosync(irq);
	spin_lock(&host->lock);
	/*Get INT status*/
	status_0 = *ATSMB3_SD_STS_0;
	status_1 = *ATSMB3_SD_STS_1;
	status_2 = *ATSMB3_SD_STS_2;
	status_3 = *ATSMB3_SD_STS_3;
	/* after WM3426 A0 using PDMA */
	if (MMC3_DRIVER_VERSION >= MMC_DRV_3426_A0) {

		pdma_event_code  = *ATSMB3_PDMA_CCR & SD_PDMA_CCR_EvtCode;

		/* clear INT status to notify HW clear EventCode*/
		*ATSMB3_PDMA_ISR |= SD_PDMA_IER_INT_STS;

		/*printk("dma_isr event code = %X\n", *ATSMB3_PDMA_CCR);*/
		/*We expect cmd with data sending back or read single block cmd run here.*/
		if (pdma_event_code == SD_PDMA_CCR_Evt_success) {
			/*means need to update the data->error and cmd->error*/
			if (host->DmaIntMask == SD_PDMA_CCR_Evt_success) {
				if (host->data != NULL) {
					host->data->error = MMC_ERR_NONE;
					host->cmd->error = MMC_ERR_NONE;
				} else {
					DBG("dma_isr3 host->data is NULL\n");
					/*disable INT*/
					*ATSMB3_PDMA_IER &= ~SD_PDMA_IER_INT_EN;
					goto out;
				}
			}
			/*else do nothing*/
			if(debug_enable)
				printk("dma_isr PDMA OK \n");
			/*atsmb3_dump_reg(host);*/
		}
		/*But unluckily, we should also be prepare for all kinds of error situation.*/
		else {
			host->data->error = -EIO; //MMC_ERR_FAILED;
			host->cmd->error = -EIO; //MMC_ERR_FAILED;
			printk("** dma_isr PDMA fail**  event code = %X\n", *ATSMB3_PDMA_CCR);
			atsmb3_dump_reg(host);
		}
        
        if (host->DmaIntMask == SD_PDMA_CCR_Evt_success)
		    atsmb3_fmt_check_rsp(host);

		/*disable INT*/
		*ATSMB3_PDMA_IER &= ~SD_PDMA_IER_INT_EN;
	}

	/*wake up some one who is sleeping.*/
	if ((pdma_event_code != SD_PDMA_CCR_Evt_success) || (host->DmaIntMask == SD_PDMA_CCR_Evt_success)) {
		if (host->done_data) {/* We only use done_data when requesting data.*/
			host->soft_timeout = 0;
			host->done(host);
		} else
			atsmb3_request_end(host, host->mrq); /*for cmd with data sending back.*/
	}

out:	
	spin_unlock(&host->lock);
    enable_irq(irq);
	DBG("[%s] e\n",__func__);
	return IRQ_HANDLED;
}

/**********************************************************************
Name  	 : atsmb3_regular_isr
Function    :.
Calls		:
Called by	:
Parameter :
Author 	 : Leo Lee
History	:
***********************************************************************/
//static irqreturn_t atsmb3_regular_isr(int irq, void *dev_id, struct pt_regs *regs)
irqreturn_t atsmb3_regular_isr(int irq, void *dev_id)
{

	struct atsmb_host *host = dev_id;
	u8 status_0, status_1, status_2, status_3,mask_0,mask_1;
	u32 pdma_sts;
	
	DBG("[%s] s\n",__func__);
	WARN_ON(host == NULL);

	disable_irq_nosync(irq);
	spin_lock(&host->lock);

	/*Get INT status*/
	status_0 = *ATSMB3_SD_STS_0;
    status_1 = *ATSMB3_SD_STS_1;
	status_2 = *ATSMB3_SD_STS_2;
	status_3 = *ATSMB3_SD_STS_3;
       
    mask_0 = *ATSMB3_INT_MASK_0;
    mask_1 = *ATSMB3_INT_MASK_1;
	/*******************************************************
						card insert interrupt
	********************************************************/
	if ((status_0 & ATSMB_DEVICE_INSERT)      /*Status Set and IRQ enabled*/
	/*To aviod the situation that we intentionally disable IRQ to do rescan.*/
		&& (*ATSMB3_INT_MASK_0 & 0x80)) {
		
		if(debug_enable)
				printk("card inset interrupt\n");	
		if (host->mmc->ops->get_slot_status(host->mmc)) {
			host->mmc->scan_retry = 3;
			host->mmc->card_scan_status = false;
		} else {
			host->mmc->scan_retry = 0;
			host->mmc->card_scan_status = false;
		}
		
		if(!isMtk6620) { //added by rubbit
			mmc_detect_change(host->mmc, HZ/2); 
		}
		
		/*Taipei Side Request: Disable INSERT IRQ when doing rescan.*/
		//*ATSMB3_INT_MASK_0 &= (~0x80);/* or 40?*/	//zhf: marked by James Tian
		/*Then we clear the INT status*/
		//iowrite32(ATSMB_DEVICE_INSERT, host->base+_SD_STS_0);
    *ATSMB3_SD_STS_0 |= ATSMB_DEVICE_INSERT;  
    spin_unlock(&host->lock);
		enable_irq(irq);
		DBG("[%s] e1\n",__func__);
		return IRQ_HANDLED;
	}
    
    if ((status_1 & mask_1)& ATSMB_SDIO_INT) {
				if(debug_enable)
						printk("sdio int:%x\n",(status_1 & mask_1));	    	
        spin_unlock(&host->lock);
        mmc_signal_sdio_irq(host->mmc);
       
        if (((status_1 & mask_1) == ATSMB_SDIO_INT) && ((status_0 & mask_0) == 0)) {
            
            enable_irq(irq);
            DBG("[%s] e2\n",__func__);

            return IRQ_HANDLED;
        }
        spin_lock(&host->lock);
    }
	pdma_sts = *ATSMB3_PDMA_CCR;
    
    if (((status_0 & mask_0) | (status_1 & mask_1)) == 0) {
        spin_unlock(&host->lock);
		enable_irq(irq);
        printk("[%s] e3\n",__func__);
        return IRQ_HANDLED;
    }
	/*******************************************************
							command interrupt.
	*******************************************************/
	/*for write single block*/
	if (host->opcode == MMC_WRITE_BLOCK) {
		if ((status_0 & ATSMB_BLOCK_XFER_DONE)
			&& (status_1 & ATSMB_RSP_DONE)) {
			host->data->error = MMC_ERR_NONE;
			host->cmd->error = MMC_ERR_NONE;
			/* okay, what we want.*/
		} else {
			host->data->error = -EIO; //MMC_ERR_FAILED;
			host->cmd->error = -EIO; //MMC_ERR_FAILED;
            printk("[%s] err1\n",__func__);
			atsmb3_dump_reg(host);
		}
	} else if (host->opcode == MMC_WRITE_MULTIPLE_BLOCK
			|| host->opcode == MMC_READ_MULTIPLE_BLOCK) {
		if ((status_1 & (ATSMB_AUTO_STOP|ATSMB_RSP_DONE))
			&& (status_0 & ATSMB_MULTI_XFER_DONE)) {
			/*If CRC error occurs, I think this INT would not occrs.*/
			/*okay, that is what we want.*/
			host->data->error = MMC_ERR_NONE;
			host->cmd->error = MMC_ERR_NONE;
		} else {
			host->data->error = -EIO; //MMC_ERR_FAILED;
			host->cmd->error = -EIO; //MMC_ERR_FAILED;
            printk("[%s] err2\n",__func__);
			atsmb3_dump_reg(host);

		}
	} else if (host->opcode == MMC_READ_SINGLE_BLOCK) {/* we want DMA TC, run here, must be error.*/
		host->data->error = -EIO; //MMC_ERR_FAILED;
		host->cmd->error = -EIO; //MMC_ERR_FAILED;
        printk("[%s] err3\n",__func__);
		atsmb3_dump_reg(host);
	} else if (host->opcode == SD_IO_RW_EXTENDED){
        /*Write operation*/
        if (*ATSMB3_CTL & BIT2) {
            if ((*ATSMB3_CTL & 0xf0) == 0x10) {   /*single block write*/
                if ((status_0 & ATSMB_BLOCK_XFER_DONE)
        		&& (status_1 & ATSMB_RSP_DONE)) {
        			host->data->error = MMC_ERR_NONE;
        			host->cmd->error = MMC_ERR_NONE;
        		/* okay, what we want.*/
        		if(debug_enable)		
							printk("sdio single block write ok\n");
        	    } else {
        			host->data->error = -EIO; //MMC_ERR_FAILED;
        			host->cmd->error = -EIO; //MMC_ERR_FAILED;
                    printk("[%s] err4 status_0 = %x status_1 = %x\n",__func__,status_0,status_1);
        	    }
                
            } else if ((*ATSMB3_CTL & 0xf0) == 0x50) {
                if ((status_0 & ATSMB_MULTI_XFER_DONE)
    			&& (status_1 & ATSMB_RSP_DONE)) {
        			host->data->error = MMC_ERR_NONE;
        			host->cmd->error = MMC_ERR_NONE;
    			/* okay, what we want.*/
    			    if(debug_enable)		
								printk("sdio multi block write ok\n");
    		    } else {
        			host->data->error = -EIO; //MMC_ERR_FAILED;
        			host->cmd->error = -EIO; //MMC_ERR_FAILED;
                    printk("[%s] err4-2 status_0 = %x status_1 = %x\n",__func__,status_0,status_1);
    		    }
            } else {
                host->data->error = -EIO; //MMC_ERR_FAILED;
    			host->cmd->error = -EIO; //MMC_ERR_FAILED;
                printk("[%s] err4-3 status_0 = %x status_1 = %x\n",__func__,status_0,status_1);
            }
        }
        else {
        /*Read operation*/
            host->data->error = -EIO; //MMC_ERR_FAILED;
		    host->cmd->error = -EIO; //MMC_ERR_FAILED;
            printk("[%s] err5\n",__func__);
			atsmb3_dump_reg(host);
        }
        
        
    } else {
/* command, not request data*/
/* the command which need data sending back,*/
/* like switch_function, send_ext_csd, send_scr, send_num_wr_blocks.*/
/* NOTICE: we also send status before reading or writing data, so SEND_STATUS should be excluded.*/
		if (host->data && host->opcode != MMC_SEND_STATUS) {
			host->data->error = -EIO; //MMC_ERR_FAILED;
			host->cmd->error = -EIO; //MMC_ERR_FAILED;
            printk("[%s] err6\n",__func__);
			atsmb3_dump_reg(host);
		} else {			/* Just command, no need data sending back.*/
			if (status_1 & ATSMB_RSP_DONE) {
				/*Firstly, check data-response is busy or not.*/
				if (host->cmd->flags == (MMC_RSP_R1B | MMC_CMD_AC)) {
					int i = 10000;
					
					while (status_2 & ATSMB_RSP_BUSY) {
						status_2 = *ATSMB3_SD_STS_2;
						if (--i == 0)
							break;
						printk(" IRQ:Status_2 = %d, busy!\n", status_2);
					}
					if (i == 0)
						printk("[MMC driver] Error :SD data-response always busy!");

				}
#if 1
/*for our host, even no card in slot, for SEND_STATUS also returns no error.*/
/*The protocol layer depends on SEND_STATUS to check whether card is in slot or not.*/
/*In fact, we can also avoid this situation by checking the response whether they are all zeros.*/
				if (!atsmb3_get_slot_status(host->mmc) && host->opcode == MMC_SEND_STATUS) {
					host->cmd->retries = 0; /* No retry.*/
//					host->cmd->error = MMC_ERR_INVALID;
					host->cmd->error = -EINVAL;
					printk("err66\n");
				} else {
#endif
				host->cmd->error = MMC_ERR_NONE;
				if(debug_enable)		
					printk("rsp done ok\n");
			}
			} else {
				if (status_1 & ATSMB_RSP_TIMEOUT) {/* RSP_Timeout .*/
//					host->cmd->error = MMC_ERR_TIMEOUT;
					host->cmd->error = -ETIMEDOUT;
                    printk("[%s] err7\n",__func__);
                } else  {/*or RSP CRC error*/
//					host->cmd->error = MMC_ERR_BADCRC;
					host->cmd->error = -EILSEQ;
                    printk("[%s] err8\n",__func__);
                }
				atsmb3_dump_reg(host);
			}
		}
	}
	atsmb3_fmt_check_rsp(host);

	/*disable INT */
	*ATSMB3_INT_MASK_0 &= ~(ATSMB_BLOCK_XFER_DONE_EN | ATSMB_MULTI_XFER_DONE_EN);
	*ATSMB3_INT_MASK_1 &= ~(ATSMB_WRITE_CRC_ERR_EN|ATSMB_READ_CRC_ERR_EN|ATSMB_RSP_CRC_ERR_EN
		|ATSMB_DATA_TIMEOUT_EN|ATSMB_AUTO_STOP_EN|ATSMB_RSP_TIMEOUT_EN|ATSMB_RSP_DONE_EN);


	/*clear INT status. In fact, we will clear again before issuing new command.*/
	*ATSMB3_SD_STS_0 |= status_0;
	*ATSMB3_SD_STS_1 |= status_1;

	if (*ATSMB3_PDMA_ISR & SD_PDMA_IER_INT_STS)
		*ATSMB3_PDMA_ISR |= SD_PDMA_IER_INT_STS;

	/*wake up some one who is sleeping.*/
	if (host->done_data) { /* We only use done_data when requesting data.*/
		host->soft_timeout = 0;
		host->done(host);
	} else
		atsmb3_request_end(host, host->mrq); /*for cmd without data.*/

	spin_unlock(&host->lock);
	enable_irq(irq);
	DBG("[%s] e4\n",__func__);
	return IRQ_HANDLED;
}
EXPORT_SYMBOL(atsmb3_regular_isr);

/**********************************************************************
Name  	 : atsmb3_get_ro
Function    :.
Calls		:
Called by	:
Parameter :
returns	: 0 : write protection is disabled. 1: write protection is enabled.
Author 	 : Leo Lee
History	:
***********************************************************************/
int atsmb3_get_ro(struct mmc_host *mmc)
{
	struct atsmb_host *host = mmc_priv(mmc);
	unsigned char status_0 = 0;
	unsigned long flags;
	unsigned long ret = 0;
	DBG("[%s] s\n",__func__);
	spin_lock_irqsave(&host->lock, flags);
	status_0 = *ATSMB3_SD_STS_0;
	spin_unlock_irqrestore(&host->lock, flags);
	DBG("[%s]\nstatus_0 = 0x%x\n", __func__,status_0);
	ret = ((status_0 & ATSMB_WRITE_PROTECT) ? 0 : 1);
	DBG("[%s] e\n",__func__);
	return ret;
}
/**********************************************************************
Name  	 : atsmb3_dump_host_regs
Function    :
Calls		:
Called by	:
Parameter :
returns	: 
Author 	 : Leo Lee
History	:
***********************************************************************/
void atsmb3_dump_host_regs(struct mmc_host *mmc)
{
	struct atsmb_host *host = mmc_priv(mmc);
	DBG("[%s] s\n",__func__);
	atsmb3_dump_reg(host);
	DBG("[%s] e\n",__func__);
}
EXPORT_SYMBOL(atsmb3_dump_host_regs);

/**********************************************************************
Name  	 : atsmb3_enable_sdio_irq
Function    :
Calls		:
Called by	:
Parameter :
returns	: 
Author 	 : Tommy Huang
History	:
***********************************************************************/
int wmt_enable_irq = 1;

static void atsmb3_enable_sdio_irq(struct mmc_host *mmc, int enable)
{
	struct atsmb_host *host = mmc_priv(mmc);
	unsigned long flags;

	DBG("[%s] s enable = %d *ATSMB3_INT_MASK_1 = %x\n",__func__,enable,*ATSMB3_INT_MASK_1);
	spin_lock_irqsave(&host->lock, flags);
    
	if (enable) {
		if(wmt_enable_irq==1)
		*ATSMB3_INT_MASK_1 |= ATSMB_SDIO_EN;
	} else {
		*ATSMB3_INT_MASK_1 &= ~ATSMB_SDIO_EN;
	}

	spin_unlock_irqrestore(&host->lock, flags);
    DBG("[%s] e\n",__func__);
    
}

void wmt_detect_sdio(void){

	if(mmc3_host_attr!=NULL){

		struct atsmb_host *host = mmc_priv(mmc3_host_attr);
		
		mmc_detect_change(host->mmc, HZ/2);
	}
}



void wmt_enable_sdio_irq(int enable)
{
	if (enable) {
		*ATSMB3_INT_MASK_1 |= ATSMB_SDIO_EN;
		wmt_enable_irq = 1;
		
	} else {
		wmt_enable_irq = 0;
		*ATSMB3_INT_MASK_1 &= ~ATSMB_SDIO_EN;
	}
}

void wmt_set_sd3_max(int rate)
{
	if(mmc3_host_attr!=NULL){

		struct atsmb_host *host = mmc_priv(mmc3_host_attr);
		host->mmc->f_max = rate;
	}
	
}
void force_remove_sdio3(void)
{
	mmc_force_remove_card(mmc3_host_attr);
}
EXPORT_SYMBOL(wmt_enable_sdio_irq);
EXPORT_SYMBOL(wmt_detect_sdio);
EXPORT_SYMBOL(wmt_set_sd3_max);
EXPORT_SYMBOL(force_remove_sdio3);


/**********************************************************************
Name  	 : atsmb3_request
Function    :.
Calls		:
Called by	:
Parameter :
Author 	 : Leo Lee
History	:
***********************************************************************/
static void atsmb3_request(struct mmc_host *mmc, struct mmc_request *mrq)
{

	struct atsmb_host *host = mmc_priv(mmc);
	DBG("[%s] s\n",__func__);
	
	/* May retry process comes here.*/
	host->mrq = mrq;
	host->data = mrq->data;
	host->cmd = mrq->cmd;
	host->done_data = NULL;
	host->done = NULL;
	if(debug_enable)
			printk("host->cmd->opcode :%x\n",host->cmd->opcode);
	/*for data request*/
	if (host->data) {
		if (host->cmd->opcode == MMC_WRITE_BLOCK
			|| host->cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK
			|| host->cmd->opcode == MMC_READ_SINGLE_BLOCK
			|| host->cmd->opcode == MMC_READ_MULTIPLE_BLOCK
			|| host->cmd->opcode == SD_IO_RW_EXTENDED) {
			atsmb3_start_data(host);
        } else {
			atsmb3_cmd_with_data_back(host);
        }     
	} else {
		atsmb3_start_cmd(host);
    }
	DBG("[%s] e\n",__func__);
}
/**********************************************************************
Name  	 : atsmb3_set_clock
Function    :.
Calls		:
Called by	:
Parameter :
Author 	 : Eason Chien
History	:2012/7/19
***********************************************************************/
static int atsmb3_set_clock(struct mmc_host *mmc, unsigned int clock)
{
	if (clock == mmc->f_min) {
        DBG("[%s]ATSMB3 Host 400KHz\n", __func__);            
 		return auto_pll_divisor(DEV_SDMMC3, SET_DIV, 1, 390);
    }
	else if (clock >= 50000000) {
        DBG("[%s]ATSMB3 Host 50MHz\n", __func__);
		return auto_pll_divisor(DEV_SDMMC3, SET_DIV, 2, 45);
	} else if ((clock >= 25000000) && (clock < 50000000)) {
		DBG("[%s]ATSMB3 Host 25MHz\n", __func__);
		return auto_pll_divisor(DEV_SDMMC3, SET_DIV, 2, 24);
	} else if ((clock >= 20000000) && (clock < 25000000)) {
		DBG("[%s]ATSMB3 Host 20MHz\n", __func__);
        return  auto_pll_divisor(DEV_SDMMC3, SET_DIV, 2, 20);
	} else {
        DBG("[%s]ATSMB3 Host 390KHz\n", __func__);
        return auto_pll_divisor(DEV_SDMMC3, SET_DIV, 1, 390);
	}
}

/**********************************************************************
Name  	 : atsmb3_set_ios
Function    :.
Calls		:
Called by	:
Parameter :
Author 	 : Leo Lee
History	:
***********************************************************************/
static void atsmb3_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{

	struct atsmb_host *host = mmc_priv(mmc);
	unsigned long flags;
	spin_lock_irqsave(&host->lock, flags);

	if (ios->power_mode == MMC_POWER_OFF) {
		if (MMC3_DRIVER_VERSION == MMC_DRV_3481) {
            /* stop SD output clock */
			*ATSMB3_BUS_MODE &= ~(ATSMB_CST);

            /*  disable SD Card power  */			
			/*set SD3 power pin as GPO pin*/
            if (SD3_function == SDIO_WIFI) {
            		if(!isMtk6620)
                GPIO_OC_GP0_BYTE_VAL |= BIT6;
            } else {
                GPIO_CTRL_GP18_SDWRSW_BYTE_VAL |= GPIO_SD3_POWER;
                GPIO_OC_GP18_SDWRSW_BYTE_VAL |= GPIO_SD3_POWER;
            }
            
			/*set internal pull up*/
            GPIO_PULL_CTRL_GP18_SDWRSW_BYTE_VAL |= GPIO_SD3_POWER;
			/*set internal pull enable*/
            GPIO_PULL_EN_GP18_SDWRSW_BYTE_VAL |= GPIO_SD3_POWER;
			/*disable SD3 power*/
            if (SD3_function == SDIO_WIFI) {
            		if(!isMtk6620)
                GPIO_OD_GP0_BYTE_VAL &= ~BIT6;
            } else {
			    		GPIO_OD_GP18_SDWRSW_BYTE_VAL |= GPIO_SD3_POWER;
			    	}

			/* Disable Pull up/down resister of SD Bus */
			GPIO_PULL_CTRL_GP13_SD3_BYTE_VAL &=  ~(GPIO_SD3_Data | GPIO_SD3_WriteProtect | GPIO_SD3_Command | GPIO_SD3_Clock);

			/*  Config SD3 to GPIO  */
            GPIO_CTRL_GP13_SD3_BYTE_VAL |= (GPIO_SD3_Data | GPIO_SD3_WriteProtect | GPIO_SD3_Command | GPIO_SD3_Clock);
				
			/*  SD3 all pins output low  */
			GPIO_OD_GP13_SD3_BYTE_VAL &= ~(GPIO_SD3_Data | GPIO_SD3_WriteProtect | GPIO_SD3_Command | GPIO_SD3_Clock);
			
			/*  Config SD3 to GPIO  */
			GPIO_OC_GP13_SD3_BYTE_VAL |= (GPIO_SD3_Data | GPIO_SD3_WriteProtect | GPIO_SD3_Command | GPIO_SD3_Clock);
		                  
        }

	} else if (ios->power_mode == MMC_POWER_UP) {
		if (MMC3_DRIVER_VERSION == MMC_DRV_3481) {
            /*  disable SD Card power  */
            /*set SD3 power pin as GPO pin*/
            if (SD3_function == SDIO_WIFI) {
            		if(!isMtk6620)
                GPIO_OC_GP0_BYTE_VAL |= BIT6;
          } else {
			    	GPIO_CTRL_GP18_SDWRSW_BYTE_VAL |= GPIO_SD3_POWER;
			    	GPIO_OC_GP18_SDWRSW_BYTE_VAL |= GPIO_SD3_POWER;
          }
          
       /*set internal pull up*/
			GPIO_PULL_CTRL_GP18_SDWRSW_BYTE_VAL |= GPIO_SD3_POWER;
			/*set internal pull enable*/
			GPIO_PULL_EN_GP18_SDWRSW_BYTE_VAL |= GPIO_SD3_POWER;
			/*disable SD3 power*/
            if (SD3_function == SDIO_WIFI) {
            		if(!isMtk6620)
                GPIO_OD_GP0_BYTE_VAL &= ~BIT6;
            } else {
			    			GPIO_OD_GP18_SDWRSW_BYTE_VAL |= GPIO_SD3_POWER;
			    	}
                 				

            /*  Config SD PIN share  */
			GPIO_PIN_SHARING_SEL_4BYTE_VAL &= ~GPIO_SD3_PinShare;

            /* do not config GPIO_SD0_CD because ISR has already run,
			 * config card detect will issue ISR storm.
			 */
			/*  Config SD to GPIO  */
			GPIO_CTRL_GP13_SD3_BYTE_VAL |= (GPIO_SD3_Data | GPIO_SD3_WriteProtect | GPIO_SD3_Command | GPIO_SD3_Clock);
			/*  SD all pins output low  */
			GPIO_OD_GP13_SD3_BYTE_VAL &= ~(GPIO_SD3_Data | GPIO_SD3_WriteProtect | GPIO_SD3_Command | GPIO_SD3_Clock);
			/*  Config SD to GPO   */
			GPIO_OC_GP13_SD3_BYTE_VAL |= (GPIO_SD3_Data | GPIO_SD3_WriteProtect | GPIO_SD3_Command | GPIO_SD3_Clock);
			
            
            
			/* Pull up/down resister of SD Bus */
           /*Disable Clock & CMD Pull enable*/
			GPIO_PULL_EN_GP13_SD3_BYTE_VAL &= ~(GPIO_SD3_Clock | GPIO_SD3_Command);
			
			/*Set CD ,WP ,DATA pin pull up*/
            if (SD3_function == SDIO_WIFI) {
                GPIO_PULL_CTRL_GP21_I2C_BYTE_VAL &= ~GPIO_SD3_CD; /*pull down card always in slot*/
                GPIO_PULL_CTRL_GP13_SD3_BYTE_VAL |= GPIO_SD3_Data;
				GPIO_PULL_CTRL_GP13_SD3_BYTE_VAL &= ~GPIO_SD3_WriteProtect; /*pull down R/W able*/
            } else {
            	GPIO_PULL_CTRL_GP21_I2C_BYTE_VAL |= GPIO_SD3_CD;
			    GPIO_PULL_CTRL_GP13_SD3_BYTE_VAL |= (GPIO_SD3_Data | GPIO_SD3_WriteProtect);
            }
			
            
			/*Enable CD ,WP ,DATA  internal pull*/
			GPIO_PULL_EN_GP21_I2C_BYTE_VAL |=  GPIO_SD3_CD;
			GPIO_PULL_EN_GP13_SD3_BYTE_VAL |= (GPIO_SD3_Data | GPIO_SD3_WriteProtect);			
            
			spin_unlock_irqrestore(&host->lock, flags);
			msleep(100);
			spin_lock_irqsave(&host->lock, flags);

			/*  enable SD3 Card Power  */
            if (SD3_function == SDIO_WIFI) {
            		if(!isMtk6620)
                	GPIO_OD_GP0_BYTE_VAL |= BIT6;
            } else {    
			    		GPIO_OD_GP18_SDWRSW_BYTE_VAL &= ~GPIO_SD3_POWER; 
			    	}
            

            /* enable SD3 output clock */
			*ATSMB3_BUS_MODE |= ATSMB_CST;
			
			spin_unlock_irqrestore(&host->lock, flags);
			msleep(10);
			spin_lock_irqsave(&host->lock, flags);

			/* Config SD3 back to function  */
			GPIO_CTRL_GP21_I2C_BYTE_VAL &= ~ GPIO_SD3_CD;
			GPIO_CTRL_GP13_SD3_BYTE_VAL &= ~(GPIO_SD3_Data | GPIO_SD3_WriteProtect | GPIO_SD3_Command | GPIO_SD3_Clock);

        }
	} else {
		/*nothing to do when powering on.*/
	}

	host->current_clock = atsmb3_set_clock(mmc,ios->clock);
/*    if (ios->clock == mmc->f_min) {
 		host->current_clock = auto_pll_divisor(DEV_SDMMC3, SET_DIV, 1, 390);
        DBG("[%s]ATSMB3 Host 400KHz\n", __func__);            
    }
	else if (ios->clock >= 50000000) {
		host->current_clock = auto_pll_divisor(DEV_SDMMC3, SET_DIV, 2, 45);
        DBG("[%s]ATSMB3 Host 50MHz\n", __func__);
	} else if ((ios->clock >= 25000000) && (ios->clock < 50000000)) {
        host->current_clock = auto_pll_divisor(DEV_SDMMC3, SET_DIV, 2, 24);
        DBG("[%s]ATSMB3 Host 25MHz\n", __func__);
	} else if ((ios->clock >= 20000000) && (ios->clock < 25000000)) {
        host->current_clock = auto_pll_divisor(DEV_SDMMC3, SET_DIV, 2, 20);
        DBG("[%s]ATSMB3 Host 20MHz\n", __func__);
	} else {
        host->current_clock = auto_pll_divisor(DEV_SDMMC3, SET_DIV, 1, 390);
        DBG("[%s]ATSMB3 Host 390KHz\n", __func__);
	}
*/    
	if (ios->bus_width == MMC_BUS_WIDTH_8) {
		*ATSMB3_EXT_CTL |= (0x04);
	} else if (ios->bus_width == MMC_BUS_WIDTH_4) {
		*ATSMB3_BUS_MODE |= ATSMB_BUS_WIDTH_4;
		*ATSMB3_EXT_CTL &= ~(0x04);
	} else {
		*ATSMB3_BUS_MODE &= ~(ATSMB_BUS_WIDTH_4);
		*ATSMB3_EXT_CTL &= ~(0x04);
	}
#if 1
	if (ios->timing == MMC_TIMING_SD_HS)
		*ATSMB3_EXT_CTL |= 0x80; /*HIGH SPEED MODE*/
	else
		*ATSMB3_EXT_CTL &= ~(0x80);
#endif
#if 0	//zhf: marked by James Tian
	if (ios->ins_en == MMC_INSERT_IRQ_EN) 
        *ATSMB3_INT_MASK_0 |= (0x80);/* or 40?*/
	else
        *ATSMB3_INT_MASK_0 &= (~0x80);/* or 40?*/        
#endif

	spin_unlock_irqrestore(&host->lock, flags);
}


static const struct mmc_host_ops atsmb3_ops = {
	.request	= atsmb3_request,
	.set_ios	= atsmb3_set_ios,
	.get_ro	= atsmb3_get_ro,
	.get_slot_status = atsmb3_get_slot_status,
	.dump_host_regs = atsmb3_dump_host_regs,
	.enable_sdio_irq	= atsmb3_enable_sdio_irq,
};

extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);

/**********************************************************************
Name  	 :atsmb3_probe
Function    :.
Calls		:
Called by	:
Parameter :
Author 	 : Leo Lee
History	:
***********************************************************************/
static int __init atsmb3_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mmc_host *mmc_host  = NULL;
	struct atsmb_host *atsmb_host = NULL;
	struct resource *resource = NULL;
	int  irq[2] = {0};
	int ret = 0;

	DBG("[%s] s\n",__func__);
	if (!pdev) {
		ret = -EINVAL; /* Invalid argument */
		goto the_end;
	}

	/*Enable SD host clock*/
    auto_pll_divisor(DEV_SDMMC3, CLK_ENABLE, 0, 0);
    
    if (MMC3_DRIVER_VERSION == MMC_DRV_3481) {
		/* Pull up/down resister of SD CD */
        if (SD3_function != SDIO_WIFI) {
		    GPIO_PULL_CTRL_GP21_I2C_BYTE_VAL |= GPIO_SD3_CD; /*pull up CD*/
			GPIO_PULL_EN_GP21_I2C_BYTE_VAL |= GPIO_SD3_CD;
			/* config CardDetect pin to SD function */
	    	GPIO_CTRL_GP21_I2C_BYTE_VAL &= ~GPIO_SD3_CD;
		}
	}
	
	
    resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!resource) {
		ret = -ENXIO;	/* No such device or address */
		printk(KERN_ALERT "[MMC/SD driver] Getting platform resources failed!\n");
		goto the_end;
	}
#if 0
	if (!request_mem_region(resource->start, SZ_1K, MMC3_DRIVER_NAME)) {
		ret = -EBUSY;
		printk(KERN_ALERT "[MMC/SD driver] Request memory region failed!\n");
		goto the_end ;
	}
#endif
	irq[0] = platform_get_irq(pdev, 0);	/*get IRQ for device*/;
	irq[1] = platform_get_irq(pdev, 1);	/*get IRQ for dma*/;

    if (irq[0] == NO_IRQ || irq[1] == NO_IRQ) {
		ret = -ENXIO;/* No such device or address */
		printk(KERN_ALERT "[MMC/SD driver] Get platform IRQ failed!\n");
		goto rls_region;
	}

	/*allocate a standard msp_host structure attached with a atsmb structure*/
	mmc_host = mmc_alloc_host(sizeof(struct atsmb_host), dev);
	if (!mmc_host) {
		ret = -ENOMEM;
		printk(KERN_ALERT "[MMC/SD driver] Allocating driver's data failed!\n");
		goto rls_region;
	}
	mmc_host->wmt_host_index = 3; /*to identify host number*/
	
	dev_set_drvdata(dev, (void *)mmc_host); /* mmc_host is driver data for the atsmb dev.*/
	atsmb_host = mmc_priv(mmc_host);

	mmc_host->ops = &atsmb3_ops;

	mmc_host->ocr_avail = MMC_VDD_32_33|MMC_VDD_33_34;

	mmc_host->f_min = 390425; /*390.425Hz = 400MHz/64/16*/
	mmc_host->f_max = 50000000; /* in fact, the max frequency is 400MHz( = 400MHz/1/1)*/
	mmc_host->caps = MMC_CAP_4_BIT_DATA | MMC_CAP_SD_HIGHSPEED | MMC_CAP_MMC_HIGHSPEED | MMC_CAP_SDIO_IRQ	
                    |MMC_CAP_NONREMOVABLE;/* |MMC_CAP_8_BIT_DATA;*/	//zhf: marked by James Tian
	/* keep power while host suspended 1:keep power, 0:otherwise 
	added by Eason 2012/09/21*/
	mmc_host->pm_caps |= MMC_PM_KEEP_POWER;	
	mmc_host->pm_caps |= MMC_PM_WAKE_SDIO_IRQ;//add by rubbitxiao for mtk6620

	mmc_host->max_segs = 128;	/*we use software sg. so we could manage even larger number.*/
	
	/*1MB per each request */
	/*we have a 16 bit block number register, and block length is 512 bytes.*/
	mmc_host->max_req_size = 16*512*(mmc_host->max_segs);
	mmc_host->max_seg_size = 65024; /* 0x7F*512 PDMA one descriptor can transfer 64K-1 byte*/
	mmc_host->max_blk_size = 2048;	/* our block length register is 11 bits.*/
	mmc_host->max_blk_count = (mmc_host->max_req_size)/512;

	/*set the specified host -- ATSMB*/
#ifdef CONFIG_MMC_UNSAFE_RESUME
	mmc_host->card_attath_status = card_attach_status_unchange;
#endif
    sema_init(&mmc_host->req_sema,1); /*initial request semaphore*/
#if 0
	atsmb_host->base = ioremap(resource->start, SZ_1K);    
	if (!atsmb_host->base) {
		printk(KERN_ALERT "[MMC/SD driver] IO remap failed!\n");
		ret = -ENOMEM;
		goto fr_host;
	}
#endif
    atsmb_host->base = (void *)resource->start;
	atsmb_host->mmc = mmc_host;
	spin_lock_init(&atsmb_host->lock);
	atsmb_host->res = resource;/* for atsmb3_remove*/

	/*disable all interrupt and clear status by resetting controller.*/
	*ATSMB3_BUS_MODE |= ATSMB_SFTRST;
	*ATSMB3_BLK_LEN &= ~(0xa000);
	*ATSMB3_SD_STS_0 |= 0xff;
	*ATSMB3_SD_STS_1 |= 0xff;

	/* WM3437 A0 default not output clock, after SFTRST need to enable SD clock */
	//if (MMC3_DRIVER_VERSION >= MMC_DRV_3437_A0) /* including 3429 */
   	*ATSMB3_BUS_MODE |= ATSMB_CST;

	atsmb_host->regular_irq = irq[0];
	atsmb_host->dma_irq = irq[1];

    ret = request_irq(atsmb_host->regular_irq,
					atsmb3_regular_isr,
					IRQF_SHARED,			//SA_SHIRQ, /*SA_INTERRUPT, * that is okay?*/	//zhf: modified by James Tian, should be IRQF_SHARED?
					MMC3_DRIVER_NAME,
					(void *)atsmb_host);
	if (ret) {
		printk(KERN_ALERT "[MMC/SD driver] Failed to register regular ISR!\n");
		goto unmap;
	}

	ret = request_irq(atsmb_host->dma_irq,
					atsmb3_dma_isr,
					IRQF_DISABLED,	//	SA_INTERRUPT,  //zhf: modified by James Tian
					MMC3_DRIVER_NAME,
					(void *)atsmb_host);
	if (ret) {
		printk(KERN_ALERT "[MMC/SD driver] Failed to register DMA ISR!\n");
		goto fr_regular_isr;
	}


    /*wait card detect status change*/
    //msleep(10);
	
	/*enable card insertion interrupt and enable DMA and its Global INT*/
	*ATSMB3_BLK_LEN |= (0xa000); /* also, we enable GPIO to detect card.*/
	*ATSMB3_SD_STS_0 |= 0xff;
	//*ATSMB3_INT_MASK_0 |= 0x80; /*or 0x40?*/ move Register HOST there, marded by Eason

	/*allocation dma descriptor*/
	ret = atsmb3_alloc_desc(atsmb_host, sizeof(struct SD_PDMA_DESC_S) * MAX_DESC_NUM);
	if (ret == -1) {
		printk(KERN_ALERT "[MMC/SD driver] Failed to allocate DMA descriptor!\n");
			goto fr_dma_isr;
	}
	printk(KERN_INFO "WMT ATSMB3 (AHB To SD/MMC3 Bus) controller registered!\n");

	/*Register HOST, power control by framework config*/
    if (SD3_function == SDIO_WIFI) {
    //comm_wlan_power_on(1);
		mmc_add_host(mmc_host,false);
		*ATSMB3_INT_MASK_0 &= ~0x80; /*if wifi and disable Slot device insertion*/
	}else {
     mmc_add_host(mmc_host,true);
		*ATSMB3_INT_MASK_0 |= 0x80; /*if SD/MMC and enable Slot device insertion*/
	}

    mmc3_host_attr = mmc_host;
	DBG("[%s] e1\n",__func__);
	return 0;

fr_dma_isr:
	free_irq(atsmb_host->dma_irq, atsmb_host);
fr_regular_isr:
	free_irq(atsmb_host->regular_irq, atsmb_host);
unmap:
	//iounmap(atsmb_host->base);
//fr_host:
	dev_set_drvdata(dev, NULL);
	mmc_free_host(mmc_host);
rls_region:
	//release_mem_region(resource->start, SZ_1K);
the_end:
	printk(KERN_ALERT "[MMC/SD driver] ATSMB3 probe Failed!\n") ;
	DBG("[%s] e2\n",__func__);
	return ret;
}
/**********************************************************************
Name  	 : atsmb3_remove
Function    :.
Calls		:
Called by	:
Parameter :
Author 	 : Leo Lee
History	:
***********************************************************************/
static int atsmb3_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mmc_host *mmc_host = (struct mmc_host *)dev_get_drvdata(dev);
	struct atsmb_host *atsmb_host;

	DBG("[%s] s\n",__func__);
	atsmb_host = mmc_priv(mmc_host);
	if (!mmc_host || !atsmb_host) {
		printk(KERN_ALERT "[MMC/SD driver] ATSMB3 remove method failed!\n");
		DBG("[%s] e1\n",__func__);
		return -ENXIO;
	}
	mmc_remove_host(mmc_host);

	/*disable interrupt by resetting controller -- for safey*/
	*ATSMB3_BUS_MODE |= ATSMB_SFTRST;
	*ATSMB3_BLK_LEN &= ~(0xa000);
	*ATSMB3_SD_STS_0 |= 0xff;
	*ATSMB3_SD_STS_1 |= 0xff;

	(void)free_irq(atsmb_host->regular_irq, atsmb_host);
	(void)free_irq(atsmb_host->dma_irq, atsmb_host);
	(void)iounmap(atsmb_host->base);
	(void)release_mem_region(atsmb_host->res->start, SZ_1K);
	dev_set_drvdata(dev, NULL);
	/*free dma descriptor*/
	dma_free_coherent(atsmb_host->mmc->parent, atsmb_host->DescSize,
		atsmb_host->DescVirAddr, atsmb_host->DescPhyAddr);
	(void)mmc_free_host(mmc_host);/* also free atsmb_host.*/
	DBG("[%s] e2\n",__func__);
	return 0;
}

/**********************************************************************
Name  	 : atsmb3_shutdown
Function    :.
Calls		:
Called by	:
Parameter :
Author 	 : Tommy Huang
History	:
***********************************************************************/
static void atsmb3_shutdown(struct platform_device *pdev)
{
	/*atsmb3_shutdown don't be used now.*/
	/*struct device *dev = &pdev->dev;
	struct mmc_host *mmc_host = (struct mmc_host *)dev_get_drvdata(dev);*/

	DBG("[%s] s\n",__func__);
    if (SD3_function != SDIO_WIFI) {
	    /*Disable card detect interrupt*/
	    *ATSMB3_INT_MASK_0 &= ~0x80;
    }
	DBG("[%s] e\n",__func__);
	
}

/**********************************************************************
Name  	 : atsmb3_suspend
Function    :.
Calls		:
Called by	:
Parameter :
Author 	 : Leo Lee
History	:
***********************************************************************/
int SD3_card_state = 1; /*0: card remove >=1: card enable*/

typedef enum {
	LOW_WAKEUP = 0,
	HIGH_WAKEUP = 1,
	FALLING_WAKEUP = 2,
	RISING_WAKEUP = 3,
	BOTH_EDGE_WAKEUP = 4,
	WAKEUP_TYPE_MAX
}WAKE_TYPE;

typedef enum {
	WAKEUP0 = 0x00,
	WAKEUP1 = 0x01,
	WAKEUP2 = 0x02,
	WAKEUP3 = 0x03,
	WAKEUP4 = 0x04,
	WAKEUP5 = 0x05,
	WAKEUP6 = 0x06,
	WAKEUP7 = 0x07,
	WAKEUPMAX
}WAKEUP_NUM;


/*
*		wakenum parameter range is: 0~7
*		0 coressponding to wakeup0, 1 to wakeup1, 2 to wakeup2
*/
pmc_wakeup_enable(unsigned int wakenum,WAKE_TYPE type)
{
		/*clear corresponding type field*/
		PMWT_VAL &= (~(0xf<<(wakenum*4)));
		/*set corresponding type field*/
		PMWT_VAL |= (type<<(wakenum*4));
		/*enable wakeup event*/
		PMWE_VAL |= 1<<wakenum;
}

pmc_wakeup_disable(unsigned int wakenum)
{
		/*disable corresponding wakeup enable bit*/
		PMWE_VAL &= ~(1<<wakenum);
}

#ifdef CONFIG_PM
static int atsmb3_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct device *dev = &pdev->dev;
	struct mmc_host *mmc = (struct mmc_host *)dev_get_drvdata(dev);
	int ret = 0;
	DBG("[%s] s\n",__func__);

	if (mmc) {
		
		if (SD3_function == SDIO_WIFI) {
			if (SD3_card_state > 0) {
				/*enable wakeup event in suspend mode*/
				//pmc_enable_wakeup_event(8,0000);
				//pmc_wakeup_enable(0x01,LOW_WAKEUP);
				/*If enter suspend in power on, kepp power*/
				mmc->pm_flags |= MMC_PM_KEEP_POWER;	
			}
		}
		ret = mmc_suspend_host(mmc);
		if (ret == 0) {
			/*disable all interrupt and clear status by resetting controller. */
			*ATSMB3_BUS_MODE |= ATSMB_SFTRST;
			*ATSMB3_BLK_LEN &= ~(0xa000);
			*ATSMB3_SD_STS_0 |= 0xff;
			*ATSMB3_SD_STS_1 |= 0xff;
		} else {
			printk("%s: mmc_suspend_host fail, ret = %d\n", __func__, ret);
			return ret;
		}
		/*disable source clock*/
		auto_pll_divisor(DEV_SDMMC3, CLK_DISABLE, 0, 0);
		//debug_enable = 0x01;
#ifdef CONFIG_MMC_UNSAFE_RESUME
		/*clean SD card attatch status change*/
		//PMCWS_VAL |= BIT19;
		mmc->card_attath_status = card_attach_status_unchange;
#endif
	}

	DBG("[%s] e\n",__func__);
	return ret;
}
/**********************************************************************
Name  	 : atsmb3_resume
Function    :.
Calls		:
Called by	:
Parameter :
Author 	 : Leo Lee
History	:
***********************************************************************/
static int atsmb3_resume(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mmc_host *mmc = (struct mmc_host *)dev_get_drvdata(dev);
	int ret = 0, i = 0;
	DBG("[%s] s\n",__func__);

	/*
	 * enable interrupt, DMA, etc.
	 * Supply power to slot.
	 */
	if (mmc) {
		/*enable source clock*/
		auto_pll_divisor(DEV_SDMMC3, CLK_ENABLE, 0, 0);
		msleep(10);  //add by rubbit
    udelay(1);
		/*enable card insertion interrupt and enable DMA and its Global INT*/
		*ATSMB3_BUS_MODE |= ATSMB_SFTRST;
		*ATSMB3_BLK_LEN |= (0xa000);
		*ATSMB3_INT_MASK_0 |= 0x80;	/* or 40?*/
#ifdef CONFIG_MMC_UNSAFE_RESUME
		/*modify SD card attatch status change*/
		//if ((PMCWS_VAL & BIT19) && !mmc->bus_dead) {
			/*card change when suspend mode*/
		mmc->card_attath_status = card_attach_status_change;
			/*clean SD card attatch status change*/
		//	PMCWS_VAL |= BIT19;
		//}
#endif
        if (SD3_function == SDIO_WIFI) {
			if (SD3_card_state > 0) {
				/*disable wakeup event in normal node*/
				//pmc_disable_wakeup_event(8);
				//pmc_wakeup_disable(0x01);
				
					
				// Initial SD bus {
				/* SD Bus */
		        GPIO_PULL_CTRL_GP13_SD3_BYTE_VAL |= GPIO_SD3_Data | GPIO_SD3_Command;
				GPIO_PULL_CTRL_GP13_SD3_BYTE_VAL &= ~GPIO_SD3_WriteProtect;
				GPIO_PULL_EN_GP13_SD3_BYTE_VAL |= (GPIO_SD3_Data | GPIO_SD3_WriteProtect);	
				GPIO_PULL_EN_GP13_SD3_BYTE_VAL &= ~GPIO_SD3_Clock;
				GPIO_CTRL_GP13_SD3_BYTE_VAL &= ~(GPIO_SD3_Data | GPIO_SD3_WriteProtect | GPIO_SD3_Command | GPIO_SD3_Clock);

				/* WiFi Out-band INT */
				//GPIO_PULL_CTRL_GP22_UART_0_1_BYTE_VAL |= BIT6;
				//GPIO_PULL_EN_GP22_UART_0_1_BYTE_VAL |= BIT6;
				//GPIO_OC_GP22_UART_0_1_BYTE_VAL &= ~BIT6;
				//GPIO_CTRL_GP22_UART_0_1_BYTE_VAL |= BIT6;
				//UART1CTS_INT_REQ_TYPE_VAL = 0; //Low level trigger
				//GPIO_PIN_SHARING_SEL_4BYTE_VAL &= ~BIT14; //Select to UART1CTS

				/* Enable SD3 clock */
				*ATSMB3_BUS_MODE |= ATSMB_CST;						
				msleep(10);
				msleep(10);
				//} Initial SD bus
				printk("[%s] mmc_resume_host s\n",__func__);
				mmc_detect_change(mmc3_host_attr, 1*HZ/2); //check again by eason 2012/3/27
				printk("[%s] mmc_resume_host e\n",__func__);
				while (!mmc3_host_attr->card_scan_status) {
					msleep(10);
					i++;
					if (i>100) {
						printk("%s, Scan timeout!!!\n",__func__);
						break;	
					}
				}
			}
        }

	    ret = mmc_resume_host(mmc);
	}

	DBG("[%s] e\n",__func__);
	return ret;
}
#else
#define atsmb3_suspend	NULL
#define atsmb3_resume	NULL
#endif

static struct platform_driver atsmb3_driver = {
	.driver.name		= "sdmmc3",
	//.probe		= atsmb3_probe,
	.remove		= atsmb3_remove,
	.shutdown	= atsmb3_shutdown,
	.suspend	= atsmb3_suspend,
	.resume		= atsmb3_resume,
};

static struct platform_device wmt_sdmmc3_device = {
	.name			= "sdmmc3",
	.id				= 0,
	.dev            = {
	.dma_mask 		= &wmt_sdmmc3_dma_mask,
	.coherent_dma_mask = ~0,
	.release = atsmb3_release,
	},
	.num_resources  = ARRAY_SIZE(wmt_sdmmc3_resources),
	.resource       = wmt_sdmmc3_resources,
};

static DEFINE_MUTEX(SD3_state_lock);

static ssize_t atsmb3_state_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
    int ret = 0;
    
    DBG("[%s] s\n",__func__);

	ret = sprintf(buf, "%d\n", SD3_card_state);
	DBG("[%s]SD3_card_state = %d\n",__func__,SD3_card_state);
    DBG("[%s] e\n",__func__);
	return ret;
}

static ssize_t atsmb3_state_store(struct kobject *kobj, struct kobj_attribute *attr,
	       const char *buf, size_t n)
{
	int val;
    DBG("[%s] s\n",__func__);
	
	mutex_lock(&SD3_state_lock);
	if (sscanf(buf, "%d", &val) == 1) {
        DBG("[%s] val = %d\n",__func__,val);
        if (val == 1) {
			SD3_card_state++;
			DBG("[%s] add state SD3_card_state = %d\n",__func__,SD3_card_state);
			if ((SD3_card_state == 1) && (mmc3_host_attr->card == NULL)) {
				DBG("[%s]add card\n",__func__);
            	mmc_detect_change(mmc3_host_attr, 0);
            	msleep(1000);
			} else {
				msleep(1000); /*to prevent the card not init ready*/
			}
        } else if (val == 0) {
			SD3_card_state--;
			printk("[%s] sub state SD3_card_state = %d\n",__func__,SD3_card_state);
			if (SD3_card_state < 0) {
				SD3_card_state = 0;
				printk("[%s] set 0 SD3_card_state = %d\n",__func__,SD3_card_state);
			}
			
			if ((SD3_card_state == 0) && (mmc3_host_attr->card != NULL)) {
            	DBG("[%s]remove card\n",__func__);
            	mmc_force_remove_card(mmc3_host_attr);
			}
        }
		
        DBG("[%s] e1\n",__func__);
		mutex_unlock(&SD3_state_lock);
        return n;
	}
	
	mutex_unlock(&SD3_state_lock);
    DBG("[%s] e2\n",__func__);
	return -EINVAL;
}

static struct kobj_attribute atsmb3_state_attr = {	\
	.attr	= {				\
		.name = __stringify(state),	\
		.mode = 0755,			\
	},					\
	.show	= atsmb3_state_show,			\
	.store	= atsmb3_state_store,		\
};

static struct attribute * g3[] = {
	&atsmb3_state_attr.attr,
	NULL,
};

static struct attribute_group attr3_group = {
	.attrs = g3,
};

static int __init atsmb3_init(void)
{
	int ret;

	int retval;
	unsigned char buf[80];
	unsigned char * tmp,*token;
	int varlen = 80;
	char *varname = "wmt.sd3.param";	
	int sd_enable = 0; /*0 :disable 1:enable*/
	
	DBG("[%s] s\n",__func__);

#ifdef CONFIG_MTD_WMT_SF
	/*Read system param to identify host function 0: SD/MMC 1:SDIO wifi*/
    retval = wmt_getsyspara(varname, buf, &varlen);
	if (retval == 0) {
		sscanf(buf,"%d:%d", &sd_enable,&SD3_function);
		printk(KERN_ALERT "wmt.sd3.param = %d:%d\n", sd_enable,SD3_function);
		if (SD3_function < 0)
			return -ENODEV;
	} else {
        printk(KERN_ALERT "Default wmt.sd3.param = %d:%d\n", sd_enable,SD3_function);
    }

    retval = wmt_getsyspara("wmt.init.rc", buf, &varlen);
	if (retval == 0) {
			tmp = buf;
			while(token = strsep(&tmp,":")) {
  	  if(!strcmp(token,"mtk6620")) {
 						printk("mainBoard have mtk6620 module\n");
						isMtk6620 = 0x001; 
						break; 	  	
  	  }
  	}
	} else {
        printk(KERN_ALERT "do not have set uboot enviroment: wmt.init.rc\n");
    }    
    
#endif
	/*SD function disable*/
	if (sd_enable == 0)
		return -ENODEV;
	
	get_driver_version3();
	
	if (platform_device_register(&wmt_sdmmc3_device))//add by jay,for modules support
		return -1;
	//ret = platform_driver_register(&atsmb3_driver);
	ret = platform_driver_probe(&atsmb3_driver, atsmb3_probe);

    atsmb3_kobj = kobject_create_and_add("mmc3", NULL);
	if (!atsmb3_kobj)
		return -ENOMEM;
	return sysfs_create_group(atsmb3_kobj, &attr3_group);
    
	DBG("[%s] e\n",__func__);
	return ret;
}

static void __exit atsmb3_exit(void)
{
	DBG("[%s] s\n",__func__);
	(void)platform_driver_unregister(&atsmb3_driver);
	(void)platform_device_unregister(&wmt_sdmmc3_device);//add by jay,for modules support
	DBG("[%s] e\n",__func__);
}


module_init(atsmb3_init);
module_exit(atsmb3_exit);
module_param(fmax3, uint, 0444);

MODULE_AUTHOR("WonderMedia Technologies, Inc.");
MODULE_DESCRIPTION("WMT [AHB to SD/MMC3 Bridge] driver");
MODULE_LICENSE("GPL");
