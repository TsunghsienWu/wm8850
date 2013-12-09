# WM8850 Kernel Makefile
#
# build:
#   make clean
#   make Android_defconfig
#   make
#   make modules
#   make modules_install

CROSS_COMPILE ?= arm-none-linux-gnueabi-
BUILD_FLAGS ?= LOCALVERSION= CROSS_COMPILE=$(CROSS_COMPILE)

all: ubin

menuconfig:
	make -C ANDROID_3.0.8 $(BUILD_FLAGS) menuconfig
	
ubin:
	make -C ANDROID_3.0.8 $(BUILD_FLAGS) ubin -j8

Android_defconfig:
	make -C ANDROID_3.0.8 $(BUILD_FLAGS) Android_defconfig
	
modules:
	make -C ANDROID_3.0.8 $(BUILD_FLAGS) modules
	#mkdir modules
	#find ANDROID_3.0.8 -name "*.ko" |  xargs -i cp  {} `pwd`/modules/
	
modules_install:
	if [ -e $(PWD)/out ]; then rm -rf $(PWD)/out; fi
	mkdir $(PWD)/out
	make -C ANDROID_3.0.8 $(BUILD_FLAGS) \
		INSTALL_MOD_PATH=$(PWD)/out modules_install
	find $(PWD)/out -type f
	
clean:	
	make -C ANDROID_3.0.8 $(BUILD_FLAGS) clean
	#rm -rf modules
	cp ANDROID_3.0.8_Driver_Obj/* ANDROID_3.0.8/. -arf
	find ANDROID_3.0.8 -name "built-in.o" -exec rm -rf {} \;
	find ANDROID_3.0.8 -name ".*.o.cmd" -exec rm -rf {} \;


