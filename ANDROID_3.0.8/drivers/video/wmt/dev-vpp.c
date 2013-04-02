/*++ 
 * linux/drivers/video/wmt/dev-vpp.c
 * WonderMedia video post processor (VPP) driver
 *
 * Copyright c 2012  WonderMedia  Technologies, Inc.
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
--*/

#define DEV_VPP_C

// #include <fcntl.h>
// #include <unistd.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/fb.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <asm/page.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/sysctl.h>
#include <linux/delay.h>
#include <linux/netlink.h>
#include <net/sock.h>

#undef DEBUG
//#define DEBUG
//#define DEBUG_DETAIL
#include "vpp.h"

#define VPP_PROC_NUM		10
#define VPP_DISP_FB_MAX		10

typedef struct {
	int (*func)(void *arg);
	void *arg;
	struct list_head list;
	vpp_int_t type;
	struct semaphore sem;
} vpp_proc_t;

typedef struct {
	vpp_dispfb_t parm;
	vpp_pts_t pts;
	struct list_head list;
} vpp_dispfb_parm_t;

struct list_head vpp_disp_fb_list;
struct list_head vpp_disp_free_list;
struct list_head vpp_vpu_fb_list;
vpp_dispfb_parm_t vpp_disp_fb_array[VPP_DISP_FB_MAX];

typedef struct {
	struct list_head list;
	struct tasklet_struct tasklet;
	void (*proc)(int arg);
} vpp_irqproc_t;

vpp_irqproc_t *vpp_irqproc_array[32];
struct list_head vpp_free_list;
vpp_proc_t vpp_proc_array[VPP_PROC_NUM];

extern void vpp_init(void);
extern void vpp_config(vout_info_t *info);

extern struct fb_var_screeninfo vfb_var;
extern struct fb_var_screeninfo gefb_var;
int vpp_disp_fb_cnt(struct list_head *list);
void vpp_set_path(int arg,vdo_framebuf_t *fb);

static unsigned int vpp_cur_dispfb_y_addr;
static unsigned int vpp_cur_dispfb_c_addr;
static unsigned int vpp_pre_dispfb_y_addr;
static unsigned int vpp_pre_dispfb_c_addr;

#ifdef CONFIG_HW_VPU_HALT_GOVW_TG_ERR
vpp_path_t vpp_govm_path;
int vpp_govw_tg_err_parm;
int vpp_govw_tg_err_parm_cnt = 0;
#endif

#ifdef CONFIG_SCL_DIRECT_PATH
#define VPP_SCL_MB_MAX	3
#define VPP_SCL_FB_MAX	5
int vpp_scl_dirpath;
struct list_head vpp_scl_fb_list;
struct list_head vpp_scl_free_list;
vpp_dispfb_parm_t vpp_scl_fb_array[VPP_SCL_FB_MAX];
extern int scl_scale_complete;
#endif

#define CONFIG_VPP_NOTIFY
#ifdef CONFIG_VPP_NOTIFY
#define VPP_NETLINK_PROC_MAX		2

typedef struct {
	__u32 pid;
	rwlock_t lock;
} vpp_netlink_proc_t;

vpp_netlink_proc_t vpp_netlink_proc[VPP_NETLINK_PROC_MAX];

static struct sock *vpp_nlfd;
int vpp_mv_debug,vpp_mv_buf = 2;

/* netlink receive routine */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
static DECLARE_MUTEX(vpp_netlink_receive_sem);
#else
static DEFINE_SEMAPHORE(vpp_netlink_receive_sem);
#endif
#endif

typedef enum {
	VPP_FB_PATH_SCL,
	VPP_FB_PATH_VPU,
	VPP_FB_PATH_DIRECT_GOVR,
	VPP_FB_PATH_GOVR,
	VPP_FB_PATH_PIP,
	VPP_FB_PATH_MAX
} vpp_fb_path_t;

/*----------------------- VPP API in Linux Kernel --------------------------------------*/
#ifdef CONFIG_VPP_NOTIFY
vpp_netlink_proc_t *vpp_netlink_get_proc(int no)
{
	if( no == 0 ) 
		return 0;
	if( no > VPP_NETLINK_PROC_MAX ) 
		return 0;
	return &vpp_netlink_proc[no-1];
}

static void vpp_netlink_receive(struct sk_buff *skb)
{
	struct nlmsghdr * nlh = NULL;
	vpp_netlink_proc_t *proc;

	if(down_trylock(&vpp_netlink_receive_sem))  
		return;

	if(skb->len >= sizeof(struct nlmsghdr)){
		nlh = nlmsg_hdr(skb);
		if((nlh->nlmsg_len >= sizeof(struct nlmsghdr))
			&& (skb->len >= nlh->nlmsg_len)){
			if((proc = vpp_netlink_get_proc(nlh->nlmsg_type))){
				write_lock_bh(&proc->lock);
				proc->pid =nlh->nlmsg_pid;
				write_unlock_bh(&proc->lock);
				DPRINT("[VPP] rx user pid 0x%x\n",proc->pid);
			}
		}
	}
	up(&vpp_netlink_receive_sem);         
}

void vpp_netlink_notify(int no,int cmd,int arg)
{
	int ret;
	int size;
	unsigned char *old_tail;
   	struct sk_buff *skb;
   	struct nlmsghdr *nlh;
	vpp_netlink_proc_t *proc;

	MSG("[VPP] netlink notify %d,cmd %d,0x%x\n",no,cmd,arg);

	if( !(proc = vpp_netlink_get_proc(no)) )
		return;

	switch(cmd){
		case DEVICE_RX_DATA:
			size = NLMSG_SPACE(sizeof(struct wmt_cec_msg));  
			break;
		case DEVICE_PLUG_IN:
		case DEVICE_PLUG_OUT:
			size = NLMSG_SPACE(sizeof(struct wmt_cec_msg));  			
			break;
		default:
			return;
	}

	skb = alloc_skb(size, GFP_ATOMIC);
	if(skb==NULL) 
		return;
	old_tail = skb->tail;     
	nlh = NLMSG_PUT(skb, 0, 0, 0, size-sizeof(*nlh)); 
	nlh->nlmsg_len = skb->tail - old_tail;

	switch(cmd){
		case DEVICE_RX_DATA:
			nlh->nlmsg_type = DEVICE_RX_DATA;
			memcpy(NLMSG_DATA(nlh),(struct wmt_cec_msg *)arg,sizeof(struct wmt_cec_msg));
			break;
		case DEVICE_PLUG_IN:
		case DEVICE_PLUG_OUT:
			{
				static int cnt;
				struct wmt_cec_msg *msg;

				msg = (struct wmt_cec_msg *)NLMSG_DATA(nlh);
				msg->msgdata[0] = cnt;
				cnt++;
			}
			if( arg ){
				nlh->nlmsg_type = DEVICE_PLUG_IN;
				nlh->nlmsg_flags = edid_get_hdmi_phy_addr();
			}
			else {
				nlh->nlmsg_type = DEVICE_PLUG_OUT;
			}
			size = NLMSG_SPACE(sizeof(0));  			
			break;
		default:
			return;
	}
	
	NETLINK_CB(skb).pid = 0;
	NETLINK_CB(skb).dst_group = 0;

   	if(proc->pid!=0){
   		ret = netlink_unicast(vpp_nlfd, skb, proc->pid, MSG_DONTWAIT);
		return;
	}
nlmsg_failure: //NLMSG_PUT go to
	if(skb!=NULL)
		kfree_skb(skb);
   	return;
}

#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
static DECLARE_MUTEX(vpp_sem);
static DECLARE_MUTEX(vpp_sem2);
#else
static DEFINE_SEMAPHORE(vpp_sem);
static DEFINE_SEMAPHORE(vpp_sem2);
#endif
void vpp_set_mutex(int idx,int lock)
{
	struct semaphore *sem;

	sem = ((g_vpp.dual_display==0) || (idx==0))? &vpp_sem:&vpp_sem2;
	if( lock )
		down(sem);
	else
		up(sem);
}

#define VPP_DEBUG_FUNC
#ifdef VPP_DEBUG_FUNC
#define VPP_DBG_TMR_NUM		3
//#define VPP_DBG_DIAG_NUM	100
#ifdef VPP_DBG_DIAG_NUM
char vpp_dbg_diag_str[VPP_DBG_DIAG_NUM][100];
int vpp_dbg_diag_index;
int vpp_dbg_diag_delay;
#endif

void vpp_dbg_show(int level,int tmr,char *str)
{
	static struct timeval pre_tv[VPP_DBG_TMR_NUM];
	struct timeval tv;
	unsigned int tm_usec = 0;

	if( vpp_check_dbg_level(level)==0 )
		return;

	if( tmr && (tmr <= VPP_DBG_TMR_NUM) ){
		do_gettimeofday(&tv);
		if( pre_tv[tmr-1].tv_sec ){
			tm_usec = ( tv.tv_sec == pre_tv[tmr-1].tv_sec )? (tv.tv_usec - pre_tv[tmr-1].tv_usec):(1000000 + tv.tv_usec - pre_tv[tmr-1].tv_usec);
		}
		pre_tv[tmr-1] = tv;
	}

#ifdef VPP_DBG_DIAG_NUM
	if( level == VPP_DBGLVL_DIAG ){
		if( str ){
			char *ptr = &vpp_dbg_diag_str[vpp_dbg_diag_index][0];
			sprintf(ptr,"%s (%d,%d)(T%d %d usec)",str,(int)tv.tv_sec,(int)tv.tv_usec,tmr,(int) tm_usec);
			vpp_dbg_diag_index = (vpp_dbg_diag_index + 1) % VPP_DBG_DIAG_NUM;
		}

		if( vpp_dbg_diag_delay ){
			vpp_dbg_diag_delay--;
			if( vpp_dbg_diag_delay == 0 ){
				int i;
				
				DPRINT("----- VPP DIAG -----\n");
				for(i=0;i<VPP_DBG_DIAG_NUM;i++){
					DPRINT("%02d : %s\n",i,&vpp_dbg_diag_str[vpp_dbg_diag_index][0]);
					vpp_dbg_diag_index = (vpp_dbg_diag_index + 1) % VPP_DBG_DIAG_NUM;				
				}
			}
		}
		return;
	}
#endif
	
	if( str ) {
		if( tmr ){
			DPRINT("[VPP] %s (T%d period %d usec)\n",str,tmr-1,(int) tm_usec);
		}
		else {
			DPRINT("[VPP] %s\n",str);
		}
	}
} /* End of vpp_dbg_show */

static void vpp_dbg_show_val1(int level,int tmr,char *str,int val)
{
	if( vpp_check_dbg_level(level) ){
		char buf[50];

		sprintf(buf,"%s 0x%x",str,val);
		vpp_dbg_show(level,tmr,buf);
	}
}

static DECLARE_WAIT_QUEUE_HEAD(vpp_dbg_wq);
int vpp_dbg_wait_flag;
void vpp_dbg_wait(char *str)
{
	DPRINT("[VPP] vpp_dbg_wait(%s)\n",str);
	wait_event_interruptible(vpp_dbg_wq, (vpp_dbg_wait_flag));
	vpp_dbg_wait_flag = 0;
	DPRINT("[VPP] Exit vpp_dbg_wait\n");
}

int vpp_dbg_get_period_usec(vpp_dbg_period_t *p,int cmd)
{
	static struct timeval pre_tv;
	struct timeval tv;
	unsigned int tm_usec = 0;

	do_gettimeofday(&tv);
	if( pre_tv.tv_sec ){
		tm_usec = ( tv.tv_sec == pre_tv.tv_sec )? (tv.tv_usec - pre_tv.tv_usec):(1000000 + tv.tv_usec - pre_tv.tv_usec);
	}
	pre_tv = tv;
	if( p ){
		if( cmd == 0 ){
			p->index = 0;
			memset(&p->period_us,0,VPP_DBG_PERIOD_NUM);
		}
		else {
			p->period_us[p->index] = tm_usec;
		}
		p->index++;
		if( cmd == 2 ){
			int i,sum = 0;
			
			DPRINT("[VPP] period");
			for(i=0;i<VPP_DBG_PERIOD_NUM;i++){
				DPRINT(" %d",p->period_us[i]);
				sum += p->period_us[i];
			}
			DPRINT(",sum %d\n",sum);
		}
	}
	return tm_usec;
}
#else
void vpp_dbg_show(int level,int tmr,char *str){}
static void vpp_dbg_show_val1(int level,int tmr,char *str,int val){}
void vpp_dbg_wait(char *str){}
#endif

/*----------------------- Linux Kernel feature --------------------------------------*/
#ifdef CONFIG_PROC_FS
#define CONFIG_VPP_PROC
#ifdef CONFIG_VPP_PROC
unsigned int vpp_proc_value;
char vpp_proc_str[16];
static ctl_table vpp_table[];
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
static int vpp_do_proc(ctl_table * ctl,int write,void *buffer,size_t * len,loff_t *ppos)
#else
static int vpp_do_proc(ctl_table * ctl,int write,struct file *file,void *buffer,size_t * len,loff_t *ppos)
#endif
{	
	int ret;
	int ctl_name;
	
	ctl_name = (((int)ctl - (int)vpp_table) / sizeof(ctl_table)) + 1;
	if( !write ){
		switch( ctl_name ){
			case 1:
				vpp_proc_value = g_vpp.dbg_msg_level;
				break;
			case 4:
				vpp_proc_value = p_vpu->dei_mode;
				break;
			case 5:
				vpp_proc_value = g_vpp.disp_fb_max;
				break;
			case 6:
				vpp_proc_value = g_vpp.govw_skip_all;
				break;
			case 7:
				vpp_proc_value = g_vpp.video_quality_mode;
				break;
			case 8:
				vpp_proc_value = g_vpp.scale_keep_ratio;
				break;
			case 10:
				vpp_proc_value = p_vpu->scale_mode;
				break;
			case 11:
				vpp_proc_value = p_scl->scale_mode;
				break;
			case 13:
				vpp_proc_value = p_vpu->underrun_cnt;
				break;
			case 14:
				vpp_proc_value = g_vpp.vpu_skip_all;
				break;
			case 15:
				vpp_proc_value = g_vpp.hdmi_cp_enable;
				break;
			case 16:
				vpp_proc_value = vpp_get_base_clock(VPP_MOD_GOVRH);
				break;
			case 17:
				vpp_proc_value = g_vpp.hdmi_i2c_freq;
				break;
			case 18:
				vpp_proc_value = g_vpp.hdmi_i2c_udelay;
				break;
#ifdef VPP_DEBUG_FUNC
			case 19:
				vpp_proc_value = vpp_dbg_wait_flag;
				break;
#endif
			case 22:
				vpp_proc_value = p_scl->filter_mode;
				break;
			case 23:
				vpp_proc_value = g_vpp.dbg_flag;
				break;
			case 24:
				vpp_proc_value = g_vpp.hdmi_3d_type;
				break;
			case 25:
				vpp_proc_value = g_vpp.hdmi_certify_flag;
				break;
			default:
				break;
		}
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
	ret = proc_dointvec(ctl, write, buffer, len, ppos);
#else
	ret = proc_dointvec(ctl, write, file, buffer, len, ppos);
#endif
	if( write ){
		switch( ctl_name ){
			case 1:
				DPRINT("---------- VPP debug level ----------\n");
				DPRINT("0-disable,255-show all\n");
				DPRINT("1-scale,2-disp fb,3-interrupt,4-TG\n");
				DPRINT("5-ioctl,6-diag,7-deinterlace\n");
				DPRINT("-------------------------------------\n");
				g_vpp.dbg_msg_level = vpp_proc_value;
				break;
#ifdef CONFIG_LCD_WMT			
			case 2:
				lcd_blt_set_level(lcd_blt_id,lcd_blt_level);
				break;
			case 3:
				lcd_blt_set_freq(lcd_blt_id,lcd_blt_freq);
				break;
#endif			
#ifdef WMT_FTBLK_VPU
			case 4:
				p_vpu->dei_mode = vpp_proc_value;
				vpu_dei_set_mode(p_vpu->dei_mode);
				break;
#endif
			case 5:
				g_vpp.disp_fb_max = vpp_proc_value;
				break;
			case 6:
				g_vpp.govw_skip_all = vpp_proc_value;
				break;
			case 7:
				g_vpp.video_quality_mode = vpp_proc_value;
				vpp_set_video_quality(g_vpp.video_quality_mode);
				break;
			case 8:
				g_vpp.scale_keep_ratio = vpp_proc_value;
				break;
#ifdef CONFIG_WMT_EDID
			case 9:
				{
					vout_t *vo;

					vo = vout_get_entry_adapter(vpp_proc_value);
					if( (vo->inf) && (vo->inf->get_edid) ){
						vo->status &= ~VPP_VOUT_STS_EDID;
						if( vout_get_edid(vo->num) ){
							int i;
							
							vo->edid_info.option = 0;
							edid_dump(vo->edid);
							for(i=1;i<=vo->edid[126];i++){
								edid_dump(vo->edid+128*i);
							}
							if( edid_parse(vo->edid,&vo->edid_info) ){
							}
							else {
								DBG_ERR("parse EDID\n");
							}
						}
						else {
							DBG_ERR("read EDID\n");
						}
					}
				}
				break;
#endif
			case 10:
			case 11:
				DPRINT("---------- scale mode ----------\n");
				DPRINT("0-recursive normal\n");
				DPRINT("1-recursive sw bilinear\n");
				DPRINT("2-recursive hw bilinear\n");
				DPRINT("3-realtime noraml (quality but x/32 limit)\n");
				DPRINT("4-realtime bilinear (fit edge but skip line)\n");
				DPRINT("-------------------------------------\n");
				if( ctl_name == 15 ){
					p_vpu->scale_mode = vpp_proc_value;
				}
				else {
					p_scl->scale_mode = vpp_proc_value;
				}
				break;
			case 13:
				p_vpu->underrun_cnt = vpp_proc_value;
				g_vpp.vpu_skip_all = 0;
				g_vpp.govw_skip_all = 0;
				break;
			case 14:
				g_vpp.vpu_skip_all = vpp_proc_value;
				break;
			case 15:
				{
#ifdef WMT_FTBLK_HDMI
				hdmi_set_cp_enable(vpp_proc_value);
#endif				
				g_vpp.hdmi_cp_enable = vpp_proc_value;
				}
				break;
			case 16:
				govrh_set_clock(p_govrh,vpp_proc_value);
				DPRINT("[HDMI] set pixclk %d\n",vpp_proc_value);
				break;
			case 17:
				g_vpp.hdmi_i2c_freq	= vpp_proc_value;
				break;
			case 18:
				g_vpp.hdmi_i2c_udelay = vpp_proc_value;
				break;
#ifdef VPP_DEBUG_FUNC
			case 19:
				vpp_dbg_wait_flag = vpp_proc_value;
				wake_up(&vpp_dbg_wq);
				break;
#endif
			case 22:
				p_scl->filter_mode = vpp_proc_value;
				break;
			case 23:
				g_vpp.dbg_flag = vpp_proc_value;
				break;
			case 24:
				g_vpp.hdmi_3d_type = vpp_proc_value;
#ifdef CONFIG_WMT_HDMI
				hdmi_tx_vendor_specific_infoframe_packet();
#endif
				break;
			case 25:
				g_vpp.hdmi_certify_flag = vpp_proc_value;
				break;
			case 27:
#ifdef CONFIG_VPP_MOTION_VECTOR
				DPRINT("---------- MV duplicate ----------\n");
				DPRINT("0-no dup\n");
				DPRINT("1-top + bottom\n");
				DPRINT("2-top dup\n");
				DPRINT("3-bottom dup\n");
				DPRINT("4-test pattern\n");
				DPRINT("5-top & bottom\n");
				DPRINT("6-top | bottom\n");
				DPRINT("-------------------------------------\n");
#endif
				break;
			default:
				break;
		}
	}	
	return ret;
}

	struct proc_dir_entry *vpp_proc_dir = 0;

	static ctl_table vpp_table[] = {
	    {
//			.ctl_name	= 1,
			.procname	= "dbg_msg",
			.data		= &vpp_proc_value,
			.maxlen		= sizeof(int),
			.mode		= 0666,
			.proc_handler = &vpp_do_proc,
		},
	    {
//			.ctl_name	= 2,
			.procname	= "lcd_blt_level",
			.data		= &lcd_blt_level,
			.maxlen		= sizeof(int),
			.mode		= 0666,
			.proc_handler = &vpp_do_proc,
		},
	    {
//			.ctl_name	= 3,
			.procname	= "lcd_blt_freq",
			.data		= &lcd_blt_freq,
			.maxlen		= sizeof(int),
			.mode		= 0666,
			.proc_handler = &vpp_do_proc,
		},
		{
//			.ctl_name 	= 4,
			.procname	= "dei_mode",
			.data		= &vpp_proc_value,
			.maxlen		= sizeof(int),
			.mode		= 0666,
			.proc_handler = &vpp_do_proc,
		},
		{
//			.ctl_name 	= 5,
			.procname	= "disp_fb_max",
			.data		= &vpp_proc_value,
			.maxlen		= sizeof(int),
			.mode		= 0666,
			.proc_handler = &vpp_do_proc,
		},
		{
//			.ctl_name 	= 6,
			.procname	= "govw_skip_all",
			.data		= &vpp_proc_value,
			.maxlen		= sizeof(int),
			.mode		= 0666,
			.proc_handler = &vpp_do_proc,
		},
		{
//			.ctl_name 	= 7,
			.procname	= "video_quality_mode",
			.data		= &vpp_proc_value,
			.maxlen		= sizeof(int),
			.mode		= 0666,
			.proc_handler = &vpp_do_proc,
		},
		{
//			.ctl_name 	= 8,
			.procname	= "scale_keep_ratio",
			.data		= &vpp_proc_value,
			.maxlen		= sizeof(int),
			.mode		= 0666,
			.proc_handler = &vpp_do_proc,
		},
		{
//			.ctl_name 	= 9,
			.procname	= "vout_edid",
			.data		= &vpp_proc_value,
			.maxlen		= sizeof(int),
			.mode		= 0666,
			.proc_handler = &vpp_do_proc,
		},
		{
//			.ctl_name 	= 10,
			.procname	= "vpu_scale_mode",
			.data		= &vpp_proc_value,
			.maxlen		= sizeof(int),
			.mode		= 0666,
			.proc_handler = &vpp_do_proc,
		},
		{
//			.ctl_name 	= 11,
			.procname	= "scl_scale_mode",
			.data		= &vpp_proc_value,
			.maxlen		= sizeof(int),
			.mode		= 0666,
			.proc_handler = &vpp_do_proc,
		},
		{
//			.ctl_name 	= 12,
			.procname	= "vpu_err_skip",
			.data		= &vpp_proc_value,
			.maxlen		= sizeof(int),
			.mode		= 0666,
			.proc_handler = &vpp_do_proc,
		},
		{
//			.ctl_name 	= 13,
			.procname	= "vpu_underrun_cnt",
			.data		= &vpp_proc_value,
			.maxlen		= sizeof(int),
			.mode		= 0666,
			.proc_handler = &vpp_do_proc,
		},
		{
//			.ctl_name 	= 14,
			.procname	= "vpu_skip_all",
			.data		= &vpp_proc_value,
			.maxlen		= sizeof(int),
			.mode		= 0666,
			.proc_handler = &vpp_do_proc,
		},
		{
//			.ctl_name 	= 15,
			.procname	= "hdmi_cp_enable",
			.data		= &vpp_proc_value,
			.maxlen		= sizeof(int),
			.mode		= 0666,
			.proc_handler = &vpp_do_proc,
		},
		{
//			.ctl_name 	= 16,
			.procname	= "pixel_clock",
			.data		= &vpp_proc_value,
			.maxlen		= sizeof(int),
			.mode		= 0666,
			.proc_handler = &vpp_do_proc,
		},
		{
//			.ctl_name 	= 17,
			.procname	= "hdmi_i2c_freq",
			.data		= &vpp_proc_value,
			.maxlen		= sizeof(int),
			.mode		= 0666,
			.proc_handler = &vpp_do_proc,
		},
		{
//			.ctl_name 	= 18,
			.procname	= "hdmi_i2c_udelay",
			.data		= &vpp_proc_value,
			.maxlen		= sizeof(int),
			.mode		= 0666,
			.proc_handler = &vpp_do_proc,
		},
		{
//			.ctl_name 	= 19,
			.procname	= "dbg_wait",
			.data		= &vpp_proc_value,
			.maxlen		= sizeof(int),
			.mode		= 0666,
			.proc_handler = &vpp_do_proc,
		},
		{
//			.ctl_name 	= 20,
			.procname	= "vo_mode",
			.data		= vpp_proc_str,
			.maxlen		= 12,
			.mode		= 0666,
			.proc_handler = &proc_dostring,
		},
		{
//			.ctl_name 	= 21,
			.procname	= "edid_msg",
			.data		= &edid_msg_enable,
			.maxlen		= sizeof(int),
			.mode		= 0666,
			.proc_handler = &vpp_do_proc,
		},
		{
//			.ctl_name 	= 22,
			.procname	= "scl_filter",
			.data		= &vpp_proc_value,
			.maxlen		= sizeof(int),
			.mode		= 0666,
			.proc_handler = &vpp_do_proc,
		},
		{
//			.ctl_name 	= 23,
			.procname	= "vpp_debug",
			.data		= &vpp_proc_value,
			.maxlen		= sizeof(int),
			.mode		= 0666,
			.proc_handler = &vpp_do_proc,
		},
		{
//			.ctl_name 	= 24,
			.procname	= "hdmi_3d",
			.data		= &vpp_proc_value,
			.maxlen		= sizeof(int),
			.mode		= 0666,
			.proc_handler = &vpp_do_proc,
		},
		{
//			.ctl_name 	= 25,
			.procname	= "hdmi_certify",
			.data		= &vpp_proc_value,
			.maxlen		= sizeof(int),
			.mode		= 0666,
			.proc_handler = &vpp_do_proc,
		},
		{
//			.ctl_name 	= 26
			.procname	= "mv_dbg",
			.data		= &vpp_mv_debug,
			.maxlen		= sizeof(int),
			.mode		= 0666,
			.proc_handler = &vpp_do_proc,
		},
		{
//			.ctl_name 	= 27
			.procname	= "mv_buf",
			.data		= &vpp_mv_buf,
			.maxlen		= sizeof(int),
			.mode		= 0666,
			.proc_handler = &vpp_do_proc,
		},
		{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
		.ctl_name = 0
#endif
		}
	};

	static ctl_table vpp_root_table[] = {
		{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
			.ctl_name	= CTL_DEV,
#endif
			.procname	= "vpp",	// create path ==> /proc/sys/vpp
			.mode		= 0555,
			.child 		= vpp_table
		},
		{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
		.ctl_name = 0 
#endif
		}
	};
	static struct ctl_table_header *vpp_table_header;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
static int vpp_sts_read_proc(char *buf, char **start, off_t offset, int len,int *eof,void *data)
#else
static int vpp_sts_read_proc(char *buf, char **start, off_t offset, int len)
#endif
{
	volatile struct govrh_regs *regs = (volatile struct govrh_regs *) REG_GOVRH_BASE1_BEGIN;
	unsigned int yaddr,caddr;
	char *p = buf;
	static struct timeval pre_tv;
	struct timeval tv;
	unsigned int tm_usec;
	unsigned int reg;
	
	p += sprintf(p, "--- VPP HW status ---\n");
#ifdef WMT_FTBLK_GOVRH	
	p += sprintf(p, "GOVRH memory read underrun error %d,cnt %d,cnt2 %d\n",(regs->interrupt.val & 0x200)?1:0,p_govrh->underrun_cnt,p_govrh2->underrun_cnt);
	p_govrh->clr_sts(VPP_INT_ALL);
#endif

#ifdef WMT_FTBLK_GOVW
	p += sprintf(p, "---------------------------------------\n");
	p += sprintf(p, "GOVW TG error %d,cnt %d\n",vppif_reg32_read(GOVW_INTSTS_TGERR),g_vpp.govw_tg_err_cnt);
	p += sprintf(p, "GOVW Y fifo overflow %d\n",vppif_reg32_read(GOVW_INTSTS_MIFYERR));
	p += sprintf(p, "GOVW C fifo overflow %d\n",vppif_reg32_read(GOVW_INTSTS_MIFCERR));
	p_govw->clr_sts(VPP_INT_ALL);
#endif

#ifdef WMT_FTBLK_GOVM
	p += sprintf(p, "---------------------------------------\n");
	p += sprintf(p, "GOVM VPU not ready %d,cnt %d\n",(vppif_reg32_read(GOVM_INTSTS_VPU_READY))?0:1,g_vpp.govw_vpu_not_ready_cnt);
	p += sprintf(p, "GOVM GE not ready %d,cnt %d\n",(vppif_reg32_read(GOVM_INTSTS_GE_READY))?0:1,g_vpp.govw_ge_not_ready_cnt);
	p += sprintf(p, "GE not ready G1 %d, G2 %d\n",vppif_reg32_read(GE1_BASE_ADDR+0xF4,BIT0,0),vppif_reg32_read(GE1_BASE_ADDR+0xF4,BIT1,1));
	REG32_VAL(GE1_BASE_ADDR+0xf4) |= 0x3;
	p_govm->clr_sts(VPP_INT_ALL);
#endif

#ifdef WMT_FTBLK_SCL	
	p += sprintf(p, "---------------------------------------\n");
	p += sprintf(p, "SCL TG error %d\n",vppif_reg32_read(SCL_INTSTS_TGERR));
	p += sprintf(p, "SCLR MIF1 read error %d\n",vppif_reg32_read(SCLR_INTSTS_R1MIFERR));
	p += sprintf(p, "SCLR MIF2 read error %d\n",vppif_reg32_read(SCLR_INTSTS_R2MIFERR));
	p += sprintf(p, "SCLW RGB fifo overflow %d\n",vppif_reg32_read(SCLW_INTSTS_MIFRGBERR));
	p += sprintf(p, "SCLW Y fifo overflow %d\n",vppif_reg32_read(SCLW_INTSTS_MIFYERR));
	p += sprintf(p, "SCLW C fifo overflow %d\n",vppif_reg32_read(SCLW_INTSTS_MIFCERR));
	p_scl->clr_sts(VPP_INT_ALL);
#endif

#ifdef WMT_FTBLK_VPU
	p += sprintf(p, "---------------------------------------\n");
	p += sprintf(p, "VPU TG error %d\n",vppif_reg32_read(VPU_INTSTS_TGERR));
	p += sprintf(p, "VPUR MIF1 read error %d\n",vppif_reg32_read(VPU_R_INTSTS_R1MIFERR));
	p += sprintf(p, "VPUR MIF2 read error %d\n",vppif_reg32_read(VPU_R2_MIF_ERR));	
	p += sprintf(p, "VPUW Y fifo overflow %d,cnt %d\n",vppif_reg32_read(VPU_W_MIF_YERR),g_vpp.vpu_y_err_cnt);
//	p += sprintf(p, "VPUW C fifo overflow %d,cnt %d\n",vppif_reg32_read(VPU_W_MIF_CERR),g_vpp.vpu_c_err_cnt);
	p += sprintf(p, "VPU scale underrun %d,cnt %d\n",vppif_reg32_read(VPU_W_MIF_CERR),g_vpp.vpu_c_err_cnt);
	p += sprintf(p, "VPUW RGB fifo overflow %d\n",vppif_reg32_read(VPU_W_MIF_RGBERR));
	p += sprintf(p, "VPU MVR fifo overflow %d\n",vppif_reg32_read(VPU_MVR_MIF_ERR));
	p_vpu->clr_sts(VPP_INT_ALL);
#endif

	if( REG32_VAL(GE3_BASE_ADDR+0x50) < vppif_reg32_read(GOVM_DISP_X_CR) ){
		p += sprintf(p, "*E* GE resx %d < GOV resx %d\n",REG32_VAL(GE3_BASE_ADDR+0x50),vppif_reg32_read(GOVM_DISP_X_CR));
	}
	if( REG32_VAL(GE3_BASE_ADDR+0x54) < vppif_reg32_read(GOVM_DISP_Y_CR) ){
		p += sprintf(p, "*E* GE resy %d < GOV resy %d\n",REG32_VAL(GE3_BASE_ADDR+0x54),vppif_reg32_read(GOVM_DISP_Y_CR));
	}

	p += sprintf(p, "---------------------------------------\n");
	p += sprintf(p, "(6A8.0)G1 Enable %d,(6AC.0)G2 Enable %d\n",REG32_VAL(GE3_BASE_ADDR+0xa8),REG32_VAL(GE3_BASE_ADDR+0xac));
	p += sprintf(p, "(880.0)GOVRH Enable %d,(900.0)TG %d\n",regs->mif.b.enable,regs->tg_enable.b.enable);
	p += sprintf(p, "(C84.8)GOVW Enable %d,(CA0.0)TG %d\n",vppif_reg32_read(GOVW_HD_MIF_ENABLE),vppif_reg32_read(GOVW_TG_ENABLE));
	p += sprintf(p, "(308.0)GOVM path GE %d,(30C.0) VPU %d\n",vppif_reg32_read(GOVM_GE_SOURCE),vppif_reg32_read(GOVM_VPU_SOURCE));	
	p += sprintf(p, "(1C0.0)VPUR1 Enable %d,(1C0.1)R2 Enable %d\n",vppif_reg32_read(VPU_R_MIF_ENABLE),vppif_reg32_read(VPU_R_MIF2_ENABLE));
	p += sprintf(p, "(1E0.0)VPUW Enable %d,(1A0.0)TG %d\n",vppif_reg32_read(VPU_W_MIF_EN),vppif_reg32_read(VPU_TG_ENABLE));	
	
	reg = REG32_VAL(PM_CTRL_BASE_ADDR+0x258);
	p += sprintf(p, "--- POWER CONTROL ---\n");
	p += sprintf(p, "0x%x = 0x%x\n",PM_CTRL_BASE_ADDR+0x258,reg);
	p += sprintf(p, "HDCP %d,VPU %d,VPP %d,SCL %d,HDMI I2C %d\n",(reg & BIT7)?1:0,(reg & BIT17)?1:0,(reg & BIT18)?1:0,(reg & BIT21)?1:0,(reg & BIT22)?1:0);
	p += sprintf(p, "HDMI %d,GOVW %d,GOVR %d,GE %d,DISP %d\n",(reg & BIT23)?1:0,(reg & BIT24)?1:0,(reg & BIT25)?1:0,(reg & BIT26)?1:0,(reg & BIT27)?1:0);
	p += sprintf(p, "DVO %d,HDMI OUT %d,SDTV %d\n",(reg & BIT29)?1:0,(reg & BIT30)?1:0,(reg & BIT31)?1:0);

	p += sprintf(p, "--- VPP fb Address ---\n");
	p += sprintf(p, "GOV mb 0x%x 0x%x\n",g_vpp.mb[0],g_vpp.mb[1]);	
#ifdef WMT_FTBLK_VPU
	vpu_r_get_fb_addr(&yaddr,&caddr);
	p += sprintf(p, "VPU fb addr Y(0x%x) 0x%x, C(0x%x) 0x%x\n",REG_VPU_R_Y1SA,yaddr,REG_VPU_R_C1SA,caddr);
#else
	sclr_get_fb_addr(&yaddr,&caddr);
	p += sprintf(p, "VPU fb addr Y(0x%x) 0x%x, C(0x%x) 0x%x\n",REG_SCLR_YSA,yaddr,REG_SCLR_CSA,caddr);
#endif

#ifdef WMT_FTBLK_GOVW
	govw_get_fb_addr(&yaddr,&caddr);
	p += sprintf(p, "GOVW fb addr Y(0x%x) 0x%x, C(0x%x) 0x%x\n",REG_GOVW_HD_YSA,yaddr,REG_GOVW_HD_CSA,caddr);	
#endif
#ifdef WMT_FTBLK_GOVRH
	govrh_get_fb_addr(p_govrh,&yaddr,&caddr);
	p += sprintf(p, "GOVRH fb addr Y(0x%x) 0x%x, C(0x%x) 0x%x\n",REG_GOVRH_YSA,yaddr,REG_GOVRH_CSA,caddr);
	govrh_get_fb_addr(p_govrh2,&yaddr,&caddr);
	p += sprintf(p, "GOVRH2 fb addr Y(0x%x) 0x%x, C(0x%x) 0x%x\n",REG_GOVRH2_YSA,yaddr,REG_GOVRH2_CSA,caddr);	
#endif
	p += sprintf(p, "--- VPP SW status ---\n");
	p += sprintf(p, "vpp path %s\n",vpp_vpath_str[g_vpp.vpp_path]);
	
	do_gettimeofday(&tv);
	tm_usec=(tv.tv_sec==pre_tv.tv_sec)? (tv.tv_usec-pre_tv.tv_usec):(1000000*(tv.tv_sec-pre_tv.tv_sec)+tv.tv_usec-pre_tv.tv_usec);
	p += sprintf(p, "Time period %d usec\n",(int) tm_usec);
	p += sprintf(p, "GOVR fps %d,GOVR2 fps %d\n",(1000000*p_govrh->vbis_cnt/tm_usec),(1000000*p_govrh2->vbis_cnt/tm_usec));
	p += sprintf(p, "GOVW fps %d\n",(1000000*g_vpp.govw_vbis_cnt/tm_usec));
	pre_tv = tv;
	
	p += sprintf(p, "GOVW VBIS INT cnt %d\n",g_vpp.govw_vbis_cnt);
	p += sprintf(p, "GOVW PVBI INT cnt %d (toggle dual buf)\n",g_vpp.govw_pvbi_cnt);
	p += sprintf(p, "GOVW TG ERR INT cnt %d\n",g_vpp.govw_tg_err_cnt);

	p += sprintf(p, "--- disp fb status ---\n");
	p += sprintf(p, "DISP fb isr cnt %d\n",g_vpp.disp_fb_isr_cnt);
	p += sprintf(p, "queue max %d,full cnt %d\n",g_vpp.disp_fb_max,g_vpp.disp_fb_full_cnt);
	p += sprintf(p, "VPU disp fb cnt %d, skip %d\n",g_vpp.disp_cnt,g_vpp.disp_skip_cnt);
	p += sprintf(p, "Queue cnt disp:%d\n",vpp_disp_fb_cnt(&vpp_disp_fb_list));
#ifdef CONFIG_HW_VPU_HALT_GOVW_TG_ERR
	p += sprintf(p, "GOVW TG err cnt %d,step %d\n",vpp_govw_tg_err_parm_cnt,vpp_govw_tg_err_parm);
#endif
	g_vpp.govw_vbis_cnt = 0;
	g_vpp.govw_pvbi_cnt = 0;
	g_vpp.disp_fb_isr_cnt = 0;
	g_vpp.disp_fb_full_cnt = 0;
	g_vpp.disp_cnt = 0;
	g_vpp.govw_tg_err_cnt = 0;
	g_vpp.disp_skip_cnt = 0;
	g_vpp.govw_vpu_not_ready_cnt = 0;
	g_vpp.govw_ge_not_ready_cnt = 0;
	g_vpp.vpu_y_err_cnt = 0;
	g_vpp.vpu_c_err_cnt = 0;
	p_govrh->underrun_cnt = 0;
	p_govrh2->underrun_cnt = 0;
	p_govrh->vbis_cnt = 0;
	p_govrh2->vbis_cnt = 0;

#ifdef CONFIG_SCL_DIRECT_PATH_DEBUG
	p += sprintf(p, "--- scl fb ---\n");
	p += sprintf(p, "fb cnt %d,Avg %d ms\n",g_vpp.scl_fb_cnt,(g_vpp.scl_fb_cnt)? (g_vpp.scl_fb_tmr/g_vpp.scl_fb_cnt):0);
	p += sprintf(p, "in queue %d,mb in use %d,over %d\n",vpp_disp_fb_cnt(&vpp_scl_fb_list),g_vpp.scl_fb_mb_used,g_vpp.scl_fb_mb_over);
	p += sprintf(p, "isr %d,on work %d,complete %d\n",g_vpp.scl_fb_isr,g_vpp.scl_fb_mb_on_work,scl_scale_complete);

	g_vpp.scl_fb_cnt = 0;
	g_vpp.scl_fb_tmr = 0;
	g_vpp.scl_fb_mb_over = 0;
#endif
#ifdef CONFIG_GOVW_SCL_PATH
	{
		vpp_fb_pool_t *pool;

		pool = &g_vpp.govw_fb_pool;
		p += sprintf(p, "--- govw fb pool ---\n");
		p += sprintf(p, "size %d,num %d,used 0x%x\n",pool->size,pool->num,pool->used);
		p += sprintf(p, "full %d\n",pool->full);
	}
#endif
	return (p - buf);
} /* End of vpp_sts_read_proc */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
static int vpp_reg_read_proc(char *buf, char **start, off_t offset, int len,int *eof,void *data)
#else
static int vpp_reg_read_proc(char *buf,char **start,off_t offset,int len)
#endif
{
	char *p = buf;
	vpp_mod_base_t *mod_p;
	int i;
	
	DPRINT("Product ID:0x%x\n",vpp_get_chipid());
	for(i=0;i<VPP_MOD_MAX;i++){
		mod_p = vpp_mod_get_base(i);
		if( mod_p && mod_p->dump_reg ){
			mod_p->dump_reg();
		}
	}
#ifdef 	WMT_FTBLK_HDMI
	hdmi_reg_dump();
#endif
#ifdef WMT_FTBLK_LVDS
	lvds_reg_dump();
#endif
	
	// p += sprintf(p, "Dump VPP HW register by kernel message\n");
	
	return (p-buf);
} /* End of vpp_reg_read_proc */

static char *vpp_show_module(vpp_mod_t mod,char *p)
{
	vpp_mod_base_t *mod_p;
	vpp_fb_base_t *fb_p;
	vdo_framebuf_t *fb;

	mod_p = vpp_mod_get_base(mod);
	p += sprintf(p, "int catch 0x%x\n",mod_p->int_catch);
	p += sprintf(p, "pm dev id %d,clk %d\n",mod_p->pm & 0xFF,(mod_p->pm & VPP_MOD_CLK_ON)? 1:0);
	
	fb_p = mod_p->fb_p;
	if( fb_p ){
		fb = &fb_p->fb;
		p += sprintf(p, "----- frame buffer -----\n");
		p += sprintf(p, "Y addr 0x%x, size %d\n",fb->y_addr,fb->y_size);
		p += sprintf(p, "C addr 0x%x, size %d\n",fb->c_addr,fb->c_size);
		p += sprintf(p, "W %d, H %d, FB W %d, H %d\n",fb->img_w,fb->img_h,fb->fb_w,fb->fb_h);
		p += sprintf(p, "bpp %d, color fmt %s\n",fb->bpp,vpp_colfmt_str[fb->col_fmt]);
		p += sprintf(p, "H crop %d, V crop %d\n",fb->h_crop,fb->v_crop);
		p += sprintf(p, "flag 0x%x\n",fb->flag);
		p += sprintf(p, "CSC mode %d,frame rate %d\n",fb_p->csc_mode,fb_p->framerate);
		p += sprintf(p, "media fmt %d,wait ready %d\n",fb_p->media_fmt,fb_p->wait_ready);
	}
	return p;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
static int vpp_info_read_proc(char *buf, char **start, off_t offset, int len,int *eof,void *data)
#else
static int vpp_info_read_proc(char *buf,char **start,off_t offset,int len)
#endif
{
	char *p = buf;

	p += sprintf(p, "========== VPP ==========\n");
	p += sprintf(p, "vpp path %s\n",vpp_vpath_str[g_vpp.vpp_path]);
	p += sprintf(p, "mb0 0x%x,mb1 0x%x\n",g_vpp.mb[0],g_vpp.mb[1]);

#ifdef WMT_FTBLK_GOVRH
	p += sprintf(p, "========== GOVRH ==========\n");
	p = vpp_show_module(VPP_MOD_GOVRH,p);

	p += sprintf(p, "========== GOVRH2 ==========\n");
	p = vpp_show_module(VPP_MOD_GOVRH2,p);
#endif

	p += sprintf(p, "========== GOVW ==========\n");
	p = vpp_show_module(VPP_MOD_GOVW,p);

	p += sprintf(p, "========== GOVM ==========\n");
	p += sprintf(p, "path 0x%x\n",p_govm->path);
	p = vpp_show_module(VPP_MOD_GOVM,p);

	p += sprintf(p, "========== VPU ==========\n");
	p += sprintf(p, "visual res (%d,%d),pos (%d,%d)\n",p_vpu->resx_visual,p_vpu->resy_visual,p_vpu->posx,p_vpu->posy);
	p = vpp_show_module(VPP_MOD_VPU,p);

	p += sprintf(p, "========== SCLR ==========\n");
	p = vpp_show_module(VPP_MOD_SCL,p);

	p += sprintf(p, "========== SCLW ==========\n");
	p = vpp_show_module(VPP_MOD_SCLW,p);
	return (p-buf);
} /* End of vpp_info_read_proc */
#endif

#if 0
struct timer_list vpp_config_timer;
void vpp_config_tmr(int status)
{
	vout_set_blank(VPP_VOUT_ALL,0);
//	DPRINT("[VPP] %s\n",__FUNCTION__);
}

void vpp_set_vout_enable_timer(void)
{
	init_timer(&vpp_config_timer);
	vpp_config_timer.data = 0;
	vpp_config_timer.function = (void *) vpp_config_tmr;
	vpp_config_timer.expires = jiffies + HZ / 2;
	add_timer(&vpp_config_timer);
}
#endif

#ifdef CONFIG_VPP_GOVRH_FBSYNC
void vpp_fbsync_cal_fps(void)
{
	unsigned int gcd,total,frame;
	int fps_in,fps_out;

	fps_in = p_govw->fb_p->framerate;
	fps_out = p_govrh->fb_p->framerate;

	gcd = vpp_get_gcd(fps_out,fps_in);
	total = fps_out / gcd;
	frame = fps_in / gcd;

	if( frame > total ) frame = total;
	g_vpp.fbsync_frame = frame;
	g_vpp.fbsync_step = total / frame;
	g_vpp.fbsync_substep = ( total % frame )? (frame / (total % frame)):0;
	if( g_vpp.fbsync_step == g_vpp.fbsync_substep ){
		g_vpp.fbsync_step = g_vpp.fbsync_step + 1;
		g_vpp.fbsync_substep = total - (g_vpp.fbsync_step * frame);
	}
	
	g_vpp.fbsync_cnt = 1;
	g_vpp.fbsync_vsync = g_vpp.fbsync_step;
	if( vpp_check_dbg_level(VPP_DBGLVL_SYNCFB) ){
		char buf[50];

		DPRINT("[VPP] govrh fps %d,govw fps %d,gcd %d\n",fps_out,fps_in,gcd);
		DPRINT("[VPP] total %d,frame %d\n",total,frame);
		sprintf(buf,"sync frame %d,step %d,sub %d",g_vpp.fbsync_frame,g_vpp.fbsync_step,g_vpp.fbsync_substep);
		vpp_dbg_show(VPP_DBGLVL_SYNCFB,3,buf);
	}
}

int vpp_frc_check(void)
{
	if( !g_vpp.fbsync_enable )
		return 0;

	g_vpp.fbsync_vsync_cnt++;
	g_vpp.fbsync_isrcnt++;
	if( g_vpp.fbsync_vsync_cnt < g_vpp.fbsync_vsync ){
		return 1;
	}
	
	g_vpp.fbsync_cnt++;
	g_vpp.fbsync_vsync_cnt=0;					
	if( g_vpp.fbsync_cnt > g_vpp.fbsync_frame ){
		g_vpp.fbsync_cnt = 1;
		g_vpp.fbsync_vsync = g_vpp.fbsync_step;
		g_vpp.fbsync_isrcnt = 0;
		if( g_vpp.fbsync_substep < 0 ){
			if( g_vpp.fbsync_cnt <= ( g_vpp.fbsync_substep * (-1))){
				g_vpp.fbsync_vsync -= 1;
			}
		}
	}
	else {
		g_vpp.fbsync_vsync = g_vpp.fbsync_step;
		if( g_vpp.fbsync_substep < 0 ){
			// if( g_vpp.fbsync_cnt > (g_vpp.fbsync_frame + g_vpp.fbsync_substep )){
			if( g_vpp.fbsync_cnt <= ( g_vpp.fbsync_substep * (-1))){
				g_vpp.fbsync_vsync -= 1;
			}
		}
		else if( g_vpp.fbsync_substep ){
			if( (g_vpp.fbsync_cnt % g_vpp.fbsync_substep) == 0 ){
				g_vpp.fbsync_vsync += 1;
			}
		}
	}
	
	if( vpp_check_dbg_level(VPP_DBGLVL_SYNCFB) ){
		char buf[50];

		sprintf(buf,"sync frame %d,sync cnt %d,vsync %d,isr %d",g_vpp.fbsync_frame,
			g_vpp.fbsync_cnt,g_vpp.fbsync_vsync,g_vpp.fbsync_isrcnt );
		vpp_dbg_show(VPP_DBGLVL_SYNCFB,3,buf);
	}
	return 0;
}
#endif

void vpp_dei_dynamic_detect(void)
{
#ifdef CONFIG_VPP_DYNAMIC_DEI
	#define VPP_DEI_CHECK_PERIOD	150

	if( vppif_reg32_read(VPU_DEI_ENABLE) && (p_vpu->dei_mode == VPP_DEI_DYNAMIC) ){
		static int dei_cnt = VPP_DEI_CHECK_PERIOD;
		static unsigned int weave_sum,bob_sum;
		static unsigned int pre_weave_sum,pre_bob_sum;
		unsigned int usum,vsum;
		static vpp_deinterlace_t dei_mode = 0;
		unsigned int weave_diff,bob_diff,cur_diff;
		
		switch( dei_cnt ){
			case 2:
				if( dei_mode != VPP_DEI_ADAPTIVE_ONE ){
					g_vpp.govw_skip_frame = 1;
				}
				vpu_dei_set_mode(VPP_DEI_ADAPTIVE_ONE);
				break;
			case 1:
				vpu_dei_get_sum(&weave_sum,&usum,&vsum);
				if( dei_mode != VPP_DEI_FIELD ){
					g_vpp.govw_skip_frame = 1;
				}
				vpu_dei_set_mode(VPP_DEI_FIELD);				
				break;
			case 0:
				vpu_dei_get_sum(&bob_sum,&usum,&vsum);
				if( (vpp_calculate_diff(bob_sum,pre_bob_sum)<100000) 
					&& (vpp_calculate_diff(weave_sum,pre_weave_sum)<100000)){
					dei_mode = VPP_DEI_WEAVE;
				}
				else {
					dei_mode = ( bob_sum > (2*weave_sum) )? VPP_DEI_FIELD:VPP_DEI_ADAPTIVE_ONE;
				}
				bob_diff = vpp_calculate_diff(bob_sum,pre_bob_sum);
				weave_diff = vpp_calculate_diff(weave_sum,pre_weave_sum);
				cur_diff = vpp_calculate_diff(weave_sum,bob_sum);
				pre_bob_sum = bob_sum;
				pre_weave_sum = weave_sum;
				vpu_dei_set_mode(dei_mode);
				dei_cnt = VPP_DEI_CHECK_PERIOD;
				if( vpp_check_dbg_level(VPP_DBGLVL_DEI) ){
					static vpp_deinterlace_t pre_mode = 0;
					DPRINT("[VPP] bob %d,weave %d,diff bob %d,weave %d,cur %d\n",bob_sum,weave_sum,bob_diff,weave_diff,cur_diff);
					if( pre_mode != dei_mode ){
						DPRINT("[VPP] dei mode %d -> %d\n",pre_mode,dei_mode);
						pre_mode = dei_mode;
					}
				}
				break;
			default:
				break;
		}
		dei_cnt--;
	}
#endif
}

#ifdef CONFIG_VPP_MOTION_VECTOR
void vpp_dei_mv_duplicate(vdo_framebuf_t *fb)
{
	unsigned int phy,vir;
	int mv_h;

	phy = fb->c_addr+(fb->c_addr - fb->y_addr)/2;
	vir = (unsigned int) mb_phys_to_virt((unsigned long)phy);
	mv_h = fb->img_h / 16;

#if 0	// convert MV byte to bit
{
	int i,j,k,mv_w;
	char *s,*d;

	mv_w = fb->img_w / 16;
	vpp_reg_dump(vir,mv_w*10);
	for(i=0;i<mv_h;i++){
		for(j=0;j<mv_w;j++){
			s = vir + mv_w * i + j;
			d = vir + VPU_MV_BUF_LEN * i + (j / 8);
			k = j % 8;
			if( k == 0 ) *d = 0;
			*d |= (*s != 0 )? (0x1 << k):0x0;
		}
	}
	//vpp_reg_dump(vir,mv_w);	
}
#endif
	
	switch( vpp_mv_buf ){
		case 0:
		default:
			break;
		case 1:	// top + bottom
			{
				unsigned int vir_tmp;
				int i;
				int size;

				size = VPU_MV_BUF_LEN * mv_h;
				vir_tmp = vir + size;
				memcpy((void *)vir_tmp,(void *)vir,size);
				for(i=0;i<(mv_h/2);i++){
					memcpy((void *)vir+VPU_MV_BUF_LEN*(2*i),(void *)vir_tmp+VPU_MV_BUF_LEN*i,VPU_MV_BUF_LEN);
					memcpy((void *)vir+VPU_MV_BUF_LEN*(2*i+1),(void *)vir_tmp+VPU_MV_BUF_LEN*i+(size/2),VPU_MV_BUF_LEN);
				}
			}
			break;
		case 2:	// duplicate top
			{
				int i;

				for(i=(mv_h/2)-1;i>=0;i--){
					memcpy((void *)vir+VPU_MV_BUF_LEN*(2*i),(void *)vir+VPU_MV_BUF_LEN*i,VPU_MV_BUF_LEN);
					memcpy((void *)vir+VPU_MV_BUF_LEN*(2*i+1),(void *)vir+VPU_MV_BUF_LEN*i,VPU_MV_BUF_LEN);
				}
			}
			break;
		case 3:	// duplicate bottom
			{
				unsigned int vir_tmp;
				int i;
				int size;

				size = VPU_MV_BUF_LEN * (fb->img_h/16);
				vir_tmp = vir + size;
				memcpy((void *)vir_tmp,(void *)vir,size);
				for(i=0;i<(mv_h/2);i++){
					memcpy((void *)vir+VPU_MV_BUF_LEN*(2*i),(void *)vir_tmp+VPU_MV_BUF_LEN*i+(size/2),VPU_MV_BUF_LEN);
					memcpy((void *)vir+VPU_MV_BUF_LEN*(2*i+1),(void *)vir_tmp+VPU_MV_BUF_LEN*i+(size/2),VPU_MV_BUF_LEN);
				}
			}
			break;
		case 4:	// debug pattern
			{
				int i,value;

				for(i=0;i<mv_h;i++){
					switch(i%3){
						case 0:
							value = 0xff;
							break;
						case 1:
							value = 0x55;
							break;
						case 2:
							value = 0xaa;
							break;
					}
					memset((void *)vir+VPU_MV_BUF_LEN*i,value,VPU_MV_BUF_LEN);
				}
			}
			break;
		case 5:	// top & bottom
			{
				unsigned int vir_tmp;
				int i,j,value;
				int size;
				char *p;

				size = VPU_MV_BUF_LEN * mv_h;
				vir_tmp = vir + size;
				memcpy((void *)vir_tmp,(void *)vir,size);
				// vpp_reg_dump(vir,size);
				for(i=0;i<(mv_h/2);i++){
					for(j=0;j<VPU_MV_BUF_LEN;j++){
						p = (char *)(vir_tmp + VPU_MV_BUF_LEN * i + j);
						value = *p & *(p+size/2);
						p = (char *)(vir + VPU_MV_BUF_LEN * 2 * i + j);
						*p = value;
						*(p+VPU_MV_BUF_LEN) = value;
					}
				}
				// vpp_reg_dump(vir,size);		
			}
			break;
		case 6:	// top | bottom
			{
				unsigned int vir_tmp;
				int i,j,value;
				int size;
				char *p;

				size = VPU_MV_BUF_LEN * mv_h;
				vir_tmp = vir + size;
				memcpy((void *)vir_tmp,(void *)vir,size);
				// vpp_reg_dump(vir,size);
				for(i=0;i<(mv_h/2);i++){
					for(j=0;j<VPU_MV_BUF_LEN;j++){
						p = (char *)(vir_tmp + VPU_MV_BUF_LEN * i + j);
						value = *p | *(p+size/2);
						p = (char *)(vir + VPU_MV_BUF_LEN * 2 * i + j);
						*p = value;
						*(p+VPU_MV_BUF_LEN) = value;
					}
				}
				// vpp_reg_dump(vir,size);		
			}
			break;
	}

#if 0
	{ // MV block always static for TV logo text
		char *p;

		p = vir + 6 * VPU_MV_BUF_LEN + 4;
		*p &= ~0x60;
	}
#endif
}

void vpp_dei_mv_analysis(vdo_framebuf_t *s,vdo_framebuf_t *d)
{
	char *mv_p;	
	int d_vir;
	int i,j,k;

	mv_p = mb_phys_to_virt(vppif_reg32_in(REG_MVR_YSA));
	d_vir = (int) mb_phys_to_virt(d->c_addr);
	for(i=0;i<(d->img_h/16);i++){
		for(j=0;j<(d->img_w/16);j++){
			k = j % 8;
			if( (mv_p[VPU_MV_BUF_LEN*i+j/8] & (0x1 << k)) == 0 ){
				char *c_p;
				int line;

				c_p = (char *)(d_vir + (i * 8 * d->fb_w) + (j * 16));
				for(line=0;line<8;line++){
					memset(c_p,0x00,16);
					c_p += d->fb_w;
				}
			}
		}
	}
}
#endif

void vpp_set_NA12_hiprio(int type)
{
	static int reg1,reg2;

	switch(type){
		case 0: // restore NA12 priority
			vppif_reg32_out(MEMORY_CTRL_V4_CFG_BASE_ADDR+0x8,reg1);
			vppif_reg32_out(MEMORY_CTRL_V4_CFG_BASE_ADDR+0xC,reg2);
			break;
		case 1:	// set NA12 to high priority
			reg1 = vppif_reg32_in(MEMORY_CTRL_V4_CFG_BASE_ADDR+0x8);
			reg2 = vppif_reg32_in(MEMORY_CTRL_V4_CFG_BASE_ADDR+0xC);
			vppif_reg32_out(MEMORY_CTRL_V4_CFG_BASE_ADDR+0x8,0x600000);
			vppif_reg32_out(MEMORY_CTRL_V4_CFG_BASE_ADDR+0xC,0x0ff00000);
			break;
		case 2:
			reg1 = vppif_reg32_in(MEMORY_CTRL_V4_CFG_BASE_ADDR+0x8);
			reg2 = vppif_reg32_in(MEMORY_CTRL_V4_CFG_BASE_ADDR+0xC);
			vppif_reg32_out(MEMORY_CTRL_V4_CFG_BASE_ADDR+0x8,0x20003f);
			vppif_reg32_out(MEMORY_CTRL_V4_CFG_BASE_ADDR+0xC,0x00ffff00);
			break;
		default:
			break;
	}
}

void vpp_fb_pool_init(vpp_fb_pool_t *pool,int size,int num)
{
	int i;
	unsigned long phys_addr;

	DBG_MSG("(%d,%d)\n",size,num);
	if( (pool->size == size) && (pool->num == num) ){
		pool->used = 0;
		return;
	}

	for(i=0;i<pool->num;i++){
		mb_free(pool->mb[i]);
	}
	
	pool->size = size;
	for(i=0;i<num;i++){
		if( (phys_addr = mb_alloc(size)) == 0 )
			break;
		pool->mb[i] = phys_addr;
		DBG_MSG("fb%d alloc 0x%x\n",i,pool->mb[i]);
	}
	pool->num = i;
	pool->used = 0;
}

unsigned int vpp_fb_pool_get(vpp_fb_pool_t *pool)
{
	int i;

	for( i=0; i<pool->num; i++ ){
		if( pool->used & (0x1 << i) ){
			continue;
		}
		pool->used |= (0x1 << i);
		break;
	}
	DBG_DETAIL("%d\n",i);	
	if( i >= pool->num ){
		pool->full++;
		return 0;
	}
	return pool->mb[i];
}

void vpp_fb_pool_put(vpp_fb_pool_t *pool,unsigned int addr)
{
	int i;

	for( i=0; i<pool->num; i++ ){
		if( pool->mb[i] == addr )
			break;
	}
	if( i >= pool->num )
		return;
	pool->used &= ~(0x1<<i);
	DBG_DETAIL("%d\n",i);
}

int vpp_disp_fb_cnt(struct list_head *list)
{
	struct list_head *ptr;
	int cnt;
	
	vpp_lock();
	cnt = 0;
	ptr = list;
	while( ptr->next != list ){
		ptr = ptr->next;
		cnt++;
	}
	vpp_unlock();
	return cnt;
}

int vpp_disp_fb_compare(vdo_framebuf_t *fb1,vdo_framebuf_t *fb2)
{
	if( fb1->img_w != fb2->img_w ) return 0;
	if( fb1->img_h != fb2->img_h ) return 0;
	if( fb1->fb_w != fb2->fb_w ) return 0;
	if( fb1->fb_h != fb2->fb_h ) return 0;
	if( fb1->col_fmt != fb2->col_fmt ) return 0;
	if( fb1->h_crop != fb2->h_crop ) return 0;
	if( fb1->v_crop != fb2->v_crop ) return 0;
	if( fb1->flag != fb2->flag ) return 0;
	return 1;
}

void vpp_disp_fb_get_addr(vpp_dispfb_parm_t *entry,unsigned int *yaddr,unsigned int *caddr)
{
	if( entry->parm.flag & VPP_FLAG_DISPFB_ADDR ){
		*yaddr = entry->parm.yaddr;
		*caddr = entry->parm.caddr;
	}
	else if(entry->parm.flag & VPP_FLAG_DISPFB_INFO){
		*yaddr = entry->parm.info.y_addr;
		*caddr = entry->parm.info.c_addr;
	}
	else {
		*yaddr = *caddr = 0;
	}
}

static int vpp_disp_fb_add
(
	vpp_dispfb_t *fb,		/*!<; // display frame pointer */
	vpp_fb_path_t path		/*!<; // frame buf path */
)
{
	vpp_dispfb_parm_t *entry;
	struct list_head *ptr;
	unsigned int yaddr=0,caddr=0;
	struct list_head *fb_list=0;
	int ret = 0;
	int check_disp_fb_cnt = 0;

	vpp_lock();
	switch( path ){
#ifdef CONFIG_SCL_DIRECT_PATH
		case VPP_FB_PATH_SCL:
			ptr = &vpp_scl_free_list;
			fb_list = &vpp_scl_fb_list;
			break;
#endif
		case VPP_FB_PATH_VPU:
			ptr = &vpp_disp_free_list;
			fb_list = &vpp_vpu_fb_list;
			check_disp_fb_cnt = 1;
			break;
		case VPP_FB_PATH_PIP:
			ret = 0;
			goto end_dispfb_add;
		case VPP_FB_PATH_DIRECT_GOVR:
			check_disp_fb_cnt = 1;
		case VPP_FB_PATH_GOVR:
		default:
			ptr = &vpp_disp_free_list;
			fb_list = &vpp_disp_fb_list;
			break;
	}
	
	if( check_disp_fb_cnt && (g_vpp.disp_fb_cnt >= g_vpp.disp_fb_max) ){
		if( vpp_check_dbg_level(VPP_DBGLVL_DISPFB) ){
			char buf[50];

			sprintf(buf,"*W* add dispfb full max %d cnt %d",g_vpp.disp_fb_max,g_vpp.disp_fb_cnt);
			vpp_dbg_show(VPP_DBGLVL_DISPFB,1,buf);	
		}
		g_vpp.disp_fb_full_cnt++;
		ret = -1;
		goto end_dispfb_add;
	}

	if( list_empty(ptr) ){
		ret = -1;
		goto end_dispfb_add;
	}

	// get a entry from free list to modify then add to frame buf queue
	ptr = ptr->next;
	entry = list_entry(ptr,vpp_dispfb_parm_t,list);
	list_del_init(ptr);
	entry->parm = *fb;
	memcpy(&entry->pts,&g_vpp.frame_pts,sizeof(vpp_pts_t));

#if 1	// VPU bilinear mode
	if( (path == VPP_FB_PATH_VPU) && ((fb->flag & VPP_FLAG_DISPFB_PIP) == 0) ){
		if(entry->parm.flag & VPP_FLAG_DISPFB_INFO){
			if( vpp_disp_fb_compare(&entry->parm.info,&p_vpu->fb_p->fb) ){
				entry->parm.flag &= ~VPP_FLAG_DISPFB_INFO;
				entry->parm.flag |= VPP_FLAG_DISPFB_ADDR;
				entry->parm.yaddr = entry->parm.info.y_addr;
				entry->parm.caddr = entry->parm.info.c_addr;
			}
		}
	}
#endif	
	list_add_tail(&entry->list,fb_list);
	vpp_disp_fb_get_addr(entry,&yaddr,&caddr);
	switch( path ){
		case VPP_FB_PATH_VPU:
		case VPP_FB_PATH_DIRECT_GOVR:
			g_vpp.disp_fb_cnt++;
			break;
		case VPP_FB_PATH_SCL:
			if( g_vpp.vpp_path == VPP_VPATH_SCL ){
				g_vpp.disp_fb_cnt++;
			}
			break;
		case VPP_FB_PATH_GOVR:
			goto end_dispfb_add;
		default:
			break;
	}

	if( (entry->parm.flag & VPP_FLAG_DISPFB_MB_NO) == 0 ){
		if( yaddr ){
			mb_get(yaddr);
		}
		
		if( caddr && !(entry->parm.flag & VPP_FLAG_DISPFB_MB_ONE)){
			mb_get(caddr);
		}
	}
end_dispfb_add:
	if( ret == 0 ){
		if( vpp_check_dbg_level(VPP_DBGLVL_DISPFB) ){
			char buf[100];
			int tmr_no = 1;

			switch( path ){
#ifdef CONFIG_SCL_DIRECT_PATH
				case VPP_FB_PATH_SCL:
					sprintf(buf,"add %s Y 0x%x C 0x%x(Q %d/%d,MB %d/%d,C %d,I %d,W %d)","scl",yaddr,caddr,
						vpp_disp_fb_cnt(&vpp_scl_fb_list),VPP_SCL_FB_MAX,g_vpp.scl_fb_mb_used,VPP_SCL_MB_MAX,
						g_vpp.scl_fb_cnt,g_vpp.scl_fb_mb_index,g_vpp.scl_fb_mb_on_work);
					tmr_no = 3;
					break;
#endif
				case VPP_FB_PATH_VPU:
					tmr_no = 2;
				case VPP_FB_PATH_DIRECT_GOVR:
				case VPP_FB_PATH_GOVR:
				default:
					sprintf(buf,"add %s Y 0x%x C 0x%x(Q %d/%d,E %d/%d)",(path == VPP_FB_PATH_VPU)?"vpu":"govr",yaddr,caddr,
						g_vpp.disp_fb_cnt,g_vpp.disp_fb_max,g_vpp.disp_cnt,g_vpp.disp_fb_isr_cnt);
					break;
			}
			vpp_dbg_show(VPP_DBGLVL_DISPFB,tmr_no,buf);
		}
	}
	else {
//		DPRINT("[VPP] add full %d\n",path);
	}
	vpp_unlock();
	return ret;
} /* End of vpp_disp_fb_add */

void vpp_disp_fb_free(unsigned int y_addr,unsigned int c_addr)
{
	#define FREE_QUEUE_MAX	4
	static int cnt = 0;
	static unsigned int y_addr_queue[FREE_QUEUE_MAX];
	static unsigned int c_addr_queue[FREE_QUEUE_MAX];
	int i;

	if( y_addr == 0 ){
		for(i=0;i<cnt;i++){
			y_addr = y_addr_queue[i];
			c_addr = c_addr_queue[i];
			if( y_addr ) mb_put(y_addr);
			if( c_addr ) mb_put(c_addr);
			y_addr_queue[i] = c_addr_queue[i] = 0;
		}
		cnt = 0;
	}
	else {
		y_addr_queue[cnt] = y_addr;
		c_addr_queue[cnt] = c_addr;
		cnt++;		
		if( cnt > g_vpp.disp_fb_keep ){
			y_addr = y_addr_queue[0];
			c_addr = c_addr_queue[0];
			cnt--;
			
			for(i=0;i<cnt;i++){
				y_addr_queue[i] = y_addr_queue[i+1];
				c_addr_queue[i] = c_addr_queue[i+1];
			}
			if( y_addr ) mb_put(y_addr);
			if( c_addr ) mb_put(c_addr);
			if( vpp_check_dbg_level(VPP_DBGLVL_DISPFB) ){
				char buf[50];

				sprintf(buf,"free dispfb Y 0x%x C 0x%x,keep %d",y_addr,c_addr,cnt);
				vpp_dbg_show(VPP_DBGLVL_DISPFB,2,buf);
			}
#if 0
			DPRINT("[VPP] mb put vir(0x%x,0x%x)\n",y_addr,c_addr);
#endif
		}
	}
}

static int vpp_disp_fb_clr(int pip,int chk_view)
{
	vpp_dispfb_parm_t *entry;
	struct list_head *fb_list,*ptr;
	unsigned int yaddr,caddr;
	struct list_head *free_list;
	vpp_mod_t mod;
	int i;

	vpp_lock();

	if( vpp_check_dbg_level(VPP_DBGLVL_DISPFB) ){
		DPRINT("[VPP] %s\n",__FUNCTION__);
	}

	fb_list = &vpp_disp_fb_list;
	yaddr = vpp_pre_dispfb_y_addr;
	caddr = vpp_pre_dispfb_c_addr;
	vpp_pre_dispfb_y_addr = 0;
	vpp_pre_dispfb_c_addr = 0;
	if( yaddr ) mb_put(yaddr);
	if( caddr ) mb_put(caddr);
	if( yaddr && vpp_check_dbg_level(VPP_DBGLVL_DISPFB) ){
		char buf[50];

		sprintf(buf,"free dispfb Y 0x%x C 0x%x",yaddr,caddr);
		vpp_dbg_show(VPP_DBGLVL_DISPFB,2,buf);
	}
	g_vpp.disp_fb_cnt = 0;		

	for(i=0;i<3;i++){
		switch(i){
			case 0:	// govrh
				fb_list = &vpp_disp_fb_list;
				free_list = &vpp_disp_free_list;
				mod = VPP_MOD_GOVRH;
				break;
			case 1:	// vpu
				fb_list = &vpp_vpu_fb_list;
				free_list = &vpp_disp_free_list;
				mod = VPP_MOD_VPU;
				break;
#ifdef CONFIG_SCL_DIRECT_PATH
			case 2: // scl
				fb_list = &vpp_scl_fb_list;
				free_list = &vpp_scl_free_list;
				mod = VPP_MOD_SCL;
				break;
#endif
			default:
				continue;
		}
		
		if( yaddr && vpp_check_dbg_level(VPP_DBGLVL_DISPFB) ){
			char buf[50];
			sprintf(buf,"free %s fb",vpp_mod_str[mod]);
			vpp_dbg_show(VPP_DBGLVL_DISPFB,2,buf);
		}
		
		while( !list_empty(fb_list) ){
			ptr = fb_list->next;
#ifdef CONFIG_SCL_DIRECT_PATH
			if( mod == VPP_MOD_SCL ){ // scl fb list, not clean fb in scaling
				if( g_vpp.scl_fb_mb_on_work ){
					if( fb_list == ptr->next )
						break;
					ptr = ptr->next;
				}
			}
#endif
			entry = list_entry(ptr,vpp_dispfb_parm_t,list);
			list_del_init(ptr);
			list_add_tail(&entry->list,free_list);
			vpp_disp_fb_get_addr(entry,&yaddr,&caddr);
			
			if( mod == VPP_MOD_VPU ){ // vpu fb list, check view in queue
				if( chk_view ){
					if( entry->parm.flag & VPP_FLAG_DISPFB_ADDR ){
						vpp_fb_base_t *mod_fb_p;
						
						yaddr = entry->parm.yaddr;
						caddr = entry->parm.caddr;
						mod_fb_p = vpp_mod_get_fb_base(VPP_MOD_VPU);
						mod_fb_p->set_addr(yaddr,caddr);
						mod_fb_p->fb.y_addr = yaddr;
						mod_fb_p->fb.c_addr = caddr;
					}

					if( entry->parm.flag & VPP_FLAG_DISPFB_INFO ){
						vpp_fb_base_t *mod_fb_p;

						mod_fb_p = vpp_mod_get_fb_base(VPP_MOD_VPU);
						yaddr = entry->parm.info.y_addr;
						caddr = entry->parm.info.c_addr;
						mod_fb_p->fb = entry->parm.info;
						mod_fb_p->set_framebuf(&mod_fb_p->fb);
					}
					
					if( entry->parm.flag & VPP_FLAG_DISPFB_VIEW ){
						vpp_set_video_scale(&entry->parm.view);
					}
				}
			}
			
			if( (entry->parm.flag & VPP_FLAG_DISPFB_MB_NO) == 0 ){
				if( yaddr ){
					mb_put(yaddr);
				}
				
				if( caddr && !(entry->parm.flag & VPP_FLAG_DISPFB_MB_ONE)){
					mb_put(caddr);
				}
				
				if( yaddr && vpp_check_dbg_level(VPP_DBGLVL_DISPFB) ){
					char buf[50];

					sprintf(buf,"free fb Y 0x%x C 0x%x",yaddr,caddr);
					vpp_dbg_show(VPP_DBGLVL_DISPFB,2,buf);
				}
			}
			else {
				if( yaddr && vpp_check_dbg_level(VPP_DBGLVL_DISPFB) ){
					char buf[50];

					sprintf(buf,"free fb2 Y 0x%x C 0x%x",yaddr,caddr);
					vpp_dbg_show(VPP_DBGLVL_DISPFB,2,buf);
				}
			}
		}
	}
#ifdef CONFIG_SCL_DIRECT_PATH
	g_vpp.scl_fb_mb_used = 0;
#ifdef CONFIG_GOVW_SCL_PATH
	g_vpp.govw_fb_pool.used = 0;
#endif
#endif

	if( vpp_check_dbg_level(VPP_DBGLVL_DISPFB) ){
		DPRINT("[VPP] Exit %s\n",__FUNCTION__);
	}

	vpp_unlock();
	return 0;
} /* End of vpp_disp_fb_clr */

static int vpp_disp_fb_isr(vpp_fb_path_t path)
{
	vpp_mod_t mod;
	vpp_dispfb_parm_t *entry;
	struct list_head *ptr = 0;
	unsigned int yaddr = 0,caddr = 0;
	int ret = 0;
	int disp_flag = 0;
	int scl_mb_free = 0;
	vpp_mod_base_t *base;

	if( g_vpp.vpp_path_no_ready )
		return 0;

	vpp_lock();

	switch( path ){
		case VPP_FB_PATH_GOVR:
			switch( g_vpp.vpp_path ){
				case VPP_VPATH_SCL:
				case VPP_VPATH_GOVR:
					scl_mb_free = 1;
					disp_flag = 1;
					break;
				case VPP_VPATH_GOVW_SCL:
					scl_mb_free = 1;
					break;
				case VPP_VPATH_VPU:
					goto vpu_path_label;
					break;
				default:
					break;
			}
			ptr = &vpp_disp_fb_list;
			mod = VPP_MOD_GOVRH;
			break;
		case VPP_FB_PATH_VPU:
vpu_path_label:			
			if( g_vpp.vpu_skip_all ){
				g_vpp.disp_skip_cnt++;
				goto end_dispfb_isr;
			}
			switch( g_vpp.vpp_path ){
				case VPP_VPATH_VPU:
				case VPP_VPATH_GOVW:
				case VPP_VPATH_GOVW_SCL:
					disp_flag = 1;
					break;
				default:
					break;
			}
 			ptr = &vpp_vpu_fb_list;
			mod = VPP_MOD_VPU;
			break;
		default:
			goto end_dispfb_isr;
	}

	if( disp_flag ){
#ifdef CONFIG_VPP_DISPFB_FREE_POSTPONE
	if( vpp_pre_dispfb_y_addr ){
		vpp_disp_fb_free(vpp_pre_dispfb_y_addr,vpp_pre_dispfb_c_addr);
		vpp_pre_dispfb_y_addr = 0;
		vpp_pre_dispfb_c_addr = 0;
	}
#endif
	}
	
#ifdef CONFIG_VPP_GOVRH_FBSYNC
	if( path == VPP_FB_PATH_GOVR ){
		if( vpp_frc_check() ){
			goto end_dispfb_isr;
		}
	}
#endif
	g_vpp.disp_fb_isr_cnt++;
	if( list_empty(ptr) )
		goto end_dispfb_isr;

	ret = 1;
	ptr = ptr->next;
	entry = list_entry(ptr,vpp_dispfb_parm_t,list);
	memcpy(&g_vpp.govw_pts,&entry->pts,sizeof(vpp_pts_t));

	base = vpp_mod_get_base(mod);
	if( mod == VPP_MOD_VPU ){
#ifdef WMT_FTBLK_VPU
		vpu_set_reg_update(VPP_FLAG_DISABLE);
#else
		scl_set_reg_update(VPP_FLAG_DISABLE);
#endif
		govm_set_reg_update(VPP_FLAG_DISABLE);
	}

	if( entry->parm.flag & VPP_FLAG_DISPFB_ADDR ){
		vpp_fb_base_t *mod_fb_p;
		
		yaddr = entry->parm.yaddr;
		caddr = entry->parm.caddr;
		mod_fb_p = vpp_mod_get_fb_base(mod);
		mod_fb_p->set_addr(yaddr,caddr);
		mod_fb_p->fb.y_addr = yaddr;
		mod_fb_p->fb.c_addr = caddr;
	}

	if( entry->parm.flag & VPP_FLAG_DISPFB_INFO ){
		vpp_fb_base_t *mod_fb_p;

		mod_fb_p = vpp_mod_get_fb_base(mod);
		yaddr = entry->parm.info.y_addr;
		caddr = entry->parm.info.c_addr;
		if( mod == VPP_MOD_GOVRH ){
#ifdef WMT_FTBLK_GOVRH
			vpp_display_format_t field;

			if( entry->parm.info.col_fmt != mod_fb_p->fb.col_fmt ){
				mod_fb_p->fb.col_fmt = entry->parm.info.col_fmt;
				govrh_set_color_format((void *)base,mod_fb_p->fb.col_fmt);
				mod_fb_p->set_csc(mod_fb_p->csc_mode);
			}
#ifdef CONFIG_VPP_PATH_SWITCH_SCL_GOVR
			if( entry->parm.info.fb_w != mod_fb_p->fb.fb_w ){
				govrh_set_fb_width((void *)base,entry->parm.info.fb_w);
//				DPRINT("[VPP] govrh fb width %d->%d\n",mod_fb_p->fb.fb_w,mod_fb_p->fb.fb_w);
			}
#endif
			field = (entry->parm.info.flag & VDO_FLAG_INTERLACE)?VPP_DISP_FMT_FIELD:VPP_DISP_FMT_FRAME;
			govrh_set_source_format((void *)base,field);
			govrh_set_fb_addr((void *)base,yaddr,caddr);
			mod_fb_p->fb = entry->parm.info;
#endif
		}
		else {
			mod_fb_p->fb = entry->parm.info;
			mod_fb_p->set_framebuf(&mod_fb_p->fb);
		}
	}

	if( entry->parm.flag & VPP_FLAG_DISPFB_VIEW ){
		vpp_set_video_scale(&entry->parm.view);
	}

	if( mod == VPP_MOD_VPU ){
#ifdef WMT_FTBLK_VPU
		vpu_set_reg_update(VPP_FLAG_ENABLE);
#else
		scl_set_reg_update(VPP_FLAG_ENABLE);
#endif
		govm_set_reg_update(VPP_FLAG_ENABLE);
	}

	list_del_init(ptr);
	list_add_tail(&entry->list,&vpp_disp_free_list);

	if( disp_flag ){
		if( vpp_check_dbg_level(VPP_DBGLVL_DISPFB) ){
			char buf[50];

			sprintf(buf,"show disp fb Y 0x%x C 0x%x (Q %d/%d,E %d/%d),%d",yaddr,caddr,
					g_vpp.disp_fb_cnt,g_vpp.disp_fb_max,g_vpp.disp_cnt,g_vpp.disp_fb_isr_cnt,path);
			vpp_dbg_show(VPP_DBGLVL_DISPFB,2,buf);
		}

#ifndef CONFIG_VPP_DISPFB_FREE_POSTPONE
		vpp_disp_fb_free(vpp_pre_dispfb_y_addr,vpp_pre_dispfb_c_addr);
#endif
		vpp_pre_dispfb_y_addr = vpp_cur_dispfb_y_addr;
		vpp_pre_dispfb_c_addr = vpp_cur_dispfb_c_addr;
		if( entry->parm.flag & VPP_FLAG_DISPFB_MB_NO ){
			vpp_cur_dispfb_y_addr = 0;
			vpp_cur_dispfb_c_addr = 0;
		}
		else {
			vpp_cur_dispfb_y_addr = (yaddr)? yaddr:0;
			vpp_cur_dispfb_c_addr = (caddr && !(entry->parm.flag & VPP_FLAG_DISPFB_MB_ONE))? caddr:0;
		}
	}

#ifdef CONFIG_SCL_DIRECT_PATH
	if( scl_mb_free ){
		g_vpp.scl_fb_mb_used--;
//		DPRINT("[VPP] scl mb free 0x%x\n",yaddr);
	}
#endif
end_dispfb_isr:
	vpp_unlock();
	return ret;
} /* End of vpp_disp_fb_isr */

#ifdef CONFIG_SCL_DIRECT_PATH
void vpp_scl_fb_init(void)
{
	int i;
	
	INIT_LIST_HEAD(&vpp_scl_free_list);
	INIT_LIST_HEAD(&vpp_scl_fb_list);
	for(i=0;i<VPP_SCL_FB_MAX;i++)
		list_add_tail(&vpp_scl_fb_array[i].list,&vpp_scl_free_list);
	g_vpp.scl_fb_mb_clear = 0xFF;
}

int vpp_scl_fb_get_mb(unsigned int *yaddr,unsigned int *caddr,int next)
{
	int y_size,c_size;

	if( next == 0 ){
		if( g_vpp.scl_fb_mb_used >= VPP_SCL_MB_MAX ){
			return 1;
		}
	}
	y_size = p_govrh->fb_p->fb.y_size;
	c_size = p_govrh->fb_p->fb.c_size;
get_mb_label:
	if( g_vpp.scl_fb_mb_index < g_vpp.mb_govw_cnt ){
		*yaddr = g_vpp.mb_govw[g_vpp.scl_fb_mb_index];
		*caddr = g_vpp.mb_govw[g_vpp.scl_fb_mb_index] + y_size;
	}
	else {
		unsigned int offset;
		offset = REG32_VAL(GE3_BASE_ADDR+0x50);
		offset *= REG32_VAL(GE3_BASE_ADDR+0x54);
		offset *= 4;
		if( g_vpp.scl_fb_mb_index > VPP_MB_ALLOC_NUM ){
			offset += ((y_size + c_size)*(g_vpp.scl_fb_mb_index-VPP_MB_ALLOC_NUM));
		}
		offset = vpp_calc_align(offset,VPP_FB_ADDR_ALIGN);
		
		*yaddr = (num_physpages << PAGE_SHIFT) + offset;
		*caddr = (num_physpages << PAGE_SHIFT) + offset + y_size;
	}

	if( next ){
		g_vpp.scl_fb_mb_index++;
		if(g_vpp.scl_fb_mb_index >= g_vpp.mb_govw_cnt){
			g_vpp.scl_fb_mb_index = 0;
		}

#ifdef CONFIG_SCL_DIRECT_PATH_DEBUG
		g_vpp.scl_fb_mb_used++;
		if( g_vpp.scl_fb_mb_used > VPP_SCL_MB_MAX ){
			DPRINT("[VPP] *W* scl mb use %d(max %d)\n",g_vpp.scl_fb_mb_used,VPP_SCL_MB_MAX);
			g_vpp.scl_fb_mb_over++;
		}
#endif
	}
	else {
		unsigned int ysa,csa;
		
		// skip for mb is current govr fb
		govrh_get_fb_addr(p_govrh,&ysa,&csa);
		if( *yaddr == ysa ){
			if( g_vpp.scl_fb_mb_used == 0 ){
				g_vpp.scl_fb_mb_index++;
				if(g_vpp.scl_fb_mb_index >= g_vpp.mb_govw_cnt){
					g_vpp.scl_fb_mb_index = 0;
				}
				goto get_mb_label;
			}
			return 1;
		}
	}
	return 0;
}

#ifdef CONFIG_SCL_DIRECT_PATH_DEBUG
void vpp_scl_fb_cal_tmr(int begin)
{
	static struct timeval pre_tv;

	if( begin ){
		do_gettimeofday(&pre_tv);
	}
	else {
		struct timeval tv;
		unsigned int tm_usec = 0;
		
		do_gettimeofday(&tv);
		if( pre_tv.tv_sec ){
			tm_usec = ( tv.tv_sec == pre_tv.tv_sec )? (tv.tv_usec - pre_tv.tv_usec):(1000000 + tv.tv_usec - pre_tv.tv_usec);
			g_vpp.scl_fb_tmr += (tm_usec / 1000);
		}
	}
}
#endif	

void vpp_scl_fb_set_size(vdo_framebuf_t *fb)
{
	unsigned int line_byte;
	unsigned int offset;

#if 0	// debug for clear memory issue
	g_vpp.scl_fb_mb_clear = 0xFF;
#else
	if( (fb->img_w == p_vpu->resx_visual) && (fb->img_h == p_vpu->resy_visual) ){
		g_vpp.scl_fb_mb_clear = 0;
		return;
	}
#endif

	fb->y_addr += p_vpu->posx + p_vpu->posy * p_govw->fb_p->fb.fb_w;
	line_byte = p_govw->fb_p->fb.fb_w;
	offset = p_vpu->posx;
	switch( p_govw->fb_p->fb.col_fmt ){
		case VDO_COL_FMT_YUV420:
		case VDO_COL_FMT_YUV422H:			
			offset = (offset / 2) * 2;
			break;
		case VDO_COL_FMT_YUV444:
			line_byte *= 2;
			break;
		default:
			break;
	}
	fb->c_addr += offset + p_vpu->posy * line_byte;
#if 0
	fb->y_addr &= ~0x3F;
	fb->c_addr &= ~0x3F;
#else
	fb->fb_w = vpp_calc_align(fb->fb_w,VPP_FB_ADDR_ALIGN);
#endif

	fb->img_w = p_vpu->resx_visual;
	fb->img_h = p_vpu->resy_visual;
	if( g_vpp.scl_fb_mb_clear & (0x1 << g_vpp.scl_fb_mb_index) ){
		unsigned int yaddr,caddr;

		yaddr = g_vpp.mb_govw[g_vpp.scl_fb_mb_index];
		caddr = g_vpp.mb_govw[g_vpp.scl_fb_mb_index] + p_govw->fb_p->fb.y_size;
		yaddr = (unsigned int) mb_phys_to_virt(yaddr);
		caddr = (unsigned int) mb_phys_to_virt(caddr);
		memset((void *)yaddr,0x0,p_govw->fb_p->fb.y_size);
		memset((void *)caddr,0x80,p_govw->fb_p->fb.y_size);
		
		g_vpp.scl_fb_mb_clear &= ~(0x1 << g_vpp.scl_fb_mb_index);
//		DPRINT("[VPP] clr scl fb mb %d\n",g_vpp.scl_fb_mb_index);
	}
}

void vpp_scl_fb_isr(void)
{
	static vpp_dispfb_parm_t *entry = 0;
	vdo_framebuf_t src_fb,dst_fb;
	unsigned int yaddr,caddr;
	unsigned int pre_yaddr,pre_caddr;

	/* ---------------------------------------------------------------------- */
	/* scale complete */
	/* ---------------------------------------------------------------------- */
	g_vpp.scl_fb_isr++;
	if( scl_scale_complete ){
		vpp_scl_fb_get_mb(&yaddr,&caddr,1);
		pre_yaddr = pre_caddr = 0;
		if( entry->parm.flag & VPP_FLAG_DISPFB_ADDR ){
			pre_yaddr = entry->parm.yaddr;
			pre_caddr = entry->parm.caddr;
			entry->parm.yaddr = yaddr;
			entry->parm.caddr = caddr;
		}

		if( entry->parm.flag & VPP_FLAG_DISPFB_INFO ){
			pre_yaddr = entry->parm.info.y_addr;
			pre_caddr = entry->parm.info.c_addr;
			entry->parm.info.y_addr = yaddr;
			entry->parm.info.c_addr = caddr;
		}

#ifdef CONFIG_GOVW_SCL_PATH
		if( g_vpp.vpp_path == VPP_VPATH_GOVW_SCL ){
			vpp_fb_pool_put(&g_vpp.govw_fb_pool,pre_yaddr);
			entry->parm.yaddr = yaddr;
			entry->parm.caddr = caddr;
			entry->parm.flag = (VPP_FLAG_DISPFB_MB_ONE + VPP_FLAG_DISPFB_MB_NO + VPP_FLAG_DISPFB_ADDR);
		}
		else 
#endif
		{
			pre_yaddr = (pre_yaddr)? pre_yaddr:0;
			pre_caddr = (pre_caddr && !(entry->parm.flag & VPP_FLAG_DISPFB_MB_ONE))? pre_caddr:0;
			vpp_disp_fb_free(pre_yaddr,pre_caddr);

			entry->parm.flag = (VPP_FLAG_DISPFB_MB_ONE + VPP_FLAG_DISPFB_MB_NO);
#ifdef CONFIG_VPP_PATH_SWITCH_SCL_GOVR
			if( (p_govw->fb_p->fb.col_fmt != p_govrh->fb_p->fb.col_fmt) || 
				(p_govw->fb_p->fb.fb_w != p_govrh->fb_p->fb.fb_w)){
				entry->parm.info.y_addr = yaddr;
				entry->parm.info.c_addr = caddr;
				entry->parm.info.col_fmt = p_govw->fb_p->fb.col_fmt;
				entry->parm.info.fb_w = p_govw->fb_p->fb.fb_w;
				entry->parm.info.flag = 0;
				entry->parm.flag |= VPP_FLAG_DISPFB_INFO;
			}
			else {
#else
			if( 1 ){
#endif
				entry->parm.yaddr = yaddr;
				entry->parm.caddr = caddr;
				entry->parm.flag |= VPP_FLAG_DISPFB_ADDR;
			}
		}

		if( vpp_disp_fb_add(&entry->parm,VPP_FB_PATH_GOVR) ){
			g_vpp.scl_fb_mb_used--;
		}

		// free src fb
		list_del_init(vpp_scl_fb_list.next);
		list_add_tail(&entry->list,&vpp_scl_free_list);
		scl_scale_complete = 0;
		g_vpp.scl_fb_mb_on_work = 0;
		entry = 0;
//		DPRINT("[VPP] SCALE PATH COMPLETE (0x%x,0x%x)->(0x%x,0x%x)\n",pre_yaddr,pre_caddr,yaddr,caddr);
	}

	/* ---------------------------------------------------------------------- */
	/* check fb queue for scale  */
	/* ---------------------------------------------------------------------- */
	if( g_vpp.scl_fb_mb_on_work )	// in scaling ...
		return;

	if( list_empty(&vpp_scl_fb_list) )
		return;

	yaddr = caddr = 0;
	entry = list_entry(vpp_scl_fb_list.next,vpp_dispfb_parm_t,list);
	vpp_disp_fb_get_addr(entry,&yaddr,&caddr);
	if( entry->parm.flag & VPP_FLAG_DISPFB_INFO ){
#ifdef CONFIG_GOVW_SCL_PATH
//		if( g_vpp.vpp_path == VPP_VPATH_GOVW_SCL )
#endif
		if( vpp_disp_fb_compare(&p_vpu->fb_p->fb,&entry->parm.info) == 0 ){
			p_vpu->fb_p->fb = entry->parm.info;
			g_vpp.scl_fb_mb_clear = 0xFF;
		}
	}

	if( entry->parm.flag & VPP_FLAG_DISPFB_VIEW ){
		vpp_set_video_scale(&entry->parm.view);
	}

#ifdef CONFIG_GOVW_SCL_PATH
	if( g_vpp.vpp_path == VPP_VPATH_GOVW_SCL ){
		if( (p_govw->fb_p->fb.img_w == p_govrh->fb_p->fb.img_w) &&
			(p_govw->fb_p->fb.img_h == p_govrh->fb_p->fb.img_h)
			){
			vpp_fb_pool_put(&g_vpp.govw_fb_pool,yaddr);
			entry->parm.yaddr = yaddr;
			entry->parm.caddr = caddr;
			entry->parm.flag = (VPP_FLAG_DISPFB_MB_ONE + VPP_FLAG_DISPFB_MB_NO + VPP_FLAG_DISPFB_ADDR);
			if( vpp_disp_fb_add(&entry->parm,VPP_FB_PATH_GOVR) ){
				return;
			}
			g_vpp.scl_fb_mb_used++;
			list_del_init(vpp_scl_fb_list.next);
			list_add_tail(&entry->list,&vpp_scl_free_list);
			return;
		}
	}
	else 
#endif
#ifdef CONFIG_VPP_PATH_SWITCH_SCL_GOVR	// if no scale then pass to govr path
	if( (p_vpu->resx_visual == p_vpu->fb_p->fb.img_w) &&
		(p_vpu->resy_visual == p_vpu->fb_p->fb.img_h) && 
		(p_vpu->resx_visual == p_govw->fb_p->fb.img_w) &&
		(p_vpu->resy_visual == p_govw->fb_p->fb.img_h)
		){
		if( vpp_disp_fb_add(&entry->parm,VPP_FB_PATH_GOVR) ){
			return;
		}
		g_vpp.scl_fb_mb_used++;
		list_del_init(vpp_scl_fb_list.next);
		list_add_tail(&entry->list,&vpp_scl_free_list);
//		DPRINT("[VPP] SCALE DIRPATH (disp cnt %d,mb used %d)\n",g_vpp.disp_fb_cnt,g_vpp.scl_fb_mb_used);
		return;
	}
#endif

	if( yaddr ){
		src_fb = p_vpu->fb_p->fb;
		src_fb.y_addr = yaddr;
		src_fb.c_addr = caddr;

		if( g_vpp.vpp_path == VPP_VPATH_GOVW_SCL ){
			dst_fb = p_govrh->fb_p->fb;
			if( vpp_scl_fb_get_mb(&dst_fb.y_addr,&dst_fb.c_addr,0) ){
				return;
			}
		}
		else {
			dst_fb = p_govw->fb_p->fb;
			if( vpp_scl_fb_get_mb(&dst_fb.y_addr,&dst_fb.c_addr,0) ){
				return;
			}

			vpp_scl_fb_set_size(&dst_fb);
		}

//		DPRINT("[VPP] SCALE PATH(0x%x,0x%x)->(0x%x,0x%x)\n",yaddr,caddr,dst_fb.y_addr,dst_fb.c_addr);

#ifdef CONFIG_SCL_DIRECT_PATH_DEBUG
		g_vpp.scl_fb_cnt++;
		vpp_scl_fb_cal_tmr(1);
#endif

#if 1
		// scale next frame
		p_scl->scale_sync = 0;
		g_vpp.scl_fb_mb_on_work = 1;
		if( vpp_set_recursive_scale(&src_fb,&dst_fb) ){
			DPRINT("[VPP] *E* scl fb scale fail\n");
		}
#endif
	}
	else {
		list_del_init(vpp_scl_fb_list.next);
		list_add_tail(&entry->list,&vpp_scl_free_list);
	}
}
#endif

#ifdef CONFIG_VPP_STREAM_CAPTURE
unsigned int vpp_mb_get_mask(unsigned int phy)
{
	int i;
	unsigned int mask;

	for(i=0;i<g_vpp.mb_govw_cnt;i++){
		if( g_vpp.mb_govw[i] == phy )
			break;
	}
	if( i >= g_vpp.mb_govw_cnt ){
		return 0;
	}
	mask = 0x1 << i;
	return mask;
}

int vpp_mb_get(unsigned int phy)
{
	unsigned int mask;
	int i,cnt;

	if( g_vpp.stream_mb_sync_flag ){	// not new mb updated
		vpp_dbg_show(VPP_DBGLVL_STREAM,0,"[VPP] *W* mb_get addr not update");
		return -1;
	}

	for(i=0,cnt=0;i<g_vpp.mb_govw_cnt;i++){
		if( g_vpp.stream_mb_lock & (0x1 << i))
			cnt++;
	}

	if( cnt >= (g_vpp.mb_govw_cnt - 2) ){
		vpp_dbg_show(VPP_DBGLVL_STREAM,0,"[VPP] *W* mb_get addr not free");
		return -1;
	}

	if( (mask = vpp_mb_get_mask(phy)) == 0 ){
		vpp_dbg_show(VPP_DBGLVL_STREAM,0,"[VPP] *W* mb_get invalid addr");
		return -1;
	}
	if( g_vpp.stream_mb_lock & mask ){
		vpp_dbg_show(VPP_DBGLVL_STREAM,0,"[VPP] *W* mb_get lock addr");	
		return -1;
	}
	g_vpp.stream_mb_lock |= mask;
	g_vpp.stream_mb_sync_flag = 1;
	if( vpp_check_dbg_level(VPP_DBGLVL_STREAM) ){
		char buf[50];

		sprintf(buf,"stream mb get 0x%x,mask 0x%x",phy,mask);
		vpp_dbg_show(VPP_DBGLVL_STREAM,1,buf);
	}
	return 0;
}

int vpp_mb_put(unsigned int phy)
{
	unsigned int mask;

	if( phy == 0 ){
		g_vpp.stream_mb_lock = 0;
		g_vpp.stream_mb_index = 0;
		return 0;
	}

	if( (mask = vpp_mb_get_mask(phy)) == 0 ){
		DPRINT("[VPP] *W* mb_put addr 0x%x\n",phy);
		return 1;
	}
	if( !(g_vpp.stream_mb_lock & mask) ){
		DPRINT("[VPP] *W* mb_put nonlock addr 0x%x\n",phy);
	}
	g_vpp.stream_mb_lock &= ~mask;
	if( vpp_check_dbg_level(VPP_DBGLVL_STREAM) ){
		char buf[50];

		sprintf(buf,"stream mb put 0x%x,mask 0x%x",phy,mask);
		vpp_dbg_show(VPP_DBGLVL_STREAM,2,buf);
	}
	return 0;
}

void vpp_mb_sync(int govr_mode,int cnt)
{
	if( g_vpp.stream_enable == 0 )
		return;

	if( vout_info[0].govr_mod != govr_mode )
		return;
	
	if( (cnt % 2) == 0 ){
		g_vpp.stream_mb_sync_flag = 0;
	}
}
#endif

void vpp_govw_int_routine(void)
{
	unsigned int new_y,new_c;
	unsigned int cur_y,cur_c;
	int i;

	new_y = new_c = 0;
	cur_y = cur_c = 0;
	if( g_vpp.govw_fb_cnt ){
		g_vpp.govw_fb_cnt--;
		if( g_vpp.govw_fb_cnt == 0 ){
			vpp_set_govw_tg(VPP_FLAG_DISABLE);
		}
	}

	if( g_vpp.govw_skip_all ){
		return;
	}

	if( g_vpp.govw_skip_frame ){
		g_vpp.govw_skip_frame--;
		vpp_dbg_show(VPP_DBGLVL_DIAG,3,"GOVW skip");
		return;
	}

#ifdef CONFIG_GOVW_SCL_PATH
	if( g_vpp.vpp_path == VPP_VPATH_GOVW_SCL ){
		vpp_dispfb_t fb;

		if( (new_y = vpp_fb_pool_get(&g_vpp.govw_fb_pool)) == 0 ){
			// DPRINT("[VPP] *W* no fb in govw pool\n");
			return;
		}
		new_c = vpp_calc_align(new_y + p_govw->fb_p->fb.y_size,64);

		memcpy(&fb.info,&p_govw->fb_p->fb,sizeof(vdo_framebuf_t));
		govw_get_width(&fb.info.img_w,&fb.info.fb_w);
		fb.flag = VPP_FLAG_DISPFB_INFO | VPP_FLAG_DISPFB_MB_NO;
		if( vpp_disp_fb_add(&fb,VPP_FB_PATH_SCL) ){
			vpp_fb_pool_put(&g_vpp.govw_fb_pool,new_y);
			return;
		}
	}
	else 
#endif

	// check current address	
	cur_y = p_govw->fb_p->fb.y_addr;
	for(i=0;i<g_vpp.mb_govw_cnt;i++){
		if( cur_y == g_vpp.mb_govw[i] )
			break;
	}
	if( i >= g_vpp.mb_govw_cnt )
		i = 0;
	cur_y = g_vpp.mb_govw[i];
	cur_c = cur_y + g_vpp.mb_govw_y_size;

	// get new address
	do {
		i++;
		if( i >= g_vpp.mb_govw_cnt ){
			i = 0;
		}
		break;
	} while(1);
	new_y = g_vpp.mb_govw[i];
	new_c = new_y + g_vpp.mb_govw_y_size;

	if( cur_y ){
		g_vpp.govr->fb_p->set_addr(cur_y,cur_c);
		g_vpp.govr->fb_p->fb.y_addr = cur_y;
		g_vpp.govr->fb_p->fb.c_addr = cur_c;
	}

	if( new_y ){
		p_govw->fb_p->set_addr(new_y,new_c);
		p_govw->fb_p->fb.y_addr = new_y;
		p_govw->fb_p->fb.c_addr = new_c;
	}
	memcpy(&g_vpp.disp_pts,&g_vpp.govw_pts,sizeof(vpp_pts_t));
} /* End of vpp_govw_int_routine */

int vpp_irqproc_get_no(vpp_int_t vpp_int)
{
	int i;
	unsigned int mask;

	if( vpp_int == 0 )
		return 0xFF;
		
	for(i=0,mask=0x1;i<32;i++,mask<<=1){
		if( vpp_int & mask )
			break;
	}
	return i;
} /* End of vpp_irqproc_get_no */

vpp_irqproc_t *vpp_irqproc_get_entry(vpp_int_t vpp_int)
{
	int no;

	no = vpp_irqproc_get_no(vpp_int);
	if( no >= 32 ) 
		return 0;
	return vpp_irqproc_array[no];
} /* End of vpp_irqproc_get_entry */

static void vpp_irqproc_do_tasklet
(
	unsigned long data		/*!<; // tasklet input data */
)
{
	vpp_proc_t *entry;
	struct list_head *ptr;
	vpp_irqproc_t *irqproc;

	vpp_lock();
	irqproc = vpp_irqproc_get_entry(data);
	if( irqproc ){
		do {
			if( list_empty(&irqproc->list) )
				break;

			/* get task from work head queue */
			ptr = (&irqproc->list)->next;
			entry = list_entry(ptr,vpp_proc_t,list);
			if( entry->func ){
				if( entry->func(entry->arg) )
					break;
			}
			list_del_init(ptr);
			list_add_tail(&entry->list,&vpp_free_list);
			up(&entry->sem);
//			DPRINT("[VPP] vpp_irqproc_do_tasklet\n");
		} while(1);
	}
	vpp_unlock();
} /* End of vpp_irqproc_do_tasklet */

int vpp_irqproc_new
(
	vpp_int_t vpp_int,
	void (*proc)(int arg)
)
{
	int no;
	vpp_irqproc_t *irqproc;

	no = vpp_irqproc_get_no(vpp_int);
	if( no >= 32 ) 
		return 1;
	
	irqproc = kmalloc(sizeof(vpp_irqproc_t),GFP_KERNEL);
	vpp_irqproc_array[no] = irqproc;
	INIT_LIST_HEAD(&irqproc->list);
	tasklet_init(&irqproc->tasklet,vpp_irqproc_do_tasklet,vpp_int);
	irqproc->proc = proc;
	return 0;
} /* End of vpp_irqproc_new */

int vpp_irqproc_work
(
	vpp_int_t type,
	int (*func)(void *argc),
	void *arg,
	int wait
)
{
	int ret;
	vpp_proc_t *entry;
	struct list_head *ptr;
	vpp_irqproc_t *irqproc;
	int enable_interrupt = 0;

	// DPRINT("[VPP] vpp_irqproc_work(type 0x%x,wait %d)\n",type,wait);

	if( (vpp_free_list.next == 0) || list_empty(&vpp_free_list) ){
		if( func ) func(arg);
		return 0;
	}

	if( vppm_get_int_enable(type) == 0 ){
		vppm_set_int_enable(1,type);
		enable_interrupt = 1;
	}

	ret = 0;
	vpp_lock();
	
	ptr = vpp_free_list.next;
	entry = list_entry(ptr,vpp_proc_t,list);
	list_del_init(ptr);
	entry->func = func;
	entry->arg = arg;
	entry->type = type;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
    sema_init(&entry->sem,1);
#else
    init_MUTEX(&entry->sem);
#endif
	down(&entry->sem);
	
	irqproc = vpp_irqproc_get_entry(type);
	if( irqproc ){
		list_add_tail(&entry->list,&irqproc->list);
	}
	else {
		irqproc = vpp_irqproc_array[31];
		list_add_tail(&entry->list,&irqproc->list);
	}
	vpp_unlock();
	if( wait ) {
		ret = down_timeout(&entry->sem,HZ);
		if( ret ){
			DPRINT("[VPP] *W* vpp_irqproc_work timeout(type 0x%x)\n",type);
			list_del_init(ptr);
			list_add_tail(ptr,&vpp_free_list);
			if( func ) func(arg);
		}
	}

	if( enable_interrupt ){
		vppm_set_int_enable(0,type);
	}
	return ret;
} /* End of vpp_irqproc_work */

int vpp_irqproc_chg_path(int arg)
{
	vdo_framebuf_t *fb;
	govrh_mod_t *govr;

	govr = (govrh_mod_t *) g_vpp.govr;
	fb = &govr->fb_p->fb;
	govrh_set_reg_update(govr,0);
	govrh_set_color_format(govr,fb->col_fmt);
	govrh_set_csc_mode(govr,govr->fb_p->csc_mode);
	govrh_set_fb_info(govr,fb->fb_w, fb->img_w, fb->h_crop, fb->v_crop);
	govrh_set_fb_addr(govr,fb->y_addr, fb->c_addr);
	govrh_set_reg_update(govr,1);
//	DPRINT("[VPP] irqproc chg_path %d\n",arg);
	return 0;
}

#ifdef WMT_FTBLK_GOVRH
void vpp_irqproc_govrh_vbis(int arg)
{
	if( arg == 0 ){
		p_govrh->vbis_cnt++;
		vout_plug_detect(VPP_VOUT_NUM_HDMI);
#ifdef CONFIG_VPP_STREAM_CAPTURE
		vpp_mb_sync(VPP_MOD_GOVRH,p_govrh->vbis_cnt);
#endif
	}
}

void vpp_irqproc_govrh2_vbis(int arg)
{
	if( arg == 0 ){
		p_govrh2->vbis_cnt++;
#ifdef CONFIG_VPP_STREAM_CAPTURE
		vpp_mb_sync(VPP_MOD_GOVRH2,p_govrh2->vbis_cnt);
#endif		
	}
}

void vpp_irqproc_govrh_pvbi(int arg)
{
	int ret;

	if( arg == 0 ){
#ifdef WMT_FTBLK_GOVRH_CURSOR
		if( p_cursor->enable ){
			if( p_cursor->chg_flag ){
				p_cursor->hide_cnt = 0;
				govrh_CUR_set_enable(p_cursor,1);
			}
			else {
				p_cursor->hide_cnt++;
				if( p_cursor->hide_cnt > (GOVRH_CURSOR_HIDE_TIME * p_govrh->fb_p->framerate) ){
					govrh_CUR_set_enable(p_cursor,0);
				}
			}
		}

		if( p_cursor->chg_flag ){
			govrh_irqproc_set_position(p_cursor);
			p_cursor->chg_flag = 0;
		}
#endif

		if( g_vpp.govr != p_govrh )
			return;

		ret = vpp_disp_fb_isr(VPP_FB_PATH_GOVR);
		if( !g_vpp.vpp_path_no_ready ){
			switch( g_vpp.vpp_path ){
#ifdef CONFIG_SCL_DIRECT_PATH
				case VPP_VPATH_SCL:
					if(ret){
						g_vpp.disp_fb_cnt--;
						g_vpp.disp_cnt++;
					}
				case VPP_VPATH_GOVW_SCL:
					vpp_scl_fb_isr();
					break;
#endif
				case VPP_VPATH_GOVR:
				case VPP_VPATH_VPU:
					if( ret ){
						g_vpp.disp_fb_cnt--;
						g_vpp.disp_cnt++;
					}
					break;
				default:
					break;
			}
		}
	}
}

void vpp_irqproc_govrh2_pvbi(int arg)
{
	int ret;

	if( arg == 0 ){
		if( g_vpp.govr != p_govrh2 )
			return;
		
		ret = vpp_disp_fb_isr(VPP_FB_PATH_GOVR);
		if( !g_vpp.vpp_path_no_ready ){
			switch( g_vpp.vpp_path ){
				case VPP_VPATH_GOVR:
				case VPP_VPATH_VPU:
					if( ret ){
						g_vpp.disp_fb_cnt--;
						g_vpp.disp_cnt++;
					}
					break;
				default:
					break;
			}
		}
	}
}
#endif

void vpp_irqproc_govw_pvbi(int arg)
{
	if( arg == 0 ){
		if( !g_vpp.vpp_path_no_ready ){
			vpp_govw_int_routine();
			if( vpp_disp_fb_isr(VPP_FB_PATH_VPU) ){
				g_vpp.disp_fb_cnt--;
				g_vpp.disp_cnt++;
			}
			vpp_dei_dynamic_detect();
		}
	}

	if( arg == 1 ){
		g_vpp.govw_pvbi_cnt++;
	}
}

void vpp_irqproc_govw_vbis(int arg)
{
	if( arg == 0 ){
 #ifdef CONFIG_HW_VPU_HALT_GOVW_TG_ERR2
		static unsigned int w_ypxlwid;
 
 		if( vpp_govw_tg_err_parm == 2 ){
			vppif_reg32_write(VPU_R_YPXLWID,1);
			w_ypxlwid = vppif_reg32_read(VPU_W_YPXLWID);
			vppif_reg32_write(VPU_W_YPXLWID,1);
			vpp_govw_tg_err_parm = 3;
		}
		else if( vpp_govw_tg_err_parm == 3 ){
			unsigned int temp;
			
			vppif_reg32_write(GOVW_HD_MIF_ENABLE,1);

			temp = p_vpu->fb_p->fb.img_w;
			vppif_reg32_write(VPU_HXWIDTH,temp);
			vppif_reg32_write(VPU_R_YPXLWID,temp);
			vppif_reg32_write(VPU_W_YPXLWID,w_ypxlwid);
			temp = temp * 16 / p_vpu->resx_visual;
			vppif_reg32_write(VPU_H_STEP,temp);
			vppif_reg32_write(VPU_HBI_MODE,3);

			vpp_govw_tg_err_parm = 0;
//	if( (vpp_govw_tg_err_parm_cnt % 1000) == 0 ){
				DPRINT("[GOVW TG INT] 2/3 scale,VBIE,pxlw=1(VBIS),ret(VBIS),%d\n",vpp_govw_tg_err_parm_cnt);
//	}
			vpp_govw_tg_err_parm_cnt++;
			// vpp_reg_dump(VPU_BASE_ADDR,256);
		}
#endif
	}

	if( arg == 1 ){
		g_vpp.govw_vbis_cnt++;
		if( vpp_check_dbg_level(VPP_DBGLVL_INT) ){
		    static struct timeval pre_tv;
		    struct timeval tv;
		    unsigned int tm_usec,fps;

			do_gettimeofday(&tv);
			tm_usec=(tv.tv_sec==pre_tv.tv_sec)? (tv.tv_usec-pre_tv.tv_usec):(1000000*(tv.tv_sec-pre_tv.tv_sec)+tv.tv_usec-pre_tv.tv_usec);
			fps = 1000000 / tm_usec;
			if( fps < (p_govw->fb_p->framerate - 10) ){
				DPRINT("[VPP] *W* fps %d,ori %d\n",fps,p_govw->fb_p->framerate);
			}
			pre_tv = tv;
		}
	}
}
	
void vpp_irqproc_govw_vbie(int arg)
{
	if( arg == 0 ){
#ifdef CONFIG_HW_VPU_HALT_GOVW_TG_ERR
#ifdef CONFIG_HW_VPU_HALT_GOVW_TG_ERR1
		if( vpp_govw_tg_err_parm ){
			unsigned int resx;
			unsigned int temp;
			unsigned int w_ypxlwid;

			vppif_reg32_write(VPU_R_YPXLWID,1);
			w_ypxlwid = vppif_reg32_read(VPU_W_YPXLWID);
			vppif_reg32_write(VPU_W_YPXLWID,1);

			vppif_reg32_write(GOVW_HD_MIF_ENABLE,1);

			resx = p_vpu->fb_p->fb.img_w;
			if(resx % 2) resx -= 1;
			vppif_reg32_write(VPU_HXWIDTH,resx);
			vppif_reg32_write(VPU_R_YPXLWID,resx);
			vppif_reg32_write(VPU_W_YPXLWID,w_ypxlwid);
			temp = resx * 16 / p_vpu->resx_visual;
			vppif_reg32_write(VPU_H_STEP,temp);
			temp = (resx * 16) % p_vpu->resx_visual;
			vppif_reg32_write(VPU_H_SUBSTEP,temp);
			vppif_reg32_write(VPU_HBI_MODE,3);

			vpp_govw_tg_err_parm = 0;
			// if( (vpp_govw_tg_err_parm_cnt % 10000) == 0 ){
				DPRINT("[GOVW TG INT] 2/3 scale,ret(VBIE),%d,resx %d,vis %d\n"
							,vpp_govw_tg_err_parm_cnt,resx,p_vpu->resx_visual);
			//}
			vpp_govw_tg_err_parm_cnt++;
			// vpp_reg_dump(VPU_BASE_ADDR,256);
		}
#endif
#ifdef CONFIG_HW_VPU_HALT_GOVW_TG_ERR2
		if( vpp_govw_tg_err_parm == 1 ){
			vpp_govw_tg_err_parm = 2;
		}
#endif
		if( vpp_govm_path ){
			govm_set_in_path(vpp_govm_path,1);
			vpp_govm_path = 0;
			DPRINT("[GOVW TG INT] Off GOVM path,On(VBIE)\n");
		}
#endif
	}

	if( arg == 1 ){
		vpp_dbg_show(VPP_DBGLVL_DIAG,3,"GOVW VBIE");
	}
}

#if 0
int vpp_irqproc_enable_vpu_path_cnt;
int vpp_irqproc_enable_vpu_path(void *arg)
{
	vpp_irqproc_enable_vpu_path_cnt--;
	if( vpp_irqproc_enable_vpu_path_cnt )
		return 1;
	
	vppif_reg32_write(GOVM_VPU_SOURCE, 1);
	DPRINT("[VPP] irqproc enable vpu path\n");
	return 0;
}
#endif

int vpp_irqproc_enable_vpu(void *arg)
{
	vpu_set_tg_enable((vpp_flag_t)arg);
	return 0;
}

static irqreturn_t vpp_interrupt_routine
(
	int irq, 				/*!<; // irq id */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
	void *dev_id 			/*!<; // device id */
#else	
	void *dev_id, 			/*!<; // device id */
	struct pt_regs *regs	/*!<; // reg pointer */
#endif
)
{
	vpp_int_t int_sts;

	switch(irq){
#ifdef WMT_FTBLK_VPU
		case VPP_IRQ_VPU:
			int_sts = p_vpu->get_sts();
			p_vpu->clr_sts(int_sts);
            vpp_dbg_show_val1(VPP_DBGLVL_INT,0,"[VPP] VPU INT",int_sts);
			if( int_sts & VPP_INT_ERR_VPUW_MIFY )
				g_vpp.vpu_y_err_cnt++;
			if( int_sts & VPP_INT_ERR_VPUW_MIFC ){
				g_vpp.vpu_c_err_cnt++;
				if(p_vpu->underrun_cnt){
					p_vpu->underrun_cnt--;
					if(p_vpu->underrun_cnt==0){
						// g_vpp.govw_skip_all = 1;
						g_vpp.govw_fb_cnt = 1;
						g_vpp.vpu_skip_all = 1;
						DPRINT("[VPP] *E* skip all GOVW & VPU fb\n");
					}
				}
			}
			break;
#endif
		case VPP_IRQ_VPPM:	/* VPP */
			int_sts = p_vppm->get_sts();
			p_vppm->clr_sts(int_sts);

			{
				int i;
				unsigned int mask;
				vpp_irqproc_t *irqproc;

#if 1
				if( int_sts & VPP_INT_GOVW_PVBI ){
					if( int_sts & VPP_INT_GOVW_VBIS ){
						int_sts &= ~VPP_INT_GOVW_PVBI;
					}
				}
#endif
				for(i=0,mask=0x1;(i<32) && int_sts;i++,mask<<=1){
					if( (int_sts & mask) == 0 )
						continue;

					if( (irqproc = vpp_irqproc_array[i]) ){
						irqproc->proc(0);	// pre irq handle
						
						if( list_empty(&irqproc->list) == 0 )
							tasklet_schedule(&irqproc->tasklet);

						irqproc->proc(1);	// post irq handle
						
						if( vpp_check_dbg_level(VPP_DBGLVL_INT) ){
							if( mask == VPP_INT_GOVW_PVBI ){
								if( int_sts & VPP_INT_GOVW_VBIS ){
									DPRINT("[VPP] *W* pvbi over vbi (hw)\n");
								}
								else if( vppif_reg32_in(REG_VPP_INTSTS) & BIT1 ){
									DPRINT("[VPP] *W* pvbi over vbi (sw)\n");
								}
							}
						}
					}
					else {
						irqproc = vpp_irqproc_array[31];
						if( list_empty(&irqproc->list) == 0 ){
							vpp_proc_t *entry;
							struct list_head *ptr;

							ptr = (&irqproc->list)->next;
							entry = list_entry(ptr,vpp_proc_t,list);
							if( entry->type == mask )
								tasklet_schedule(&irqproc->tasklet);
						}							
					}
					int_sts &= ~mask;
				}
			}
			break;
#ifdef WMT_FTBLK_SCL
		case VPP_IRQ_SCL:	/* SCL */
			int_sts = p_scl->get_sts();
			p_scl->clr_sts(int_sts);
            vpp_dbg_show_val1(VPP_DBGLVL_INT,0,"[VPP] SCL INT",int_sts);
			break;
#endif			
		case VPP_IRQ_GOVM:	/* GOVM */
			int_sts = p_govm->get_sts();
			p_govm->clr_sts(int_sts);
			vpp_dbg_show_val1(VPP_DBGLVL_INT,0,"[VPP] GOVM INT",int_sts);
			break;
		case VPP_IRQ_GOVW:	/* GOVW */
			int_sts = p_govw->get_sts();
			vpp_dbg_show_val1(VPP_DBGLVL_INT,0,"[VPP] GOVW INT",int_sts);
			g_vpp.govw_tg_err_cnt++;			
			if( int_sts & VPP_INT_ERR_GOVW_TG ){
				int flag = 0;
				
				if( vppif_reg32_read(GOVM_INTSTS_VPU_READY) == 0 ){
					g_vpp.govw_vpu_not_ready_cnt++;
					flag = 1;
				}
				if( vppif_reg32_read(GOVM_INTSTS_GE_READY) == 0 ){
					g_vpp.govw_ge_not_ready_cnt++;
					flag = 1;
				}
#ifdef CONFIG_HW_VPU_HALT_GOVW_TG_ERR
				if( flag && (vpp_govm_path==0)&&(vpp_govw_tg_err_parm==0)){
					if( vppif_reg32_read(VPU_HBI_MODE) == 3){
						unsigned int temp;
						unsigned int resx;

						vppif_reg32_write(GOVW_HD_MIF_ENABLE,0);

						vppif_reg32_out(REG_VPU_HTB0,0x1C);
						vppif_reg32_out(REG_VPU_HPTR,0x2);
						vppif_reg32_out(REG_VPU_HRES_TB,0x2);
						resx = p_vpu->resx_visual;
						if( p_vpu->resx_visual % 2 ){
							resx *= 2;
						}
						temp = resx * 3 / 2;
						vppif_reg32_write(VPU_HXWIDTH,temp);
						vppif_reg32_write(VPU_R_YPXLWID,temp);
						temp = temp * 16 / resx;
						vppif_reg32_write(VPU_H_STEP,temp);
						vppif_reg32_write(VPU_HBI_MODE,0);
						vpp_govw_tg_err_parm = 1;
					}
					else {
						vpp_govw_tg_err_parm = 0;
						vpp_govm_path = vpp_get_govm_path();
						govm_set_in_path(vpp_govm_path,0);
#if 0
						vpp_vpu_sw_reset();
#endif
					}
				}
#endif				
				if( vpp_check_dbg_level(VPP_DBGLVL_INT) ){
					DPRINT("[VPP] GOVW TG err %d,GE %d,VPU %d\n",g_vpp.govw_tg_err_cnt,
						g_vpp.govw_ge_not_ready_cnt,g_vpp.govw_vpu_not_ready_cnt);
				}
			}
			p_govw->clr_sts(int_sts);
#ifdef CONFIG_VPP_GOVW_TG_ERR_DROP_FRAME
			if( vpp_disp_fb_cnt(&vpp_disp_fb_list) > 1 ){
				if( vpp_disp_fb_isr(VPP_FB_PATH_VPU) ){		// drop display frame when bandwidth not enouth
					g_vpp.disp_fb_cnt--;
				}
				g_vpp.disp_skip_cnt++;
				vpp_dbg_show(VPP_DBGLVL_DISPFB,0,"skip disp fb");
			}
#endif
#ifdef VPP_DBG_DIAG_NUM
			vpp_dbg_show(VPP_DBGLVL_DIAG,3,"GOVW Err");
			vpp_dbg_diag_delay = 10;
#endif
			g_vpp.govw_skip_frame = 3;
			break;
#ifdef WMT_FTBLK_GOVRH
		case VPP_IRQ_GOVR:	/* GOVR */
		case VPP_IRQ_GOVR2:
			{
				govrh_mod_t *govr;

				govr = (irq == VPP_IRQ_GOVR)? p_govrh:p_govrh2;
				int_sts = govr->get_sts();
				govr->clr_sts(int_sts);
				vpp_dbg_show_val1(VPP_DBGLVL_INT,0,"[VPP] GOVR INT",int_sts);
				govr->underrun_cnt++;
#ifdef VPP_DBG_DIAG_NUM
				vpp_dbg_show(VPP_DBGLVL_DIAG,3,"GOVR MIF Err");
				vpp_dbg_diag_delay = 10;
#endif
			}
			break;
#endif			
		default:
			DPRINT("*E* invalid vpp isr\n");
			break;
	}
	return IRQ_HANDLED;
} /* End of vpp_interrupt_routine */

void vpp_set_path(int arg,vdo_framebuf_t *fb)
{
	govrh_mod_t *govr;

	if( g_vpp.vpp_path == arg )
		return;

	vpp_mod_set_clock(VPP_MOD_GOVW,VPP_FLAG_ENABLE,1);
	vpp_mod_set_clock(VPP_MOD_VPU,VPP_FLAG_ENABLE,1);

	govr = (govrh_mod_t *) g_vpp.govr;
	g_vpp.vpp_path_no_ready = 1;

	DPRINT("[VPP] vpp_set_path(%s-->%s)\n",vpp_vpath_str[g_vpp.vpp_path],vpp_vpath_str[arg]);
	vpp_disp_fb_clr(0,1);

	// pre video path process
	switch( g_vpp.vpp_path ){
		case VPP_VPATH_GOVW:	// VPU,GE fb -> Queue -> VPU,GE -> GOVW -> GOVW fb -> GOVR fb -> GOVR
			// govw disable
			vpp_set_govw_tg(VPP_FLAG_DISABLE);
			break;
#ifdef CONFIG_SCL_DIRECT_PATH
		case VPP_VPATH_SCL:		// VPU fb -> Queue -> SCL -> SCL fb -> Queue -> GOVR fb -> GOVR
			if( g_vpp.scl_fb_mb_on_work ){
				p_scl->scale_finish();
				scl_scale_complete = 0;
				g_vpp.scl_fb_mb_on_work = 0;
				DPRINT("[VPP] wait scl complete\n");
			}
#endif
		case VPP_VPATH_GOVR:	// VPU fb -> Queue -> GOVR fb -> GOVR
		case VPP_VPATH_GE:		// GE fb -> pan display -> GOVR fb -> GOVR
			break;
		case VPP_VPATH_VPU:		// VPU fb -> Queue -> VPU -> GOVR
			govr = p_govrh2;
#ifdef CONFIG_VPU_DIRECT_PATH
			vpu_set_direct_path(arg);
			govrh_set_direct_path(govr,VPP_FLAG_DISABLE);
			// govw_set_tg_enable(VPP_FLAG_ENABLE);
			govrh_set_MIF_enable(govr,VPP_FLAG_ENABLE);
			vpp_set_vppm_int_enable(VPP_INT_GOVRH_PVBI+VPP_INT_GOVRH_VBIS,0);
			vpp_set_vppm_int_enable(VPP_INT_GOVRH2_PVBI+VPP_INT_GOVRH2_VBIS,0);
			vppm_set_NA_mode(0);
#endif
			break;
		default:
			break;
	}

	g_vpp.vpp_path = arg;

	// new video path process
	switch( g_vpp.vpp_path ){
#ifdef CONFIG_SCL_DIRECT_PATH
		case VPP_VPATH_SCL:
			g_vpp.scl_fb_mb_clear = 0xFF;
			g_vpp.scl_fb_mb_index = (govr->fb_p->fb.y_addr == g_vpp.mb_govw[0])? 1:2;
#endif
		case VPP_VPATH_GOVR:
		case VPP_VPATH_GE:
#ifdef CONFIG_VPP_GOVRH_FBSYNC
			vpp_fbsync_cal_fps();
#endif
			vpp_set_vppm_int_enable(VPP_INT_GOVRH_PVBI+VPP_INT_GOVRH_VBIS+VPP_INT_GOVRH2_VBIS,VPP_FLAG_ENABLE);

			// set ge fb to govrh
			if( fb ){
				if( (fb->img_w == govr->fb_p->fb.img_w) && (fb->img_h == govr->fb_p->fb.img_h) ){
					govr->fb_p->fb = *fb;
					vpp_irqproc_work((govr->mod == VPP_MOD_GOVRH)? VPP_INT_GOVRH_PVBI:VPP_INT_GOVRH2_PVBI,(void *)vpp_irqproc_chg_path,0,1);
				}
			}
			break;
		case VPP_VPATH_VPU:
#ifdef CONFIG_VPU_DIRECT_PATH
			if( govr != p_govrh2 ){
				DBG_ERR("VPU dirpath only govrh2\n");
				govr = p_govrh2;
			}
#ifdef CONFIG_VPP_GOVRH_FBSYNC
			vpp_fbsync_cal_fps();
#endif
			vpu_set_direct_path(VPP_VPATH_VPU);
			govrh_set_direct_path(govr,VPP_FLAG_ENABLE);
			govrh_set_MIF_enable(govr,VPP_FLAG_DISABLE);
			vppm_set_NA_mode(1);
			vpp_set_vppm_int_enable(VPP_INT_GOVRH_PVBI+VPP_INT_GOVRH_VBIS,1);
			vpp_set_vppm_int_enable(VPP_INT_GOVRH2_PVBI+VPP_INT_GOVRH2_VBIS,1);
#endif
			break;
#ifdef CONFIG_GOVW_SCL_PATH
		case VPP_VPATH_GOVW_SCL:
			{
				int size;

				size = vpp_calc_fb_width(p_govw->fb_p->fb.col_fmt,p_govw->fb_p->fb.fb_w * p_govw->fb_p->fb.fb_h);
				g_vpp.govw_fb_pool.mb = g_vpp.govw_fb_mb;
				vpp_fb_pool_init(&g_vpp.govw_fb_pool,size,3);
			}
#endif			
		case VPP_VPATH_GOVW:
			g_vpp.govw_skip_all = 1;

			govr->fb_p->fb = p_govw->fb_p->fb;
			vppif_reg32_write(GOVM_VPU_SOURCE,0);

			// govw enable
			vpp_lock();
			vpp_set_govw_tg(VPP_FLAG_ENABLE);
			vpp_unlock();

			vpp_irqproc_work(VPP_INT_GOVW_VBIE,0,0,1);
			vpp_vpu_sw_reset();
			vpp_irqproc_work(VPP_INT_GOVW_VBIE,0,0,1);
			vppif_reg32_write(GOVM_VPU_SOURCE,1);

			// update govrh fb
			govw_set_fb_addr(govr->fb_p->fb.y_addr, govr->fb_p->fb.c_addr);
			vpp_irqproc_work(VPP_INT_GOVW_VBIE,0,0,1);
			
			// update govw fb
			govw_set_fb_addr(p_govw->fb_p->fb.y_addr, p_govw->fb_p->fb.c_addr);
			vpp_irqproc_work(VPP_INT_GOVW_VBIE,0,0,1);

			vpp_irqproc_work((govr->mod == VPP_MOD_GOVRH)? VPP_INT_GOVRH_PVBI:VPP_INT_GOVRH2_PVBI,(void *)vpp_irqproc_chg_path,0,1);
#ifdef CONFIG_GOVW_SCL_PATH
			if( g_vpp.vpp_path != VPP_VPATH_GOVW_SCL ){
				vpp_set_vppm_int_enable(VPP_INT_GOVRH_PVBI+VPP_INT_GOVRH_VBIS+VPP_INT_GOVRH2_VBIS,VPP_FLAG_ENABLE);
			}
			else
#endif
			vpp_set_vppm_int_enable(VPP_INT_GOVRH_PVBI+VPP_INT_GOVRH_VBIS+VPP_INT_GOVRH2_VBIS,VPP_FLAG_DISABLE);
			g_vpp.govw_skip_all = 0;
			break;
		default:
			break;
	}
	g_vpp.vpp_path_no_ready = 0;
	vpp_mod_set_clock(VPP_MOD_GOVW,VPP_FLAG_DISABLE,1);
	vpp_mod_set_clock(VPP_MOD_VPU,VPP_FLAG_DISABLE,1);
}

void vpp_free_framebuffer(void)
{
	vpp_lock();
	mb_free(g_vpp.mb[0]);
	g_vpp.mb[0] = 0;
	vpp_unlock();
}

int vpp_alloc_framebuffer(unsigned int resx,unsigned int resy)
{
	vdo_framebuf_t *fb;
	unsigned int y_size;
	unsigned int fb_size;
	unsigned int colfmt;
	unsigned int resx_fb;
	int y_bpp,c_bpp;
	int i;

	vpp_lock();

	// alloc govw & govrh frame buffer
	if( g_vpp.mb[0] == 0 ){
		unsigned int mb_resx,mb_resy;
		int fb_num;
		char buf[100];
		int varlen = 100;
		unsigned int phy_base;

		if( wmt_getsyspara("wmt.display.mb",(unsigned char *)buf,&varlen) == 0 ){
			unsigned int parm[10];
			
			vpp_parse_param(buf,(unsigned int *)parm,4,0);
			MSG("boot parm mb (%d,%d),bpp %d,fb %d\n",parm[0],parm[1],parm[2],parm[3]);
			mb_resx = parm[0];
			mb_resy = parm[1];
			y_bpp = parm[2] * 8;
			c_bpp = 0;
			fb_num = parm[3];
		}
		else {
			mb_resx = VPP_HD_MAX_RESX;
			mb_resy = VPP_HD_MAX_RESY;
			colfmt = g_vpp.mb_colfmt;
			vpp_get_colfmt_bpp(colfmt,&y_bpp,&c_bpp);
			fb_num = VPP_MB_ALLOC_NUM;
		}
		mb_resx = vpp_calc_align(mb_resx,VPP_FB_ADDR_ALIGN/(y_bpp/8));
		y_size = mb_resx * mb_resy * y_bpp / 8;
		fb_size = mb_resx * mb_resy * (y_bpp+c_bpp) / 8;
		g_vpp.mb_fb_size = fb_size;
		g_vpp.mb_y_size = y_size;
		if( (phy_base = mb_alloc(fb_size*fb_num)) ){
			DPRINT("[VPP] alloc base 0x%x,%d\n",phy_base,fb_size*fb_num);
			for(i=0;i<fb_num;i++){
				g_vpp.mb[i] = (unsigned int) (phy_base+(fb_size*i));
				DPRINT("[VPP] alloc mb 0x%x,fb %d,y %d\n",g_vpp.mb[i],fb_size,y_size);
			}
		}
		else {
			DBG_ERR("alloc fail\n");
		}
	}

	// assign mb to govw mb for GOVW path
	{
		int index = 0,offset = 0;
		unsigned int size = g_vpp.mb_fb_size;

		colfmt = VPP_GOVW_FB_COLFMT;
		vpp_get_colfmt_bpp(colfmt,&y_bpp,&c_bpp);
		resx_fb = vpp_calc_fb_width(colfmt,resx);
		y_size = resx_fb * resy * y_bpp / 8;
		fb_size = vpp_calc_align(resx_fb,64) * resy * (y_bpp+c_bpp)/ 8; 
		g_vpp.mb_govw_y_size = y_size;
		g_vpp.mb_govw_cnt = VPP_GOV_MB_ALLOC_NUM;
		for(i=0;i<VPP_GOV_MB_ALLOC_NUM;i++){
			if( size < fb_size ){
				index++;
				if( index >= VPP_MB_ALLOC_NUM ){
					index = 0;
					g_vpp.mb_govw_cnt = i;
					break;
				}
				offset=0;
				size = g_vpp.mb_fb_size;
			}
			g_vpp.mb_govw[i] = g_vpp.mb[index] + offset;
			size -= fb_size;
			offset += fb_size;
			DBG_DETAIL("govw mb %d 0x%x\n",i,g_vpp.mb_govw[i]);
		}
	}

	fb = &p_govw->fb_p->fb;
#ifdef CONFIG_GOVW_SCL_PATH	
	if( g_vpp.vpp_path == VPP_VPATH_GOVW_SCL ){
		int size;
		int y_size;

		y_size = p_govw->fb_p->fb.fb_w * p_govw->fb_p->fb.fb_h;
		vpp_get_colfmt_bpp(p_govw->fb_p->fb.col_fmt,&y_bpp,&c_bpp);
		size = y_size * (y_bpp+c_bpp) / 8;
		g_vpp.govw_fb_pool.mb = g_vpp.govw_fb_mb;
		vpp_fb_pool_init(&g_vpp.govw_fb_pool,size,3);
		if( (fb->y_addr = vpp_fb_pool_get(&g_vpp.govw_fb_pool)) == 0 ){
			return 0;
		}
		vpp_fb_pool_put(&g_vpp.govw_fb_pool,fb->y_addr);
		fb->c_addr = vpp_calc_align(fb->y_addr + y_size,VPP_FB_ADDR_ALIGN);
		fb->y_size = y_size;
		fb->c_size = size - y_size;
	}
	else
#endif
	{
		fb->y_addr = g_vpp.mb_govw[0];
		fb->c_addr = g_vpp.mb_govw[0] + y_size;
		fb->y_size = y_size;
		fb->c_size = fb_size - y_size;
		fb->fb_w = resx_fb;
		fb->fb_h = resy;
		fb->img_w = resx;
		fb->img_h = resy;
	}
	vpp_unlock();
	return 0;
} /* End of vpp_alloc_framebuffer */

/*----------------------- Linux Kernel interface --------------------------------------*/
int vpp_dev_init(void)
{
	int i;

	g_vpp.alloc_framebuf = vpp_alloc_framebuffer;

	// init module
//	p_scl->int_catch = VPP_INT_ERR_SCL_TG | VPP_INT_ERR_SCLR1_MIF | VPP_INT_ERR_SCLR2_MIF 
//						| VPP_INT_ERR_SCLW_MIFRGB | VPP_INT_ERR_SCLW_MIFY |	VPP_INT_ERR_SCLW_MIFC;
	p_vppm->int_catch = VPP_INT_GOVW_PVBI | VPP_INT_GOVW_VBIS | VPP_INT_GOVW_VBIE 
						| VPP_INT_GOVRH_PVBI | VPP_INT_GOVRH_VBIS | VPP_INT_GOVRH2_VBIS;

#ifdef CONFIG_VPP_GOVW_TG_ERR_DROP_FRAME			
	p_govw->int_catch = VPP_INT_ERR_GOVW_TG; // | VPP_INT_ERR_GOVW_MIFY | VPP_INT_ERR_GOVW_MIFC;
#endif
//	p_govm->int_catch = VPP_INT_ERR_GOVM_VPU | VPP_INT_ERR_GOVM_GE;

	// init disp fb queue
	INIT_LIST_HEAD(&vpp_disp_free_list);
	INIT_LIST_HEAD(&vpp_disp_fb_list);
	INIT_LIST_HEAD(&vpp_vpu_fb_list);
	for(i=0;i<VPP_DISP_FB_MAX;i++)
		list_add_tail(&vpp_disp_fb_array[i].list,&vpp_disp_free_list);

#ifdef CONFIG_SCL_DIRECT_PATH 
	vpp_scl_fb_init();
#endif

	// init irq proc	
	INIT_LIST_HEAD(&vpp_free_list);

	vpp_irqproc_new(VPP_INT_MAX,0);
#ifdef WMT_FTBLK_GOVRH
	vpp_irqproc_new(VPP_INT_GOVRH_PVBI,vpp_irqproc_govrh_pvbi);
	vpp_irqproc_new(VPP_INT_GOVRH_VBIS,vpp_irqproc_govrh_vbis);
	vpp_irqproc_new(VPP_INT_GOVRH2_PVBI,vpp_irqproc_govrh2_pvbi);
	vpp_irqproc_new(VPP_INT_GOVRH2_VBIS,vpp_irqproc_govrh2_vbis);
#endif
	vpp_irqproc_new(VPP_INT_GOVW_PVBI,vpp_irqproc_govw_pvbi);
	vpp_irqproc_new(VPP_INT_GOVW_VBIS,vpp_irqproc_govw_vbis);
	vpp_irqproc_new(VPP_INT_GOVW_VBIE,vpp_irqproc_govw_vbie);

	for(i=0;i<VPP_PROC_NUM;i++)
		list_add_tail(&vpp_proc_array[i].list,&vpp_free_list);

	vpp_init();

#ifdef CONFIG_VPP_PROC
	// init system proc
	if( vpp_proc_dir == 0 ){
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
		struct proc_dir_entry *res;
		
		vpp_proc_dir = proc_mkdir("driver/vpp", NULL);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
		vpp_proc_dir->owner = THIS_MODULE;
#endif
		
		res=create_proc_entry("sts", 0, vpp_proc_dir);
	    if (res) {
    	    res->read_proc = vpp_sts_read_proc;
	    }
		res=create_proc_entry("reg", 0, vpp_proc_dir);
	    if (res) {
    	    res->read_proc = vpp_reg_read_proc;
	    }
		res=create_proc_entry("info", 0, vpp_proc_dir);
	    if (res) {
    	    res->read_proc = vpp_info_read_proc;
	    }
		vpp_table_header = register_sysctl_table(vpp_root_table);
#else
		
		vpp_proc_dir = proc_mkdir("driver/vpp", NULL);
		vpp_proc_dir->owner = THIS_MODULE;
		create_proc_info_entry("sts", 0, vpp_proc_dir, vpp_sts_read_proc);
		create_proc_info_entry("reg", 0, vpp_proc_dir, vpp_reg_read_proc);
		create_proc_info_entry("info", 0, vpp_proc_dir, vpp_info_read_proc);		

		vpp_table_header = register_sysctl_table(vpp_root_table, 1);
#endif	
	}
#endif

#ifdef CONFIG_VPP_NOTIFY
	vpp_netlink_proc[0].pid=0;
	vpp_netlink_proc[1].pid=0;	
	vpp_nlfd = netlink_kernel_create(&init_net,NETLINK_CEC_TEST, 0, vpp_netlink_receive,NULL,THIS_MODULE);
	if(!vpp_nlfd){
		DPRINT(KERN_ERR "can not create a netlink socket\n");
	}
#endif

	// init interrupt service routine
#ifdef WMT_FTBLK_SCL
	if ( vpp_request_irq(VPP_IRQ_SCL, vpp_interrupt_routine, SA_INTERRUPT, "scl", (void *)&g_vpp) ) {
		DPRINT("*E* request VPP ISR fail\n");
		return -1;
	}
#endif

	if ( vpp_request_irq(VPP_IRQ_VPPM, vpp_interrupt_routine, SA_INTERRUPT, "vpp", (void *)&g_vpp) ) {
		DPRINT("*E* request VPP ISR fail\n");
		return -1;
	}

	if ( vpp_request_irq(VPP_IRQ_GOVM, vpp_interrupt_routine, SA_INTERRUPT, "govm", (void *)&g_vpp) ) {
		DPRINT("*E* request VPP ISR fail\n");
		return -1;
	}

	if ( vpp_request_irq(VPP_IRQ_GOVW, vpp_interrupt_routine, SA_INTERRUPT, "govw", (void *)&g_vpp) ) {
		DPRINT("*E* request VPP ISR fail\n");
		return -1;
	}

#ifdef WMT_FTBLK_GOVRH
	if ( vpp_request_irq(VPP_IRQ_GOVR, vpp_interrupt_routine, SA_INTERRUPT, "govr", (void *)&g_vpp) ) {
		DPRINT("*E* request VPP ISR fail\n");
		return -1;
	}

	if ( vpp_request_irq(VPP_IRQ_GOVR2, vpp_interrupt_routine, SA_INTERRUPT, "govr2", (void *)&g_vpp) ) {
		DPRINT("*E* request VPP ISR fail\n");
		return -1;
	}
#endif	
#ifdef WMT_FTBLK_VPU
	if ( vpp_request_irq(VPP_IRQ_VPU, vpp_interrupt_routine, SA_INTERRUPT, "vpu", (void *)&g_vpp) ) {
		DPRINT("*E* request VPU ISR fail\n");
		return -1;
	}
#endif
	return 0;
}
module_init(vpp_dev_init);

int vpp_exit(struct fb_info *info)
{
	DBG_MSG("vpp_exit\n");

	vout_exit();
#ifdef CONFIG_VPP_PROC
	unregister_sysctl_table(vpp_table_header);
#endif
	return 0;
}

#ifdef CONFIG_PM
unsigned int vpp_pmc_258_bk;
unsigned int vpp_pmc_25c_bk;
unsigned int vpp_vout_blank_mask;
int	vpp_suspend(int state)
{
	vpp_mod_base_t *mod_p;
	vout_t *vo;		
	int i;

	MSG("vpp_suspend\n");
	vpp_vout_blank_mask = 0;
	for(i=0;i<=VPP_VOUT_NUM;i++){
		if( (vo = vout_get_entry(i)) ){
			vpp_vout_blank_mask |= (0x1 << i);
		}
	}
	vout_set_blank(VPP_VOUT_ALL,VOUT_BLANK_POWERDOWN);

	// disable module
	for(i=0;i<VPP_MOD_MAX;i++){
		mod_p = vpp_mod_get_base(i);
		if( mod_p && mod_p->suspend ){
			mod_p->suspend(0);
		}
	}
#ifdef WMT_FTBLK_HDMI
	hdmi_suspend(0);
#endif
#ifdef WMT_FTBLK_LVDS
	lvds_suspend(0);
#endif
	// wait
	mdelay(100);

	// disable tg
	for(i=0;i<VPP_MOD_MAX;i++){
		mod_p = vpp_mod_get_base(i);
		if( mod_p && mod_p->suspend ){
			mod_p->suspend(1);
		}
	}
#ifdef WMT_FTBLK_HDMI
	hdmi_suspend(1);
#endif
#ifdef WMT_FTBLK_LVDS
	lvds_suspend(1);
#endif	
	// backup registers
	for(i=0;i<VPP_MOD_MAX;i++){
		mod_p = vpp_mod_get_base(i);
		if( mod_p && mod_p->suspend ){
			mod_p->suspend(2);
		}
	}
#ifdef WMT_FTBLK_HDMI
	hdmi_suspend(2);
#endif
#ifdef WMT_FTBLK_LVDS
	lvds_suspend(2);
#endif
	return 0;
} /* End of vpp_suspend */

int	vpp_resume(void)
{
	vpp_mod_base_t *mod_p;
	int i;

	MSG("vpp_resume\n");

	// restore registers
	for(i=0;i<VPP_MOD_MAX;i++){
		mod_p = vpp_mod_get_base(i);
		if( mod_p && mod_p->resume ){
			mod_p->resume(0);
		}
	}
#ifdef WMT_FTBLK_LVDS
	lvds_resume(0);
#endif
#ifdef WMT_FTBLK_HDMI
	hdmi_resume(0);
#endif
	// enable tg
	for(i=0;i<VPP_MOD_MAX;i++){
		mod_p = vpp_mod_get_base(i);
		if( mod_p && mod_p->resume ){
			mod_p->resume(1);
		}
	}
#ifdef WMT_FTBLK_LVDS
	lvds_resume(1);
#endif
#ifdef WMT_FTBLK_HDMI
	hdmi_resume(1);
#endif
	// wait
        //--> removed by howayhuo. The GPIO10 is used by Gensis Led
	//REG32_VAL(GPIO_BASE_ADDR+0x80) |= 0x400; // for ASUS GPIO10	
	//REG32_VAL(GPIO_BASE_ADDR+0xC0) |= 0x400;
        //<-- end removed
	msleep(150);

	// enable module
	for(i=0;i<VPP_MOD_MAX;i++){
		mod_p = vpp_mod_get_base(i);
		if( mod_p && mod_p->resume ){
			mod_p->resume(2);
		}
	}
#ifdef WMT_FTBLK_LVDS
	lvds_resume(2);
#endif
#ifdef WMT_FTBLK_HDMI
	hdmi_resume(2);
#endif
	vout_set_blank(vpp_vout_blank_mask,VOUT_BLANK_UNBLANK);

//--> added by howayhuo
	if( g_vpp.virtual_display && cs8556_tvformat_is_set())
	{
		if(!hdmi_get_plugin())
		{
			vpp_netlink_notify(USER_PID,DEVICE_PLUG_IN,0);
			vpp_netlink_notify(WP_PID,DEVICE_PLUG_IN,0);
			vpp_netlink_notify(USER_PID,DEVICE_PLUG_IN,1);
			vpp_netlink_notify(WP_PID,DEVICE_PLUG_IN,1);
		}
	}
//<-- end add

	return 0;
} /* End of vpp_resume */
#endif

int vpp_check_mmap_offset(dma_addr_t offset)
{
	vdo_framebuf_t *fb;
	int i;

//	DBGMSG("vpp_check_mmap_offset 0x%x\r\n",offset);
	if( offset == 0 )
		return -1;

	for(i=0;i<VPP_MOD_MAX;i++){
		fb = vpp_mod_get_framebuf(i);
		if( fb ){
			if( (offset >= fb->y_addr) && (offset < (fb->y_addr + fb->y_size))){
//				DBGMSG("mmap to mod %d Y frame buffer\r\n",i);
				return 0;
			}

			if( (offset >= fb->c_addr) && (offset < (fb->c_addr + fb->c_size))){
//				DBGMSG("mmap to mod %d C frame buffer\r\n",i);
				return 0;
			}
		}
	}
	return -1;
} /* End of vpp_check_mmap_offset */

int vpp_mmap(struct vm_area_struct *vma)
{
//	DBGMSG("vpp_mmap\n");

	/* which buffer need to remap */
	if( vpp_check_mmap_offset(vma->vm_pgoff << PAGE_SHIFT) != 0 ){
//		DPRINT("*E* vpp_mmap 0x%x\n",(int) vma->vm_pgoff << PAGE_SHIFT);
		return -EINVAL;
	}

//	DBGMSG("Enter vpp_mmap remap 0x%x\n",(int) (vma->vm_pgoff << PAGE_SHIFT));
	
	vma->vm_flags |= VM_IO | VM_RESERVED;
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,vma->vm_end - vma->vm_start, vma->vm_page_prot))
		return -EAGAIN;

	return 0;
} /* End of vpp_mmap */

void vpp_wait_vsync(int no,int cnt)
{
	vout_info_t *vo_info;
	vpp_int_t type;

	vo_info = vout_info_get_entry(no);
	if( vo_info->govr == 0 )
		return;

	if( g_vpp.virtual_display ){
		type = (govrh_get_MIF_enable(p_govrh))? VPP_INT_GOVRH_VBIS:VPP_INT_GOVRH2_VBIS;
	}
	else {
		type = (vo_info->govr->mod == VPP_MOD_GOVRH)? VPP_INT_GOVRH_VBIS:VPP_INT_GOVRH2_VBIS;
	}
	
	if( (no == 0) && (vppif_reg32_read(GOVW_TG_ENABLE)) ){
		type = VPP_INT_GOVW_VBIS;
	}

	while(cnt){
		vpp_irqproc_work(type,0,0,1);
		cnt--;
	}
} /* End of vpp_wait_vsync */

int vpp_common_ioctl(unsigned int cmd,unsigned long arg)
{
	vpp_mod_base_t *mod_p;
	vpp_fb_base_t *mod_fb_p;
	int retval = 0;

	switch(cmd){
		case VPPIO_VPPGET_INFO:
			{
			int i;
			vpp_cap_t parm;

			parm.chip_id = vpp_get_chipid();
			parm.version = 0x01;
			parm.resx_max = VPP_HD_MAX_RESX;
			parm.resy_max = VPP_HD_MAX_RESY;
			parm.pixel_clk = 400000000;
			parm.module = 0x0;
			for(i=0;i<VPP_MOD_MAX;i++){
				mod_p = vpp_mod_get_base(i);
				if( mod_p ){
					parm.module |= (0x01 << i);
				}
			}
			parm.option = VPP_CAP_DUAL_DISPLAY;
			copy_to_user( (void *)arg, (void *) &parm, sizeof(vpp_cap_t));
			}
			break;
		case VPPIO_VPPSET_INFO:
			{
			vpp_cap_t parm;

			copy_from_user((void *)&parm,(const void *)arg,sizeof(vpp_cap_t));
			}
			break;
		case VPPIO_I2CSET_BYTE:
			{
			vpp_i2c_t parm;
			unsigned int id;

			copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_i2c_t));
			id = (parm.addr & 0x0000FF00) >> 8;
			vpp_i2c_write(id,(parm.addr & 0xFF),parm.index,(char *)&parm.val,1);
			}
			break;
		case VPPIO_I2CGET_BYTE:
			{
			vpp_i2c_t parm;
			unsigned int id;
			int len;

			copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_i2c_t));
			id = (parm.addr & 0x0000FF00) >> 8;
			len = parm.val;
			{
				unsigned char buf[len];
				
				vpp_i2c_read(id,(parm.addr & 0xFF),parm.index,buf,len);
				parm.val = buf[0];
			}
			copy_to_user( (void *)arg, (void *) &parm, sizeof(vpp_i2c_t));
			}
			break;
		case VPPIO_VPPSET_DIRECTPATH:
			// DPRINT("[VPP] set vpu path %s\n",vpp_vpath_str[arg]);
			switch( arg ){
#ifdef CONFIG_VPU_DIRECT_PATH
				case VPP_VPATH_VPU:
					g_vpp.vpu_path = VPP_VPATH_VPU;
					g_vpp.govr = p_govrh2;
					break;
#endif
				default:
					g_vpp.vpu_path = VPP_VPATH_GOVW;
					break;
			}
			
			{
				vpp_video_path_t vpath;

				vpath = (vpp_get_govm_path() & VPP_PATH_GOVM_IN_VPU)? g_vpp.vpu_path:g_vpp.ge_path;
				vpp_pan_display(0,0,(vpath == g_vpp.ge_path));
			}
			break;
		case VPPIO_VPPSET_FBDISP:
			{
				vpp_dispfb_t parm;
				vpp_fb_path_t path;

				copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_dispfb_t));

				if( vpp_check_dbg_level(VPP_DBGLVL_IOCTL) ){
//					DPRINT("[VPP] set fbdisp, flag 0x%x\n",parm.flag);
				}

				switch( g_vpp.vpp_path ){
					case VPP_VPATH_SCL:
						path = VPP_FB_PATH_SCL;
						break;
					case VPP_VPATH_GOVR:
						path = VPP_FB_PATH_DIRECT_GOVR;
						break;
#ifdef CONFIG_GOVW_SCL_PATH
					case VPP_VPATH_GOVW_SCL:
						path = VPP_FB_PATH_VPU;
						break;
#endif
					default:
						if( parm.flag & VPP_FLAG_DISPFB_PIP ){
							path = VPP_FB_PATH_PIP;
						}
						else {
							path = VPP_FB_PATH_VPU;
						}
						break;
				}
				retval = vpp_disp_fb_add(&parm,path);
				if( retval ){
					vpp_dbg_show(VPP_DBGLVL_DISPFB,1,"add disp fb full");
				}
				else {
					// vpp_set_dbg_gpio(4,0xFF);
				}
			}
			break;
		case VPPIO_VPPGET_FBDISP:
			{
				vpp_dispfb_info_t parm;

				parm.queue_cnt = g_vpp.disp_fb_max;
				parm.cur_cnt = g_vpp.disp_fb_cnt;
				parm.isr_cnt = g_vpp.disp_fb_isr_cnt;
				parm.disp_cnt = g_vpp.disp_cnt;
				parm.skip_cnt = g_vpp.disp_skip_cnt;
				parm.full_cnt = g_vpp.disp_fb_full_cnt;

				g_vpp.disp_fb_isr_cnt = 0;
				g_vpp.disp_cnt = 0;
				g_vpp.disp_skip_cnt = 0;
				g_vpp.disp_fb_full_cnt = 0;
				copy_to_user( (void *)arg, (void *) &parm, sizeof(vpp_dispfb_info_t));
			}
			break;
		case VPPIO_WAIT_FRAME:
			vpp_wait_vsync(0,arg);
			break;
		case VPPIO_MODULE_FRAMERATE:
			{
				vpp_mod_arg_t parm;

				copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_mod_arg_t));
				if( !(mod_fb_p = vpp_mod_get_fb_base(parm.mod)) )
					break;
				if( parm.read ){
					parm.arg1 = mod_fb_p->framerate;
					copy_to_user( (void *)arg, (void *) &parm, sizeof(vpp_mod_arg_t));
				}
				else {
					mod_fb_p->framerate = parm.arg1;
					if( parm.mod == VPP_MOD_GOVW ){
#ifdef CONFIG_VPP_GOVRH_FBSYNC
							vpp_fbsync_cal_fps();
#endif
							mod_fb_p->set_framebuf(&mod_fb_p->fb);
					}
				}
			}
			break;
		case VPPIO_MODULE_ENABLE:
			{
				vpp_mod_arg_t parm;
				
				copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_mod_arg_t));
				if(!(mod_p = vpp_mod_get_base(parm.mod)))
					break;
				if( parm.read ){
					
				}
				else {
					mod_p->set_enable(parm.arg1);
					if( parm.mod == VPP_MOD_CURSOR ){
						vpp_set_vppm_int_enable(VPP_INT_GOVRH_PVBI,parm.arg1);
						p_cursor->enable = parm.arg1;
					}
				}
			}
			break;
		case VPPIO_MODULE_TIMING:
			{
				vpp_mod_timing_t parm;
				vpp_clock_t clock;

				copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_mod_timing_t));
				if( !(mod_p = vpp_mod_get_base(parm.mod)) )
					break;
				if( parm.read ){
					mod_p->get_tg(&clock);
					vpp_trans_timing(parm.mod,&parm.tmr,&clock,0);
					copy_to_user( (void *)arg, (void *) &parm, sizeof(vpp_mod_timing_t));
				}
				else {
					if( g_vpp.govr->mod == parm.mod )
						vpp_alloc_framebuffer(parm.tmr.hpixel,parm.tmr.vpixel);
					vpp_mod_set_timing(parm.mod,&parm.tmr);
				}
			}
			break;
		case VPPIO_MODULE_FBADDR:
			{
				vpp_mod_arg_t parm;

				copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_mod_arg_t));
				if( !(mod_fb_p = vpp_mod_get_fb_base(parm.mod)) )
					break;
				mod_p = vpp_mod_get_base(parm.mod);
				if( parm.read ){
					mod_fb_p->get_addr(&parm.arg1,&parm.arg2);
					copy_to_user( (void *)arg, (void *) &parm, sizeof(vpp_mod_arg_t));
				}
				else {
					mod_fb_p->set_addr(parm.arg1,parm.arg2);
				}
			}
			break;
		case VPPIO_MODULE_FBINFO:
			{
				vpp_mod_fbinfo_t parm;

				copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_mod_fbinfo_t));
				if( !(mod_fb_p = vpp_mod_get_fb_base(parm.mod)) )
					break;
				mod_p = vpp_mod_get_base(parm.mod);
				if( parm.read ){
					parm.fb = mod_fb_p->fb;
					switch(parm.mod){
						case VPP_MOD_GOVRH:
						case VPP_MOD_GOVRH2:
							govrh_get_framebuffer((govrh_mod_t *)mod_p,&parm.fb);
							break;
						default:
							break;
					}
					copy_to_user( (void *)arg, (void *) &parm, sizeof(vpp_mod_fbinfo_t));
				}
				else {
					mod_fb_p->fb = parm.fb;
					mod_fb_p->set_framebuf(&parm.fb);
				}
			}
			break;
		case VPPIO_MODULE_VIEW:
			{
				vpp_mod_view_t parm;
				copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_mod_view_t));
				if( !(mod_fb_p = vpp_mod_get_fb_base(parm.mod)) )
					break;
				mod_p = vpp_mod_get_base(parm.mod);
				if( parm.read ){
					mod_fb_p->fn_view(VPP_FLAG_RD,&parm.view);
					copy_to_user( (void *)arg, (void *) &parm, sizeof(vpp_mod_view_t));
				}
				else {
					mod_fb_p->fn_view(0,&parm.view);
				}
			}
			break;
		case VPPIO_VPPGET_PTS:
			copy_to_user( (void *)arg, (void *) &g_vpp.disp_pts, sizeof(vpp_pts_t));
			break;
		case VPPIO_VPPSET_PTS:
			copy_from_user( (void *) &g_vpp.frame_pts, (const void *)arg, sizeof(vpp_pts_t));
			{
				int i;
				for(i=0;i<sizeof(vpp_pts_t);i++){
					if( g_vpp.frame_pts.pts[i] )
						break;
				}
				if( i == sizeof(vpp_pts_t )){
					memset(&g_vpp.govw_pts,0x0,sizeof(vpp_pts_t));
					memset(&g_vpp.disp_pts,0x0,sizeof(vpp_pts_t));
				}
			}
			break;
#ifdef CONFIG_VPP_STREAM_CAPTURE
		case VPPIO_STREAM_ENABLE:
			g_vpp.stream_enable = arg;
			g_vpp.stream_mb_sync_flag = 0;
			DPRINT("[VPP] VPPIO_STREAM_ENABLE %d\n",g_vpp.stream_enable);

			if( g_vpp.alloc_framebuf == 0 ){
				if( arg ){
					vout_info_t *info;

					info = vout_info_get_entry(0);
					vpp_alloc_framebuffer(info->resx,info->resy);
				}
				else {
					vpp_free_framebuffer();
				}
			}
			
			vpp_pan_display(0,0,(vpp_get_govm_path() & VPP_PATH_GOVM_IN_VPU)?0:1);
			if(!g_vpp.stream_enable){
				vpp_lock();
				vpp_mb_put(0);
				vpp_unlock();
			}
			break;
		case VPPIO_STREAM_GETFB:
			{
				vdo_framebuf_t fb;

				vpp_lock();
				fb = vout_info[0].fb;
				fb.fb_w = vpp_calc_align(fb.fb_w,64);
				fb.y_addr = g_vpp.mb_govw[g_vpp.stream_mb_index];
				fb.y_size = fb.fb_w * fb.img_h;
				fb.c_addr = fb.y_addr + fb.y_size;
				fb.c_size = fb.y_size;
				fb.col_fmt = VDO_COL_FMT_YUV422H;

				copy_to_user( (void *)arg, (void *) &fb,sizeof(vdo_framebuf_t));
				retval = vpp_mb_get(fb.y_addr);
				vpp_unlock();
			}
			break;
		case VPPIO_STREAM_PUTFB:
			{
				vdo_framebuf_t fb;
				copy_from_user( (void *) &fb, (const void *)arg, sizeof(vdo_framebuf_t));
				vpp_lock();
				vpp_mb_put(fb.y_addr);
				vpp_unlock();
			}
			break;
#endif
		case VPPIO_MODULE_CSC:
			{
				vpp_mod_arg_t parm;
				copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_mod_arg_t));
				if( !(mod_fb_p = vpp_mod_get_fb_base(parm.mod)) )
					break;
				mod_p = vpp_mod_get_base(parm.mod);
				if( parm.read ){
					parm.arg1 = mod_fb_p->csc_mode;
					copy_to_user( (void *)arg, (void *) &parm, sizeof(vpp_mod_arg_t));
				}
				else {
					mod_fb_p->csc_mode = parm.arg1;
					mod_fb_p->set_csc(mod_fb_p->csc_mode);
				}
			}
			break;
		default:
			retval = -ENOTTY;
			break;
	}
	return retval;
} /* End of vpp_common_ioctl */

int vout_ioctl(unsigned int cmd,unsigned long arg)
{
	int retval = 0;

//	DBGMSG("vout_ioctl\n");

	switch(cmd){
		case VPPIO_VOGET_INFO:
			{
				vpp_vout_info_t parm;
				vout_t *vo;
				int num;
				vout_inf_t *inf;
				int fb_num;
				vout_info_t *vo_info;

	//			DBGMSG("VPPIO_VOGET_INFO\n");
				copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_vout_info_t));
				fb_num = (parm.num & 0xF0) >> 4;
				num = parm.num & 0xF;
				if( (fb_num >= VPP_VOUT_INFO_NUM) || (num >= VOUT_MODE_MAX) ){
					retval = -ENOTTY;
					break;
				}
				memset(&parm,0,sizeof(vpp_vout_info_t));
				if( (vo = vout_get_entry_adapter(num)) == 0 ){
					retval = -ENOTTY;
					break;
				}
				if( (inf = vout_get_inf_entry_adapter(num)) == 0 ){
					retval = -ENOTTY;
					break;
				}
				if( (vo_info = vout_get_info_entry(vo->num)) == 0 ){
					retval = -ENOTTY;
					break;
				}
				if( (g_vpp.virtual_display == 0) && (fb_num != vo_info->num) ){
					retval = -ENOTTY;
					break;
				}
				DBGMSG("VPPIO_VOGET_INFO %d,fb%d,0x%x,0x%x\n",num,vo_info->num,(int)vo,(int)inf);
				if( vo && inf ){
					parm.num = num;
					parm.status = vo->status | VPP_VOUT_STS_REGISTER;
					strncpy(parm.name,vout_adpt_str[num],10);
					switch( inf->mode ){
						case VOUT_INF_DVI:
							parm.status &= ~VPP_VOUT_STS_ACTIVE;
							if( vo->dev ){
								// check current DVI is dvi or dvo2hdmi or lcd
								switch(num){
									case VOUT_DVI:
										if( strcmp("VT1632",vo->dev->name) == 0 ){
											parm.status |= VPP_VOUT_STS_ACTIVE;
										}
										parm.status |= VPP_VOUT_STS_ACTIVE;
										break;
									case VOUT_LCD:
										if( strcmp("LCD",vo->dev->name) == 0 ){
											parm.status |= VPP_VOUT_STS_ACTIVE;
										}
										break;
									case VOUT_DVO2HDMI:
										if( strcmp("SIL902X",vo->dev->name) == 0 ){
											parm.status |= VPP_VOUT_STS_ACTIVE;
										}
										break;
									default:
										break;
								}
							}
							else { // dvi hw mode
								if( num == VOUT_DVI )
									parm.status |= VPP_VOUT_STS_ACTIVE;
							}
							
							if( g_vpp.virtual_display ){
								if( vout_chkplug(VPP_VOUT_NUM_HDMI) ){
									parm.status &= ~VPP_VOUT_STS_ACTIVE;
								}
							}
							break;
						case VOUT_INF_HDMI:
						case VOUT_INF_LVDS:
							// check current HDMI is HDMI or LVDS
							if( vo->inf != inf ){
								parm.status = VPP_VOUT_STS_REGISTER;
							}
							break;
						default:
							break;
					}

					if( parm.status & VPP_VOUT_STS_ACTIVE ){
						if( vout_chkplug(vo->num) )
							parm.status |= VPP_VOUT_STS_PLUGIN;
						else 
							parm.status &= ~VPP_VOUT_STS_PLUGIN;
					}
					else {
						parm.status = VPP_VOUT_STS_REGISTER;
					}
				}
				copy_to_user( (void *)arg, (const void *) &parm, sizeof(vpp_vout_info_t));
			}
			break;
		case VPPIO_VOSET_MODE:
			{
			vpp_vout_parm_t parm;
			vout_t *vo;
			vout_inf_t *inf;
			
			copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_vout_parm_t));
			vo = vout_get_entry_adapter(parm.num);
			inf = vout_get_inf_entry_adapter(parm.num);
			if( vo && inf ){
				DBG_DETAIL("VPPIO_VOSET_MODE %d %d\n",vo->num,parm.arg);
				vout_set_blank((0x1 << vo->num),(parm.arg)?0:1);
				retval = vout_set_mode(vo->num, inf->mode);
			}
			}
			break;
		case VPPIO_VOSET_BLANK:
			{
			vpp_vout_parm_t parm;
			vout_t *vo;
			
			copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_vout_parm_t));
			if( (vo = vout_get_entry_adapter(parm.num)) ){
				retval = vout_set_blank((0x1 << vo->num),parm.arg);
			}
			}
			break;
#ifdef WMT_FTBLK_GOVRH			
		case VPPIO_VOSET_DACSENSE:
			{
			vpp_vout_parm_t parm;
			
			copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_vout_parm_t));
			// hw no support
			}
			break;
		case VPPIO_VOSET_BRIGHTNESS:
			{
			vpp_vout_parm_t parm;
			
			copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_vout_parm_t));
			govrh_set_brightness(p_govrh,parm.arg);
			}
			break;
		case VPPIO_VOGET_BRIGHTNESS:
			{
			vpp_vout_parm_t parm;
			
			parm.num = 0;
			parm.arg = govrh_get_brightness(p_govrh);
			copy_to_user((void *)arg,(void *)&parm, sizeof(vpp_vout_parm_t));
			}
			break;
		case VPPIO_VOSET_CONTRAST:
			{
			vpp_vout_parm_t parm;
			
			copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_vout_parm_t));
			govrh_set_contrast(p_govrh,parm.arg);
			}
			break;
		case VPPIO_VOGET_CONTRAST:
			{
			vpp_vout_parm_t parm;
			
			parm.num = 0;
			parm.arg = govrh_get_contrast(p_govrh);
			copy_to_user((void *)arg,(void *) &parm, sizeof(vpp_vout_parm_t));
			}
			break;
#endif			
		case VPPIO_VOSET_OPTION:
			{
			vpp_vout_option_t option;
			vout_t *vo;
			int num;
			vout_inf_t *inf;

			copy_from_user( (void *) &option, (const void *)arg, sizeof(vpp_vout_option_t));
			num = option.num;
			if( num >= VOUT_MODE_MAX ){
				retval = -ENOTTY;
				break;
			}
			vo = vout_get_entry_adapter(num);
			inf = vout_get_inf_entry_adapter(num);
			if( vo && inf ){
				vo->option[0] = option.option[0];
				vo->option[1] = option.option[1];
				vo->option[2] = option.option[2];
				vout_set_mode(vo->num,inf->mode);
			}
			}
			break;
		case VPPIO_VOGET_OPTION:
			{
			vpp_vout_option_t option;
			vout_t *vo;
			int num;

			copy_from_user( (void *) &option, (const void *)arg, sizeof(vpp_vout_option_t));
			num = option.num;
			if( num >= VOUT_MODE_MAX ){
				retval = -ENOTTY;
				break;
			}
			memset(&option,0,sizeof(vpp_vout_info_t));			
			if( (vo = vout_get_entry_adapter(num)) ){
				option.num = num;
				option.option[0] = vo->option[0];
				option.option[1] = vo->option[1];
				option.option[2] = vo->option[2];
			}
			copy_to_user( (void *)arg, (const void *) &option, sizeof(vpp_vout_option_t));
			}
			break;
		case VPPIO_VOUT_VMODE:
			{
			vpp_vout_vmode_t parm;
			int i;
			vpp_timing_t *vmode;
			unsigned int resx,resy,fps;
			unsigned int pre_resx,pre_resy,pre_fps;
			int index,from_index;
			int support;
			unsigned int option,pre_option;
#ifdef CONFIG_WMT_EDID
			edid_info_t *edid_info;
#endif
			vpp_timing_t *timing;
			vpp_vout_t mode;

			copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_vout_vmode_t));
			from_index = parm.num;
			parm.num = 0;
			mode = parm.mode & 0xF;
#ifdef CONFIG_VPP_DEMO
			parm.parm[parm.num].resx = 1920;
			parm.parm[parm.num].resy = 1080;
			parm.parm[parm.num].fps = 60;
			parm.parm[parm.num].option = 0;
			parm.num++;
#else
#ifdef CONFIG_WMT_EDID
			//--> add by howayhuo for VGA virtual_display
			if(g_vpp.virtual_display && (mode == VPP_VOUT_DVI))
			{
				if(cs8556_tvformat_is_set())
				{
					vpp_timing_t timing;
					if(cs8556_get_output_format() == VGA)
					{
						cs8556_vga_get_edid();
						cs8556_get_timing(&timing);		
						parm.parm[parm.num].resx = timing.hpixel;
						parm.parm[parm.num].resy = timing.vpixel;
						parm.parm[parm.num].fps = cs8556_get_fps();
						parm.parm[parm.num].option = VPP_OPT_FPS_VAL(parm.parm[parm.num].fps);
						parm.num++;
						goto vout_vmode_end;
					}
				}			
			}
			//<-- end add

			{
				vout_t *vo;

				if( !(vo = vout_get_entry_adapter(mode)) ){
					goto vout_vmode_end;
				}
				
				if( !(vo->status & VPP_VOUT_STS_PLUGIN) ){
					DPRINT("*W* not plugin\n");
					goto vout_vmode_end;
				}
				
				if( vout_get_edid(vo->num) == 0 ){
					DPRINT("*W* read EDID fail\n");
					goto vout_vmode_end;
				}
				if( edid_parse(vo->edid,&vo->edid_info) == 0 ){
					DPRINT("*W* parse EDID fail\n");
					goto vout_vmode_end;
				}
				edid_info = &vo->edid_info;
			}
#endif
			index = 0;
			resx = resy = fps = option = 0;
			pre_resx = pre_resy = pre_fps = pre_option = 0;
			for(i=0;;i++){
				vmode = (vpp_timing_t *) &vpp_video_mode_table[i];
				if( vmode->pixel_clock == 0 ) 
					break;
				resx = vmode->hpixel;
				resy = vmode->vpixel;
				fps = vpp_get_video_mode_fps(vmode);
				if( vmode->option & VPP_OPT_INTERLACE ){
					resy *= 2;
					i++;
				}
				option = fps & EDID_TMR_FREQ;
				option |= (vmode->option & VPP_OPT_INTERLACE)? EDID_TMR_INTERLACE:0;
				if( (pre_resx == resx) && (pre_resy == resy) && (pre_fps == fps) && (pre_option == option)){
					continue;
				}
				pre_resx = resx;
				pre_resy = resy;
				pre_fps = fps;
				pre_option = option;
				support = 0;
				
#ifdef CONFIG_WMT_EDID
				if( edid_find_support(edid_info,resx, resy,option,&timing) ){
#else
				if( 1 ){
#endif
					support = 1;
				}

				switch( mode ){
					case VPP_VOUT_HDMI:
					case VPP_VOUT_DVO2HDMI:
						if( g_vpp.hdmi_video_mode ){
							if( resy > g_vpp.hdmi_video_mode ){
								support = 0;
							}
						}
						break;
					default:
						break;
				}

				if( support ){
					if( index >= from_index ){
						parm.parm[parm.num].resx = resx;
						parm.parm[parm.num].resy = resy;
						parm.parm[parm.num].fps = fps;
						parm.parm[parm.num].option = vmode->option;
						parm.num++;
					}
					index++;
					if( parm.num >= VPP_VOUT_VMODE_NUM )
						break;
				}
			}
#ifdef CONFIG_WMT_EDID
vout_vmode_end:
#endif
#endif
			// DPRINT("[VPP] get support vmode %d\n",parm.num);
			copy_to_user( (void *)arg, (const void *) &parm, sizeof(vpp_vout_vmode_t));
			}
			break;
		case VPPIO_VOGET_EDID:
			{
				vpp_vout_edid_t parm;
				char *edid;
				vout_t *vo;
				int size;
				
				copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_vout_edid_t));
				size = 0;
#ifdef CONFIG_WMT_EDID
				if(!(vo = vout_get_entry_adapter(parm.mode))){
					goto vout_edid_end;
				}

				if( !(vo->status & VPP_VOUT_STS_PLUGIN) ){
					DPRINT("*W* not plugin\n");
					goto vout_edid_end;
				}
				
				if( (edid = vout_get_edid(vo->num)) == 0 ){
					DPRINT("*W* read EDID fail\n");
					goto vout_edid_end;
				}
				size = (edid[0x7E] + 1) * 128;
				if( size > parm.size )
					size = parm.size;
				copy_to_user((void *) parm.buf, (void *) edid, size);
vout_edid_end:	
#endif
				parm.size = size;
				copy_to_user( (void *)arg, (const void *) &parm, sizeof(vpp_vout_edid_t));
			}
			break;
		case VPPIO_VOGET_CP_INFO:
			{
				vpp_vout_cp_info_t parm;
				int num;

				copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_vout_cp_info_t));
				num = parm.num;
				if( num >= VOUT_MODE_MAX ){
					retval = -ENOTTY;
					break;
				}
				memset(&parm,0,sizeof(vpp_vout_cp_info_t));
				switch( num ){
					case VOUT_DVO2HDMI:
					case VOUT_HDMI:
						parm.bksv[0] = g_vpp.hdmi_bksv[0];
						parm.bksv[1] = g_vpp.hdmi_bksv[1];
						break;
					default:
						parm.bksv[0] = parm.bksv[1] = 0;
						break;
				}
				copy_to_user( (void *)arg, (const void *) &parm, sizeof(vpp_vout_cp_info_t));
			}
			break;
		case VPPIO_VOSET_CP_KEY:
			if( g_vpp.hdmi_cp_p == 0 ){
				g_vpp.hdmi_cp_p = (char *)kmalloc(sizeof(vpp_vout_cp_key_t),GFP_KERNEL);
			}
			if( g_vpp.hdmi_cp_p ){
				copy_from_user( (void *) g_vpp.hdmi_cp_p, (const void *)arg, sizeof(vpp_vout_cp_key_t));
				if( hdmi_cp ){
					hdmi_cp->init();
				}
			}
			break;
#ifdef WMT_FTBLK_HDMI
		case VPPIO_VOSET_AUDIO_PASSTHRU:
			vppif_reg32_write(HDMI_AUD_SUB_PACKET,(arg)?0xF:0x0);
			break;
#endif
#ifdef CONFIG_VPP_OVERSCAN
		case VPPIO_VOSET_OVERSCAN_OFFSET:
			{
				vpp_vout_overscan_offset_t parm;
				vout_info_t *vo_info;
				
				copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_vout_overscan_offset_t));
				if( (vo_info = vout_get_info_entry(parm.fb_idx)) ){
					vo_info->fb_xoffset = parm.xoffset;
					vo_info->fb_yoffset = parm.yoffset;
					vo_info->fb_clr = ~0x0;
				}
			}
			break;
#endif
#ifdef CONFIG_VPP_VIRTUAL_DISPLAY
		case VPPIO_VOSET_VIRTUAL_FBDEV:
			//disable fb0
			//vout_set_blank(VPP_VOUT_ALL,1);
			g_vpp.virtual_display = arg;
			DPRINT("[VPP] virtual display %d\n",(int)arg);
			break;
#endif			
		default:
			retval = -ENOTTY;
			break;
	}
	return retval;
} /* End of vout_ioctl */

int govr_ioctl(unsigned int cmd,unsigned long arg)
{
	int retval = 0;
	
 	switch(cmd){
#ifdef WMT_FTBLK_GOVRH		
		case VPPIO_GOVRSET_DVO:
			{
			vdo_dvo_parm_t parm;

			copy_from_user( (void *) &parm, (const void *)arg, sizeof(vdo_dvo_parm_t));
			govrh_set_dvo_enable(p_govrh,parm.enable);
			govrh_set_dvo_color_format(p_govrh,parm.color_fmt);
			govrh_set_dvo_clock_delay(p_govrh,parm.clk_inv,parm.clk_delay);
			govrh_set_dvo_outdatw(p_govrh,parm.data_w);
			govrh_set_dvo_sync_polar(p_govrh,parm.sync_polar,parm.vsync_polar);
			p_govrh->fb_p->set_csc(p_govrh->fb_p->csc_mode);
			}
			break;
#endif
#ifdef WMT_FTBLK_GOVRH_CURSOR
		case VPPIO_GOVRSET_CUR_COLKEY:
			{
			vpp_mod_arg_t parm;

			copy_from_user( (void *) &parm, (const void *)arg, sizeof(vdo_dvo_parm_t));
			govrh_CUR_set_color_key(p_cursor,VPP_FLAG_ENABLE,0,parm.arg1);
			}
			break;
		case VPPIO_GOVRSET_CUR_HOTSPOT:
			{
			vpp_mod_arg_t parm;
			vdo_view_t view;

			copy_from_user( (void *) &parm, (const void *)arg, sizeof(vdo_dvo_parm_t));
			p_cursor->hotspot_x = parm.arg1;
			p_cursor->hotspot_y = parm.arg2;
			view.posx = p_cursor->posx;
			view.posy = p_cursor->posy;
			p_cursor->fb_p->fn_view(0,&view);
			}
			break;
#endif
		default:
			retval = -ENOTTY;
			break;
	}
	return retval;
} /* End of govr_ioctl */

int govw_ioctl(unsigned int cmd,unsigned long arg)
{
	int retval = 0;
	
	switch(cmd){
		case VPPIO_GOVW_ENABLE:
			vpp_set_govw_tg(arg);
			break;
		default:
			retval = -ENOTTY;
			break;
	}
	return retval;
} /* End of govw_ioctl */

int govm_ioctl(unsigned int cmd,unsigned long arg)
{
	int retval = 0;

	switch(cmd){
		case VPPIO_GOVMSET_SRCPATH:
			{
			vpp_src_path_t parm;
			vpp_video_path_t vpath;

			copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_src_path_t));
			// fix first line show green line when enable vpu path in full screen
			if((parm.src_path & VPP_PATH_GOVM_IN_VPU) && parm.enable ){
				if(	vppif_reg32_read(GOVM_VPU_Y_STA_CR) == 0 ){
					g_vpp.govw_skip_frame = 2;
				}
			}
#ifdef CONFIG_HW_VPU_HALT_GOVW_TG_ERR
			vpp_lock();
			if( vpp_govm_path ){
				extern int vpp_govm_path1;
				if( parm.enable ){
					vpp_govm_path |= parm.src_path;
					vpp_govm_path1 |= parm.src_path; 
				}
				else {
					vpp_govm_path &= ~parm.src_path;
					vpp_govm_path1 &= ~parm.src_path;
				}
			}
			else {
				vpp_set_govm_path(parm.src_path,parm.enable);
			}
			vpp_unlock();
#else
			vpp_set_govm_path(parm.src_path,parm.enable);
#endif
			vpath = (vpp_get_govm_path() & VPP_PATH_GOVM_IN_VPU)? g_vpp.vpu_path:g_vpp.ge_path;
//			DPRINT("[VPP] set src path 0x%x,enable %d,vpp path %s,cur path %s\n",parm.src_path,parm.enable,
//				vpp_vpath_str[vpath],vpp_vpath_str[g_vpp.vpp_path]);

			vpp_pan_display(0,0,(vpath == g_vpp.ge_path));
			}
			break;
		case VPPIO_GOVMGET_SRCPATH:
			retval = vpp_get_govm_path();
			break;
		case VPPIO_GOVMSET_ALPHA:
			{
			vpp_alpha_parm_t parm;

			copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_alpha_parm_t));				
			govm_set_alpha_mode(parm.enable,parm.mode,parm.A,parm.B);
			}
			break;
		case VPPIO_GOVMSET_GAMMA:
			govm_set_gamma_mode(arg);
			break;
		case VPPIO_GOVMSET_CLAMPING:
			govm_set_clamping_enable(arg);
			break;		
		default:
			retval = -ENOTTY;
			break;
	}
	return retval;
} /* End of govm_ioctl */

int vpu_ioctl(unsigned int cmd,unsigned long arg)
{
	int retval = 0;
	
	switch(cmd){
		case VPPIO_VPUSET_VIEW:
			{
			vdo_view_t view;

			copy_from_user( (void *) &view, (const void *)arg, sizeof(vdo_view_t));
			p_vpu->fb_p->fn_view(0,&view);
			if( vpp_check_dbg_level(VPP_DBGLVL_SCALE) ){
				DPRINT("[VPP] set view\n");
			}
#ifdef CONFIG_SCL_DIRECT_PATH
			if( g_vpp.vpp_path == VPP_VPATH_SCL ){
				g_vpp.scl_fb_mb_clear = 0xFF;
			}
#endif
			}
			break;
		case VPPIO_VPUGET_VIEW:
			{
			vdo_view_t view;

			p_vpu->fb_p->fn_view(1,&view);
			copy_to_user( (void *)arg, (void *) &view, sizeof(vdo_view_t));
			}
			break;
		case VPPIO_VPUSET_FBDISP:
			{
			vpp_dispfb_t parm;
			
			copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_dispfb_t));
			retval = vpp_disp_fb_add(&parm,VPP_FB_PATH_VPU);
			}
			break;
		case VPPIO_VPU_CLR_FBDISP:
			retval = vpp_disp_fb_clr(0,0);
			break;
		case VPPIO_VPUSET_DEIMODE:
			{
			vpp_deinterlace_parm_t parm;

			copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_deinterlace_parm_t));
			p_vpu->dei_mode = parm.mode;
			vpu_dei_set_mode(p_vpu->dei_mode);
			if( !vpp_mv_debug ){
			 	vpu_dei_set_param(p_vpu->dei_mode,parm.level);
			}
			}
			break;
		default:
			retval = -ENOTTY;
			break;
	}
	return retval;
} /* End of vpu_ioctl */

int scl_ioctl(unsigned int cmd,unsigned long arg)
{
	int retval = 0;
	extern int ge_do_alpha_bitblt(vdo_framebuf_t *src,vdo_framebuf_t *src2,vdo_framebuf_t *dst);

	if( g_vpp.vpp_path == VPP_VPATH_SCL ){
		return -ENOTTY;
	}

	switch(cmd){
		case VPPIO_SCL_SCALE_OVERLAP:
			{
				vpp_scale_overlap_t parm;

				copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_scale_overlap_t));
				
				// enable VPU
				auto_pll_divisor(DEV_VPU,CLK_ENABLE,0,0);
				vpu_set_enable(1);
				vpu_r_set_mif_enable(1);
				vpu_r_set_mif2_enable(1);

				vpp_set_NA12_hiprio(2);
				p_vpu->scl_fb = parm.dst_fb;
				ge_do_alpha_bitblt(&parm.src_fb,&parm.src2_fb,&parm.dst_fb);
				vpp_set_NA12_hiprio(0);
				
				// disable VPU
				vpu_r_set_mif_enable(0);
				vpu_r_set_mif2_enable(0);
				vpu_set_enable(0);
				auto_pll_divisor(DEV_VPU,CLK_DISABLE,0,0);
				
				copy_to_user( (void *) arg, (void *) &parm, sizeof(vpp_scale_overlap_t));
			}
			break;
		case VPPIO_SCL_SCALE_VPU:
			{
				vpp_scale_t parm;
				extern int vpu_proc_scale(vdo_framebuf_t *src_fb,vdo_framebuf_t *dst_fb);

				p_vpu->scale_sync = 1;
				copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_scale_t));
				
				auto_pll_divisor(DEV_VPU,CLK_ENABLE,0,0);
				
				vpu_set_enable(1);
				vpu_r_set_mif_enable(1);
				vpu_r_set_mif2_enable(1);
				
				vpp_set_NA12_hiprio(1);
#ifdef CONFIG_VPP_MOTION_VECTOR
				if(p_vpu->dei_mode == VPP_DEI_MOTION_VECTOR ){
					parm.src_fb.flag |= VDO_FLAG_MOTION_VECTOR;
					vpp_dei_mv_duplicate(&parm.src_fb);
				}
#endif
				p_vpu->scl_fb = parm.dst_fb;
				ge_do_alpha_bitblt(&parm.src_fb,0,&parm.dst_fb);
				vpp_set_NA12_hiprio(0);

				// disable VPU
				vpu_r_set_mif_enable(0);
				vpu_r_set_mif2_enable(0);
				vpu_set_enable(0);
				
				auto_pll_divisor(DEV_VPU,CLK_DISABLE,0,0);

				retval = 0;
				copy_to_user( (void *) arg, (void *) &parm, sizeof(vpp_scale_t));
#ifdef CONFIG_VPP_MOTION_VECTOR
				if( vpp_mv_debug ){
					vpp_dei_mv_analysis(&parm.src_fb,&parm.dst_fb);
				}
#endif
			}			
			break;
		case VPPIO_SCL_SCALE_ASYNC:
		case VPPIO_SCL_SCALE:
			{
				vpp_scale_t parm;

//				g_vpp.scale_type = VPP_SCALE_TYPE_OVERLAP;
				p_scl->scale_sync = (cmd==VPPIO_SCL_SCALE)? 1:0;
				copy_from_user( (void *) &parm, (const void *)arg, sizeof(vpp_scale_t));
				switch( g_vpp.scale_type ){
					case VPP_SCALE_TYPE_OVERLAP:
						{
						// enable VPU
						auto_pll_divisor(DEV_VPU,CLK_ENABLE,0,0);
						vpu_set_enable(1);
						vpu_r_set_mif_enable(1);
						vpu_r_set_mif2_enable(1);

						ge_do_alpha_bitblt(&parm.src_fb,0,&parm.dst_fb);

						// disable VPU
						vpu_r_set_mif_enable(0);
						vpu_r_set_mif2_enable(0);
						vpu_set_enable(0);
						auto_pll_divisor(DEV_VPU,CLK_DISABLE,0,0);
						}
						break;
					case VPP_SCALE_TYPE_NORMAL:
					default:
						vpp_set_NA12_hiprio(1);
						retval = vpp_set_recursive_scale(&parm.src_fb,&parm.dst_fb);
						vpp_set_NA12_hiprio(0);
						break;
				}
				copy_to_user( (void *) arg, (void *) &parm, sizeof(vpp_scale_t));
			}
			break;
#ifdef WMT_FTBLK_SCL
		case VPPIO_SCL_DROP_LINE_ENABLE:
			scl_set_drop_line(arg);
			break;
#endif
		case VPPIO_SCL_SCALE_FINISH:
			retval = p_scl->scale_finish();
			break;
		case VPPIO_SCL_SCALE_TYPE:
			g_vpp.scale_type = arg;
			break;
		default:
			retval = -ENOTTY;
			break;
	}
	return retval;
} /* End of scl_ioctl */

int vpp_ioctl(unsigned int cmd,unsigned long arg)
{
	int retval = 0;
	int err = 0;

//	DBGMSG("vpp_ioctl\n");

	switch( _IOC_TYPE(cmd) ){
		case VPPIO_MAGIC:
			break;
		default:
			return -ENOTTY;
	}
	
	/* check argument area */
	if( _IOC_DIR(cmd) & _IOC_READ )
		err = !access_ok( VERIFY_WRITE, (void __user *) arg, _IOC_SIZE(cmd));
	else if ( _IOC_DIR(cmd) & _IOC_WRITE )
		err = !access_ok( VERIFY_READ, (void __user *) arg, _IOC_SIZE(cmd));
	
	if( err ) return -EFAULT;

	if( vpp_check_dbg_level(VPP_DBGLVL_IOCTL) ){
		switch( cmd ){
			case VPPIO_VPPSET_FBDISP:
			case VPPIO_VPPGET_FBDISP:
			case VPPIO_MODULE_VIEW:		// cursor pos
				break;
			default:
				DPRINT("[VPP] ioctl cmd 0x%x,arg 0x%x\n",_IOC_NR(cmd),(int)arg);
				break;
		}
	}

	vpp_set_mutex(1,1);
	switch(_IOC_NR(cmd)){
		case VPPIO_VPP_BASE ... (VPPIO_VOUT_BASE-1):
//			DBGMSG("VPP command ioctl\n");
			vpp_mod_set_clock(VPP_MOD_GOVW,VPP_FLAG_ENABLE,1);
			vpp_mod_set_clock(VPP_MOD_VPU,VPP_FLAG_ENABLE,1);
			retval = vpp_common_ioctl(cmd,arg);
			vpp_mod_set_clock(VPP_MOD_GOVW,VPP_FLAG_DISABLE,1);
			vpp_mod_set_clock(VPP_MOD_VPU,VPP_FLAG_DISABLE,1);
			break;
		case VPPIO_VOUT_BASE ... (VPPIO_GOVR_BASE-1):
//			DBGMSG("VOUT ioctl\n");
			retval = vout_ioctl(cmd,arg);
			break;
		case VPPIO_GOVR_BASE ... (VPPIO_GOVW_BASE-1):
//			DBGMSG("GOVR ioctl\n");
			retval = govr_ioctl(cmd,arg);
			break;
		case VPPIO_GOVW_BASE ... (VPPIO_GOVM_BASE-1):
//			DBGMSG("GOVW ioctl\n");
			retval = govw_ioctl(cmd,arg);
			break;
		case VPPIO_GOVM_BASE ... (VPPIO_VPU_BASE-1):
//			DBGMSG("GOVM ioctl\n");
			vpp_mod_set_clock(VPP_MOD_GOVW,VPP_FLAG_ENABLE,1);
			vpp_mod_set_clock(VPP_MOD_VPU,VPP_FLAG_ENABLE,1);
			retval = govm_ioctl(cmd,arg);
			vpp_mod_set_clock(VPP_MOD_GOVW,VPP_FLAG_DISABLE,1);
			vpp_mod_set_clock(VPP_MOD_VPU,VPP_FLAG_DISABLE,1);
			break;
		case VPPIO_VPU_BASE ... (VPPIO_SCL_BASE-1):
//			DBGMSG("VPU ioctl\n");
			vpp_mod_set_clock(VPP_MOD_GOVW,VPP_FLAG_ENABLE,1);
			vpp_mod_set_clock(VPP_MOD_VPU,VPP_FLAG_ENABLE,1);
			retval = vpu_ioctl(cmd,arg);
			vpp_mod_set_clock(VPP_MOD_GOVW,VPP_FLAG_DISABLE,1);
			vpp_mod_set_clock(VPP_MOD_VPU,VPP_FLAG_DISABLE,1);
			break;
		case VPPIO_SCL_BASE ... (VPPIO_MAX-1):
//			DBGMSG("SCL ioctl\n");
#ifdef CONFIG_VPP_MOTION_VECTOR
			retval = scl_ioctl(cmd,arg);
#else
			vpp_set_mutex(1,0);
			retval = scl_ioctl(cmd,arg);
			vpp_set_mutex(1,1);
#endif
			break;
		default:
			retval = -ENOTTY;
			break;
	}
	vpp_set_mutex(1,0);

	if( vpp_check_dbg_level(VPP_DBGLVL_IOCTL) ){
		switch( cmd ){
			case VPPIO_VPPSET_FBDISP:
			case VPPIO_VPPGET_FBDISP:
			case VPPIO_MODULE_VIEW:
				break;
			default:
				DPRINT("[VPP] ioctl cmd 0x%x,ret 0x%x\n",_IOC_NR(cmd),(int)retval);
				break;
		}
	}
	return retval;
} /* End of vpp_ioctl */

int vpp_set_audio(int format,int sample_rate,int channel)
{
	vout_audio_t info;

	DBG_MSG("set audio(fmt %d,rate %d,ch %d)\n",format,sample_rate,channel);
	info.fmt = format;
	info.sample_rate = sample_rate;
	info.channel = channel;
	return vout_set_audio(&info);
}

// this function is for old vpp api compatible, if value < 1M is pico else HZ
unsigned int vpp_check_pixclock(unsigned int pixclk,int hz)
{
	unsigned int pixclk_hz;

	pixclk_hz = ( pixclk > 1000000 )? pixclk:(PICOS2KHZ(pixclk)*1000);
	return (hz)? pixclk_hz:KHZ2PICOS(pixclk_hz/1000);
}

void vpp_get_info(int fbn,struct fb_var_screeninfo *var)
{
	vout_info_t *info;
	vpp_clock_t tmr;

	info = vout_info_get_entry(fbn);
#ifdef CONFIG_VPP_OVERSCAN
	if( info->fb_xres ){
		var->xres = info->fb_xres;
		var->yres = info->fb_yres;
		var->pixclock = KHZ2PICOS((var->xres * var->yres * 60) / 1000);
	}
	else
#endif
	{
		if( info->govr ){
			info->govr->get_tg(&tmr);
			info->pixclk = vpp_get_base_clock(info->govr->mod);
			var->pixclock = KHZ2PICOS(info->pixclk/1000);
			var->left_margin = tmr.begin_pixel_of_active;
			var->right_margin = tmr.total_pixel_of_line - tmr.end_pixel_of_active;
			var->upper_margin = tmr.begin_line_of_active;
			var->lower_margin = tmr.total_line_of_frame - tmr.end_line_of_active;
			var->hsync_len = tmr.hsync;
			var->vsync_len = tmr.vsync;
		}
		else {
			var->pixclock = KHZ2PICOS((info->resx * info->resy * 60) / 1000);
		}
		var->xres = info->resx;
		var->yres = info->resy;
	}
	var->xres_virtual = vpp_calc_fb_width(g_vpp.mb_colfmt,var->xres);
	var->yres_virtual = var->yres * VPP_MB_ALLOC_NUM;
	info->pico = var->pixclock;
	if(g_vpp.mb_colfmt == VDO_COL_FMT_ARGB){
		var->bits_per_pixel = 32;
		var->red.offset = 16;
		var->red.length = 8;
		var->red.msb_right = 0;
		var->green.offset = 8;
		var->green.length = 8;
		var->green.msb_right = 0;
		var->green.offset = 0;
		var->green.length = 8;
		var->green.msb_right = 0;
		var->transp.offset = 24;
		var->transp.length = 8;
		var->transp.msb_right = 0;
	}
	DBG_MSG("(%d,%dx%d,%dx%d,%d,%d)\n",fbn,var->xres,var->yres,var->xres_virtual,var->yres_virtual,var->pixclock,var->bits_per_pixel);
}

#if 0
void vpp_show_fb_info(struct fb_info *info)
{
	struct fb_var_screeninfo *var;

	var = &info->var;

	DPRINT("res(%d,%d),vir(%d,%d)\n",var->xres,var->yres,var->xres_virtual,var->yres_virtual);
	DPRINT("off(%d,%d),bpp %d,gray %d\n",var->xoffset,var->yoffset,var->bits_per_pixel,var->grayscale);
//	DPRINT("r %d,g %d,b %d,t %d\n",var->red,var->green,var->blue,var->transp);
	DPRINT("nonstd %d,activate %d,h %d,w %d\n",var->nonstd,var->activate,var->height,var->width);
	DPRINT("acc %d,pixclk %d\n",var->accel_flags,var->pixclock);
	DPRINT("left %d,right %d,up %d,low %d\n",var->left_margin,var->right_margin,var->upper_margin,var->lower_margin);
	DPRINT("hsync %d,vsync %d,sync 0x%x\n",var->hsync_len,var->vsync_len,var->sync);
	DPRINT("vmode 0x%x,rotate %d\n",var->vmode,var->rotate);
}
#endif

void vpp_var_to_fb(struct fb_var_screeninfo *var, struct fb_info *info,vdo_framebuf_t *fb)
{
	extern unsigned int fb_egl_swap;
	unsigned int addr;

	if( var ){
		switch (var->bits_per_pixel) {
			case 16:
				if ((info->var.red.length == 5) &&
					(info->var.green.length == 6) &&
					(info->var.blue.length == 5)) {
					fb->col_fmt = VDO_COL_FMT_RGB_565;
				} else if ((info->var.red.length == 5) &&
					(info->var.green.length == 5) &&
					(info->var.blue.length == 5)) {
					fb->col_fmt = VDO_COL_FMT_RGB_1555;
				} else {
					fb->col_fmt = VDO_COL_FMT_RGB_5551;
				}
				break;
			case 32:
				fb->col_fmt = VDO_COL_FMT_ARGB;
				break;
			default:
				fb->col_fmt = VDO_COL_FMT_RGB_565;
				break;
		}

        if(info->node)//modify by aksenxu, keep buffer width 64byte alignment, otherwise scl will cause error
            var->xres_virtual = vpp_calc_align(var->xres_virtual, 64);
        else
            var->xres_virtual = vpp_calc_fb_width(fb->col_fmt,var->xres);
		fb->img_w = var->xres;
		fb->img_h = var->yres;
		fb->fb_w = var->xres_virtual;
		fb->fb_h = var->yres_virtual;
		fb->h_crop = 0;
		fb->v_crop = 0;
		fb->flag = 0;
		fb->c_addr = 0;

		addr = var->yoffset * var->xres_virtual + var->xoffset;
		addr *= var->bits_per_pixel >> 3;
		addr += info->fix.smem_start;
		fb->y_addr = addr;
		fb->y_size = var->xres_virtual * var->yres * (var->bits_per_pixel >> 3);
	}

	if ( info && (info->node==0) && (fb_egl_swap != 0))	// for Android
		fb->y_addr = fb_egl_swap;

#if 0
	if( info->node != 0 )
		vpp_show_fb_info(info);
#endif
}

#ifdef CONFIG_VPP_OVERSCAN
int vpp_fb_post_scale(vout_info_t *vo_info,unsigned int offset)
{
	vdo_framebuf_t src,dst,out;
	unsigned int y_bpp,c_bpp;

	if( vo_info->fb_xres == 0 )
		return 0;

	// out fb
	govrh_get_framebuffer(vo_info->govr,&out);				
	vpp_get_colfmt_bpp(out.col_fmt,&y_bpp,&c_bpp);
	offset = (offset)?1:0;
	out.y_addr = g_vpp.mb_govw[offset];
	out.y_size = out.fb_w * out.fb_h * y_bpp / 8;
	out.c_addr = out.y_addr + out.y_size;
	out.c_size = (c_bpp)? (out.fb_w * out.fb_h * c_bpp / 8):0;
	if( vo_info->fb_clr & (0x1 << offset) ){
		unsigned int yaddr,caddr;

		yaddr = (unsigned int) mb_phys_to_virt(out.y_addr);
		caddr = (unsigned int) mb_phys_to_virt(out.c_addr);
		memset((void *)yaddr,0x0,out.y_size);
		memset((void *)caddr,0x80,out.c_size);
		
		vo_info->fb_clr &= ~(0x1 << offset);
	}

	// scale dst fb
	dst = out;
	dst.img_w -= (vo_info->fb_xoffset * 2);
	dst.img_h -= (vo_info->fb_yoffset * 2);
	offset = ((vo_info->fb_yoffset * dst.fb_w) + vo_info->fb_xoffset) * y_bpp / 8;
		dst.y_addr = out.y_addr + offset;
	offset = ((vo_info->fb_yoffset * dst.fb_w) + vo_info->fb_xoffset) * c_bpp / 8;
	dst.c_addr = out.c_addr + offset;

	// scale
	src = vo_info->fb;
	p_scl->scale_sync = 1;
	vpp_set_recursive_scale(&src,&dst);
	
	vout_set_framebuffer(vout_get_mask(vo_info),&out);
	return 1;
}
#endif

int vpp_pan_display(struct fb_var_screeninfo *var, struct fb_info *info,int enable)
{
#ifdef CONFIG_VPP_GE_DIRECT_PATH
	vpp_video_path_t path;
	vout_info_t *vo_info;

	DBG_DETAIL("(fb %d,enable %d)\n",(info)? info->node:0,enable);

    if( g_vpp.ge_direct_init == 0 )
		return 0;

	if( g_vpp.hdmi_certify_flag )
		return 0;

	vo_info = vout_info_get_entry((info)? info->node:0);
	if( vo_info->govr && vpp_check_dbg_level(VPP_DBGLVL_DISPFB) ){
		char buf[50];
		unsigned int yaddr,caddr;

		govrh_get_fb_addr(vo_info->govr,&yaddr,&caddr);
		sprintf(buf,"pan_display %d,%s,isr %d,0x%x",vo_info->num,vpp_mod_str[vo_info->govr_mod],vo_info->govr->vbis_cnt,yaddr);
		vpp_dbg_show(VPP_DBGLVL_DISPFB,vo_info->num+1,buf);
	}
	
	if( info && info->node ){	// only fb0 will check vpp_path
		goto pan_disp_govr2;
	}

	// check parameter for vpp path 
	path = (enable)? g_vpp.ge_path:g_vpp.vpu_path;
	if( g_vpp.vpu_path == VPP_VPATH_GOVW_SCL ){
		path = VPP_VPATH_GOVW_SCL;
		enable = 0;
	}
	
	if( g_vpp.vpp_path != path ){
#ifdef CONFIG_HW_VPU_HALT_GOVW_TG_ERR
		if( vpp_govm_path ){
			// DBG_MSG("skip chg GE direct path %d\n",enable);
			return 0;
		}
#endif
		if( enable ){
			vpp_var_to_fb(var,info,&vo_info->fb);
		}
		vpp_set_path(path,(enable)?&vo_info->fb:0);
	}
	else {
		vpp_fb_base_t *fb_p;
		
pan_disp_govr2:
		DBG_DETAIL("govr %d\n",vo_info->govr->mod);
		fb_p = vo_info->govr->fb_p;
		vpp_var_to_fb(var,info,&vo_info->fb);
		if( enable ){
#ifdef CONFIG_VPP_OVERSCAN
			if( vpp_fb_post_scale(vo_info,var->yoffset) )
				return 0;
#endif
			
//			if( var && (var->xres == fb_p->fb.img_w) && (var->yres == fb_p->fb.img_h) ){
				vout_set_framebuffer(vout_get_mask(vo_info),&vo_info->fb);
//			}
#ifdef CONFIG_VPP_STREAM_CAPTURE
			if( g_vpp.stream_enable ){
				int index = g_vpp.stream_mb_index;
				
				if( (info && (info->node==0)) || (info==0) ){
					do {
						index++;
						if( index >= g_vpp.mb_govw_cnt ){
							index = 0;
						}

						if( g_vpp.stream_mb_lock & (0x1 << index) ){
							continue;
						}
						break;
					} while(1);

					g_vpp.stream_mb_index = index;
					{
						vdo_framebuf_t src,dst;

						p_scl->scale_sync = 1;
						src = vo_info->fb;
						dst = vo_info->fb;
						dst.col_fmt = VDO_COL_FMT_YUV422H;
						dst.fb_w = vpp_calc_align(dst.fb_w,64);
						dst.y_addr = g_vpp.mb_govw[index];
						dst.c_addr = dst.y_addr + (dst.fb_w * dst.img_h);
						
						vpp_set_recursive_scale(&src,&dst);
					}
				}
			}
#endif
		}
	}
#endif
	return 0;
}

int vpp_set_par(struct fb_info *info)
{
	vout_info_t *vo_info;

	if( g_vpp.hdmi_certify_flag )
		return 0;

	if( (g_vpp.dual_display == 0) && (info->node == 1) ){
		return 0;
	}

	vo_info = vout_info_get_entry(info->node);
#ifdef CONFIG_VPP_OVERSCAN
	if( vo_info->fb_xres ){
		g_vpp.govrh_preinit = 0;
		g_vpp.ge_direct_init = 1;
		return 0;
	}
#endif
	
	info->var.xres_virtual = vpp_calc_fb_width(g_vpp.mb_colfmt,info->var.xres);
	if( (vo_info->force_config) || (g_vpp.govrh_preinit) ){
		vo_info->force_config = 0;
	}
	else if( (vo_info->resx == info->var.xres) && (vo_info->resy == info->var.yres) && 
		(vo_info->pico == info->var.pixclock) ){
		return 0;
	}
	DBG_MSG("fb%d,%dx%d,%d\n",info->node,info->var.xres,info->var.yres,info->var.pixclock);
	DBG_MSG("vir %dx%d\n",info->var.xres_virtual,info->var.yres_virtual);
//	DBG_MSG("%dx%d,%d\n",vo_info->resx,vo_info->resy,vo_info->pixclk);

	vpp_set_mutex(info->node,1);
	vo_info->resx = info->var.xres;
	vo_info->resy = info->var.yres;
	vo_info->resx_virtual = info->var.xres_virtual;
	vo_info->resy_virtual = info->var.yres_virtual;
	vo_info->pico = info->var.pixclock;
	vo_info->pixclk = vpp_check_pixclock(info->var.pixclock,1);
	vo_info->bpp = info->var.bits_per_pixel;
	vo_info->fps = info->var.pixclock / (info->var.xres * info->var.yres);
	if( vo_info->fps == 0 ) vo_info->fps = 60;

#ifdef CONFIG_GOVW_SCL_PATH
	if( info->node == 0 ){
		if( (g_vpp.ge_direct_init == 0) && (g_vpp.vpp_path == VPP_VPATH_GOVW_SCL) ){
			int size;
			int y_bpp,c_bpp;
			
			vo_info->resx = p_govrh->fb_p->fb.img_w;
			vo_info->resy = p_govrh->fb_p->fb.img_h;
			vo_info->pixclk = vpp_get_base_clock(vo_info->govr->mod);
			vo_info->fps = vo_info->pixclk / (vo_info->resx * vo_info->resy);
			vpp_get_colfmt_bpp(p_govrh->fb_p->fb.col_fmt,&y_bpp,&c_bpp);
			size = vpp_calc_align(p_govrh->fb_p->fb.img_w,VPP_FB_WIDTH_ALIGN/(y_bpp/8)) * p_govrh->fb_p->fb.img_h;
			p_govrh->fb_p->fb.y_size = size * y_bpp / 8;
			p_govrh->fb_p->fb.c_size = size * (y_bpp+c_bpp) / 8;
		}
	}
#endif

	vpp_config(vo_info);
	if( vo_info->govr ){
		vo_info->pixclk = vpp_get_base_clock(vo_info->govr->mod);
		info->var.pixclock = KHZ2PICOS(vo_info->pixclk/1000);
	}
	info->var.xres = vo_info->resx;
	info->var.yres = vo_info->resy;
	info->var.xres_virtual = vpp_calc_fb_width(g_vpp.mb_colfmt,info->var.xres);
	info->var.yres_virtual = info->var.yres * VPP_MB_ALLOC_NUM;
	
	if( info->node == 1 ){
		info->fix.smem_start = g_vpp.mb[0];
		info->fix.smem_len = g_vpp.mb_fb_size * VPP_MB_ALLOC_NUM;
		info->screen_base = mb_phys_to_virt(info->fix.smem_start);
		info->var.yres_virtual = info->var.yres * VPP_MB_ALLOC_NUM;
//		DPRINT("[VPP] fb1 smem 0x%x,len %d,base 0x%x\n",info->fix.smem_start,info->fix.smem_len,info->screen_base);
	}
	g_vpp.ge_direct_init = 1;
	vpp_set_mutex(info->node,0);
	
	DBG_MSG("fb%d,%dx%d,%d\n",info->node,info->var.xres,info->var.yres,info->var.pixclock);
	DBG_MSG("vir %dx%d\n",info->var.xres_virtual,info->var.yres_virtual);
	return 0;
}

int vpp_set_blank(struct fb_info *info,int blank)
{
	vout_info_t *vo_info;

	if( (g_vpp.dual_display == 0) && (info->node == 1) ){
		return 0;
	}

	DBG_MSG("(%d,%d)\n",info->node,blank);
	
	vo_info = vout_info_get_entry(info->node);
	vout_set_blank(vout_get_mask(vo_info),blank);
	if( g_vpp.virtual_display || (g_vpp.dual_display == 0) ){
		int plugin;
		
		plugin = vout_chkplug(VPP_VOUT_NUM_HDMI);

        vpp_clock_t tmr;
        govrh_get_tg(vo_info->govr,&tmr);

        if((tmr.end_pixel_of_active - tmr.begin_pixel_of_active != vo_info->resx)
            || (tmr.end_line_of_active - tmr.begin_line_of_active != vo_info->resy)){
            //govr_setting is set in UBOOT step
            //Maybe Different from wmt.display.param in OTT project
            printk("GOVR Setting Changed, Config Again\n");
            vo_info->force_config = 1;
        }
        
		vout_set_blank((0x1 << VPP_VOUT_NUM_HDMI),(plugin)?0:1);
		vout_set_blank((0x1 << VPP_VOUT_NUM_DVI),(plugin)?1:0);
	}
	return 0;
}

