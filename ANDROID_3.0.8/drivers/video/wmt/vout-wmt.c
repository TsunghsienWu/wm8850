/*++ 
 * linux/drivers/video/wmt/vout-wmt.c
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
#undef DEBUG
//#define DEBUG
//#define DEBUG_DETAIL

#include "vpp.h"

#ifndef CFG_LOADER
static int vo_plug_flag;
#endif
int vo_plug_vout;
int (*vo_plug_func)(int hotplug);
vout_mode_t dvo_vout_mode;
vout_mode_t int_vout_mode;
vpp_timing_t vo_oem_tmr;
int hdmi_cur_plugin;
vout_t *vo_poll_vout;

// GPIO 10 & 11
swi2c_reg_t vo_gpio_scl = {
	.bit_mask = BIT10,
	.gpio_en = (__GPIO_BASE + 0x40),
	.out_en = (__GPIO_BASE + 0x80),
	.data_in = (__GPIO_BASE + 0x00),
	.data_out = (__GPIO_BASE + 0xC0),
	.pull_en = (__GPIO_BASE + 0x480),
	.pull_en_bit_mask = BIT10,
};

swi2c_reg_t vo_gpio_sda = {
	.bit_mask = BIT11,
	.gpio_en = (__GPIO_BASE + 0x40),
	.out_en = (__GPIO_BASE + 0x80),
	.data_in = (__GPIO_BASE + 0x00),
	.data_out = (__GPIO_BASE + 0xC0),
	.pull_en = (__GPIO_BASE + 0x480),
	.pull_en_bit_mask = BIT11,
};

swi2c_handle_t vo_swi2c_dvi = { 
	.scl_reg = &vo_gpio_scl,
	.sda_reg = &vo_gpio_sda,
};

typedef struct {
	unsigned int virtual_display;
	unsigned int def_resx;
	unsigned int def_resy;
	unsigned int def_fps;
	unsigned int ub_resx;
	unsigned int ub_resy;
} vout_init_parm_t;

extern vout_dev_t *lcd_get_dev(void);
extern void hdmi_config_audio(vout_audio_t *info);

#define DVI_POLL_TIME_MS	100

/*--------------------------------------- API ---------------------------------------*/
int vo_i2c_proc(int id,unsigned int addr,unsigned int index,char *pdata,int len)
{
	swi2c_handle_t *handle = 0;
	int ret = 0;

	switch(id){
		case 1:	// dvi
			if( lvds_get_enable() )	// share pin with LVDS
				return -1;

			handle = &vo_swi2c_dvi;
			break;
		default:
			break;
	}

	if( handle ){
		if( wmt_swi2c_check(handle) ){
			return -1;
		}
		
		if( addr & 0x1 ){	// read
			*pdata = 0xff;
#ifdef CONFIG_WMT_EDID			
			ret = wmt_swi2c_read(handle,addr & ~0x1,index,pdata,len);
#else
			ret = -1;
#endif
		}
		else {	// write
			DBG_ERR("not support sw i2c write\n");
		}
	}
	return ret;
}

#ifndef CFG_LOADER
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
static void vo_do_plug(struct work_struct *ptr)
#else
static void vo_do_plug
(
	void *ptr		/*!<; // work input data */
)
#endif
{
	vout_t *vo;
	int plugin;

	if( vo_plug_func == 0 )
		return;

	vo = vout_get_entry(vo_plug_vout);
 	govrh_set_dvo_enable(vo->govr,1);
	plugin = vo_plug_func(1);
	govrh_set_dvo_enable(vo->govr,plugin);
	vout_change_status(vo,VPP_VOUT_STS_PLUGIN,plugin);
	vo_plug_flag = 0;
	DBG_DETAIL("vo_do_plug %d\n",plugin);
	vppif_reg32_write(GPIO_BASE_ADDR+0x300+VPP_VOINT_NO,0x80,7,1);	// GPIO irq enable
	vppif_reg32_write(GPIO_BASE_ADDR+0x80,0x1<<VPP_VOINT_NO,VPP_VOINT_NO,0x0);	// GPIO input mode
#ifdef __KERNEL__
	vpp_netlink_notify(USER_PID,DEVICE_PLUG_IN,plugin);
	vpp_netlink_notify(WP_PID,DEVICE_PLUG_IN,plugin);
#endif
	return;
}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
DECLARE_DELAYED_WORK(vo_plug_work, vo_do_plug);
#else
DECLARE_WORK(vo_plug_work,vo_do_plug,0);
#endif

static irqreturn_t vo_plug_interrupt_routine
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
	DBG_DETAIL("Enter\n");
	if( (vppif_reg8_in(GPIO_BASE_ADDR+0x360) &  (0x1<<VPP_VOINT_NO)) == 0 )
		return IRQ_NONE;

	vppif_reg8_out(GPIO_BASE_ADDR+0x360, 0x1<<VPP_VOINT_NO);	// clear int status
#ifdef __KERNEL__
//	if( vo_plug_flag == 0 ){
		vppif_reg32_write(GPIO_BASE_ADDR+0x300+VPP_VOINT_NO,0x80,7,0);	// GPIO irq disable
		schedule_delayed_work(&vo_plug_work, HZ/5);
		vo_plug_flag = 1;
//	}
#else
	if( vo_plug_func ) vo_do_plug(0);
#endif
	return IRQ_HANDLED;	
}

#define CONFIG_VO_POLL_WORKQUEUE
struct timer_list vo_poll_timer;
#ifdef CONFIG_VO_POLL_WORKQUEUE
static void vo_do_poll(struct work_struct *ptr)
{
	vout_t *vo;

	if( (vo = vo_poll_vout) ){
		if( vo->dev ){
			vo->dev->poll();
		}
		mod_timer(&vo_poll_timer,jiffies + msecs_to_jiffies(vo_poll_timer.data));
	}
	return;
}

DECLARE_DELAYED_WORK(vo_poll_work, vo_do_poll);
#else
struct tasklet_struct vo_poll_tasklet;
static void vo_do_poll_tasklet
(
	unsigned long data		/*!<; // tasklet input data */
)
{
	vout_t *vo;

	vpp_lock();
	if( (vo = vo_poll_vout) ){
		if( vo->dev ){
			vo->dev->poll();
		}
		mod_timer(&vo_poll_timer,jiffies + msecs_to_jiffies(vo_poll_timer.data));
	}
	vpp_unlock();
}
#endif

void vo_do_poll_tmr(int ms)
{
#ifdef CONFIG_VO_POLL_WORKQUEUE
	schedule_delayed_work(&vo_poll_work,msecs_to_jiffies(ms));
#else
	tasklet_schedule(&vo_poll_tasklet);
#endif
}

static void vo_set_poll(vout_t *vo,int on,int ms)
{
	if( on ){
		vo_poll_vout = vo;
		if( vo_poll_timer.function ){
			vo_poll_timer.data = ms/2;
			mod_timer(&vo_poll_timer,jiffies + msecs_to_jiffies(vo_poll_timer.data));
		}
		else {
			init_timer(&vo_poll_timer);
			vo_poll_timer.data = ms/2;
			vo_poll_timer.function = (void *) vo_do_poll_tmr;
			vo_poll_timer.expires = jiffies + msecs_to_jiffies(vo_poll_timer.data);
			add_timer(&vo_poll_timer);
		}
#ifndef CONFIG_VO_POLL_WORKQUEUE
		tasklet_init(&vo_poll_tasklet,vo_do_poll_tasklet,0);
#endif
	}
	else {
		del_timer(&vo_poll_timer);
#ifndef CONFIG_VO_POLL_WORKQUEUE
		tasklet_kill(&vo_poll_tasklet);
#endif
		vo_poll_vout = 0;
	}
}
#endif //fan

void vout_set_int_type(int type)
{
	unsigned char reg;

	reg = vppif_reg8_in(GPIO_BASE_ADDR+0x300+VPP_VOINT_NO);
	reg &= ~0x7;
	switch( type ){
		case 0:	// low level
		case 1:	// high level
		case 2:	// falling edge
		case 3:	// rising edge
		case 4:	// rising edge or falling
			reg |= type;
			break;
		default:
			break;
	}
	vppif_reg8_out(GPIO_BASE_ADDR+0x300+VPP_VOINT_NO,reg);
}

static void vo_plug_enable(int enable,void *func,int no)
{
	vout_t *vo;
	
	DBG_DETAIL("%d\n",enable);
	vo_plug_vout = no;
	vo = vout_get_entry(no);
#ifdef CONFIG_WMT_EXT_DEV_PLUG_DISABLE
	vo_plug_func = 0;
 	govrh_set_dvo_enable(vo->govr,enable);
#else
	vo_plug_func = func;
	if( vo_plug_func == 0 )
		return;

	if( enable ){
		vppif_reg32_write(GPIO_BASE_ADDR+0x40,0x1<<VPP_VOINT_NO,VPP_VOINT_NO,0x0);	// GPIO disable
		vppif_reg32_write(GPIO_BASE_ADDR+0x80,0x1<<VPP_VOINT_NO,VPP_VOINT_NO,0x0);	// GPIO input mode
		vppif_reg32_write(GPIO_BASE_ADDR+0x480,0x1<<VPP_VOINT_NO,VPP_VOINT_NO,0x1);	// GPIO pull enable
		vppif_reg32_write(GPIO_BASE_ADDR+0x4c0,0x1<<VPP_VOINT_NO,VPP_VOINT_NO,0x1);	// GPIO pull-up
#ifndef CFG_LOADER
		vo_do_plug(0);
		if ( vpp_request_irq(IRQ_GPIO, vo_plug_interrupt_routine, IRQF_SHARED, "vo plug", (void *) &vo_plug_vout) ) {
			DBG_ERR("request GPIO ISR fail\n");
		}
		vppif_reg32_write(GPIO_BASE_ADDR+0x300+VPP_VOINT_NO,0x80,7,1);					// GPIO irq enable
	}
	else {
		vpp_free_irq(IRQ_GPIO,(void *) &vo_plug_vout);
#endif //fan
	}
#endif	
}

/*--------------------------------------- DVI ---------------------------------------*/
#ifdef WMT_FTBLK_VOUT_DVI
static int vo_dvi_blank(vout_t *vo,vout_blank_t arg)
{
	DBG_DETAIL("(%d)\n",arg);
	if( vo->pre_blank == VOUT_BLANK_POWERDOWN ){
		if( vo->dev ){
			vo->dev->init(vo);
#ifdef __KERNEL__
			if( vo->dev->poll ){
				vo_set_poll(vo,(vo->dev->poll)?1:0,DVI_POLL_TIME_MS);
			}
#endif
		}
	}
#ifdef __KERNEL__
	if( arg == VOUT_BLANK_POWERDOWN ){
		vo_set_poll(vo,0,0);
	}

	// patch for virtual fb,if HDMI plugin then DVI blank
	if( g_vpp.virtual_display || (g_vpp.dual_display == 0) ){
		if( vout_chkplug(VPP_VOUT_NUM_HDMI) ){
			arg = VOUT_BLANK_NORMAL;
			vout_change_status(vo,VPP_VOUT_STS_BLANK,arg);
		}
	}
#endif
	govrh_set_dvo_enable(vo->govr,(arg==VOUT_BLANK_UNBLANK)? 1:0);
	vo->pre_blank = arg;
	return 0;
}

static int vo_dvi_config(vout_t *vo,int arg)
{
//	vout_info_t *vo_info;

	DBG_DETAIL("Enter\n");

	return 0;
}

static int vo_dvi_init(vout_t *vo,int arg)
{
	unsigned int clk_delay;

	DBG_DETAIL("(%d)\n",arg);

	govrh_set_dvo_color_format(vo->govr,vo->option[0]);
	govrh_set_dvo_outdatw(vo->govr,vo->option[1] & BIT0);	// bit0:0-12 bit,1-24bit
	govrh_IGS_set_mode(vo->govr,0,(vo->option[1]&0x600)>>9,(vo->option[1]&0x800)>>11);
	govrh_IGS_set_RGB_swap(vo->govr,(vo->option[1]&0x3000)>>12);
	clk_delay = ( vo->option[1] & BIT0 )? VPP_GOVR_DVO_DELAY_24:VPP_GOVR_DVO_DELAY_12;
	govrh_set_dvo_clock_delay(vo->govr,((clk_delay & BIT14)!=0x0),clk_delay & 0x3FFF);
	govrh_set_dvo_sync_polar(vo->govr, 1, 1);  //added by howayhuo for virtual_display

	if( vo->dev ){
		vo->dev->set_mode(&vo->option[0]);
		vo->dev->set_power_down(VPP_FLAG_DISABLE);
		if( vo->dev->interrupt ){
			vo_plug_enable(VPP_FLAG_ENABLE,vo->dev->interrupt,vo->num);
		}
#ifdef __KERNEL__
		if( vo->dev->poll ){
			vo_set_poll(vo,(vo->dev->poll)?1:0,DVI_POLL_TIME_MS);
		}
#endif
		vout_change_status(vo,VPP_VOUT_STS_PLUGIN,vo->dev->check_plugin(0));
	}
	vo->govr->fb_p->set_csc(vo->govr->fb_p->csc_mode);
	govrh_set_dvo_enable(vo->govr,(vo->status & VPP_VOUT_STS_BLANK)? VPP_FLAG_DISABLE:VPP_FLAG_ENABLE);
	return 0;
}

static int vo_dvi_uninit(vout_t *vo,int arg)
{
	DBG_DETAIL("(%d)\n",arg);

	vo_plug_enable(VPP_FLAG_DISABLE,0,VPP_VOUT_NUM);
	govrh_set_dvo_enable(vo->govr,VPP_FLAG_DISABLE);
#ifdef __KERNEL__
	vo_set_poll(vo,0,DVI_POLL_TIME_MS);
#endif
	return 0;
}

static int vo_dvi_chkplug(vout_t *vo,int arg)
{
	int plugin = 1;

	DBG_MSG("plugin %d\n",plugin);
	return plugin;
}

static int vo_dvi_get_edid(vout_t *vo,int arg)
{
	char *buf;
	int i,cnt;

	DBG_DETAIL("Enter\n");

	buf = (char *) arg;
	memset(&buf[0],0x0,128*EDID_BLOCK_MAX);
	if( vpp_i2c_read(VPP_DVI_EDID_ID,0xA0,0,&buf[0],128) ){
		DBG_ERR("read edid\n");
		return 1;
	}

	if( edid_checksum(buf,128) ){
		DBG_ERR("checksum\n");
		return 1;
	}

	cnt = buf[0x7E];
	if( cnt >= 3 ) cnt = 3;
	for(i=1;i<=cnt;i++){
		vpp_i2c_read(VPP_DVI_EDID_ID,0xA0,0x80*i,&buf[128*i],128);
	}
	return 0;
}

vout_inf_t vo_dvi_inf = 
{
	.mode = VOUT_INF_DVI,
	.init = vo_dvi_init,
	.uninit = vo_dvi_uninit,
	.blank = vo_dvi_blank,
	.config = vo_dvi_config,
	.chkplug = vo_dvi_chkplug,
	.get_edid = vo_dvi_get_edid,
};

int vo_dvi_initial(void)
{
	vout_inf_register(VOUT_INF_DVI,&vo_dvi_inf);
	return 0;
}
module_init(vo_dvi_initial);

#endif /* WMT_FTBLK_VOUT_DVI */

/*--------------------------------------- HDMI ---------------------------------------*/
void vo_hdmi_set_clock(int enable)
{
	DBG_DETAIL("(%d)\n",enable);

	enable = (enable)? CLK_ENABLE:CLK_DISABLE;
	auto_pll_divisor(DEV_HDMII2C,enable,0,0);
	auto_pll_divisor(DEV_HDMI,enable,0,0);
	auto_pll_divisor(DEV_HDCE,enable,0,0);
}

#ifdef WMT_FTBLK_VOUT_HDMI
#define VOUT_HDMI_PLUG_DELAY	100	// plug stable delay ms
#define VOUT_HDMI_CP_TIME 		3	// should more than 2 seconds

#ifdef __KERNEL__
struct timer_list hdmi_cp_timer;
#endif

void vo_hdmi_cp_set_enable_tmr(int sec)
{
	DBG_MSG("[HDMI] set enable tmr %d sec\n",sec);

	if( sec == 0 ){
		hdmi_set_cp_enable(VPP_FLAG_ENABLE);
		return ;
	}
	
#ifdef __KERNEL__
	if( hdmi_cp_timer.function ){
		mod_timer(&hdmi_cp_timer,(jiffies + sec * HZ));
	}
	else {
		init_timer(&hdmi_cp_timer);
		hdmi_cp_timer.data = VPP_FLAG_ENABLE;
		hdmi_cp_timer.function = (void *) hdmi_set_cp_enable;
		hdmi_cp_timer.expires = jiffies + sec * HZ;
		add_timer(&hdmi_cp_timer);
	}
#else
	mdelay(sec*1000);
	hdmi_set_cp_enable(VPP_FLAG_ENABLE);
#endif	
}

static int vo_hdmi_blank(vout_t *vo,vout_blank_t arg)
{
	int enable;

	DBG_DETAIL("(%d)\n",arg);

	enable = (arg == VOUT_BLANK_UNBLANK)? 1:0;
	if( g_vpp.hdmi_cp_enable && enable ){
		vo_hdmi_cp_set_enable_tmr((g_vpp.hdmi_certify_flag)? 0:2);
	}
	else {
		hdmi_set_cp_enable(VPP_FLAG_DISABLE);
	}
	hdmi_set_enable(enable);
#ifdef __KERNEL__
	if( arg == VOUT_BLANK_POWERDOWN ){
		vpp_netlink_notify(USER_PID,DEVICE_PLUG_IN,0);
		vpp_netlink_notify(WP_PID,DEVICE_PLUG_IN,0);
		vout_change_status(vo,VPP_VOUT_STS_PLUGIN,0);
	}
#endif
	return 0;
}

#ifndef CFG_LOADER
static irqreturn_t vo_hdmi_cp_interrupt
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
	vout_t *vo;

	DBG_DETAIL("%d\n",irq);
	vo = vout_get_entry(VPP_VOUT_NUM_HDMI);
	switch( hdmi_check_cp_int() ){
		case 1:
			hdmi_set_cp_enable(VPP_FLAG_DISABLE);
			vo_hdmi_cp_set_enable_tmr(VOUT_HDMI_CP_TIME);
			vout_change_status(vo,VPP_VOUT_STS_CONTENT_PROTECT,0);
			break;
		case 0:
			vout_change_status(vo,VPP_VOUT_STS_CONTENT_PROTECT,1);
			if( !g_vpp.hdmi_certify_flag ){
				if( g_vpp.hdmi_bksv[0] == 0 ){
					hdmi_get_bksv(&g_vpp.hdmi_bksv[0]);
					DBG_MSG("get BKSV 0x%x 0x%x\n",g_vpp.hdmi_bksv[0],g_vpp.hdmi_bksv[1]);
				}
			}
			break;
		case 2:
			hdmi_ri_tm_cnt = 3 * 30;
			break;
		default:
			break;
	}
	return IRQ_HANDLED;	
}

#ifdef __KERNEL__
static void vo_hdmi_do_plug(struct work_struct *ptr)
#else
static void vo_hdmi_do_plug(void)
#endif
{
	vout_t *vo;
	int plugin;
	int option = 0;

	plugin = hdmi_check_plugin(1);
	vo = vout_get_entry(VPP_VOUT_NUM_HDMI);
	vout_change_status(vo,VPP_VOUT_STS_PLUGIN,plugin);
	if( plugin ){
		option = vout_get_edid_option(VPP_VOUT_NUM_HDMI);
#ifdef CONFIG_VPP_DEMO
		option |= (EDID_OPT_HDMI + EDID_OPT_AUDIO);
#endif
	}
	else {
		g_vpp.hdmi_bksv[0] = g_vpp.hdmi_bksv[1] = 0;
	}
	hdmi_set_option(option);
	vo_hdmi_blank(vo,(vo->status & VPP_VOUT_STS_BLANK)? 1:!(plugin));
//--> added by howayhuo for VGA virtual display
	if(g_vpp.virtual_display && cs8556_tvformat_is_set())
	{
		if(plugin)
			cs8556_disable();
		else
			cs8556_enable();
	}
//<-- end add
	if( !g_vpp.hdmi_certify_flag ){
		hdmi_hotplug_notify(plugin);
	}
#ifdef CONFIG_VPP_OVERSCAN
	if( plugin ){
		vpp_plugin_reconfig(VPP_VOUT_NUM_HDMI);
	}
#endif
	DBG_MSG("%d\n",plugin);
	return;
}
DECLARE_DELAYED_WORK(vo_hdmi_plug_work, vo_hdmi_do_plug);

static irqreturn_t vo_hdmi_plug_interrupt
(
	int irq, 				/*!<; // irq id */
	void *dev_id 			/*!<; // device id */
)
{
	DBG_MSG("vo_hdmi_plug_interrupt %d\n",irq);
	hdmi_clear_plug_status();

#ifdef __KERNEL__	
	if( !g_vpp.hdmi_certify_flag ){
		schedule_delayed_work(&vo_hdmi_plug_work,msecs_to_jiffies(VOUT_HDMI_PLUG_DELAY));
	}
	else 
#endif
	{
		vo_hdmi_do_plug(0);
	}
	return IRQ_HANDLED;	
}
#endif //fan

static int vo_hdmi_init(vout_t *vo,int arg)
{
	DBG_DETAIL("(%d)\n",arg);

	vo_hdmi_set_clock(1);
	vout_change_status(vout_get_entry(VPP_VOUT_NUM_HDMI),VPP_VOUT_STS_PLUGIN,hdmi_check_plugin(0));
	lvds_set_enable(0);
	hdmi_enable_plugin(1);
#ifndef CFG_LOADER
	if ( vpp_request_irq(VPP_IRQ_HDMI_CP, vo_hdmi_cp_interrupt, SA_INTERRUPT, "hdmi cp", (void *) 0) ) {
		DBG_ERR("*E* request HDMI ISR fail\n");
	}
	if ( vpp_request_irq(VPP_IRQ_HDMI_HPDH, vo_hdmi_plug_interrupt, SA_INTERRUPT, "hdmi plug", (void *) 0) ) {
		DBG_ERR("*E* request HDMI ISR fail\n");
	}
	if ( vpp_request_irq(VPP_IRQ_HDMI_HPDL, vo_hdmi_plug_interrupt, SA_INTERRUPT, "hdmi plug", (void *) 0) ) {
		DBG_ERR("*E* request HDMI ISR fail\n");
	}
#endif
	hdmi_set_enable((vo->status & VPP_VOUT_STS_BLANK)? VPP_FLAG_DISABLE:VPP_FLAG_ENABLE);
	return 0;
}

static int vo_hdmi_uninit(vout_t *vo,int arg)
{
	DBG_DETAIL("(%d)\n",arg);
	hdmi_enable_plugin(0);
	hdmi_set_cp_enable(VPP_FLAG_DISABLE);
	hdmi_set_enable(VPP_FLAG_DISABLE);
#ifndef CFG_LOADER
	vpp_free_irq(VPP_IRQ_HDMI_CP,(void *) 0);
	vpp_free_irq(VPP_IRQ_HDMI_HPDH,(void *) 0);
	vpp_free_irq(VPP_IRQ_HDMI_HPDL,(void *) 0);
#endif
	vo_hdmi_set_clock(0);
	return 0;
}

static int vo_hdmi_config(vout_t *vo,int arg)
{
	vout_info_t *vo_info;
	hdmi_info_t hdmi_info;
	vdo_color_fmt colfmt;

	hdmi_set_enable(0);
	vo_info = (vout_info_t *) arg;

	DBG_DETAIL("(%dx%d@%d)\n",vo_info->resx,vo_info->resy,vo_info->fps);
	
	// 1280x720@60, HDMI pixel clock 74250060 not 74500000
	if( (vo_info->resx == 1280) && (vo_info->resy == 720) ){
		if( vo_info->pixclk == 74500000 ){
			vo_info->pixclk = 74250060;
		}
	}
	colfmt = (vo->option[0]==VDO_COL_FMT_YUV422V)? VDO_COL_FMT_YUV422H:vo->option[0];
	hdmi_cur_plugin = hdmi_check_plugin(0);
	hdmi_info.option = (hdmi_cur_plugin)? vout_get_edid_option(VPP_VOUT_NUM_HDMI):0;
	hdmi_info.channel = g_vpp.hdmi_audio_channel;
	hdmi_info.freq = g_vpp.hdmi_audio_freq;
	hdmi_info.outfmt = colfmt;
	hdmi_info.vic = VPP_GET_OPT_HDMI_VIC(vo_info->option);
	govrh_set_csc_mode(vo->govr,vo->govr->fb_p->csc_mode);
	hdmi_set_sync_low_active((vo_info->option & VPP_VGA_HSYNC_POLAR_HI)? 0:1
								,(vo_info->option & VPP_VGA_VSYNC_POLAR_HI)? 0:1);
	hdmi_config(&hdmi_info);
	mdelay(200);	// patch for VIZIO change resolution issue
	vo_hdmi_blank(vo,(vo->status & VPP_VOUT_STS_BLANK)? 1:!(hdmi_cur_plugin));
	return 0;
}

static int vo_hdmi_chkplug(vout_t *vo,int arg)
{
	int plugin;
	plugin = hdmi_get_plugin();
	DBG_DETAIL("%d\n",plugin);
	return plugin;
}

static int vo_hdmi_get_edid(vout_t *vo,int arg)
{
	char *buf;
#ifdef CONFIG_WMT_EDID
	int i,cnt;
#endif
	DBG_DETAIL("Enter\n");
	buf = (char *) arg;
#ifdef CONFIG_WMT_EDID
	memset(&buf[0],0x0,128*EDID_BLOCK_MAX);
	if( hdmi_DDC_read(0xA0,0x0,&buf[0],128) ){
		DBG_ERR("read edid\n");
		return 1;
	}

	if( edid_checksum(buf,128) ){
		DBG_ERR("hdmi checksum\n");
		g_vpp.dbg_hdmi_ddc_crc_err++;
		return 1;
	}

	cnt = buf[0x7E];
	if( cnt >= 3 ) cnt = 3;
	for(i=1;i<=cnt;i++){
		hdmi_DDC_read(0xA0,0x80*i,&buf[128*i],128);
	}
#endif
	return 0;
}

vout_inf_t vo_hdmi_inf = 
{
	.mode = VOUT_INF_HDMI,
	.init = vo_hdmi_init,
	.uninit = vo_hdmi_uninit,
	.blank = vo_hdmi_blank,
	.config = vo_hdmi_config,
	.chkplug = vo_hdmi_chkplug,
	.get_edid = vo_hdmi_get_edid,
};

int vo_hdmi_initial(void)
{
	vout_inf_register(VOUT_INF_HDMI,&vo_hdmi_inf);
	return 0;
}
module_init(vo_hdmi_initial);

#endif /* WMT_FTBLK_VOUT_HDMI */

/*--------------------------------------- LVDS ---------------------------------------*/
#ifdef WMT_FTBLK_VOUT_LVDS
int vo_lvds_init_flag;
static int vo_lvds_blank(vout_t *vo,vout_blank_t arg)
{
	DBG_DETAIL("(%d)\n",arg);
	lvds_set_enable((arg==VOUT_BLANK_UNBLANK)? 1:0);
	if( arg == VOUT_BLANK_POWERDOWN ){
		vppif_reg32_write(LVDS_TRE_EN,0);
	}
	if( vo_lvds_init_flag )
		lvds_set_power_down(arg);
	return 0;
}

static int vo_lvds_config(vout_t *vo,int arg)
{
	DBG_DETAIL("(%d)\n",arg);
	lvds_set_power_down(VPP_FLAG_DISABLE);
	vo_lvds_init_flag = 1;
	return 0;
}

static int vo_lvds_init(vout_t *vo,int arg)
{
	DBG_DETAIL("(%d)\n",arg);

	vo_hdmi_set_clock(1);
	if( vo->dev ){
		vo->dev->set_mode(&vo->option[0]);
	}
//	lvds_set_power_down(VPP_FLAG_DISABLE);
	govrh_set_csc_mode(vo->govr,vo->govr->fb_p->csc_mode);
	lvds_set_rgb_type( vo->option[1] );
	lvds_set_sync_polar((vo->option[2] & VPP_DVO_SYNC_POLAR_HI)? 0:1,(vo->option[2] & VPP_DVO_VSYNC_POLAR_HI)? 0:1);
	lvds_set_enable((vo->status & VPP_VOUT_STS_BLANK)? VPP_FLAG_DISABLE:VPP_FLAG_ENABLE);
	return 0;
}

static int vo_lvds_uninit(vout_t *vo,int arg)
{
	DBG_DETAIL("(%d)\n",arg);
	lvds_set_enable(VPP_FLAG_DISABLE);
	if( vo->dev ){
		vo->dev->set_mode(0);
	}
	lvds_set_power_down(VPP_FLAG_ENABLE);
	vo_hdmi_set_clock(0);
	vo_lvds_init_flag = 0;
	return 0;
}

static int vo_lvds_chkplug(vout_t *vo,int arg)
{
	DBG_DETAIL("\n");
#if 0	
	vo = vout_get_info(VOUT_LVDS);
	if( vo->dev ){
		return vo->dev->check_plugin(0);
	}
#endif
	return 1;
}

vout_inf_t vo_lvds_inf = 
{
	.mode = VOUT_INF_LVDS,
	.init = vo_lvds_init,
	.uninit = vo_lvds_uninit,
	.blank = vo_lvds_blank,
	.config = vo_lvds_config,
	.chkplug = vo_lvds_chkplug,
#ifdef WMT_FTBLK_VOUT_HDMI
	.get_edid = vo_hdmi_get_edid,
#endif
};

int vo_lvds_initial(void)
{
	vout_inf_register(VOUT_INF_LVDS,&vo_lvds_inf);
	return 0;
}
module_init(vo_lvds_initial);

#endif /* WMT_FTBLK_VOUT_LVDS */
/*--------------------------------------- API ---------------------------------------*/
#ifndef CFG_LOADER
int vout_set_audio(vout_audio_t *arg)
{
	vout_t *vout;
	int ret = 0;

#if 0	
	vout = vout_get_info(VPP_VOUT_DVO2HDMI);
	if( vout && (vout->status & VPP_VOUT_STS_PLUGIN)){
		if( vout->dev->set_audio ){
			vout->dev->set_audio(arg);
		}
	}
#endif

#ifdef WMT_FTBLK_VOUT_HDMI
	vout = vout_get_entry(VPP_VOUT_NUM_HDMI);
//	if( vout && (vout->status & VPP_VOUT_STS_PLUGIN)){
	if( vout ){
		hdmi_config_audio(arg);
		ret = 1;
	}
#endif	
	return ret;
}

void vout_plug_detect(int no)
{
	int plugin = -1;

	switch(no){
#ifdef WMT_FTBLK_VOUT_HDMI
		case VPP_VOUT_NUM_HDMI:
			if( hdmi_poll_cp_status() ){
				vo_hdmi_cp_set_enable_tmr(VOUT_HDMI_CP_TIME);
			}
			break;
#endif
		default:
			break;
	}

	if( plugin >= 0 ){
		vout_t *vo;

		vo = vout_get_entry(no);
		vout_change_status(vo,VPP_VOUT_STS_PLUGIN,plugin);
	}
}
#endif

/* 3445 port1 : DVI/SDD, port2 : VGA/SDA, port3 : HDMI/LVDS */
/* 3481 port1 : HDMI/LVDS, port2 : DVI */
vout_t vout_entry_0 = {
	.fix_cap = BIT(VOUT_INF_HDMI) + BIT(VOUT_INF_LVDS),
	.option[0] = VDO_COL_FMT_ARGB,
	.option[1] = VPP_DATAWIDHT_24,
	.option[2] = 0,
};

vout_t vout_entry_1 = {
	.fix_cap = BIT(VOUT_INF_DVI) + VOUT_CAP_EXT_DEV + 0x100,	// i2c bus 1,ext dev
	.option[0] = VDO_COL_FMT_ARGB,
	.option[1] = VPP_DATAWIDHT_24,
	.option[2] = 0,
};

void vout_init_param(vout_init_parm_t *init_parm)
{
	char buf[100];
	int varlen = 100;
	unsigned int parm[10];

	memset(init_parm,0,sizeof(vout_init_parm_t));

	// register vout & set default
	vout_register(0,&vout_entry_0);
	vout_entry_0.inf = vout_inf_get_entry(VOUT_INF_HDMI);
	vout_register(1,&vout_entry_1);
	vout_entry_1.inf = vout_inf_get_entry(VOUT_INF_DVI);

	/* [uboot parameter] up bound : resx:resy */
	if( wmt_getsyspara("wmt.display.upbound",buf,&varlen) == 0){
		vpp_parse_param(buf,parm,2,0);
		init_parm->ub_resx = parm[0];
		init_parm->ub_resy = parm[1];
		MSG("up bound(%d,%d)\n",init_parm->ub_resx,init_parm->ub_resy);
	}

	/* [uboot parameter] default resolution : resx:resy:fps */
	init_parm->def_resx = VOUT_INFO_DEFAULT_RESX;
	init_parm->def_resy = VOUT_INFO_DEFAULT_RESY;
	init_parm->def_fps = VOUT_INFO_DEFAULT_FPS;
	if( wmt_getsyspara("wmt.display.default.res",buf,&varlen) == 0 ){
		vpp_parse_param(buf,parm,3,0);
		init_parm->def_resx = parm[0];
		init_parm->def_resy = parm[1];
		init_parm->def_fps = parm[2];
		MSG("default res(%d,%d,%d)\n",init_parm->def_resx,init_parm->def_resy,init_parm->def_fps);
	}
}

const char *vout_sys_parm_str[] = {"wmt.display.param","wmt.display.param2"};
int vout_check_display_info(vout_init_parm_t *init_parm)
{
	char buf[100];
	int varlen = 100;
	unsigned int parm[10];
	vout_t *vo = 0;
	int ret = 1;
	int i;

	DBG_DETAIL("Enter\n");

	/* [uboot parameter] display param : type:op1:op2:resx:resy:fps */
	for(i=0;i<2;i++){
		if( wmt_getsyspara((char *)vout_sys_parm_str[i],buf,&varlen) == 0 ){
			int info_no;
			
			MSG("display param %d : %s\n",i,buf);
			vpp_parse_param(buf,(unsigned int *)parm,6,0);
			
			//--> added by howayhuo for support CS8556 VGA Encoder
			if(parm[0] == VOUT_LCD)	
			{
				if(parm[1] != LCD_MYSON_CS8556)
				{
					char buf2[100];
					unsigned int param2[10];

					if( wmt_getsyspara("wmt.display.tmr",buf2,&varlen) == 0 )
					{
						vpp_parse_param(buf2,param2,10,0);
						parm[3] = param2[4];
						parm[4] = param2[8];
					}
				}
				else
				{
					vpp_timing_t timing;
				
					lcd_cs8556_initial();
					cs8556_get_timing(&timing);

					parm[3] = timing.hpixel;
					parm[4] = timing.vpixel;
					parm[5] = cs8556_get_fps();
				}
			}
			//<-- end add

			MSG("boot parm vo %d opt %d,%d, %dx%d@%d\n",parm[0],parm[1],parm[2],parm[3],parm[4],parm[5]);
#ifdef CONFIG_VPP_VIRTUAL_DISPLAY
			if( parm[0] == VOUT_BOOT ){
				MSG("[VOUT] virtual display\n");
				init_parm->def_resx = parm[3];
				init_parm->def_resy = parm[4];
				init_parm->def_fps = parm[5];
				ret = 1;
#ifdef CONFIG_UBOOT
				g_vpp.dual_display = 0;
				g_vpp.virtual_display = 1;
#else
				init_parm->virtual_display = 1;
#if 1
                //whether HDMI plugin or not, fb0 is DVI always
                //if( vout_chkplug(VPP_VOUT_NUM_HDMI) == 0 )
                {
                    if( (vo = vout_get_entry(VPP_VOUT_NUM_DVI)) ){
                        vo->resx = init_parm->def_resx;
                        vo->resy = init_parm->def_resy;
                        vo->pixclk = init_parm->def_fps;
                        vout_info_add_entry(vo);
                    }
                }
#endif

#endif
				//--> added by howayhuo for virtual_display
				g_vpp.virtual_display = 1; 

				if(cs8556_tvformat_is_set())
				{
					lcd_cs8556_initial();
					if(!vout_chkplug(VPP_VOUT_NUM_HDMI))
					{
						vpp_timing_t timing;
					
						if( cs8556_get_output_format() == VGA)
						{	
							if(!cs8556_vga_get_edid())
							{
								cs8556_get_timing(&timing);
						
								init_parm->def_resx = timing.hpixel;
								init_parm->def_resy = timing.vpixel;
								init_parm->def_fps =  cs8556_get_fps();
							}
						}
					}
				}
				//<-- end add

				break;
			}
#endif
			if( (vo = vout_get_entry_adapter(parm[0])) == 0 ){
				ret = 1;
				DBG_ERR("uboot param invalid\n");
				break;
			}
			vo->inf = vout_get_inf_entry_adapter(parm[0]);
			vo->option[0] = parm[1];
			vo->option[1] = parm[2];
			vo->resx = parm[3];
			vo->resy = parm[4];
			vo->pixclk = parm[5];
			info_no = vout_info_add_entry(vo);
			vout_change_status(vo,VPP_VOUT_STS_BLANK,(parm[2]&VOUT_OPT_BLANK)? VOUT_BLANK_NORMAL:VOUT_BLANK_UNBLANK);
			switch( parm[0] ){
				case VOUT_LCD:
					{
						vout_dev_t *dev;
						
						lcd_set_parm(parm[1],parm[2]&0xFF);
						dev = lcd_get_dev();
						vo->ext_dev = dev;
						vo->dev = dev;
						dev->vout = vo;
						vo->option[0] = VDO_COL_FMT_ARGB;
						vo->option[1] &= ~0xFF;
						vo->option[1] |= VPP_DATAWIDHT_24;
					}
					break;
				case VOUT_LVDS:
					{
						vpp_timing_t *timing;
						vout_info_t *info;

						MSG("[VOUT] LVDS parm\n");
						
						info = vout_info_get_entry(info_no);
						timing = 0;
						if( (parm[1] == 0) && ( parm[3] == 0 ) && ( parm[4] == 0 ) ){ // auto detect
							if( vout_get_edid_option(vo->num) ){
								timing = &vo->edid_info.detail_timing[0];
								if( timing->pixel_clock == 0 ){
									timing = 0;
									DBG_ERR("LVDS timing\n");
								}
							}
								
							if( vo->inf->get_edid(vo,(int)vo->edid) == 0 ){
								if( edid_parse(vo->edid,&vo->edid_info) ){
								}
								else {
									DBG_ERR("LVDS edid parse\n");
								}
							}
							else {
								DBG_ERR("LVDS edid read\n");
							}
						}

						if( timing == 0 ){ // use internal timing
							lcd_parm_t *p = 0;

							if( parm[1] ){
								p = lcd_get_parm(parm[1],parm[2]);
							}

							if( p == 0 )
								p = lcd_get_oem_parm(parm[3],parm[4]);
							timing = &p->timing;
						}
						memcpy(&vo_oem_tmr,timing,sizeof(vpp_timing_t));
						vo->option[2] = timing->option;
						info->resx = timing->hpixel;
						info->resy = timing->vpixel;
						info->fps  = timing->option & EDID_TMR_FREQ;
						vout_info_set_fixed_timing(info_no,&vo_oem_tmr);
					}
					break;
				default:
					break;
			}
			ret = 0;
		}
	}

#ifndef CONFIG_HW_GOVR2_DVO
	/* disable dual display if no display param */
	if( ret ){
		g_vpp.dual_display = 0;
	}
#endif
	
	if( g_vpp.dual_display ){
		MSG("[VOUT] dual display\n");
	}

	// add vout entry to info
	for(i=0;i<VPP_VOUT_NUM;i++){
		if( (vo = vout_get_entry(i)) ){
			if( vo->resx == 0 ){
				vo->resx = init_parm->def_resx;
				vo->resy = init_parm->def_resy;
				vo->pixclk = init_parm->def_fps;
			}
			vout_info_add_entry(vo);
		}
	}
	DBG_DETAIL("Leave\n");
	return ret;
}

void vout_check_ext_device(void)
{
	char buf[100];
	int varlen = 100;
	vout_t *vo = 0;
	vout_dev_t *dev = 0;
	int i;
	vout_info_t *info;

	DBG_DETAIL("Enter\n");
	vpp_i2c_init(1,0xA0);

	/* [uboot parameter] reg operation : addr op val */
	if( wmt_getsyspara("wmt.display.regop",buf,&varlen) == 0){
	    unsigned int addr;
	    unsigned int val;
	    char op;
	    char *p,*endp;

		p = buf;
	    while(1){
	        addr = simple_strtoul(p, &endp, 16);
	        if( *endp == '\0')
	            break;

	        op = *endp;
	        if( endp[1] == '~'){
	            val = simple_strtoul(endp+2, &endp, 16);
	            val = ~val;
	        }
	        else {
	            val = simple_strtoul(endp+1, &endp, 16);
	        }

	        DBG_DETAIL("  reg op: 0x%X %c 0x%X\n", addr, op, val);
	        switch(op){
	            case '|': REG32_VAL(addr) |= val; break;
	            case '=': REG32_VAL(addr) = val; break;
	            case '&': REG32_VAL(addr) &= val; break;
	            default:
	                DBG_ERR("Error, Unknown operator %c\n", op);
	        }

	        if(*endp == '\0')
	            break;
	        p = endp + 1;
	    }
	}

	/* [uboot parameter] dvi device : name:i2c id */
	vo = vout_get_entry(VPP_VOUT_NUM_DVI);
	info = vout_get_info_entry(VPP_VOUT_NUM_DVI);
	if( lcd_get_dev() ){
		vo->dev->init(vo);
		vout_info_set_fixed_timing(info->num,&p_lcd->timing);
	}
	else if( vo->ext_dev == 0 ){
		if( wmt_getsyspara("wmt.display.dvi.dev",buf,&varlen) == 0 ){
			unsigned int param[1];
			char *p;

			p = strchr(buf,':');
			*p = 0;
			vpp_parse_param(p+1,param,1,0x1);
			vpp_i2c_release();
			vpp_i2c_init(param[0],0xA0);
			MSG("dvi dev %s : %x\n",buf,param[0]);
			do {		
				dev = vout_get_device(dev);
				if( dev == 0 ) break;
				if( strcmp(buf,dev->name) == 0 ){
					vo->ext_dev = dev;
					vo->dev = dev;
					dev->vout = vo;
					
					// probe & init external device
					if( dev->init(vo) ){
						DBG_ERR("%s not exist\n",dev->name);
						vo->dev = vo->ext_dev = 0;
						break;
					}
					else {
						MSG("[VOUT] dvi dev %s\n",buf);
					}

					// if LCD then set fixed timing
					if( vo->dev == lcd_get_dev() ){
						vout_info_set_fixed_timing(info->num,&p_lcd->timing);
					}
				}
			} while(1);
		}
	}

	for(i=0;i<VPP_VOUT_NUM;i++){
		if( (vo = vout_get_entry(i)) == 0 )
			continue;

		if( (vo->fix_cap & VOUT_CAP_EXT_DEV) && !(vo->ext_dev) ){
			dev = 0;
			do {
				dev = vout_get_device(dev);
				if( dev == 0 ) break;
				if( vo->fix_cap & BIT(dev->mode) ){
					vo->inf = vout_inf_get_entry(dev->mode);
					if( dev->init(vo) == 0 ){
						vo->ext_dev = dev;
						vo->dev = dev;
						dev->vout = vo;
						break;
					}
				}
			} while(1);
		}
		DBG_MSG("vout %d ext dev : %s\n",i,(vo->dev)? vo->dev->name:"NO");
	}

	/* [uboot parameter] pwm : id:freq:level or id:scalar:period:duty */
#ifdef CONFIG_LCD_BACKLIGHT_WMT
	if( wmt_getsyspara("wmt.display.pwm",buf,&varlen) == 0 ){
		unsigned int id,parm1,parm2,parm3;
		unsigned int param[4];
		int parm_cnt;
		
		DBG_MSG("pwm %s\n",buf);
		parm_cnt = vpp_parse_param(buf,(unsigned int *)param,4,1);
		id = param[0] & 0xF;
		parm1 = param[1];
		parm2 = param[2];
		parm3 = param[3];
		id = id & 0xF; // bit0-3 pwm number,bit4 invert
		if( parm_cnt == 3 ){
			lcd_blt_id = id;
			lcd_blt_freq = parm1;
			lcd_blt_level = parm2;
			lcd_blt_set_freq(id, lcd_blt_freq);
			lcd_blt_set_level(id, lcd_blt_level);
			MSG("blt %d,freq %d,level %d\n",lcd_blt_id,lcd_blt_freq,lcd_blt_level);
		}
		else {
#ifndef CFG_LOADER
			lcd_blt_set_pwm(id,parm1,parm2,parm3);
#endif
			MSG("blt %d,scalar %d,period %d,duty %d\n",lcd_blt_id,parm1,parm2,parm3);
		}
	}
#endif

	/* [uboot parameter] oem timing : pixclk:option:hsync:hbp:hpixel:hfp:vsync:vbp:vpixel:vfp */
	if( wmt_getsyspara("wmt.display.tmr",buf,&varlen) == 0 ){
		unsigned int param[10];
		vpp_timing_t *tp;
		
		tp = &vo_oem_tmr;
		DBG_MSG("tmr %s\n",buf);
		vpp_parse_param(buf,param,10,0);
		tp->pixel_clock = param[0];
		tp->option = param[1];
		tp->hsync = param[2];
		tp->hbp = param[3];
		tp->hpixel = param[4];
		tp->hfp = param[5];
		tp->vsync = param[6];
		tp->vbp = param[7];
		tp->vpixel = param[8];
		tp->vfp = param[9];
		tp->pixel_clock *= 1000;
		DBG_MSG("tmr pixclk %d,option 0x%x,hsync %d,hbp %d,hpixel %d,hfp %d,vsync %d,vbp %d,vpixel %d,vfp %d\n",
					tp->pixel_clock,tp->option,tp->hsync,tp->hbp,tp->hpixel,tp->hfp,tp->vsync,tp->vbp,tp->vpixel,tp->vfp);
		vout_info_set_fixed_timing(0,&vo_oem_tmr);
	}
	DBG_DETAIL("Leave\n");
}

void vout_check_monitor_resolution(vout_init_parm_t *init_parm)
{
	vout_t *vo;
	int i;

	DBG_DETAIL("Check monitor support\n");
	for(i=0;i<VPP_VOUT_INFO_NUM;i++){
		vout_info_t *p;
		int support;
		int vo_num;

		p = &vout_info[i];

		DBG_DETAIL("info %d (%d,%d,%d)\n",i,p->resx,p->resy,p->fps);
		
		if( p->vo_mask == 0 )
			break;
		
		if( p->fixed_timing ){
			continue;
		}

		if( init_parm->ub_resx ) p->resx = init_parm->ub_resx;
		if( init_parm->ub_resy ) p->resy = init_parm->ub_resy;
		if( !p->resx ) p->resx = init_parm->def_resx;
		if( !p->resy ) p->resy = init_parm->def_resy;
		if( !p->fps ) p->fps = init_parm->def_fps;

		DBG_DETAIL("info %d (%d,%d,%d)\n",i,p->resx,p->resy,p->fps);

		support = 0;
		for(vo_num=0;vo_num<VPP_VOUT_NUM;vo_num++){
			if( (p->vo_mask & (0x1 << vo_num)) == 0 )
				continue;
			
			if( (vo = vout_get_entry(vo_num)) == 0 )
				continue;
			
#ifdef CONFIG_WMT_EDID
			if( vout_chkplug(vo_num) ){
				int edid_option;
				
				vout_change_status(vo,VPP_VOUT_STS_BLANK,0);
				if( (edid_option = vout_get_edid_option(vo_num)) ){
					vpp_timing_t *timing;

					if( edid_find_support(&vo->edid_info,p->resx,p->resy,p->fps,&timing) ){
						support = 1;
						break;
					}
					else {
						if( vout_find_edid_support_mode(&vo->edid_info,(unsigned int *)&p->resx,
														(unsigned int *)&p->resy,(unsigned int *)&p->fps,
														(edid_option & EDID_OPT_16_9)? 1:0) ){
							support = 1;
							break;
						}
					}
				}
			}
#else
			break;
#endif
		}

		if( support == 0 ){
			p->resx = init_parm->def_resx;
			p->resy = init_parm->def_resy;
			p->fps = init_parm->def_fps;
			DBG_MSG("use default(%dx%d@%d)\n",p->resx,p->resy,p->fps);
		}
		DBG_MSG("fb%d(%dx%d@%d)\n",i,p->resx,p->resy,p->fps);
	}
	DBG_DETAIL("Leave\n");
}

int vout_init(void)
{
	vout_t *vo;
	int flag;
	int i;
	vout_init_parm_t init_parm;

	DBG_DETAIL("Enter\n");

	vout_init_param(&init_parm);

	// check vout info
	flag = vout_check_display_info(&init_parm);

	// probe external device
	vout_check_ext_device();
	
	// check plug & EDID for resolution
	if( flag ){
		vout_check_monitor_resolution(&init_parm);
	}

	DBG_DETAIL("Set mode\n");
	for(i=0;i<VPP_VOUT_NUM;i++){
		if( (vo = vout_get_entry(i)) ){
			if( vo->inf ){
				vout_inf_mode_t mode;

				mode = vo->inf->mode;
				vo->inf = 0; // force interface init
				vout_set_mode(i,mode);
				DBG_DETAIL("vout %d : inf %s, ext dev %s,status 0x%x\n",i,(vo->inf)? vout_inf_str[vo->inf->mode]:"No"
					,(vo->dev)? vo->dev->name:"No",vo->status);
			}
		}
	}

#ifndef CONFIG_UBOOT
	if( (init_parm.virtual_display) || (g_vpp.dual_display == 0) ){
		int plugin;
		
		plugin = vout_chkplug(VPP_VOUT_NUM_HDMI);
		vout_change_status(vout_get_entry(VPP_VOUT_NUM_HDMI),VPP_VOUT_STS_BLANK,(plugin)?0:1);
		vout_change_status(vout_get_entry(VPP_VOUT_NUM_DVI),VPP_VOUT_STS_BLANK,(plugin)?1:0);
	}

	for(i=0;i<VPP_VOUT_NUM;i++){
		if( (vo = vout_get_entry(i)) ){
			vout_set_blank((0x1 << i),(vo->status & VPP_VOUT_STS_BLANK)?1:0);
		}
	}
#endif

	// show vout info
	for(i=0;i<VPP_VOUT_INFO_NUM;i++){
		vout_info_t *info;

		info = &vout_info[i];
		if( info->vo_mask == 0 )
			break;
		if( i == 0 ){
			if( info->govr ){
				g_vpp.govr = info->govr;
#ifdef WMT_FTBLK_GOVRH_CURSOR
				p_cursor->mmio = info->govr->mmio;
#endif
			}
		}
		MSG("[VOUT] info %d,mask 0x%x,%s,%dx%d@%d\n",i,info->vo_mask,(info->govr)? vpp_mod_str[info->govr_mod]:"VIR",info->resx,info->resy,info->fps);
	}
	DBG_DETAIL("Leave\n");
	return 0;
}

int vout_exit(void)
{
	return 0;
}
