/*
 * Copyright (c) 2008-2011 WonderMedia Technologies, Inc. All Rights Reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * WonderMedia Technologies, Inc.
 * 4F, 533, Chung-Cheng Road, Hsin-Tien, Taipei 231, R.O.C
 */

/* Written by Vincent Chen, WonderMedia Technologies, Inc., 2008-2011 */

#include <asm/cacheflush.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include "mali.h"
#include <mach/hardware.h>

/* Mali-400 Power Management Kernel Driver */

#define MALI_DEBUG 0
#define REG_MALI_BADDR (0xD8080000 + WMT_MMAP_OFFSET)
/*
#define MALI_ALWAYS_ON
#define MALI_PMU_CONTROL
*/

extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);
extern unsigned int *vpp_backup_reg(unsigned int addr, unsigned int size);
extern int vpp_restore_reg(unsigned int addr, unsigned int size,
	unsigned int *reg_ptr);

struct mali_param_s {
	int enabled;
	int buflen1; /* Dedicated phycical memory*/
	int buflen2; /* OS memory */
	int buflen3; /* UMP memory */
};

static spinlock_t mali_spinlock;

static struct mali_param_s mali_param;

static int get_mali_param(struct mali_param_s *param);
static int mali_suspend(u32 cores);
static int mali_resume(u32 cores);
static void mali_enable_clock(int enab1le);
static void mali_enable_power(int enable);
static void mali_set_memory_base(unsigned int val);
static void mali_set_memory_size(unsigned int val);
static void mali_set_mem_validation_base(unsigned int val);
static void mali_set_mem_validation_size(unsigned int val);
static void mali_get_memory_base(unsigned int *val);
static void mali_get_memory_size(unsigned int *val);
static void mali_get_mem_validation_base(unsigned int *val);
static void mali_get_mem_validation_size(unsigned int *val);

/* symbols for ARM's Mali.ko */
unsigned int mali_memory_base;
unsigned int mali_memory_size;
unsigned int mali_mem_validation_base;
unsigned int mali_mem_validation_size;
unsigned int mali_ump_secure_id;
unsigned int (*mali_get_ump_secure_id)(unsigned int addr, unsigned int size);
void         (*mali_put_ump_secure_id)(unsigned int ump_id);

EXPORT_SYMBOL(mali_memory_base);
EXPORT_SYMBOL(mali_memory_size);
EXPORT_SYMBOL(mali_mem_validation_base);
EXPORT_SYMBOL(mali_mem_validation_size);
EXPORT_SYMBOL(mali_ump_secure_id);
EXPORT_SYMBOL(mali_get_ump_secure_id);
EXPORT_SYMBOL(mali_put_ump_secure_id);

int mali_platform_init_impl(void *data);
int mali_platform_deinit_impl(void *data);
int mali_platform_powerdown_impl(u32 cores);
int mali_platform_powerup_impl(u32 cores);
void set_mali_parent_power_domain(struct platform_device* dev);

EXPORT_SYMBOL(mali_platform_init_impl);
EXPORT_SYMBOL(mali_platform_deinit_impl);
EXPORT_SYMBOL(mali_platform_powerdown_impl);
EXPORT_SYMBOL(mali_platform_powerup_impl);
EXPORT_SYMBOL(set_mali_parent_power_domain);

/* PMU */
#ifndef REG_SET32
#define REG_SET32(addr, val)    (*(volatile unsigned int *)(addr)) = val
#define REG_GET32(addr)         (*(volatile unsigned int *)(addr))
#define REG_VAL32(addr)         (*(volatile unsigned int *)(addr))
#endif

#define REG_MALI400_BASE        (0xd8080000 + WMT_MMAP_OFFSET)
#define REG_MALI400_GP          (REG_MALI400_BASE)
#define REG_MALI400_L2          (REG_MALI400_BASE + 0x1000)
#define REG_MALI400_PMU         (REG_MALI400_BASE + 0x2000)
#define REG_MALI400_MMU_GP      (REG_MALI400_BASE + 0x3000)
#define REG_MALI400_MMU_PP0     (REG_MALI400_BASE + 0x4000)
#define REG_MALI400_MMU_PP1     (REG_MALI400_BASE + 0x5000)
#define REG_MALI400_MMU_PP2     (REG_MALI400_BASE + 0x6000)
#define REG_MALI400_MMU_PP3     (REG_MALI400_BASE + 0x7000)
#define REG_MALI400_PP0         (REG_MALI400_BASE + 0x8000)
#define REG_MALI400_PP1         (REG_MALI400_BASE + 0xa000)
#define REG_MALI400_PP2         (REG_MALI400_BASE + 0xc000)
#define REG_MALI400_PP3         (REG_MALI400_BASE + 0xe000)

#define MMU_DTE_ADDR            0x00
#define MMU_STATUS              0x04
#define MMU_COMMAND             0x08
#define MMU_PAGE_FAULT_ADDR     0x0c
#define MMU_ZAP_ONE_LINE        0x10
#define MMU_INT_RAWSTAT         0x14
#define MMU_INT_CLEAR           0x18
#define MMU_INT_MASK            0x1c
#define MMU_INT_STATUS          0x20

extern unsigned long msleep_interruptible(unsigned int msecs);

int mali_pmu_power_up(unsigned int msk)
{
	volatile int timeout;
	unsigned int pmu0c;
	unsigned int pmu08;

	if ((REG_VAL32(REG_MALI400_PMU + 8) & msk) == 0)
		return 0;

	pmu08 = REG_VAL32(REG_MALI400_PMU + 8);
	pmu0c = REG_VAL32(REG_MALI400_PMU + 0xc);
	REG_SET32(REG_MALI400_PMU + 0xc, 0);

	asm volatile("" ::: "memory");

	/* PMU_POWER_UP */
	REG_SET32(REG_MALI400_PMU, msk);

	asm volatile("" ::: "memory");

	timeout = 10;

	do {
		if ((REG_VAL32(REG_MALI400_PMU + 8) & msk) == 0)
			break;
		msleep_interruptible(1);
		timeout--;
	} while (timeout > 0);

#if MALI_DEBUG
	if (timeout == 0)
		printk("mali_pmu_power_up(0x%x) = 0x%08x -> 0x%08x: fail\n",
			msk, pmu08, REG_VAL32(REG_MALI400_PMU + 8));
	else
		printk("mali_pmu_power_up(0x%x) = 0x%08x -> 0x%08x: pass\n",
			msk, pmu08, REG_VAL32(REG_MALI400_PMU + 8));
#endif

	REG_SET32(REG_MALI400_PMU + 0xc, pmu0c);

	msleep_interruptible(10);

	if (timeout == 0) {
		printk("mali pmu power up failure\n");
		return -1;
	}

	return 0;
}

int mali_pmu_power_down(unsigned int msk)
{
	unsigned int pmu08;
	unsigned int pmu0c;
	volatile int timeout;

	if ((REG_VAL32(REG_MALI400_PMU + 8) & msk) == msk)
		return 0;

	pmu08 = REG_VAL32(REG_MALI400_PMU + 8);
	pmu0c = REG_VAL32(REG_MALI400_PMU + 0xc);
	REG_SET32(REG_MALI400_PMU + 0xc, 0);

	asm volatile("" ::: "memory");

	/* PMU_POWER_DOWN */
	REG_SET32(REG_MALI400_PMU + 4, msk);

	asm volatile("" ::: "memory");

	timeout = 10;

	do {
		if ((REG_VAL32(REG_MALI400_PMU + 8) & msk) == msk)
			break;
		msleep_interruptible(1);
		timeout--;
	} while (timeout > 0);

#if MALI_DEBUG
	if (timeout == 0)
		printk("mali_pmu_power_down(0x%x) = 0x%08x -> 0x%08x: fail\n",
			msk, pmu08, REG_VAL32(REG_MALI400_PMU + 8));
	else
		printk("mali_pmu_power_down(0x%x) = 0x%08x -> 0x%08x: pass\n",
			msk, pmu08, REG_VAL32(REG_MALI400_PMU + 8));
#endif

	REG_SET32(REG_MALI400_PMU + 0xc, pmu0c);

	msleep_interruptible(10);

	if (timeout == 0) {
		printk("mali pmu power down failure\n");
		return -1;
	}

	return 0;
}

static void mali_show_info(void)
{
	/* GP_CONTR_REG_VERSION */
	printk("maligp: version = 0x%08x\n", REG_VAL32(REG_MALI400_GP + 0x6c));

	/* PP0_VERSION */
	printk("malipp: version = 0x%08x\n", REG_VAL32(REG_MALI400_PP0 + 0x1000));
}

struct mali_device *create_mali_device(void)
{
	struct mali_device *dev;
       
	dev = kcalloc(1, sizeof(struct mali_device), GFP_KERNEL);
	dev->suspend = &mali_suspend;
	dev->resume = &mali_resume;
	dev->enable_clock = &mali_enable_clock;
	dev->enable_power = &mali_enable_power;
	dev->set_memory_base = &mali_set_memory_base;
	dev->set_memory_size = &mali_set_memory_size;
	dev->set_mem_validation_base = &mali_set_mem_validation_base;
	dev->set_mem_validation_size = &mali_set_mem_validation_size;
	dev->get_memory_base = &mali_get_memory_base;
	dev->get_memory_size = &mali_get_memory_size;
	dev->get_mem_validation_base = &mali_get_mem_validation_base;
	dev->get_mem_validation_size = &mali_get_mem_validation_size;

	return dev;
}

void release_mali_device(struct mali_device *dev)
{
	kfree(dev);
}

EXPORT_SYMBOL(create_mali_device);
EXPORT_SYMBOL(release_mali_device);

static int get_mali_param(struct mali_param_s *param)
{
	unsigned char buf[32];
	int varlen = 32;

	/* wmt.mali.param 1:-1:-1:-1 */

	if (wmt_getsyspara("wmt.mali.param", buf, &varlen) == 0) {
		sscanf(buf, "%d:%d:%d:%d", &param->enabled,
			&param->buflen1, &param->buflen2, &param->buflen3);
	} else {
		param->enabled =  1;
		param->buflen1 = -1; /* auto */
		param->buflen2 = -1; /* auto */
		param->buflen3 = -1; /* auto */
	}

	printk(KERN_INFO "wmt.mali.param = %d:%d:%d:%d\n", param->enabled,
		param->buflen1, param->buflen2, param->buflen3);

	if (param->enabled) {
		/* MBytes -> Bytes */
		if (param->buflen1 > 0)
			param->buflen1 <<= 20;
		if (param->buflen2 > 0)
			param->buflen2 <<= 20;
		if (param->buflen3 > 0)
			param->buflen3 <<= 20;
	} else {
		param->buflen1 = 0;
		param->buflen2 = 0;
		param->buflen3 = 0;
	}

	return 0;
}

static int mali_suspend(u32 cores)
{
#if MALI_DEBUG
	printk("mali_suspend(%d)\n", cores);
#endif
	return 0;
}

static int mali_resume(u32 cores)
{
#if MALI_DEBUG
	printk("mali_resume(%d)\n", cores);
#endif
	return 0;
}

static void mali_enable_clock(int enable)
{
	int clk_en;

	/*
	 * if your enable clock with auto_pll_divisor() twice,
	 * then you have to call it at least twice to disable clock.
	 * It is really bad.
	 */

	if (enable) {
		auto_pll_divisor(DEV_MALI, CLK_ENABLE, 0, 0);
#if MALI_DEBUG
		printk("Mali clock enabled\n");
#endif
	} else {
		do {
			clk_en = auto_pll_divisor(DEV_MALI, CLK_DISABLE, 0, 0);
		} while (clk_en);
#if MALI_DEBUG
		printk("Mali clock disabled\n");
#endif
	}
}

static void mali_enable_power(int enable)
{
	/* Mali-400's power was always enabled on WM3481. */
}

static void mali_set_memory_base(unsigned int val)
{
	spin_lock(&mali_spinlock);
	mali_memory_base = val;
	spin_unlock(&mali_spinlock);
}

static void mali_set_memory_size(unsigned int val)
{
	spin_lock(&mali_spinlock);
	mali_memory_size = val;
	spin_unlock(&mali_spinlock);
}

static void mali_set_mem_validation_base(unsigned int val)
{
	spin_lock(&mali_spinlock);
	mali_mem_validation_base = val;
	spin_unlock(&mali_spinlock);
}

static void mali_set_mem_validation_size(unsigned int val)
{
	spin_lock(&mali_spinlock);
	mali_mem_validation_size = val;
	spin_unlock(&mali_spinlock);
}

static void mali_get_memory_base(unsigned int *val)
{
	spin_lock(&mali_spinlock);
	*val = mali_memory_base;
	spin_unlock(&mali_spinlock);
}

static void mali_get_memory_size(unsigned int *val)
{
	spin_lock(&mali_spinlock);
	*val = mali_memory_size;
	spin_unlock(&mali_spinlock);
}

static void mali_get_mem_validation_base(unsigned int *val)
{
	spin_lock(&mali_spinlock);
	*val = mali_mem_validation_base;
	spin_unlock(&mali_spinlock);
}

static void mali_get_mem_validation_size(unsigned int *val)
{
	spin_lock(&mali_spinlock);
	*val = mali_mem_validation_size;
	spin_unlock(&mali_spinlock);
}

/* Export functions */

int mali_platform_init_impl(void *data)
{
#if MALI_DEBUG
	printk("mali_platform_init_impl\n");
#endif
	return 0;
}

int mali_platform_deinit_impl(void *data)
{
#if MALI_DEBUG
	printk("mali_platform_deinit_impl\n");
#endif
	return 0;
}

int mali_platform_powerdown_impl(u32 cores)
{
	unsigned int status;

#if MALI_DEBUG
	printk("mali_platform_powerdown_impl(%d)\n", cores);
#endif

	status = 0x7;

#ifdef MALI_ALWAYS_ON
	return 0;
#endif /* MALI_ALWAYS_ON */

#ifdef MALI_PMU_CONTROL
	status = REG_VAL32(REG_MALI400_PMU + 8);

	/* printk("mali pmu: status = 0x08%x\n", status); */

	if ((status & 0x7) != 0x7) {
		mali_pmu_power_down(0x7);
	}
#endif

	spin_lock(&mali_spinlock);
	mali_enable_clock(0);
	mali_enable_power(0);
	spin_unlock(&mali_spinlock);

	return 0;
}

int mali_platform_powerup_impl(u32 cores)
{
	unsigned int status;

#if MALI_DEBUG
	printk("mali_platform_powerup_impl(%d)\n", cores);
#endif

#ifdef MALI_PMU_CONTROL
	status = REG_VAL32(REG_MALI400_PMU + 8);

	/* printk("mali pmu: status = 0x08%x\n", status); */

	if ((status & 0x7) != 0)
		mali_pmu_power_up(0x7);
#else
	status = 0;
#endif

	spin_lock(&mali_spinlock);
	mali_enable_power(1);
	mali_enable_clock(1);
	spin_unlock(&mali_spinlock);

	return 0;
}

void set_mali_parent_power_domain(struct platform_device* dev)
{
	/* No implemented yet */
}

static int __init mali_init(void)
{
	unsigned long smem_start;
	unsigned long smem_len;
	int err = 0;

	spin_lock_init(&mali_spinlock);

	smem_start = num_physpages << PAGE_SHIFT;

	get_mali_param(&mali_param);

	if (mali_param.buflen1 < 0)
		mali_param.buflen1 = 1 << 20; /* 1MB */

	smem_len = mali_param.buflen1;

	mali_set_memory_base(smem_start);
	mali_set_memory_size(smem_len);
	mali_set_mem_validation_base(0);
	mali_set_mem_validation_size(0);

	mali_ump_secure_id = (unsigned int) -1;
	mali_get_ump_secure_id = NULL;
	mali_put_ump_secure_id = NULL;

	if (!mali_param.enabled)
		return -1;

	mali_enable_power(1);
	mali_enable_clock(1);

	/* Wait for power stable */
	msleep_interruptible(1);

	/* Verify Mali-400 PMU */
	err += mali_pmu_power_down(0x7);
	if (!err)
		err += mali_pmu_power_up(0x2);
	if (!err)
		err += mali_pmu_power_up(0x7);
	if (!err)
		mali_show_info();
#ifndef MALI_ALWAYS_ON
	err += mali_pmu_power_down(0x5);
	err += mali_pmu_power_down(0x7);
#endif /* MALI_ALWAYS_ON */

	return err;
}

static void __exit mali_exit(void)
{
	mali_enable_clock(0);
	mali_enable_power(0);
}

module_init(mali_init);
module_exit(mali_exit);

MODULE_AUTHOR("WonderMedia Technologies, Inc.");
MODULE_DESCRIPTION("Mali PM Kernel Driver");
MODULE_LICENSE("GPL");

