KERNELDIR=/home/hangyan/android8850/kernel/ANDROID_3.0.8
#KERNELDIR=../../../../
CROSS = $(CROSS_COMPILE)
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


MY_MODULE_NAME=s_wmt_bq27x00batt

obj-m	:= $(MY_MODULE_NAME).o
$(MY_MODULE_NAME)-objs  := batt_client.o bq27x00_battery.o bq27x00_wmt_batt.o

default:
	$(MAKE) -C $(KERNELDIR) SUBDIRS=$(PWD) modules
	$(STRIP) --strip-debug $(MY_MODULE_NAME).ko
	rm -rf *.o *~ core .depend .*.cmd *.mod.c .tmp_versions *.order *.symvers

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.order *.symvers
