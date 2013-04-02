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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <mach/hardware.h>
#include "com-ge.h"
#include "ge_accel.h"

#define VPU

#ifdef VPU
#include "../vpp.h"
extern void vpp_wait_vsync(int idx, int cnt);
extern void vpp_set_mutex(int idx,int lock);
extern void vpp_get_info(int fbn,struct fb_var_screeninfo *var);
extern unsigned int *vpp_backup_reg(unsigned int addr, unsigned int size);
extern int vpp_restore_reg(unsigned int addr, unsigned int size,
		unsigned int *reg_ptr);
extern int vpp_pan_display(struct fb_var_screeninfo *var,
		struct fb_info *info, int enable);
#endif /* VPU */

extern unsigned long msleep_interruptible(unsigned int msecs);
extern int auto_pll_divisor(enum dev_id dev, enum clk_cmd cmd,
		int unit, int freq);

#ifdef CONFIG_LOGO_WMT_ANIMATION
#include "../bootanimation/animation.h"
#endif

#ifdef DEBUG
#define DPRINTK(fmt, args...) \
	printk(KERN_WARN "%s: " fmt, __func__ , ## args)
#define ENTER() DPRINTK("Enter %s, file:%s line:%d\n", \
	__func__, __FILE__, __LINE__)
#define LEAVE() DPRINTK("Exit %s, file:%s line:%d\n",\
	__func__, __FILE__, __LINE__)
#else
#define DPRINTK(fmt, args...)
#define ENTER()
#define LEAVE()
#endif

#define REG_GOV_GE_EN  (GOVM_BASE_ADDR + 0x8)      /* remapped 0xd8050308 */
#define REG_VBIE       (VPP_BASE_ADDR + 0x4)       /* remapped 0xd8050f04 */
#define REG_GOVRH      (0xd8050890 + WMT_MMAP_OFFSET)

#define GE_OPTION_NONE       0x00000000
#define GE_OPTION_SYNC       0x00000001
#define GE_OPTION_ASYNC      0x00000002
#define GE_OPTION_NOSYNC     0x00000004
#define GE_OPTION_SYNC2      0x00000008

/* #define GE_SW_ONLY */

DECLARE_WAIT_QUEUE_HEAD(ge_wq);
static ge_info_t *geinfo;
static ge_surface_t primary_surface[1];
static unsigned int *ge_regs_save;
static unsigned int *ge_regs_save2;
#if GE_DEBUG
static unsigned int ge_regs_save_cksum;
#endif
extern int ge_options;

#define GE_STATUS

#ifdef GE_STATUS
#include <linux/proc_fs.h>
#include <linux/fs.h>
static struct proc_dir_entry *ge_procfs_dir;
static struct proc_dir_entry *ge_procfs_status_file;
static int init_procfs_ge(void);
static void exit_procfs_ge(void);
int ge_status_flip_cnt;
int ge_status_flip_req;
#endif

unsigned int num_skip_frames = 0;
unsigned int fb_egl_swap = 0; /* obsoleted */

/**************************
 *    Export functions    *
 **************************/

#define M(x) ((x)<<20)

unsigned int phy_mem_end(void)
{
	unsigned int memsize = (num_physpages << PAGE_SHIFT);

	if (memsize > M(512)) {         /* 1024M */
		memsize = M(1024);
	} else if (memsize > M(256)) {  /* 512M */
		memsize = M(512);
	} else if (memsize > M(128)) {  /* 256M */
		memsize = M(256);
	} else if (memsize > M(64)) {   /* 128M */
		memsize = M(128);
	} else if (memsize > M(32)) {   /* 64M */
		memsize = M(64);
	} else if (memsize > M(16)) {   /* 32M */
		memsize = M(32);
	} else {
		memsize = M(0);
	}
	DPRINTK("GE has detected that RAM size is %d MB \n", memsize>>20);

	return memsize;
}

unsigned int phy_mem_end_sub(u32 size)
{
	return phy_mem_end() - M(size);
}
EXPORT_SYMBOL(phy_mem_end_sub);

static irqreturn_t ge_interrupt(int irq, void *dev_id)
{
	volatile ge_regs_t *regs = (ge_regs_t *)geinfo->mmio;
	volatile unsigned int val;

	/* save flags */
	val = regs->ge_int_stat;
	regs->ge_int_stat = val | regs->ge_int_flag;
		
	/* write clear regs->ge_int_flag to stop interrupts */
	regs->ge_int_flag = ~0;

	ge_mb();

	if ((regs->ge_int_stat & BIT8) == 0 || regs->ge_status != 0)
		printk("%s: error. ge_status = %08x, ge_int_stat = %08x\n",
			__func__, regs->ge_status, regs->ge_int_stat);

	wake_up_interruptible(&ge_wq);

	return IRQ_HANDLED;
}

void ge_clock_enable(int enable)
{
	int clk_en;

	/*
	 * if your enable clock with auto_pll_divisor() twice,
	 * then you have to call it at least twice to disable clock.
	 * It is really bad.
	 */

	if (enable) {
		auto_pll_divisor(DEV_GE, CLK_ENABLE, 0, 0);
	} else {
		do {
			clk_en = auto_pll_divisor(DEV_GE, CLK_DISABLE, 0, 0);
		} while (clk_en);
	}
}

#if GE_DEBUG
static unsigned int checksum(unsigned int *addr, int len)
{
	unsigned int sum;

	sum = 0;

	while (len) {
		sum += *addr;
		addr++;
		len -= 4;
	}

	return sum;
}

static void print_diff(unsigned int *src, unsigned int *dst, int len)
{
	while (len) {
		if (*src != *dst)
			printk("%p:0x%08x != %p:0x%08x\n",
				src, *src, dst, *dst);
		src++;
		dst++;
		len -= 4;
	}
}
#endif /* GE_DEBUG */

/* ge_vo_functions depends vpu to work */

void ge_vo_get_default_var(struct fb_var_screeninfo *var)
{
#ifdef VPU
	vpp_get_info(0,var);
#endif
}

void ge_vo_wait_vsync(void)
{
#ifdef VPU
	if ((ge_options & GE_OPTION_NOSYNC) == 0) {
		vpp_set_mutex(0,1);
		vpp_wait_vsync(0, 1);
		vpp_set_mutex(0,0);
	}

#endif
}

static int needs_ge_amx(void)
{
	int enable;
#ifdef VPU
	vpp_set_mutex(0,1);
	enable = (vpp_get_govm_path() & VPP_PATH_GOVM_IN_VPU) ? 1 : 0;
	vpp_set_mutex(0,0);
#else
	enable = 0;
#endif
	return enable;
}

static int ge_vo_pan_display(struct fb_var_screeninfo *var,
			     struct fb_info *info)
{
#ifdef VPU
	int direct;

	vpp_set_mutex(0,1);
	direct = !((vpp_get_govm_path() & VPP_PATH_GOVM_IN_VPU));
	vpp_pan_display(var, info, direct);
	vpp_set_mutex(0,0);
#endif
#ifdef GE_STATUS
	ge_status_flip_cnt++;
#endif

	return 0;
}

static void ge_vo_enable_overlay(int val)
{
#ifdef VPU
	vpp_set_govm_path(VPP_PATH_GOVM_IN_GE, val);
#endif
}

static unsigned int *ge_backup_reg(unsigned int addr,unsigned int size)
{
#ifdef VPU
	return vpp_backup_reg(addr, size);
#else
	return NULL;
#endif
}

static int ge_restore_reg(unsigned int addr, unsigned int size,
	unsigned int *ptr)
{
#ifdef VPU
	return vpp_restore_reg(addr, size, ptr);
#else
	return 0;
#endif
}

#ifdef GE_SW_ONLY
static void iomemcpy(unsigned long src, unsigned long dst, size_t len)
{
	void *psrc = ioremap(src, len);
	void *pdst = ioremap(dst, len);
	void *ptmp = kmalloc(len, GFP_KERNEL);

	if (psrc && pdst && ptmp) {
		memcpy(ptmp, psrc, len);
		memcpy(pdst, ptmp, len);
	}

	if (ptmp) kfree(ptmp);
	if (psrc) iounmap(psrc);
	if (pdst) iounmap(pdst);
}
#endif /* GE_SW_ONLY */

struct ge_var {
	struct fb_info *info;
	struct fb_var_screeninfo var[1];
	struct fb_var_screeninfo new_var[1];
	struct workqueue_struct *wq;
	struct work_struct work;
	struct timeval most_recent_flip_time;
	int dirty;
	int force_sync;
	spinlock_t lock[1];
	void (*start)(struct ge_var *ge_var);
	void (*stop)(struct ge_var *ge_var);
	void (*get)(struct ge_var *ge_var, struct fb_var_screeninfo *var);
	void (*set)(struct ge_var *ge_var, struct fb_var_screeninfo *var);
	void (*sync)(struct ge_var *ge_var);
};

static struct ge_var *ge_var_s;

static void ge_var_start(struct ge_var *ge_var);
static void ge_var_stop(struct ge_var *ge_var);
static void ge_var_get(struct ge_var *ge_var, struct fb_var_screeninfo *var);
static void ge_var_set(struct ge_var *ge_var, struct fb_var_screeninfo *var);
static void ge_var_sync(struct ge_var *ge_var);
static void ge_var_sync2(struct ge_var *ge_var);

static void ge_var_work(struct work_struct *work)
{
	struct ge_var *ge_var = container_of(work, struct ge_var, work);
	ge_var->sync(ge_var);
}

static struct ge_var *create_ge_var(struct fb_info *info)
{
	struct ge_var *ge_var;

	ge_var = (struct ge_var *)
		kcalloc(1, sizeof(struct ge_var), GFP_KERNEL);

	ge_var->wq = create_singlethread_workqueue("ge_var_wq");
	ge_var->info = info;
	ge_var->start = &ge_var_start;
	ge_var->stop = &ge_var_stop;
	ge_var->get = &ge_var_get;
	ge_var->set = &ge_var_set;
	ge_var->sync = &ge_var_sync;

	do_gettimeofday(&ge_var->most_recent_flip_time);

	INIT_WORK(&ge_var->work, ge_var_work);

	ge_var->start(ge_var);

	return ge_var;
}

static void release_ge_var(struct ge_var *ge_var)
{
	if (ge_var) {
		ge_var->stop(ge_var);
		flush_workqueue(ge_var->wq);
		destroy_workqueue(ge_var->wq);
		kfree(ge_var);
	}
}

static void ge_var_start(struct ge_var *ge_var)
{
	spin_lock_init(ge_var->lock);
}

static void ge_var_stop(struct ge_var *ge_var)
{
}

static void ge_var_get(struct ge_var *ge_var, struct fb_var_screeninfo *var)
{
	spin_lock(ge_var->lock);
	memcpy(var, ge_var->var, sizeof(struct fb_var_screeninfo));
	spin_unlock(ge_var->lock);
}

static void ge_var_set(struct ge_var *ge_var, struct fb_var_screeninfo *var)
{
	int sync;
	int async;

	spin_lock(ge_var->lock);
	if (memcmp(ge_var->new_var, var, sizeof(struct fb_var_screeninfo))) {
		memcpy(ge_var->new_var, var, sizeof(struct fb_var_screeninfo));
		ge_var->dirty++;
	}
	spin_unlock(ge_var->lock);

	sync  = ((ge_options & (GE_OPTION_ASYNC | GE_OPTION_SYNC)) == GE_OPTION_SYNC);
	async = ((ge_options & (GE_OPTION_ASYNC | GE_OPTION_SYNC)) == GE_OPTION_ASYNC);

	if (async)
		queue_work(ge_var->wq, &ge_var->work);
	else if (sync || (var->activate & FB_ACTIVATE_VBL))
		ge_var->sync(ge_var);
	else
		queue_work(ge_var->wq, &ge_var->work);
}

static void ge_var_sync(struct ge_var *ge_var)
{
	if (ge_options & GE_OPTION_SYNC2)
		return ge_var_sync2(ge_var);

	spin_lock(ge_var->lock);

	if (ge_var->dirty == 0) {
		spin_unlock(ge_var->lock);
		return;
	}

	memcpy(ge_var->var, ge_var->new_var, sizeof(struct fb_var_screeninfo));
	ge_vo_pan_display(ge_var->var, ge_var->info);
	ge_var->dirty = 0;

	spin_unlock(ge_var->lock);

	ge_vo_wait_vsync();
}

static void ge_var_sync2(struct ge_var *ge_var)
{
	struct timeval t;
	struct timeval *mrft;
	unsigned long us;

	spin_lock(ge_var->lock);

	if (ge_var->dirty == 0) {
		spin_unlock(ge_var->lock);
		return;
	}

	memcpy(ge_var->var, ge_var->new_var, sizeof(struct fb_var_screeninfo));

	mrft = &ge_var->most_recent_flip_time;
	do_gettimeofday(&t);

	us = (t.tv_sec - mrft->tv_sec) * 1000000 +
		(t.tv_usec - mrft->tv_usec);

	ge_vo_pan_display(ge_var->var, ge_var->info);
	ge_var->dirty = 0;

	spin_unlock(ge_var->lock);

	/* 60 fps */
	if (us < 16667)
		ge_vo_wait_vsync();

	do_gettimeofday(&ge_var->most_recent_flip_time);
}

static unsigned long get_buffer(struct fb_info *info, int id)
{
	struct fb_var_screeninfo *var;
	unsigned long len;
	unsigned long offset;

	var = &info->var;
	len = var->yres * var->xres_virtual * (var->bits_per_pixel >> 3);
	offset = len * id;

	if ((offset + len) > info->fix.smem_len)
		return info->fix.smem_start;

	return info->fix.smem_start + offset;
}

static int get_surface(struct fb_info *info,
		       struct fb_var_screeninfo *var,
		       ge_surface_t *s)
{
	unsigned long offset;

	if (!var)
		var = &info->var;

	offset = (var->yoffset * var->xres_virtual + var->xoffset);
	offset *= var->bits_per_pixel >> 3;

	s->addr = info->fix.smem_start + offset;
	s->xres = var->xres;
	s->yres = var->yres;
	s->xres_virtual = var->xres_virtual;
	s->yres_virtual = var->yres;
	s->x = 0;
	s->y = 0;

	switch (info->var.bits_per_pixel) {
	case 8:
		s->pixelformat = GSPF_LUT8;
		break;
	case 16:
		if ((info->var.red.length == 5) &&
			(info->var.green.length == 6) &&
			(info->var.blue.length == 5)) {
			s->pixelformat = GSPF_RGB16;
		} else if ((info->var.red.length == 5) &&
			(info->var.green.length == 5) &&
			(info->var.blue.length == 5)) {
			s->pixelformat = GSPF_RGB555;
		} else {
			s->pixelformat = GSPF_RGB454;
		}
		break;
	case 32:
		s->pixelformat = GSPF_RGB32;
		break;
	default:
		s->pixelformat = GSPF_RGB16;
		break;
	}
	return 0;
}

static int surface_changed(ge_surface_t *s1, ge_surface_t *s2)
{
	if (s1->addr != s2->addr) {
		/*
		printk(KERN_DEBUG "%s: addr: %p -> %p\n",
			__func__,
			(void *)s1->addr, (void *)s2->addr);
		*/
		return 1;
	}
	if (s1->xres_virtual != s2->xres_virtual) {
		/*
		printk(KERN_DEBUG
		       "%s: xres_virtual %lu -> %lu\n",
			__func__,
		       s1->xres_virtual, s2->xres_virtual);
		*/
		return 1;
	}
	if (s1->x != s2->x || s1->y != s2->y ||
	    s1->xres != s2->xres || s1->yres != s2->yres) {
		/*
		printk(KERN_DEBUG
		       "%s: {%lu,%lu,%lu,%lu} -> {%lu,%lu,%lu,%lu}\n",
		       __func__,
		       s1->x, s1->y, s1->xres, s1->yres,
		       s2->x, s2->y, s2->xres, s2->yres);
		*/
		return 1;
	}
	if (s1->pixelformat != s2->pixelformat) {
		/*
		printk(KERN_DEBUG "%s: pixfmt %lu -> %lu\n",
		       __func__,
		       s1->pixelformat, s2->pixelformat);
		*/
		return 1;
	}

	return 0;
}

static void monitor_display_address(const char *str)
{
	/*
	ge_surface_t s;

	amx_get_surface(geinfo, 0, &s);
	printk("%s: s.addr = %p, govrh = %p\n",
		str,
		(void *)s.addr,
		(void *)REG_VAL32(REG_GOVRH));
	*/
}

static int lock_screen(struct fb_info *info)
{
	struct fb_var_screeninfo var;
	ge_surface_t s;
	struct ge_var *ge_var;

	get_surface(info, &info->var, &s);
	s.addr = get_buffer(info, GE_FB_NUM - 1);

	/* clear internal buffer */
	ge_lock(geinfo);
	ge_wait_sync(geinfo);
	ge_set_color(geinfo, 0, 0, 0, 0, s.pixelformat);
	ge_set_destination(geinfo, &s);
	ge_set_dst_pixelformat(geinfo, s.pixelformat);
	ge_set_command(geinfo, GECMD_BLIT, 0xf0);
	ge_wait_sync(geinfo);
	ge_unlock(geinfo);

	if (needs_ge_amx()) {
		amx_show_surface(geinfo, 0, &s, 0, 0); /* id:0, x:0, y:0 */
		amx_sync(geinfo);
	}

	/* REG_SET32(REG_GOVRH, (unsigned int)s.addr); */

	memcpy(&var, &info->var, sizeof(struct fb_var_screeninfo));
	var.xoffset = 0;
	var.yoffset = var.yres * (GE_FB_NUM - 1);
	ge_var = ge_var_s;
	ge_var->set(ge_var, &var);
	ge_var->sync(ge_var);

	printk(KERN_DEBUG "gefb: screen locked to %p\n",
		(void *) s.addr);

	return 0;
}

static int unlock_screen(struct fb_info *info)
{
	return 0;
}

/**
 * ge_init - Initial and display framebuffer.
 *
 * Fill the framebuffer with a default color, back.
 * Display the framebuffer using GE AMX.
 *
 * Although VQ is supported in design, I just can't find any benefit
 * from VQ. It wastes extra continuous physical memory, and runs much
 * slower than direct register access. Moreover, the source code
 * becomes more complex and is hard to maintain. Accessing VQ from
 * the user space is also a nightmare. In brief, the overhead of VQ makes
 * it useless. In order to gain the maximum performance
 * from GE and to keep the driver simple, I'm going to stop using VQ.
 * I will use VQ only when it is necessary.
 *
 * @info is the fb_info provided by framebuffer driver.
 * @return zero on success.
 */
int ge_init(struct fb_info *info)
{
	static int boot_init; /* boot_init = 0 */
	volatile ge_regs_t *regs;
	unsigned int chip_id;
	unsigned int ge_irq;
	ge_surface_t s;
#ifndef GE_SW_ONLY
	ge_surface_t window;
#endif /* GE_SW_ONLY */
	ge_surface_t logo;
	int i;
	int len;
	vout_info_t *vo;
	/*
	 * Booting time initialization
	 */
	if (!boot_init) {
#ifdef USE_GE_PWR_CTRL
		ge_clock_enable(1);
#endif /* USE_GE_PWR_CTRL */

		ge_sys_init(&geinfo, info);

		DPRINTK(KERN_INFO "ge: iomem region at 0x%lx, mapped to 0x%x, "
			"using %d, total %d\n",
			info->fix.mmio_start, (u32) geinfo->mmio,
			info->fix.mmio_len, info->fix.mmio_len);

		ge_get_chip_id(geinfo, &chip_id);
		chip_id >>= 16;
		/*
		printk(KERN_INFO "chip is 0x%08x (0x%08x)\n", chip_id, geinfo->id);
		*/

		switch (chip_id) {
		case 0x3451:		/* WM8440 */
		case 0x3465:		/* WM8650 */
			ge_irq = IRQ_VPP_IRQ7;
			break;
		case 0x3445:		/* WM8710 */
		case 0x3481:		/* WM8950 */
			ge_clock_enable(1);
			ge_irq = IRQ_VPP_IRQ7;
			break;
		default:
			ge_irq = 0;
			break;
		}

		regs = geinfo->mmio;

		/* 0x00000 (fastest) - 0x30003 (slowest) */
		ge_lock(geinfo);
		regs->ge_delay = 0x10001;
		ge_unlock(geinfo);

		/*
		 * GE interrupt is enabled by default.
		 */
		if (ge_irq) {
			request_irq(ge_irq, ge_interrupt, IRQF_DISABLED,
				"ge", NULL);
			ge_lock(geinfo);
			regs->ge_int_en = BIT8 | BIT9;
			ge_unlock(geinfo);
		}

		ge_vo_enable_overlay(1);

		amx_set_csc(geinfo, AMX_CSC_JFIF_0_255);
		amx_set_alpha(geinfo, 0, 0xff);

		/* Set primary surface uninitialized to active ge_pan_display */
		memset(primary_surface, 0, sizeof(ge_surface_t););

		/* Get fb surface */
		get_surface(info, &info->var, &s);

		/*
		 * U-Boot logo is at mali's memory.
		 * Copy U-Boot logo to GE's memory and
		 * let GOVRH to read GE's memory.
		 */
		memcpy(&logo, &s, sizeof(ge_surface_t));

		vo = vout_info_get_entry(0);
		vo->govr->fb_p->get_addr((unsigned int *)&logo.addr,(unsigned int *)&i);
		
		if (logo.addr != s.addr) {
			/* Due to U-Boot logo may overlap to 1st buffer,
			 * we have to copy U-Boot logo to 2nd buffer first,
			 * and then copy 2nd buffer to 1st buffer.
			 */
			len = s.xres_virtual * s.yres *
				GSPF_BYTES_PER_PIXEL(s.pixelformat);
			s.addr += len * GE_FB_NUM;
			i = GE_FB_NUM;
			while (i--) {
				s.addr -= len;
				/*
				printk(KERN_DEBUG "copy logo from %p to %p\n",
					(void *)logo.addr, (void *)s.addr);
				*/
#ifdef GE_SW_ONLY
				iomemcpy(logo.addr, s.addr, len);
#else  /* GE_SW_ONLY */
				ge_lock(geinfo);
				ge_set_source(geinfo, &logo);
				ge_set_destination(geinfo, &s);
				ge_set_dst_pixelformat(geinfo, s.pixelformat);
				ge_set_command(geinfo, GECMD_BLIT, 0xcc);
				ge_wait_sync(geinfo);
				ge_unlock(geinfo);
#endif /* GE_SW_ONLY */
				logo.addr = s.addr;
				p_govrh->fb_p->fb.y_addr = s.addr;
				p_govrh->fb_p->fb.c_addr = 0;
				govrh_set_fb_addr(p_govrh,s.addr,0);
				p_govrh2->fb_p->fb.y_addr = s.addr;
				p_govrh2->fb_p->fb.c_addr = 0;
				govrh_set_fb_addr(p_govrh2,s.addr,0);
			}
		}

#ifndef GE_SW_ONLY
		/* Clear GE's memory */

		get_surface(info, &info->var, &window);
		i = GE_FB_NUM;
		while (i--) {
			window.addr = get_buffer(info, i);
			if (window.addr != logo.addr) {
				ge_lock(geinfo);
				ge_set_color(geinfo, 0, 0, 0, 0,
						window.pixelformat);
				ge_set_destination(geinfo, &window);
				ge_set_dst_pixelformat(geinfo,
						window.pixelformat);
				ge_set_command(geinfo, GECMD_BLIT, 0xf0);
				ge_wait_sync(geinfo);
				ge_unlock(geinfo);
			}
		}
#endif /* GE_SW_ONLY */

		ge_var_s = create_ge_var(info);

		boot_init = 1;
#ifdef GE_STATUS
		ge_status_flip_req = ge_status_flip_cnt = 0;
		init_procfs_ge();
#endif /* GE_STATUS */
#ifdef USE_GE_PWR_CTRL
		ge_clock_enable(0);
#endif /* USE_GE_PWR_CTRL */
	}

	return 0;
}

/**
 * ge_exit - Disable GE.
 *
 * No memory needs to be released here.
 * Turn off the AMX to stop displaying framebuffer.
 * Update the index of MMU.
 *
 * @info is fb_info from fbdev.
 * @return zero on success.
 */
int ge_exit(struct fb_info *info)
{
#ifdef GE_STATUS
	exit_procfs_ge();
#endif
	release_ge_var(ge_var_s);
	release_mem_region(info->fix.mmio_start, info->fix.mmio_len);
	ge_sys_exit(geinfo, info);

	return 0;
}

static int get_args(unsigned int *to, void *from, int num)
{
	unsigned int count;

	count = sizeof(unsigned int);
	count *= num;

	if (copy_from_user(to, from, count)) {
		printk(KERN_ERR "%s: copy_from_user failure\n", __func__);
		return  -EFAULT;
	}

	return 0;
}

/**
 * ge_ioctl - Extension for fbdev ioctl.
 *
 * Not quite usable now.
 *
 * @return zero on success.
 */
int ge_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	volatile ge_regs_t *regs;
	int ret = 0;
	unsigned int chip_id;
	unsigned int args[8];

	regs = geinfo->mmio;

	unlock_fb_info(info);

	switch (cmd) {
	case GEIOSET_AMX_EN:
		if (arg) {
			/* Only enable G1 */
			ge_lock(geinfo);
			regs->g1_amx_en = 1;
			regs->g2_amx_en = 0;
			regs->ge_reg_upd = 1;
			ge_unlock(geinfo);
		} else {
			/* Disable G1 and G2 */
			ge_lock(geinfo);
			regs->g1_amx_en = 0;
			regs->g2_amx_en = 0;
			regs->ge_reg_upd = 1;
			ge_unlock(geinfo);
		}
		break;
	case GEIO_ALPHA_BLEND:
	case GEIOSET_OSD:
		ge_alpha_blend((u32)arg);
		break;
	case GEIOSET_COLORKEY:
		ge_set_amx_colorkey((u32)arg, 0);
		break;
	case GEIO_WAIT_SYNC:
		ge_wait_sync_timeout(geinfo, arg);
		break;
	case GEIO_LOCK:
		switch (arg) {
		case 0:
			ret = ge_unlock(geinfo);
			break;
		case 1:
			ret = ge_lock(geinfo);
			break;
		default:
			ret = ge_trylock(geinfo);
			break;
		}
		break;
	case GEIOGET_CHIP_ID:
		ge_get_chip_id(geinfo, &chip_id);
		copy_to_user((void *)arg, (void *) &chip_id,
			sizeof(unsigned int));
		break;
	case GEIO_ROTATE:
		ret = get_args(args, (void *) arg, 6);
		if (ret == 0) {
			ge_simple_rotate(args[0], args[1], args[2], args[3],
					 args[4], args[5]);
		}
		break;
#ifdef CONFIG_LOGO_WMT_ANIMATION
	case GEIO_STOP_LOGO:
		printk("fan GEIO_STOP_LOGO\n");
		ret = animation_stop();
		break;
#endif
	case GEIO_ALLOW_PAN_DISPLAY:
		/* obsoleted ioctl */
		break;
	default:
		ret = -1;
	}

	lock_fb_info(info);

	return ret;
}

int ge_setcolreg(unsigned regno, unsigned red, unsigned green, unsigned blue,
	unsigned transp, struct fb_info *info)
{
	/* N/A */
	return 0;
}

void ge_poll_vsync(void)
{
	const int has_vbie = 1;

	if (has_vbie) {
		REG_VAL32(REG_VBIE) |= 0x4; /* VBIE */

		while (!(REG_VAL32(REG_VBIE) & 0x4))
			msleep_interruptible(10); /* 10 ms */
	}
}

void ge_wait_vsync(void)
{
	struct ge_var *ge_var = ge_var_s;

	ge_var->sync(ge_var);
}

int ge_sync(struct fb_info *info)
{
	/*
	ge_lock(geinfo);
	ge_wait_sync(geinfo);
	ge_unlock(geinfo);
	*/

	return 0;
}

int ge_release(struct fb_info *info)
{
	struct ge_var *ge_var = ge_var_s;

	if (ge_sem_owner == current)
		return ge_unlock(geinfo);

	ge_var->sync(ge_var); /* apply pending changes */

	return 0;
}

#define CONFIG_AMX_CROP_EN 0

/**
 * ge_pan_display - Pans the display.
 *
 * Pan (or wrap, depending on the `vmode' field) the display using the
 * `xoffset' and `yoffset' fields of the `var' structure.
 * If the values don't fit, return -EINVAL.
 *
 * @var: frame buffer variable screen structure
 * @info: frame buffer structure that represents a single frame buffer
 *
 * Returns negative errno on error, or zero on success.
 */
int ge_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	ge_regs_t *regs;
	ge_surface_t s;
	struct ge_var *ge_var;

	regs = (ge_regs_t *) geinfo->mmio;
	ge_var = ge_var_s;

	/*
	printk(KERN_DEBUG "%s: xoff = %d, yoff = %d, xres = %d, yres = %d \n",
	       __func__, var->xoffset, var->yoffset,
	       info->var.xres, info->var.yres);
	*/
#ifdef GE_STATUS
	ge_status_flip_req++;
#endif

	if ((var->xoffset + info->var.xres > info->var.xres_virtual) ||
	    (var->yoffset + info->var.yres > info->var.yres_virtual)) {
		/* Y-pan is used in most case.
		 * So please make sure that yres_virtual is
		 * greater than (yres + yoffset).
		 */
		printk(KERN_ERR "%s: out of range \n", __func__);
		return -EINVAL;
	}

	get_surface(info, var, &s);

	if (num_skip_frames) {
		lock_screen(info);
		num_skip_frames--;
	} else {
		unlock_screen(info);
		if ((var->activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW &&
		    surface_changed(primary_surface, &s)) {
			if (needs_ge_amx()) {
				amx_show_surface(geinfo, 0, &s, 0, 0);
				amx_sync(geinfo);
			}
			ge_var->set(ge_var, var);

			monitor_display_address(__func__);

			memcpy(primary_surface, &s, sizeof(ge_surface_t));
		}
	}

	return 0;
}

/**
 *  ge_blank - for APM
 *
 */
int ge_blank(int mode, struct fb_info *info)
{
	volatile ge_regs_t *regs;

	vpp_set_blank(info,mode);

	/* Disable FB_BLANK due to the buggy VT8430 APM. */
	return 0;

	ge_wait_sync(geinfo);

	regs = geinfo->mmio;

	switch (mode) {
	case FB_BLANK_NORMAL:
	case FB_BLANK_HSYNC_SUSPEND:
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_POWERDOWN:
		ge_vo_enable_overlay(0);
		ge_lock(geinfo);
		regs->g1_amx_en = 0;	/* G1 AMX disable */
		regs->g2_amx_en = 0;	/* G2 AMX disable */
		regs->ge_reg_upd = 1;	/* register update */
		ge_unlock(geinfo);
		break;
	case FB_BLANK_UNBLANK:
		ge_lock(geinfo);
		regs->g1_amx_en = 1;	/* G1 AMX enable */
		regs->ge_reg_upd = 1;	/* register update */
		ge_unlock(geinfo);
		ge_vo_enable_overlay(1);
		break;
	default:
		break;
	}

	return 0;
}

int ge_suspend(struct fb_info *info)
{
	volatile ge_regs_t *regs;
	unsigned int ge_regs_save_len;
	unsigned int id = geinfo->id >> 16;

	if (id <= 0x3445) {
		ge_regs_save_len = sizeof(ge_regs_t);

		ge_clock_enable(1);

		regs = geinfo->mmio;
		regs->g1_amx_en = 0;
		regs->g2_amx_en = 0;
		regs->ge_reg_sel = 1;
		regs->ge_reg_upd = 1;

		/* reset GE */
		regs->bitblt_alpha = 0xff;
		regs->ck_sel = 0;
		regs->ge_eng_en = 0;
		regs->ge_eng_en = 1;

		ge_regs_save = ge_backup_reg((unsigned int)regs, ge_regs_save_len);

		ge_clock_enable(0);
	}
	else {
		ge_regs_save_len = sizeof(ge_regs_t);

		ge_clock_enable(1);

		regs = geinfo->mmio;
		regs->g1_amx_en = 0;
		regs->g2_amx_en = 0;
		regs->ge_reg_sel = 1;
		regs->ge_reg_upd = 1;

		/* reset GE */
		regs->bitblt_alpha = 0xff;
		regs->ck_sel = 0;
		regs->ge_eng_en = 0;
		regs->ge_eng_en = 1;

		ge_regs_save = ge_backup_reg(GE1_BASE_ADDR,256);
		ge_regs_save2 = ge_backup_reg(GE1_BASE_ADDR+0x1000,256);
		ge_clock_enable(0);
	}
	return 0;
}

int ge_resume(struct fb_info *info)
{
	volatile ge_regs_t *regs;
	unsigned int ge_regs_save_len;

	ge_regs_save_len = sizeof(ge_regs_t);

	ge_clock_enable(1);

	regs = geinfo->mmio;

	ge_lock(geinfo);

#if 1
	ge_restore_reg(GE1_BASE_ADDR,256,
			ge_regs_save);
	ge_restore_reg(GE1_BASE_ADDR+0x1000,256,
			ge_regs_save2);

#else
	ge_restore_reg((unsigned int)regs, ge_regs_save_len,
			ge_regs_save);
#endif
	regs->ge_reg_sel = 0;
	regs->ge_reg_upd = 1;

	/* reset amx */
	amx_set_csc(geinfo, AMX_CSC_JFIF_0_255);
	amx_set_alpha(geinfo, 0, 0xff);

	ge_unlock(geinfo);

	/* drop the first frame to fix flicker */
	num_skip_frames = 0;

	return 0;
}

/**
 * ge_alpha_blend - Set alpha and fill transparent color to the RGB screen.
 *
 * The color consists of A, R, G, B. The screen is opaque while alpha{A} equals
 * to zero. The alpha blending formula is as follow:
 * DISPLAY = A*G1 + (0xFF - A)*VPU.
 * Please note that all colors on the G1 is effected by alpha
 * except the transparent color{R,G,B}.
 *
 * @param color is the transparency color of {A,R,G,B}
 */
void ge_alpha_blend(unsigned int color)
{
	return ge_set_amx_colorkey(color, 1);
}

void ge_set_amx_colorkey(unsigned int color, int erase)
{
	ge_surface_t s;
	u8 a, r, g, b;

	a = (u8)((color >> 24) & 0xff);
	r = (u8)((color >> 16) & 0xff);
	g = (u8)((color >> 8) & 0xff);
	b = (u8)(color & 0xff);

	/* Set transparency */
	amx_get_surface(geinfo, 0, &s);

	switch (s.pixelformat) {
	case GSPF_RGB16:
		/* 5:6:5 => 8:8:8 */
		r = (r >> 3) << 3;
		g = (g >> 2) << 2;
		b = (b >> 3) << 3;
		break;
	case GSPF_RGB555:
		/* 5:5:5 => 8:8:8 */
		r = (r >> 3) << 3;
		g = (g >> 3) << 3;
		b = (b >> 3) << 3;
		break;
	case GSPF_RGB454:
		/* 4:5:4 => 8:8:8 */
		r = (r >> 4) << 4;
		g = (g >> 3) << 3;
		b = (b >> 4) << 4;
		break;
	default:
		break;
	}

	ge_wait_sync(geinfo);

	if (a)
		amx_set_colorkey(geinfo, 0, 1, r, g, b, s.pixelformat);
	else
		amx_set_colorkey(geinfo, 0, 0, r, g, b, s.pixelformat);

	amx_sync(geinfo);

	if (erase) {
		ge_lock(geinfo);
		ge_set_color(geinfo, r, g, b, 0, s.pixelformat);
		ge_set_destination(geinfo, &s);
		ge_set_dst_pixelformat(geinfo, s.pixelformat);
		ge_set_command(geinfo, GECMD_BLIT, 0xf0);
		ge_unlock(geinfo);
	}
}

void ge_simple_rotate(unsigned int phy_src, unsigned int phy_dst,
	int width, int height, int bpp, int arc)
{
	volatile ge_regs_t *regs;
	ge_surface_t s, d;
	unsigned int chip_id;

	regs = geinfo->mmio;

	if (arc == 0) {
		ge_simple_blit(phy_src, phy_dst, width, height, bpp);
		return;
	}

	switch (bpp) {
	case 8:
		s.pixelformat = GSPF_LUT8;
		d.pixelformat = GSPF_LUT8;
		break;
	case 16:
		s.pixelformat = GSPF_RGB16;
		d.pixelformat = GSPF_RGB16;
		break;
	case 32:
		s.pixelformat = GSPF_RGB32;
		d.pixelformat = GSPF_RGB32;
		break;
	default:
		/* Not supported */
		return;
	}

	s.addr = phy_src;
	s.x = 0;
	s.y = 0;
	s.xres = width;
	s.yres = height;
	s.xres_virtual = width;
	s.yres_virtual = height;

	d.addr = phy_dst;
	d.x = 0;
	d.y = 0;

	switch (arc) {
	case 90:
	case 270:
		d.xres = height;
		d.yres = width;
		d.xres_virtual = height;
		d.yres_virtual = width;
		break;
	default:
		d.xres = width;
		d.yres = height;
		d.xres_virtual = width;
		d.yres_virtual = height;
		break;
	}

	ge_get_chip_id(geinfo, &chip_id);
	chip_id >>= 16;

	/* Rotate */
	ge_lock(geinfo);

	ge_set_source(geinfo, &s);
	ge_set_destination(geinfo, &d);
	ge_set_dst_pixelformat(geinfo, s.pixelformat);
	ge_cmd_rotate(geinfo, arc);
	ge_wait_sync(geinfo);

	ge_unlock(geinfo);
}

void ge_simple_blit(unsigned int phy_src, unsigned int phy_dst,
	int width, int height, int bpp)
{
	ge_surface_t s, d;

	switch (bpp) {
	case 8:
		s.pixelformat = GSPF_LUT8;
		d.pixelformat = GSPF_LUT8;
		break;
	case 16:
		s.pixelformat = GSPF_RGB16;
		d.pixelformat = GSPF_RGB16;
		break;
	case 32:
		s.pixelformat = GSPF_RGB32;
		d.pixelformat = GSPF_RGB32;
		break;
	default:
		/* Not supported */
		return;
	}

	s.addr = phy_src;
	s.x = 0;
	s.y = 0;
	s.xres = width;
	s.yres = height;
	s.xres_virtual = width;
	s.yres_virtual = height;

	d.addr = phy_dst;
	d.x = 0;
	d.y = 0;
	d.xres = width;
	d.yres = height;
	d.xres_virtual = width;
	d.yres_virtual = height;

	/* Blit */
	ge_lock(geinfo);

	ge_set_source(geinfo, &s);
	ge_set_destination(geinfo, &d);
	ge_set_dst_pixelformat(geinfo, s.pixelformat);
	ge_cmd_blit(geinfo);
	ge_wait_sync(geinfo);

	ge_unlock(geinfo);
}

#ifdef CONFIG_LOGO_WMT_ANIMATION
void run_boot_animation(struct fb_info *info)
{
	struct animation_fb_info ani_info;
	ge_surface_t s;

	amx_get_surface(geinfo, 0, &s);

	ani_info.width = info->var.xres;
	ani_info.height = info->var.yres;
	/* 0 = 565, 1 = 888 */
	ani_info.color_fmt = (info->var.bits_per_pixel == 16) ? 0 : 1;
	//ani_info.addr = (unsigned char * )(info->screen_base + (s.addr - info->fix.smem_start));
	ani_info.addr = (unsigned char * )info->screen_base;
	animation_start(&ani_info);
}
#endif

#ifdef GE_STATUS
static int ge_proc_read_status(char *page, char **start,
				   off_t off, int count,
				   int *eof, void *data)
{
	int len;
	char *s;
	int ret;
	/*
	ge_info_t *info;

	info = (ge_info_t *) data;
	*/
	ret = 0;
	s = page;

	len = sprintf(s, "options: %d, flip: %d / %d\n",
		ge_options, ge_status_flip_cnt, ge_status_flip_req);

	ge_status_flip_cnt = ge_status_flip_req = 0;

	ret += len;

	return ret;
}

static int __init init_procfs_ge(void)
{
	int ret = 0;

	ge_procfs_dir = proc_mkdir("ge", NULL);

	if (ge_procfs_dir == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	ge_procfs_status_file = create_proc_read_entry("status", 0444,
		ge_procfs_dir, ge_proc_read_status, NULL);

	if (ge_procfs_status_file == NULL) {
		ret = -ENOMEM;
		goto no_status;
	}

	ge_procfs_status_file->data = (void *) geinfo;

	return 0;

no_status:			      
	remove_proc_entry("ge", NULL);
out:
	return ret;

}

static void exit_procfs_ge(void)
{
	remove_proc_entry("status", ge_procfs_dir);
	remove_proc_entry("ge", NULL);
}

ge_info_t *ge_get_geinfo(void)
{
	return geinfo;
}

#endif /* GE_STATUS */
