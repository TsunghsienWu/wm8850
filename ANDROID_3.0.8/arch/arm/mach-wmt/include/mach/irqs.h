/*
 *	linux/include/asm-arm/arch-wmt/irqs.h
 *
 *	Copyright (c) 2008 WonderMedia Technologies, Inc.
 *
 *	This program is free software: you can redistribute it and/or modify it under the
 *	terms of the GNU General Public License as published by the Free Software Foundation,
 *	either version 2 of the License, or (at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful, but WITHOUT
 *	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 *	PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *	You should have received a copy of the GNU General Public License along with
 *	this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *	WonderMedia Technologies, Inc.
 *	10F, 529, Chung-Cheng Road, Hsin-Tien, Taipei 231, R.O.C.
 */

#ifndef __ASM_ARCH_IRQS_H
#define __ASM_ARCH_IRQS_H

/*
 *
 *  Interrupt sources.
 *
 */
/*** Interrupt Controller 0 ***/
/* #define IRQ_REVERSED      0		*/
#define IRQ_SDC1             1		/* SD Host controller 0             */
#define IRQ_SDC1_DMA         2
/* IRQ_REVERSED              3      */
#define IRQ_PMC_AXI_PWR      4
#define IRQ_GPIO             5
/* IRQ_REVERSED              6      */
#define IRQ_I2C2             7
#define IRQ_KPAD             8
#define IRQ_VDMA             9
#define	IRQ_ETH0             10
#define IRQ_SDC2             11
#define IRQ_SDC2_DMA         12
#define IRQ_SDC3             13
#define IRQ_SDC3_DMA         14
#define IRQ_I2C3             15
#define IRQ_APBB             16
#define IRQ_DMA_CH_0         17
#define	IRQ_I2C1             18      /* I2C controller                   */
#define	IRQ_I2C0             19      /* I2C controller                   */
#define	IRQ_SDC0             20      /* SD Host controller 1             */
#define	IRQ_SDC0_DMA         21
#define	IRQ_PMC_WAKEUP       22      /* PMC wakeup                       */
#define IRQ_PCM              23
#define	IRQ_SPI0             24      /* Serial Peripheral Interface 0    */
#define	IRQ_SPI1             25
#define	IRQ_UHDC             26
#define IRQ_DMA_CH_1         27
#define	IRQ_NFC              28
#define	IRQ_NFC_DMA          29
#define IRQ_EBMC             30
/* IRQ_REVERSED      31     */
#define	IRQ_UART0            32
#define	IRQ_UART1            33
#define IRQ_DMA_CH_2         34
#define	IRQ_OST7             35      /* OS Timer match 7               	 */
#define	IRQ_OST0             36      /* OS Timer match 0               	 */
#define	IRQ_OST1             37      /* OS Timer match 1               	 */
#define	IRQ_OST2             38      /* OS Timer match 2                 */
#define	IRQ_OST3             39      /* OS Timer match 3               	 */
#define IRQ_DMA_CH_3         40
#define IRQ_DMA_CH_4         41
#define	IRQ_OST4             42
#define	IRQ_OST5             43
#define	IRQ_OST6             44
#define IRQ_DMA_CH_5         45
#define IRQ_DMA_CH_6         46
#define	IRQ_UART2            47
#define	IRQ_RTC1             48      /* RTC_PCLK_INTR              	 */
#define	IRQ_RTC2             49      /* RTC_PCLK_RTI              	 */
#define	IRQ_UART3            50
#define IRQ_DMA_CH_7         51
#define IRQ_PMC_MDM_RDY      52
#define IRQ_PMC_EBM_WAKEREQ  53
#define IRQ_PMC_MDM_WAKE_AP  54
#define	IRQ_CIR              55
#define IRQ_IC1_IRQ0         56
#define IRQ_IC1_IRQ1         57
#define IRQ_IC1_IRQ2         58
#define IRQ_IC1_IRQ3         59
#define IRQ_IC1_IRQ4         60
#define IRQ_IC1_IRQ5         61
#define IRQ_IC1_IRQ6         62
#define IRQ_IC1_IRQ7         63
/*** Interrupt Controller 1 ***/
#define IRQ_JDEC             64		/* Video Decode Unit			*/
/* #define IRQ_TSIN          65 */	/* Transport Stream INput		*/
#define IRQ_SAE              66		/* Cypher						*/
#define IRQ_VPP_IRQ0         67		/* SCL_INISH_INT				*/
#define IRQ_VPP_IRQ1         68		/* SCL_INIT						*/
#define IRQ_VPP_IRQ2         69		/* SCL444_TG_INT				*/
#define IRQ_MSVD             70		
#define IRQ_DZ_0             71		/*AUDPRF*/
#define IRQ_DZ_1             72
#define IRQ_DZ_2             73
#define IRQ_DZ_3             74
#define IRQ_DZ_4             75
#define IRQ_DZ_5             76
#define IRQ_DZ_6             77
#define IRQ_DZ_7             78
#define IRQ_VPP_IRQ3         79		/* VPP_INT						*/
#define IRQ_VPP_IRQ4         80		/* GOVW_TG_INT					*/
#define IRQ_VPP_IRQ5         81		/* GOVW_INT						*/
#define IRQ_VPP_IRQ6         82		/* GOV_INT						*/
#define IRQ_VPP_IRQ7         83		/* GE_INT						*/
#define IRQ_VPP_IRQ8         84		/* GOVRHD_TG_INT				*/
#define IRQ_VPP_IRQ9         85		/* DVO_INT						*/
#define IRQ_VPP_IRQ10        86		/* VID_INT						*/
#define IRQ_H264             87
#define IRQ_VP8DEC           88
#define IRQ_MALI_PMU         89
#define IRQ_MALI_GPMMU       90
#define IRQ_VPP_IRQ25        91
#define IRQ_DMA_CH_8         92
#define IRQ_DMA_CH_9         93
#define IRQ_DMA_CH_10        94
#define IRQ_DMA_CH_11        95
#define IRQ_DMA_CH_12        96
#define IRQ_DMA_CH_13        97
#define IRQ_DMA_CH_14        98
#define IRQ_DMA_CH_15        99
#define IRQ_MALI_GP          100
#define IRQ_MALI_PPMMU0      101
#define IRQ_MALI_PP0         102
#define IRQ_VPP_IRQ19        103
#define IRQ_VPP_IRQ20        104
#define IRQ_L220_L2          105
#define IRQ_VPP_IRQ21        106
#define IRQ_VPP_IRQ22        107
#define IRQ_VPP_IRQ23        108
#define IRQ_VPP_IRQ24        109
#define IRQ_DZ_8             110
#define IRQ_VPP_IRQ11        111	/* GOVR_INT							*/
#define IRQ_VPP_IRQ12        112	/* GOVRSD_TG_INT					*/
#define IRQ_VPP_IRQ13        113	/* VPU_INT							*/
#define IRQ_VPP_IRQ14        114	/* VPU_TG_INT						*/
#define IRQ_VPP_IRQ15        115	/* unused							*/
#define IRQ_VPP_IRQ16        116	/* NA12								*/
#define IRQ_VPP_IRQ17        117	/* NA12								*/
#define IRQ_VPP_IRQ18        118	/* NA12								*/
#define IRQ_L220_ECNTR       119
#define IRQ_L220_PARRT       120
#define IRQ_L220_PARRD       121
#define IRQ_L220_ERRWT       122
#define IRQ_L220_ERRWD       123
#define IRQ_L220_ERRRT       124
#define IRQ_L220_ERRRD       125
#define IRQ_L220_SLVERR      126
#define IRQ_L220_DECERR      127
#define IRQ_I2S              IRQ_DZ_4

#define NR_IRQS 128

#endif
