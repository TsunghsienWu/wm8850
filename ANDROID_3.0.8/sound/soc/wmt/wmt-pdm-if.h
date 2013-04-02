/*++
linux/include/asm-arm/arch-wmt/wmt_pcm.h

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

/* Be sure that virtual mapping is defined right */

#ifndef __WMT_PDM_IF_H
#define __WMT_PDM_IF_H

/*
 * Address
 */
#define PCM_CR_ADDR                     (0x0000+PCM_BASE_ADDR)
#define PCM_SR_ADDR                     (0x0004+PCM_BASE_ADDR)
/* Reserved 0x0008 ~ 0x000F */
#define PCM_DFCR_ADDR                   (0x0008+PCM_BASE_ADDR)
#define PCM_DIVR_ADDR                   (0x000C+PCM_BASE_ADDR)
/* Reserved 0x0020 ~ 0x007F */
#define PCM_TFIFO_ADDR                  (0x0010+PCM_BASE_ADDR)
#define PCM_TFIFO_1_ADDR                (0x0014+PCM_BASE_ADDR)

#define PCM_RFIFO_ADDR                  (0x0030+PCM_BASE_ADDR)
#define PCM_RFIFO_1_ADDR                (0x0034+PCM_BASE_ADDR)
/* Reserved 0x0100 ~ 0xFFFF */

/*
 * Value
 */
#define PCMCR_VAL       (REG32_VAL(PCM_CR_ADDR))
#define PCMSR_VAL       (REG32_VAL(PCM_SR_ADDR))
#define PCMDFCR_VAL     (REG32_VAL(PCM_DFCR_ADDR))
#define PCMDIVR_VAL     (REG32_VAL(PCM_DIVR_ADDR))

#define PCMCLK_DIV_MASK			0x7FF
#define PLL_B_DIVF_MASK			0xFF0000
#define PLL_B_DIVR_MASK			0x1F00
#define PLL_B_DIVQ_MASK			0x07
#define OSC_25M					25000
#define PCMCLK_128K				128
#define PCMCLK_256K				256


//=============================================================================
//
// PCMCR 	PCM Control Register
//
//=============================================================================
#define PCMCR_PCM_ENABLE    (BIT0)		/* PCM Interface Enable	*/
#define PCMCR_SLAVE         (BIT1)		/* Master/Slave Mode */

#define PCMCR_BCLK_SEL      (BIT3)		/* PCM_CLK Select */
#define PCMCR_SYNC_MODE     (BIT4)		/* Frame Sync Mode: 0-long, 1-Short */
#define PCMCR_DMA_EN        (BIT5)		/* DMA Enable */
#define PCMCR_SYNC_OFF      (BIT6)		/* Sync Disable */
#define PCMCR_MUTE          (BIT7)		/* Mute Enable */
#define PCMCR_IRQ_EN        (BIT8)		/* Interrupt Enable */
#define PCMCR_DMA_IRQ_SEL   (BIT9)		/* Threshold DMA/IRQ Select */
#define PCMCR_RXFE_EN       (BIT10)		/* RX FIFO Empty Interrupt Enable */
#define PCMCR_RXFF_EN       (BIT11)		/* RX FIFO Full Interrupt Enable */
#define PCMCR_RXOVR_EN      (BIT12)		/* RX FIFO Overrun Interrupt Enable */
#define PCMCR_TXFE_EN       (BIT13)		/* TX FIFO Empty Interrupt Enable */
#define PCMCR_TXUND_EN      (BIT14)		/* TX FIFO Underrun Interrupt Enable */

#define PCMCR_TXFF_RST      (BIT24)		/* TX FIFO Reset */
#define PCMCR_RXFF_RST      (BIT25)		/* RX FIFO Reset */


//=============================================================================
//
// PCMSR 	PCM Status Register
//
//=============================================================================
#define PCMSR_RXFE          (BIT0)		/* RX FIFO Empty Status */
#define PCMSR_RXFF          (BIT1)		/* RX FIFO Full Status */
#define PCMSR_RXOVR         (BIT2)		/* RX FIFO Overrun Status */
#define PCMSR_TXFE          (BIT3)		/* TX FIFO Empty Status */
#define PCMSR_TXUND         (BIT4)		/* TX FIFO Underrun Status */
#define PCMSR_TXFAE         (BIT5)		/* TX FIFO Almost Empty Status */
#define PCMSR_RXFAF         (BIT6)		/* RX FIFO Almost Full Status */


//=============================================================================
//
// PCMDFCR 	PCM Data Format Control Register
//
//=============================================================================
#define PCMDFCR_TXFM_EN     (BIT0)		/* TX Data Format Enable */
#define PCMDFCR_TX_SZ_13    0x00		/* TX Input Data Size 13 bits wide */
#define PCMDFCR_TX_SZ_14    0x02		/* TX Input Data Size 14 bits wide */
#define PCMDFCR_TX_SZ_08    0x04		/* TX Input Data Size 8 bits wide */
#define PCMDFCR_WR_AL       (BIT3)		/* TX Write Data Alignment Setup */
#define PCMDFCR_TX_AL       (BIT4)		/* PCM_OUT Alignment Control */
#define PCMDFCR_TX_PAD      (BIT5)		/* PCM_OUT Padding Control */
#define PCMDFCR_TX_MSB      (BIT6)		/* PCM_OUT First Bit Select */

#define PCMDFCR_RXFM_EN     (BIT8)		/* RX Data Format Enable */
#define PCMDFCR_RX_SZ_13    0x0000		/* RCM_IN Data Size 13 bits wide */
#define PCMDFCR_RX_SZ_14    0x0200		/* RCM_IN Data Size 14 bits wide */
#define PCMDFCR_RX_SZ_08    0x0400		/* RCM_IN Data Size 8 bits wide */
#define PCMDFCR_RX_AL       (BIT11)		/* RCM_IN Data Alignment Setup */
#define PCMDFCR_RD_AL       (BIT12)		/* PX FIFO Alignment Control */
#define PCMDFCR_RX_PAD      (BIT13)		/* PX FIFO Padding Control */
#define PCMDFCR_RX_MSB      (BIT14)		/* RX FIFO First Bit Select */	


#endif /* __WMT_PDM_IF_H */
