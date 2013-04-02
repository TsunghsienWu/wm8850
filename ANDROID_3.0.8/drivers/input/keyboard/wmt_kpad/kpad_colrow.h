/*++
Copyright (c) 2011-2013  WonderMedia Technologies, Inc. All Rights Reserved.

This PROPRIETARY SOFTWARE is the property of WonderMedia
Technologies, Inc. and may contain trade secrets and/or other confidential
information of WonderMedia Technologies, Inc. This file shall not be
disclosed to any third party, in whole or in part, without prior written consent of
WonderMedia.  

THIS PROPRIETARY SOFTWARE AND ANY RELATED
DOCUMENTATION ARE PROVIDED AS IS, WITH ALL FAULTS, AND
WITHOUT WARRANTY OF ANY KIND EITHER EXPRESS OR IMPLIED,
AND WonderMedia TECHNOLOGIES, INC. DISCLAIMS ALL EXPRESS
OR IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE, QUIET ENJOYMENT OR
NON-INFRINGEMENT.  
--*/

#ifndef WMT_KPAD_COLROW_201101051516
#define WMT_KPAD_COLROW_201101051516
#include <linux/ioport.h>
#include "wmt_keypadall.h"


/////////////////////////////////////////////////

//irq type
#define WMT_KPADROWCOL_ZERO           0
#define WMT_KPADROWCOL_ONE            1
#define WMT_KPADROWCOL_FALLING        2
#define WMT_KPADROWCOL_RISING         3
#define WMT_KPADROWCOL_FALLING_RISING 4


///////////////////////////////////////////////
extern int kpad_colrow_setkey(char pintype, int pinnum, int key, int index);
extern unsigned int* kpad_colrow_getkeytbl(void);
extern int kpad_colrow_register(struct keypadall_device* kpad_dev);
extern int early_suspend();
extern int late_resume();

#endif
