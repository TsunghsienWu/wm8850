#
# Makefile for the linux kernel.
#

# Common support
obj-y   := generic.o irq.o board.o wmt_clk.o perf.o dma.o  wmt_misc.o gpio-cs.o#time.o memblock.o
obj-m   := 
obj-n   :=
obj-    :=

# LEDs support
led-y   := #leds.o leds-wmt.o
obj-$(CONFIG_LEDS)				+= $(led-y)
obj-$(CONFIG_PM)                                += pm.o pm_cpai.o sleep.o

# CPUFreq support
obj-$(CONFIG_CPU_FREQ)			+= wmt_cpufreq.o
obj-$(CONFIG_CPU_IDLE)			+= wmt_cpuidle.o
