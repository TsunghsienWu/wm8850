/*++ 
 * linux/sound/soc/wmt/wmt-i2s.c
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
#include "wmt-pcm.h"

#define NULL_DMA                ((dmach_t)(-1))
#define AUD_SPDIF_ENABLE	1

static int wmt_i2s_output = 0;

/*
 * Debug
 */
#define AUDIO_NAME "WMT_I2S"
//#define WMT_I2S_DEBUG 1
//#define WMT_I2S_DEBUG_DETAIL 1

#ifdef WMT_I2S_DEBUG
#define DPRINTK(format, arg...) \
	printk(KERN_INFO AUDIO_NAME ": " format "\n" , ## arg)
#else
#define DPRINTK(format, arg...) do {} while (0)
#endif

#ifdef WMT_I2S_DEBUG_DETAIL
#define DBG_DETAIL(format, arg...) \
	printk(KERN_INFO AUDIO_NAME ": [%s]" format "\n" , __FUNCTION__, ## arg)
#else
#define DBG_DETAIL(format, arg...) do {} while (0)
#endif

#define err(format, arg...) \
	printk(KERN_ERR AUDIO_NAME ": " format "\n" , ## arg)
#define info(format, arg...) \
	printk(KERN_INFO AUDIO_NAME ": " format "\n" , ## arg)
#define warn(format, arg...) \
	printk(KERN_WARNING AUDIO_NAME ": " format "\n" , ## arg)

struct wmt_i2s_asoc_data {
	unsigned int bus_id;
	struct i2s_s i2s;
	/*
	 * Flags indicating is the bus already activated and configured by
	 * another substream
	 */
	int active;
	int configured;
	struct timer_list delay_timer;
	struct audio_stream_a s[2];
};

static int wmt_pdm_module_enable = 0;
extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);

#define to_i2s_data(priv)	container_of((priv), struct wmt_i2s_asoc_data, bus_id)
#define SNDRV_PCM_STREAM_ALL	2

static void i2s_init(int mode);
static void i2s_exit(void);

#ifdef CONFIG_FB_WMT
extern int vpp_set_audio(int format, int sample_rate, int channel);
#endif


static struct wmt_i2s_asoc_data i2s_data[NUM_LINKS] = {
	{
		.bus_id = 0,
		.i2s = {
				/* interrupt counters */
				{0, 0, 0, 0, 0, 0, 0, 0},
				/* irq number*/
				0,
				/* reference counter */
				0,
				/* channels */
				0,
				/* format */
				0,
				/* fragment size */
				0,
				/* sample rate */
				0,
				i2s_init,
				i2s_exit,
		},
		.s =  {
			{
				.id = "WMT I2S out",
				.stream_id = SNDRV_PCM_STREAM_PLAYBACK,
				.dmach = NULL_DMA,
				.dma_dev = AHB1_AUD_DMA_REQ_1,
				/*.dma_cfg = dma_device_cfg_table[I2S_TX_DMA_REQ],*/
			},
			{
				.id = "WMT I2S in",
				.stream_id = SNDRV_PCM_STREAM_CAPTURE,
				.dmach = NULL_DMA,
				.dma_dev = AHB1_AUD_DMA_REQ_0,
				/*.dma_cfg = dma_device_cfg_table[I2S_RX_DMA_REQ],*/
			},
		},
	},
};


/* for HDMI Audio status */
int hd_audio_flag = 0;



#ifdef CONFIG_WMT_I2S_INT
/* wmt_i2s_interrupt()
 *
 * It's only interrupt counter now, might be useful to
 * debug or benchmark.
 */
static irqreturn_t
wmt_i2s_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	DBG_DETAIL();
	
	return IRQ_HANDLED;
}
#endif

static void delay_timer_handler(unsigned long data)
{
	DBG_DETAIL();
}

void wmt_i2s_dac0_ctrl(int HDMI_audio_enable)
{
	if (HDMI_audio_enable) {
		/* disable DAC#0, if HDMI Audio is enabled  */
		/* assign ch0 to DAC#1_L, DAC#2_L, SPDIF_L,
			  	  ch1 to DAC#1_R, DAC#2_R, SPDIF_R,
			  	  ch8 to DAC#0_L,
			  	  ch9 to DAC#0_R */
		ASMPFCHCFG0_VAL = 0x10109810;

		/* assign ch0 to DAC#3_L, ch1 to DAC#3_R */
		ASMPFCHCFG1_VAL = 0x10;
		
		info("HDMI Audio is enabled, disable dac0 of I2S");
	}
	else {
		/* assign ch0 to DAC#0_L, DAC#1_L, DAC#2_L, SPDIF_L,
			  	  ch1 to DAC#0_R, DAC#1_R, DAC#2_R, SPDIF_R */
		ASMPFCHCFG0_VAL = 0x10101010;

		/* assign ch0 to DAC#3_L, ch1 to DAC#3_R */
		ASMPFCHCFG1_VAL = 0x10;
		
		info("HDMI Audio is disabled, enable dac0 of I2S");
	}

	wmt_i2s_output = HDMI_audio_enable;
}
EXPORT_SYMBOL(wmt_i2s_dac0_ctrl);

static void i2s_disable(void)
{
	DBG_DETAIL();

	DACCFG_VAL &= ~DACITF_ENABLE;
	ASMPFCFG_VAL &=~ASMPF_ENABLE;
	HDACKGEN_VAL &= ~HDACKGEN_ENABLE;
	
#ifdef AUD_SPDIF_ENABLE
	DGOCFG_VAL &= ~DGOITF_ENABLE;
#endif

	AADCF0CFG_VAL &= ~(AADCITF_ENABLE | AADCF_ENABLE);
}

static void i2s_enable(void)
{
	DBG_DETAIL();

	AADCF0CFG_VAL |= (AADCITF_ENABLE | AADCF_ENABLE);
	ASMPFCFG_VAL |= (ASMPF_ENABLE);
	
#ifdef AUD_SPDIF_ENABLE
	DGOCFG_VAL |= DGOITF_ENABLE;
#endif

	DACCFG_VAL |= DACITF_ENABLE;

	if (hd_audio_flag)
		HDACKGEN_VAL |= HDACKGEN_ENABLE;
}

static void aud_audprf_setting(unsigned char smp_rate_index)
{
	unsigned long cfg_tbl[] = {0x00002001, 0x00002001, 0x00002001,
		0x00002001, 0x00002001, 0x00002001, 0x00002001, 0x00002001,
		0x00002001, 0x00002001, 0x00002001, 0x00002001,
		0x00000001, 0x00000001, 0x00000001
	};
	unsigned int dgo_tbl[] = {0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
								0x02, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00};
	unsigned int sysclk_tbl[] = {4096, 5632, 6144, 8192, 11264, 12288, 16384,
								22579, 24576, 32768, 45056, 49152, 16384, 22528, 24576};
#ifdef AUD_SPDIF_ENABLE	
	unsigned int dgocs_tbl[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
								0x00, 0x02, 0x00, 0x08, 0x0A, 0x00, 0x0C, 0x0E};
#endif
	//unsigned int clock = 0;
	unsigned long aud_reg_val;

	DBG_DETAIL();
	
	ADCCFG_VAL = cfg_tbl[smp_rate_index];

	aud_reg_val = dgo_tbl[smp_rate_index];
	DGOCFG_VAL = aud_reg_val;
	HDACKGEN_VAL = aud_reg_val;

#ifdef AUD_SPDIF_ENABLE	
	/* set ADGO channel status */
	aud_reg_val = dgocs_tbl[smp_rate_index] << 24;
	DGOCS0A_VAL = aud_reg_val;
	DGOCS1A_VAL = aud_reg_val;
	//info("%s : DGOCS01A_VAL=0x%x", __func__, (unsigned int)aud_reg_val);
#endif	

	DACCFG_VAL = cfg_tbl[smp_rate_index];

	auto_pll_divisor(DEV_I2S, CLK_ENABLE , 0, 0);
	auto_pll_divisor(DEV_I2S, SET_PLLDIV, 1, sysclk_tbl[smp_rate_index]);
	/*clock = auto_pll_divisor(DEV_I2S, GET_FREQ , 0, 0);
	info("%s : clock=%d", __func__, clock);*/
}

static unsigned char aud_smp_rate_convert(unsigned int smp_rate)
{
	unsigned char i = 0;
	unsigned int smp_rate_tbl[] = {8000, 11025, 12000, 16000, 22050, 24000, 32000,
		                           44100, 48000, 64000, 88200, 96000, 128000, 176000, 192000};

	DBG_DETAIL();
	
	/* boundary checking */
	if (smp_rate < 8000) {
		i = 0x00;
	}
	else if (smp_rate > 192000) {
		i = 0x0E;
	}
	else {
		for (i = 0; i < 0x1F; i++) {
			if (smp_rate == smp_rate_tbl[i]) {
				break;
			}
			else if (smp_rate < smp_rate_tbl[i + 1]) {
				if (smp_rate < (smp_rate_tbl[i] + ((smp_rate_tbl[i + 1] - smp_rate_tbl[i]) / 2))) {
					break;
				}
				else {
					i++;
					break;
				}
			}
		}
	}

	return i;
	
}

static void i2s_sample_rate(unsigned int rate)
{
	unsigned char rate_index;

	DBG_DETAIL("rate=%d", rate);

	if (rate == i2s_data->i2s.rate)
		return;

	i2s_data->i2s.rate = rate;
	
	rate_index = aud_smp_rate_convert(rate);
	
	aud_audprf_setting(rate_index);

#ifdef CONFIG_FB_WMT
	/* pass information of audio to HDMI Audio */
	hd_audio_flag = vpp_set_audio(16, rate, 2);
#endif	
}

void wmt_set_i2s_share_pin()
{
	/* disable GPIO and Pull Down mode */
	GPIO_CTRL_GP10_I2S_BYTE_VAL &= ~0xFF;
	GPIO_CTRL_GP27_BYTE_VAL &= ~(BIT0 | BIT1 | BIT2);

	GPIO_PULL_EN_GP10_I2S_BYTE_VAL &= ~0xFF;
	GPIO_PULL_EN_GP27_BYTE_VAL &= ~(BIT0 | BIT1 | BIT2);

	if (wmt_pdm_module_enable == 0) {
		// [19:18]=b01: select I2SADCLRC
		GPIO_PIN_SHARING_SEL_4BYTE_VAL &= ~BIT19;
		GPIO_PIN_SHARING_SEL_4BYTE_VAL |= BIT18;
		// [17:16]=b01: select I2SADCBCLK
		GPIO_PIN_SHARING_SEL_4BYTE_VAL &= ~BIT17;
		GPIO_PIN_SHARING_SEL_4BYTE_VAL |= BIT16;
	}
	
	GPIO_PIN_SHARING_SEL_4BYTE_VAL |= BIT2;  // enable I2SMCLK
	GPIO_PIN_SHARING_SEL_4BYTE_VAL &= ~BIT3; // select SPDIFO
}
EXPORT_SYMBOL(wmt_set_i2s_share_pin);

static void i2s_init(int mode)
{
#ifdef DEBUG
	int ret;
#endif
	int temp ;
	//unsigned int clock = 0;

	DPRINTK("i2s_ref = %d ", i2s_data->i2s.ref);

	if (++i2s_data->i2s.ref > 1)
		return;

	DBG_DETAIL();
	
	if (!mode) {
		/* set to 24.576MHz */
		auto_pll_divisor(DEV_I2S, CLK_ENABLE , 0, 0);
		auto_pll_divisor(DEV_I2S, SET_PLLDIV, 1, 24576);
		/*clock = auto_pll_divisor(DEV_I2S, GET_FREQ , 0, 0);
		info("%s : clock=%d \n" , __func__, clock);*/

		/* Enable BIT4:ARFP clock, BIT3:ARF clock */
		PMCEU_VAL |= (BIT4 | BIT3);

		/* Enable BIT2:AUD clock */
		PMCE3_VAL |= BIT2;

		wmt_set_i2s_share_pin();
	}

	/* connect DZDRQ8 to ADC0 FIFO, DZDRQ9 to DAC FIFO and DZDRQA to ADC1 FIFO */
	DZDRQ8_CFG_VAL = 0x0;
	DZDRQ9_CFG_VAL = 0x1;
	DZDRQA_CFG_VAL = 0x2;

	DACCFG_VAL = 0x0;
	ADCCFG_VAL = 0x0;

	/* little endian, signed format, enable sample FIFO, 16bit sample, 2 channel */
	ASMPFCFG_VAL = 0x52;

	/* assign ch0 to DAC#0_L, DAC#1_L, DAC#2_L, SPDIF_L,
			  ch1 to DAC#0_R, DAC#1_R, DAC#2_R, SPDIF_R */
	ASMPFCHCFG0_VAL = 0x10101010;

	/* assign ch0 to DAC#3_L, ch1 to DAC#3_R */
	ASMPFCHCFG1_VAL = 0x10;

	/* select ADCDAT0, 16 bits mode, enable AADCFIFO and AADCITF */
	AADCF0CFG_VAL = (AADCITF_ENABLE | AADCF16_ENABLE | AADCF_ENABLE);
	
	/* the sequence must be ARF_ADCCFG first then ARF_DGOCFG, finally ARF_DACCFG while slave mode,
	   otherwise will generate noise when record function is active */ 
	
	/* ADC slave mode, 48K sample rate, I2S mode */
	ADCCFG_VAL = 0x2001;

	i2s_data->i2s.rate = 48000;
	i2s_data->i2s.channels = 2;
	i2s_data->i2s.format = SNDRV_PCM_FORMAT_S16_LE;

#ifdef AUD_SPDIF_ENABLE
	/* B1: 0 -> PCM, 1 -> Bitstream 
	   B24 ~ B27: sample rate */
	/* set ADGO channel status for 48K sample rate */
	DGOCS0A_VAL = 0x02 << 24;
	DGOCS1A_VAL = 0x02 << 24;
	
	/* enable ADGOITF and ADGOCKGEN for 48K sample rate */
	DGOCFG_VAL = 0x82;
#endif
	
	/* enable ADACITF and ADACCKGEN for 44.1K sample rate, I2S mode */
	DACCFG_VAL = 0x402001;

	/* enable HD Audio clock for 48K sample rate or not*/
	HDACKGEN_VAL = 0x12;

#ifdef CONFIG_FB_WMT	
	/* pass information of audio to HDMI Audio */
	hd_audio_flag = vpp_set_audio(16, i2s_data->i2s.rate, i2s_data->i2s.channels);
#endif
	
	if (!hd_audio_flag)
		HDACKGEN_VAL = 0;

	/* audio peri reset */
	temp = AUDPRFRST_VAL;
	
#ifdef AUD_SPDIF_ENABLE
	temp |= (ASMPF_RESET | DACITF_RESET | ADCITF_RESET | DGOITF_RESET);
#else
	temp |= (ASMPF_RESET | DACITF_RESET | ADCITF_RESET);
#endif

	AUDPRFRST_VAL = temp;

	/*
	 request irq
	*/
#ifdef CONFIG_WMT_I2S_INT
	ret = request_irq(i2s_data->i2s.irq,
			wmt_i2s_interrupt,
			SA_INTERRUPT,
			"wmt_alsa_vt1602",
			NULL);
	if (ret)
		printk("%s : unable to request IRQ \n" , __func__);
#endif
}

static void i2s_exit(void)
{
	DBG_DETAIL();

	if (--i2s_data->i2s.ref)
		return;

	DPRINTK("Do i2s_exit ");

#ifdef CONFIG_WMT_I2S_INT	
	free_irq(i2s_data->i2s.irq, NULL);
#endif

	/* Reset counter.*/
	memset(&i2s_data->i2s.ints, 0, sizeof(struct i2s_ints_s));
	return;
}

static int wmt_i2s_dai_startup(struct snd_pcm_substream *substream,
											struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int stream_id = substream->pstr->stream;
	struct audio_stream_a *s = &i2s_data->s[0];
	
	DBG_DETAIL();
	
	s[stream_id].stream = substream;

	runtime->private_data = s;

	return 0;
}

static void wmt_i2s_dai_shutdown(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	DBG_DETAIL();
}

static int wmt_i2s_dai_trigger(struct snd_pcm_substream *substream, int cmd,
								struct snd_soc_dai *dai)
{
	int err = 0;
	int stream_id = substream->pstr->stream;

	DBG_DETAIL();
	
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (stream_id == SNDRV_PCM_STREAM_PLAYBACK) {
			ASMPFCFG_VAL |= ASMPF_ENABLE;
		}
		else if (stream_id == SNDRV_PCM_STREAM_CAPTURE) {
			AADCF0CFG_VAL |= AADCF_ENABLE;
		}
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (stream_id == SNDRV_PCM_STREAM_PLAYBACK) {
			ASMPFCFG_VAL &= ~ASMPF_ENABLE;
		}
		else if (stream_id == SNDRV_PCM_STREAM_CAPTURE) {
			AADCF0CFG_VAL &= ~AADCF_ENABLE;
		}
		
		/*
		mod_timer(&i2s_data->delay_timer, jiffies + HZ / 100);
		*/
		break;
	default:
		err = -EINVAL;
	}

	return err;
}

static int wmt_i2s_dai_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	DBG_DETAIL();
	return 0;
}

static int wmt_i2s_prepare(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int channel, byte;
	int stream_id = substream->pstr->stream;

#ifdef CONFIG_SND_OSSEMUL
	//info("oss.rate=%d, oss.channels=%d", runtime->oss.rate, runtime->oss.channels);
#else
	DBG_DETAIL();
#endif

	byte = (runtime->sample_bits)/8;
	channel = runtime->channels;

	DPRINTK("snd_wmt_alsa_prepare byte = %d, channels = %d", byte, runtime->channels);
	

	if ((runtime->rate != i2s_data->i2s.rate) || (runtime->format != i2s_data->i2s.format) ||
			(runtime->channels != i2s_data->i2s.channels)) {
		info("*** stream_id=%d, rate=%d, format=0x%x channels=%d ***",
			stream_id, runtime->rate, runtime->format, runtime->channels);
		i2s_data->i2s.format = runtime->format;
		i2s_data->i2s.channels = runtime->channels;
	}		
	else { 			
		return 0;
	}	

	i2s_disable();

	/* format setting */
	if (stream_id == SNDRV_PCM_STREAM_PLAYBACK) {
		/* little or big endian check */
		switch (runtime->format) {
			case SNDRV_PCM_FORMAT_S16_BE:
			case SNDRV_PCM_FORMAT_U16_BE:
			case SNDRV_PCM_FORMAT_S24_BE:
			case SNDRV_PCM_FORMAT_U24_BE:
			case SNDRV_PCM_FORMAT_S32_BE:
			case SNDRV_PCM_FORMAT_U32_BE:	
				ASMPFCFG_VAL |= ASMPF_EXCH_ENDIAN;
				break;
			default:	
				break;
		}

		/* unsigned or signed check */
		switch (runtime->format) {
			case SNDRV_PCM_FORMAT_U8:
			case SNDRV_PCM_FORMAT_U16_LE:
			case SNDRV_PCM_FORMAT_U16_BE:
			case SNDRV_PCM_FORMAT_U24_LE:
			case SNDRV_PCM_FORMAT_U24_BE:
			case SNDRV_PCM_FORMAT_U32_LE:
			case SNDRV_PCM_FORMAT_U32_BE:	
				ASMPFCFG_VAL |= ASMPF_EXCH_FMT;
				break;
			default:	
				break;
		}

		/* sample quantization check */
		ASMPFCFG_VAL &= ~(BIT4 | BIT5);
		switch (runtime->format) {
			case SNDRV_PCM_FORMAT_S8:
			case SNDRV_PCM_FORMAT_U8:
				ASMPFCFG_VAL |= ASMPF_8BIT_SMP;
				break;
			case SNDRV_PCM_FORMAT_S16_LE:
			case SNDRV_PCM_FORMAT_S16_BE:
			case SNDRV_PCM_FORMAT_U16_LE:
			case SNDRV_PCM_FORMAT_U16_BE:
				ASMPFCFG_VAL |= ASMPF_16BIT_SMP;
				break;
			case SNDRV_PCM_FORMAT_S32_LE:
			case SNDRV_PCM_FORMAT_S32_BE:
			case SNDRV_PCM_FORMAT_U32_LE:
			case SNDRV_PCM_FORMAT_U32_BE:
				ASMPFCFG_VAL |= ASMPF_32BIT_SMP;
				break;
			default:	
				break;
		}

		/* channel number check */
		ASMPFCFG_VAL &= ~(BIT0 | BIT1 | BIT2 | BIT3);
		ASMPFCFG_VAL |= runtime->channels;

		switch (runtime->channels) {
			case 1:
				ASMPFCHCFG0_VAL = 0x00;
				ASMPFCHCFG1_VAL = 0x00;
				break;
			case 2:
				//ASMPFCHCFG0_VAL = 0x10101010;
				//ASMPFCHCFG1_VAL = 0x10;
				wmt_i2s_dac0_ctrl(wmt_i2s_output);
				break;
			default:
				//ASMPFCHCFG0_VAL = 0x10101010;
				//ASMPFCHCFG1_VAL = 0x10;
				wmt_i2s_dac0_ctrl(wmt_i2s_output);
				break;	
		}
	}
	
	/* sample rate setting */
#ifdef CONFIG_SND_OSSEMUL
	if (runtime->oss.rate) {
		i2s_sample_rate(runtime->oss.rate);
	}
	else {
		i2s_sample_rate(runtime->rate);
	}
#else
	i2s_sample_rate(runtime->rate);
#endif

	i2s_enable();

	/*
	printk("avail_max=%d, rate=%d, channels=%d, period_size=%d, periods=%d, buffer_size=%d, tick_time=%d, \
		min_align=%d, byte_align=%d, frame_bits=%d, sample_bits=%d, sleep_min=%d, xfer_align=%d, boundary=%d\n", 
		runtime->avail_max, runtime->rate, runtime->channels, runtime->period_size, runtime->periods, 
		runtime->buffer_size, runtime->tick_time, runtime->min_align, runtime->byte_align,
		runtime->frame_bits, runtime->sample_bits,
		runtime->sleep_min, runtime->xfer_align, runtime->boundary);
	*/
	return 0;
}

/*
 * This must be called before _set_clkdiv and _set_sysclk since McBSP register
 * cache is initialized here
 */
static int wmt_i2s_dai_set_dai_fmt(struct snd_soc_dai *cpu_dai,
				      unsigned int fmt)
{
	DBG_DETAIL();
	
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;
	default:
		/* Unsupported data format */
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		break;
	default:
		return -EINVAL;
	}

	/* Set bit clock (CLKX/CLKR) and FS polarities */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		/*
		 * Normal BCLK + FS.
		 * FS active low. TX data driven on falling edge of bit clock
		 * and RX data sampled on rising edge of bit clock.
		 */
		break;
	case SND_SOC_DAIFMT_NB_IF:
		break;
	case SND_SOC_DAIFMT_IB_NF:
		break;
	case SND_SOC_DAIFMT_IB_IF:
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_PM
static int wmt_i2s_suspend(struct snd_soc_dai *cpu_dai)
{
	DBG_DETAIL();

	i2s_data->i2s.ref = 0;

	return 0;
}

static int wmt_i2s_resume(struct snd_soc_dai *cpu_dai)
{
	DBG_DETAIL();
	
	i2s_init(1);

	return 0;
}
#else
#define wmt_i2s_suspend		NULL
#define wmt_i2s_resume		NULL
#endif

static struct snd_soc_dai_ops wmt_i2s_dai_ops = {
	.startup	= wmt_i2s_dai_startup,
	.prepare = wmt_i2s_prepare,
	.shutdown = wmt_i2s_dai_shutdown,
	.trigger = wmt_i2s_dai_trigger,
	.hw_params = wmt_i2s_dai_hw_params,
	.set_fmt = wmt_i2s_dai_set_dai_fmt,
};

struct snd_soc_dai_driver wmt_i2s_dai = {
	.suspend = wmt_i2s_suspend,
	.resume = wmt_i2s_resume,
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_FLOAT,
	},
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.ops = &wmt_i2s_dai_ops,
};

static int wmt_i2s_probe(struct platform_device *pdev)
{
	int ret;
	char buf[64];
	int varlen = 64;
	
	DBG_DETAIL();

	ret = wmt_getsyspara("wmt.audio.pdm", buf, &varlen);
	
	if (ret == 0) {
		sscanf(buf, "%d", &wmt_pdm_module_enable);
	}
	
	i2s_data->s[0].dma_cfg = dma_device_cfg_table[AHB1_AUD_DMA_REQ_1];
	i2s_data->s[1].dma_cfg = dma_device_cfg_table[AHB1_AUD_DMA_REQ_0];
	/*init i2s controller*/
	i2s_data->i2s.init(0);
	
	spin_lock_init(&i2s_data->s[0].dma_lock);
	spin_lock_init(&i2s_data->s[1].dma_lock);
	init_timer(&i2s_data->delay_timer);
	i2s_data->delay_timer.function = delay_timer_handler;

	/* register with the ASoC layers */
	ret = snd_soc_register_dai(&pdev->dev, &wmt_i2s_dai);
	if (ret) {
		pr_err("Failed to register DAI: %d\n", ret);
		return ret;
	}
	
	return 0;
}

static int __devexit wmt_i2s_remove(struct platform_device *pdev)
{
	DBG_DETAIL();

	snd_soc_unregister_dai(&pdev->dev);

	return 0;
}

static struct platform_driver wmt_i2s_driver = {
	.probe  = wmt_i2s_probe,
	.remove = __devexit_p(wmt_i2s_remove),
	.driver = {
		.name = "wmt-i2s",
		.owner = THIS_MODULE,
	},
};


static int __init wmt_i2s_init(void)
{
	DBG_DETAIL();

	return platform_driver_register(&wmt_i2s_driver);
}

static void __exit wmt_i2s_exit(void)
{
	DBG_DETAIL();
	
	platform_driver_unregister(&wmt_i2s_driver);
}

module_init(wmt_i2s_init);
module_exit(wmt_i2s_exit);

MODULE_AUTHOR("WonderMedia Technologies, Inc.");
MODULE_DESCRIPTION("WMT [ALSA SoC] driver");
MODULE_LICENSE("GPL");

