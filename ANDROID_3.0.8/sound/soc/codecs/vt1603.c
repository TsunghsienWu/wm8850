/*++ 
 * linux/sound/soc/codecs/vt1603.c
 * WonderMedia audio driver for ALSA
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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/interrupt.h>
#include <asm/irq.h>
#include <linux/workqueue.h>
#include <linux/switch.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>

#include "vt1603.h"
#include <linux/mfd/vt1603/core.h>

#define VT1603_TIMER_INTERVAL    350 // ms

enum {
	WMT_REC_AMIC_IN = 0,
	WMT_REC_DMIC_IN,
	WMT_REC_LINE_IN,
};

/* codec private data */
struct vt1603_priv {
	unsigned int sysclk;
	struct switch_dev hp_switch; // state of headphone insert or not
	struct work_struct work;
	struct snd_soc_codec *codec;
	struct timer_list timer; // timer for detecting headphone
	struct proc_dir_entry *proc;
};

struct vt1603_boardinfo {
	int hp_level; // 0: headset level low; 1: high
	int donot_report_hp_switch;
	int rec_src;  // 0: AMIC-IN; 1: DMIC-IN 
};
static struct vt1603_boardinfo vt1603_boardinfo;

extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);

/*
 * vt1603 register cache
 */
static const u16 vt1603_regs[] = {
	0x0020, 0x00D7, 0x00D7, 0x0004,  /* 0-3 */
	0x0000, 0x0040, 0x0000, 0x00c0,  /* 4-7 */
	0x00c0, 0x0010, 0x0001, 0x0041,  /* 8-11 */
	0x0000, 0x0000, 0x000b, 0x0093,  /* 12-15 */
	0x0004, 0x0016, 0x0026, 0x0060,  /* 16-19 */
	0x001a, 0x0000, 0x0002, 0x0000,  /* 20-23 */
	0x0000, 0x000a, 0x0000, 0x0010,  /* 24-27 */
	0x0000, 0x00e2, 0x0000, 0x0000,  /* 28-31 */
	0x0000, 0x0000, 0x0000, 0x0000,  /* 32-35 */
	0x0000, 0x000c, 0x0054, 0x0000,  /* 36-39 */
	0x0001, 0x000c, 0x000c, 0x000c,  /* 40-43 */
	0x000c, 0x000c, 0x00c0, 0x00d5,  /* 44-47 */
	0x00c5, 0x0012, 0x0085, 0x002b,  /* 48-51 */
	0x00cd, 0x00cd, 0x008e, 0x008d,  /* 52-55 */
	0x00e0, 0x00a6, 0x00a5, 0x0050,  /* 56-59 */
	0x00e9, 0x00cf, 0x0040, 0x0000,  /* 60-63 */
	0x0000, 0x0008, 0x0004, 0x0000,  /* 64-67 */
	0x0040, 0x0000, 0x0040, 0x0000,  /* 68-71 */
	0x0000, 0x0000, 0x0000, 0x0000,  /* 72-75 */
	0x0000, 0x0000, 0x0000, 0x0000,  /* 76-79 */

	0x0000, 0x0000, 0x0000, 0x0000,  /* 80-83 */
	0x0000, 0x0000, 0x0000, 0x0000,  /* 84-87 */
	0x0000, 0x0000, 0x0000, 0x0000,  /* 88-91 */
	0x0000, 0x0000, 0x0000, 0x0000,  /* 92-95 */
	0x00c4, 0x0079, 0x000c, 0x0024,  /* 96-99 */
	0x0016, 0x0016, 0x0060, 0x0002,  /* 100-103 */
	0x005b, 0x0003, 0x0039, 0x0039,  /* 104-107 */
	0x00fe, 0x0012, 0x0034, 0x0000,  /* 108-111 */
	0x0004, 0x00f0, 0x0000, 0x0000,  /* 112-115 */
	0x0000, 0x0000, 0x0003, 0x0000,  /* 116-119 */
	0x0000, 0x0000, 0x0000, 0x0000,  /* 120-123 */
	0x00f2, 0x0020, 0x0023, 0x006e,  /* 124-127 */
	0x0019, 0x0040, 0x0000, 0x0004,  /* 128-131 */
	0x000a, 0x0075, 0x0035, 0x00b0,  /* 132-135 */
	0x0040, 0x0000, 0x0000, 0x0000,  /* 136-139 */
	0x0088, 0x0013, 0x000c, 0x0003,  /* 140-143 */
	0x0000, 0x0000, 0x0008, 0x0000,  /* 144-147 */
	0x0003, 0x0020, 0x0030, 0x0018,  /* 148-151 */
	0x001b,
};
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/

/* Control Interface Functions */

static int vt1603_write(struct snd_soc_codec *codec, 
		unsigned int reg, unsigned int value)
{
	int ret = vt1603_reg_write(codec->control_data, reg, value);
	if (ret == 0)
		((u16 *)codec->reg_cache)[reg] = value;
	return ret;
}

static unsigned int vt1603_read(struct snd_soc_codec *codec, 
		unsigned int reg)
{
	return ((u16 *)codec->reg_cache)[reg];
}

static int vt1603_hw_write(struct snd_soc_codec *codec, 
		unsigned int reg, unsigned int value)
{
	return vt1603_reg_write(codec->control_data, reg, value);
}

static unsigned int vt1603_hw_read(struct snd_soc_codec *codec, 
		unsigned int reg)
{
	u8 val;
	int ret = vt1603_reg_read(codec->control_data, reg, &val);
	if (ret == 0)
		return val;
	else
		return ((u16 *)codec->reg_cache)[reg];
}

/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/

static void vt1603_get_boardinfo(void)
{
	int ret = 0;
	unsigned char buf[64];
	int varlen = sizeof(buf);

	memset(buf, 0x0, sizeof(buf));
	ret = wmt_getsyspara("wmt.audio.i2s", buf, &varlen);
	if (ret == 0) {
		pr_info("wmt.audio.i2s = %s\n", buf);
		sscanf(buf, "vt1603:%d:%d:%d", 
			&vt1603_boardinfo.hp_level,
			&vt1603_boardinfo.donot_report_hp_switch,
			&vt1603_boardinfo.rec_src);
	} 
	else {
		pr_info("wmt.audio.i2s not set, set default value.\n");
		vt1603_boardinfo.hp_level = 0;
		vt1603_boardinfo.donot_report_hp_switch = 0;
		vt1603_boardinfo.rec_src = 0;
	}
}

static int vt1603_readproc(char *page, char **start, off_t off,
		int count, int *eof, void *data)
{
	struct vt1603_priv *vt1603 = data;
	printk("\nvt1603 regs dump:\n");
	vt1603_regs_dump(vt1603->codec->control_data);
	return 0;
}

static void vt1603_codec_reset(struct snd_soc_codec *codec)
{
	unsigned int val;
	
	vt1603_hw_write(codec, 0xc2, 0x01);
	// reset all registers to the default value
	vt1603_hw_write(codec, VT1603_REG_RESET, 0xff);
	vt1603_hw_write(codec, VT1603_REG_RESET, 0x00);

	//reset signal from control interface, reset ADC
	val = vt1603_hw_read(codec, VT1603_R60);
	val &= ~(BIT6 + BIT7);
	vt1603_hw_write(codec, VT1603_R60, val);
	val |= BIT6 + BIT7;
	vt1603_hw_write(codec, VT1603_R60, val);
}

static void vt1603_codec_init(struct snd_soc_codec *codec)
{
	unsigned int val;
	
	// vt1603 touch uses irq low active???
	if (vt1603_boardinfo.hp_level) {
		//hp high
		snd_soc_write(codec, VT1603_R21, 0xfd);
	} 
	else {
		//hp low
		snd_soc_write(codec, VT1603_R21, 0xff);
	}

	// vt1603 codec irq settings
	snd_soc_write(codec, VT1603_R1b, 0xff);
	snd_soc_write(codec, VT1603_R1b, 0xfd);
	snd_soc_write(codec, VT1603_R1c, 0xff);
	snd_soc_write(codec, VT1603_R1d, 0xff);
	snd_soc_write(codec, VT1603_R24, 0x04);
	
	snd_soc_write(codec, VT1603_R95, 0x00);
	snd_soc_write(codec, VT1603_R60, 0xc7);
	snd_soc_write(codec, VT1603_R93, 0x02);
	snd_soc_write(codec, VT1603_R7b, 0x10);
	snd_soc_write(codec, VT1603_R92, 0x2c);

	// r4 is reserved, I don't know more about it.
	snd_soc_write(codec, VT1603_R04, 0x00);
	// r9[5] is reserved, I don't know more about it.
	val = snd_soc_read(codec, VT1603_R09);
	val |= BIT5;
	snd_soc_write(codec, VT1603_R09, val);
	// r79 is reserved, I don't know more about it.
	val = snd_soc_read(codec, VT1603_R79);
	val |= BIT2;
	val &= ~(BIT0+BIT1);
	snd_soc_write(codec, VT1603_R79, val);
	// r7a is reserved, I don't know more about it.
	val = snd_soc_read(codec, VT1603_R7a);
	val |= BIT3+BIT4+BIT7;
	snd_soc_write(codec, VT1603_R7a, val);
	// r87 is reserved, I don't know more about it.
	snd_soc_write(codec, VT1603_R87, 0x90);
	// r88 is reserved, I don't know more about it.
	snd_soc_write(codec, VT1603_R88, 0x28);
	// r8a is reserved, I don't know more about it.
	val = snd_soc_read(codec, VT1603_R8a);
	val |= BIT1;
	snd_soc_write(codec, VT1603_R8a, val);
	// enable CLK_SYS, CLK_DSP, CLK_AIF
	val = snd_soc_read(codec, VT1603_R40);
	val |= BIT4+BIT5+BIT6;
	snd_soc_write(codec, VT1603_R40, val);
	// set DAC sample rate divier=CLK_SYS/1, DAC clock divider=DAC_CLK_512X
	val = snd_soc_read(codec, VT1603_R41);
	val &= ~(BIT3+BIT2+BIT1+BIT0);
	snd_soc_write(codec, VT1603_R41, val);
	// set BCLK rate=CLK_SYS/8
	val = snd_soc_read(codec, VT1603_R42);
	val |= BIT0+BIT1+BIT2;
	val &= ~BIT3;
	snd_soc_write(codec, VT1603_R42, val);
	// enable VREF_SC_DA buffer
	val = snd_soc_read(codec, VT1603_R61);
	val |= BIT7;
	snd_soc_write(codec, VT1603_R61, val);
	// ADC VREF_SC offset voltage
	snd_soc_write(codec, VT1603_R93, 0x20);
	// set mic bias voltage = 90% * AVDD
	val = snd_soc_read(codec, VT1603_R92);
	val |= BIT2;
	snd_soc_write(codec, VT1603_R92, val); 
	// enable bypass for AOW0 parameterized HPF
	val = snd_soc_read(codec, VT1603_R0a);
	val |= BIT6;
	snd_soc_write(codec, VT1603_R0a, val);
	// CPVEE=2.2uF, the ripple frequency ON CPVEE above 22kHz, that will reduce HP noise
	val = snd_soc_read(codec, VT1603_R7a);
	val |= BIT6;
	val &= ~BIT5;
	snd_soc_write(codec, VT1603_R7a, val);

	// set DAC soft mute mode
	val = snd_soc_read(codec, VT1603_R0b);
	val |= BIT1+BIT2;
	snd_soc_write(codec, VT1603_R0b, val);
	// DAC High Voltage Switch Control Signal enable???
	val = snd_soc_read(codec, VT1603_R62);
	val |= BIT7+BIT6;
	val &= ~BIT3;
	snd_soc_write(codec, VT1603_R62, val);
	// enable microphone bias
	val = snd_soc_read(codec, VT1603_R60);
	val |= 0x0f;
	snd_soc_write(codec, VT1603_R60, val);
	// capture pga settings??
	val = snd_soc_read(codec, VT1603_R8e);
	val |= 0x0f;
	snd_soc_write(codec, VT1603_R8e, val);
	// capture pga settings??
	val = snd_soc_read(codec, VT1603_R66);
	val |= BIT1+BIT2+BIT3+BIT4; //
	snd_soc_write(codec, VT1603_R66, val);
	// PGA Zero Cross Detector Enable, Change gain on zero cross only
	val = snd_soc_read(codec, VT1603_R64);
	val |= BIT0;
	snd_soc_write(codec, VT1603_R64, val);
	val = snd_soc_read(codec, VT1603_R65);
	val |= BIT0;
	snd_soc_write(codec, VT1603_R65, val);
	// enable hp output mode
	val = snd_soc_read(codec, VT1603_R68);
	val |= BIT2; 
	snd_soc_write(codec, VT1603_R68, val);
	// enable class-d, unmute channels
	snd_soc_write(codec, VT1603_R7c, 0xe0);

	// set DAC gain update
	val = snd_soc_read(codec, VT1603_R05);
	val |= BIT6+BIT7;
	snd_soc_write(codec, VT1603_R05, val);
	// set class-d AC boost gain=1.6
	snd_soc_write(codec, VT1603_R97, 0x1c);
	val = snd_soc_read(codec, VT1603_R97);
	val = (val & (~0x07)) + 0x04;
	snd_soc_write(codec, VT1603_R97, val);
	// set ADC gain update, enable ADCDAT output
	val = snd_soc_read(codec, VT1603_R00);
	val |= BIT5+BIT6+BIT7;
	snd_soc_write(codec, VT1603_R00, val);

	// DA DRC settings
	//snd_soc_write(codec, VT1603_R0e, 0x0b);
	//snd_soc_write(codec, VT1603_R0f, 0x94);
	//snd_soc_write(codec, VT1603_R10, 0x02);
	//snd_soc_write(codec, VT1603_R11, 0x00);
	//snd_soc_write(codec, VT1603_R12, 0x60);
}

/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/

/* IRQ handle Functions */

static void vt1603_set_switch_state(struct snd_soc_codec *codec)
{
	/* Just report hp state, don't switch hw device, app will handle it. */
	
	struct vt1603_priv *vt1603 = snd_soc_codec_get_drvdata(codec);
	unsigned int val = vt1603_hw_read(codec, VT1603_R21);
	
	if (vt1603_boardinfo.hp_level) {
		if (val & BIT1) {
			val &= ~BIT1;
			snd_soc_write(codec, VT1603_R21, val);
			if (!vt1603_boardinfo.donot_report_hp_switch)
				switch_set_state(&vt1603->hp_switch, 0);
		} 
		else {
			val |= BIT1;
			snd_soc_write(codec, VT1603_R21, val);
			if (!vt1603_boardinfo.donot_report_hp_switch)
				switch_set_state(&vt1603->hp_switch, 1);
		}
	} 
	else {
		if (val & BIT1) {
			val &= ~BIT1;
			snd_soc_write(codec, VT1603_R21, val);
			if (!vt1603_boardinfo.donot_report_hp_switch)
				switch_set_state(&vt1603->hp_switch, 1);
		}
		else {
			val |= BIT1;
			snd_soc_write(codec, VT1603_R21, val);
			if (!vt1603_boardinfo.donot_report_hp_switch)
				switch_set_state(&vt1603->hp_switch, 0);
		}
	}
}

static void vt1603_codec_irq_handle(struct work_struct *work)
{
	struct vt1603_priv *priv = container_of(work, struct vt1603_priv, work);
	struct snd_soc_codec *codec = priv->codec;
	unsigned int val;
	unsigned int times = 0;
	static unsigned int count = 0;
	int hp_state = switch_get_state(&priv->hp_switch);

	/** if switch cld-d to headset, set debounce-times = 3 */
	times = (hp_state == 0) ? 3 : 1;
	val = vt1603_hw_read(codec, VT1603_R51);
	if (val & BIT1) {
		val = vt1603_hw_read(codec, VT1603_R1b);
		val |= BIT1;
		snd_soc_write(codec, VT1603_R1b, val);
		val &= ~BIT1;
		snd_soc_write(codec, VT1603_R1b, val);
		count++; 
	}
	else {
		count = 0;
	}
	if (count == times) {
		pr_info("vt1603: hpdetect\n");
		vt1603_set_switch_state(codec);
		// clear headphone irq mask
		val = vt1603_hw_read(codec,VT1603_R1b);
		val |= BIT1;
		snd_soc_write(codec, VT1603_R1b, val);
		val &= ~BIT1;
		snd_soc_write(codec, VT1603_R1b, val);
		count = 0;
	}

	val = vt1603_hw_read(codec, VT1603_R52);
	if (val & BIT4) {
		pr_info("vt1603: left cld oc\n");
		val = vt1603_hw_read(codec,VT1603_R1c);
		val |= BIT4;
		snd_soc_write(codec, VT1603_R1c, val);
		val &= ~BIT4;
		snd_soc_write(codec, VT1603_R1c, val);
	}
	if (val & BIT3) {
		pr_info("vt1603: right cld oc\n");
		val = vt1603_hw_read(codec,VT1603_R1c);
		val |= BIT3;
		snd_soc_write(codec, VT1603_R1c, val);
		val &= ~BIT3;
		snd_soc_write(codec, VT1603_R1c, val);
	}
}

static void vt1603_timer_handler(unsigned long data)
{
	struct vt1603_priv *vt1603 = (struct vt1603_priv *)data;
	schedule_work(&vt1603->work);
	mod_timer(&vt1603->timer, jiffies+VT1603_TIMER_INTERVAL*HZ/1000);
}

/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/

/* Mixer controls, dapm */

static const DECLARE_TLV_DB_SCALE(digital_tlv, -9550, 50, 1);
static const DECLARE_TLV_DB_SCALE(digital_clv, -4350, 50, 1);
static const DECLARE_TLV_DB_SCALE(adc_boost_clv, 0, 1000, 1);
static const DECLARE_TLV_DB_SCALE(dac_boost_tlv, 0, 600, 0);
static const DECLARE_TLV_DB_SCALE(adc_pga_clv, -1650, 150, 1);
static const DECLARE_TLV_DB_SCALE(analog_tlv, -2100, 300, 1);
static const DECLARE_TLV_DB_SCALE(hp_mixer_clv, -5700, 100, 1);
static const DECLARE_TLV_DB_SCALE(cld_mixer_clv, -300, 300, 1);

static const char *adc_cutoff_freq_txt[] = {
	"Hi-fi mode", 
	"Voice mode 1", 
	"Voice mode 2", 
	"Voice mode 3",
};

static const struct soc_enum adc_cutoff_freq =
SOC_ENUM_SINGLE(VT1603_R00, 3, 4, adc_cutoff_freq_txt);

static const char *lr_txt[] = {
	"Left", "Right"
};

static const char *clamper_limit_voltage_txt[] = {
	"Vih=2.40V, Vil=0.90V, P=0.63W",
	"Vih=2.45V, Vil=0.85V, P=0.72W",
	"Vih=2.50V, ViL=0.80V, P=0.81W",
	"Vih=2.55V, Vil=0.75V, P=0.91W",
	"Vih=2.60V, Vil=0.70V, P=1.02W",
	"Vih=2.65V, Vil=0.65V, P=1.12W",
	"Vih=2.70V, Vil=0.60V, P=1.24W",
	"Vih=2.75V, Vil=0.55V, P=1.36W",
	"Vih=2.80V, Vil=0.50V, P=1.49W",
	"Vih=2.85V, Vil=0.45V, P=1.62W",
	"Vih=2.90V, Vil=0.40V, P=1.76W",
	"Vih=2.95V, Vil=0.35V, P=1.90W",
	"Vih=3.00V, Vil=0.30V, P=2.05W",
	"Vih=3.05V, Vil=0.25V, P=2.20W",
	"Vih=3.10V, Vil=0.20V, P=2.37W",
	"Vih=3.15V, Vil=0.15V, P=2.53W",
};

static const char *clamper_work_voltage_txt[] = {
	"Voh=3.10V, Vol=1.90V, P=0.36W",
	"Voh=3.25V, Vol=1.75V, P=0.56W",
	"Voh=3.40V, Vol=1.60V, P=0.81W",
	"Voh=3.55V, Vol=1.45V, P=1.10W",
	"Voh=3.70V, Vol=1.30V, P=1.44W",
	"Voh=3.85V, Vol=1.15V, P=1.82W",
	"Voh=4.00V, Vol=1.00V, P=2.25W",
	"Voh=4.15V, Vol=0.85V, P=2.72W",
};

static const struct soc_enum dacl_src =
SOC_ENUM_SINGLE(VT1603_R0b, 7, 2, lr_txt);

static const struct soc_enum dacr_src =
SOC_ENUM_SINGLE(VT1603_R0b, 6, 2, lr_txt);

static const struct soc_enum adcl_src =
SOC_ENUM_SINGLE(VT1603_R03, 3, 2, lr_txt);

static const struct soc_enum adcr_src =
SOC_ENUM_SINGLE(VT1603_R03, 2, 2, lr_txt);

static const struct soc_enum clamper_limit_voltage =
SOC_ENUM_SINGLE(VT1603_R87, 4, 16, clamper_limit_voltage_txt);

static const struct soc_enum clamper_work_voltage =
SOC_ENUM_SINGLE(VT1603_R87, 0, 8, clamper_work_voltage_txt);

static const struct snd_kcontrol_new vt1603_snd_controls[] = {
/* Volume */
SOC_DOUBLE_R_TLV("Digital Capture Volume", VT1603_R01, VT1603_R02, 0, 0x7f, 0, digital_clv),
SOC_DOUBLE_R_TLV("Digital Playback Volume", VT1603_R07, VT1603_R08, 0, 0xc0, 0, digital_tlv),
SOC_DOUBLE_R_TLV("Analog Playback Volume", VT1603_R72, VT1603_R73, 0, 0x07, 1, analog_tlv),
SOC_DOUBLE_R_TLV("Input Boost Volume", VT1603_R64, VT1603_R65, 6, 4, 0, adc_boost_clv),
SOC_DOUBLE_R_TLV("Input PGA Volume", VT1603_R64, VT1603_R65, 1, 0x1f, 0, adc_pga_clv),
SOC_DOUBLE_TLV("Output Boost Volume", VT1603_R06, 0, 3, 7, 0, dac_boost_tlv),
SOC_DOUBLE_TLV("Output CLD Volume", VT1603_R91, 5, 2, 7, 0, cld_mixer_clv),
SOC_DOUBLE_R_TLV("Output HP Volume", VT1603_R6a, VT1603_R6b, 0, 0x3f, 0, hp_mixer_clv),

/* Switch */
SOC_SINGLE("EQ Switch", VT1603_R28, 0, 1, 0),

SOC_SINGLE("DAC Mono Mix Switch", VT1603_R0d, 4, 1, 0),
SOC_SINGLE("Left DAC Switch", VT1603_R62, 5, 1, 0),
SOC_SINGLE("Right DAC Switch", VT1603_R62, 4, 1, 0),

SOC_ENUM("Left DAC Source", dacl_src),
SOC_ENUM("Right DAC Source", dacr_src),

SOC_ENUM("Digital Capture Voice Mode", adc_cutoff_freq),
SOC_ENUM("Left ADC Source", adcl_src),
SOC_ENUM("Right ADC Source", adcr_src),

SOC_DOUBLE("Analog Playback Switch", VT1603_R71, 7, 6, 1, 1),
SOC_SINGLE("Digital Playback Switch", VT1603_R0b, 0, 1, 1),
SOC_DOUBLE("Digital Capture Switch", VT1603_R63, 7, 6, 1, 0),

SOC_SINGLE("Headset Switch", VT1603_R68, 4, 1, 1),
SOC_SINGLE("Headfree Switch", VT1603_R25, 1, 1, 0),

SOC_SINGLE("Left CLD Switch", VT1603_R82, 3, 1, 1),
SOC_SINGLE("Right CLD Switch", VT1603_R82, 4, 1, 1),

SOC_ENUM("Limit Voltage of Power Clamper", clamper_limit_voltage),
SOC_ENUM("Voltage of Clamper Work", clamper_work_voltage),
};

/*
 * DAPM Controls
 */

static const struct snd_kcontrol_new linmix_controls[] = {
SOC_DAPM_SINGLE("Left Mic Switch", VT1603_R8e, 5, 1, 0),
SOC_DAPM_SINGLE("Left Linein Switch", VT1603_R8e, 7, 1, 0),
};

static const struct snd_kcontrol_new rinmix_controls[] = {
SOC_DAPM_SINGLE("Right Mic Switch", VT1603_R8e, 4, 1, 0),
SOC_DAPM_SINGLE("Right Linein Switch", VT1603_R8e, 6, 1, 0),
};

static const struct snd_kcontrol_new loutmix_controls[] = {
SOC_DAPM_SINGLE("Left Input Mixer Switch", VT1603_R71, 3, 1, 0),
SOC_DAPM_SINGLE("DACL Switch", VT1603_R71, 1, 1, 0),
};

static const struct snd_kcontrol_new routmix_controls[] = {
SOC_DAPM_SINGLE("Right Input Mixer Switch", VT1603_R71, 2, 1, 0),
SOC_DAPM_SINGLE("DACR Switch", VT1603_R71, 0, 1, 0),
};

static const struct snd_kcontrol_new lcld_controls[] = {
SOC_DAPM_SINGLE("Left Output Mixer Switch", VT1603_R90, 5, 1, 0),
SOC_DAPM_SINGLE("DACL Switch", VT1603_R90, 4, 1, 0),
};

static const struct snd_kcontrol_new rcld_controls[] = {
SOC_DAPM_SINGLE("Right Output Mixer Switch", VT1603_R90, 3, 1, 0),
SOC_DAPM_SINGLE("DACR Switch", VT1603_R90, 2, 1, 0),
};

static const struct snd_kcontrol_new lhp_controls[] = {
SOC_DAPM_SINGLE("Left Output Mixer Switch", VT1603_R69, 5, 1, 0),
SOC_DAPM_SINGLE("DACL Switch", VT1603_R69, 7, 1, 0),
};

static const struct snd_kcontrol_new rhp_controls[] = {
SOC_DAPM_SINGLE("Right Output Mixer Switch", VT1603_R69, 2, 1, 0),
SOC_DAPM_SINGLE("DACR Switch", VT1603_R69, 4, 1, 0),
};

static const struct snd_soc_dapm_widget vt1603_dapm_widgets[] = {
SND_SOC_DAPM_INPUT("Mic"),
SND_SOC_DAPM_INPUT("Linein"),
SND_SOC_DAPM_INPUT("Internal ADC Source"),

SND_SOC_DAPM_PGA("Left Input Boost", VT1603_R67, 7, 0, NULL, 0),
SND_SOC_DAPM_PGA("Right Input Boost", VT1603_R67, 6, 0, NULL, 0),

SND_SOC_DAPM_PGA("Left Input PGA", VT1603_R67, 5, 0, NULL, 0),
SND_SOC_DAPM_PGA("Right Input PGA", VT1603_R67, 4, 0, NULL, 0),


SND_SOC_DAPM_MIXER("Left Input Mixer", VT1603_R66, 6, 1,
		linmix_controls, ARRAY_SIZE(linmix_controls)),
SND_SOC_DAPM_MIXER("Right Input Mixer", VT1603_R66, 5, 1,
		rinmix_controls, ARRAY_SIZE(rinmix_controls)),

SND_SOC_DAPM_ADC("ADCL", "Left Capture", VT1603_R01, 7, 1),
SND_SOC_DAPM_ADC("ADCR", "Right Capture", VT1603_R02, 7, 1),

//SND_SOC_DAPM_DAC("DACL", "Left Playback", VT1603_R62, 5, 0),
//SND_SOC_DAPM_DAC("DACR", "Right Playback", VT1603_R62, 4, 0),

SND_SOC_DAPM_INPUT("DACL"),
SND_SOC_DAPM_INPUT("DACR"),

SND_SOC_DAPM_MIXER("Left Output Mixer", VT1603_R72, 6, 0,
		loutmix_controls, ARRAY_SIZE(loutmix_controls)),
SND_SOC_DAPM_MIXER("Right Output Mixer", VT1603_R73, 6, 0,
		routmix_controls, ARRAY_SIZE(routmix_controls)),

SND_SOC_DAPM_MIXER("Left CLD Mixer", VT1603_R91, 1, 0,
		lcld_controls, ARRAY_SIZE(lcld_controls)),
SND_SOC_DAPM_MIXER("Right CLD Mixer", VT1603_R91, 0, 0,
		rcld_controls, ARRAY_SIZE(rcld_controls)),

SND_SOC_DAPM_PGA("Left CLD PGA", VT1603_R82, 3, 1, NULL, 0),
SND_SOC_DAPM_PGA("Right CLD PGA", VT1603_R82, 4, 1, NULL, 0),

SND_SOC_DAPM_MIXER("Left HP Mixer", VT1603_R68, 1, 1,
		lhp_controls, ARRAY_SIZE(lhp_controls)),
SND_SOC_DAPM_MIXER("Right HP Mixer", VT1603_R68, 0, 1,
		rhp_controls, ARRAY_SIZE(rhp_controls)),

SND_SOC_DAPM_OUTPUT("Internal DAC Sink"),
SND_SOC_DAPM_OUTPUT("Left HP"),
SND_SOC_DAPM_OUTPUT("Right HP"),
SND_SOC_DAPM_OUTPUT("Left SPK"),
SND_SOC_DAPM_OUTPUT("Right SPK"),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/* Make DACs turn on when playing even if not mixed into any outputs */
	//{"Internal DAC Sink", NULL, "DACL"},
	//{"Internal DAC Sink", NULL, "DACR"},

	/* Make ADCs turn on when recording even if not mixed from any inputs */
	//{"ADCL", NULL, "Internal ADC Source"},
	//{"ADCR", NULL, "Internal ADC Source"},

	
	/* Capture route */
	
	{ "Left Input Mixer", "Left Linein Switch", "Linein" },
	{ "Left Input Mixer", "Left Mic Switch", "Mic" },
	{ "Right Input Mixer", "Right Linein Switch", "Linein" },
	{ "Right Input Mixer", "Right Mic Switch", "Mic" },

	{ "Left Input Boost", NULL, "Left Input Mixer"},
	{ "Right Input Boost", NULL, "Right Input Mixer"},

	{ "Left Input PGA", NULL, "Left Input Boost"},
	{ "Right Input PGA", NULL, "Right Input Boost"},

	{ "ADCL", NULL, "Left Input PGA" },
	{ "ADCR", NULL, "Right Input PGA" },

	{ "Left Input Mixer", NULL, "Left Input PGA" },
	{ "Right Input Mixer", NULL, "Right Input PGA" },
	
	/* Playback route */
	
	{ "Left Output Mixer", "Left Input Mixer Switch", "Left Input Mixer" },
	{ "Left Output Mixer", "DACL Switch", "DACL" },
	{ "Right Output Mixer", "Right Input Mixer Switch", "Right Input Mixer" },
	{ "Right Output Mixer", "DACR Switch", "DACR" },
	
	{ "Left CLD Mixer", "Left Output Mixer Switch", "Left Output Mixer" },
	{ "Left CLD Mixer", "DACL Switch", "DACL" },
	{ "Right CLD Mixer", "Right Output Mixer Switch", "Right Output Mixer" },
	{ "Right CLD Mixer", "DACR Switch", "DACR" },

	{ "Left HP Mixer", "Left Output Mixer Switch", "Left Output Mixer" },
	{ "Left HP Mixer", "DACL Switch", "DACL" },
	{ "Right HP Mixer", "Right Output Mixer Switch", "Right Output Mixer" },
	{ "Right HP Mixer", "DACR Switch", "DACR" },

	{ "Left CLD PGA", NULL, "Left CLD Mixer" },
	{ "Right CLD PGA", NULL, "Right CLD Mixer" },

	{ "Left HP", NULL, "Left HP Mixer" },
	{ "Right HP", NULL, "Right HP Mixer" },
	
	{ "Left SPK", NULL, "Left CLD PGA" },
	{ "Right SPK", NULL, "Right CLD PGA" },
};

/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/

/* codec dai functions */

struct _coeff_div {
	u32 mclk;
	u32 rate;
	u16 fs;
	u8 sr;
	u8 bclk_div;
};

/* codec hifi mclk clock divider coefficients */
static const struct _coeff_div coeff_div[] = {
	/* 8k */
	{12288000, 8000, 1536, 0x4, 0x0},
	/* 11.025k */
	{11289600, 11025, 1024, 0x8, 0x0},
	/* 16k */
	{12288000, 16000, 768, 0x5, 0x0},
	/* 22.05k */
	{11289600, 22050, 512, 0x9, 0x0},
	/* 32k */
	{12288000, 32000, 384, 0x7, 0x0},
	/* 44.1k */
	{11289600, 44100, 256, 0x6, 0x07},
	/* 48k */
	{12288000, 48000, 256, 0x0, 0x07},
	/* 96k */
	{12288000, 96000, 128, 0x1, 0x04},
};

static inline int get_coeff(int mclk, int rate)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(coeff_div); i++) {
		if (coeff_div[i].rate == rate && coeff_div[i].mclk == mclk)
			return i;
	}
	return -EINVAL;
}

static int vt1603_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct vt1603_priv *vt1603 = snd_soc_codec_get_drvdata(codec);
	switch (freq) {
	case 11289600:
	case 12000000:
	case 12288000:
	case 16934400:
	case 18432000:
		vt1603->sysclk = freq;
		return 0;
	}
	return -EINVAL;
}

static int vt1603_set_dai_fmt(struct snd_soc_dai *codec_dai, 
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 iface = snd_soc_read(codec, VT1603_R19);
	
	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		iface |= BIT5 ;
		break;	
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		iface |= 0x0002;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface |= 0x0001;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface |= 0x0003;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		iface |= 0x0013;
		break;
	default:
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_IF:
		iface |= 0x0090;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		iface |= 0x0080;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		iface |= 0x0010;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_write(codec, VT1603_R19, iface);
	return 0;
}

static int vt1603_pcm_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct vt1603_priv *vt1603 = snd_soc_codec_get_drvdata(codec);
	u16 val;
	u16 iface = snd_soc_read(codec, VT1603_R19) & 0x73;
	int coeff = get_coeff(vt1603->sysclk, params_rate(params));
	
	/* bit size */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		iface |= 0x04;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:	
		iface |= 0x08;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		iface |= 0x0c;
		break;
	}
	
	/* set iface & srate */
	snd_soc_write(codec, VT1603_R19, iface);
	
	if (coeff >= 0) {
		val = snd_soc_read(codec, VT1603_R42);
		val &= 0xf0;
		val |= coeff_div[coeff].bclk_div;
		snd_soc_write(codec, VT1603_R42, val);

		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			val = snd_soc_read(codec, VT1603_R05);
			val &= 0xf0;
			val |= coeff_div[coeff].sr;
			snd_soc_write(codec, VT1603_R05, val);
		}
               
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			val = snd_soc_read(codec, VT1603_R03);
			val &= 0x0f;
			val |= ((coeff_div[coeff].sr<<4) & 0xf0);
			snd_soc_write(codec, VT1603_R03, val);
		}
	}

	return 0;
}

/*static int vt1603_mute(struct snd_soc_dai *dai, int mute)
{ 
	struct snd_soc_codec *codec = dai->codec;
	u16 val = snd_soc_read(codec, VT1603_R0b);
	if (mute)
		val |= BIT0;
	else
		val &= ~BIT0;
	snd_soc_write(codec, VT1603_R0b, val);
	return 0;
}*/

static int vt1603_set_bias_level(struct snd_soc_codec *codec, 
		enum snd_soc_bias_level level)
{
	u16 val;
	
	switch (level) {
	case SND_SOC_BIAS_ON:
		break;
		
	case SND_SOC_BIAS_PREPARE:
		val = snd_soc_read(codec, VT1603_R96);
		val |= BIT4+BIT5;
		snd_soc_write(codec, VT1603_R96, val);
		break;
		
	case SND_SOC_BIAS_STANDBY:
		break;
	
	case SND_SOC_BIAS_OFF:
		break;
	}
	
	codec->dapm.bias_level = level;
	return 0;
}

#define VT1603_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
	SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_44100 | \
	SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000)

#define VT1603_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
	SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_U8)

static struct snd_soc_dai_ops vt1603_dai_ops = {
	.hw_params    = vt1603_pcm_hw_params,
	//.digital_mute = vt1603_mute,
	.set_fmt      = vt1603_set_dai_fmt,
	.set_sysclk   = vt1603_set_dai_sysclk,
};

struct snd_soc_dai_driver vt1603_dai = {
	.name     = "VT1603",
	.playback = {
		.stream_name  = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates        = VT1603_RATES,
		.formats      = VT1603_FORMATS,
	},
	.capture  = {
		.stream_name  = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates        = VT1603_RATES,
		.formats      = VT1603_FORMATS,
	},
	.ops      = &vt1603_dai_ops,
};

/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/

/* codec driver functions */

static int vt1603_codec_suspend(struct snd_soc_codec *codec, pm_message_t state)
{
	struct vt1603_priv *vt1603 = snd_soc_codec_get_drvdata(codec);
	del_timer_sync(&vt1603->timer);
	return 0;
}


static int vt1603_codec_resume(struct snd_soc_codec *codec)
{
	struct vt1603_priv *vt1603 = snd_soc_codec_get_drvdata(codec);
	int reg;
	u16 *cache = codec->reg_cache;
	unsigned int val;
	int state = switch_get_state(&vt1603->hp_switch);
	
	// reset vt1603 codec after resume
	vt1603_codec_reset(codec);
	
	for (reg = 0; reg < ARRAY_SIZE(vt1603_regs); reg++) {
		if (reg == VT1603_REG_RESET)
			continue;
		snd_soc_write(codec, reg, cache[reg]);
	}

	// restore headphone irq status, some wrong intterrupts may happen in suspend
	val = vt1603_hw_read(codec, VT1603_R21);
	if ((vt1603_boardinfo.hp_level && state) ||
			(!vt1603_boardinfo.hp_level && !state)) {
		val |= BIT1;
		snd_soc_write(codec, VT1603_R21, val);
	}
	else {
		val &= ~BIT1;
		snd_soc_write(codec, VT1603_R21, val);
	}
	
	// open headphone detect
	val = vt1603_hw_read(codec,VT1603_R1b);
	val |= BIT1;
	snd_soc_write(codec, VT1603_R1b, val);
	val &= ~BIT1;
	snd_soc_write(codec, VT1603_R1b, val);

	// clear touch interrupt status??? why here??
	snd_soc_write(codec, 0xca, 0x0f);

	mod_timer(&vt1603->timer, jiffies + 10*HZ/1000);	
	return 0;
}

static int vt1603_codec_probe(struct snd_soc_codec *codec)
{
	struct vt1603_priv *vt1603;

	vt1603 = kzalloc(sizeof(struct vt1603_priv), GFP_KERNEL);
	if (vt1603 == NULL)
		return -ENOMEM;
	snd_soc_codec_set_drvdata(codec, vt1603);

	// get struct(vt1603) defined in vt1603-core
	codec->control_data = dev_get_platdata(codec->dev); 
	
	vt1603->codec = codec;
	vt1603->hp_switch.name = "h2w";
	switch_dev_register(&vt1603->hp_switch);

	vt1603_codec_reset(codec);
	vt1603_codec_init(codec);
	vt1603_set_bias_level(codec, SND_SOC_BIAS_PREPARE);    


	INIT_WORK(&vt1603->work, vt1603_codec_irq_handle);
	init_timer(&vt1603->timer);
	vt1603->timer.data = (unsigned long)vt1603;
	vt1603->timer.function = vt1603_timer_handler;
	// start to detect hp
	mod_timer(&vt1603->timer, jiffies + 2*HZ);

	vt1603->proc = create_proc_read_entry("vt1603_dumpregs", 0666, NULL, 
			vt1603_readproc, vt1603);
	return 0;
}

static int vt1603_codec_remove(struct snd_soc_codec *codec)
{
	struct vt1603_priv *vt1603 = snd_soc_codec_get_drvdata(codec);

	remove_proc_entry("vt1603_dumpregs", NULL);
	switch_dev_unregister(&vt1603->hp_switch);
	del_timer_sync(&vt1603->timer);
	vt1603_set_bias_level(codec, SND_SOC_BIAS_OFF);
	kfree(vt1603);
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_vt1603 = {
	.probe =	vt1603_codec_probe,
	.remove =	vt1603_codec_remove,
	.suspend =	vt1603_codec_suspend,
	.resume =	vt1603_codec_resume,
	.read =		vt1603_read,
	.write =	 vt1603_write,
	.set_bias_level = vt1603_set_bias_level,
	.reg_cache_size = ARRAY_SIZE(vt1603_regs),
	.reg_word_size = sizeof(u16),
	.reg_cache_default = vt1603_regs,

	.controls = vt1603_snd_controls,
	.num_controls = ARRAY_SIZE(vt1603_snd_controls),
	.dapm_widgets = vt1603_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(vt1603_dapm_widgets),
	.dapm_routes = audio_map,
	.num_dapm_routes = ARRAY_SIZE(audio_map),
};

static int __devinit vt1603_probe(struct platform_device *pdev)
{
	return snd_soc_register_codec(&pdev->dev, &soc_codec_dev_vt1603,
			&vt1603_dai, 1);
}

static int __devexit vt1603_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static struct platform_driver vt1603_codec_driver = {
	.driver = {
		   .name = "vt1603-codec",
		   .owner = THIS_MODULE,
		   },
	.probe = vt1603_probe,
	.remove = __devexit_p(vt1603_remove),
};

static __init int vt1603_init(void)
{
	vt1603_get_boardinfo();
	return platform_driver_register(&vt1603_codec_driver);
}
module_init(vt1603_init);

static __exit void vt1603_exit(void)
{
	platform_driver_unregister(&vt1603_codec_driver);
}
module_exit(vt1603_exit);


MODULE_DESCRIPTION("WMT [ALSA SoC] driver");
MODULE_AUTHOR("WonderMedia Technologies, Inc.");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:vt1603-codec");
