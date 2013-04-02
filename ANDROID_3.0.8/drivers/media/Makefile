#
# Makefile for the kernel multimedia device drivers.
#

media-objs	:= media-device.o media-devnode.o media-entity.o

ifeq ($(CONFIG_MEDIA_CONTROLLER),y)
  obj-$(CONFIG_MEDIA_SUPPORT) += media.o
endif

obj-y += common/ rc/ video/
obj-$(CONFIG_VIDEO_DEV) += video/
obj-$(CONFIG_VIDEO_DEV) += radio/
obj-$(CONFIG_DVB_CORE)  += dvb/
obj-$(CONFIG_WMT_VIDEO_DECODE) += video/wmt/
obj-$(CONFIG_WMT_VIDEO_ENCODE) += video/wmt/encode/
obj-$(CONFIG_WMT_V4L2) += video/wmt_v4l2/
