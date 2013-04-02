#KERNELDIR=/home/hangyan/android8850/kernel/ANDROID_3.0.8/
KERNELDIR=../../../../
PWD=`pwd`
MODULE_NAME=kpad

CROSS = arm_1103_le-
CC=	$(CROSS)gcc
LD=	$(CROSS)ld
STRIP=  $(CROSS)strip

DEBUG = n 

# Add your debugging flag (or not) to EXTRA_CFLAGS
ifeq ($(DEBUG),y)
#  DEBFLAGS = -O -g -DSCULL_DEBUG # "-O" is needed to expand inlines
DEBFLAGS = -O0 -g -DSCULL_DEBUG # "-O" is needed to expand inlines

else
  DEBFLAGS = -O2 -Wall
endif

EXTRA_CFLAGS += $(DEBFLAGS)

obj-m	:= $(MODULE_NAME).o
$(MODULE_NAME)-objs  := kpad_colrow.o wmt_keypadall.o

default:
	$(MAKE) -C $(KERNELDIR) SUBDIRS=$(PWD) modules
	$(STRIP) --strip-debug $(MODULE_NAME).ko
	@rm -rf *.o *~ core .depend .*.cmd *.mod.c .tmp_versions

clean:
	@rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c *.symvers *.order .tmp_versions
