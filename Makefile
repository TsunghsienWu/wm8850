
all: ubin

menuconfig:
	make -C ANDROID_3.0.8 menuconfig
	
ubin:
	make -C ANDROID_3.0.8 ubin -j12

Android_defconfig:
	make -C ANDROID_3.0.8 Android_defconfig
	
modules:
	make -C ANDROID_3.0.8 modules
	#mkdir modules
	#find ANDROID_3.0.8 -name "*.ko" |  xargs -i cp  {} `pwd`/modules/
	
modules_install:
	make -C ANDROID_3.0.8 modules_install
	
clean:	
	make -C ANDROID_3.0.8 clean
	#rm -rf modules
	cp ANDROID_3.0.8_Driver_Obj/* ANDROID_3.0.8/. -arf
	find ANDROID_3.0.8 -name "built-in.o" -exec rm -rf {} \;
	find ANDROID_3.0.8 -name ".*.o.cmd" -exec rm -rf {} \;


