/*++
linux/arch/arm/mach-wmt/wmt_cpufreq.c

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
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/cpufreq.h>
#include <mach/hardware.h>

//#define DEBUG
#ifdef  DEBUG
static int dbg_mask = 1;
module_param(dbg_mask, int, S_IRUGO | S_IWUSR);
#define fq_dbg(fmt, args...) \
        do {\
            if (dbg_mask) \
                printk(KERN_ERR "[%s]_%d: " fmt, __func__ , __LINE__, ## args);\
        } while(0)
#define fq_trace() \
        do {\
            if (dbg_mask) \
                printk(KERN_ERR "trace in %s %d\n", __func__, __LINE__);\
        } while(0)
#else
#define fq_dbg(fmt, args...)
#define fq_trace()
#endif

#define DVFS_TABLE_NUM_MAX  20
#define PMC_BASE            PM_CTRL_BASE_ADDR
#define PMC_PLLA            (PM_CTRL_BASE_ADDR + 0x200)
#define ARM_DIV_OFFSET      (PM_CTRL_BASE_ADDR + 0x300)
#define MALI_DIV_OFFSET     (PM_CTRL_BASE_ADDR + 0x388)
#define AHB_DIV_OFFSET      (PM_CTRL_BASE_ADDR + 0x304)
#define APB_DIV_OFFSET      (PM_CTRL_BASE_ADDR + 0x320)
#define L2C_DIV_OFFSET      (PM_CTRL_BASE_ADDR + 0x30C)
#define LAXI_DIV_OFFSET     (PM_CTRL_BASE_ADDR + 0x3B0)
#define LRAM_DIV_OFFSET     (PM_CTRL_BASE_ADDR + 0x3F0)

struct wmt_dvfs_table {
    unsigned int freq;
    unsigned int vol;
    int index;
    struct list_head node;
};

struct wmt_dvfs_driver_data {
    unsigned int tbl_num;
    unsigned int sample_rate;
    struct list_head wmt_dvfs_list;
    struct wmt_dvfs_table *dvfs_table;
    struct cpufreq_frequency_table *freq_table;
};
static struct wmt_dvfs_driver_data wmt_dvfs_drvdata;
extern int wmt_setsyspara(char *varname, unsigned char *varval);
extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlenex);

//#define WMT_CPU_TEST
#ifdef  WMT_CPU_TEST
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/fs.h>

#define WMT_CPU_DEV_MAJOR      166
#define WMT_CPU_NR_DEVS        1
#define DEV_NAME               "wmt_cpu"
#define WMT_CPU_IOC_MAGIC      's'
#define WMT_CPU_GET_INFO       _IOW(WMT_CPU_IOC_MAGIC, 1, struct wmt_plla_ioc)
#define WMT_CPU_SET_FREQ       _IOW(WMT_CPU_IOC_MAGIC, 2, struct wmt_plla_ioc)
#define WMT_CPU_SET_DIV        _IOW(WMT_CPU_IOC_MAGIC, 3, struct wmt_plla_ioc)
#define WMT_CPU_SET_DIVF       _IOW(WMT_CPU_IOC_MAGIC, 4, struct wmt_plla_ioc)
#define WMT_CPU_SET_DIVR       _IOW(WMT_CPU_IOC_MAGIC, 5, struct wmt_plla_ioc)
#define WMT_CPU_SET_DIVQ       _IOW(WMT_CPU_IOC_MAGIC, 6, struct wmt_plla_ioc)
#define WMT_CPU_IOC_OPEN_DBG   _IO(WMT_CPU_IOC_MAGIC, 20)
#define WMT_CPU_IOC_CLOSE_DBG  _IO(WMT_CPU_IOC_MAGIC, 21)

struct wmt_cpu_device {
    struct cdev cdev;
    struct wmt_dvfs_driver_data *drv_data;
};

struct wmt_plla_ioc {
    enum dev_id  dev_id;
    int freq; // plla freq
    int filter;
    int divf;
    int divr;
    int divq;
    int arm_div;
    int mali_div;
    int ahb_div;
    int apb_div;
    int l2c_div;
    int laxi_div;
    int lram_div;
};

struct wmt_cpu_device wmt_cpu_dev;
static int wmt_cpu_dev_major = 0;//WMT_CPU_DEV_MAJOR;
static int wmt_cpu_dev_minor = 0;
module_param(wmt_cpu_dev_major, int, S_IRUGO);
module_param(wmt_cpu_dev_minor, int, S_IRUGO);
static struct class *wmt_cpu_class;
static unsigned int wmt_getspeed(unsigned int cpu);
static int wmt_change_plla_freq(unsigned target);




static int wmt_cpu_dev_open(struct inode *inode, struct file *filp)
{
    struct wmt_cpu_device *cpu_dev;

    cpu_dev = container_of(inode->i_cdev, struct wmt_cpu_device, cdev);
    if (cpu_dev->drv_data == NULL) {
        fq_dbg("can not get wmt_cpu driver data\n");
        return -ENODATA;
    }
    filp->private_data = cpu_dev;

    return 0;
}

static int wmt_cpu_dev_close(struct inode *inode, struct file *filp)
{
    struct wmt_cpu_device *cpu_dev;

    cpu_dev = container_of(inode->i_cdev, struct wmt_cpu_device, cdev);
    if (cpu_dev->drv_data == NULL) {
        fq_dbg("can not get wmt_cpu driver data\n");
        return -ENODATA;
    }

    return 0;
}

static void get_plla_dev_info(struct wmt_plla_ioc *ioc)
{
    unsigned int tmp = 0;

    tmp = *(volatile unsigned int *)PMC_PLLA;
    ioc->filter = (tmp >> 24) & 0x07;
    ioc->divf = (tmp >> 16) & 0xFF;
    ioc->divr = (tmp >>  8) & 0x3F;
    ioc->divq = tmp & 0x07;
    ioc->freq = 25000 * (ioc->divf + 1) / ((1 << ioc->divq) * (ioc->divr + 1));
    ioc->arm_div    = *(volatile unsigned char *)ARM_DIV_OFFSET;
    ioc->mali_div   = *(volatile unsigned char *)MALI_DIV_OFFSET;
    ioc->l2c_div    = *(volatile unsigned char *)L2C_DIV_OFFSET;
    ioc->apb_div    = *(volatile unsigned char *)APB_DIV_OFFSET;
    ioc->ahb_div    = *(volatile unsigned char *)AHB_DIV_OFFSET;
    ioc->laxi_div   = *(volatile unsigned char *)LAXI_DIV_OFFSET;
    ioc->lram_div   = *(volatile unsigned char *)LRAM_DIV_OFFSET;

    return ;
}

static int set_plla_dev_freq(struct wmt_plla_ioc *ioc)
{   
    unsigned int target = 0;

    fq_dbg("plla need to seed freq to %dKhz\n", ioc->freq);
    if (ioc->freq < 100 * 1000 || ioc->freq > 1000 * 1000)
        return -1;

    target = ioc->freq - ioc->freq % 25000;
    fq_dbg("plla need to seed freq to %dKhz\n", ioc->freq);
    return wmt_change_plla_freq(target);
}

static int set_plla_dev_info(struct wmt_plla_ioc *ioc)
{
    struct wmt_plla_ioc tmp;

	return 0;
    get_plla_dev_info(&tmp);
    if (ioc->divf >= 0)
        tmp.divf = ioc->divf;
    if (ioc->divr >= 0)
        tmp.divr = ioc->divr;
    if (ioc->divq >= 0)
        tmp.divq = ioc->divq;

    if ((ioc->dev_id == DEV_ARM) && (ioc->arm_div >= 0))
        return manu_pll_divisor(DEV_ARM, tmp.divf, tmp.divr, tmp.divq, ioc->arm_div);
    else if ((ioc->dev_id == DEV_L2C) && (ioc->l2c_div >= 0))
        return manu_pll_divisor(DEV_L2C, tmp.divf, tmp.divr, tmp.divq, ioc->l2c_div);
    else if ((ioc->dev_id == DEV_L2CAXI) && (ioc->laxi_div >= 0))
        return manu_pll_divisor(DEV_L2CAXI, tmp.divf, tmp.divr, tmp.divq, ioc->laxi_div);
    else if ((ioc->dev_id == DEV_MALI) && (ioc->mali_div >= 0))
        return manu_pll_divisor(DEV_MALI, tmp.divf, tmp.divr, tmp.divq, ioc->mali_div);
    else if ((ioc->dev_id == DEV_APB) && (ioc->apb_div >= 0))
        return manu_pll_divisor(DEV_APB, tmp.divf, tmp.divr, tmp.divq, ioc->apb_div);
    else if ((ioc->dev_id == DEV_AHB) && (ioc->ahb_div >= 0))
        return manu_pll_divisor(DEV_AHB, tmp.divf, tmp.divr, tmp.divq, ioc->ahb_div);
    else
        return -1;        
}

static long
wmt_cpu_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    int err = 0;
    struct wmt_cpu_device *cpu_dev;
    struct wmt_dvfs_driver_data *cpu_drv;
    struct wmt_plla_ioc cpu_ioc;

    fq_dbg("Enter\n");
	return ret;
    if (_IOC_TYPE(cmd) != WMT_CPU_IOC_MAGIC)
        return -ENOTTY;

	if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    if (err == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
        err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
    if (err)
        return -EFAULT;

    cpu_dev = filp->private_data;
    cpu_drv = cpu_dev->drv_data;

    switch (cmd) {
    case WMT_CPU_GET_INFO:
        ret = copy_from_user(&cpu_ioc, (void __user *)arg, sizeof(cpu_ioc));
        if (ret == 0) {
            get_plla_dev_info(&cpu_ioc);
            ret = copy_to_user((void __user *)arg, &cpu_ioc, sizeof(cpu_ioc));
        }
        break;
    case WMT_CPU_SET_FREQ:
        ret = copy_from_user(&cpu_ioc, (void __user *)arg, sizeof(cpu_ioc));
        if (ret == 0) {
            fq_dbg("set plla freq to %dKhz\n", cpu_ioc.freq);
            ret = set_plla_dev_freq(&cpu_ioc);
            get_plla_dev_info(&cpu_ioc);
            copy_to_user((void __user *)arg, &cpu_ioc, sizeof(cpu_ioc));
        }
        break;
    case WMT_CPU_SET_DIV:
    case WMT_CPU_SET_DIVF:
    case WMT_CPU_SET_DIVR:
    case WMT_CPU_SET_DIVQ:
        ret = copy_from_user(&cpu_ioc, (void __user *)arg, sizeof(cpu_ioc));
        if (ret == 0) {
            fq_dbg("freq:%d, divf:%d, divr:%d, divq:%d\n", cpu_ioc.freq, 
                                cpu_ioc.divf, cpu_ioc.divr, cpu_ioc.divq);
            fq_dbg("divisor, arm:%d, mali:%d, ahb:%d, apb:%d, l2c:%d, lram:%d,"
                    " laxi:%d\n", cpu_ioc.arm_div, cpu_ioc.mali_div,
                    cpu_ioc.ahb_div, cpu_ioc.apb_div, cpu_ioc.l2c_div,
                    cpu_ioc.lram_div, cpu_ioc.laxi_div);            
            ret = set_plla_dev_info(&cpu_ioc);
            get_plla_dev_info(&cpu_ioc);
            copy_to_user((void __user *)arg, &cpu_ioc, sizeof(cpu_ioc));
        }
        break;
#ifdef DEBUG
    case WMT_CPU_IOC_OPEN_DBG:
        dbg_mask = 1;
        fq_dbg("wmt cpufreq debug info enable\n");
        break;
    case WMT_CPU_IOC_CLOSE_DBG:
        fq_dbg("wmt cpufreq debug info disable\n");
        dbg_mask = 0;
        break;
#endif
    default:
        fq_dbg("command unknow");
		ret = -EINVAL;
        break;
    }

    fq_dbg("Exit\n");
    return ret;
}

static struct file_operations wmt_cpu_fops = {
    .owner          = THIS_MODULE,
    .open           = wmt_cpu_dev_open,
    .unlocked_ioctl = wmt_cpu_dev_ioctl,
    .release        = wmt_cpu_dev_close,
};

static int wmt_cpu_dev_setup(void)
{
    dev_t dev_no = 0;
    int ret = 0;
    struct device *dev = NULL;

    if (wmt_cpu_dev_major) {
        dev_no = MKDEV(wmt_cpu_dev_major, wmt_cpu_dev_minor);
        ret = register_chrdev_region(dev_no, WMT_CPU_NR_DEVS, DEV_NAME);
    } else {
        ret = alloc_chrdev_region(&dev_no, wmt_cpu_dev_minor, 
                                WMT_CPU_NR_DEVS, DEV_NAME);
        wmt_cpu_dev_major = MAJOR(dev_no);
        wmt_cpu_dev_minor = MINOR(dev_no);
        fq_dbg("wmt_cpu device major is %d\n", wmt_cpu_dev_major);
    }

    if (ret < 0) {
        fq_dbg("can not get major %d\n", wmt_cpu_dev_major);
        goto out;
    }

    cdev_init(&wmt_cpu_dev.cdev, &wmt_cpu_fops);
    wmt_cpu_dev.drv_data   = &wmt_dvfs_drvdata;
    wmt_cpu_dev.cdev.owner = THIS_MODULE;
    wmt_cpu_dev.cdev.ops   = &wmt_cpu_fops;
    ret = cdev_add(&wmt_cpu_dev.cdev, dev_no, WMT_CPU_NR_DEVS);
    if (ret) {
        fq_dbg("add char dev for wmt_cpu failed\n");
        goto release_region;
    }

    wmt_cpu_class = class_create(THIS_MODULE, "wmt_cpu_class");
    if (IS_ERR(wmt_cpu_class)) {
        fq_dbg("create wmt_cpu class failed\n");
        ret = PTR_ERR(wmt_cpu_class);
        goto release_cdev;
    }

    dev = device_create(wmt_cpu_class, NULL, dev_no, NULL, DEV_NAME);
	if (IS_ERR(dev)) {
        fq_dbg("create device for wmt cpu failed\n");
		ret = PTR_ERR(dev);
		goto release_class;
	}
    goto out;

release_class:
    class_destroy(wmt_cpu_class);
    wmt_cpu_class = NULL;
release_cdev:
    cdev_del(&wmt_cpu_dev.cdev);
release_region:
    unregister_chrdev_region(dev_no, WMT_CPU_NR_DEVS);
out:
    return ret;
}

static void wmt_cpu_dev_cleanup(void)
{
    dev_t dev_no = MKDEV(wmt_cpu_dev_major, wmt_cpu_dev_minor);

    cdev_del(&wmt_cpu_dev.cdev);
    unregister_chrdev_region(dev_no, WMT_CPU_NR_DEVS);
    device_destroy(wmt_cpu_class, dev_no);
    class_destroy(wmt_cpu_class);
}
#else
static int  wmt_cpu_dev_setup(void) { return 0; }
static void wmt_cpu_dev_cleanup(void) { ; }
#endif

static int wmt_init_cpufreq_table(struct wmt_dvfs_driver_data *drv_data)
{
    int i = 0;
    struct wmt_dvfs_table *dvfs_tbl = NULL;
    struct cpufreq_frequency_table *freq_tbl = NULL;

    freq_tbl = kzalloc(sizeof(struct wmt_dvfs_table) * (drv_data->tbl_num + 1),
                                                                    GFP_KERNEL);
    if (freq_tbl == NULL) {
	    printk(KERN_ERR "%s: failed to allocate frequency table\n", __func__);
        return -ENOMEM;
    }

    drv_data->freq_table = freq_tbl;
    list_for_each_entry(dvfs_tbl, &drv_data->wmt_dvfs_list, node) {
        fq_dbg("freq_table[%d]:freq->%dKhz", i, dvfs_tbl->freq);
        freq_tbl[i].index = i;
        freq_tbl[i].frequency = dvfs_tbl->freq;
        i++;
    }
    /* the last element must be initialized to CPUFREQ_TABLE_END */
	freq_tbl[i].index = i;
	freq_tbl[i].frequency = CPUFREQ_TABLE_END;

    return 0;
}

static void wmt_exit_cpufreq_table(struct wmt_dvfs_driver_data *drv_data)
{
    if (!drv_data)
        return;
    if (drv_data->freq_table)
        kfree(drv_data->freq_table);
    if (drv_data->dvfs_table)
        kfree(drv_data->dvfs_table);

    return ;
}
static int gMaxCpuFreq=0;//added by rubbitxiao

static unsigned int wmt_getspeed(unsigned int cpu)
{
    int freq = 0;
 
    if (cpu)
        return 0;

		return gMaxCpuFreq; //added by rubbitxiao
		
    freq = auto_pll_divisor(DEV_ARM, GET_FREQ, 0, 0) / 1000;

    if (freq < 0)
        freq = 0;

    return freq;
}

static struct wmt_dvfs_table *find_freq_ceil(unsigned int *target)
{
    struct wmt_dvfs_table *dvfs_tbl = NULL;
    unsigned int freq = *target;

    list_for_each_entry(dvfs_tbl, &wmt_dvfs_drvdata.wmt_dvfs_list, node) {
        if (dvfs_tbl->freq >= freq) {
            *target = dvfs_tbl->freq;
            return dvfs_tbl;
        }
    }

    return NULL;
}

static struct wmt_dvfs_table *find_freq_floor(unsigned int *target)
{
    struct wmt_dvfs_table *dvfs_tbl = NULL;
    unsigned int freq = *target;

    list_for_each_entry_reverse(dvfs_tbl, &wmt_dvfs_drvdata.wmt_dvfs_list, node) {
        if (dvfs_tbl->freq <= freq) {
            *target = dvfs_tbl->freq;
            return dvfs_tbl;
        }
    }

    return NULL;
}

static struct wmt_dvfs_table *
wmt_recalc_target_freq(unsigned *target_freq, unsigned relation)
{
    struct wmt_dvfs_table *dvfs_tbl = NULL;

    if (!target_freq)
        return NULL;

    switch (relation) {
        case CPUFREQ_RELATION_L:
			/* Try to select a new_freq higher than or equal target_freq */
            dvfs_tbl = find_freq_ceil(target_freq);
            break;
        case CPUFREQ_RELATION_H:
			/* Try to select a new_freq lower than or equal target_freq */
            dvfs_tbl = find_freq_floor(target_freq);
            break;
        default:
            dvfs_tbl = NULL;
            break;
    }

    return dvfs_tbl;
}

static int get_arm_plla_param(void)
{
    unsigned ft, df, dr, dq, div, tmp;

    tmp =  *(volatile unsigned int *)PMC_PLLA;
    ft = (tmp >> 24) & 0x07;
    df = (tmp >> 16) & 0xFF;
    dr = (tmp >>  8) & 0x3F;
    dq = tmp & 0x07;
    
    tmp = *(volatile unsigned int *)ARM_DIV_OFFSET;
    div = tmp & 0xFF;

    fq_dbg("ft:%d, df:%d, dr:%d, dq:%d, div:%d\n", ft, df, dr, dq, div);
    return 0;
}

static int wmt_change_plla_freq(unsigned target)
{
    int ret = 0;
    int tmp = 0;
    int df, dr, dq, div;
    unsigned freq = target / 1000; // Khz to Mhz

//	return 1200000000;
		return gMaxCpuFreq;//unit is hz
    if (freq <= 1000 && freq >= 525) {
        dr  = 1;
        dq  = 1;
        div = 1;
        tmp = (1000 - (freq - freq % 25)) / 25;
        df  = 159 - 4 * tmp;
    } else if (freq < 525 && freq >= 325) {
        dr  = 1;
        div = 1;
        dq  = 2;
        tmp = (500 - (freq - freq % 25)) / 25;
        df  = 159 - 8 * tmp;
    } else if (freq >= 300) {
        df = 95;
        dr = 1;
        dq = 1;
        div = 2;
    } else if (freq >= 275) {
        df = 87;
        dr = 1;
        dq = 1;
        div = 2;
    } else if (freq >= 250) {
        df = 159;
        dr = 1;
        dq = 2;
        div = 2;
    } else if (freq >= 225) {
        df = 143;
        dr = 1;
        dq = 2;
        div = 2;
    } else if (freq >= 200) {
        df = 127;
        dr = 1;
        dq = 2;
        div = 2;
    } else if (freq >= 175) {
        df = 111;
        dr = 1;
        dq = 2;
        div = 2;
    } else if (freq >= 150) {
        df = 95;
        dr = 1;
        dq = 1;
        div = 4;
    } else if (freq >= 125) { 
        df = 159;
        dr = 1;
        dq = 2;
        div = 4;
    } else if (freq >= 100) {
        df = 127;
        dr = 1;
        dq = 2;
        div = 4;
    } else {
       printk(KERN_ERR "CPU freq can not be %dMhz\n", freq);
       ret = -EINVAL;
       return ret;
    }

    ret = manu_pll_divisor(DEV_ARM, df, dr, dq, div);
    return ret;
}


static int
wmt_target(struct cpufreq_policy *policy, unsigned target, unsigned relation)
{
    int ret = 0;
    struct cpufreq_freqs freqs;
    struct wmt_dvfs_table *dvfs_tbl = NULL;
 
    fq_dbg("cpu freq:%dMhz now, need %s to %dMhz\n", wmt_getspeed(0) / 1000,
              (relation == CPUFREQ_RELATION_L) ? "DOWN" : "UP", target / 1000);

    if (policy->cpu) {
        ret = -EINVAL;
        goto out;
    }

    /* Ensure desired rate is within allowed range.  Some govenors
     * (ondemand) will just pass target_freq=0 to get the minimum. */
    if (target < policy->min)
        target = policy->min;
    if (target > policy->max)
        target = policy->max;

    /* find out (freq, voltage) pair to do dvfs */
    dvfs_tbl= wmt_recalc_target_freq(&target, relation);
    if (dvfs_tbl == NULL) {
        fq_dbg("Can not change to target_freq:%dKhz", target);
        ret = -EINVAL;
        goto out;
    }
    fq_dbg("recalculated target freq is %dMhz\n", target / 1000);

    freqs.cpu = policy->cpu;
    freqs.new = target;
    freqs.old = wmt_getspeed(policy->cpu); 

    if (freqs.new == freqs.old) {
        ret = 0;
        goto out;
    }

    /* actually we just scaling CPU frequency here */
    cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

    get_arm_plla_param();
    ret = wmt_change_plla_freq(target);
    get_arm_plla_param();
    fq_dbg("change to %dKhz\n", ret / 1000);

    if (ret < 0) {
        ret = -ENODATA;
        fq_dbg("wmt_cpufreq: auto_pll_divisor failed\n");
    }
    ret = 0;
    cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

out:
    fq_dbg("cpu freq scaled to %dMhz now\n\n", wmt_getspeed(0) / 1000);
    return ret;
}

static int wmt_verify_speed(struct cpufreq_policy *policy)
{
    int ret = 0;
    struct cpufreq_frequency_table *freq_tbl = wmt_dvfs_drvdata.freq_table;

    if (policy->cpu)
        return -EINVAL;

    if (NULL == freq_tbl)
        ret = -EINVAL;
    else
        ret = cpufreq_frequency_table_verify(policy, freq_tbl);

    return ret;
}

static int __init wmt_cpu_init(struct cpufreq_policy *policy)
{
    int ret = 0;
    struct cpufreq_frequency_table *wmt_freq_tbl = NULL;
    
    if (policy->cpu)
		return -EINVAL;

    policy->cur = wmt_getspeed(policy->cpu);
    policy->min = wmt_getspeed(policy->cpu);
    policy->max = wmt_getspeed(policy->cpu);
    
    /* generate cpu frequency table here */
    wmt_init_cpufreq_table(&wmt_dvfs_drvdata);
    wmt_freq_tbl = wmt_dvfs_drvdata.freq_table;
    if (NULL == wmt_freq_tbl) {
        printk(KERN_ERR "wmt_cpufreq create wmt_freq_tbl failed\n");
        return -EINVAL;
    }

    /* 
     * check each frequency and find max_freq 
     * min_freq in the table, then set:
     *  policy->min = policy->cpuinfo.min_freq = min_freq;
     *  policy->max = policy->cpuinfo.max_freq = max_freq; 
     */
    ret = cpufreq_frequency_table_cpuinfo(policy, wmt_freq_tbl);
    if (0 == ret)
        cpufreq_frequency_table_get_attr(wmt_freq_tbl, policy->cpu);

    policy->cur = wmt_getspeed(policy->cpu);
    policy->cpuinfo.transition_latency = wmt_dvfs_drvdata.sample_rate;
    
    fq_dbg("p->max:%d, p->min:%d, c->max=%d, c->min=%d, current:%d\n",
            policy->max, policy->min, policy->cpuinfo.max_freq, 
            policy->cpuinfo.min_freq, wmt_getspeed(0)); 
		gMaxCpuFreq = policy->max; //added by rubbitxiao
    return ret;
}

static int wmt_cpu_exit(struct cpufreq_policy *policy)
{
    wmt_exit_cpufreq_table(&wmt_dvfs_drvdata);
    memset(&wmt_dvfs_drvdata, sizeof(wmt_dvfs_drvdata), 0x00);
    cpufreq_frequency_table_put_attr(policy->cpu);

    return 0;
}

static struct freq_attr *wmt_cpufreq_attr[] = {
        &cpufreq_freq_attr_scaling_available_freqs,
        NULL,
};

static struct cpufreq_driver wmt_cpufreq_driver = {
    .name       = "wmt_cpufreq",
    .owner      = THIS_MODULE,
    .flags		= CPUFREQ_STICKY,
    .init       = wmt_cpu_init,
    .verify     = wmt_verify_speed,
    .target     = wmt_target,
    .get        = wmt_getspeed,
    .exit       = wmt_cpu_exit,
    .attr       = wmt_cpufreq_attr,
};

static int __init wmt_cpufreq_check_env(void)
{
    int i = 0;
    int ret = 0;
    int varlen = 512;
    char *ptr = NULL;
    unsigned int tbl_num = 0;
    unsigned int drv_en = 0;
    unsigned int sample_rate = 0;
    unsigned int freq = 0;
    unsigned int voltage = 0;
    struct wmt_dvfs_table *wmt_dvfs_tbl = NULL;
    unsigned char buf[512] = {0};

    /* uboot env name is: wmt.cpufreq.param/wmt.dvfs.param, format is:
        <Enable>:<Sample rate>:<Table_Number>:<Freq_Table>:<Voltage_Table> 
        Example:
        1:100000:4:[200000:1000][400000:1100][600000:1200][800000:1300] */
		/*1:100000:2:[1500000:1300][1500000:1300]*/
    ret = wmt_getsyspara("wmt.misc.param", buf, &varlen);                                                         
    if (ret) {
        printk(KERN_INFO "Can not find uboot env wmt.misc.param\n");
       strcpy(buf, "1:100000:2:[1200000:1300][1200000:1300]"); 
       ret = 0;
    }
//	strcpy(buf, "1:100000:2:[1200000:1300][1200000:1300]");
    fq_dbg("wmt.cpufreq.param:%s\n", buf);

    sscanf(buf, "%d:%d:%d", &drv_en, &sample_rate, &tbl_num);
    if (!drv_en) {
        printk(KERN_INFO "wmt cpufreq disaled\n");
        ret = -ENODEV;
        goto out;
    }

    /* 2 dvfs table at least */
    if (tbl_num < 2) {
        printk(KERN_INFO "No frequency information found\n");
        ret = -ENODATA;
        goto out;
    }
    if (tbl_num > DVFS_TABLE_NUM_MAX)
        tbl_num = DVFS_TABLE_NUM_MAX;

    wmt_dvfs_tbl = kzalloc(sizeof(struct wmt_dvfs_table) * tbl_num, GFP_KERNEL);
    if (!wmt_dvfs_tbl) {
        ret = -ENOMEM;
        goto out;
    }
    wmt_dvfs_drvdata.tbl_num = tbl_num;
    wmt_dvfs_drvdata.sample_rate = sample_rate;
    wmt_dvfs_drvdata.dvfs_table = wmt_dvfs_tbl;
    INIT_LIST_HEAD(&wmt_dvfs_drvdata.wmt_dvfs_list);

    /* copy freq&vol info from uboot env to wmt_dvfs_table */
    ptr = buf;
    for (i = 0; i < tbl_num; i++) {
        strsep(&ptr, "[");
        sscanf(ptr, "%d:%d]:[", &freq, &voltage);
        wmt_dvfs_tbl[i].freq = freq;
        wmt_dvfs_tbl[i].vol  = voltage;
        wmt_dvfs_tbl[i].index = i;
        INIT_LIST_HEAD(&wmt_dvfs_tbl[i].node);
        list_add_tail(&wmt_dvfs_tbl[i].node, &wmt_dvfs_drvdata.wmt_dvfs_list);
        fq_dbg("dvfs_table[%d]: freq %dKhz, voltage %dmV\n", i, freq, voltage);
    }

out:
    return ret;
}

static int __init wmt_cpufreq_driver_init(void)
{
    int ret = 0;

    /* if cpufreq disabled, cpu will always run at current frequency
     * which defined in wmt.plla.param */
    ret = wmt_cpufreq_check_env();
    if (ret) {
        //printk(KERN_WARNING "wmt_cpufreq check env failed, current cpu "
        //                            "frequency is %dKhz\n", wmt_getspeed(0));
        printk(KERN_WARNING "wmt_cpufreq check env failed\n");        
        goto out;
    }
    ret = cpufreq_register_driver(&wmt_cpufreq_driver);

    wmt_cpu_dev_setup();

out:
    return ret;
}
late_initcall(wmt_cpufreq_driver_init);

static void __exit wmt_cpufreq_driver_exit(void)
{
    wmt_cpu_dev_cleanup();
    cpufreq_unregister_driver(&wmt_cpufreq_driver);
}
module_exit(wmt_cpufreq_driver_exit);

MODULE_AUTHOR("WonderMedia Technologies, Inc");
MODULE_DESCRIPTION("WMT CPU frequency driver");
MODULE_LICENSE("Dual BSD/GPL");
