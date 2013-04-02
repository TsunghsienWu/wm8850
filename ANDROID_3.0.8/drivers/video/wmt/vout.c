/*++ 
 * linux/drivers/video/wmt/vout.c
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

#define VOUT_C
#undef DEBUG
//#define DEBUG
//#define DEBUG_DETAIL
/*----------------------- DEPENDENCE -----------------------------------------*/
#include "vout.h"

/*----------------------- PRIVATE MACRO --------------------------------------*/
/* #define  VO_XXXX  xxxx    *//*Example*/

/*----------------------- PRIVATE CONSTANTS ----------------------------------*/
/* #define VO_XXXX    1     *//*Example*/

/*----------------------- PRIVATE TYPE  --------------------------------------*/
/* typedef  xxxx vo_xxx_t; *//*Example*/

/*----------EXPORTED PRIVATE VARIABLES are defined in vout.h  -------------*/
/*----------------------- INTERNAL PRIVATE VARIABLES - -----------------------*/
/* int  vo_xxx;        *//*Example*/
vout_t *vout_array[VPP_VOUT_NUM];
vout_inf_t *vout_inf_array[VOUT_INF_MODE_MAX];
vout_dev_t *vout_dev_list;

/*--------------------- INTERNAL PRIVATE FUNCTIONS ---------------------------*/
/* void vo_xxx(void); *//*Example*/

/*----------------------- Function Body --------------------------------------*/
/*----------------------- vout API --------------------------------------*/
void vout_print_entry(vout_t *vo)
{
	MSG(" ===== vout entry ===== \n");
	MSG("0x%x\n",(int)vo);
	if( vo == 0 ) return;
 
	MSG("num %d,fix 0x%x,var 0x%x\n",vo->num,vo->fix_cap,vo->var_cap);
	MSG("inf 0x%x,dev 0x%x,ext_dev 0x%x\n",(int)vo->inf,(int)vo->dev,(int)vo->ext_dev);
	MSG("resx %d,resy %d,pixclk %d\n",vo->resx,vo->resy,vo->pixclk);
	MSG("status 0x%x,option %d,%d,%d\n",vo->status,vo->option[0],vo->option[1],vo->option[2]);

	if( vo->inf ){
		MSG(" ===== inf entry ===== \n");
		MSG("mode %d, %s\n",vo->inf->mode,vout_inf_str[vo->inf->mode]);
 	}

	if( vo->dev ){
		MSG(" ===== dev entry ===== \n");
		MSG("name %s,inf %d,%s\n",vo->dev->name,vo->dev->mode,vout_inf_str[vo->dev->mode]);
		MSG("vout 0x%x,capability 0x%x\n",(int)vo->dev->vout,vo->dev->capability);
 	}
}

void vout_register(int no,vout_t *vo)
{
	if( no >= VPP_VOUT_NUM )
		return;

	vo->num = no;
	vo->govr = (void *) p_govrh;
	vo->status = VPP_VOUT_STS_REGISTER + VPP_VOUT_STS_BLANK;
	vout_array[no] = vo;
}

vout_t *vout_get_entry(int no)
{
	if( no >= VPP_VOUT_NUM ){
//		DBG_ERR("%d\n",no);
		return 0;
	}

	return vout_array[no];
}

vout_info_t *vout_get_info_entry(int no)
{
	int i;
	
	if( no >= VPP_VOUT_NUM ){
		return 0;
	}
	
	for(i=0;i<VPP_VOUT_NUM;i++){
		if( vout_info[i].vo_mask & (0x1 << no) ){
			return &vout_info[i];
		}
	}
	return 0;
}

void vout_change_status(vout_t *vo,int mask,int sts)
{
	DBG_DETAIL("(0x%x,%d)\n",mask,sts);
	if( sts ){
		vo->status |= mask;
	}
	else {
		vo->status &= ~mask;
	}
	
	switch( mask ){
		case VPP_VOUT_STS_PLUGIN:
			if( sts == 0 ){
				vo->status &= ~(VPP_VOUT_STS_EDID + VPP_VOUT_STS_CONTENT_PROTECT);
				vo->edid_info.option = 0;
			}
			break;
		default:
			break;
	}
}

int vout_query_inf_support(int no,vout_inf_mode_t mode)
{
	vout_t *vo;

	if( no >= VPP_VOUT_NUM )
		return 0;

	if( mode >= VOUT_INF_MODE_MAX ){
		return 0;
	}

	vo = vout_get_entry(no);
	return (vo->fix_cap & BIT(mode))? 1:0;
}

/*----------------------- vout interface API --------------------------------------*/
int vout_inf_register(vout_inf_mode_t mode,vout_inf_t *inf)
{
	if( mode >= VOUT_INF_MODE_MAX ){
		DBG_ERR("vout interface mode invalid %d\n",mode);
		return -1;
	}

	if( vout_inf_array[mode] ){
		DBG_ERR("vout interface register again %d\n",mode);
	}
	
	vout_inf_array[mode] = inf;
	return 0;
} /* End of vout_register */

vout_inf_t *vout_inf_get_entry(vout_inf_mode_t mode)
{
	if( mode >= VOUT_INF_MODE_MAX ){
		DBG_ERR("vout interface mode invalid %d\n",mode);
		return 0;
	}
	return vout_inf_array[mode];
}

/*----------------------- vout device API --------------------------------------*/
int vout_device_register(vout_dev_t *ops)
{
	vout_dev_t *list;

	if( vout_dev_list == 0 ){
		vout_dev_list = ops;
		list = ops;
	}
	else {
		list = vout_dev_list;
		while( list->next != 0 ){
			list = list->next;
		}
		list->next = ops;
	}
	ops->next = 0;
	return 0;
}

vout_dev_t *vout_get_device(vout_dev_t *ops)
{
	if( ops == 0 ){
		return vout_dev_list;
	}
	return ops->next;
}

vout_t *vout_get_entry_adapter(vout_mode_t mode)
{
	int no;

	switch(mode){
		case VOUT_SD_DIGITAL:
		case VOUT_DVI:
		case VOUT_LCD:
		case VOUT_DVO2HDMI:
		case VOUT_SD_ANALOG:
		case VOUT_VGA:
			no = VPP_VOUT_NUM_DVI;
			break;
		case VOUT_HDMI:
		case VOUT_LVDS:
			no = VPP_VOUT_NUM_HDMI;
			break;
		default:
			no = VPP_VOUT_NUM;
			break;
	}
	return vout_get_entry(no);
}

vout_inf_t *vout_get_inf_entry_adapter(vout_mode_t mode)
{
	int no;

	switch(mode){
		case VOUT_SD_DIGITAL:
		case VOUT_SD_ANALOG:
		case VOUT_DVI:
		case VOUT_LCD:
		case VOUT_DVO2HDMI:
		case VOUT_VGA:			
			no = VOUT_INF_DVI;
			break;
		case VOUT_HDMI:
			no = VOUT_INF_HDMI;
			break;
		case VOUT_LVDS:
			no = VOUT_INF_LVDS;
			break;
		default:
			no = VOUT_INF_MODE_MAX;
			return 0;
	}
	return vout_inf_get_entry(no);
}

int vout_info_add_entry(vout_t *vo)
{
	vout_info_t *p;
	int i;

	if( vo->inf == 0 )
		return 0;

#ifdef CONFIG_HW_GOVR2_DVO
	switch( vo->inf->mode ){
		case VOUT_INF_DVI:
			vo->govr = p_govrh2;
			break;
		default:
			vo->govr = p_govrh;
			break;
	}
#else
	vo->govr = ( (vo->inf->mode == VOUT_INF_DVI) && g_vpp.dual_display )? p_govrh2:p_govrh;
#endif
	for(i=0;i<VPP_VOUT_INFO_NUM;i++){
		p = &vout_info[i];
		p->num = i;
		if( p->vo_mask == 0 ){
			break;
		}
		
		if( p->vo_mask & (0x1 << vo->num) ){
			return i;
		}

		if( vo->govr ){
			if( p->govr_mod == ((vpp_mod_base_t *)(vo->govr))->mod ){
				goto add_entry_exist;
			}
		}
	}

	if( i >= VPP_VOUT_INFO_NUM ){
		DBG_ERR("full\n");
		goto add_entry_exist;
		return VPP_VOUT_INFO_NUM;
	}
	if( (g_vpp.dual_display == 0) && (i!=0) ){
		p->resx = vout_info[0].resx;
		p->resy = vout_info[0].resy;
	}
	else {
		p->resx = vo->resx;
		p->resy = vo->resy;
	}
	p->resx_virtual = vpp_calc_align(p->resx,4);
	p->resy_virtual = p->resy;
	p->fps = (int) vo->pixclk;
	p->govr_mod = (vo->govr)? ((vpp_mod_base_t *)vo->govr)->mod:VPP_MOD_MAX;
	p->govr = vo->govr;
add_entry_exist:
	p->vo_mask |= (0x1 << vo->num);	
	DBG_MSG("info %d,vo mask 0x%x,%s,%dx%d@%d\n",i,p->vo_mask,vpp_mod_str[p->govr_mod],p->resx,p->resy,p->fps);
	return i;
}

vout_info_t *vout_info_get_entry(int no)
{
#ifdef CONFIG_HW_GOVR2_DVO
	if( g_vpp.dual_display == 0 ){
		return &vout_info[0];
	}
#endif

	if( (no >= VPP_VOUT_INFO_NUM) || (vout_info[no].vo_mask == 0) ){
		no = 0;
	}
	return &vout_info[no];
}

void vout_info_set_fixed_timing(int no,vpp_timing_t *timing)
{
	vout_info_t *info;

	DBG_MSG("(%d)\n",no);

	info = vout_info_get_entry(no);
	info->fixed_timing = timing;
}

int vout_check_ratio_16_9(unsigned int resx,unsigned int resy)
{
	int val;

	val = ( resx * 10 ) / resy;
	return (val >= 15)? 1:0;
}

vpp_timing_t *vout_find_video_mode(int no,vout_info_t *info)
{
	vout_t *vo;
	char *edid = 0;
	unsigned int vo_option;
	vpp_timing_t *p_timing = 0;
	unsigned int opt = 0;
	int index = 0;
	vpp_timing_t *edid_timing = 0;

	if( (vo = vout_get_entry(no)) == 0 )
		return 0;

//	if( vo->num != VPP_VOUT_NUM_HDMI )	// wm3445 simulate dual display
	{
	if( info->fixed_timing ){
		p_timing = info->fixed_timing;
		DBG_MSG("%d(fixed timer)\n",no);
		goto find_video_mode;
	}
	}

	if( vo->status & VPP_VOUT_STS_PLUGIN ){
		if( (edid = vout_get_edid(no)) ){
			if( edid_parse(edid,&vo->edid_info) == 0 ){
				edid = 0;
			}
		}
	}

//--> added by howayhuo for VGA virtual_display
	if(g_vpp.virtual_display && (edid == 0))
	{
		if(cs8556_tvformat_is_set())
		{
			p_timing = cs8556_get_timing2();
			goto find_video_mode;
		}
	}
//<-- end add

	vo_option = vo->option[1];
	DBG_MSG("(%d,%dx%d@%d),%s\n",no,info->resx,info->resy,info->pixclk,(vo_option & VOUT_OPT_INTERLACE)?"i":"p");
	do {
		p_timing = vpp_get_video_mode(info->resx,info->resy,info->pixclk,&index);
		DBG_MSG("find(%dx%d@%d) -> %dx%d\n",info->resx,info->resy,info->pixclk,p_timing->hpixel,p_timing->vpixel);
		info->resx = p_timing->hpixel;
		info->resy = p_timing->vpixel;
		if( p_timing->option & VPP_OPT_INTERLACE ){
			info->resy *= 2;
		}
		info->fps = VPP_GET_OPT_FPS(p_timing->option);
		opt = info->fps | ((vo_option & VOUT_OPT_INTERLACE)? EDID_TMR_INTERLACE:0);
		if( edid == 0 )
			break;

		DBG_MSG("find edid %dx%d@%d%s\n",info->resx,info->resy,info->fps,(opt & EDID_TMR_INTERLACE)?"i":"p");
		if( edid_find_support(&vo->edid_info,info->resx,info->resy,opt,&edid_timing) ){
			break;
		}

		opt = info->fps | ((vo_option & VOUT_OPT_INTERLACE)? 0:EDID_TMR_INTERLACE);
		DBG_MSG("find edid %dx%d@%d%s\n",info->resx,info->resy,info->fps,(opt & EDID_TMR_INTERLACE)?"i":"p");
		if( edid_find_support(&vo->edid_info,info->resx,info->resy,opt,&edid_timing) ){
			break;
		}

		if( (info->resx <= vpp_video_mode_table[0].hpixel) || (index <=1) ){
			info->resx = vpp_video_mode_table[0].hpixel;
			info->resy = vpp_video_mode_table[0].vpixel;
			break;
		}
		
		do {
			index--;
			p_timing = (vpp_timing_t *) &vpp_video_mode_table[index];
			if( info->resx == p_timing->hpixel ){
				int vpixel;

				vpixel = p_timing->vpixel;
				if( p_timing->option & VPP_OPT_INTERLACE ){
					vpixel *= 2;
					index--;
				}
				if( info->resy == vpixel ){
					continue;
				}
			}
			break;
		} while(1);
		info->resx = vpp_video_mode_table[index].hpixel;
		info->resy = vpp_video_mode_table[index].vpixel;
		if(vpp_video_mode_table[index].option & VPP_OPT_INTERLACE){
			info->resy *= 2;
		}
	} while(1);

	if( edid_timing ){
		p_timing = edid_timing;
		DBG_MSG("Use EDID detail timing\n");
	}
	else {
		if( opt & EDID_TMR_INTERLACE )
			vo_option |= VOUT_OPT_INTERLACE;
		else 
			vo_option &= ~VOUT_OPT_INTERLACE;
		
		p_timing = vpp_get_video_mode_ext(info->resx,info->resy,info->fps,vo_option);
	}
find_video_mode:
	{
		int hpixel,vpixel;
		
		info->resx = p_timing->hpixel;
		info->resy = p_timing->vpixel;
		hpixel = p_timing->hpixel + p_timing->hsync + p_timing->hfp + p_timing->hbp;
		vpixel = p_timing->vpixel + p_timing->vsync + p_timing->vfp + p_timing->vbp;
		if( p_timing->option & VPP_OPT_INTERLACE ) {
			info->resy *= 2;
			vpixel *= 2;
		}
		info->pixclk = p_timing->pixel_clock;
		info->fps = info->pixclk / (hpixel * vpixel);
		info->resx_virtual = vpp_calc_fb_width(g_vpp.mb_colfmt,info->resx);
		info->resy_virtual = info->resy;
	}
	DBG_MSG("Leave (%dx%d@%d)\n",p_timing->hpixel,p_timing->vpixel,p_timing->pixel_clock);
	return p_timing;
}

int vout_find_edid_support_mode
(
	edid_info_t *info,
	unsigned int *resx,
	unsigned int *resy,
	unsigned int *fps,
	int r_16_9
)
{
	/* check the EDID to find one that not large and same ratio mode*/
#ifdef CONFIG_WMT_EDID
	int i,cnt;
	vpp_timing_t *p;
	unsigned int w,h,f,option;

	for(i=0,cnt=0;;i++){
		if( vpp_video_mode_table[i].pixel_clock == 0 )
			break;
		cnt++;
	}

	for(i=cnt-1;i>=0;i--){
		p = (vpp_timing_t *) &vpp_video_mode_table[i];
		h = p->vpixel;
		if(p->option & VPP_OPT_INTERLACE){
			h = h*2;
			i--;
		}
		if( h > *resy ) continue;
		
		w = p->hpixel;
		if( w > *resx ) continue;
		
		f = vpp_get_video_mode_fps(p);
		if( f > *fps ) continue;

		if( r_16_9 != vout_check_ratio_16_9(w,h) ){
			continue;
		}

		option = f & EDID_TMR_FREQ;
		option |= (p->option & VPP_OPT_INTERLACE)? EDID_TMR_INTERLACE:0;

		if( edid_find_support(info,w,h,option,&p) ){
			*resx = w;
			*resy = h;
			*fps = f;
			DPRINT("[VOUT] %s(%dx%d@%d)\n",__FUNCTION__,w,h,f);
			return 1;
		}
	}
#endif
	return 0;
}

/*----------------------- vout control API --------------------------------------*/
void vout_set_framebuffer(unsigned int mask,vdo_framebuf_t *fb)
{
	int i;
	vout_t *vo;

	if( mask == 0 )
		return;

	for(i=0;i<VPP_VOUT_NUM;i++){
		if( (mask & (0x1 << i)) == 0 )
			continue;
		if( (vo = vout_get_entry(i)) ){
			if( vo->govr ){
				vo->govr->fb_p->set_framebuf(fb);
			}
		}
	}
}

void vout_set_tg_enable(unsigned int mask,int enable)
{
	int i;
	vout_t *vo;

	if( mask == 0 )
		return;

	for(i=0;i<VPP_VOUT_NUM;i++){
		if( (mask & (0x1 << i)) == 0 )
			continue;
		if( (vo = vout_get_entry(i)) ){
			if( vo->govr ){
				govrh_set_tg_enable(vo->govr,enable);
			}
		}
	}
}

int vout_set_blank(unsigned int mask,vout_blank_t arg)
{
	int i;
	vout_t *vo;

	DBG_DETAIL("(0x%x,%d)\n",mask,arg);

	if( mask == 0 )
		return 0;

	for(i=0;i<VPP_VOUT_NUM;i++){
		if( (mask & (0x1 << i)) == 0 )
			continue;

		if( (vo = vout_get_entry(i)) ){
			if( vo->inf ){
				vout_change_status(vo,VPP_VOUT_STS_BLANK,arg);
				vo->inf->blank(vo,arg);
				if( vo->dev && vo->dev->set_power_down ){
					vo->dev->set_power_down((arg==VOUT_BLANK_POWERDOWN)?1:0);
				}
			}
		}
	}

	if(1){
		int govr_enable_mask,govr_mask;
		govrh_mod_t *govr;
		
		govr_enable_mask = 0;
		for(i=0;i<VPP_VOUT_NUM;i++){
			if( (vo = vout_get_entry(i)) ){
				if( vo->govr ){
					govr_mask = (vo->govr->mod == VPP_MOD_GOVRH)? 0x1:0x2;
					govr_enable_mask |= (vo->status & VPP_VOUT_STS_BLANK)? 0:govr_mask;
				}
			}
		}
		for(i=0;i<VPP_VOUT_INFO_NUM;i++){
			govr = (i==0)? p_govrh:p_govrh2;
			govr_mask = 0x1 << i;
			govrh_set_MIF_enable(govr,(govr_enable_mask & govr_mask)? 1:0);
		}
	}
	return 0;
}

int vout_set_mode(int no,vout_inf_mode_t mode)
{
	vout_t *vo;

	DBG_DETAIL("(%d,%d)\n",no,mode);

	if( vout_query_inf_support(no,mode)==0 ){
		DBG_ERR("not support this interface(%d,%d)\n",no,mode);
		return -1;
	}

	vo = vout_get_entry(no);
	if( vo->inf ){
		if( vo->inf->mode == mode )
			return 0;
		vo->inf->uninit(vo,0);
		vout_change_status(vo,VPP_VOUT_STS_ACTIVE,0);
		if( vo->dev )
			vo->dev->set_power_down(1);
	}
	
	vo->inf = vout_inf_get_entry(mode);
	vo->dev = ( vo->ext_dev && (vo->ext_dev->mode == mode) )? vo->ext_dev:0;
	vo->inf->init(vo,0);
	vout_change_status(vo,VPP_VOUT_STS_ACTIVE,1);
	return 0;
}

int vout_check_info(unsigned int mask,vout_info_t *info)
{
	vpp_timing_t *p_timing;
	int no = VPP_VOUT_NUM;
	int i;

	DBG_DETAIL("(0x%x)\n",mask);

	for(i=0;i<VPP_VOUT_NUM;i++){
		if( (mask & (0x1 << i)) == 0 )
			continue;

		if( no == VPP_VOUT_NUM ){
			no = i;		// get first vo
		}
		
		if( vout_chkplug(i) ){
			no = i;
			break;
		}
	}

	if( (p_timing = vout_find_video_mode(no,info)) == 0 ){
		DBG_ERR("no support mode\n");
		return -1;
	}
	return 0;
}

int vout_config(unsigned int mask,vout_info_t *info)
{
	vout_t *vo;
	vpp_timing_t *p_timing;
	int no = VPP_VOUT_NUM;
	int i;
	unsigned int govr_mask = 0;

	DBG_DETAIL("(0x%x)\n",mask);

	if( mask == 0 )
		return 0;

	for(i=0;i<VPP_VOUT_NUM;i++){
		if( (mask & (0x1 << i)) == 0 )
			continue;

		if( no == VPP_VOUT_NUM ){
			no = i;		// get first vo
		}
		
		if( vout_chkplug(i) ){
			no = i;
			break;
		}
	}

	if( (p_timing = vout_find_video_mode(no,info)) == 0 ){
		return -1;
	}
	
	info->option = p_timing->option;
	for(i=0;i<VPP_VOUT_NUM;i++){
		if( (mask & (0x1 << i)) == 0 )
			continue;

		if( (vo = vout_get_entry(i)) == 0 )
			continue;

		if( vo->govr == 0 )
			continue;

		if( (govr_mask & (0x1 << vo->govr->mod)) == 0 ){
			govrh_set_timing(vo->govr,p_timing);
			govr_mask |= (0x1 << vo->govr->mod);
		}

		if( vo->inf ){
			vo->inf->config(vo,(int)info);
			if( vo->dev )
				vo->dev->config(info);
		}
	}
	return 0;
}

int vout_chkplug(int no)
{
	vout_t *vo;
	vout_inf_t *inf;
	int ret = 0;

	DBG_DETAIL("(%d)\n",no);

	if( (vo = vout_get_entry(no)) == 0 ){
		return 0;
	}

	if( vo->inf == 0 ){
		return 0;
	}

	if( (inf = vout_inf_get_entry(vo->inf->mode)) == 0 ){
		return 0;
	}

	if( vo->dev && vo->dev->check_plugin ){
		ret = vo->dev->check_plugin(0);
	}
	else {
		ret = inf->chkplug(vo,0);
	}
	vout_change_status(vo,VPP_VOUT_STS_PLUGIN,ret);
	return ret; 
}

int vout_inf_chkplug(int no,vout_inf_mode_t mode)
{
	vout_t *vo;
	vout_inf_t *inf;
	int plugin = 0;

	DBG_MSG("(%d,%d)\n",no,mode);
	if( vout_query_inf_support(no,mode) == 0 )
		return 0;

	vo = vout_get_entry(no);
	if( (inf = vout_inf_get_entry(mode)) ){
		if( inf->chkplug ){
			plugin = inf->chkplug(vo,0);
		}
	}
	return plugin;
}

char *vout_get_edid(int no)
{
	vout_t *vo;
	int ret;

	DBG_DETAIL("(%d)\n",no);

	if( (vo = vout_get_entry(no)) == 0 ){
		return 0;
	}

	if( vo->status & VPP_VOUT_STS_EDID ){
		DBG_MSG("edid exist\n");
		return vo->edid;
	}

	vout_change_status(vo,VPP_VOUT_STS_EDID,0);
#ifdef CONFIG_VOUT_EDID_ALLOC
	if( vo->edid == 0 ){
//		DBG_MSG("edid buf alloc\n");
		vo->edid = (char *) kmalloc(128*EDID_BLOCK_MAX,GFP_KERNEL);
		if( !vo->edid ){
			DBG_ERR("edid buf alloc\n");
			return 0;
		}
	}
#endif

	ret = 1;	
	if( vo->dev && vo->dev->get_edid ){
		ret = vo->dev->get_edid(vo->edid);
	}
	else {
		if( vo->inf->get_edid ){
			ret = vo->inf->get_edid(vo,(int)vo->edid);
		}
	}
	
	if( ret == 0 ){
		vout_info_t *info;
		
		DBG_DETAIL("edid read\n");
		vout_change_status(vo,VPP_VOUT_STS_EDID,1);
		if( (info = vout_get_info_entry((g_vpp.virtual_display)?1:no)) ){
			info->force_config = 1;
		}
		return vo->edid;
	}
	else {
		DBG_ERR("read edid fail\n");
	}

#ifdef CONFIG_VOUT_EDID_ALLOC
	if( vo->edid ){
		kfree(vo->edid);
		vo->edid = 0;
	}
#endif
	return 0;
}

int vout_get_edid_option(int no)
{
	vout_t *vo;

	DBG_DETAIL("(%d)\n",no);

	if( (vo = vout_get_entry(no)) == 0 ){
		return 0;
	}

	if( vo->edid_info.option ){
		return vo->edid_info.option;
	}

	if( vout_get_edid(no) == 0 ){
		return 0;
	}

	edid_parse(vo->edid,&vo->edid_info);
	return vo->edid_info.option;
}

unsigned int vout_get_mask(	vout_info_t *vo_info)
{
	if( g_vpp.dual_display == 0 )
		return VPP_VOUT_ALL;

	if( g_vpp.virtual_display ){
		if( vo_info->num == 0 ){
			return 0;
		}
		return VPP_VOUT_ALL;
	}
	return vo_info->vo_mask;
}

/*--------------------End of Function Body -----------------------------------*/

#undef VOUT_C

