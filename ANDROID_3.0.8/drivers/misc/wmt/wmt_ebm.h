/*++
	Copyright (c) 2012  WonderMedia Technologies, Inc.

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

#ifndef __WMT_EBM_H__
#define __WMT_EBM_H__

/*EBMC 0x0 ~ 0x8*/
struct ebm_dev_reg_s {
	unsigned int EbmcControl;    /* EBM bus protocol control - 0*/
	unsigned int EbmcOnOff;      /* EBMC on/off control - 4*/
	unsigned int EbmBaseAddr;    /* EBM system base address - 8*/
};

enum ebm_callback_func_e {
	MDM_RDY,
	EBM_WAKEREQ,
	MDM_WAKE_AP
};

void wmt_ebm_register_callback(enum ebm_callback_func_e callback_num,
				void (*callback)(void *data), void *callback_data);
void ap_wake_mdm(unsigned int wake);
void mdm_reset(unsigned int reset);
unsigned int is_ebm_enable(void);
unsigned int get_firmware_baseaddr(void);
unsigned int get_firmware_len(void);
#endif
