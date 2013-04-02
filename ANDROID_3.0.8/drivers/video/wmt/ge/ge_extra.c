#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/fb.h>
#include <linux/dma-mapping.h>
#include <asm/page.h>
#include <linux/mm.h>
#include <linux/sched.h>

#include "../vpp.h"
#include "com-ge.h"

int ge_set_vpu(ge_info_t *geinfo,
	       const ge_surface_t *src,
	       ge_rect_t *sr,
	       ge_rect_t *dr)
{
	void *vfb;
	void *view;

	if (src) {
		vfb = ge_helper_create_vdo_framebuf();
		view = ge_helper_create_vdo_view();

		ge_helper_get_vdo_framebuf(vfb, src, sr);
		ge_helper_get_vdo_view(view, src, sr, dr);

		/* vpp.h */
		vpu_set_direct_path(VPP_VPATH_VPU_GE);
		p_vpu->fb_p->fb = *((vdo_framebuf_t *) vfb);
		vpu_r_set_framebuffer(vfb);
		vpp_set_video_scale(view);

		ge_helper_release_vdo_framebuf(vfb);
		ge_helper_release_vdo_view(view);
	} else {
		/* vpp.h */
		vpu_set_direct_path(0);
	}

	return 0;
}

int ge_stretch_blit_vpu(ge_info_t *geinfo,
			const ge_surface_t *src,
			const ge_surface_t *dst,
			const ge_surface_t *out,
			ge_rect_t *sr,
			ge_rect_t *dr)
{
	ge_regs_t *regs = (ge_regs_t *)geinfo->mmio;
	ge_surface_t *s;
	ge_surface_t *d;
	ge_surface_t *o;
	ge_surface_t *vpu;
	ge_rect_t sr2[1];
	ge_rect_t dr2[1];

	if (!sr) {
		sr2->x = src->x;
		sr2->y = src->y;
		sr2->w = src->xres;
		sr2->h = src->yres;
		sr = sr2;
	}

	if (!dr) {
		dr2->x = dst->x;
		dr2->y = dst->y;
		dr2->w = dst->xres;
		dr2->h = dst->yres;
		dr = dr2;
	}

	s = d = o = NULL;

	if (src)
		s = ge_dup_surface(geinfo, src);
	if (dst)
		d = ge_dup_surface(geinfo, dst);
	if (out)
		o = ge_dup_surface(geinfo, out);

	if (sr) {
		s->x = sr->x;
		s->y = sr->y;
		s->xres = sr->w;
		s->yres = sr->h;
	}

	if (dr) {
		d->x = dr->x;
		d->y = dr->y;
		d->xres = dr->w;
		d->yres = dr->h;
		if (o) {
			o->x = dr->x;
			o->y = dr->y;
			o->xres = dr->w;
			o->yres = dr->h;
		}
	}

	/* set vpu */
	ge_set_vpu(geinfo, s, sr, dr);

	vpu = ge_create_null_surface(geinfo, dr->w, dr->h, GSPF_VPU);

	if (d->pixelformat == GSPF_ARGB) {
		d->blitting_flags |= GSBLIT_BLEND_ALPHACHANNEL;
		d->src_blend_func = GSBF_INVDESTALPHA;
		d->dst_blend_func = GSBF_DESTALPHA;
	}

	if (d->blitting_flags & GSBLIT_BLEND_ALPHACHANNEL) {
		ge_blend_enable(geinfo, 1);
		ge_set_src_blend(geinfo, d->src_blend_func);
		ge_set_dst_blend(geinfo, d->dst_blend_func);
	} else
		ge_blend_enable(geinfo, 0);

	/* alpha bitblt */
	if (o && o->pixelformat != d->pixelformat) {
		ge_set_source(geinfo, vpu);
		ge_set_destination(geinfo, d);
		ge_set_direct_write(geinfo, o);

		if (GSPF_YUV(o->pixelformat) == 0) {
			if (GSPF_YUV(s->pixelformat) == 0) {
				/* RGB-to-RGB */
				regs->src_csc_en = 0;
				ge_set_src_csc(geinfo, GE_CSC_RGB_BYPASS);
			} else {
				/* YC-to-RGB */
				regs->src_csc_en = 1;
				ge_set_src_csc(geinfo, GE_CSC_YC_RGB_JFIF_0_255);
			}
			if (GSPF_YUV(d->pixelformat) == 0) {
				/* RGB-to-RGB */
				regs->des_csc_en = 0;
				ge_set_dst_csc(geinfo, GE_CSC_RGB_BYPASS);
			} else {
				/* YC-to-RGB */
				regs->des_csc_en = 1;
				ge_set_dst_csc(geinfo, GE_CSC_YC_RGB_JFIF_0_255);
			}
		} else {
			if (GSPF_YUV(s->pixelformat) != 0) {
				/* YC-to-YC */
				regs->src_csc_en = 0;
				ge_set_src_csc(geinfo, GE_CSC_YC_BYPASS);
			} else {
				/* RGB-to-YC */
				regs->src_csc_en = 1;
				ge_set_src_csc(geinfo, GE_CSC_RGB_YC_JFIF_0_255);
			}
			if (GSPF_YUV(d->pixelformat) != 0) {
				/* YC-to-YC */
				regs->des_csc_en = 0;
				ge_set_dst_csc(geinfo, GE_CSC_YC_BYPASS);
			} else {
				/* RGB-to-YC */
				regs->des_csc_en = 1;
				ge_set_dst_csc(geinfo, GE_CSC_RGB_YC_JFIF_0_255);
			}
		}

		ge_set_command(geinfo, GECMD_ALPHA_BITBLT, 0xcc);
		ge_wait_sync(geinfo);
	} else {
		if (GSPF_YUV(s->pixelformat)) {
			if (GSPF_YUV(d->pixelformat))
				ge_yc_to_yc(geinfo, vpu, d, o);
			else
				ge_yc_to_rgb(geinfo, vpu, d, o);
		} else {
			if (GSPF_YUV(d->pixelformat))
				ge_rgb_to_yc(geinfo, vpu, d, o);
			else
				ge_rgb_to_rgb(geinfo, vpu, d, o);
		}
	}

	ge_set_vpu(geinfo, 0, 0, 0);

	if (vpu)
		ge_release_surface(geinfo, vpu);
	if (s)
		ge_release_surface(geinfo, s);
	if (d)
		ge_release_surface(geinfo, d);
	if (o)
		ge_release_surface(geinfo, o);

	return 0;
}

extern ge_info_t *ge_get_geinfo(void);

int ge_do_alpha_bitblt(vdo_framebuf_t *src,
		       vdo_framebuf_t *dst,
		       vdo_framebuf_t *out)
{
	ge_info_t *geinfo;
	ge_surface_t s, d, o;

	/*
	vpp_show_framebuf("src", src);
	vpp_show_framebuf("dst", dst);
	vpp_show_framebuf("out", out);
	*/

	s.addr = d.addr = o.addr = 0;

	geinfo = ge_get_geinfo();

	if (src)
		ge_helper_set_vdo_framebuf(src, &s, NULL);
	if (dst)
		ge_helper_set_vdo_framebuf(dst, &d, NULL);
	if (out)
		ge_helper_set_vdo_framebuf(out, &o, NULL);

	if (!src || !s.addr)
		return -1;

	ge_lock(geinfo);
	if (d.addr && o.addr)
		ge_stretch_blit_vpu(geinfo, &s, &d, &o, NULL, NULL);
	else if (d.addr)
		ge_stretch_blit_vpu(geinfo, &s, &d, NULL, NULL, NULL);
	else if (o.addr)
		ge_stretch_blit_vpu(geinfo, &s, &o, NULL, NULL, NULL);
	ge_unlock(geinfo);
	return 0;
}

