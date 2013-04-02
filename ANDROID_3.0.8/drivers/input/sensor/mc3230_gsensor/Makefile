KERNELDIR=../../../../
#KERNELDIR=/home/hangyan/android8850/kernel/ANDROID_3.0.8
CROSS = arm_1103_le-
CC=		$(CROSS)gcc
LD=		$(CROSS)ld
STRIP = $(CROSS)strip

DEBUG = n 

# Add your debugging flag (or not) to EXTRA_CFLAGS
ifeq ($(DEBUG),y)
#  DEBFLAGS = -O -g -DSCULL_DEBUG # "-O" is needed to expand inlines
DEBFLAGS = -O0 -g -DSCULL_DEBUG # "-O" is needed to expand inlines

else
  DEBFLAGS = -O2 -Wall
endif

EXTRA_CFLAGS += $(DEBFLAGS)


MY_MODULE_NAME=s_wmt_gsensor_mc3230

obj-m	:= $(MY_MODULE_NAME).o
$(MY_MODULE_NAME)-objs  := mc32x0.o mc32x0_driver.o

default:
	$(MAKE) -C $(KERNELDIR) SUBDIRS=$(PWD) modules
	$(STRIP) --strip-debug $(MY_MODULE_NAME).ko
	rm -rf *.o *~ core .depend .*.cmd *.mod.c .tmp_versions

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.order *.symvers

