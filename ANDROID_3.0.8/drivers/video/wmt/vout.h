/*++ 
 * linux/drivers/video/wmt/vout.h
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

#ifndef VOUT_H
/* To assert that only one occurrence is included */
#define VOUT_H
/*-------------------- MODULE DEPENDENCY -------------------------------------*/
#include "vpp.h"
#include "sw_i2c.h"
#include "edid.h"

/*	following is the C++ header	*/
#ifdef	__cplusplus
extern	"C" {
#endif

/*-------------------- EXPORTED PRIVATE CONSTANTS ----------------------------*/
/* #define  VO_XXXX  1    *//*Example*/
// #define CONFIG_VOUT_EDID_ALLOC
#define CONFIG_VOUT_REFACTORY

#define VOUT_INFO_DEFAULT_RESX	1024
#define VOUT_INFO_DEFAULT_RESY	768
#define VOUT_INFO_DEFAULT_FPS	60

/*-------------------- EXPORTED PRIVATE TYPES---------------------------------*/
/* typedef  void  vo_xxx_t;  *//*Example*/
typedef enum {
	VOUT_SD_ANALOG,
	VOUT_SD_DIGITAL,
	VOUT_LCD,
	VOUT_DVI,
	VOUT_HDMI,
	VOUT_DVO2HDMI,
	VOUT_LVDS,
	VOUT_VGA,
	VOUT_BOOT,
	VOUT_MODE_MAX,
	VOUT_MODE_ALL = VOUT_MODE_MAX
} vout_mode_t;

typedef enum {
	VOUT_DEV_VGA,
	VOUT_DEV_DVI,
	VOUT_DEV_LCD,
	VOUT_DEV_HDMI,
	VOUT_DEV_SDD,
	VOUT_DEV_LVDS,
	VOUT_DEV_MODE_MAX
} vout_dev_mode_t;

typedef enum {
	VOUT_INF_DVI,
	VOUT_INF_HDMI,
	VOUT_INF_LVDS,
	VOUT_INF_MODE_MAX
} vout_inf_mode_t;

typedef struct {
	int num;
	unsigned int vo_mask;	// vo bit mask for multi vout
	int govr_mod;			// govr module type
	govrh_mod_t *govr;		// govr pointer

	int resx;
	int resy;
	int resx_virtual;
	int resy_virtual;
	int bpp;
	int fps;
	unsigned int pixclk;
	unsigned int pico;		// keep fb pico for no config in same resolution
	unsigned int option;
	int force_config;

	vpp_timing_t *fixed_timing;
	vdo_framebuf_t fb;
#ifdef CONFIG_VPP_OVERSCAN
	int fb_xres;
	int fb_yres;
	int fb_xoffset;
	int fb_yoffset;
	int fb_clr;
#endif
} vout_info_t;

typedef struct {
	int fmt;			// sample bits
	int sample_rate;	// sample rate
	int channel;		// channel count
} vout_audio_t;

struct vout_s;
struct vout_inf_s;

#define VOUT_DEV_CAP_FIX_RES		0x1
#define VOUT_DEV_CAP_EDID			0x2
#define VOUT_DEV_CAP_AUDIO			0x4

typedef struct vout_dev_s {
	struct vout_dev_s *next;
	char name[10];
	vout_inf_mode_t mode;
	struct vout_s *vout;
	unsigned int capability;
	
	int (*init)(struct vout_s *vo);
	void (*set_power_down)(int enable);
	int (*set_mode)(unsigned int *option);
	int (*config)(vout_info_t *info);
	int (*check_plugin)(int hotplug);
	int (*get_edid)(char *buf);
	int (*set_audio)(vout_audio_t *arg);
	int (*interrupt)(void);
	void (*poll)(void);
} vout_dev_t;

typedef enum {
	VOUT_BLANK_UNBLANK,			/* screen: unblanked, hsync: on,  vsync: on */
	VOUT_BLANK_NORMAL,			/* screen: blanked,   hsync: on,  vsync: on */
	VOUT_BLANK_VSYNC_SUSPEND,	/* screen: blanked,   hsync: on,  vsync: off */
	VOUT_BLANK_HSYNC_SUSPEND,	/* screen: blanked,   hsync: off, vsync: on */
	VOUT_BLANK_POWERDOWN		/* screen: blanked,   hsync: off, vsync: off */
} vout_blank_t;

#define VOUT_CAP_INTERFACE	0x000000FF
#define VOUT_CAP_BUS		0x00000F00
#define VOUT_CAP_GOVR		0x0000F000
#define VOUT_CAP_EXT_DEV	0x00010000
#define VOUT_CAP_FIX_PLUG	0x00020000
#define VOUT_CAP_AUDIO		0x00040000
#define VOUT_CAP_EDID		0x00080000

typedef struct vout_s {
	int num;
	unsigned int fix_cap;
	unsigned int var_cap;
	struct vout_inf_s *inf;		// interface ops
	vout_dev_t *dev;			// device ops
	vout_dev_t *ext_dev;
	govrh_mod_t *govr;
	int resx;
	int resy;
	int pixclk;
	unsigned int status;
#ifdef CONFIG_VOUT_EDID_ALLOC
	char *edid;
#else
	char edid[128*EDID_BLOCK_MAX];
#endif
	edid_info_t edid_info;
	unsigned int option[3];
	vout_blank_t pre_blank;
} vout_t;

typedef struct vout_inf_s {
	vout_inf_mode_t mode;

	// function	
	int (*init)(vout_t *vo,int arg);
	int (*uninit)(vout_t *vo,int arg);
	int (*blank)(vout_t *vo,vout_blank_t arg);
	int (*config)(vout_t *vo,int arg);
	int (*chkplug)(vout_t *vo,int arg);
	int (*get_edid)(vout_t *vo,int arg);
//	int (*ioctl)(vout_t *vo,int arg);
} vout_inf_t;

/*-------------------- EXPORTED PRIVATE VARIABLES -----------------------------*/
#ifdef VOUT_C /* allocate memory for variables only in vout.c */
#define EXTERN

const char *vout_inf_str[] = {"DVI","HDMI","LVDS","VGA","SDA","SDD"};
const char *vout_adpt_str[] = {"SD_DIGITAL","SD_DIGITAL","LCD","DVI","HDMI","DVO2HDMI","LVDS","VGA","BOOT"};

#else
#define EXTERN   extern

extern const char *vout_inf_str[];
extern const char *vout_adpt_str[];

#endif /* ifdef VOUT_C */

EXTERN vout_info_t vout_info[VPP_VOUT_INFO_NUM];

/* EXTERN int      vo_xxx; *//*Example*/
EXTERN int (*vout_board_info)(int arg);

#undef EXTERN

/*--------------------- EXPORTED PRIVATE MACROS -------------------------------*/
/* #define VO_XXX_YYY   xxxx *//*Example*/
/*--------------------- EXPORTED PRIVATE FUNCTIONS  ---------------------------*/
/* extern void  vo_xxx(void); *//*Example*/

void vout_register(int no,vout_t *vo);
vout_t *vout_get_entry(int no);
vout_info_t *vout_get_info_entry(int no);
void vout_change_status(vout_t *vo,int mask,int sts);
int vout_query_inf_support(int no,vout_inf_mode_t mode);

int vout_inf_register(vout_inf_mode_t mode,vout_inf_t *inf);
vout_inf_t *vout_inf_get_entry(vout_inf_mode_t mode);

int vout_device_register(vout_dev_t *ops);
vout_dev_t *vout_get_device(vout_dev_t *ops);

vout_t *vout_get_entry_adapter(vout_mode_t mode);
vout_inf_t *vout_get_inf_entry_adapter(vout_mode_t mode);
int vout_info_add_entry(vout_t *vo);
vout_info_t *vout_info_get_entry(int no);
void vout_info_set_fixed_timing(int no,vpp_timing_t *timing);

int vout_config(unsigned int mask,vout_info_t *info);
int vout_set_mode(int no,vout_inf_mode_t mode);
int vout_set_blank(unsigned int mask,vout_blank_t blank);
void vout_set_tg_enable(unsigned int mask,int enable);
void vout_set_framebuffer(unsigned int mask,vdo_framebuf_t *fb);
int vout_chkplug(int no);
void vout_set_int_type(int type);
char *vout_get_edid(int no);
int vout_get_edid_option(int no);

int vout_init(void);
int vout_exit(void);
void vout_plug_detect(int no);
int vo_i2c_proc(int id,unsigned int addr,unsigned int index,char *pdata,int len);
int vout_set_audio(vout_audio_t *arg);
int vout_find_edid_support_mode(edid_info_t *info,unsigned int *resx,unsigned int *resy,unsigned int *fps,int r_16_9);
int vout_check_ratio_16_9(unsigned int resx,unsigned int resy);
int vout_check_info(unsigned int mask,vout_info_t *info);
unsigned int vout_get_mask(	vout_info_t *vo_info);

#ifdef	__cplusplus
}
#endif	

#endif /* ifndef VOUT_H */

/*=== END vout.h ==========================================================*/
