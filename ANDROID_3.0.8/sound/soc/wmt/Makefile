#
## Makefile for ALSA WMT
#
#
# WMT SOC Support
snd-soc-wmt-i2s-objs := wmt-i2s.o wmt_swmixer.o
snd-soc-wmt-objs := wmt-soc.o wmt-pcm.o wmt_wm8994.o

snd-soc-wmt-pdm-objs := wmt-pdm-if.o wmt-pdm-pcm.o

obj-$(CONFIG_SND_WMT_SOC_I2S) += snd-soc-wmt-i2s.o
obj-$(CONFIG_SND_WMT_SOC) += snd-soc-wmt.o

obj-$(CONFIG_SND_WMT_SOC_PDM) += snd-soc-wmt-pdm.o
