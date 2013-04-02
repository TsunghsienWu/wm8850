/*++ 
 * WonderMedia core driver for VT1603/VT1609
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

#ifndef __MFD_VT1603_CORE_H__
#define __MFD_VT1603_CORE_H__

enum vt1603_type {
	VT1603 = 0,
	VT1609 = 1,
};

struct vt1603 {
	int (*reg_read)(struct vt1603 *vt1603, u8 reg, u8 *val);
	int (*reg_write)(struct vt1603 *vt1603, u8 reg, u8 val);
	void *control_data;
	int type;
	struct device *dev;
};

/* Device I/O API */
int vt1603_reg_write(struct vt1603 *vt1603, u8 reg, u8 val);
int vt1603_reg_read(struct vt1603 *vt1603, u8 reg, u8 *val);
void vt1603_regs_dump(struct vt1603 *vt1603);

#endif
