/*++
linux/arch/arm/mach-wmt/wmt_clk.c

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

#include <linux/mtd/mtd.h>
#include "wmt_clk.h"
#include <mach/hardware.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/cpufreq.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>

#include <asm/system.h>
#include <asm/pgtable.h>
#include <asm/mach/map.h>
#include <asm/irq.h>
#include <asm/sizes.h>
#include <linux/i2c.h>
#include <asm/div64.h>

#include "generic.h"

#define PMC_BASE PM_CTRL_BASE_ADDR
#define PMC_PLL (PM_CTRL_BASE_ADDR + 0x200)
#define PMC_CLK (PM_CTRL_BASE_ADDR + 0x250)

#define PLL_BUSY 0x7F0038
#define MAX_DF ((1<<7) - 1)
#define MAX_DR 1 //((1<<5) - 1)
#define MAX_DQ ((1<<2) - 1)
#define GET_DIVF(d) ((((d>>16)&MAX_DF)+1)*2)
#define GET_DIVR(d) (((d>>8)&MAX_DR)+1)
#define GET_DIVQ(d) (1<<(d&MAX_DQ))

#define SRC_FREQ25 25
#define SRC_FREQ24 24
#define SRC_FREQ SRC_FREQ24
/*#define debug_clk*/
static int dev_en_count[128] = {0};

static void check_PLL_DIV_busy(void)
{
	while ((*(volatile unsigned int *)(PMC_BASE+0x18))&PLL_BUSY)
		;
}

#ifdef debug_clk
static void print_refer_count(void)
{
	int i;
	for (i = 0; i < 4; i++) {
		printk("clk cnt %d ~ %d: %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",i*16,(i*16)+15,
		dev_en_count[i*16],dev_en_count[i*16+1],dev_en_count[i*16+2],dev_en_count[i*16+3],dev_en_count[i*16+4],dev_en_count[i*16+5],dev_en_count[i*16+6],dev_en_count[i*16+7],
		dev_en_count[i*16+8],dev_en_count[i*16+9],dev_en_count[i*16+10],dev_en_count[i*16+11],dev_en_count[i*16+12],dev_en_count[i*16+13],dev_en_count[i*16+14],dev_en_count[i*16+15]);
	}
}
#endif

static int disable_dev_clk(enum dev_id dev)
{
	int en_count;

	if (dev >= 128) {
		printk(KERN_INFO"device dev_id = %d > 128\n",dev);
		return -1;
	}

	#ifdef debug_clk
	print_refer_count();
	#endif

	en_count = dev_en_count[dev];
	if (en_count <= 1) {
		dev_en_count[dev] = en_count = 0;
		*(volatile unsigned int *)(PMC_CLK + 4*(dev/32))
		&= ~(1 << (dev - 32*(dev/32)));
		/*printk(KERN_INFO" really disable clock\n");*/
	} else if (en_count > 1) {
		dev_en_count[dev] = (--en_count);
	}

	#ifdef debug_clk
	print_refer_count();
	#endif

	return en_count;
}

static int enable_dev_clk(enum dev_id dev)
{
	int en_count, tmp;

	if (dev > 128) {
		printk(KERN_INFO"device dev_id = %d > 128\n",dev);
		return -1;
	}

	#ifdef debug_clk
	printk(KERN_INFO"device dev_id = %d\n",dev);
	print_refer_count();
	#endif

	en_count = dev_en_count[dev];
	tmp = *(volatile unsigned int *)(PMC_CLK + 4*(dev/32));
	if (en_count <= 0 || !(tmp&(1 << (dev - 32*(dev/32))))) {
		dev_en_count[dev] = en_count = 1;
		*(volatile unsigned int *)(PMC_CLK + 4*(dev/32))
		|= 1 << (dev - 32*(dev/32));
		/*printk(KERN_INFO" really enable clock\n");*/
	} else
		dev_en_count[dev] = (++en_count);

	#ifdef debug_clk
	print_refer_count();
	#endif

	return en_count;
}
/*
* PLLA return 0, PLLB return 1,
* PLLC return 2, PLLD return 3,
* PLLE return 4, PLLF return 5,
* PLLG return 6,
* device not found return -1.
* device has no divisor but has clock enable return -2.
*/
static int calc_pll_num(enum dev_id dev, int *div_offset)
{
	switch (dev) {
		case DEV_UART0:
		case DEV_UART1:
		case DEV_UART2:
		case DEV_UART3:
		case DEV_UART4:
		case DEV_UART5:
		case DEV_CIR:
		case DEV_VP8DEC:
		*div_offset = 0;
		return -2;
		case DEV_I2C0:
		*div_offset = 0x3A0;
		return 1;
		case DEV_I2C1:
		*div_offset = 0x3A4;
		return 1;
		case DEV_I2C2:
		*div_offset = 0x3A8;
		return 1;
		case DEV_I2C3:
		*div_offset = 0x3AC;
		return 1;
		case DEV_PWM:
		*div_offset = 0x350;
		return 1;
		case DEV_SPI0:
		*div_offset = 0x340;
		return 1;
		case DEV_SPI1:
		*div_offset = 0x344;
		return 1;
		case DEV_HDMILVDS:
		case DEV_SDTV:
		*div_offset = 0x36C;
		return 2;
		case DEV_DVO:
		*div_offset = 0x370;
		return 6;
		case DEV_EBM:
		*div_offset = 0x38C;
		return 3;
		case DEV_SCC:
		case DEV_JDEC:
		case DEV_MSVD:
		case DEV_AMP:
		case DEV_VPU:
		case DEV_MBOX:
		case DEV_GE:
		case DEV_GOVRHD:
		case DEV_KEYPAD:
		case DEV_RTC:
		case DEV_GPIO:
		*div_offset = 0;
		return -2;
		case DEV_MALI:
		*div_offset = 0x388;
		return 1;
		case DEV_DDRMC:
		*div_offset = 0x310;
		return 3;
		case DEV_NA0:
		*div_offset = 0x358;
		return 3;
		case DEV_NA12:
		case DEV_VPP:
		*div_offset = 0x35C;
		return 1;
		case DEV_L2C:
		*div_offset = 0x30C;
		return 0;
		case DEV_L2CAXI:
		*div_offset = 0x3B0;
		return 0;
		case DEV_L2CPAXI:
		*div_offset = 0x3C0;
		return 0;
		case DEV_ARF:
		case DEV_ARFP:
		case DEV_DMA:
		//case DEV_ROT:
		case DEV_UHDC:
		case DEV_PERM:
		case DEV_PDMA:
		case DEV_VDMA:
		*div_offset = 0;
		return -2;
		case DEV_VDU:
		*div_offset = 0x368;
		return 1;
		case DEV_NAND:
		*div_offset = 0x318;
		return 1;
		case DEV_NOR:
		*div_offset = 0x31C;
		return 1;
		case DEV_SDMMC0:
		*div_offset = 0x330;
		return 1;
		case DEV_SDMMC1:
		*div_offset = 0x334;
		return 1;
		case DEV_SDMMC2:
		*div_offset = 0x338;
		return 1;
		case DEV_SDMMC3:
		*div_offset = 0x33C;
		return 1;
		case DEV_ETHMAC:
		*div_offset = 0x394;
		return 1;
		case DEV_SF:
		*div_offset = 0x314;
		return 1;
		case DEV_PCM:
		*div_offset = 0x354;
		return 1;
		case DEV_I2S:
		*div_offset = 0x374;
		return 4;
		case DEV_HDMI:
		case DEV_SCL444U:
		case DEV_GOVW:
		case DEV_VID:
		case DEV_SAE:
		*div_offset = 0;
		return -2;
		case DEV_ARM:
		*div_offset = 0x300;
			return 0;
		case DEV_AHB:
		*div_offset = 0x304;
			return 1;
		case DEV_APB:
		*div_offset = 0x320;
			return 1;
		case DEV_ETHPHY:
		*div_offset = 25;
			return -2;
		default:
		return -1;
	}
}
/*
static int check_filter(int filter)
{
	if (filter >= 166) 
		return 7;
	else if (filter >= 104)
		return 6;
	else if (filter >= 65)
		return 5;
	else if (filter >= 42)
		return 4;
	else if (filter >= 26)
		return 3;
	else if (filter >= 16)
		return 2;
	else if (filter >= 10)
		return 1;
	else// if (filter >= 0)
		return 0;
}
*/
static int get_freq(enum dev_id dev, int *divisor) {

	unsigned int div = 0, pmc_clk_en, freq;
	int PLL_NO, j = 0, div_addr_offs;
	unsigned long long freq_llu, base = 1000000, tmp, mod;

	PLL_NO = calc_pll_num(dev, &div_addr_offs);
	if (PLL_NO == -1) {
		printk(KERN_INFO"device not found");
		return -1;
	}

	/* if the clk of the dev is not enable, then enable it */
	if (dev < 128) {
		pmc_clk_en = *(volatile unsigned int *)(PMC_CLK + 4*(dev/32));
		if (!(pmc_clk_en & (1 << (dev - 32*(dev/32))))) {
			/*enable_dev_clk(dev);
			printk(KERN_INFO"device clock is disabled");
			return -1;*/
		}
	}
	if (PLL_NO == -2)
		return div_addr_offs*1000000;

	if (dev == DEV_SDTV)
		*(volatile unsigned char *)(PMC_BASE + div_addr_offs + 2) = 1;
	else
		*(volatile unsigned char *)(PMC_BASE + div_addr_offs + 2) = 0;

	check_PLL_DIV_busy();

	div = *divisor = *(volatile unsigned char *)(PMC_BASE + div_addr_offs);
	//printk(KERN_INFO"div_addr_offs=0x%x PLL_NO=%d PMC_PLL=0x%x\n", div_addr_offs, PLL_NO, PMC_PLL);
	tmp = *(volatile unsigned int *)(PMC_PLL + 4*PLL_NO);
	
	if ((dev != DEV_SDMMC0) && (dev != DEV_SDMMC1) && (dev != DEV_SDMMC2) && (dev != DEV_SDMMC3)) {
		div = div&0x1F;
	} else {
		if (div & (1<<5))
			j = 1;
		div &= 0x1F;
		div = div*(j?64:1)*2;
	}

	freq_llu = (SRC_FREQ*GET_DIVF(tmp)) * base;
	//printk(KERN_INFO" freq_llu =%llu\n", freq_llu);
	tmp = GET_DIVR(tmp) * GET_DIVQ(tmp) * div;
	mod = do_div(freq_llu, tmp);
	freq = (unsigned int)freq_llu;
	//printk("get_freq cmd: freq=%d freq(unsigned long long)=%llu div=%llu mod=%llu\n", freq, freq_llu, tmp, mod);

	return freq;
}

static int set_divisor(enum dev_id dev, int unit, int freq, int *divisor) {

	unsigned int div = 0, pmc_clk_en, tmp;
	int PLL_NO, i, j = 0, div_addr_offs, SD_MMC = 1;
	unsigned long long base = 1000000, PLL, PLL_tmp, mod, div_llu, freq_target = freq, mini,tmp1;
	if ((dev != DEV_SDMMC0) && (dev != DEV_SDMMC1) && (dev != DEV_SDMMC2) && (dev != DEV_SDMMC3))
		SD_MMC = 0;

	PLL_NO = calc_pll_num(dev, &div_addr_offs);
	if (PLL_NO == -1) {
		printk(KERN_INFO"device not found");
		return -1;
	}

	if (PLL_NO == -2) {
		printk(KERN_INFO"device has no divisor");
		return -1;
	}

	if (dev == DEV_SDTV)
		*(volatile unsigned char *)(PMC_BASE + div_addr_offs + 2) = 1;
	else
		*(volatile unsigned char *)(PMC_BASE + div_addr_offs + 2) = 0;

	check_PLL_DIV_busy();

	tmp = *(volatile unsigned int *)(PMC_PLL + 4*PLL_NO);
	PLL = SRC_FREQ*GET_DIVF(tmp)*base;
	//printk(KERN_INFO" PLL =%llu, dev_id=%d, PLL_NO=%d, PMC_PLL=0x%x, tmp = 0x%x\n", PLL, dev, PLL_NO,PMC_PLL,(unsigned int)&tmp);
	div_llu = GET_DIVR(tmp)*GET_DIVQ(tmp);

	/* if the clk of the dev is not enable, then enable it */
	if (dev < 128) {
		pmc_clk_en = *(volatile unsigned int *)(PMC_CLK + 4*(dev/32));
		if (!(pmc_clk_en & (1 << (dev - 32*(dev/32))))) {
			/*enable_dev_clk(dev);*/
			printk(KERN_INFO"device clock is disabled");
			return -1;
		}
	}

	if (unit == 1)
		freq_target *= 1000;
	else if (unit == 2)
		freq_target *= 1000000;

	if (SD_MMC == 0) {
		for (i = 1; i < 33; i++) {
			if ((i > 1 && (i%2)) && (PLL_NO == 2))
				continue;
				PLL_tmp = PLL;
				mod = do_div(PLL_tmp, div_llu*i);
			if (PLL_tmp <= freq_target) {
				*divisor = div = i;
				break;
			}
		}
	} else {
		mini = freq_target;
		tmp1 = PLL;
		do_div(tmp1, div_llu);
		do_div(tmp1, 64);
		if ((tmp1) >= freq_target)
				j = 1;
		for (i = 1; i < 33; i++) {
			PLL_tmp = PLL;
			mod = do_div(PLL_tmp, div_llu*(i*(j?64:1)*2));
		if (PLL_tmp <= freq_target) {
			*divisor = div = i;
				break;
			}
		}
	}
	if (div != 0 && div < 33) {
		check_PLL_DIV_busy();
		if (dev == DEV_NA0)
			*(volatile unsigned int *)(PMC_BASE + div_addr_offs) =	(0x200 | ((div == 32) ? 0 : div));
		else
			*(volatile unsigned int *)(PMC_BASE + div_addr_offs)
			=	((dev == DEV_SDTV)?0x10000:0) + (j?32:0) + ((div == 32) ? 0 : div);
		check_PLL_DIV_busy();
		freq = (unsigned int)PLL_tmp;
		/*printk("set_divisor: freq=%d freq(unsigned long long)=%llu div=%llu mod=%llu divisor%d pmc_pll=0x%x\n",
		freq, PLL_tmp, div_llu, mod, *divisor, tmp);*/
		return freq;
	}
	printk(KERN_INFO"no suitable divisor");
	return -1;
}

static int set_pll_speed(enum dev_id dev, int unit, int freq, int *divisor) {

	unsigned int PLL, DIVF=1, DIVR=1, DIVQ=1;
	unsigned int last_freq, div=1/*, filt_range, filter*/;
	unsigned long long minor, min_minor = 0xFF000000;
	int PLL_NO, div_addr_offs, DF, DR, VD, DQ;
	unsigned long long base = 1000000, base_f = 1, PLL_llu, div_llu, freq_llu, mod;
	PLL_NO = calc_pll_num(dev, &div_addr_offs);
	if (PLL_NO == -1) {
		printk(KERN_INFO"device not belong to PLL A B C D E");
		return -1;
	}

	if (PLL_NO == -2) {
		printk(KERN_INFO"device has no divisor");
		return -1;
	}

	if (dev == DEV_SDTV)
		*(volatile unsigned char *)(PMC_BASE + div_addr_offs + 2) = 1;
	else
		*(volatile unsigned char *)(PMC_BASE + div_addr_offs + 2) = 0;

	check_PLL_DIV_busy();
	VD = *divisor = *(volatile unsigned char *)(PMC_BASE + div_addr_offs);

	if (unit == 1)
		base_f = 1000;
	else if (unit == 2)
		base_f = 1000000;
	else if (unit != 0) {
		printk(KERN_INFO"unit is out of range");
		return -1;
	}
	freq_llu = freq * base_f;
	for (DR = MAX_DR; DR >= 0; DR--) {
		for (DQ = MAX_DQ; DQ >= 0; DQ--) {
			for (DF = 0; DF <= MAX_DF; DF++) {
				if (SRC_FREQ/(DR+1) < 10)
					break;
				if ((800/SRC_FREQ) > ((2*(DF+1))/(DR+1)))
					continue;
				if ((1600/SRC_FREQ) < ((2*(DF+1))/(DR+1)))
					break;
				PLL_llu = (unsigned long long)(SRC_FREQ * 2 * (DF+1) * base);
				div_llu = (unsigned long long)(VD * (DR+1)*(1<<DQ));
				mod = do_div(PLL_llu, div_llu);
				if (PLL_llu == freq_llu) {
					DIVF = DF;
					DIVR = DR;
					DIVQ = DQ;
					div = VD;
					/*printk(KERN_INFO"find the equal value");*/
					goto find;
				} else if (PLL_llu < freq_llu) {
					minor = freq_llu - PLL_llu;
					//printk(KERN_INFO"minor=0x%x, min_minor=0x%x", minor, min_minor);
					if (minor < min_minor) {
						DIVF = DF;
						DIVR = DR;
						DIVQ = DQ;
						div = VD;
						min_minor = minor;
					}
				} else if (PLL_llu > freq_llu) {
					if (PLL_NO == 2) {
						minor = PLL_llu - freq_llu;
						if (minor < min_minor) {
							DIVF = DF;
							DIVR = DR;
							DIVQ = DQ;
							div = VD;
							min_minor = minor;
						}
					}
					break;
				}
			}//DF
		}//DQ
	}//DR
/*minimun:*/
	//printk(KERN_INFO"minimum minor=0x%x, unit=%d \n", (unsigned int)min_minor, unit);
find:

	/*filter = SRC_FREQ/(DIVR+1);
	filt_range = check_filter(filter);*/

	PLL_llu = (unsigned long long)(SRC_FREQ * 2 * (DIVF+1) * base);
	div_llu = (unsigned long long)((DIVR+1)*(1<<DIVQ)*(*divisor));
	mod = do_div(PLL_llu, div_llu);
	last_freq = (unsigned int)PLL_llu;
	/*printk("set_pll cmd: freq=%d freq(unsigned long long)=%llu div=%llu mod=%llu\n", last_freq, PLL_llu, div_llu, mod);
	printk(KERN_INFO"DIVF%d, DIVR%d, DIVQ%d, divisor%d freq=%dHz \n",
	DIVF, DIVR, DIVQ, *divisor, last_freq);*/
	PLL = (DIVF<<16) + (DIVR<<8) + DIVQ/* + (filt_range<<24)*/;

	/* if the clk of the device is not enable, then enable it */
	/*if (dev < 128) {
		pmc_clk_en = *(volatile unsigned int *)(PMC_CLK + 4*(dev/32));
		if (!(pmc_clk_en & (1 << (dev - 32*(dev/32)))))
			enable_dev_clk(dev);
	}*/

	check_PLL_DIV_busy();
	/*printk(KERN_INFO"PLL0x%x, pll addr =0x%x\n", PLL, PMC_PLL + 4*PLL_NO);*/
	*(volatile unsigned int *)(PMC_PLL + 4*PLL_NO) = PLL;
	check_PLL_DIV_busy();
	return last_freq;

	printk(KERN_INFO"no suitable pll");
	return -1;
}

static int set_pll_divisor(enum dev_id dev, int unit, int freq, int *divisor) {

	unsigned int PLL, DIVF=1, DIVR=1, DIVQ=1, pmc_clk_en, old_divisor;
	unsigned int last_freq, div=1/*, filt_range, filter*/;
	unsigned long long minor, min_minor = 0xFF000000;
	int PLL_NO, div_addr_offs, DF, DR, VD, DQ;
	unsigned long long base = 1000000, base_f = 1, PLL_llu, div_llu, freq_llu, mod;
	PLL_NO = calc_pll_num(dev, &div_addr_offs);
	if (PLL_NO == -1) {
		printk(KERN_INFO"device not belong to PLL A B C D E");
		return -1;
	}

	if (PLL_NO == -2) {
		printk(KERN_INFO"device has no divisor");
		return -1;
	}

	if (dev == DEV_SDTV)
		*(volatile unsigned char *)(PMC_BASE + div_addr_offs + 2) = 1;
	else
		*(volatile unsigned char *)(PMC_BASE + div_addr_offs + 2) = 0;

	check_PLL_DIV_busy();

	old_divisor = *(volatile unsigned char *)(PMC_BASE + div_addr_offs);

	if (unit == 1)
		base_f = 1000;
	else if (unit == 2)
		base_f = 1000000;
	else if (unit != 0) {
		printk(KERN_INFO"unit is out of range");
		return -1;
	}
//printk(KERN_INFO"target freq=%d unit=%d PLL_NO=%d\n", freq, unit, PLL_NO);
	freq_llu = freq * base_f;
	//tmp = div * freq;
	for (DR = MAX_DR; DR >= 0; DR--) {
		for (DQ = MAX_DQ; DQ >= 0; DQ--) {
			for (VD = 32; VD >= 1; VD--) {
				if ((VD > 1 && (VD%2)) && ((PLL_NO == 2) || (PLL_NO == 6)))
					continue;
				for (DF = 0; DF <= MAX_DF; DF++) {
					if (SRC_FREQ/(DR+1) < 10)
						break;
					if ((800/SRC_FREQ) > ((2*(DF+1))/(DR+1)))
						continue;
					if ((1600/SRC_FREQ) < ((2*(DF+1))/(DR+1)))
						break;
					PLL_llu = (unsigned long long)(SRC_FREQ * 2 * (DF+1) * base);
					div_llu = (unsigned long long)(VD * (DR+1)*(1<<DQ));
					mod = do_div(PLL_llu, div_llu);
					if (PLL_llu == freq_llu) {
						DIVF = DF;
						DIVR = DR;
						DIVQ = DQ;
						div = VD;
						/*printk(KERN_INFO"find the equal value");*/
						goto find;
					} else if ((PLL_llu < freq_llu) /*&& (dev != DEV_I2S)*/) {
						minor = freq_llu - PLL_llu;
						//printk(KERN_INFO"minor=0x%x, min_minor=0x%x", minor, min_minor);
						if (minor < min_minor) {
							DIVF = DF;
							DIVR = DR;
							DIVQ = DQ;
							div = VD;
							min_minor = minor;
							/*printk(KERN_INFO"< target :DIVF%d, DIVR%d, DIVQ%d, divisor%d min_minor=%lluHz \n",
							DIVF, DIVR, DIVQ, div, min_minor);*/
						}
					} else if (PLL_llu > freq_llu) {
						if ((PLL_NO == 2) || (PLL_NO == 6)) {
							minor = PLL_llu - freq_llu;
							if (minor < min_minor) {
								DIVF = DF;
								DIVR = DR;
								DIVQ = DQ;
								div = VD;
								min_minor = minor;
								/*printk(KERN_INFO" > target DIVF%d, DIVR%d, DIVQ%d, divisor%d min_minor=%lluHz \n",
								DIVF, DIVR, DIVQ, div, min_minor);*/
							}
						}
						break;
					}
				}//DF
			}//VD
		}//DQ
	}//DR
/*minimun:*/
	//printk(KERN_INFO"minimum minor=%llu, unit=%d \n", min_minor, unit);
find:

	/*filter = SRC_FREQ/(DIVR+1);
	filt_range = check_filter(filter);*/
	*divisor = div;

	PLL_llu = (unsigned long long)(SRC_FREQ * 2 * (DIVF+1) * base);
	div_llu = (unsigned long long)((DIVR+1)*(1<<DIVQ)*(*divisor));
	mod = do_div(PLL_llu, div_llu);
	last_freq = (unsigned int)PLL_llu;
	/*printk("set_pll_divisor cmd: freq=%d freq(unsigned long long)=%llu div=%llu mod=%llu\n", last_freq, PLL_llu, div_llu, mod);
	printk(KERN_INFO"DIVF%d, DIVR%d, DIVQ%d, divisor%d freq=%dHz \n",
	DIVF, DIVR, DIVQ, *divisor, last_freq);*/
	PLL = (DIVF<<16) + (DIVR<<8) + DIVQ/* + (filt_range<<24)*/;


	/* if the clk of the device is not enable, then enable it */
	if (dev < 128) {
		pmc_clk_en = *(volatile unsigned int *)(PMC_CLK + 4*(dev/32));
		if (!(pmc_clk_en & (1 << (dev - 32*(dev/32))))) {
			/*enable_dev_clk(dev);*/
			printk(KERN_INFO"device clock is disabled");
			return -1;
		}
	}

	check_PLL_DIV_busy();
	if (old_divisor < *divisor) {
		if (dev == DEV_NA0)
			*(volatile unsigned int *)(PMC_BASE + div_addr_offs) =	(0x200 | ((div == 32) ? 0 : div));
		else
			*(volatile unsigned int *)(PMC_BASE + div_addr_offs)
			= ((dev == DEV_SDTV)?0x10000:0) +/*(j?32:0) + */((div == 32) ? 0 : div)/* + (div&1) ? (1<<8): 0*/;
		check_PLL_DIV_busy();
	}
	//printk(KERN_INFO"PLL0x%x, pll addr =0x%x\n", PLL, PMC_PLL + 4*PLL_NO);
	*(volatile unsigned int *)(PMC_PLL + 4*PLL_NO) = PLL;
	check_PLL_DIV_busy();
	/*printk(KERN_INFO"DIVF%d, DIVR%d, DIVQ%d, div%d div_addr_offs=0x%x\n",
	DIVF, DIVR, DIVQ, div, div_addr_offs);*/

	if (old_divisor > *divisor) {
		if (dev == DEV_NA0)
			*(volatile unsigned int *)(PMC_BASE + div_addr_offs) =	(0x200 | ((div == 32) ? 0 : div));
		else
			*(volatile unsigned int *)(PMC_BASE + div_addr_offs)
			= ((dev == DEV_SDTV)?0x10000:0) +/*(j?32:0) + */((div == 32) ? 0 : div)/* + (div&1) ? (1<<8): 0*/;
		check_PLL_DIV_busy();
	}


	/*ddv = *(volatile unsigned int *)(PMC_BASE + div_addr_offs);
	check_PLL_DIV_busy();
	pllx = *(volatile unsigned int *)(PMC_PLL + 4*PLL_NO);
	printk(KERN_INFO"read divisor=%d, pll=0x%x from register\n", 0x1F&ddv, pllx);*/
	return last_freq;

	/*printk(KERN_INFO"no suitable divisor");
	return -1;*/
}

/*
* cmd : CLK_DISABLE  : disable clock,
*       CLK_ENABLE   : enable clock,
*       GET_FREQ     : get device frequency, it doesn't enable or disable clock,
*       SET_DIV      : set clock by setting divisor only(clock must be enabled by CLK_ENABLE command),
*       SET_PLL      : set clock by setting PLL only, no matter clock is enabled or not,
*                      this cmd can be used before CLK_ENABLE cmd to avoid a extreme speed
*                      to be enabled when clock enable.
*       SET_PLLDIV   : set clock by setting PLL and divisor(clock must be enabled by CLK_ENABLE command).
* dev : Target device ID to be set the clock.
* unit : the unit of parameter "freq", 0 = Hz, 1 = KHz, 2 = MHz.
* freq : frequency of the target to be set when cmd is "SET_XXX".
*
* return : return value is different depend on cmd type,
*				CLK_DISABLE  : return internal count which means how many drivers still enable this clock,
*				               retrun -1 if this device has no clock enable register.
*				               
*				CLK_ENABLE   : return internal count which means how many drivers enable this clock,
*				               retrun -1 if this device has no clock enable register.
*
*				GET_FREQ     : return device frequency in Hz when clock is enabled,
*				               return -1 when clock is disabled.
*
*       SET_DIV      : return the finally calculated frequency when clock is enabled,
*				               return -1 when clock is disabled.
*
*       SET_PLL      : return the finally calculated frequency no matter clock is enabled or not.
*
*       SET_PLLDIV   : return the finally calculated frequency when clock is enabled,
*				               return -1 when clock is disabled.
* Caution :
* 1. The final clock freq maybe an approximative value,
*    equal to or less than the setting freq.
* 2. SET_DIV and SET_PLLDIV commands which would set divisor register must use CLK_ENABLE command
*    first to enable device clock.
* 3. Due to default frequency may be extremely fast when clock is enabled. use SET_PLL command can
*    set the frequency into a reasonable value, but don't need to enable clock first.
*/

int auto_pll_divisor(enum dev_id dev, enum clk_cmd cmd, int unit, int freq)
{
	int last_freq, divisor, en_count;

	switch (cmd) {
		case CLK_DISABLE:
			if (dev < 128) {
				en_count = disable_dev_clk(dev);
				return en_count;
			} else {
				printk(KERN_INFO"device has not clock enable register");
				return -1;
			}
		case CLK_ENABLE:
			if (dev < 128) {
				en_count = enable_dev_clk(dev);
				return en_count;
			} else {
				printk(KERN_INFO"device has not clock enable register");
				return -1;
			}
		case GET_FREQ:
			last_freq = get_freq(dev, &divisor);
			return last_freq;
		case SET_DIV:
			divisor = 0;
			last_freq = set_divisor(dev, unit, freq, &divisor);
			return last_freq;
		case SET_PLL:
			divisor = 0;
			last_freq = set_pll_speed(dev, unit, freq, &divisor);
			return last_freq;
		case SET_PLLDIV:
			divisor = 0;
			last_freq = set_pll_divisor(dev, unit, freq, &divisor);
			return last_freq;
		default:
		printk(KERN_INFO"clock cmd unknow");
		return -1;
	}
}
EXPORT_SYMBOL(auto_pll_divisor);

/**
* freq = src_freq * 2 * (DIVF+1)/((DIVR+1)*(2^DIVQ))
*
* dev : Target device ID to be set the clock.
* DIVF : Feedback divider value.
* DIVR : Reference divider value.
* DIVQ : Output divider value.
* dev_div : Divisor belongs to each device, 0 means not changed.
* return : The final clock freq, in Hz, will be returned when success,
*            -1 means fail (waiting busy timeout).
*
* caution :
* 1. src_freq/(DIVR+1) should great than or equal to 10,
*/
int manu_pll_divisor(enum dev_id dev, int DIVF, int DIVR, int DIVQ, int dev_div)
{

	unsigned int PLL, freq, pmc_clk_en, old_divisor, div;
	int PLL_NO, div_addr_offs, j = 0, /*filter, filt_range,*/ SD_MMC = 0;
	
	if ((dev == DEV_SDMMC0)||(dev == DEV_SDMMC1)||(dev == DEV_SDMMC2)||(dev == DEV_SDMMC3))
		SD_MMC = 1;
	
	if (DIVF < 0 || DIVF > MAX_DF){
		printk(KERN_INFO"DIVF is out of range 0 ~ %d", MAX_DF);
		return -1;
	}
	if (DIVR < 0 || DIVR > MAX_DR){
		printk(KERN_INFO"DIVR is out of range 0 ~ %d", MAX_DR);
		return -1;
	}
	if (DIVQ < 0 || DIVQ > MAX_DQ){
		printk(KERN_INFO"DIVQ is out of range 0 ~ %d", MAX_DQ);
		return -1;
	}
	if ((800/SRC_FREQ) > ((2*(DIVF+1))/(DIVR+1))) {
		printk(KERN_INFO"((2*(DIVF+1))/(DIVR+1)) should great than (800/SRC_FREQ)");
		return -1;
	}
	if ((1600/SRC_FREQ) < ((2*(DIVF+1))/(DIVR+1))) {
		printk(KERN_INFO"((2*(DIVF+1))/(DIVR+1)) should less than (1600/SRC_FREQ)");
		return -1;
	}
	/*filter = SRC_FREQ/(DIVR+1);
	if (filter < 10 || filter > 50) {
		printk(KERN_INFO"SRC_FREQ/(DIVR+1) should great then 9 and less then 201");
		return -1;
	}*/
	if (dev_div > ((SD_MMC == 1)?63:31)){
		printk(KERN_INFO"divisor is out of range 0 ~ 31");
		return -1;
	}

	PLL_NO = calc_pll_num(dev, &div_addr_offs);
	if (PLL_NO == -1) {
		printk(KERN_INFO"device not found");
		return -1;
	}
	old_divisor = *(volatile unsigned char *)(PMC_BASE + div_addr_offs);

	if (SD_MMC == 1 && (dev_div&32))
		j = 1; /* sdmmc has a another divider = 64 */	
	div = dev_div&0x1F;	
	freq = (1000 * SRC_FREQ * 2 * (DIVF+1))/((DIVR+1)*(1<<DIVQ)*div*(j?128:1));
	freq *= 1000;
	//printk(KERN_INFO"DIVF%d, DIVR%d, DIVQ%d, dev_div%d, freq=%dkHz\n", DIVF, DIVR, DIVQ, dev_div, freq);

	//filt_range = check_filter(filter);
	PLL = (DIVF<<16) + (DIVR<<8) + DIVQ/* + (filt_range<<24)*/;

	/* if the clk of the device is not enable, then enable it */
	if (dev < 128) {
		pmc_clk_en = *(volatile unsigned int *)(PMC_CLK + 4*(dev/32));
		if (!(pmc_clk_en & (1 << (dev - 32*(dev/32)))))
			enable_dev_clk(dev);
	}

	check_PLL_DIV_busy();
	if (old_divisor < dev_div) {
		*(volatile unsigned int *)(PMC_BASE + div_addr_offs)
		= (j?32:0) + ((div == 32) ? 0 : div)/* + (div&1) ? (1<<8): 0*/;
		check_PLL_DIV_busy();
	}
	*(volatile unsigned int *)(PMC_PLL + 4*PLL_NO) = PLL;
	check_PLL_DIV_busy();
	if (old_divisor > dev_div) {
		*(volatile unsigned int *)(PMC_BASE + div_addr_offs)
		= (j?32:0) + ((div == 32) ? 0 : div)/* + (div&1) ? (1<<8): 0*/;
		check_PLL_DIV_busy();
	}


	//PLL = (j?32:0) + ((div == 32) ? 0 : div) /*+ (div&1) ? (1<<8): 0*/;
	//printk(KERN_INFO"set divisor =0x%x, divider address=0x%x\n", PLL, (PMC_BASE + div_addr_offs));
	return freq;
}
EXPORT_SYMBOL(manu_pll_divisor);


int wm_pmc_set(enum dev_id dev, enum power_cmd cmd)
{
	int retval = -1;
	unsigned int temp = 0, base = 0, mali_val = 0;
	unsigned int mali_off, mali_l2c_off = 0x00130620, mali_gp_off = 0x00130624;
	mali_off = mali_l2c_off;

	base = 0xfe000000;
	temp = REG32_VAL(base + 0x00110110);
	temp = ((temp >> 10)&0xC0)|(temp&0x3F);

	if ((cmd == DEV_PWRON)) {
		if ((temp & 0xC0) == 0x80) {//8700/8720 can't be power on
			retval = -1;
		} else {
			while ((REG32_VAL(base + mali_off)&0xF0)!= 0
			&& (REG32_VAL(base + mali_off)&0xF0)!= 0xF0)
				;

			mali_val = REG32_VAL(base + mali_off);
			if (!((mali_val&0xF0)==0xF0))
				REG32_VAL(base + mali_off) |= 1;

			while ((REG32_VAL(base + mali_off)&0xF0)!= 0
			&& (REG32_VAL(base + mali_off)&0xF0)!= 0xF0)
				;
			retval = 0;
		}
	} else if (cmd == DEV_PWROFF) {
		if ((temp & 0xC3) == 0xC0) {//8710B0 can't power off after power on
			retval = -1;
		} else {
			while ((REG32_VAL(base + mali_off)&0xF0)!= 0
			&& (REG32_VAL(base + mali_off)&0xF0)!= 0xF0)
				;
			mali_val = REG32_VAL(base + mali_off);
			if ((mali_val&0xF0))
				REG32_VAL(base + mali_off) &= ~1;
			while ((REG32_VAL(base + mali_off)&0xF0)!= 0
			&& (REG32_VAL(base + mali_off)&0xF0)!= 0xF0)
				;
			retval = 0;
		}
	} else if (cmd == DEV_PWRSTS) {
		while ((REG32_VAL(base + mali_off)&0xF0)!= 0
		&& (REG32_VAL(base + mali_off)&0xF0)!= 0xF0)
			;
		temp = REG32_VAL(base + mali_off);
		if (temp & 0xF0)
			retval = 1;
		else
			retval = 0;
	}
	return retval;
}



#define LOADER_ADDR												0xffff0000
#define HIBERNATION_ENTER_EXIT_CODE_BASE_ADDR	0xFFFFFFC0
#define DO_POWER_SET							      (HIBERNATION_ENTER_EXIT_CODE_BASE_ADDR + 0x2C)
#define CP15_C1_XPbit 23
/*
 * Function:
 * int wmt_power_dev(enum dev_id dev, enum power_cmd cmd)
 * This function is used to control/get WMT SoC specific device power state.
 * 
 * Parameter:
 *  The available dev_id is DEV_MALI cmd used to control/get device power state. 
 *  The available power_cmd are
 *  - DEV_PWRON
 *  - DEV_PWROFF
 *  - DEV_PWRSTS
 * 
 * Return:
 *  - As power_cmd are DEV_PWRON, DEV_PWROFF 0 indicates success. 
 *    Negative value indicates failure as error code.
 * 
 *  - As power_cmd is DEV_PWRSTS
 * 	 0 indicates the current device is power off.
 * 	 1 indicates the current device is power on.
 * 	 Negative value indicates failure as error code.
 */
extern int spi_read_status(int chip);
int wmt_power_dev(enum dev_id dev, enum power_cmd cmd)
{
#if 0
	static unsigned int base = 0;
	unsigned int exec_at = (unsigned int)-1, temp = 0, bootdev;
	int (*theKernel_power)(int from, enum dev_id dev, enum power_cmd cmd);
	int en_count, retval = 0, rc = 0;
	unsigned long flags;
	bootdev = GPIO_STRAP_STATUS_VAL >> 1;

	/*printk(KERN_INFO"entry dev_id=%d cmd=%d, boot_strap=%x,\n",dev, cmd, GPIO_STRAP_STATUS_VAL);*/
	/*enble SF clock*/
	if((bootdev & 0x3) == 0)
		en_count = enable_dev_clk(DEV_SF); //SF boot
	else if((bootdev & 0x3) == 1) 
		en_count = enable_dev_clk(DEV_NAND);//NAND boot
	else if((bootdev & 0x3) == 2) 
		en_count = enable_dev_clk(DEV_NOR);//NOR boot
	else
		en_count = enable_dev_clk(DEV_SDMMC0);//MMC boot
	udelay(1);
#ifdef CONFIG_MTD_WMT_SF	
	rc = spi_read_status(0);
	if (rc)
		printk("wr c0 wait status ret=%d\n", rc);
#endif	
	/*rc = spi_read_status(1);
	if (rc)
		printk("wr c1 wait status ret=%d\n", rc);*/
	
	rc = 0;
	/*jump to loader api to do something*/
	if (base == 0)
		base = (unsigned int)ioremap/*_nocache*/(LOADER_ADDR, 0x10000);
	exec_at = base + (DO_POWER_SET - LOADER_ADDR);
	theKernel_power = (int (*)(int from, enum dev_id dev, enum power_cmd cmd))exec_at;
	/*temp = *(volatile unsigned int *)(0xFE110110);
	printk(KERN_INFO"entry exec_at=0x%x chip_id=0x%x, clock enable=0x%x\n", exec_at,
	((temp >> 10)&0xC0)|(temp&0x3F), *(volatile unsigned int *)(0xFE130250));*/
	/*backup flags and disable irq*/
	local_irq_save(flags);
	/*enable subpage AP bits*/
	asm volatile ("mrc p15, 0, %0, c1, c0, 0" : "=r" (temp));
	//if (!(temp & (1<<CP15_C1_XPbit)))
//		printk(KERN_INFO"1 xp(23) bits =0x%x cp15c0=0x%x\n", temp&(1<<CP15_C1_XPbit), temp);
	if (temp & (1<<CP15_C1_XPbit)) {
		rc = 1;
		temp &= ~(1<<CP15_C1_XPbit);
	  /*printk(KERN_INFO"2 xp(23) bits =0x%x \n", temp&(1<<CP15_C1_XPbit));*/
		asm volatile ("mcr p15, 0, %0, c1, c0, 0" : : "r" (temp));
	}
	//retval = theKernel_power(4, DEV_MALI, cmd);
	retval = wm_pmc_set(DEV_MALI, cmd);

	if (rc == 1) {
		/*disable subpage AP bits*/
		asm volatile ("mrc p15, 0, %0, c1, c0, 0" : "=r" (temp));
		/*printk(KERN_INFO"3 xp(23) bits =0x%x \n", temp&(1<<CP15_C1_XPbit));*/
		temp |= (1<<CP15_C1_XPbit);
		/*printk(KERN_INFO"4 xp(23) bits =0x%x \n", temp&(1<<CP15_C1_XPbit));*/
		asm volatile ("mcr p15, 0, %0, c1, c0, 0" : : "r" (temp));
	}
	/*restore irq flags*/
	local_irq_restore(flags);

	/*iounmap((void *)base);*/
	/*printk(KERN_INFO"entry base=0x%x exec_at=0x%x clock enable =0x%x\n",
	base, exec_at, *(volatile unsigned int *)(0xFE130250));*/

	/*disable SF clock*/
	if((bootdev & 0x3) == 0)
		en_count = disable_dev_clk(DEV_SF); //SF boot
	else if((bootdev & 0x3) == 1) 
		en_count = disable_dev_clk(DEV_NAND);//NAND boot
	else if((bootdev & 0x3) == 2) 
		en_count = disable_dev_clk(DEV_NOR);//NOR boot
	else
		en_count = disable_dev_clk(DEV_SDMMC0);//MMC boot

	/*printk(KERN_INFO"exit!!ret = (%d)\n",retval);*/
#endif
	return 0;//retval;
}
EXPORT_SYMBOL(wmt_power_dev);
