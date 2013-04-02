/*++ 
 * linux/sound/soc/wmt/wmt-pdm-if.c
 * WonderMedia I2S audio driver for ALSA
 *
 * Copyright c 2010  WonderMedia  Technologies, Inc.
 *
 * This program is free software: you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 2 of the License, or 
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details. 
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * WonderMedia Technologies, Inc.
 * 4F, 533, Chung-Cheng Road, Hsin-Tien, Taipei 231, R.O.C
--*/


#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <asm/irq.h>
#include <linux/delay.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>

#include <mach/hardware.h>
#include <asm/dma.h>
#include "wmt-soc.h"
#include "wmt-pdm-if.h"

#define PCM_IS_MASTER_MODE
#define NULL_DMA                ((dmach_t)(-1))

static int wmt_pdm_module_enable = 0;
extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);

static struct audio_stream_a wmt_pdm_data[] = {
	{
		.id = "WMT PDM out",
		.stream_id = SNDRV_PCM_STREAM_PLAYBACK,
		.dmach = NULL_DMA,
		.dma_dev = PCM_TX_DMA_REQ,
		/*.dma_cfg = dma_device_cfg_table[I2S_TX_DMA_REQ],*/
	},
	{
		.id = "WMT PDM in",
		.stream_id = SNDRV_PCM_STREAM_CAPTURE,
		.dmach = NULL_DMA,
		.dma_dev = PCM_RX_DMA_REQ,
		/*.dma_cfg = dma_device_cfg_table[I2S_RX_DMA_REQ],*/
	},
};

struct wmt_pdm
{
	int irq_no;
	int pcm_clk_src;
	int pdm_enable;
};

static struct wmt_pdm wmt_pdm;

static irqreturn_t
wmt_pdm_irq_handler(int irq, void *dev_id)
{	
	if (PCMSR_VAL & PCMSR_TXUND) {
		printk("-->PCMSR_TXUND\n");
	}
	else if (PCMSR_VAL & PCMSR_RXOVR) {
		printk("-->PCMSR_RXOVR\n");
	}
		
	PCMSR_VAL = 0x7F;	//write clear all intr
	return IRQ_HANDLED;
}

static void wmt_pdm_enable(void)
{
	PCMCR_VAL |= (PCMCR_PCM_ENABLE | PCMCR_DMA_EN);
}

static void wmt_pdm_disable(void)
{
	PCMCR_VAL &= ~(PCMCR_PCM_ENABLE | PCMCR_DMA_EN);
}

static int wmt_pdm_init(void)
{
	wmt_pdm.irq_no = IRQ_PCM;

	// Before control pcm-module, enable pcm clock first
	PMCEL_VAL |= BIT16;

	// set pcm_clk_source = 62464khz
	//auto_pll_divisor(DEV_PCM, CLK_ENABLE, 0, 0);
	//auto_pll_divisor(DEV_PCM, SET_PLLDIV, 1, 62464);

	wmt_pdm.pcm_clk_src = auto_pll_divisor(DEV_PCM, GET_FREQ, 0, 0);
	wmt_pdm.pcm_clk_src /= 1000;
	wmt_pdm.pdm_enable = 0;
	printk("wmt_pdm_init: pcm_clk_src=%d \n\r", wmt_pdm.pcm_clk_src);  

	if (wmt_pdm_module_enable) {
		/* disable GPIO and Pull Down mode */
		/* Bit1:I2SDACDAT1=PCMSYNC, Bit2:I2SDACDAT2=PCMCLK, Bit3:I2SDACDAT3=PCMIN, Bit4:I2SADCMCLK=PCMOUT */
		GPIO_CTRL_GP10_I2S_BYTE_VAL &= ~(BIT1 | BIT2 | BIT3 | BIT4);
		GPIO_PULL_EN_GP10_I2S_BYTE_VAL &= ~(BIT1 | BIT2 | BIT3 | BIT4);

		/* set to pcm mode */
		// select PCMMCLK[bit0], PCMSYNC[bit17:16], PCMCLK[19:18], PCMIN[20], PCMOUT[22:21]
		GPIO_PIN_SHARING_SEL_4BYTE_VAL &= ~(BIT0 | BIT16 | BIT18 | BIT21);
		GPIO_PIN_SHARING_SEL_4BYTE_VAL |= (BIT17 | BIT19 | BIT20 | BIT22);
	}

	// set pcm control register
	PCMCR_VAL |= (PCMCR_TXFF_RST | PCMCR_RXFF_RST);
	//PCMCR_VAL |= 0x00880000;	// TX/RX Fifo Threshold A
    PCMCR_VAL &= ~(PCMCR_BCLK_SEL);
	PCMCR_VAL &= ~PCMCR_SLAVE;  // master mode

	// set pcm format register
	PCMDFCR_VAL = 0;
	PCMDFCR_VAL |= (PCMDFCR_WR_AL | PCMDFCR_TX_AL | PCMDFCR_RX_AL | PCMDFCR_RD_AL);
	PCMDFCR_VAL |= (PCMDFCR_TX_SZ_14 | PCMDFCR_RX_SZ_14);
	//PCMDFCR_VAL |= (PCMDFCR_TX_SZ_08 | PCMDFCR_RX_SZ_08);
	
	//
	// request irq
	//
	/*if (request_irq(wmt_pdm.irq_no, &wmt_pdm_irq_handler, IRQF_DISABLED, "wmt_pdm", NULL)){
		printk(KERN_ERR "PCM_IRQ Request Failed!\n"); 
	}
	PCMCR_VAL |= (PCMCR_IRQ_EN | PCMCR_TXUND_EN | PCMCR_RXOVR_EN);
	*/
	return 0 ;
}


static int wmt_pdm_dai_startup(struct snd_pcm_substream *substream,
											struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int stream_id = substream->pstr->stream;
	struct audio_stream_a *s = &wmt_pdm_data[0];
	
	s[stream_id].stream = substream;
	runtime->private_data = s;

	return 0;
}

static void wmt_pdm_dai_shutdown(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	
}

static int wmt_pdm_dai_trigger(struct snd_pcm_substream *substream, int cmd,
								struct snd_soc_dai *dai)
{
	int err = 0;
	int stream_id = substream->pstr->stream;
	
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (stream_id == SNDRV_PCM_STREAM_PLAYBACK) {
			PCMCR_VAL |= PCMCR_TXFF_RST; // reset txfifo
			PCMDFCR_VAL |= PCMDFCR_TXFM_EN;
		}
		else if (stream_id == SNDRV_PCM_STREAM_CAPTURE) {
			PCMCR_VAL |= PCMCR_RXFF_RST; // reset rxfifo
			PCMDFCR_VAL |= PCMDFCR_RXFM_EN;
		}
		//wmt_pdm_enable();
		wmt_pdm.pdm_enable++;
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (stream_id == SNDRV_PCM_STREAM_PLAYBACK) {
			PCMDFCR_VAL &= ~PCMDFCR_TXFM_EN;
		}
		else if (stream_id == SNDRV_PCM_STREAM_CAPTURE) {
			PCMDFCR_VAL &= ~PCMDFCR_RXFM_EN;
		}
		//wmt_pdm_disable();
		wmt_pdm.pdm_enable--;
		break;
	default:
		err = -EINVAL;
		break;
	}

	if (wmt_pdm.pdm_enable)
		wmt_pdm_enable();
	else
		wmt_pdm_disable();

	return err;
}

static int wmt_pdm_dai_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	return 0;
}

static int wmt_pdm_dai_prepare(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int channel, byte;
	int stream_id = substream->pstr->stream;

	byte = (runtime->sample_bits)/8;
	channel = runtime->channels;
	
	printk(KERN_INFO "wmt_pdm_dai_prepare byte = %d, channels = %d", byte, runtime->channels);

	/* format setting */
	if (stream_id == SNDRV_PCM_STREAM_PLAYBACK) {
		/* little or big endian check */
		switch (runtime->format) {
		case SNDRV_PCM_FORMAT_S16_LE:
			break;
		default:
			break;
		}

		/* channel number check */
		switch (runtime->channels) {
		case 1:
			break;
		case 2:
			break;
		default:
			break;
		}
	}
	else if (stream_id == SNDRV_PCM_STREAM_CAPTURE) {
		/* little or big endian check */
		switch (runtime->format) {
		case SNDRV_PCM_FORMAT_S16_LE:
			break;
		default:	
			break;
		}

		/* channel number check */
		switch (runtime->channels) {
		case 1:
			break;
		case 2:
			break;
		default:
			break;	
		}
	}

	switch (runtime->rate) {
	case 16000:
		PCMDIVR_VAL &= ~PCMCLK_DIV_MASK;
		PCMDIVR_VAL |= (wmt_pdm.pcm_clk_src / PCMCLK_256K);
		break;
	case 8000:
		PCMDIVR_VAL &= ~PCMCLK_DIV_MASK;
		PCMDIVR_VAL |= (wmt_pdm.pcm_clk_src / PCMCLK_128K);
		break;
	default :
		printk(KERN_ERR "not supported fs: %d \n\r", runtime->rate);
		break;
	}

	return 0;
}

/*
 * This must be called before _set_clkdiv and _set_sysclk since McBSP register
 * cache is initialized here
 */
static int wmt_pdm_dai_set_dai_fmt(struct snd_soc_dai *cpu_dai,
				      unsigned int fmt)
{
	return 0;
}

#ifdef CONFIG_PM
static int wmt_pdm_suspend(struct snd_soc_dai *cpu_dai)
{
	return 0;
}

static int wmt_pdm_resume(struct snd_soc_dai *cpu_dai)
{
	int ret = wmt_pdm_init();
	if (ret) {
		pr_err("Failed to init pcm module: %d\n", ret);
		return ret;
	}
	return 0;
}
#else
#define wmt_pdm_suspend		NULL
#define wmt_pdm_resume		NULL
#endif

static struct snd_soc_dai_ops wmt_pdm_dai_ops = {
	.startup    = wmt_pdm_dai_startup,
	.prepare    = wmt_pdm_dai_prepare,
	.shutdown   = wmt_pdm_dai_shutdown,
	.trigger    = wmt_pdm_dai_trigger,
	.hw_params  = wmt_pdm_dai_hw_params,
	.set_fmt    = wmt_pdm_dai_set_dai_fmt,
};

struct snd_soc_dai_driver wmt_pdm_dai = {
	.suspend = wmt_pdm_suspend,
	.resume = wmt_pdm_resume,
	.playback = {
		.channels_min = 1,
		.channels_max = 1,
		.rates = SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_8000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.channels_min = 1,
		.channels_max = 1,
		.rates = SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_8000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.ops = &wmt_pdm_dai_ops,
};

static int wmt_pdm_probe(struct platform_device *pdev)
{
	int ret = 0;
	char buf[64];
	int varlen = 64;

	ret = wmt_getsyspara("wmt.audio.pdm", buf, &varlen);
	if (ret == 0) {
		sscanf(buf, "%d", &wmt_pdm_module_enable);
	}

	ret = wmt_pdm_init();
	if (ret) {
		pr_err("Failed to init pcm module: %d\n", ret);
		return ret;
	}
	
	/* register with the ASoC layers */
	ret = snd_soc_register_dai(&pdev->dev, &wmt_pdm_dai);
	if (ret) {
		pr_err("Failed to register DAI: %d\n", ret);
		return ret;
	}
	
	wmt_pdm_data[0].dma_cfg = dma_device_cfg_table[PCM_TX_DMA_REQ];
	wmt_pdm_data[1].dma_cfg = dma_device_cfg_table[PCM_RX_DMA_REQ];
	
	spin_lock_init(&wmt_pdm_data[0].dma_lock);
	spin_lock_init(&wmt_pdm_data[1].dma_lock);
	
	return 0;
}

static int __devexit wmt_pdm_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&pdev->dev);
	return 0;
}

static struct platform_driver wmt_pdm_driver = {
	.probe  = wmt_pdm_probe,
	.remove = __devexit_p(wmt_pdm_remove),
	.driver = {
		.name = "wmt-pdm",
		.owner = THIS_MODULE,
	},
};


static int __init wmt_pdm_module_init(void)
{
	return platform_driver_register(&wmt_pdm_driver);
}

static void __exit wmt_pdm_module_exit(void)
{
	platform_driver_unregister(&wmt_pdm_driver);
}

module_init(wmt_pdm_module_init);
module_exit(wmt_pdm_module_exit);

MODULE_AUTHOR("WonderMedia Technologies, Inc.");
MODULE_DESCRIPTION("WMT [ALSA SoC] driver");
MODULE_LICENSE("GPL");

