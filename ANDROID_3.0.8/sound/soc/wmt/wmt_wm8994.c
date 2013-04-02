/*
 * wmt_wm8994.c
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Author: Chanwoo Choi <cw00.choi@samsung.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <sound/soc.h>
#include <sound/jack.h>

#include <asm/mach-types.h>
#include <mach/gpio.h>

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc-dapm.h>
#include <sound/hwdep.h>

#include <asm/mach-types.h>
#include <mach/hardware.h>

#include "wmt-soc.h"
#include "wmt-pcm.h"
#include "../codecs/wm8994.h"
#include <linux/mfd/wm8994/registers.h>
#include <linux/mfd/wm8994/core.h>

static struct snd_soc_card wmt;
static struct platform_device *wmt_snd_device;
static int wmt_incall = 0;

extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);
extern void wmt_set_i2s_share_pin();

static int WFD_flag = 0;

static const struct snd_pcm_hardware wmt_voice_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_INTERLEAVED |
				  SNDRV_PCM_INFO_PAUSE |
				  SNDRV_PCM_INFO_RESUME,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE,
	.rate_min		= 8000,
	.rate_max		= 8000,
	.period_bytes_min	= 16,
	.period_bytes_max	= 4*1024,
	.periods_min		= 2,
	.periods_max		= 16,
	.buffer_bytes_max	= 16*1024,
	//.fifo_size		= 32,
};

static int wmt_hwdep_open(struct snd_hwdep *hw, struct file *file)
{
	if ((file->f_flags & O_RDWR) && (WFD_flag))
		return -EBUSY;
	else if (file->f_flags & O_SYNC)
		WFD_flag = 1;
	return 0;
}

static int wmt_hwdep_release(struct snd_hwdep *hw, struct file *file)
{
	WFD_flag = 0;
	return 0;
}

static int wmt_hwdep_mmap(struct snd_hwdep *hw, struct file *file, struct vm_area_struct *vma)
{
	vma->vm_flags |= VM_IO | VM_RESERVED;
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	if (remap_pfn_range(vma, vma->vm_start, (vma->vm_pgoff),
			vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
       	printk(KERN_ERR "*E* remap page range failed: vm_pgoff=0x%x ", (unsigned int)vma->vm_pgoff);
       	return -EAGAIN;    
    }
    return 0;
}

static int wmt_hwdep_ioctl(struct snd_hwdep *hw, struct file *file, unsigned int cmd, unsigned long arg)
{
	int *value;
	WFDStrmInfo_t *info;
	
	switch (cmd) {
	case WMT_SOC_IOCTL_HDMI:
		value = (int __user *)arg;
		
		if (*value > 1) {
			printk(KERN_ERR "Not supported status for HDMI Audio %d", *value);
			return 0;
		}	
		wmt_i2s_dac0_ctrl(*value);
		return 0;
		
	case WMT_SOC_IOCTL_WFD_START:
		return copy_to_user( (void *)arg, (const void __user *) wmt_pcm_wfd_get_buf(), sizeof(unsigned int));
		
	case WMT_SOC_IOCTL_GET_STRM:
		info = (WFDStrmInfo_t *)wmt_pcm_wfd_get_strm((WFDStrmInfo_t *)arg);
		return __put_user((int)info, (unsigned int __user *) arg);

	case WMT_SOC_IOCTL_WFD_STOP:
		wmt_pcm_wfd_stop();
		return 0;
	}
	
	printk(KERN_ERR "Not supported ioctl for WMT-HWDEP");
	return -ENOIOCTLCMD;
}

static void wmt_soc_hwdep_new(struct snd_soc_codec *codec)
{
	struct snd_hwdep *hwdep;

	if (snd_hwdep_new(codec->card->snd_card, "WMT-HWDEP", 0, &hwdep) < 0) {
		printk(KERN_ERR "create WMT-HWDEP fail");
		return;
	}

	sprintf(hwdep->name, "WMT-HWDEP %d", 0);

	printk("create %s success", hwdep->name);
	
	hwdep->iface = SNDRV_HWDEP_IFACE_WMT;
	hwdep->ops.open = wmt_hwdep_open;
	hwdep->ops.ioctl = wmt_hwdep_ioctl;
	hwdep->ops.release = wmt_hwdep_release;
	hwdep->ops.mmap = wmt_hwdep_mmap;

	return;
}

static int wmt_snd_suspend_post(struct snd_soc_card *card)
{
	/* Disable BIT15:I2S clock, BIT4:ARFP clock, BIT3:ARF clock */
	PMCEU_VAL &= ~(BIT15 | BIT4 | BIT3);
	return 0;
}

static int wmt_snd_resume_pre(struct snd_soc_card *card)
{
	/*
	 *  Enable MCLK before VT1602 codec enable,
	 *  otherwise the codec will be disabled.
	 */
	/* set to 24.576MHz */
	auto_pll_divisor(DEV_I2S, CLK_ENABLE , 0, 0);
	auto_pll_divisor(DEV_I2S, SET_PLLDIV, 1, 24576);

	/* Enable BIT4:ARFP clock, BIT3:ARF clock */
	PMCEU_VAL |= (BIT4 | BIT3);

	/* Enable BIT2:AUD clock */
	PMCE3_VAL |= BIT2;

	wmt_set_i2s_share_pin();

	return 0;
}

static int wmt_wm8994_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	
	/* HeadPhone */
	snd_soc_dapm_enable_pin(dapm, "HPOUT1R");
	snd_soc_dapm_enable_pin(dapm, "HPOUT1L");

	/* MicIn */
	snd_soc_dapm_enable_pin(dapm, "IN1LN");
	snd_soc_dapm_enable_pin(dapm, "IN1RN");

	/* LineIn */
	snd_soc_dapm_enable_pin(dapm, "IN2LN");
	snd_soc_dapm_enable_pin(dapm, "IN2RN");

	/* Other pins */
	snd_soc_dapm_enable_pin(dapm, "HPOUT2P");
	snd_soc_dapm_enable_pin(dapm, "HPOUT2N");
	snd_soc_dapm_enable_pin(dapm, "SPKOUTLN");
	snd_soc_dapm_enable_pin(dapm, "SPKOUTLP");
	snd_soc_dapm_enable_pin(dapm, "SPKOUTRP");
	snd_soc_dapm_enable_pin(dapm, "SPKOUTRN");
	snd_soc_dapm_enable_pin(dapm, "LINEOUT1N");
	snd_soc_dapm_enable_pin(dapm, "LINEOUT1P");
	snd_soc_dapm_enable_pin(dapm, "LINEOUT2N");
	snd_soc_dapm_enable_pin(dapm, "LINEOUT2P");
	snd_soc_dapm_enable_pin(dapm, "IN1LP");
	snd_soc_dapm_enable_pin(dapm, "IN2LP:VXRN");
	snd_soc_dapm_enable_pin(dapm, "IN1RP");
	snd_soc_dapm_enable_pin(dapm, "IN2RP:VXRP");

	snd_soc_dapm_sync(dapm);

	wmt_soc_hwdep_new(rtd->codec);

	return 0;
}

static int wmt_hifi_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	unsigned int pll_in  = 48000;
	unsigned int pll_out = 12000000;

	/* Set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_CBS_CFS | SND_SOC_DAIFMT_I2S);
	if (ret< 0)
		return ret;

	/* Set cpu DAI configuration for I2S */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S);
	if (ret < 0)
		return ret;

	/* Set the codec system clock for DAC and ADC */			
	ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL1, WM8994_FLL_SRC_LRCLK,
			pll_in, pll_out);
	if (ret < 0)
		return ret;
	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_FLL1, pll_out,
			SND_SOC_CLOCK_IN);

	if (!wmt_incall)
		snd_soc_update_bits(codec_dai->codec, WM8994_CLOCKING_1, WM8994_SYSCLK_SRC, 0);
		//wm8994_set_bits(codec_dai->codec->control_data, WM8994_CLOCKING_1, WM8994_SYSCLK_SRC, 0);
	
	return 0;
}

static int wmt_voice_startup(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret;

	ret = snd_soc_set_runtime_hwparams(substream, &wmt_voice_hardware);

	/* Ensure that buffer size is a multiple of period size */
	ret = snd_pcm_hw_constraint_integer(runtime,SNDRV_PCM_HW_PARAM_PERIODS);
	return ret;
}

static int wmt_voice_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret = 0;
	unsigned int pll_in  = 2048000;
	unsigned int pll_out = 12288000;
	
	if (params_rate(params) != 8000)
		return -EINVAL;

	/* Set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_DSP_A |
			SND_SOC_DAIFMT_IB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* Set the codec system clock for DAC and ADC */			
	ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL2, WM8994_FLL_SRC_BCLK,
			pll_in, pll_out);
	if (ret < 0)
		return ret;
	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_FLL2, pll_out,
			SND_SOC_CLOCK_IN);

	wmt_incall = 1;
	return ret;
}

static int wmt_voice_hw_free(struct snd_pcm_substream *substream)
{
	//struct snd_soc_pcm_runtime *rtd = substream->private_data;
	//struct snd_soc_dai *codec_dai = rtd->codec_dai;
	//wm8994_set_bits(codec_dai->codec->control_data, WM8994_CLOCKING_1, WM8994_SYSCLK_SRC, 0);
	wmt_incall = 0;
	return 0;
}

static struct snd_soc_ops wmt_hifi_ops = {
	.hw_params = wmt_hifi_hw_params,
};

static struct snd_soc_ops wmt_voice_ops = {
	.startup   = wmt_voice_startup,
	.hw_params = wmt_voice_hw_params,
	.hw_free   = wmt_voice_hw_free,
};

static struct snd_soc_dai_driver voice_dai[] = {
	{
		.name = "wmt-voice-dai",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	},
};

static struct snd_soc_dai_link wmt_dai[] = {
	{
		.name = "WM8994",
		.stream_name = "WM8994 HiFi",
		.cpu_dai_name = "wmt-i2s.0",
		.codec_dai_name = "wm8994-aif1",
		.platform_name = "wmt-audio-pcm.0",
		.codec_name = "wm8994-codec",
		.init = wmt_wm8994_init,
		.ops = &wmt_hifi_ops,
	},
	{
		.name = "WM8994 Voice",
		.stream_name = "Voice",
		.cpu_dai_name = "wmt-voice-dai",
		.codec_dai_name = "wm8994-aif2",
		.codec_name = "wm8994-codec",
		.ops = &wmt_voice_ops,
	},
};

static struct snd_soc_card wmt = {
	.name = "WMT_WM8994",
	.dai_link = wmt_dai,
	.num_links = ARRAY_SIZE(wmt_dai),
	.suspend_post = wmt_snd_suspend_post,
	.resume_pre = wmt_snd_resume_pre,
};

static int __init wmt_init(void)
{
	int ret;
	char buf[128];
	int len = ARRAY_SIZE(buf);

	/* read u-boot parameter to decide wmt_dai_name and wmt_codec_name */
	if (wmt_getsyspara("wmt.audio.i2s", buf, &len) != 0)
		return -EIO;

	if (strncmp(buf, "wm8994", strlen("wm8994")))
		return -EIO;

	wmt_snd_device = platform_device_alloc("soc-audio", -1);
	if (!wmt_snd_device)
		return -ENOMEM;

	/* register voice DAI here */
	ret = snd_soc_register_dais(&wmt_snd_device->dev, voice_dai, ARRAY_SIZE(voice_dai));
	if (ret) {
		platform_device_put(wmt_snd_device);
		return ret;
	}

	platform_set_drvdata(wmt_snd_device, &wmt);
	ret = platform_device_add(wmt_snd_device);

	if (ret) {
		snd_soc_unregister_dai(&wmt_snd_device->dev);
		platform_device_put(wmt_snd_device);
	}

	return ret;
}

static void __exit wmt_exit(void)
{
	snd_soc_unregister_dai(&wmt_snd_device->dev);
	platform_device_unregister(wmt_snd_device);
}

module_init(wmt_init);
module_exit(wmt_exit);

/* Module information */
MODULE_DESCRIPTION("ALSA SoC WM8994 WMT(wm8950)");
MODULE_AUTHOR("Loonzhong <Loonzhong@wondermedia.com.cn>");
MODULE_LICENSE("GPL");
