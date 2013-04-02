/*++ 
 * linux/drivers/video/wmt/vpp.c
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

#define VPP_C
#undef DEBUG
//#define DEBUG
//#define DEBUG_DETAIL

#include "vpp.h"

vpp_mod_base_t *vpp_mod_base_list[VPP_MOD_MAX];

unsigned int vpp_get_chipid(void)
{
	// byte 3,2: chip id, byte 1:ver id, byte 0:sub id
	// ex: 0x34290101 (0x3429 A0), 0x34290102 (0x3429 A1)
	return REG32_VAL(SYSTEM_CFG_CTRL_BASE_ADDR);
}

__inline__ void vpp_cache_sync(void)
{
#if 0
  /*
  * MRC{cond} p<cpnum>, <op1>, Rd, CRn, CRm, <op2>
  */
  __asm__ __volatile__ (
    "mov     r0, #0 \n\t"
    "mcr     p15, 0, r0, c7, c14, 0 \n\t"
    :
    :
    : "r0"
  );
#endif
}

void vpp_set_dbg_gpio(int no,int value)
{
	unsigned int mask;

//	return;
	if( no < 20 ){	// dedicate GPIO
		mask = 0x1 << no;
		REG32_VAL(GPIO_BASE_ADDR+0x40) |= mask;
		REG32_VAL(GPIO_BASE_ADDR+0x80) |= mask;
		if( value == 0xFF ){
			value = ( REG32_VAL(GPIO_BASE_ADDR+0xC0) & mask )? 0:1;
		}
		
		if( value ){
			REG32_VAL(GPIO_BASE_ADDR+0xC0) |= mask;
		}
		else {
			REG32_VAL(GPIO_BASE_ADDR+0xC0) &= ~mask;
		}
	}
	else if( no < 30 ){ // suspend GPIO
		no -= 20;
		mask = 0x20 << no;
		REG8_VAL(GPIO_BASE_ADDR+0x42) |= mask;
		REG8_VAL(GPIO_BASE_ADDR+0x82) |= mask;
		if( value == 0xFF ){
			value = ( REG8_VAL(GPIO_BASE_ADDR+0xC2) & mask )? 0:1;
		}
		
		if( value ){
			REG8_VAL(GPIO_BASE_ADDR+0xC2) |= mask;
		}
		else {
			REG8_VAL(GPIO_BASE_ADDR+0xC2) &= ~mask;
		}
	}
}

int vpp_check_dbg_level(vpp_dbg_level_t level)
{
	if( level == VPP_DBGLVL_ALL )
		return 1;

	switch( g_vpp.dbg_msg_level ){
		case VPP_DBGLVL_DISABLE:
			break;
		case VPP_DBGLVL_ALL:
			return 1;
		default:
			if( g_vpp.dbg_msg_level == level ){
				return 1;
			}
			break;
	}
	return 0;
}

void vpp_mod_unregister(vpp_mod_t mod)
{
	vpp_mod_base_t *mod_p;

	if( mod >= VPP_MOD_MAX ){
		return;
	}

	if( !(mod_p = vpp_mod_base_list[mod]) )
		return;
	
	if( mod_p->fb_p ) kfree(mod_p->fb_p);
	kfree(mod_p);
	vpp_mod_base_list[mod] = 0;
}

vpp_mod_base_t *vpp_mod_register(vpp_mod_t mod,int size,unsigned int flags)
{
	vpp_mod_base_t *mod_p;

	if( mod >= VPP_MOD_MAX ){
		return 0;
	}

	if( vpp_mod_base_list[mod] ){
		vpp_mod_unregister(mod);
	}

	mod_p = (void *) kmalloc(size,GFP_KERNEL);
	if( !mod_p ) return 0;

	vpp_mod_base_list[mod] = mod_p;
	memset(mod_p,0,size);
	mod_p->mod = mod;

	if( flags & VPP_MOD_FLAG_FRAMEBUF ){
		if( !(mod_p->fb_p = (void *) kmalloc(sizeof(vpp_fb_base_t),GFP_KERNEL)) ){
			goto error;
		}
		memset(mod_p->fb_p,0,sizeof(vpp_fb_base_t));
	}
//	DPRINT("vpp mod register %d,0x%x,0x%x\n",mod,(int)mod_p,(int)mod_p->fb_p);
	return mod_p;
	
error:
	vpp_mod_unregister(mod);
	DPRINT("vpp mod register NG %d\n",mod);	
	return 0;
}

vpp_mod_base_t *vpp_mod_get_base(vpp_mod_t mod)
{
	if( mod >= VPP_MOD_MAX )
		return 0;

	return vpp_mod_base_list[mod];
}

vpp_fb_base_t *vpp_mod_get_fb_base(vpp_mod_t mod)
{
	vpp_mod_base_t *mod_p;
	mod_p = vpp_mod_get_base(mod);
	if( mod_p )
		return mod_p->fb_p;

	return 0;
}

vdo_framebuf_t *vpp_mod_get_framebuf(vpp_mod_t mod)
{
	vpp_mod_base_t *mod_p;

	mod_p = vpp_mod_get_base(mod);
	if( mod_p && mod_p->fb_p )
		return &mod_p->fb_p->fb;

	return 0;
}

void vpp_mod_set_timing(vpp_mod_t mod,vpp_timing_t *tmr)
{
	govrh_mod_t *govr;
#ifdef WMT_FTBLK_VPU
	vdo_framebuf_t *fb;
#endif
	vpp_clock_t clk;

	vpp_trans_timing(mod,tmr,&clk,1);
#ifdef WMT_FTBLK_GOVRH
	govr = (mod == VPP_MOD_GOVRH)? p_govrh:p_govrh2;
	govr->set_tg(&clk,tmr->pixel_clock);
	govr->fb_p->fb.img_w = tmr->hpixel;
	govr->fb_p->fb.img_h = tmr->vpixel;
	govr->fb_p->framerate = tmr->pixel_clock / (tmr->hpixel * tmr->vpixel);
	govr->fb_p->set_framebuf(&govr->fb_p->fb);
#endif

	if( g_vpp.govr->mod != mod )
		return;

#ifdef WMT_FTBLK_GOVW
	p_govw->fb_p->fb.img_w = tmr->hpixel;
	p_govw->fb_p->fb.img_h = tmr->vpixel;
	p_govw->fb_p->set_framebuf(&p_govw->fb_p->fb);
	
	if( g_vpp.vpp_path ){
		vpp_set_govw_tg(VPP_FLAG_DISABLE);
	}
#endif
#ifdef WMT_FTBLK_VPU
	fb = &p_vpu->fb_p->fb;
	if( fb->y_addr ){ 
		vdo_view_t view;
		
		p_vpu->resx_visual = (fb->fb_w > tmr->hpixel)? tmr->hpixel:fb->fb_w;
		p_vpu->resy_visual = (fb->fb_h > tmr->vpixel)? tmr->vpixel:fb->fb_h;

		view.resx_src = fb->fb_w;
		view.resy_src = fb->fb_h;
		view.resx_virtual = fb->img_w;
		view.resy_virtual = fb->img_h;
		view.resx_visual = p_vpu->resx_visual;
		view.resy_visual = p_vpu->resy_visual;
		view.posx = p_vpu->posx;
		view.posy = p_vpu->posy;
		view.offsetx = fb->h_crop;
		view.offsety = fb->v_crop;
		vpp_set_video_scale(&view);
	}
#endif
}

void vpp_mod_set_clock(vpp_mod_t mod,vpp_flag_t enable,int force)
{
	vpp_mod_base_t *base;
	enum dev_id pll_dev;	
	int cur_sts;
	int ret;

#ifdef CONFIG_VPP_DISABLE_PM
	return;
#endif

	if( (base = vpp_mod_get_base(mod)) == 0 )
		return;
	
	if( (pll_dev = (base->pm & 0xFF)) == 0 )
		return;

	enable = (enable)? VPP_FLAG_ENABLE:VPP_FLAG_DISABLE;
	if( force ){
		ret = auto_pll_divisor(pll_dev,(enable)? CLK_ENABLE:CLK_DISABLE,0,0);
		DBG_DETAIL("[VPP] clk force(%s,%d),ret %d\n",vpp_mod_str[mod],enable,ret);
		return;
	}

	cur_sts = (base->pm & VPP_MOD_CLK_ON)? VPP_FLAG_ENABLE:VPP_FLAG_DISABLE;
	if( cur_sts != enable ){
		ret = auto_pll_divisor(pll_dev,(enable)? CLK_ENABLE:CLK_DISABLE,0,0);
		base->pm = (enable)? (base->pm | VPP_MOD_CLK_ON):(base->pm & ~VPP_MOD_CLK_ON);
		DBG_MSG("[VPP] clk enable(%s,%d,cur %d),ret %d\n",vpp_mod_str[mod],enable,cur_sts,ret);
	}
}

void vpp_clr_framebuf(vpp_mod_t mod)
{
	vdo_framebuf_t fb;
	vpp_mod_base_t *p_mod;
	int ysize,csize;	

	if( (p_mod = vpp_mod_get_base(mod)) == 0 ){
		return;
	}

	if( p_mod->fb_p == 0 )
		return;

	if( p_mod->fb_p->get_framebuf ){
		p_mod->fb_p->get_framebuf(&fb);
	}
	else {
		fb = p_mod->fb_p->fb;
	}
	vpp_get_colfmt_bpp(fb.col_fmt,&ysize,&csize);
	if( ysize ){
		ysize = fb.fb_w * fb.fb_h * ysize / 8;
		memset((char *)mb_phys_to_virt(fb.y_addr),0,ysize);
	}

	if( csize ){
		csize = fb.fb_w * fb.fb_h * csize / 8;
		memset((char *)mb_phys_to_virt(fb.c_addr),0x80,csize);
	}
//	DPRINT("[VPP] clr fb Y(0x%x,%d) C(0x%x,%d),%s\n",fb.y_addr,ysize,fb.c_addr,csize,vpp_colfmt_str[fb.col_fmt]);
}

vpp_display_format_t vpp_get_fb_field(vdo_framebuf_t *fb)
{
	if( fb->flag & VDO_FLAG_INTERLACE )
		return VPP_DISP_FMT_FIELD;
	
	return VPP_DISP_FMT_FRAME;
}

unsigned int vpp_get_base_clock(vpp_mod_t mod)
{
	unsigned int clock = 0;
	
	switch(mod){
		default:
			clock = auto_pll_divisor(DEV_VPP,GET_FREQ,0,0);
			break;
		case VPP_MOD_GOVRH:
			clock = auto_pll_divisor(DEV_HDMILVDS,GET_FREQ,0,0);
			break;
		case VPP_MOD_GOVRH2:
			clock = auto_pll_divisor(DEV_DVO,GET_FREQ,0,0);
			break;
	}
//	DPRINT("[VPP] get base clock %s : %d\n",(pll==VPP_PLL_B)? "PLLB":"PLLC",clock);
	return clock;
}

unsigned int vpp_calculate_diff(unsigned int val1,unsigned int val2)
{
	return (val1 >= val2)?(val1-val2):(val2-val1);
}

#if 0
void vpp_calculate_clock(vpp_base_clock_t *clk,unsigned int div_max,unsigned int base_mask)
{
	unsigned int pixclk;
	unsigned int sum1,sum2;
	int diff,diff_min;
	int base,mul,div;
	int base_bk,mul_bk,div_bk;

	diff_min = pixclk = clk->pixel_clock;
	base_bk = mul_bk = div_bk = 1;
	if( base_mask & 0x1 ){	/* 25MHz base */
		for(div=1;div<=div_max;div++){
			base = 6250000;
			mul = pixclk * div / base;
			sum1 = base * mul / div;
			sum2 = base * (mul + 1) / div;
			sum1 = vpp_calculate_diff(sum1,pixclk);
			sum2 = vpp_calculate_diff(sum2,pixclk);
			mul = (sum1 < sum2)? mul:(mul+1);
			sum1 = base * mul / div;
			mul /= 2;
			base *= 2;
			if( mul > 62 ){
				base *= 2;
				mul /= 2;
			}
			sum1 = base * mul;
			if( (sum1 < 300000000) || (sum1 > 750000000) ) continue;
			if( (mul % 2) || (mul < 8) || (mul > 62) ) continue;
			sum1 = sum1 / div;
			diff = vpp_calculate_diff(sum1,pixclk);
			if( diff < diff_min ){
				diff_min = diff;
				base_bk = base;
				mul_bk = mul;
				div_bk = div;
			}
		}
	}

	if( base_mask & 0x2 ){	/* 27MHz base */
		for(div=1;div<=div_max;div++){
			base = 6750000;
			mul = pixclk * div / base;
			sum1 = base * mul / div;
			sum2 = base * (mul + 1) / div;
			sum1 = vpp_calculate_diff(sum1,pixclk);
			sum2 = vpp_calculate_diff(sum2,pixclk);
			mul = (sum1 < sum2)? mul:(mul+1);

			sum1 = base * mul / div;
			mul /= 2;
			base *= 2;
			if( mul > 62 ){
				base *= 2;
				mul /= 2;
			}
			sum1 = base * mul;
			if( (sum1 < 300000000) || (sum1 > 750000000) ) continue;
			if( (mul % 2) || (mul < 8) || (mul > 62) ) continue;
			sum1 = sum1 / div;
			diff = vpp_calculate_diff(sum1,pixclk);
			if( diff < diff_min ){
				diff_min = diff;
				base_bk = base;
				mul_bk = mul;
				div_bk = div;
			}
		}
	}
	
//	DBGMSG("pixclk %d, base %d, mul %d, div %d,diff %d\n",pixclk,base_bk,mul_bk,div_bk,diff_min);
	clk->pixel_clock = base_bk * mul_bk / div_bk;
	clk->divisor = div_bk;
	clk->rd_cyc = 0;

	switch( base_bk ){
		case 12500000: 
			clk->PLL = 0x20; 
			break;
		default:
		case 25000000: 
			clk->PLL = 0x120; 
			break;
		case 13500000:
			clk->PLL = 0x00;
			break;
		case 27000000:
			clk->PLL = 0x100;
			break;
	}
	clk->PLL = clk->PLL | (mul_bk / 2);
	
}
#endif

unsigned int vpp_check_value(unsigned int val,unsigned int min,unsigned int max)
{
	if( min >= max ) 
		return min;
	val = (val < min)? min:val;
	val = (val > max)? max:val;
	return val;
}

#if 1
void vpp_show_timing(vpp_timing_t *tmr,vpp_clock_t *clk)
{
	if( tmr ){
		DPRINT("pixel clock %d,option 0x%x\n",tmr->pixel_clock,tmr->option);
		DPRINT("H sync %d,bp %d,pixel %d,fp %d\n",tmr->hsync,tmr->hbp,tmr->hpixel,tmr->hfp);
		DPRINT("V sync %d,bp %d,pixel %d,fp %d\n",tmr->vsync,tmr->vbp,tmr->vpixel,tmr->vfp);
	}
	
	if( clk ){
		DPRINT("H beg %d,end %d,total %d\n",clk->begin_pixel_of_active,clk->end_pixel_of_active,clk->total_pixel_of_line);
		DPRINT("V beg %d,end %d,total %d\n",clk->begin_line_of_active,clk->end_line_of_active,clk->total_line_of_frame);
		DPRINT("Hsync %d, Vsync %d\n",clk->hsync,clk->vsync);
		DPRINT("VBIE %d,PVBI %d\n",clk->line_number_between_VBIS_VBIE,clk->line_number_between_PVBI_VBIS);
	}
}

void vpp_show_framebuf(char *str,vdo_framebuf_t *fb)
{
	if( fb == 0 ) return;
	DPRINT("----- %s framebuf -----\n",str);
	DPRINT("Y addr 0x%x, size %d\n",fb->y_addr,fb->y_size);
	DPRINT("C addr 0x%x, size %d\n",fb->c_addr,fb->c_size);
	DPRINT("W %d, H %d, FB W %d, H %d\n",fb->img_w,fb->img_h,fb->fb_w,fb->fb_h);
	DPRINT("bpp %d, color fmt %s\n",fb->bpp,vpp_colfmt_str[fb->col_fmt]);
	DPRINT("H crop %d, V crop %d, flag 0x%x\n",fb->h_crop,fb->v_crop,fb->flag);
}

void vpp_show_view(char *str,vdo_view_t *view)
{
	DPRINT("----- %s view -----\n",str);
	DPRINT("pos(%d,%d),offset(%d,%d)\n",view->posx,view->posy,view->offsetx,view->offsety);
	DPRINT("H %d,%d->%d\n",view->resx_src,view->resx_virtual,view->resx_visual);
	DPRINT("V %d,%d->%d\n",view->resy_src,view->resy_virtual,view->resy_visual);
}
#endif

void vpp_trans_timing(vpp_mod_t mod,vpp_timing_t *tmr,vpp_clock_t *hw_tmr,int to_hw)
{
	vpp_fb_base_t *mod_fb_p;
	unsigned int pixel_clock;
	int temp;
	
	mod_fb_p = vpp_mod_get_fb_base(mod);

	if( to_hw ){
		hw_tmr->begin_pixel_of_active = tmr->hsync + tmr->hbp;
		hw_tmr->end_pixel_of_active = hw_tmr->begin_pixel_of_active + tmr->hpixel;
		hw_tmr->total_pixel_of_line = hw_tmr->end_pixel_of_active + tmr->hfp;	
		hw_tmr->begin_line_of_active = tmr->vsync + tmr->vbp + 1;
		hw_tmr->end_line_of_active = hw_tmr->begin_line_of_active + tmr->vpixel;
		hw_tmr->total_line_of_frame = hw_tmr->end_line_of_active + tmr->vfp -1;	
//		if( tmr->option & VPP_OPT_INTERLACE ) hw_tmr->total_line_of_frame += 1;
		hw_tmr->line_number_between_VBIS_VBIE = tmr->vsync + 1; // hw_tmr->begin_line_of_active - 3;
		temp = hw_tmr->total_line_of_frame - hw_tmr->end_line_of_active;
		hw_tmr->line_number_between_PVBI_VBIS = (temp>2)? (temp-1):1;
		hw_tmr->hsync = tmr->hsync;
		hw_tmr->vsync = tmr->vsync;
		
		// pixel_clock = hw_tmr->total_pixel_of_line * hw_tmr->total_line_of_frame * mod_fb_p->framerate;
		pixel_clock = tmr->pixel_clock;
		hw_tmr->read_cycle = vpp_get_base_clock(mod) / pixel_clock;
	}
	else {
		pixel_clock = hw_tmr->total_pixel_of_line * hw_tmr->total_line_of_frame * mod_fb_p->framerate;
		tmr->pixel_clock = pixel_clock;
		tmr->option = 0;
		
		tmr->hsync = hw_tmr->hsync;
		tmr->hbp = hw_tmr->begin_pixel_of_active - hw_tmr->hsync;
		tmr->hpixel = hw_tmr->end_pixel_of_active - hw_tmr->begin_pixel_of_active;
		tmr->hfp = hw_tmr->total_pixel_of_line - hw_tmr->end_pixel_of_active;

		tmr->vsync = hw_tmr->vsync;
		tmr->vbp = hw_tmr->begin_line_of_active - hw_tmr->vsync -1;
		tmr->vpixel = hw_tmr->end_line_of_active - hw_tmr->begin_line_of_active;
		tmr->vfp = hw_tmr->total_line_of_frame - hw_tmr->end_line_of_active +1;
	}
}
#ifdef WMT_FTBLK_GOVW
void vpp_calculate_timing(vpp_mod_t mod,unsigned int fps,vpp_clock_t *tmr)
{
	unsigned int base_clock;
	unsigned int rcyc_max,rcyc_min;
	unsigned int h_min,h_max;
	unsigned int v_min,v_max;
	unsigned int diff_min,diff_h,diff_v,diff_rcyc;	
	unsigned int hbp_min,hfp_min,hporch_min;
	unsigned int vbp_min,vfp_min,vporch_min;	
	int i,j,k,temp;
	unsigned int hpixel,vpixel;
    int diff_clk;

	hpixel = tmr->end_pixel_of_active - tmr->begin_pixel_of_active;
	vpixel = tmr->end_line_of_active - tmr->begin_line_of_active;
	if( govrh_get_hscale_up(g_vpp.govr) )
		hpixel /= 2;
	base_clock = vpp_get_base_clock(mod) / fps;
	hbp_min = tmr->begin_pixel_of_active;
	hfp_min = tmr->total_pixel_of_line - tmr->end_pixel_of_active;
	hporch_min = hbp_min + hfp_min;
	vbp_min = tmr->begin_line_of_active;
	vfp_min = tmr->total_line_of_frame - tmr->end_line_of_active;
	vporch_min = vbp_min + vfp_min;

	rcyc_min = vpp_check_value((base_clock / (4096 * 4096)),WMT_GOVW_RCYC_MIN+1,256);
	rcyc_max = vpp_check_value((base_clock / (hpixel * vpixel)) + 1,WMT_GOVW_RCYC_MIN+1,256);

	if( g_vpp.govw_hfp && g_vpp.govw_hbp ){
		h_min = g_vpp.govw_hfp + g_vpp.govw_hbp + hpixel;
		h_max = g_vpp.govw_hfp + g_vpp.govw_hbp + hpixel;

		vbp_min = 4;
		vporch_min = 6;
		rcyc_min = rcyc_max = 3;
	}
	else {
		h_min = vpp_check_value((base_clock / (rcyc_max * 4096)),hpixel+hporch_min,4096);
		h_max = vpp_check_value((base_clock / (rcyc_min * vpixel)) + 1,hpixel+hporch_min,4096);
	}

	if( g_vpp.govw_vfp && g_vpp.govw_vbp ){
		v_min = g_vpp.govw_vfp + g_vpp.govw_vbp + vpixel;
		v_max = g_vpp.govw_vfp + g_vpp.govw_vbp + vpixel;
	}
	else {
		v_min = vpp_check_value((base_clock / (rcyc_max * 4096)),vpixel+vporch_min,4096);
		v_max = vpp_check_value((base_clock / (rcyc_min * hpixel)) + 1,vpixel+vporch_min,4096);
	}

//	DPRINT("[VPP] clk %d,rcyc(%d,%d),h(%d,%d),v(%d,%d)\n",base_clock,rcyc_min,rcyc_max,h_min,h_max,v_min,v_max);

	diff_min=0xFFFFFFFF;
	diff_rcyc = diff_h = diff_v = 0;
	for(i=rcyc_max;i>=rcyc_min;i--){
		for(j=v_max;j>=v_min;j--){
			temp = (base_clock * 100) / (i*j);
			k = temp / 100;
			k += ((temp % 100) >= 50)? 1:0;
			k = vpp_check_value(k,h_min,h_max);
			temp = i * j * k;
			diff_clk = vpp_calculate_diff(base_clock,temp);
			if(diff_min > diff_clk){
				diff_min = diff_clk;
				diff_h = k;
				diff_v = j;
				diff_rcyc = i;
//				DPRINT("[VPP] rcyc %d h %d v %d\n",diff_rcyc,diff_h,diff_v);
			}
			if( diff_clk == 0 ){
				i = rcyc_min;
				break;
			}
		}
	}

	tmr->read_cycle = diff_rcyc - 1;
	tmr->total_pixel_of_line = diff_h;
	if( g_vpp.govw_hfp && g_vpp.govw_hbp ){
		tmr->begin_pixel_of_active = g_vpp.govw_hbp;
		tmr->end_pixel_of_active = tmr->begin_pixel_of_active + hpixel;
	}
	else {
#if 1
		temp = diff_h - hpixel;
		tmr->begin_pixel_of_active = ( temp/10 );
		tmr->begin_pixel_of_active = vpp_check_value(tmr->begin_pixel_of_active,20,504);
#else
		temp = diff_h - hpixel - hbp_min;
		tmr->begin_pixel_of_active = ( hbp_min + temp/2 );
		tmr->begin_pixel_of_active = vpp_check_value(tmr->begin_pixel_of_active,hbp_min,504);		
#endif		
		tmr->end_pixel_of_active = tmr->begin_pixel_of_active + hpixel;
	}

	tmr->total_line_of_frame = diff_v;
	if( g_vpp.govw_vfp && g_vpp.govw_vbp ){
		tmr->begin_line_of_active = g_vpp.govw_vbp;
		tmr->end_line_of_active = tmr->begin_line_of_active + vpixel;
	}
	else {
		temp = diff_v - vpixel - vbp_min;
		tmr->begin_line_of_active = ( vbp_min + temp/2 );
		tmr->begin_line_of_active = vpp_check_value(tmr->begin_line_of_active,vbp_min,254); //504);
		tmr->end_line_of_active = tmr->begin_line_of_active + vpixel;
	}

	tmr->line_number_between_VBIS_VBIE = tmr->begin_line_of_active - 3;

}
#endif
vpp_csc_t vpp_check_csc_mode(vpp_csc_t mode,vdo_color_fmt src_fmt,vdo_color_fmt dst_fmt,unsigned int flags)
{
	if( mode >= VPP_CSC_MAX ) 
		return VPP_CSC_BYPASS;

	mode = (mode > VPP_CSC_RGB2YUV_MIN)? (mode - VPP_CSC_RGB2YUV_MIN):mode;
	if( src_fmt >= VDO_COL_FMT_ARGB ){
		mode = VPP_CSC_RGB2YUV_MIN + mode;
		src_fmt = VDO_COL_FMT_ARGB;
	}
	else {
		src_fmt = VDO_COL_FMT_YUV444;
	}
	dst_fmt = (dst_fmt >= VDO_COL_FMT_ARGB)? VDO_COL_FMT_ARGB:VDO_COL_FMT_YUV444;
	if( flags == 0 ){
		mode = ( src_fmt != dst_fmt )? mode:VPP_CSC_BYPASS;
	}
	return mode;
}

#if 0
void vpp_set_vout_resolution(int resx,int resy,int fps)
{
#ifdef WMT_FTBLK_GOVW
	p_govw->fb_p->fb.img_w = resx;
	p_govw->fb_p->fb.img_h = resy;
	p_govw->fb_p->set_framebuf(&p_govw->fb_p->fb);
#endif
#ifdef WMT_FTBLK_GOVRH
	p_govrh->fb_p->fb.img_w = resx;
	p_govrh->fb_p->fb.img_h = resy;
	p_govrh->fb_p->framerate = fps;
	p_govrh->fb_p->set_framebuf(&p_govrh->fb_p->fb);
#endif	
}
#endif

int vpp_get_gcd(int A, int B)
{
	while(A != B){
		if( A > B ){
			A = A - B;
		}
		else {
			B = B - A;
		}
	}
	return A;
}

void vpp_check_scale_ratio(int *src,int *dst,int max,int min)
{
	int val1,val2;

	if( *dst >= *src ){	// scale up
		if( (*dst) > ((*src)*max) ){
			DPRINT("*W* scale up over spec (max %d)\n",max);
			*dst = (*src) * max;
		}
	}
	else {	// scale down
		int p,q,diff;
		int diff_min,p_min,q_min;

		val1 = val2 = (*dst) * 1000000 / (*src);
		diff_min = val1;
		p_min = 1;
		q_min = min;
		for(q=2;q<=min;q++){
			for(p=1;p<q;p++){
				val2 = p * 1000000 / q;
				if( val1 < val2 ){
					break;
				}
				diff = vpp_calculate_diff(val1,val2);
				if( diff < diff_min ){
					diff_min = diff;
					p_min = p;
					q_min = q;
				}
			}
			if( val1 == val2 )
				break;
		}
		val1 = (*src) / q_min;
		val2 = (*dst) / p_min;
		val1 = (val1 < val2)? val1:val2;
		*src = val1 * q_min;
		*dst = val1 * p_min;
	}
}	

void vpp_calculate_scale_ratio(unsigned int *src,unsigned int *dst,int max,int min,int align_s,int align_d)
{
	int i;

	if( *dst >= *src ){	// scale up and bypass
		if( (*dst) > ((*src)*max) ){
			DPRINT("*W* scale up over spec (max %d)\n",max);
			*dst = (*src) * max;
		}

		*src -= (*src % align_s);
		*dst -= (*dst % align_d);
	}
	else {	// scale down
		int val1,val2;

		for(i=0;;i++){
			val1 = *src + i;
			val2 = *dst - (*dst % align_d);
			vpp_check_scale_ratio(&val1,&val2,max,min);
			if( val1 < *src ) continue;		// don't drop
			if( val1 % align_s ) continue;	// src img_w align
			if( val2 % align_d ) continue;	// dst img_w align
			if( vpp_check_dbg_level(VPP_DBGLVL_SCALE) ){
				int temp_s,temp_d;
				int diff,diff2;
				
				temp_s = (val1 > *src)? (*src * 100):(val1 * 100);
				diff = vpp_calculate_diff(*src*100,temp_s);

				temp_d = temp_s * val2 / val1;
				diff2 = vpp_calculate_diff(*dst*100,temp_d);
				DPRINT("[VPP] %d:%d->%d,%d->%d,%d->%d,s diff %d,d diff %d\n",i,*src,*dst,val1,val2,temp_s,temp_d,diff,diff2);
			}
			break;
		};
		*src = val1;
		*dst = val2;
	}
}

// check arguments for hw bilinear limit
void vpp_check_bilinear_arg(unsigned int *src,unsigned int *dst,unsigned int max)
{
	if( *dst >= *src ){	// scale up
		if( (*dst) > ((*src)*max) ){
			DBGMSG("*W* scale up over spec (max %d)\n",max);
			*dst = (*src) * max;
		}
	}
	else {	// scale down
		int gcd,q;

		/* --------------------
		1. src should be even
		2. dst should >= 4
		3. dst should <= 1920
		4. q should <= 2048
		-------------------- */
		if( *src % 2 ){
			*src -= 1;
			DBGMSG("[VPU] *W* src should 2x\n");
		}

		if( *dst < 4 ){
			int temp;

			temp = (4 / *dst) + ((4 % *dst)? 1:0);
			*dst *= temp;
			*src *= temp;
			DBGMSG("[VPU] *W* dst should >= 4\n");
		}

		if( *dst > 1920 ){
			*dst = 1920;
			DBGMSG("[VPU] *W* dst should <= 1920\n");
		}

		gcd = vpp_get_gcd(*src,*dst);
		q = *src/gcd;
		if( q > 2048 ) {
			DBGMSG("[VPU] *W* Q > 2048\n");
			vpp_check_scale_ratio((int *)src,(int *)dst,2048,32);
		}
	}
}
#ifdef WMT_FTBLK_SCL
int vpp_set_recursive_scale(vdo_framebuf_t *src_fb,vdo_framebuf_t *dst_fb)
{
	int ret;
	unsigned int s_w,s_h;
	unsigned int d_w,d_h;
	int dst_w_flag = 0;
	int align_s,align_d;
	int keep_ratio;
	int width_min;

	if( vpp_check_dbg_level(VPP_DBGLVL_SCALE) ){
		DPRINT("[S1] src(%dx%d),Y 0x%x,C 0x%x,fb(%dx%d),colfmt %d\n",src_fb->img_w,src_fb->img_h,
								src_fb->y_addr,src_fb->c_addr,src_fb->fb_w,src_fb->fb_h,src_fb->col_fmt);
		DPRINT("[S1] dst(%dx%d),Y 0x%x,C 0x%x,fb(%dx%d),colfmt %d\n",dst_fb->img_w,dst_fb->img_h,
								dst_fb->y_addr,dst_fb->c_addr,dst_fb->fb_w,dst_fb->fb_h,dst_fb->col_fmt);
	}

	s_w = src_fb->img_w;
	s_h = src_fb->img_h;
	d_w = dst_fb->img_w;
	d_h = dst_fb->img_h;
	keep_ratio = 0;
	if( g_vpp.scale_keep_ratio ){
		if( d_w < s_w  ){	// keep ratio feature just in scale down, bcz scale up don't have the scale entry limitation
			keep_ratio = ( vpp_calculate_diff((s_w*100/d_w),(s_h*100/d_h)) < 15 )? 1:0;		// keep ratio if s to d ratio diff less than 15%
		}
	}
	
	// H scale
	if( src_fb->col_fmt == VDO_COL_FMT_ARGB ){
		align_s = ( src_fb->h_crop )? 2:1;
	}
	else {
		align_s = ( src_fb->h_crop )? 8:1;
	}
	align_d = ( dst_fb->col_fmt == VDO_COL_FMT_ARGB )? 2:1;

	if( p_scl->scale_mode == VPP_SCALE_MODE_REC_TABLE ){
		//	printk("[VPP] align s %d,d %d\n",align_s,align_d);
		vpp_calculate_scale_ratio(&src_fb->img_w,&dst_fb->img_w, VPP_SCALE_UP_RATIO_H,
									(keep_ratio)?VPP_SCALE_DN_RATIO_V:VPP_SCALE_DN_RATIO_H,align_s,align_d);
	}
	else {
		if( src_fb->img_w % align_s )
			src_fb->img_w = src_fb->img_w + (align_s - (src_fb->img_w % align_s));
		if( dst_fb->img_w % align_d )
			dst_fb->img_w = dst_fb->img_w - (dst_fb->img_w % align_d);
	}

	if( vpp_check_dbg_level(VPP_DBGLVL_SCALE) ){
		DPRINT("[H] cal: src %d,dst %d\n",src_fb->img_w,dst_fb->img_w);
	}

	// dst width should more than 64 bytes
	width_min = (dst_fb->col_fmt == VDO_COL_FMT_ARGB)? 16:64;
	if( dst_fb->img_w < width_min ){
		int ratio = 1;
		do {
			if( (dst_fb->img_w * ratio) >= width_min)
				break;

			ratio++;
		} while(1);
			
		src_fb->img_w *= ratio;
		dst_fb->img_w *= ratio;
		dst_w_flag = 1;
		if( vpp_check_dbg_level(VPP_DBGLVL_SCALE) ){
			DPRINT("[H] width>=64: src %d,dst %d\n",src_fb->img_w,dst_fb->img_w);
		}
	}

	// V scale
	if( p_scl->scale_mode == VPP_SCALE_MODE_REC_TABLE ){
		if( keep_ratio ){
			int p,q,gcd;
			gcd = vpp_get_gcd(src_fb->img_w,dst_fb->img_w);
			p = dst_fb->img_w / gcd;
			q = src_fb->img_w / gcd;
			dst_fb->img_h = (dst_fb->img_h / p) * p;
			src_fb->img_h = dst_fb->img_h * q / p;
			if( vpp_check_dbg_level(VPP_DBGLVL_SCALE) ){
				DPRINT("[V] keep ratio %d/%d,src %d,dst %d\n",p,q,src_fb->img_h,dst_fb->img_h);
			}
		}
		else {
			vpp_calculate_scale_ratio(&src_fb->img_h,&dst_fb->img_h, VPP_SCALE_UP_RATIO_V, VPP_SCALE_DN_RATIO_V,1,1);
		}
	}
	
	if( vpp_check_dbg_level(VPP_DBGLVL_SCALE) ){
		DPRINT("[V] cal: src %d,dst %d\n",src_fb->img_h,dst_fb->img_h);
	}

	if( vpp_check_dbg_level(VPP_DBGLVL_SCALE) ){
		int h_gcd,v_gcd;
		DPRINT("[S2] src(%dx%d)\n",src_fb->img_w,src_fb->img_h);
		DPRINT("[S2] dst(%dx%d)\n",dst_fb->img_w,dst_fb->img_h);
		h_gcd = vpp_get_gcd(src_fb->img_w,dst_fb->img_w);
		v_gcd = vpp_get_gcd(src_fb->img_h,dst_fb->img_h);
		DPRINT("[S2] H %d/%d,%d, V %d/%d,%d \n",dst_fb->img_w/h_gcd,src_fb->img_w/h_gcd,h_gcd,
			dst_fb->img_h/v_gcd,src_fb->img_h/v_gcd,v_gcd);
	}

	ret = p_scl->scale(src_fb,dst_fb);

	// cut dummy byte
	if( dst_fb->img_w < src_fb->img_w  ){
		if( src_fb->img_w > s_w ){
			unsigned int d_w;
			d_w = dst_fb->img_w;
			dst_fb->img_w = s_w * dst_fb->img_w / src_fb->img_w;
			src_fb->img_w = src_fb->img_w * dst_fb->img_w / d_w;
		}
	}
	else {
		if( dst_w_flag ){
			if( src_fb->img_w > s_w ){
				dst_fb->img_w = s_w * dst_fb->img_w / src_fb->img_w;
				src_fb->img_w = s_w;
			}
		}
	}

	if( dst_fb->img_h < src_fb->img_h ){
		if( src_fb->img_h > s_h ){
			unsigned int d_h;
			d_h = dst_fb->img_h;
			dst_fb->img_h = s_h * dst_fb->img_h / src_fb->img_h;
			src_fb->img_h = src_fb->img_h * dst_fb->img_h / d_h;
		}
	}

	if( vpp_check_dbg_level(VPP_DBGLVL_SCALE) ){
		DPRINT("[S3] src(%dx%d)\n",src_fb->img_w,src_fb->img_h);
		DPRINT("[S3] dst(%dx%d)\n",dst_fb->img_w,dst_fb->img_h);
	}
	return ret;
}
#endif

#ifdef WMT_FTBLK_VPU
static int vpp_check_view(vdo_view_t *vw)
{
	vdo_framebuf_t *fb;
	
	fb = &p_vpu->fb_p->fb;
	if( fb->img_w != vw->resx_virtual ) return 0;
	if( fb->img_h != vw->resy_virtual ) return 0;
	if( fb->fb_w != vw->resx_src ) return 0;
	if( fb->fb_h != vw->resy_src ) return 0;
	if( fb->h_crop != vw->offsetx ) return 0;
	if( fb->v_crop != vw->offsety ) return 0;
	if( p_vpu->resx_visual != vw->resx_visual ) return 0;
	if( p_vpu->resy_visual != vw->resy_visual ) return 0;
	if( p_vpu->posx != vw->posx ) return 0;
	if( p_vpu->posy != vw->posy ) return 0;
	return 1;
}
#endif
#ifdef WMT_FTBLK_VPU
void vpp_set_video_scale(vdo_view_t *vw)
{
	vdo_framebuf_t *fb;
	unsigned int vis_w,vis_h;
	unsigned int vpu_path_bk;

	vpu_path_bk = vppif_reg32_read(GOVM_VPU_SOURCE);
//	vppif_reg32_write(GOVM_VPU_SOURCE, 0);

//	vpp_irqproc_work(VPP_INT_GOVW_VBIE,0,0,1);

#ifdef CONFIG_VPU_DIRECT_PATH
	if( g_vpp.vpu_path == VPP_VPATH_VPU ){
		vpp_clock_t tmr;
		govrh_get_tg(p_govrh2,&tmr);
		
		vw->resx_visual = tmr.end_pixel_of_active - tmr.begin_pixel_of_active;
		vw->resy_visual = tmr.end_line_of_active - tmr.begin_line_of_active;
		vw->posx = vw->posy = 0;
	}
#endif

	if( vpp_check_view(vw) ){
		if( vpp_check_dbg_level(VPP_DBGLVL_SCALE) ){
			DPRINT("[VPP] view not change\n");
		}		
		goto set_video_scale_end;
	}

	if( vpp_check_dbg_level(VPP_DBGLVL_SCALE) ){
		DPRINT("[V1] X src %d,vit %d,vis %d,pos %d,oft %d\n",vw->resx_src,vw->resx_virtual,vw->resx_visual,vw->posx,vw->offsetx);
		DPRINT("[V1] Y src %d,vit %d,vis %d,pos %d,oft %d\n",vw->resy_src,vw->resy_virtual,vw->resy_visual,vw->posy,vw->offsety);
	}

	fb = &p_vpu->fb_p->fb;
	fb->img_w = vw->resx_virtual;
	fb->img_h = vw->resy_virtual;
	fb->fb_w = vw->resx_src;
	fb->fb_h = vw->resy_src;
	fb->h_crop = vw->offsetx;
	fb->v_crop = vw->offsety;
	
	p_vpu->resx_visual = vw->resx_visual;
	p_vpu->resy_visual = vw->resy_visual;
	p_vpu->posx = vw->posx;
	p_vpu->posy = vw->posy;

	p_vpu->fb_p->set_framebuf(fb);
	g_vpp.govw_skip_frame = 3;	

set_video_scale_end:

	vis_w = p_vpu->resx_visual_scale;
	vis_h = p_vpu->resy_visual_scale;
	govm_set_vpu_coordinate(vw->posx,vw->posy,vw->posx+vis_w-1,vw->posy+vis_h-1);
	vppif_reg32_write(GOVM_VPU_SOURCE, vpu_path_bk);

	if( vpp_check_dbg_level(VPP_DBGLVL_SCALE) ){
		int h_gcd,v_gcd;
		DPRINT("[V2] X src %d,vit %d,vis %d,pos %d,oft %d\n",vw->resx_src,p_vpu->resx_virtual_scale,p_vpu->resx_visual_scale,vw->posx,vw->offsetx);
		DPRINT("[V2] Y src %d,vit %d,vis %d,pos %d,oft %d\n",vw->resy_src,p_vpu->resy_virtual_scale,p_vpu->resy_visual_scale,vw->posy,vw->offsety);
		h_gcd = vpp_get_gcd(p_vpu->resx_virtual_scale,p_vpu->resx_visual_scale);
		v_gcd = vpp_get_gcd(p_vpu->resy_virtual_scale,p_vpu->resy_visual_scale);
		DPRINT("[V2] H %d/%d,%d, V %d/%d,%d \n",p_vpu->resx_visual_scale/h_gcd,p_vpu->resx_virtual_scale/h_gcd,
												h_gcd,p_vpu->resy_visual_scale/v_gcd,p_vpu->resy_virtual_scale/v_gcd,v_gcd);
	}
}
#endif
unsigned int vpp_get_video_mode_fps(vpp_timing_t *timing)
{
	unsigned int temp;
	unsigned int resx,resy;
	unsigned int diff1,diff2;

	temp = VPP_GET_OPT_FPS(timing->option);
	if( temp )
		return temp;

	resx = timing->hsync + timing->hbp + timing->hpixel + timing->hfp;
	resy = timing->vsync + timing->vbp + timing->vpixel + timing->vfp;
	temp = timing->pixel_clock - (timing->pixel_clock % 1000);
	temp = temp / resx;
	temp = temp / resy;
	diff1 = vpp_calculate_diff(timing->pixel_clock,(resx*resy*temp));
	diff2 = vpp_calculate_diff(timing->pixel_clock,(resx*resy*(temp+1)));
	temp = (diff1 < diff2)? temp:(temp+1);
	return temp;
}

unsigned int vpp_vout_option;
vpp_timing_t *vpp_get_video_mode(unsigned int resx,unsigned int resy,unsigned int fps_pixclk,int *index)
{
	int is_fps;
	int i,j;
	vpp_timing_t *ptr;
	unsigned int line_pixel;
	int first_order = 1;

	if( vpp_video_mode_table[*index].option & VPP_OPT_INTERLACE ){
		if( vpp_video_mode_table[*index].option != vpp_video_mode_table[*index+1].option ){
			*index=*index-1;
			printk("1 index %d\n",*index);
		}
	}

	is_fps = ( fps_pixclk >= 1000000 )? 0:1;
	for(i=*index;;i++){
		ptr = (vpp_timing_t *) &vpp_video_mode_table[i];
		if( ptr->pixel_clock == 0 ){
			break;
		}
		if( vpp_vout_option & VOUT_OPT_INTERLACE ){
			if( !(ptr->option & VPP_OPT_INTERLACE) )
				continue;
		}
		
		line_pixel = (ptr->option & VPP_OPT_INTERLACE)? (ptr->vpixel*2):ptr->vpixel;
		if ((ptr->hpixel == resx) && (line_pixel == resy)) {
			for(j=i,*index=i;;j++){
				ptr = (vpp_timing_t *) &vpp_video_mode_table[j];
				if( ptr->pixel_clock == 0 ){
					break;
				}
				if( ptr->hpixel != resx ){
					break;
				}
				
				line_pixel = (ptr->option & VPP_OPT_INTERLACE)? (ptr->vpixel*2):ptr->vpixel;
				if( line_pixel != resy ){
					break;
				}
				
				if( is_fps ){
					if( fps_pixclk == vpp_get_video_mode_fps(ptr) ){
						*index = j;
						break;
					}
				}
				else {
					if( (fps_pixclk/10000) == (ptr->pixel_clock/10000) ){
						if( first_order ) // keep first order for same pixclk
							*index = j;
						first_order = 0;
					}
					if( (fps_pixclk) == (ptr->pixel_clock) ){
						*index = j;
						goto get_mode_end;
					}
				}
			}
			break;
		}
		if( ptr->hpixel > resx ){
			break;
		}
		*index = i;
		if( ptr->option & VPP_OPT_INTERLACE ){
			i++;
		}
	}
get_mode_end:
	ptr = (vpp_timing_t *) &vpp_video_mode_table[*index];
//	printk("[VPP] get video mode %dx%d@%d,index %d,0x%x\n",resx,resy,fps_pixclk,index,vpp_vout_option);	
	return ptr;	
}

vpp_timing_t *vpp_get_video_mode_ext(unsigned int resx,unsigned int resy,unsigned int fps_pixclk,unsigned int option)
{
	vpp_timing_t *time;
	int index = 0;

	vpp_vout_option = option;
	time = vpp_get_video_mode(resx,resy,fps_pixclk,&index);
	vpp_vout_option = 0;
	return time;
}

void vpp_set_video_quality(int mode)
{
#ifdef WMT_FTBLK_VPU
	vpu_set_drop_line((mode)?0:1);
	if((vppif_reg32_read(VPU_DEI_ENABLE)==0) && (vppif_reg32_read(VPU_DROP_LINE)) ){
		vpu_r_set_mif2_enable(VPP_FLAG_DISABLE);
	}
	else {
		vpu_r_set_mif2_enable(VPP_FLAG_ENABLE);
	}
#endif

#ifdef WMT_FTBLK_SCL
	scl_set_drop_line((mode)?0:1);
	if( vppif_reg32_read(SCL_TG_GOVWTG_ENABLE) && (vppif_reg32_read(SCL_SCLDW_METHOD)==0)){
		sclr_set_mif2_enable(VPP_FLAG_ENABLE);
	}
	else {
		sclr_set_mif2_enable(VPP_FLAG_DISABLE);
	}
#endif
}

#if 0
unsigned int vpp_calculate_y_crop(unsigned int hcrop,unsigned int vcrop,unsigned int fbw,vdo_color_fmt colfmt)
{
	unsigned int offset;

	offset = vcrop * fbw + hcrop;
	if( colfmt == VDO_COL_FMT_ARGB ){
		offset *= 4;
	}
	return offset;
}

unsigned int vpp_calculate_c_crop(unsigned int hcrop,unsigned int vcrop,unsigned int fbw,vdo_color_fmt colfmt)
{
	unsigned int offset;
	unsigned int stribe;
	unsigned int c_line,c_pixel;

	if( colfmt == VDO_COL_FMT_ARGB )
		return 0;

	switch( colfmt ){
		case VDO_COL_FMT_YUV420:
			c_pixel = 2;
			c_line = 2;
			break;
		case VDO_COL_FMT_YUV422H:
			c_pixel = 2;
			c_line = 1;
			break;
		default:
		case VDO_COL_FMT_YUV444:
			c_pixel = 1;
			c_line = 1;
			break;
	}
	stribe = fbw * 2 / c_pixel;
	offset = (vcrop / c_line) * stribe + (hcrop / c_pixel) * 2;
	return offset;
}
#endif

void vpp_set_vppm_int_enable(vpp_int_t int_bit,int enable)
{
#ifndef CFG_LOADER
	if( int_bit & VPP_INT_GOVRH_VBIS ){
		int int_enable;

		int_enable = enable;
//		if( vppif_reg32_read(GOVRH_CUR_ENABLE) )	// govrh hw cursor
//			int_enable = 1;

		if( g_vpp.vpp_path )	// direct path
			int_enable = 1;

		vppm_set_int_enable(int_enable,VPP_INT_GOVRH_VBIS);
		int_bit &= ~VPP_INT_GOVRH_VBIS;
	}

	if( int_bit & VPP_INT_GOVRH_PVBI ){
		int int_enable;

		int_enable = enable;
		if( p_cursor->enable ) // govrh hw cursor
			int_enable = 1;

		if( g_vpp.vpp_path )	// direct path
			int_enable = 1;

		vppm_set_int_enable(int_enable,VPP_INT_GOVRH_PVBI);
		int_bit &= ~VPP_INT_GOVRH_PVBI;
	}
	
	if( int_bit ){
		vppm_set_int_enable(enable,int_bit);
	}
#endif
} /* End of vpp_set_vppm_int_enable */

void vpp_reg_dump(unsigned int addr,int size)
{
	int i;

	for(i=0;i<size;i+=16){
		DPRINT("0x%8x : 0x%08x 0x%08x 0x%08x 0x%08x\n",addr+i,vppif_reg32_in(addr+i),
			vppif_reg32_in(addr+i+4),vppif_reg32_in(addr+i+8),vppif_reg32_in(addr+i+12));
	}
} /* End of vpp_reg_dump */

unsigned int vpp_convert_colfmt(int yuv2rgb,unsigned int data)
{
	unsigned int r,g,b;
	unsigned int y,u,v;
	unsigned int alpha;

	alpha = data & 0xff000000;
	if( yuv2rgb ){
		y = (data & 0xff0000) >> 16;
		u = (data & 0xff00) >> 8;
		v = (data & 0xff) >> 0;

		r = ((1000*y) + 1402*(v-128)) / 1000;
		if( r > 0xFF ) r = 0xFF;
		g = ((100000*y) - (71414*(v-128)) - (34414*(u-128))) / 100000;
		if( g > 0xFF ) g = 0xFF;
		b = ((1000*y) + (1772*(u-128))) / 1000;
		if( b > 0xFF ) b = 0xFF;

		data = ((r << 16) + (g << 8) + b);
	}
	else {
		r = (data & 0xff0000) >> 16;
		g = (data & 0xff00) >> 8;
		b = (data & 0xff) >> 0;

		y = ((2990*r) + (5870*g) + (1440*b)) / 10000;
		if( y > 0xFF ) y = 0xFF;
		u = (1280000 - (1687*r) - (3313*g) + (5000*b)) / 10000;
		if( u > 0xFF ) u = 0xFF;
		v = (1280000 + (5000*r) - (4187*g) - (813*b)) / 10000;
		if( v > 0xFF ) v = 0xFF;

		data = ((y << 16) + (v << 8) + u);
	}
	data = data + alpha;
	return data;
}

int vpp_govm_path1;
#ifdef WMT_FTBLK_GOVM
void vpp_set_govm_path(vpp_path_t in_path, vpp_flag_t enable)
{
//#ifdef CONFIG_HW_GOVW_TG_ENABLE
	if (VPP_PATH_GOVM_IN_VPU & in_path) {
		vpp_govm_path1 = (enable)? (vpp_govm_path1 | VPP_PATH_GOVM_IN_VPU):(vpp_govm_path1 & ~VPP_PATH_GOVM_IN_VPU);
	}
	if (VPP_PATH_GOVM_IN_GE & in_path) {
		vpp_govm_path1 = (enable)? (vpp_govm_path1 | VPP_PATH_GOVM_IN_GE):(vpp_govm_path1 & ~VPP_PATH_GOVM_IN_GE);
	}
	if (VPP_PATH_GOVM_IN_PIP & in_path) {
		vpp_govm_path1 = (enable)? (vpp_govm_path1 | VPP_PATH_GOVM_IN_PIP):(vpp_govm_path1 & ~VPP_PATH_GOVM_IN_PIP);
	}
#if 0	
	if( vppif_reg32_read(GOVW_TG_ENABLE) == 0 )
		enable = 0;
#endif
	govm_set_in_path(in_path,enable);
	if( in_path & VPP_PATH_GOVM_IN_VPU ){
		vpu_set_enable(enable);
		vpu_r_set_mif_enable(enable);
		vpu_r_set_mif2_enable(enable);
	}

//	DPRINT("[VPP] set path 0x%x, GOVW TG %d,vpu en %d,vpu path %d\n",vpp_govm_path1,vppif_reg32_read(GOVW_TG_ENABLE),vppif_reg32_read(VPU_SCALAR_ENABLE),vppif_reg32_read(GOVM_VPU_SOURCE));	
}

vpp_path_t vpp_get_govm_path(void)
{
#if 1
//#ifdef CONFIG_HW_GOVW_TG_ENABLE
	return vpp_govm_path1;
#else
	return govm_get_in_path();
#endif
}
#endif
#ifdef WMT_FTBLK_GOVW
void vpp_set_govw_tg(int enable)
{
	switch(g_vpp.vpp_path){
		case VPP_VPATH_GOVW:
		case VPP_VPATH_GOVW_SCL:
			break;
		default:
			enable = VPP_FLAG_DISABLE;
			break;
	}
	if(enable){
		vpp_mod_set_clock(VPP_MOD_GOVW,VPP_FLAG_ENABLE,0);
		vpp_mod_set_clock(VPP_MOD_VPU,VPP_FLAG_ENABLE,0);
	}
	govw_set_tg_enable(enable);
	govw_set_mif_enable(enable);
	if(!enable){
		vpp_mod_set_clock(VPP_MOD_GOVW,VPP_FLAG_DISABLE,0);
		vpp_mod_set_clock(VPP_MOD_VPU,VPP_FLAG_DISABLE,0);
	}
}
#endif

unsigned int *vpp_backup_reg(unsigned int addr,unsigned int size)
{
	unsigned int *ptr;
	int i;

	size += 4;
	if( (ptr = (unsigned int*) kmalloc(size,GFP_KERNEL)) == 0 ){
		DPRINT("[VPP] *E* malloc backup fail\n");
		return 0;
	}

	for(i=0;i<size;i+=4){
		ptr[i/4] = REG32_VAL(addr+i);
	}
	return ptr;
} /* End of vpp_backup_reg */

int vpp_restore_reg(unsigned int addr,unsigned int size,unsigned int *reg_ptr)
{
	int i;

	if( reg_ptr == NULL )
		return 0;

	size += 4;
	for(i=0;i<size;i+=4){
		REG32_VAL(addr+i) = reg_ptr[i/4];
	}
	kfree(reg_ptr);
	reg_ptr = 0;
	return 0;
} /* End of vpp_restore_reg */

void vpp_backup_reg2(unsigned int addr,unsigned int size,unsigned int *ptr)
{
	int i;

	size += 4;
	for(i=0;i<size;i+=4){
		ptr[i/4] = REG32_VAL(addr+i);
	}
}

void vpp_restore_reg2(unsigned int addr,unsigned int size,unsigned int *reg_ptr)
{
	int i;

	if( reg_ptr == NULL )
		return;

	size += 4;
	for(i=0;i<size;i+=4){
		REG32_VAL(addr+i) = reg_ptr[i/4];
	}
}

#ifdef WMT_FTBLK_VPU
void vpp_vpu_sw_reset(void)
{
	unsigned int reg_bk1[64];
	unsigned int reg_bk2[64];
//	unsigned int reg_bk;

//	reg_bk = vppif_reg32_read(GOVW_TG_ENABLE);
//	vppif_reg32_write(GOVW_TG_ENABLE,0);
#if 0
	reg_bk = vppif_reg32_read(GOVM_VPU_SOURCE);
	vppif_reg32_write(GOVM_VPU_SOURCE, 0);
#endif
	vpu_set_reg_update(0);

	p_vpu->suspend(0);
	p_vpu->suspend(1);
	// p_vpu->suspend(2);
	vpp_backup_reg2(REG_VPU_BASE1_BEGIN,(REG_VPU_BASE1_END-REG_VPU_BASE1_BEGIN),reg_bk1);
	vpp_backup_reg2(REG_VPU_BASE2_BEGIN,(REG_VPU_BASE2_END-REG_VPU_BASE2_BEGIN),reg_bk2);
	
	vppm_set_module_reset(VPP_MOD_VPU);
	// p_vpu->resume(0);
	vpp_restore_reg2(REG_VPU_BASE1_BEGIN,(REG_VPU_BASE1_END-REG_VPU_BASE1_BEGIN),reg_bk1);
	vpp_restore_reg2(REG_VPU_BASE2_BEGIN,(REG_VPU_BASE2_END-REG_VPU_BASE2_BEGIN),reg_bk2);

	p_vpu->resume(1);
	p_vpu->resume(2);

	vpu_set_reg_update(1);
#if 0
`	extern int vpp_irqproc_enable_vpu_path_cnt;;
	extern int vpp_irqproc_enable_vpu_path(void *arg);

	if( vpp_irqproc_enable_vpu_path_cnt == 0 ){
		vpp_irqproc_enable_vpu_path_cnt = 3;
		vpp_irqproc_work(VPP_INT_GOVW_PVBI,(void *)vpp_irqproc_enable_vpu_path,0,0);
	}
	vppif_reg32_write(GOVW_TG_ENABLE,reg_bk);
#else
//	vppif_reg32_write(GOVM_VPU_SOURCE, reg_bk);
#endif
	g_vpp.govw_skip_frame = 3;
}
#endif //fan

void vpp_get_sys_parameter(void)
{
	char buf[40];
	int varlen = 40;

	// vpp attribute by default
	g_vpp.disp_fb_max = VPP_DISP_FB_NUM;
	g_vpp.disp_fb_cnt = 0;
	g_vpp.video_quality_mode = 1;
	g_vpp.scale_keep_ratio = 1;
	g_vpp.disp_fb_keep = 0;
#ifdef CONFIG_VPP_GOVRH_FBSYNC
	g_vpp.fbsync_enable = 1;
#endif
	g_vpp.hdmi_audio_interface = VPP_HDMI_AUDIO_SPDIF;
	g_vpp.hdmi_cp_enable = 1;
	g_vpp.govw_vfp = 5;
	g_vpp.govw_vbp = 5;
	g_vpp.vpp_path = VPP_VPATH_GE;
	g_vpp.vpu_path = VPP_VPATH_GOVW;
#ifdef CONFIG_VPP_GE_DIRECT_PATH
	g_vpp.ge_path = VPP_VPATH_GE;
#else
	g_vpp.ge_path = VPP_VPATH_GOVW;
#endif
#ifdef WMT_FTBLK_GOVRH
	g_vpp.govr = p_govrh2;
#endif

#if 0
	if( wmt_getsyspara("wmt.display.direct_path",buf,&varlen) == 0 ){
		sscanf(buf,"%d",&g_vpp.direct_path);
		DPRINT("[VPP] direct path %d\n",g_vpp.direct_path);
	}
#endif

	if( wmt_getsyspara("wmt.display.hdmi_audio_inf",buf,&varlen) == 0 ){
		if( memcmp(buf,"i2s",3) == 0 ){
//			DPRINT("[VPP] hdmi audio i2s\n");
			g_vpp.hdmi_audio_interface = VPP_HDMI_AUDIO_I2S;
		}
		else if( memcmp(buf,"spdif",5) == 0 ){
//			DPRINT("[VPP] hdmi audio spdif\n");
			g_vpp.hdmi_audio_interface = VPP_HDMI_AUDIO_SPDIF;
		}	
	}

	if( wmt_getsyspara("wmt.display.hdmi.vmode",buf,&varlen) == 0 ){
		if( memcmp(buf,"720p",4) == 0 ){
			g_vpp.hdmi_video_mode = 720;
		}
		else if( memcmp(buf,"1080p",5) == 0 ){
			g_vpp.hdmi_video_mode = 1080;
		}
		else {
			g_vpp.hdmi_video_mode = 0;
		}
		DPRINT("[VPP] HDMI video mode %d\n",g_vpp.hdmi_video_mode);
	}

#ifdef CONFIG_GOVW_SCL_PATH
	if( wmt_getsyspara("wmt.display.path",(unsigned char *)buf,&varlen) == 0){
		unsigned int arg[4];

		vpp_parse_param(buf,arg,4,0);
		g_vpp.ge_path = arg[0];
		g_vpp.vpu_path = arg[1];
		p_govw->fb_p->fb.img_w = arg[2];
		p_govw->fb_p->fb.img_h = arg[3];
		p_govw->fb_p->fb.fb_w = vpp_calc_align(p_govw->fb_p->fb.img_w,VPP_FB_WIDTH_ALIGN);
		p_govw->fb_p->fb.fb_h = p_govw->fb_p->fb.img_h;
		g_vpp.vpp_path = g_vpp.ge_path;
		DPRINT("[VPP] path GE(%s),VPU(%s),res(%dx%d)\n",vpp_vpath_str[arg[0]],vpp_vpath_str[arg[1]],arg[2],arg[3]);
	}
#endif

#ifdef CONFIG_VPP_OVERSCAN
	/* [uboot parameter] fb param : no:xresx:yres:xoffset:yoffset */
	if( wmt_getsyspara("wmt.display.fb.res",buf,&varlen) == 0 ){
		unsigned int parm[5];

		MSG("fb param : %s\n",buf);
		vpp_parse_param(buf,(unsigned int *)parm,5,0);
		MSG("fb parm %d res(%dx%d),offset(%d,%d)\n",parm[0],parm[1],parm[2]);
		vout_info[parm[0]].fb_xres = parm[1];
		vout_info[parm[0]].fb_yres = parm[2];
		vout_info[parm[0]].fb_xoffset = parm[3];
		vout_info[parm[0]].fb_yoffset = parm[4];
		vout_info->fb_clr = ~0x0;
	}
#endif

	g_vpp.mb_colfmt = VPP_UBOOT_COLFMT;
	/* [uboot parameter] fb param : no:xresx:yres:xoffset:yoffset */
	if( wmt_getsyspara("wmt.gralloc.param",buf,&varlen) == 0 ){
		unsigned int parm[1];

		vpp_parse_param(buf,(unsigned int *)parm,1,0x1);
		if( parm[0] == 32 )
			g_vpp.mb_colfmt = VDO_COL_FMT_ARGB;
		MSG("mb colfmt : %s,%s\n",buf,vpp_colfmt_str[g_vpp.mb_colfmt]);
	}
	p_govrh->fb_p->fb.col_fmt = p_govrh2->fb_p->fb.col_fmt = g_vpp.mb_colfmt;

	/* [uboot parameter] dual display : 0-single display, 1-dual display */
	g_vpp.dual_display = 1;
	if( wmt_getsyspara("wmt.display.dual",buf,&varlen) == 0){
		unsigned int parm[1];
		
		MSG("display dual : %s\n",buf);
		vpp_parse_param(buf,(unsigned int *)parm,1,0);
		g_vpp.dual_display = parm[0];
	}

	if( g_vpp.dual_display == 0 ){
		g_vpp.alloc_framebuf = 0;
	}
	
} /* End of vpp_get_sys_parameter */

void vpp_init(void)
{
	vpp_mod_base_t *mod_p;
	unsigned int mod_mask;
	int i;
		
	vpp_get_sys_parameter();

	auto_pll_divisor(DEV_NA12,CLK_ENABLE,0,0);
	auto_pll_divisor(DEV_VPP,CLK_ENABLE,0,0);
	auto_pll_divisor(DEV_HDCE,CLK_ENABLE,0,0);
	auto_pll_divisor(DEV_HDMII2C,CLK_ENABLE,0,0);
	auto_pll_divisor(DEV_HDMI,CLK_ENABLE,0,0);
	auto_pll_divisor(DEV_GOVRHD,CLK_ENABLE,0,0);
	auto_pll_divisor(DEV_DVO,CLK_ENABLE,0,0);
	auto_pll_divisor(DEV_HDMILVDS,CLK_ENABLE,0,0);
	auto_pll_divisor(DEV_VPU,CLK_ENABLE,0,0);
	auto_pll_divisor(DEV_SCL444U,CLK_ENABLE,0,0);
	auto_pll_divisor(DEV_GOVW,CLK_ENABLE,0,0);
#ifndef CFG_LOADER
	if( g_vpp.vpp_path != VPP_VPATH_GOVW_SCL ){
		if( govrh_get_MIF_enable(p_govrh) ){
			g_vpp.govrh_preinit = 1;	
		}
		if( govrh_get_MIF_enable(p_govrh2) ){
			g_vpp.govrh_preinit = 1; 
		}
		MSG("[VPP] govrh preinit %d\n",g_vpp.govrh_preinit);
	}
#else
	govrh_set_clock(p_govrh,65000000);	// for HDMI registers access
#endif

	if( g_vpp.alloc_framebuf ){
		g_vpp.alloc_framebuf(VPP_HD_MAX_RESX,VPP_HD_MAX_RESY);
#ifndef CONFIG_UBOOT
		{
		vdo_framebuf_t *fb;
		
		fb = &g_vpp.govr->fb_p->fb;
		fb->y_addr = g_vpp.mb_govw[1];
		fb->c_addr = g_vpp.mb_govw[1] + g_vpp.mb_govw_y_size;
		fb->y_size = g_vpp.mb_govw_y_size;
		fb->c_size = g_vpp.mb_fb_size - g_vpp.mb_govw_y_size;
		fb->fb_w = p_govw->fb_p->fb.fb_w;
		fb->fb_h = p_govw->fb_p->fb.fb_h;
		}
#endif
	}

#ifdef __KERNEL__
	if( g_vpp.vpp_path == VPP_VPATH_GE ){
		extern unsigned long num_physpages;

		g_vpp.govr->fb_p->fb.y_addr = num_physpages << PAGE_SHIFT;
		g_vpp.govr->fb_p->fb.c_addr = 0;
		g_vpp.govr->fb_p->fb.col_fmt = g_vpp.mb_colfmt;
	}
#ifdef CONFIG_GOVW_SCL_PATH
	if( g_vpp.vpp_path == VPP_VPATH_GOVW_SCL ){
		govrh_set_tg_enable(p_govrh,0);
		vpp_clr_framebuf(VPP_MOD_GOVRH);
	}
#endif
#endif
	// init video out module first
	if( g_vpp.govrh_preinit == 0 ){
		mod_mask = BIT(VPP_MOD_GOVRH2) | BIT(VPP_MOD_GOVRH) | BIT(VPP_MOD_DISP) | BIT(VPP_MOD_LCDC);
		for(i=0;i<VPP_MOD_MAX;i++){
			if( !(mod_mask & (0x01 << i)) ){
				continue;
			}
			mod_p = vpp_mod_get_base(i);
			if( mod_p && mod_p->init ){
				mod_p->init(mod_p);
			}
		}
	}

#ifndef CFG_LOADER
	// init other module
	mod_mask =  BIT(VPP_MOD_GOVW) | BIT(VPP_MOD_GOVM) | BIT(VPP_MOD_SCL) | BIT(VPP_MOD_SCLW) | BIT(VPP_MOD_VPU) |
				BIT(VPP_MOD_VPUW) | BIT(VPP_MOD_PIP) | BIT(VPP_MOD_VPPM);
	for(i=0;i<VPP_MOD_MAX;i++){
		if( !(mod_mask & (0x01 << i)) ){
			continue;
		}
		mod_p = vpp_mod_get_base(i);
		if( mod_p && mod_p->init ){
			mod_p->init(mod_p);
		}
	}
#endif

#ifdef WMT_FTBLK_LVDS
	if(	!g_vpp.govrh_preinit ){
#ifdef WMT_FTBLK_HDMI
		if(vppif_reg32_read(HDMI_ENABLE)==0)
#endif
		{
			lvds_init();
		}
	}
#endif
#ifdef WMT_FTBLK_VOUT_HDMI
	hdmi_init();
#endif

#ifndef CONFIG_VPP_DISABLE_PM
	auto_pll_divisor(DEV_VPU,CLK_DISABLE,0,0);
	auto_pll_divisor(DEV_SCL444U,CLK_DISABLE,0,0);
	auto_pll_divisor(DEV_GOVW,CLK_DISABLE,0,0);
#endif

	// init vout device & get default resolution
	vout_init();

	auto_pll_divisor(DEV_HDCE,CLK_DISABLE,0,0);
	auto_pll_divisor(DEV_HDMII2C,CLK_DISABLE,0,0);
	auto_pll_divisor(DEV_HDMI,CLK_DISABLE,0,0);

#ifdef __KERNEL__
#ifndef CONFIG_HW_GOVR2_DVO
	vppm_set_DVO_select((g_vpp.dual_display)?0:1);
#endif
#ifdef CONFIG_GOVW_SCL_PATH
	if( g_vpp.vpp_path == VPP_VPATH_GOVW_SCL ){
		vout_info_t *info;

		info = vout_info_get_entry(0);
		p_govrh->fb_p->fb.img_w = info->resx;
		p_govrh->fb_p->fb.img_h = info->resy;
		info->resx = p_govw->fb_p->fb.img_w;
		info->resy = p_govw->fb_p->fb.img_h;
	}
#endif
#else
#ifndef CONFIG_HW_GOVR2_DVO
	vppif_reg32_write(VPP_DVO_SEL,(g_vpp.dual_display)?0:1);
#endif
#endif
}

void vpp_config(vout_info_t *info)
{
	vdo_framebuf_t *fb = 0;
	unsigned int vo_mask,govr_mask;

	DBG_MSG("Enter(%dx%d@%d),vir(%d,%d)\n",info->resx,info->resy,info->fps,info->resx_virtual,info->resy_virtual);

	if( (g_vpp.dual_display == 0) || g_vpp.virtual_display ){
		govr_mask = 0xFF;
	}
	else {
		govr_mask = (info->govr == p_govrh)? 0x1:0x2;
	}
	vo_mask = vout_get_mask(info);

	//--> added by howayhuo for virtual_display
	if(g_vpp.virtual_display && !vo_mask)
		return;				
	//<-- end add

	vout_check_info(vo_mask,info);

	if( info->govr == g_vpp.govr ){ // vpu path setting
		DBG_MSG("vpu config(%s)\n",vpp_vpath_str[g_vpp.vpp_path]);
	
		if( g_vpp.alloc_framebuf ){
			g_vpp.alloc_framebuf(info->resx,info->resy);
//			vpp_clr_framebuf(VPP_MOD_GOVRH);
//			vpp_clr_framebuf(VPP_MOD_GOVW);
		}
	}

	if( govr_mask & 0x1 ){
		DBG_MSG("govrh1\n");
		fb = &p_govrh->fb_p->fb;
//govrh_get_framebuffer(p_govrh,fb);
		fb->fb_w = info->resx_virtual;
		fb->fb_h = info->resy_virtual;
		fb->img_w = info->resx;
		fb->img_h = info->resy;
	}

	if( govr_mask & 0x2 ){
		DBG_MSG("govrh2\n");
		fb = &p_govrh2->fb_p->fb;
//govrh_get_framebuffer(p_govrh2,fb);
		fb->fb_w = info->resx_virtual;
		fb->fb_h = info->resy_virtual;
		fb->img_w = info->resx;
		fb->img_h = info->resy;
	}

	if( g_vpp.govrh_preinit == 0 ){
		int mif1,mif2;

		mif1 = mif2 = 0;
//		vout_set_tg_enable(vo_mask,VPP_FLAG_DISABLE);
		if( govr_mask & 0x1 ){
			mif1 = govrh_get_MIF_enable(p_govrh);
			govrh_set_MIF_enable(p_govrh,VPP_FLAG_DISABLE);
			govrh_set_framebuffer(p_govrh,fb);
			if( fb->y_addr == 0 ){
				vpp_show_framebuf("config govrh1",fb);
			}
		}
		
		if( govr_mask & 0x2 ){
			mif2 = govrh_get_MIF_enable(p_govrh2);
			govrh_set_MIF_enable(p_govrh2,VPP_FLAG_DISABLE);
			govrh_set_framebuffer(p_govrh2,fb);
			if( fb->y_addr == 0 ){
				vpp_show_framebuf("config govrh2",fb);
			}
		}
		vout_config(vo_mask,info);
//		vout_set_tg_enable(vo_mask,VPP_FLAG_ENABLE);
		if( govr_mask & 0x1 ){
			govrh_set_MIF_enable(p_govrh,mif1);
		}
		if( govr_mask & 0x2 ){
			govrh_set_MIF_enable(p_govrh2,mif2);
		}
	}

	if( info->govr == g_vpp.govr ){ // vpu path setting
#ifdef WMT_FTBLK_GOVW
		auto_pll_divisor(DEV_GOVW,CLK_ENABLE,0,0);
		vpp_set_govw_tg(VPP_FLAG_DISABLE);
		p_govw->fb_p->set_framebuf(&p_govw->fb_p->fb);
#endif

#ifdef WMT_FTBLK_VPU
		auto_pll_divisor(DEV_VPU,CLK_ENABLE,0,0);
		// set vpu frame buffer info
		fb = &p_vpu->fb_p->fb;
		if( fb->y_addr ){
			p_vpu->posx = p_vpu->posy = 0;
			p_vpu->resx_visual = info->resx;
			p_vpu->resy_visual = info->resy;
		}
		else {
			fb->fb_w = fb->img_w = p_vpu->resx_visual = info->resx;
			fb->fb_h = fb->img_h = p_vpu->resy_visual = info->resy;
		}
		p_vpu->fb_p->set_framebuf(fb);
		govm_set_vpu_coordinate(p_vpu->posx,p_vpu->posy,p_vpu->posx+p_vpu->resx_visual_scale-1,p_vpu->posy+p_vpu->resy_visual_scale-1);
		auto_pll_divisor(DEV_VPU,CLK_DISABLE,0,0);		
#endif
#ifdef WMT_FTBLK_GOVW
		vpp_set_govw_tg((g_vpp.vpp_path)? VPP_FLAG_DISABLE:VPP_FLAG_ENABLE);
		auto_pll_divisor(DEV_GOVW,CLK_DISABLE,0,0);
#endif
	}
	g_vpp.govrh_preinit = 0;
#ifdef CONFIG_VPP_OVERSCAN
	if(info->fb_xres){
		info->fb_clr = ~0x0;
	}
#endif
	DBG_DETAIL("Leave\n");
} /* End of vpp_config */

#ifdef CONFIG_VPP_OVERSCAN
void vpp_plugin_reconfig(int no)
{
	vout_info_t *vo_info;

	DBG_MSG("%d\n",no);

	if( (vo_info = vout_get_info_entry(no)) == 0 )
		return;

	if( vo_info->fb_xres == 0 )
		return;

	vo_info->resx = 1920;
	vo_info->resy = 1080;
	vo_info->pixclk = 60;
	vpp_config(vo_info);
	vpp_clr_framebuf(vo_info->govr_mod);
}
#endif

void vpp_get_colfmt_bpp(vdo_color_fmt colfmt,int *y_bpp,int *c_bpp)
{
	switch(colfmt){
		case VDO_COL_FMT_YUV420:
			*y_bpp = 8;
			*c_bpp = 4;
			break;
		case VDO_COL_FMT_YUV422H:
		case VDO_COL_FMT_YUV422V:
			*y_bpp = 8;
			*c_bpp = 8;
			break;
	    case VDO_COL_FMT_RGB_565:
	    case VDO_COL_FMT_RGB_1555:
	    case VDO_COL_FMT_RGB_5551:
			*y_bpp = 16;
			*c_bpp = 0;
			break;
		case VDO_COL_FMT_YUV444:
			*y_bpp = 8;
			*c_bpp = 16;
			break;
		case VDO_COL_FMT_RGB_888:
		case VDO_COL_FMT_RGB_666:
			*y_bpp = 24;
			*c_bpp = 0;
			break;
		case VDO_COL_FMT_ARGB:
			*y_bpp = 32;
			*c_bpp = 0;
			break;
		default:
			break;
	}
}

int vpp_calc_align(int value,int align)
{
	if( value % align ){
		value &= ~(align-1);
		value += align;
	}
	return value;
}

int vpp_calc_fb_width(vdo_color_fmt colfmt,int width)
{
	int y_bpp,c_bpp;

	vpp_get_colfmt_bpp(colfmt,&y_bpp,&c_bpp);
	return vpp_calc_align(width,VPP_FB_WIDTH_ALIGN/(y_bpp/8));
}
