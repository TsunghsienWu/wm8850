/*++
linux/arch/arm/mach-wmt/wmt_cpuidle.c

Copyright (c) 2008  WonderMedia Technologies, Inc.

This program is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software Foundation,
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License along with
this program.  If not, see <http://www.gnu.org/licenses/>.

WonderMedia Technologies, Inc.
10F, 529, Chung-Cheng Road, Hsin-Tien, Taipei 231, R.O.C.
--*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/cpuidle.h>
#include <asm/proc-fns.h>
#include <mach/hardware.h>

//#define DEBUG
#ifdef  DEBUG
static int dbg_mask = 0;
module_param(dbg_mask, int, S_IRUGO | S_IWUSR);
#define id_dbg(fmt, args...) \
        do {\
            if (dbg_mask) \
                printk(KERN_ERR "[%s]_%d: " fmt, __func__ , __LINE__, ## args);\
        } while(0)
#define id_trace() \
        do {\
            if (dbg_mask) \
                printk(KERN_ERR "trace in %s %d\n", __func__, __LINE__);\
        } while(0)
#else
#define id_dbg(fmt, args...)
#define id_trace()
#endif

#define WMT_CPU_IDLE_MAX_STATES	    2
extern int wmt_setsyspara(char *varname, unsigned char *varval);
extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlenex);

static struct cpuidle_driver wmt_cpuidle_driver = {
	.name =         "wmt_cpuidle",
	.owner =        THIS_MODULE,
};

/* Actual code that puts the SoC in different idle states */
#define PMC_BASE PM_CTRL_BASE_ADDR
static int
wmt_enter_idle(struct cpuidle_device *dev, struct cpuidle_state *state)
{
	struct timeval before, after;
	int idle_time;

    //id_dbg("cpuidle state:%s\n", state->name);
	local_irq_disable();
	do_gettimeofday(&before);
    
    if (state == &dev->states[0]) {
        // do nothing here
        cpu_do_idle();
    }
    else if (state == &dev->states[1]) {
        unsigned char tmp = 0;
        // shutdown zac_clk
        tmp = *(volatile unsigned char *)(PMC_BASE + 0x08);
        //id_dbg("tmp is %d\n", tmp);
        *(volatile unsigned char *)(PMC_BASE + 0x08) = 0x01;
        tmp = *(volatile unsigned char *)(PMC_BASE + 0x08);
        //id_dbg("tmp is %d\n", tmp);
        cpu_do_idle();
    }

    do_gettimeofday(&after);
	local_irq_enable();
	idle_time = (after.tv_sec - before.tv_sec) * USEC_PER_SEC +
			                        (after.tv_usec - before.tv_usec);

    return idle_time;
}

static int __init wmt_cpuidle_check_env(void)
{
    int ret = 0;
    int varlen = 128;
    unsigned int drv_en = 0;
    unsigned char buf[128] = {0};

    /* uboot env name is: wmt.cpuidle.param/wmt.dvfs.param */
    ret = wmt_getsyspara("wmt.cpuidle.param", buf, &varlen);                                                                    
    if (ret) {
        printk(KERN_INFO "Can not find uboot env wmt.cpuidle.param\n");
        ret = -ENODATA;
        goto out;
    }
    id_dbg("wmt.cpuidle.param:%s\n", buf);

    sscanf(buf, "%d", &drv_en);
    if (!drv_en) {
        printk(KERN_INFO "wmt cpuidle driver disaled\n");
        ret = -ENODEV;
        goto out;
    }

out:
    return ret;
}


static DEFINE_PER_CPU(struct cpuidle_device, wmt_cpuidle_device);
static int __init wmt_cpuidle_driver_init(void)
{
    struct cpuidle_device *device = NULL;

    if (wmt_cpuidle_check_env()) {
        printk(KERN_WARNING "wmt_cpuidle check env failed!\n");
        return -EINVAL;
    }

	cpuidle_register_driver(&wmt_cpuidle_driver);

	device = &per_cpu(wmt_cpuidle_device, smp_processor_id());
	device->state_count = WMT_CPU_IDLE_MAX_STATES;

	/* Wait for interrupt state */
	device->states[0].enter = wmt_enter_idle;
	device->states[0].exit_latency = 1;
	device->states[0].target_residency = 10000;
	device->states[0].flags = CPUIDLE_FLAG_TIME_VALID;
	strcpy(device->states[0].name, "WFI");
	strcpy(device->states[0].desc, "Wait for interrupt");

	/* Wait for interrupt and DDR self refresh state */
	device->states[1].enter = wmt_enter_idle;
	device->states[1].exit_latency = 10;
	device->states[1].target_residency = 10000;
	device->states[1].flags = CPUIDLE_FLAG_TIME_VALID;
	strcpy(device->states[1].name, "ZAC_OFF");
	strcpy(device->states[1].desc, "WFI and disable ZAC clock");

	if (cpuidle_register_device(device)) {
		printk(KERN_ERR "wmt_cpuidle_driver_init: Failed registering\n");
		return -EIO;
	}

    printk(KERN_INFO "WMT cpuidle driver register\n");
    return 0;
}
module_init(wmt_cpuidle_driver_init);

MODULE_AUTHOR("WonderMedia Technologies, Inc");
MODULE_DESCRIPTION("WMT CPU idle driver");
MODULE_LICENSE("Dual BSD/GPL");
