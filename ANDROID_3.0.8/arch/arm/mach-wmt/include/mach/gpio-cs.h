/*++
	linux/arch/arm/mach-wmt/gpio_cfg.h

	Some descriptions of such software. Copyright (c) 2008  WonderMedia Technologies, Inc.

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

#ifndef _GPIO_CS_H_
#define _GPIO_CS_H_
#include <mach/hardware.h>


#define GPIO_ID_BASE 		(__GPIO_BASE)
#define GPIO_EN_BASE 		(__GPIO_BASE+0x40)
#define GPIO_OC_BASE 		(__GPIO_BASE+0x80)
#define GPIO_OD_BASE 		(__GPIO_BASE+0xc0)

#define GPIO_IRQ_REQ_BASE 	(__GPIO_BASE+0x0300)
#define GPIO_IRQ_STS_BASE 	(__GPIO_BASE+0x0360)

#define GPIO_PULL_EN_BASE  	(__GPIO_BASE+0x480)
#define GPIO_PULL_CTRL_BASE	(__GPIO_BASE+0x4c0)

enum GPIO_OUTPUT_T{
	LOW		=0,
	HIGH	=1,
};

enum GPIO_MODE_T{
	INPUT	=0,
	OUTPUT	=1
};

enum GPIO_IRQ_T{
	IRQ_LOW        =0x00,            /* Input zero generate GPIO_IRQ signal */
	IRQ_HIGH       =0x01,            /* Input one generate GPIO_IRQ signal */
	IRQ_FALLING    =0x02,            /* Falling edge generate GPIO_IRQ signal */
	IRQ_RISING     =0x03,            /* Rising edge generate GPIO_IRQ signal */
	IRQ_BOTHEDGE   =0x04,	     	  /* Both edge generate GPIO_IRQ signal */
	IRQ_NULL       =0x05,
};

enum GPIO_SET_T{
	OP_EN     	=0,
	OP_DIS   	=1,
	OP_SET		=2,
	OP_CHK		=3,
};

enum GPIO_PULL_T{
	PULL_DOWN	=0,
	PULL_UP	 	=1,
};

enum GPIO_T{
	
 //IRQ
 GP0,//[7:0] GPIO[7:0]
 
 GP1,//[5:0] GPIO[13:8]
 
 //IRQ
 GP2,//[6:5] SUS_GPIO[1:0],
     //[3:2] WAKEUP/SUS_GPIO[3:2], 
     //[1:0] WAKEUP[1:0]
 
 GP3,//[4] SD0CD
 
 GP4,//[7:0] VDOUT[7:0]
 
 GP5,//[7:0] VDOUT[15:8]
 
 GP6,//[7:0] VDOUT[23:16]
 
 GP7,//[3] VDCLK
 	 //[2] VDVSYNC
 	 //[1] VDHSYNC
 	 //[0] VDDEN
 	 
 GP8,//[7:0] VDIN[7:0]
 
 GP9,//[2] VCLK
     //[1] VVSYNC
     //[0] VHSYNC
     
 //IRQ
 GP10,//[7] I2SDACMCLK 
 	  //[6] I2SDACBCLK
 	  //[5] I2SDACLRC
 	  //[4] I2SADCMCLK/I2SADCDAT1/PCMOUT
 	  //[3] I2SDACDAT3/PCMIN
 	  //[2] I2SDACDAT2/I2SADCLRC/PCMCLK
 	  //[1] I2SDACDAT1/I2SADCBCLK/PCMSYNC
 	  //[0] I2SDACDAT0
 
 //IRQ
 GP11,//[3] SPI0SS 
      //[2] SPI0MOSI
      //[1] SPI0MISO
      //[0] SPI0CLK
      
 GP12,//
 
 GP13,//[7] SD3DATA[3]
      //[6] SD3DATA[2]
      //[5] SD3DATA[1]
      //[4] SD3DATA[0]
      //[3] SD3WP/EBM_DATA[6]
      //[2] SD3CMD
      //[1] SD3CLK
      //[0] RESERVED
 
 GP14,//[7] SD0DATA[3]
      //[6] SD0DATA[2]
      //[5] SD0DATA[1]
      //[4] SD0DATA[0]
      //[3] SD0WP
      //[2] SD0CMD
      //[1] SD0CLK
      //[0] RESERVED
      
 GP15,//[7] NANDIO[7]/SD1DATA[3]
      //[6] NANDIO[6]/SD1DATA[0]
      //[5] NANDIO[5]/SD1DATA[4]
      //[4] NANDIO[4]/SD1DATA[1]
      //[3] NANDIO[3]/SD1DATA[5]
      //[2] NANDIO[2]/SD1DATA[2]
      //[1] NANDIO[1]/SD1DATA[6]
      //[0] NANDIO[0]/SD1DATA[7]
       
 GP16,//RESERVED
 
 GP17,//[7] MII0RXERR/EBM_A[19]
      //[6] MII0RXDV/EBM_A[20]
      //[5] MII0RXD3/EBM_BLE
      //[4] MII0RXD2/EBM_BHE
      //[3] MII0RXD1/EBM_DATA[14]
      //[2] MII0RXD0/EBM_DATA[9]
      //[1] PHY25MHZ/AP_RDY
      //[0] MII0RXCLK/EBM_DATA[3]
      
 GP18,//[6] SD3PWRSW/EBM_DATA[11]
      //[5] SD0PWRSW
      
 GP19,//[7] MII0CRS/EBM_DATA[7]
      //[6] MII0COL/EBM_A[21]
      //[5] MII0TXEN/EBM_CLK
      //[4] MII0TXD3/EBM_DATA[8]
      //[3] MII0TXD2/EBM_DATA[12]
      //[2] MII0TXD1/EBM_CS
      //[1] MII0TXD0/EBM_WR
      //[0] MII0TXCLK/AP_WAKE_MDM
      
 GP20,//[3] MII0PHYPD/EBM_DATA[2]
      //[2] MII0PHYRST/EBM_ADV
      //[1] MII0MDC/EBM_RDY
      //[0] MII0MDIO/EBM_OE 
      
 GP21,//[7] RESERVED 
      //[6] I2C3SCL/SD2CD
      //[5] I2C2SDA
      //[4] I2C2SCL
      //[3] I2C1SDA
      //[2] I2C1SCL
      //[1] I2C0SDA
      //[0] I2C0SCL
      
 //IRQ
 GP22,//[7] UART1RXD
      //[6] UART1CTS/UART0CTS
      //[5] UART1TXD
      //[4] UART1RTS/UART0RTS
      //[3] UART0RXD
      //[2] 
      //[1] UART0TXD
      //[0] 
      
 GP23,//[7] UART3RXD/UART2CTS/SPI1MOSI 
      //[6] 
      //[5] UART3TXD/UART2RTS/SPI1CLK
      //[4] 
      //[3] UART2RXD/SPI1SS0
      //[2] 
      //[1] UART2TXD/SPI1MISO
      //[0] 
      
 GP24,//[7] SD2CLK/KPADCOL7
      //[6] SD2CMD/KPADCOL6
      //[5] SD2DATA0/KPADCOL5
      //[4] SD2DATA1/KPADCOL4
      //[3] SD2WP/KPADROW6/EBM_DATA[0]
      //[2] SD2DATA3/KPADROW5
      //[1] KPADROW4
      //[0] 
      
 GP25,//[1] SD2PWRSW/KPADROW7/EBM_DATA[13] 
       
 //IRQ
 GP26,//[7] SD2DATA2/KPADCOL3
      //[6] KPADCOL2
      //[5] KPADCOL1
      //[4] KPADCOL0
      //[3] KPADROW3
      //[2] KPADROW2
      //[1] KPADROW1
      //[0] KPADROW0
 
 GP27,//[2] I2SADCDAT0 
      //[1] SPDIFO/I2SADCDAT3
      //[0] PCMMCLK/I2SADCDAT2
      
 GP28,//[2] NANDDQS
      //[1] NANDCLE/SD1CMD
      //[0] NANDALE/SD1CLK
      
 GP29,//[7] NANDCE[1]
      //[6] NANDCE[0]
      //[5] NANDRB[1]
      //[4] NANDRB[0]
      //[3] NANDWPD/SD1WP
      //[2] NANDWP/SD1PWRSW
      //[1] NANDRE/SD1RSTN
      //[0] NANDWE
       
 GP30,//[4] SFCLK
      //[3] SFCS[1]
      //[2] SFCS[0]
      //[1] SFDO
      //[0] SFDI
      
 //IRQ      
 GP31,//[3] PWMOUT0
      //[2] C25MHZCLKI/PWMOUT1/SPI0SS3/EBM_A[25]
      //[1] C24MHZCLKI/SPI0SS2/EBM_DATA[1]
      //[0] CLKOUT/EBM_A[22]
      
 GP32,//[3] HDMICEC
      //[2] HDMIHPD
      //[1] HDMIDDCSCL
      //[0] HDMIDDCSSDA
       
 GP48=0x30,
      //[7] GPIO23/EBM_A[16]
      //[6] GPIO22/EBM_DATA[10]
      //[5] GPIO21/EBM_A[17]
      //[4] GPIO20/EBM_A[24]
      //[3:0] RESERVED
       
 GP49,//[3] GPIO27/EBM_A[23]
      //[2] GPIO26/EBM_DATA[15]
      //[1] GPIO25/EBM_DATA[5]
      //[0] GPIO24/EBM_DATA[4]

 GP60=0x3c,
      //[5] USBOC3
      //[4] USBOC2
      //[3] USBOC1
      //[2] USBOC0
      //[1] USBATTA0
      //[0] USBSW0
 GP_MAX,

};

/*
 * mode=0 input, mode =1 output
 */
int gpio_enable(enum GPIO_T gpio_idx, int bit, enum GPIO_MODE_T mode);
int gpio_disable(enum GPIO_T gpio_idx, int bit);

/*
 * mode =0 pull down, mode = 1 pull up
 */
int gpio_pull_enable(enum GPIO_T gpio_idx, int bit,enum GPIO_PULL_T mode);
int gpio_pull_disable(enum GPIO_T gpio_idx, int bit);

/*
 * mode=0 low level 
 * mode=1 high level 
 * mode=2 falling edge 
 * mode=3 rising edge 
 * mode=4 both edge 
 */
int gpio_setup_irq(enum GPIO_T gpio_idx, int bit,enum GPIO_IRQ_T mode);
int gpio_enable_irq(enum GPIO_T gpio_idx, int bit);
int gpio_disable_irq(enum GPIO_T gpio_idx, int bit);

/*
 * Check GPIO Interrupt  Enabled
 * \retval: zero interrupt is disabled, others interrupt is enabled 
 */
int gpio_irq_isenable(enum GPIO_T gpio_idx, int bit);
/*
 * Read GPIO Irq Status Register
 * return val: 0 no interrupt, others interrupt status is set
 */
int gpio_irq_state(enum GPIO_T gpio_idx, int bit);
int gpio_irq_clean(enum GPIO_T gpio_idx, int bit);

/*
 * Read GPIO Input Register
 * return value: 0 low, 1 high
 */
int gpio_get_value(enum GPIO_T gpio_idx, int bit);

/*
 * Set GPIO Output Register
 * value = LOW set logic low,value=HIGH set logic high
 */
int gpio_set_value(enum GPIO_T gpio_idx, int bit, enum GPIO_OUTPUT_T value);



#endif




