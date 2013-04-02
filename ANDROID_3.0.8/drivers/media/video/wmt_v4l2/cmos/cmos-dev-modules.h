/*++ 
 * linux/drivers/media/video/wmt_v4l2/cmos/cmos-dev-gc.h
 * WonderMedia v4l cmos device driver
 *
 * Copyright c 2010  WonderMedia  Technologies, Inc.
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

#ifndef CMOS_DEV_MODULES_H
/* To assert that only one occurrence is included */
#define CMOS_DEV_MODULES_H
/*-------------------- MODULE DEPENDENCY -------------------------------------*/

/*-------------------- EXPORTED PRIVATE CONSTANTS ----------------------------*/


/*-------------------- EXPORTED PRIVATE TYPES---------------------------------*/
/* typedef  void  viaapi_xxx_t;  *//*Example*/

/*------------------------------------------------------------------------------
    Definitions of structures
------------------------------------------------------------------------------*/

/*-------------------- EXPORTED PRIVATE VARIABLES -----------------------------*/
#ifdef CMOS_DEV_MODULES_C 
    #define EXTERN
#else
    #define EXTERN   extern
#endif 

#undef EXTERN

/*--------------------- EXPORTED PRIVATE MACROS -------------------------------*/

/*--------------------- EXPORTED PRIVATE FUNCTIONS  ---------------------------*/
/* extern void  viaapi_xxxx(vdp_Void); *//*Example*/

typedef enum{
	CMOS_ID_NONE=-1,
	CMOS_ID_GC_0307,
	CMOS_ID_GC_0308,
	CMOS_ID_GC_0309,
	CMOS_ID_GC_0311,
	CMOS_ID_GC_0329,
	CMOS_ID_GC_2015,
	CMOS_ID_GC_2035,
	CMOS_ID_SP_0838,
	CMOS_ID_SP_0A18,
	CMOS_ID_OV_7675,
	CMOS_ID_OV_2640,
	CMOS_ID_OV_2643,
	CMOS_ID_OV_2659,
	CMOS_ID_OV_3640,
	CMOS_ID_OV_3660,
	CMOS_ID_OV_5640,
	CMOS_ID_BG_0328,
	CMOS_ID_HI_704,
	CMOS_ID_YACD_511,
	CMOS_ID_NT_99250,
	CMOS_ID_SIV_121D,
	CMOS_ID_SID_130B,
	CMOS_ID_SAMSUNG_S5K5CA,
	CMOS_ID_NUM,
}wmt_cmos_product_id;

typedef struct {
	int width;
	int height;
	int cmos_v_flip;
	int cmos_h_flip;
	int mode_switch;
} cmos_init_arg_t;

int cmos_init_ext_device(cmos_init_arg_t  *init_arg, wmt_cmos_product_id cmosid);
int cmos_exit_ext_device(wmt_cmos_product_id cmosid);
 
int updateSceneMode(wmt_cmos_product_id cmosid,int value);
int updateWhiteBalance(wmt_cmos_product_id cmosid,int value);
int updateAntiBanding(wmt_cmos_product_id cmosid,int value);
#endif /* ifndef CMOS_DEV_MODULES_H */

/*=== END cmos-dev-gc.h ==========================================================*/
