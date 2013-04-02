/*++ 
 * linux/drivers/video/wmt/govm.c
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

#define GOVM_C
// #define DEBUG

#include "govm.h"
 
#ifdef WMT_FTBLK_GOVM
void govm_reg_dump(void)
{
	DPRINT("========== GOVM register dump ==========\n");
	vpp_reg_dump(REG_GOVM_BEGIN,REG_GOVM_END-REG_GOVM_BEGIN);

	DPRINT("GE enable %d,VPU enable %d\n",vppif_reg32_read(GOVM_GE_SOURCE),vppif_reg32_read(GOVM_VPU_SOURCE));
	DPRINT("Display coordinate (%d,%d)\n",vppif_reg32_read(GOVM_DISP_X_CR),vppif_reg32_read(GOVM_DISP_Y_CR));
	DPRINT("VPU coordinate (%d,%d) - (%d,%d)\n",vppif_reg32_read(GOVM_VPU_X_STA_CR),vppif_reg32_read(GOVM_VPU_Y_STA_CR),
		vppif_reg32_read(GOVM_VPU_X_END_CR),vppif_reg32_read(GOVM_VPU_Y_END_CR));
	DPRINT("alpha enable %d,mode %d,A %d,B %d\n",vppif_reg32_read(GOVM_IND_MODE_ENABLE),vppif_reg32_read(GOVM_IND_MODE),
		vppif_reg32_read(GOVM_IND_ALPHA_A),vppif_reg32_read(GOVM_IND_ALPHA_B));
	DPRINT("color bar enable %d,mode %d,inv %d\n",vppif_reg32_read(GOVM_COLBAR_ENABLE),
		vppif_reg32_read(GOVM_COLBAR_MODE),vppif_reg32_read(GOVM_COLBAR_INVERSION));
	DPRINT("INT enable GOV %d\n",vppif_reg32_read(GOVM_INT_ENABLE));
	DPRINT("Gamma mode %d, clamp enable %d\n",vppif_reg32_read(GOVM_GAMMA_MODE),vppif_reg32_read(GOVM_CLAMPING_ENABLE));
}

void govm_set_in_path(vpp_path_t in_path, vpp_flag_t enable)
{
//#ifdef WMT_FTBLK_SCL
	if (VPP_PATH_GOVM_IN_VPU & in_path) {
#ifdef CONFIG_HW_GOVW_TG_ENABLE
		if( enable ){
			vppif_reg32_write(VPU_SCALAR_ENABLE,VPP_FLAG_ENABLE);
			vpp_vpu_sw_reset();
		}
		vppif_reg32_write(GOVM_VPU_SOURCE, enable);
		if( !enable ){
			vppif_reg32_write(VPU_SCALAR_ENABLE,VPP_FLAG_DISABLE);
		}
		vpu_int_set_enable(enable, VPP_INT_ERR_VPUW_MIFY + VPP_INT_ERR_VPUW_MIFC);
#else
		vppif_reg32_write(GOVM_VPU_SOURCE, enable);
#endif
	}
//#endif
#ifdef WMT_FTBLK_GE
	if (VPP_PATH_GOVM_IN_GE & in_path) {
		vppif_reg32_write(GOVM_GE_SOURCE, enable);
	}
#endif
}

vpp_path_t govm_get_in_path(void)
{
	vpp_path_t ret;

	ret = 0;

//#ifdef WMT_FTBLK_SCL
	if (vppif_reg32_read(GOVM_VPU_SOURCE)){
		ret |= VPP_PATH_GOVM_IN_VPU;
	}
//#endif
#ifdef WMT_FTBLK_GE
	if (vppif_reg32_read(GOVM_GE_SOURCE)){
		ret |= VPP_PATH_GOVM_IN_GE;
	}
#endif
	return ret;
}

#ifdef GOVM_TV_MODE
vpp_flag_t govm_set_output_device(vpp_output_device_t device)
{
	switch (device) {
	case VPP_OUTDEV_TV_NORMAL:
		vppif_reg32_write(GOVM_TV_MODE, 0x0);
		break;
	default:
		return VPP_FLAG_ERROR;
	}
	return VPP_FLAG_SUCCESS;
}
#endif

void govm_set_blanking_enable(vpp_flag_t enable)
{
	vppif_reg32_write(GOVM_BLUE_SCREEN_ENABLE,enable);
}

void govm_set_blanking(U32 color)
{
	vppif_reg32_out(REG_GOVM_BNK,color);
}

void govm_set_disp_coordinate(U32 x, U32 y)
{
	vppif_reg32_write(GOVM_DISP_X_CR,x-1);
	vppif_reg32_write(GOVM_DISP_Y_CR,y-1);

	// modify GE coordinate
	REG32_VAL(GE3_BASE_ADDR+0x50) = x-1;
	REG32_VAL(GE3_BASE_ADDR+0x54) = y-1;
	REG32_VAL(GE3_BASE_ADDR+0x34) = x-1;
	REG32_VAL(GE3_BASE_ADDR+0x3c) = y-1;
}

void govm_get_disp_coordinate(U32 * p_x, U32 * p_y)
{
	*p_x = vppif_reg32_read(GOVM_DISP_X_CR);
	*p_y = vppif_reg32_read(GOVM_DISP_Y_CR);
}

void govm_set_vpu_coordinate(U32 x_s, U32 y_s, U32 x_e, U32 y_e)
{
	vppif_reg32_write(GOVM_VPU_X_STA_CR,x_s);
	vppif_reg32_write(GOVM_VPU_X_END_CR,x_e);
	vppif_reg32_write(GOVM_VPU_Y_STA_CR,y_s);
	vppif_reg32_write(GOVM_VPU_Y_END_CR,y_e);
}

void govm_set_vpu_position(U32 x1, U32 y1)
{
	U32 x2, y2;

	x2 = x1 + vppif_reg32_read(GOVM_VPU_X_END_CR) - vppif_reg32_read(GOVM_VPU_X_STA_CR);
	y2 = y1 + vppif_reg32_read(GOVM_VPU_Y_END_CR) - vppif_reg32_read(GOVM_VPU_Y_STA_CR);
	govm_set_vpu_coordinate(x1, y1, x2, y2);
}

void govm_set_alpha_mode(vpp_flag_t enable,vpp_alpha_t mode,int A,int B)
{
	vppif_reg32_write(GOVM_IND_MODE_ENABLE, enable);
	
	// mode A : (VPU)*A + (1-A)*Blanking
	// mode B : (VPU)*A + (GE)*B
	vppif_reg32_write(GOVM_IND_MODE,mode);
	vppif_reg32_write(GOVM_IND_ALPHA_A, A);
	vppif_reg32_write(GOVM_IND_ALPHA_B, B);
}

void govm_set_colorbar(vpp_flag_t enable,int width, int inverse)
{
	vppif_reg32_write(GOVM_COLBAR_ENABLE, enable);
	vppif_reg32_write(GOVM_COLBAR_MODE, width);
	vppif_reg32_write(GOVM_COLBAR_INVERSION, inverse);
}

void govm_set_int_enable(vpp_flag_t enable)
{
	vppif_reg32_write(GOVM_INT_ENABLE, enable);
}

void govm_set_reg_update(vpp_flag_t enable)
{
	vppif_reg32_write(GOVM_REG_UPDATE, enable);
}

void govm_set_reg_level(vpp_reglevel_t level)
{
	switch (level) {
	case VPP_REG_LEVEL_1:
		/*
		 * don't use the VPP_REG_LEVEL_1 directly, like 'vppif_reg32_write(GOVM_REG_LEVEL, VPP_REG_LEVEL_1)';
		 * because in others module, LEVEL 1 equal '0' maybe.
		 */
		vppif_reg32_write(GOVM_REG_LEVEL, 0x00);
		break;
	case VPP_REG_LEVEL_2:
		vppif_reg32_write(GOVM_REG_LEVEL, 0x01);
		break;
	default:
		DPRINT("*E* check the parameter.\n");
		return;
	}
	return;
}

void govm_set_gamma_mode(int mode)
{
	vppif_reg32_write(GOVM_GAMMA_MODE,mode);
}

void govm_set_clamping_enable(vpp_flag_t enable)
{
	vppif_reg32_write(GOVM_CLAMPING_ENABLE, enable);
}

/*----------------------- GOVM INTERRUPT --------------------------------------*/
void govm_int_set_enable(vpp_flag_t enable, vpp_int_err_t int_bit)
{
}

vpp_int_err_t govm_get_int_status(void)
{
	vpp_int_err_t int_sts;

	int_sts = 0;
	if (vppif_reg32_read(GOVM_INTSTS_GOVM_STATUS)){
		if( vppif_reg32_read(GOVM_INTSTS_VPU_READY) == 0 ){
			int_sts |= VPP_INT_ERR_GOVM_VPU;
		}
		
		if( vppif_reg32_read(GOVM_INTSTS_GE_READY) == 0 ){
			int_sts |= VPP_INT_ERR_GOVM_GE;
		}
		vppif_reg8_out(REG_GOVM_INT+0x0,BIT0);
	}

	return int_sts;
}

void govm_clean_int_status(vpp_int_err_t int_sts)
{
	if( (int_sts & VPP_INT_ERR_GOVM_VPU) || (int_sts & VPP_INT_ERR_GOVM_GE) ){
		vppif_reg8_out(REG_GOVM_INT+0x0,BIT0);
	}
}

#ifdef CONFIG_PM
void govm_suspend(int sts)
{
	switch( sts ){
		case 0:	// disable module
			vpp_mod_set_clock(VPP_MOD_GOVW,VPP_FLAG_ENABLE,1);
			break;
		case 1: // disable tg
			break;
		case 2:	// backup register
			p_govm->reg_bk = vpp_backup_reg(REG_GOVM_BEGIN,(REG_GOVM_END-REG_GOVM_BEGIN));
			break;
		default:
			break;
	}
}

void govm_resume(int sts)
{
	switch( sts ){
		case 0:	// restore register
			vpp_restore_reg(REG_GOVM_BEGIN,(REG_GOVM_END-REG_GOVM_BEGIN),p_govm->reg_bk);
			p_govm->reg_bk = 0;			
			break;
		case 1:	// enable module
			break;
		case 2: // enable tg
			vpp_mod_set_clock(VPP_MOD_GOVW,VPP_FLAG_DISABLE,1);
			break;
		default:
			break;
	}
}
#else
#define govm_suspend NULL
#define govm_resume NULL
#endif

/*----------------------- GOVM INIT --------------------------------------*/
void govm_init(void *base)
{
	govm_mod_t *mod_p;

	mod_p = (govm_mod_t *) base;

	govm_set_colorbar(VPP_FLAG_DISABLE,0,0);
	govm_set_int_enable(VPP_FLAG_DISABLE);
	govm_set_blanking(VPP_COL_BLACK);
	govm_set_in_path(mod_p->path, VPP_FLAG_ENABLE);
	govm_set_vpu_coordinate(0,0,VPP_HD_DISP_RESX,VPP_HD_DISP_RESY);
	govm_set_disp_coordinate(VPP_HD_MAX_RESX,VPP_HD_MAX_RESY);
	if ((VPP_INT_ERR_GOVM_VPU & mod_p->int_catch) || (VPP_INT_ERR_GOVM_GE & mod_p->int_catch)) {
		govm_set_int_enable(VPP_FLAG_ENABLE);
	} else {
		govm_set_int_enable(VPP_FLAG_DISABLE);
	}
	govm_set_clamping_enable(VPP_FLAG_DISABLE);
	govm_set_reg_update(VPP_FLAG_ENABLE);
}

int govm_mod_init(void)
{
	govm_mod_t *mod_p;

	mod_p = (govm_mod_t *) vpp_mod_register(VPP_MOD_GOVM,sizeof(govm_mod_t),0);
	if( !mod_p ){
		DPRINT("*E* GOVM module register fail\n");
		return -1;
	}

	/* module member variable */
	mod_p->path = VPP_PATH_NULL;
	mod_p->int_catch = VPP_INT_NULL;

	/* module member function */
	mod_p->init = govm_init;
	mod_p->dump_reg = govm_reg_dump;
	mod_p->set_colorbar = govm_set_colorbar;
	mod_p->get_sts = govm_get_int_status;
	mod_p->clr_sts = govm_clean_int_status;
	mod_p->suspend = govm_suspend;
	mod_p->resume = govm_resume;

	p_govm = mod_p;

	return 0;
}
module_init(govm_mod_init);
#endif				//WMT_FTBLK_GOVM
