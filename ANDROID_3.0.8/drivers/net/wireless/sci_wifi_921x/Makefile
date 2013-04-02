TARGET = sci9211

OBJECT = rf.o MAC_Control.o sci921core.o

ifneq ($(KERNELRELEASE),)

#kbuild syntax.

obj-m += $(TARGET).o

$(TARGET)-objs := $(OBJECT)

else

PWD := $(shell pwd)

#KERNEL_DIR := /lib/modules/`uname -r`/build
#KERNEL_DIR := /home/thomas/linux/linux-2.6.36
#KERNEL_DIR := /home/hzzhou/sci/linux-2.6.35.13
KERNEL_DIR := /home/kevinguan/8850/ANDROID_3.0.8
#KERNEL_DIR := /home/zk/lichee/linux-3.0

#TOOL_CHAIN := arm-none-linux-gnueabi-
TOOL_CHAIN := arm_1103_le-


all : modules

modules:

	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules ARCH=arm CROSS_COMPILE=$(TOOL_CHAIN)

clean:
	rm -rf .tmp_versions *.ko $(OBJECT) *.mod.* *.o modules.* Module.* *.cmd .*.cmd
 
endif
