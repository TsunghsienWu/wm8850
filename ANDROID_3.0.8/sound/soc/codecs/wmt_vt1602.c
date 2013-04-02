/*++ 
 * linux/sound/soc/codecs/wmt_vt1602.c
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


#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "wmt_vt1602.h"

#define VT1602_VERSION "0.20"
#define I2C_DRIVERID_VT1602     90

/*
 * Debug
 */
#define AUDIO_NAME "VT1602"
//#define WMT_VT1602_DEBUG 1
//#define WMT_VT1602_DEBUG_DETAIL 1

#ifdef WMT_VT1602_DEBUG
#define DPRINTK(format, arg...) \
	printk(KERN_INFO AUDIO_NAME ": " format "\n" , ## arg)
#else
#define DPRINTK(format, arg...) do {} while (0)
#endif

#ifdef WMT_VT1602_DEBUG_DETAIL
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

/* codec private data */
struct vt1602_priv {
	unsigned int sysclk;
	enum snd_soc_control_type control_type;
	struct snd_pcm_substream *master_substream;
	struct snd_pcm_substream *slave_substream;
};

/*
 * vt1602 register cache
 * We can't read the VT1602 register space when we
 * are using 2 wire for device control, so we cache them instead.
 */
static const u16 vt1602_reg[] = {
	0x0050, 0x0050, 0x007f, 0x007f,  /*  0 */
	0x0000, 0x0008, 0x0000, 0x000a,  /*  4 */
	0x0000, 0x0000, 0x00fc, 0x00fc,  /*  8 */
	0x000f, 0x000f, 0x0000, 0x0000,  /* 12 */
	0x0000, 0x007b, 0x0000, 0x0032,  /* 16 */
	0x0000, 0x00c3, 0x00c3, 0x00c0,  /* 20 */
	0x0000, 0x0000, 0x0000, 0x0000,  /* 24 */
	0x0000, 0x0000, 0x0000, 0x0000,  /* 28 */
	0x0000, 0x0000, 0x0100, 0x0000,  /* 32 */
	0x0000, 0x0100, 0x0050, 0x0050,  /* 36 */
	0x0079, 0x0079, 0x0079,          /* 40 */
};

extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);

#define vt1602_reset(c)	snd_soc_write(c, VT1602_RESET, 0)

/*
 * VT1602 Controls
 */
static const char *vt1602_bass[] = {"Linear Control", "Adaptive Boost"};
static const char *vt1602_bass_filter[] = { "130Hz @ 48kHz", "200Hz @ 48kHz" };
static const char *vt1602_treble[] = {"8kHz", "4kHz"};
static const char *vt1602_3d_lc[] = {"200Hz", "500Hz"};
static const char *vt1602_3d_uc[] = {"2.2kHz", "1.5kHz"};
static const char *vt1602_3d_func[] = {"Capture", "Playback"};
static const char *vt1602_alc_func[] = {"Off", "Right", "Left", "Stereo"};
static const char *vt1602_ng_type[] = {"Constant PGA Gain",
	"Mute ADC Output"};
static const char *vt1602_line_mux[] = {"Line 1", "Line 2", "Line 3", "PGA",
	"Differential"};
static const char *vt1602_pga_sel[] = {"Line 1", "Line 2", "Line 3",
	"Differential"};
static const char *vt1602_out3[] = {"VREF", "ROUT1 + Vol", "MonoOut",
	"ROUT1"};
static const char *vt1602_diff_sel[] = {"Line 1", "Line 2"};
static const char *vt1602_adcpol[] = {"Normal", "L Invert", "R Invert",
	"L + R Invert"};
static const char *vt1602_deemph[] = {"None", "32Khz", "44.1Khz", "48Khz"};
static const char *vt1602_mono_mux[] = {"Stereo", "Mono (Left)",
	"Mono (Right)", "Digital Mono"};

static const struct soc_enum vt1602_enum[] = {
SOC_ENUM_SINGLE(VT1602_BASS, 7, 2, vt1602_bass),
SOC_ENUM_SINGLE(VT1602_BASS, 6, 2, vt1602_bass_filter),
SOC_ENUM_SINGLE(VT1602_TREBLE, 6, 2, vt1602_treble),
SOC_ENUM_SINGLE(VT1602_3D, 5, 2, vt1602_3d_lc),
SOC_ENUM_SINGLE(VT1602_3D, 6, 2, vt1602_3d_uc),
SOC_ENUM_SINGLE(VT1602_3D, 7, 2, vt1602_3d_func),
SOC_ENUM_SINGLE(VT1602_ALC1, 7, 4, vt1602_alc_func),
SOC_ENUM_SINGLE(VT1602_NGATE, 1, 2, vt1602_ng_type),
SOC_ENUM_SINGLE(VT1602_LOUTM1, 0, 5, vt1602_line_mux),
SOC_ENUM_SINGLE(VT1602_ROUTM1, 0, 5, vt1602_line_mux),
SOC_ENUM_SINGLE(VT1602_LADCIN, 6, 4, vt1602_pga_sel), /* 10 */
SOC_ENUM_SINGLE(VT1602_RADCIN, 6, 4, vt1602_pga_sel),
SOC_ENUM_SINGLE(VT1602_ADCTL2, 7, 4, vt1602_out3),
SOC_ENUM_SINGLE(VT1602_ADCIN, 8, 2, vt1602_diff_sel),
SOC_ENUM_SINGLE(VT1602_ADCDAC, 5, 4, vt1602_adcpol),
SOC_ENUM_SINGLE(VT1602_ADCDAC, 1, 4, vt1602_deemph),
SOC_ENUM_SINGLE(VT1602_ADCIN, 6, 4, vt1602_mono_mux), /* 16 */

};

static const struct snd_kcontrol_new vt1602_snd_controls[] = {

SOC_DOUBLE_R("Capture Volume", VT1602_LINVOL, VT1602_RINVOL, 0, 63, 0),
SOC_DOUBLE_R("Capture ZC Switch", VT1602_LINVOL, VT1602_RINVOL, 6, 1, 0),
SOC_DOUBLE_R("Capture Switch", VT1602_LINVOL, VT1602_RINVOL, 7, 1, 1),

SOC_DOUBLE_R("Headphone Playback ZC Switch", VT1602_LOUT1V,
	VT1602_ROUT1V, 7, 1, 0),
SOC_DOUBLE_R("Speaker Playback ZC Switch", VT1602_LOUT2V,
	VT1602_ROUT2V, 7, 1, 0),

SOC_ENUM("Playback De-emphasis", vt1602_enum[15]),

SOC_ENUM("Capture Polarity", vt1602_enum[14]),
SOC_SINGLE("Playback 6dB Attenuate", VT1602_ADCDAC, 7, 1, 0),
SOC_SINGLE("Capture 6dB Attenuate", VT1602_ADCDAC, 8, 1, 0),

SOC_DOUBLE_R("Master Volume", VT1602_LOUT1V, VT1602_ROUT1V, 0, 127, 0),
//SOC_DOUBLE_R("PCM Volume", VT1602_LOUT1V, VT1602_ROUT1V, 0, 127, 0),

/*
SOC_ENUM("Bass Boost", vt1602_enum[0]),
SOC_ENUM("Bass Filter", vt1602_enum[1]),
SOC_SINGLE("Bass Volume", VT1602_BASS, 0, 15, 1),

SOC_SINGLE("Treble Volume", VT1602_TREBLE, 0, 15, 0),
SOC_ENUM("Treble Cut-off", vt1602_enum[2]),

SOC_SINGLE("3D Switch", VT1602_3D, 0, 1, 0),
SOC_SINGLE("3D Volume", VT1602_3D, 1, 15, 0),
SOC_ENUM("3D Lower Cut-off", vt1602_enum[3]),
SOC_ENUM("3D Upper Cut-off", vt1602_enum[4]),
SOC_ENUM("3D Mode", vt1602_enum[5]),

SOC_SINGLE("ALC Capture Target Volume", VT1602_ALC1, 0, 7, 0),
SOC_SINGLE("ALC Capture Max Volume", VT1602_ALC1, 4, 7, 0),
SOC_ENUM("ALC Capture Function", vt1602_enum[6]),
SOC_SINGLE("ALC Capture ZC Switch", VT1602_ALC2, 7, 1, 0),
SOC_SINGLE("ALC Capture Hold Time", VT1602_ALC2, 0, 15, 0),
SOC_SINGLE("ALC Capture Decay Time", VT1602_ALC3, 4, 15, 0),
SOC_SINGLE("ALC Capture Attack Time", VT1602_ALC3, 0, 15, 0),
SOC_SINGLE("ALC Capture NG Threshold", VT1602_NGATE, 3, 31, 0),
SOC_ENUM("ALC Capture NG Type", vt1602_enum[4]),
SOC_SINGLE("ALC Capture NG Switch", VT1602_NGATE, 0, 1, 0),
*/

SOC_SINGLE("Left ADC Capture Volume", VT1602_LADC, 0, 255, 0),
SOC_SINGLE("Right ADC Capture Volume", VT1602_RADC, 0, 255, 0),

SOC_SINGLE("ZC Timeout Switch", VT1602_ADCTL1, 0, 1, 0),
SOC_SINGLE("Playback Invert Switch", VT1602_ADCTL1, 1, 1, 0),

SOC_SINGLE("Right Speaker Playback Invert Switch", VT1602_ADCTL2, 4, 1, 0),

/* Unimplemented */
/* ADCDAC Bit 0 - ADCHPD */
/* ADCDAC Bit 4 - HPOR */
/* ADCTL1 Bit 2,3 - DATSEL */
/* ADCTL1 Bit 4,5 - DMONOMIX */
/* ADCTL1 Bit 6,7 - VSEL */
/* ADCTL2 Bit 2 - LRCM */
/* ADCTL2 Bit 3 - TRI */
/* ADCTL3 Bit 5 - HPFLREN */
/* ADCTL3 Bit 6 - VROI */
/* ADCTL3 Bit 7,8 - ADCLRM */
/* ADCIN Bit 4 - LDCM */
/* ADCIN Bit 5 - RDCM */

SOC_DOUBLE_R("Mic Boost", VT1602_LADCIN, VT1602_RADCIN, 4, 3, 0),

SOC_DOUBLE_R("Bypass Left Playback Volume", VT1602_LOUTM1,
	VT1602_LOUTM2, 4, 7, 1),
SOC_DOUBLE_R("Bypass Right Playback Volume", VT1602_ROUTM1,
	VT1602_ROUTM2, 4, 7, 1),
SOC_DOUBLE_R("Bypass Mono Playback Volume", VT1602_MOUTM1,
	VT1602_MOUTM2, 4, 7, 1),

SOC_SINGLE("Mono Playback ZC Switch", VT1602_MOUTV, 7, 1, 0),

SOC_DOUBLE_R("Headphone Playback Volume", VT1602_LOUT1V, VT1602_ROUT1V,
	0, 127, 0),
SOC_DOUBLE_R("Speaker Playback Volume", VT1602_LOUT2V, VT1602_ROUT2V,
	0, 127, 0),

SOC_SINGLE("Mono Playback Volume", VT1602_MOUTV, 0, 127, 0),

};

/*
 * DAPM Controls
 */

/* Left Mixer */
static const struct snd_kcontrol_new vt1602_left_mixer_controls[] = {
SOC_DAPM_SINGLE("Playback Switch", VT1602_LOUTM1, 8, 1, 0),
SOC_DAPM_SINGLE("Left Bypass Switch", VT1602_LOUTM1, 7, 1, 0),
SOC_DAPM_SINGLE("Right Playback Switch", VT1602_LOUTM2, 8, 1, 0),
SOC_DAPM_SINGLE("Right Bypass Switch", VT1602_LOUTM2, 7, 1, 0),
};

/* Right Mixer */
static const struct snd_kcontrol_new vt1602_right_mixer_controls[] = {
SOC_DAPM_SINGLE("Left Playback Switch", VT1602_ROUTM1, 8, 1, 0),
SOC_DAPM_SINGLE("Left Bypass Switch", VT1602_ROUTM1, 7, 1, 0),
SOC_DAPM_SINGLE("Playback Switch", VT1602_ROUTM2, 8, 1, 0),
SOC_DAPM_SINGLE("Right Bypass Switch", VT1602_ROUTM2, 7, 1, 0),
};

/* Mono Mixer */
static const struct snd_kcontrol_new vt1602_mono_mixer_controls[] = {
SOC_DAPM_SINGLE("Left Playback Switch", VT1602_MOUTM1, 8, 1, 0),
SOC_DAPM_SINGLE("Left Bypass Switch", VT1602_MOUTM1, 7, 1, 0),
SOC_DAPM_SINGLE("Right Playback Switch", VT1602_MOUTM2, 8, 1, 0),
SOC_DAPM_SINGLE("Right Bypass Switch", VT1602_MOUTM2, 7, 1, 0),
};

/* Left Line Mux */
static const struct snd_kcontrol_new vt1602_left_line_controls =
SOC_DAPM_ENUM("Route", vt1602_enum[8]);

/* Right Line Mux */
static const struct snd_kcontrol_new vt1602_right_line_controls =
SOC_DAPM_ENUM("Route", vt1602_enum[9]);

/* Left PGA Mux */
static const struct snd_kcontrol_new vt1602_left_pga_controls =
SOC_DAPM_ENUM("Route", vt1602_enum[10]);

/* Right PGA Mux */
static const struct snd_kcontrol_new vt1602_right_pga_controls =
SOC_DAPM_ENUM("Route", vt1602_enum[11]);

/* Out 3 Mux */
static const struct snd_kcontrol_new vt1602_out3_controls =
SOC_DAPM_ENUM("Route", vt1602_enum[12]);

/* Differential Mux */
static const struct snd_kcontrol_new vt1602_diffmux_controls =
SOC_DAPM_ENUM("Route", vt1602_enum[13]);

/* Mono ADC Mux */
static const struct snd_kcontrol_new vt1602_monomux_controls =
SOC_DAPM_ENUM("Route", vt1602_enum[16]);

static const struct snd_soc_dapm_widget vt1602_dapm_widgets[] = {
	SND_SOC_DAPM_MIXER("Left Mixer", SND_SOC_NOPM, 0, 0,
		&vt1602_left_mixer_controls[0],
		ARRAY_SIZE(vt1602_left_mixer_controls)),
	SND_SOC_DAPM_MIXER("Right Mixer", SND_SOC_NOPM, 0, 0,
		&vt1602_right_mixer_controls[0],
		ARRAY_SIZE(vt1602_right_mixer_controls)),
	/*
	SND_SOC_DAPM_MIXER("Mono Mixer", VT1602_PWR2, 2, 0,
		&vt1602_mono_mixer_controls[0],
		ARRAY_SIZE(vt1602_mono_mixer_controls)),

	SND_SOC_DAPM_PGA("Right Out 2", VT1602_PWR2, 3, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Left Out 2", VT1602_PWR2, 4, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Right Out 1", VT1602_PWR2, 5, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Left Out 1", VT1602_PWR2, 6, 0, NULL, 0),
	SND_SOC_DAPM_DAC("Right DAC", "Right Playback", VT1602_PWR2, 7, 0),
	SND_SOC_DAPM_DAC("Left DAC", "Left Playback", VT1602_PWR2, 8, 0),
	*/
	/*
	SND_SOC_DAPM_MICBIAS("Mic Bias", VT1602_PWR1, 1, 0),
	SND_SOC_DAPM_ADC("Right ADC", "Right Capture", VT1602_PWR1, 2, 0),
	SND_SOC_DAPM_ADC("Left ADC", "Left Capture", VT1602_PWR1, 3, 0),

	SND_SOC_DAPM_MUX("Left PGA Mux", VT1602_PWR1, 5, 0,
		&vt1602_left_pga_controls),
	SND_SOC_DAPM_MUX("Right PGA Mux", VT1602_PWR1, 4, 0,
		&vt1602_right_pga_controls),
	*/
	SND_SOC_DAPM_MUX("Left Line Mux", SND_SOC_NOPM, 0, 0,
		&vt1602_left_line_controls),
	SND_SOC_DAPM_MUX("Right Line Mux", SND_SOC_NOPM, 0, 0,
		&vt1602_right_line_controls),

	SND_SOC_DAPM_MUX("Out3 Mux", SND_SOC_NOPM, 0, 0, &vt1602_out3_controls),
	/*
	SND_SOC_DAPM_PGA("Out 3", VT1602_PWR2, 1, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Mono Out 1", VT1602_PWR2, 2, 0, NULL, 0),
	*/

	SND_SOC_DAPM_MUX("Differential Mux", SND_SOC_NOPM, 0, 0,
		&vt1602_diffmux_controls),
	SND_SOC_DAPM_MUX("Left ADC Mux", SND_SOC_NOPM, 0, 0,
		&vt1602_monomux_controls),
	SND_SOC_DAPM_MUX("Right ADC Mux", SND_SOC_NOPM, 0, 0,
		&vt1602_monomux_controls),

	SND_SOC_DAPM_OUTPUT("LOUT1"),
	SND_SOC_DAPM_OUTPUT("ROUT1"),
	SND_SOC_DAPM_OUTPUT("LOUT2"),
	SND_SOC_DAPM_OUTPUT("ROUT2"),
	SND_SOC_DAPM_OUTPUT("MONO1"),
	SND_SOC_DAPM_OUTPUT("OUT3"),
	SND_SOC_DAPM_OUTPUT("VREF"),

	SND_SOC_DAPM_INPUT("LINPUT1"),
	SND_SOC_DAPM_INPUT("LINPUT2"),
	SND_SOC_DAPM_INPUT("LINPUT3"),
	SND_SOC_DAPM_INPUT("RINPUT1"),
	SND_SOC_DAPM_INPUT("RINPUT2"),
	SND_SOC_DAPM_INPUT("RINPUT3"),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/* left mixer */
	{"Left Mixer", "Playback Switch", "Left DAC"},
	{"Left Mixer", "Left Bypass Switch", "Left Line Mux"},
	{"Left Mixer", "Right Playback Switch", "Right DAC"},
	{"Left Mixer", "Right Bypass Switch", "Right Line Mux"},

	/* right mixer */
	{"Right Mixer", "Left Playback Switch", "Left DAC"},
	{"Right Mixer", "Left Bypass Switch", "Left Line Mux"},
	{"Right Mixer", "Playback Switch", "Right DAC"},
	{"Right Mixer", "Right Bypass Switch", "Right Line Mux"},

	/* left out 1 */
	{"Left Out 1", NULL, "Left Mixer"},
	{"LOUT1", NULL, "Left Out 1"},

	/* left out 2 */
	{"Left Out 2", NULL, "Left Mixer"},
	{"LOUT2", NULL, "Left Out 2"},

	/* right out 1 */
	{"Right Out 1", NULL, "Right Mixer"},
	{"ROUT1", NULL, "Right Out 1"},

	/* right out 2 */
	{"Right Out 2", NULL, "Right Mixer"},
	{"ROUT2", NULL, "Right Out 2"},

	/* mono mixer */
	{"Mono Mixer", "Left Playback Switch", "Left DAC"},
	{"Mono Mixer", "Left Bypass Switch", "Left Line Mux"},
	{"Mono Mixer", "Right Playback Switch", "Right DAC"},
	{"Mono Mixer", "Right Bypass Switch", "Right Line Mux"},

	/* mono out */
	{"Mono Out 1", NULL, "Mono Mixer"},
	{"MONO1", NULL, "Mono Out 1"},

	/* out 3 */
	{"Out3 Mux", "VREF", "VREF"},
	{"Out3 Mux", "ROUT1 + Vol", "ROUT1"},
	{"Out3 Mux", "ROUT1", "Right Mixer"},
	{"Out3 Mux", "MonoOut", "MONO1"},
	{"Out 3", NULL, "Out3 Mux"},
	{"OUT3", NULL, "Out 3"},

	/* Left Line Mux */
	{"Left Line Mux", "Line 1", "LINPUT1"},
	{"Left Line Mux", "Line 2", "LINPUT2"},
	{"Left Line Mux", "Line 3", "LINPUT3"},
	{"Left Line Mux", "PGA", "Left PGA Mux"},
	{"Left Line Mux", "Differential", "Differential Mux"},

	/* Right Line Mux */
	{"Right Line Mux", "Line 1", "RINPUT1"},
	{"Right Line Mux", "Line 2", "RINPUT2"},
	{"Right Line Mux", "Line 3", "RINPUT3"},
	{"Right Line Mux", "PGA", "Right PGA Mux"},
	{"Right Line Mux", "Differential", "Differential Mux"},

	/* Left PGA Mux */
	{"Left PGA Mux", "Line 1", "LINPUT1"},
	{"Left PGA Mux", "Line 2", "LINPUT2"},
	{"Left PGA Mux", "Line 3", "LINPUT3"},
	{"Left PGA Mux", "Differential", "Differential Mux"},

	/* Right PGA Mux */
	{"Right PGA Mux", "Line 1", "RINPUT1"},
	{"Right PGA Mux", "Line 2", "RINPUT2"},
	{"Right PGA Mux", "Line 3", "RINPUT3"},
	{"Right PGA Mux", "Differential", "Differential Mux"},

	/* Differential Mux */
	{"Differential Mux", "Line 1", "LINPUT1"},
	{"Differential Mux", "Line 1", "RINPUT1"},
	{"Differential Mux", "Line 2", "LINPUT2"},
	{"Differential Mux", "Line 2", "RINPUT2"},

	/* Left ADC Mux */
	{"Left ADC Mux", "Stereo", "Left PGA Mux"},
	{"Left ADC Mux", "Mono (Left)", "Left PGA Mux"},
	{"Left ADC Mux", "Digital Mono", "Left PGA Mux"},

	/* Right ADC Mux */
	{"Right ADC Mux", "Stereo", "Right PGA Mux"},
	{"Right ADC Mux", "Mono (Right)", "Right PGA Mux"},
	{"Right ADC Mux", "Digital Mono", "Right PGA Mux"},

	/* ADC */
	{"Left ADC", NULL, "Left ADC Mux"},
	{"Right ADC", NULL, "Right ADC Mux"},
};

struct _coeff_div {
	u32 mclk;
	u32 rate;
	u16 fs;
	u8 sr:5;
	u8 usb:1;
};

/* codec hifi mclk clock divider coefficients */
static const struct _coeff_div coeff_div[] = {
	/* 8k */
	{12288000, 8000, 1536, 0x6, 0x0},

	/* 11.025k */
	{11289600, 11025, 1024, 0x18, 0x0},

	/* 16k */
	{12288000, 16000, 768, 0xa, 0x0},

	/* 22.05k */
	{11289600, 22050, 512, 0x1a, 0x0},

	/* 32k */
	{12288000, 32000, 384, 0xc, 0x0},

	/* 44.1k */
	{11289600, 44100, 256, 0x10, 0x0},

	/* 48k */
	{12288000, 48000, 256, 0x0, 0x0},

	/* 88.2k */
	{11289600, 88200, 128, 0x1e, 0x0},

	/* 96k */
	{12288000, 96000, 128, 0xe, 0x0},
};

static inline int get_coeff(int mclk, int rate)
{
	int i;

	DBG_DETAIL("mclk=%d, rate=%d", mclk, rate);
	
	for (i = 0; i < ARRAY_SIZE(coeff_div); i++) {
		if (coeff_div[i].rate == rate && coeff_div[i].mclk == mclk)
			return i;
	}

	printk(KERN_ERR "vt1602: could not get coeff for mclk %d @ rate %d\n",
		mclk, rate);
	return -EINVAL;
}

static int vt1602_pcm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	u16 iface = snd_soc_read(codec, VT1602_IFACE) & 0x1f3;
	//int coeff = get_coeff(vt1602->sysclk, params_rate(params));

	DBG_DETAIL();
	
	/* bit size */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		iface |= 0x0004;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		iface |= 0x0008;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		iface |= 0x000c;
		break;
	}

	/* set iface & srate */
	snd_soc_write(codec, VT1602_IFACE, iface);
	
	return 0;
}

static int vt1602_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 mute_reg = snd_soc_read(codec, VT1602_ADCDAC) & 0xfff7;

	DBG_DETAIL();
	
	if (mute)
		snd_soc_write(codec, VT1602_ADCDAC, mute_reg | 0x8);
	else
		snd_soc_write(codec, VT1602_ADCDAC, mute_reg);
	return 0;
}

static int vt1602_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct vt1602_priv *vt1602 = snd_soc_codec_get_drvdata(codec);

	DBG_DETAIL();
	
	switch (freq) {
	case 11289600:
	case 12000000:
	case 12288000:
	case 16934400:
	case 18432000:
		vt1602->sysclk = freq;
		return 0;
	}
	return -EINVAL;
}

static int vt1602_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 iface = 0;

	DBG_DETAIL();
	
	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		iface = 0x0040;
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

	snd_soc_write(codec, VT1602_IFACE, iface);
	return 0;
}

static int vt1602_set_bias_level(struct snd_soc_codec *codec, enum snd_soc_bias_level level )
{
	DBG_DETAIL();
	
	switch (level) {
	case SND_SOC_BIAS_ON:
		/* set vmid to 50k and unmute dac */
		break;
	case SND_SOC_BIAS_PREPARE:
		/* set vmid to 5k for quick power up */
		break;
	case SND_SOC_BIAS_STANDBY:
		/* mute dac and set vmid to 500k, enable VREF */
		break;
	case SND_SOC_BIAS_OFF:
		break;
	}
	codec->dapm.bias_level = level;
	return 0;
}

static void vt1602_work(struct work_struct *work)
{
	struct snd_soc_codec *codec =
		container_of(work, struct snd_soc_codec, dapm.delayed_work.work);

	DBG_DETAIL();
	
	vt1602_set_bias_level(codec, codec->dapm.bias_level);
}

#define VT1602_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
		SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_32000 | \
		SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 | \
		SNDRV_PCM_RATE_96000)

#define VT1602_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
	SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_FLOAT)

static struct snd_soc_dai_ops vt1602_dai_ops = {
	.hw_params	= vt1602_pcm_hw_params,
	.digital_mute	= vt1602_mute,
	.set_sysclk	= vt1602_set_dai_sysclk,
	.set_fmt	= vt1602_set_dai_fmt,
};

struct snd_soc_dai_driver vt1602_dai = {
	.name = "VT1602",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = VT1602_RATES,
		.formats = VT1602_FORMATS,},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = VT1602_RATES,
		.formats = VT1602_FORMATS,},
	.ops = &vt1602_dai_ops,
};

static int vt1602_suspend(struct snd_soc_codec *codec, pm_message_t state)
{
	DBG_DETAIL();
	
	vt1602_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int vt1602_resume(struct snd_soc_codec *codec)
{
	DBG_DETAIL();
	
	snd_soc_cache_sync(codec);
	vt1602_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	/* charge vt1602 caps */
	if (codec->dapm.suspend_bias_level == SND_SOC_BIAS_ON) {
		vt1602_set_bias_level(codec, SND_SOC_BIAS_PREPARE);
		codec->dapm.bias_level = SND_SOC_BIAS_ON;
		schedule_delayed_work(&codec->dapm.delayed_work,
					msecs_to_jiffies(1000));
	}

	return 0;
}

static int wmt_vt1602_init(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	int ret, reg;

	DBG_DETAIL();

	/* charge output caps */
	vt1602_set_bias_level(codec, SND_SOC_BIAS_PREPARE);
	codec->dapm.bias_level = SND_SOC_BIAS_STANDBY;
	schedule_delayed_work(&codec->dapm.delayed_work, msecs_to_jiffies(1000));

	/* set the update bits */
	reg = snd_soc_read(codec, VT1602_LDAC);
	snd_soc_write(codec, VT1602_LDAC, reg | 0x0100);
	reg = snd_soc_read(codec, VT1602_RDAC);
	snd_soc_write(codec, VT1602_RDAC, reg | 0x0100);
	reg = snd_soc_read(codec, VT1602_LOUT1V);
	snd_soc_write(codec, VT1602_LOUT1V, reg | 0x0100);
	reg = snd_soc_read(codec, VT1602_ROUT1V);
	snd_soc_write(codec, VT1602_ROUT1V, reg | 0x0100);
	reg = snd_soc_read(codec, VT1602_LOUT2V);
	snd_soc_write(codec, VT1602_LOUT2V, reg | 0x0100);
	reg = snd_soc_read(codec, VT1602_ROUT2V);
	snd_soc_write(codec, VT1602_ROUT2V, reg | 0x0100);
	reg = snd_soc_read(codec, VT1602_LINVOL);
	snd_soc_write(codec, VT1602_LINVOL, reg | 0x0100);
	reg = snd_soc_read(codec, VT1602_RINVOL);
	snd_soc_write(codec, VT1602_RINVOL, reg | 0x0100);
	reg = snd_soc_read(codec, VT1602_LOUTM1);
	snd_soc_write(codec, VT1602_LOUTM1, reg);
	reg = snd_soc_read(codec, VT1602_LOUTM2);
	snd_soc_write(codec, VT1602_LOUTM2, reg);
	reg = snd_soc_read(codec, VT1602_ROUTM1);
	snd_soc_write(codec, VT1602_ROUTM1, reg);
	reg = snd_soc_read(codec, VT1602_ROUTM2);
	snd_soc_write(codec, VT1602_ROUTM2, reg);
	snd_soc_write(codec, VT1602_PWR1,  0x1FE);
	snd_soc_write(codec, VT1602_PWR2, 0x1E0);
	snd_soc_write(codec, VT1602_LADCIN,  0x020);
	snd_soc_write(codec, VT1602_RADCIN, 0x020);
	snd_soc_write(codec, VT1602_ADCTL3, 0x040);

	ret = snd_soc_add_controls(codec, vt1602_snd_controls,
			ARRAY_SIZE(vt1602_snd_controls));
	if (ret) {
		info("snd_soc_add_controls fail");
		return ret;
	}

	ret = snd_soc_dapm_new_controls(dapm, vt1602_dapm_widgets,
			ARRAY_SIZE(vt1602_dapm_widgets));
	if (ret) {
		info("snd_soc_dapm_new_controls fail");
		return ret;
	}

	ret = snd_soc_dapm_add_routes(dapm, audio_map,
			ARRAY_SIZE(audio_map));
	if (ret) {
		info("snd_soc_dapm_add_routes fail");
		return 0;
	}

	return ret;
}

static int vt1602_probe(struct snd_soc_codec *codec)
{
	struct vt1602_priv *vt1602 = snd_soc_codec_get_drvdata(codec);
	int ret;
	
	pr_info("VT1602 Audio Codec %s\n", VT1602_VERSION);

	ret = snd_soc_codec_set_cache_io(codec, 7, 9, vt1602->control_type);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	ret = vt1602_reset(codec);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to issue reset: %d\n", ret);
		return ret;
	}

	INIT_DELAYED_WORK(&codec->dapm.delayed_work, vt1602_work);

	ret = wmt_vt1602_init(codec);
	if (ret)
		info("vt1602_probe fail");
	return ret;
}

/*
 * This function forces any delayed work to be queued and run.
 */
static int run_delayed_work(struct delayed_work *dwork)
{
	int ret;

	DBG_DETAIL();
	
	/* cancel any work waiting to be queued. */
	ret = cancel_delayed_work(dwork);

	/* if there was any work waiting then we run it now and
	 * wait for it's completion */
	if (ret) {
		schedule_delayed_work(dwork, 0);
		flush_scheduled_work();
	}
	return ret;
}

/* remove everything here */
static int vt1602_remove(struct snd_soc_codec *codec)
{
	DBG_DETAIL();
	
	vt1602_set_bias_level(codec, SND_SOC_BIAS_OFF);
	run_delayed_work(&codec->dapm.delayed_work);

	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_vt1602 = {
	.probe = 	vt1602_probe,
	.remove = 	vt1602_remove,
	.suspend = 	vt1602_suspend,
	.resume =	vt1602_resume,
	.set_bias_level = vt1602_set_bias_level,
	.reg_cache_size = ARRAY_SIZE(vt1602_reg),
	.reg_word_size = sizeof(u16),
	.reg_cache_default = vt1602_reg,

	/*.controls = vt1602_snd_controls,
	.num_controls = ARRAY_SIZE(vt1602_snd_controls),
	.dapm_widgets = vt1602_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(vt1602_dapm_widgets),
	.dapm_routes = audio_map,
	.num_dapm_routes = ARRAY_SIZE(audio_map),*/
};

#if defined (CONFIG_I2C) || defined (CONFIG_I2C_MODULE)
static int __devinit vt1602_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct vt1602_priv *vt1602;
	int ret;

	DBG_DETAIL();

	vt1602 = kzalloc(sizeof(struct vt1602_priv), GFP_KERNEL);
	if (vt1602 == NULL) {
		info("vt1602_i2c_probe kzalloc fail");
		return -ENOMEM;
	}
	
	i2c_set_clientdata(i2c, vt1602);
	vt1602->control_type = SND_SOC_I2C;

	ret = snd_soc_register_codec(&i2c->dev,
			&soc_codec_dev_vt1602, &vt1602_dai, 1);
	
	if (ret < 0) {
		kfree(vt1602);
		info("snd_soc_register_codec fail");
	}

	return ret;
}

static int __devexit vt1602_i2c_remove(struct i2c_client *client)
{
	DBG_DETAIL();
	
	snd_soc_unregister_codec(&client->dev);
	kfree(i2c_get_clientdata(client));
	return 0;
}

static const struct i2c_device_id vt1602_i2c_id[] = {
	{ "vt1602", 1},
	{ }
};
MODULE_DEVICE_TABLE(i2c, vt1602_i2c_id);

/* corgi i2c codec control layer */
static struct i2c_driver vt1602_i2c_driver = {
	.driver = {
		.name = "vt1602",
		.owner = THIS_MODULE,
	},
	.probe =    vt1602_i2c_probe,
	.remove =   __devexit_p(vt1602_i2c_remove),
	.id_table = vt1602_i2c_id,
};
#endif

static struct i2c_board_info __initdata wmt_vt1602_board_info[] = {
	{
			I2C_BOARD_INFO("vt1602", 0x1a),
	},
};		

static int __init vt1602_modinit(void)
{
	char buf[80];
	int varlen = 80;
	int ret = 0;
	char codec_name[6];
	struct i2c_adapter *adapter = NULL;

	DBG_DETAIL();
	
	ret = wmt_getsyspara("wmt.audio.i2s", buf, &varlen);
	
	if (ret == 0) {
		sscanf(buf, "%6s", codec_name);
		
		if (strcmp(codec_name, "vt1602")) {
			info("vt1602 string not found");
			return -EINVAL;
		}
	}

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	/*i2c_register_board_info(1, wmt_vt1602_board_info,
				ARRAY_SIZE(wmt_vt1602_board_info));*/

	adapter = i2c_get_adapter(1);
	if (adapter == NULL) {
		err("can not get i2c adapter, client address error");
		return -ENODEV;
	}
	
	if (i2c_new_device(adapter, wmt_vt1602_board_info) == NULL) {
		err("allocate i2c client failed");
		return -ENOMEM;
	}
	
	i2c_put_adapter(adapter);

	ret = i2c_add_driver(&vt1602_i2c_driver);
	if (ret) {
		info("i2c_add_driver fail");
	}	
#endif
	
	return ret;
}
module_init(vt1602_modinit);

static void __exit vt1602_exit(void)
{
	DBG_DETAIL();
	
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	i2c_del_driver(&vt1602_i2c_driver);
#endif
}
module_exit(vt1602_exit);

MODULE_DESCRIPTION("WMT [ALSA SoC] driver");
MODULE_AUTHOR("WonderMedia Technologies, Inc.");
MODULE_LICENSE("GPL");
