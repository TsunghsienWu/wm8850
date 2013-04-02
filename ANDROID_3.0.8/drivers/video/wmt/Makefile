#
# Makefile for the Wonder Media framebuffer driver
#

# wmt external video device
obj-$(CONFIG_FB_WMT) += devices/
obj-$(CONFIG_WMT_EDID) += parse-edid.o sw_i2c.o

# wmt vout for platform
obj-$(CONFIG_FB_WMT) += vout-wmt.o

# wmt vpp
obj-$(CONFIG_FB_WMT) += vout.o vpp.o govrh.o govw.o govm.o scl.o vppm.o
obj-$(CONFIG_FB_WMT) += vpu.o lvds.o
obj-$(CONFIG_WMT_HDMI) += hdmi.o hdmi_cp.o
obj-$(CONFIG_FB_WMT) += dev-vpp.o vpp-osif.o
obj-$(CONFIG_WMT_CEC) += cec.o dev-cec.o

obj-$(CONFIG_FB_WMT) += mali.o
obj-$(CONFIG_FB_WMT_GE) += ge/

# wmt vpu fb
obj-$(CONFIG_FB_WMT) += fb-vpu.o

# wmt vpu fb
obj-$(CONFIG_WMT_MB) += memblock.o

obj-$(CONFIG_LOGO_WMT_ANIMATION) += bootanimation/
