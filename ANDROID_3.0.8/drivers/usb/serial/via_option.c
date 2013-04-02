/*
  USB Driver for GSM modems

  Copyright (C) 2005  Matthias Urlichs <smurf@smurf.noris.de>

  This driver is free software; you can redistribute it and/or modify
  it under the terms of Version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  Portions copied from the Keyspan driver by Hugh Blemings <hugh@blemings.org>

  History: see the git log.

  Work sponsored by: Sigos GmbH, Germany <info@sigos.de>

  This driver exists because the "normal" serial driver doesn't work too well
  with GSM modems. Issues:
  - data loss -- one single Receive URB is not nearly enough
  - nonstandard flow (Option devices) control
  - controlling the baud rate doesn't make sense

  This driver is named "option" because the most common device it's
  used for is a PC-Card (with an internal OHCI-USB interface, behind
  which the GSM interface sits), made by Option Inc.

  Some of the "one port" devices actually exhibit multiple USB instances
  on the USB bus. This is not a bug, these ports are used for different
  device features.
*/

#define DRIVER_VERSION "v0.7.2"
#define DRIVER_AUTHOR "Matthias Urlichs <smurf@smurf.noris.de>"
#define DRIVER_DESC "USB Driver for GSM modems"

#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/errno.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/bitops.h>
#include <linux/usb.h>
#include <linux/usb/serial.h>
#include <linux/usb/rawbulk.h>
#include <linux/suspend.h>
#include "via_usb-wwan.h"
#include "via_usb_wwan.c"

#if defined(CONFIG_VIATELECOM_SYNC_CBP)
#include <mach/viatel.h>
//#include "../drivers/usb/core/usb.h"
#endif

/* Function prototypes */
static int  option_probe(struct usb_serial *serial,
			const struct usb_device_id *id);
static int option_send_setup(struct usb_serial_port *port);
static int viatelecom_send_setup(struct usb_serial_port *port);
static void option_instat_callback(struct urb *urb);

struct wake_lock ets_wake_lock;

/* Vendor and product IDs */
#define VIATELECOM_VENDOR_ID        0x15EB
#if defined(CONFIG_JZ4770_TIANTAI_PLUS)
#define VIATELECOM_PRODUCT_ID       0x0004
#else
#define VIATELECOM_PRODUCT_ID       0x0001
#endif

#define OPTION_VENDOR_ID			0x0AF0
#define OPTION_PRODUCT_COLT			0x5000
#define OPTION_PRODUCT_RICOLA			0x6000
#define OPTION_PRODUCT_RICOLA_LIGHT		0x6100
#define OPTION_PRODUCT_RICOLA_QUAD		0x6200
#define OPTION_PRODUCT_RICOLA_QUAD_LIGHT	0x6300
#define OPTION_PRODUCT_RICOLA_NDIS		0x6050
#define OPTION_PRODUCT_RICOLA_NDIS_LIGHT	0x6150
#define OPTION_PRODUCT_RICOLA_NDIS_QUAD		0x6250
#define OPTION_PRODUCT_RICOLA_NDIS_QUAD_LIGHT	0x6350
#define OPTION_PRODUCT_COBRA			0x6500
#define OPTION_PRODUCT_COBRA_BUS		0x6501
#define OPTION_PRODUCT_VIPER			0x6600
#define OPTION_PRODUCT_VIPER_BUS		0x6601
#define OPTION_PRODUCT_GT_MAX_READY		0x6701
#define OPTION_PRODUCT_FUJI_MODEM_LIGHT		0x6721
#define OPTION_PRODUCT_FUJI_MODEM_GT		0x6741
#define OPTION_PRODUCT_FUJI_MODEM_EX		0x6761
#define OPTION_PRODUCT_KOI_MODEM		0x6800
#define OPTION_PRODUCT_SCORPION_MODEM		0x6901
#define OPTION_PRODUCT_ETNA_MODEM		0x7001
#define OPTION_PRODUCT_ETNA_MODEM_LITE		0x7021
#define OPTION_PRODUCT_ETNA_MODEM_GT		0x7041
#define OPTION_PRODUCT_ETNA_MODEM_EX		0x7061
#define OPTION_PRODUCT_ETNA_KOI_MODEM		0x7100
#define OPTION_PRODUCT_GTM380_MODEM		0x7201

#define HUAWEI_VENDOR_ID			0x12D1
#define HUAWEI_PRODUCT_E600			0x1001
#define HUAWEI_PRODUCT_E220			0x1003
#define HUAWEI_PRODUCT_E220BIS			0x1004
#define HUAWEI_PRODUCT_E1401			0x1401
#define HUAWEI_PRODUCT_E1402			0x1402
#define HUAWEI_PRODUCT_E1403			0x1403
#define HUAWEI_PRODUCT_E1404			0x1404
#define HUAWEI_PRODUCT_E1405			0x1405
#define HUAWEI_PRODUCT_E1406			0x1406
#define HUAWEI_PRODUCT_E1407			0x1407
#define HUAWEI_PRODUCT_E1408			0x1408
#define HUAWEI_PRODUCT_E1409			0x1409
#define HUAWEI_PRODUCT_E140A			0x140A
#define HUAWEI_PRODUCT_E140B			0x140B
#define HUAWEI_PRODUCT_E140C			0x140C
#define HUAWEI_PRODUCT_E140D			0x140D
#define HUAWEI_PRODUCT_E140E			0x140E
#define HUAWEI_PRODUCT_E140F			0x140F
#define HUAWEI_PRODUCT_E1410			0x1410
#define HUAWEI_PRODUCT_E1411			0x1411
#define HUAWEI_PRODUCT_E1412			0x1412
#define HUAWEI_PRODUCT_E1413			0x1413
#define HUAWEI_PRODUCT_E1414			0x1414
#define HUAWEI_PRODUCT_E1415			0x1415
#define HUAWEI_PRODUCT_E1416			0x1416
#define HUAWEI_PRODUCT_E1417			0x1417
#define HUAWEI_PRODUCT_E1418			0x1418
#define HUAWEI_PRODUCT_E1419			0x1419
#define HUAWEI_PRODUCT_E141A			0x141A
#define HUAWEI_PRODUCT_E141B			0x141B
#define HUAWEI_PRODUCT_E141C			0x141C
#define HUAWEI_PRODUCT_E141D			0x141D
#define HUAWEI_PRODUCT_E141E			0x141E
#define HUAWEI_PRODUCT_E141F			0x141F
#define HUAWEI_PRODUCT_E1420			0x1420
#define HUAWEI_PRODUCT_E1421			0x1421
#define HUAWEI_PRODUCT_E1422			0x1422
#define HUAWEI_PRODUCT_E1423			0x1423
#define HUAWEI_PRODUCT_E1424			0x1424
#define HUAWEI_PRODUCT_E1425			0x1425
#define HUAWEI_PRODUCT_E1426			0x1426
#define HUAWEI_PRODUCT_E1427			0x1427
#define HUAWEI_PRODUCT_E1428			0x1428
#define HUAWEI_PRODUCT_E1429			0x1429
#define HUAWEI_PRODUCT_E142A			0x142A
#define HUAWEI_PRODUCT_E142B			0x142B
#define HUAWEI_PRODUCT_E142C			0x142C
#define HUAWEI_PRODUCT_E142D			0x142D
#define HUAWEI_PRODUCT_E142E			0x142E
#define HUAWEI_PRODUCT_E142F			0x142F
#define HUAWEI_PRODUCT_E1430			0x1430
#define HUAWEI_PRODUCT_E1431			0x1431
#define HUAWEI_PRODUCT_E1432			0x1432
#define HUAWEI_PRODUCT_E1433			0x1433
#define HUAWEI_PRODUCT_E1434			0x1434
#define HUAWEI_PRODUCT_E1435			0x1435
#define HUAWEI_PRODUCT_E1436			0x1436
#define HUAWEI_PRODUCT_E1437			0x1437
#define HUAWEI_PRODUCT_E1438			0x1438
#define HUAWEI_PRODUCT_E1439			0x1439
#define HUAWEI_PRODUCT_E143A			0x143A
#define HUAWEI_PRODUCT_E143B			0x143B
#define HUAWEI_PRODUCT_E143C			0x143C
#define HUAWEI_PRODUCT_E143D			0x143D
#define HUAWEI_PRODUCT_E143E			0x143E
#define HUAWEI_PRODUCT_E143F			0x143F
#define HUAWEI_PRODUCT_K4505			0x1464
#define HUAWEI_PRODUCT_K3765			0x1465
#define HUAWEI_PRODUCT_E14AC			0x14AC
#define HUAWEI_PRODUCT_K3806			0x14AE
#define HUAWEI_PRODUCT_K4605			0x14C6
#define HUAWEI_PRODUCT_K3770			0x14C9
#define HUAWEI_PRODUCT_K3771			0x14CA
#define HUAWEI_PRODUCT_K4510			0x14CB
#define HUAWEI_PRODUCT_K4511			0x14CC
#define HUAWEI_PRODUCT_ETS1220			0x1803
#define HUAWEI_PRODUCT_E353			0x1506

#define QUANTA_VENDOR_ID			0x0408
#define QUANTA_PRODUCT_Q101			0xEA02
#define QUANTA_PRODUCT_Q111			0xEA03
#define QUANTA_PRODUCT_GLX			0xEA04
#define QUANTA_PRODUCT_GKE			0xEA05
#define QUANTA_PRODUCT_GLE			0xEA06

#define NOVATELWIRELESS_VENDOR_ID		0x1410

/* YISO PRODUCTS */

#define YISO_VENDOR_ID				0x0EAB
#define YISO_PRODUCT_U893			0xC893

/*
 * NOVATEL WIRELESS PRODUCTS
 *
 * Note from Novatel Wireless:
 * If your Novatel modem does not work on linux, don't
 * change the option module, but check our website. If
 * that does not help, contact ddeschepper@nvtl.com
*/
/* MERLIN EVDO PRODUCTS */
#define NOVATELWIRELESS_PRODUCT_V640		0x1100
#define NOVATELWIRELESS_PRODUCT_V620		0x1110
#define NOVATELWIRELESS_PRODUCT_V740		0x1120
#define NOVATELWIRELESS_PRODUCT_V720		0x1130

/* MERLIN HSDPA/HSPA PRODUCTS */
#define NOVATELWIRELESS_PRODUCT_U730		0x1400
#define NOVATELWIRELESS_PRODUCT_U740		0x1410
#define NOVATELWIRELESS_PRODUCT_U870		0x1420
#define NOVATELWIRELESS_PRODUCT_XU870		0x1430
#define NOVATELWIRELESS_PRODUCT_X950D		0x1450

/* EXPEDITE PRODUCTS */
#define NOVATELWIRELESS_PRODUCT_EV620		0x2100
#define NOVATELWIRELESS_PRODUCT_ES720		0x2110
#define NOVATELWIRELESS_PRODUCT_E725		0x2120
#define NOVATELWIRELESS_PRODUCT_ES620		0x2130
#define NOVATELWIRELESS_PRODUCT_EU730		0x2400
#define NOVATELWIRELESS_PRODUCT_EU740		0x2410
#define NOVATELWIRELESS_PRODUCT_EU870D		0x2420
/* OVATION PRODUCTS */
#define NOVATELWIRELESS_PRODUCT_MC727		0x4100
#define NOVATELWIRELESS_PRODUCT_MC950D		0x4400
/*
 * Note from Novatel Wireless:
 * All PID in the 5xxx range are currently reserved for
 * auto-install CDROMs, and should not be added to this
 * module.
 *
 * #define NOVATELWIRELESS_PRODUCT_U727		0x5010
 * #define NOVATELWIRELESS_PRODUCT_MC727_NEW	0x5100
*/
#define NOVATELWIRELESS_PRODUCT_OVMC760		0x6002
#define NOVATELWIRELESS_PRODUCT_MC780		0x6010
#define NOVATELWIRELESS_PRODUCT_EVDO_FULLSPEED	0x6000
#define NOVATELWIRELESS_PRODUCT_EVDO_HIGHSPEED	0x6001
#define NOVATELWIRELESS_PRODUCT_HSPA_FULLSPEED	0x7000
#define NOVATELWIRELESS_PRODUCT_HSPA_HIGHSPEED	0x7001
#define NOVATELWIRELESS_PRODUCT_HSPA_HIGHSPEED3	0x7003
#define NOVATELWIRELESS_PRODUCT_HSPA_HIGHSPEED4	0x7004
#define NOVATELWIRELESS_PRODUCT_HSPA_HIGHSPEED5	0x7005
#define NOVATELWIRELESS_PRODUCT_HSPA_HIGHSPEED6	0x7006
#define NOVATELWIRELESS_PRODUCT_HSPA_HIGHSPEED7	0x7007
#define NOVATELWIRELESS_PRODUCT_MC996D		0x7030
#define NOVATELWIRELESS_PRODUCT_MF3470		0x7041
#define NOVATELWIRELESS_PRODUCT_MC547		0x7042
#define NOVATELWIRELESS_PRODUCT_EVDO_EMBEDDED_FULLSPEED	0x8000
#define NOVATELWIRELESS_PRODUCT_EVDO_EMBEDDED_HIGHSPEED	0x8001
#define NOVATELWIRELESS_PRODUCT_HSPA_EMBEDDED_FULLSPEED	0x9000
#define NOVATELWIRELESS_PRODUCT_HSPA_EMBEDDED_HIGHSPEED	0x9001
#define NOVATELWIRELESS_PRODUCT_G1		0xA001
#define NOVATELWIRELESS_PRODUCT_G1_M		0xA002
#define NOVATELWIRELESS_PRODUCT_G2		0xA010

/* AMOI PRODUCTS */
#define AMOI_VENDOR_ID				0x1614
#define AMOI_PRODUCT_H01			0x0800
#define AMOI_PRODUCT_H01A			0x7002
#define AMOI_PRODUCT_H02			0x0802
#define AMOI_PRODUCT_SKYPEPHONE_S2		0x0407

#define DELL_VENDOR_ID				0x413C

/* Dell modems */
#define DELL_PRODUCT_5700_MINICARD		0x8114
#define DELL_PRODUCT_5500_MINICARD		0x8115
#define DELL_PRODUCT_5505_MINICARD		0x8116
#define DELL_PRODUCT_5700_EXPRESSCARD		0x8117
#define DELL_PRODUCT_5510_EXPRESSCARD		0x8118

#define DELL_PRODUCT_5700_MINICARD_SPRINT	0x8128
#define DELL_PRODUCT_5700_MINICARD_TELUS	0x8129

#define DELL_PRODUCT_5720_MINICARD_VZW		0x8133
#define DELL_PRODUCT_5720_MINICARD_SPRINT	0x8134
#define DELL_PRODUCT_5720_MINICARD_TELUS	0x8135
#define DELL_PRODUCT_5520_MINICARD_CINGULAR	0x8136
#define DELL_PRODUCT_5520_MINICARD_GENERIC_L	0x8137
#define DELL_PRODUCT_5520_MINICARD_GENERIC_I	0x8138

#define DELL_PRODUCT_5730_MINICARD_SPRINT	0x8180
#define DELL_PRODUCT_5730_MINICARD_TELUS	0x8181
#define DELL_PRODUCT_5730_MINICARD_VZW		0x8182

#define KYOCERA_VENDOR_ID			0x0c88
#define KYOCERA_PRODUCT_KPC650			0x17da
#define KYOCERA_PRODUCT_KPC680			0x180a

#define ANYDATA_VENDOR_ID			0x16d5
#define ANYDATA_PRODUCT_ADU_620UW		0x6202
#define ANYDATA_PRODUCT_ADU_E100A		0x6501
#define ANYDATA_PRODUCT_ADU_500A		0x6502

#define AXESSTEL_VENDOR_ID			0x1726
#define AXESSTEL_PRODUCT_MV110H			0x1000

#define BANDRICH_VENDOR_ID			0x1A8D
#define BANDRICH_PRODUCT_C100_1			0x1002
#define BANDRICH_PRODUCT_C100_2			0x1003
#define BANDRICH_PRODUCT_1004			0x1004
#define BANDRICH_PRODUCT_1005			0x1005
#define BANDRICH_PRODUCT_1006			0x1006
#define BANDRICH_PRODUCT_1007			0x1007
#define BANDRICH_PRODUCT_1008			0x1008
#define BANDRICH_PRODUCT_1009			0x1009
#define BANDRICH_PRODUCT_100A			0x100a

#define BANDRICH_PRODUCT_100B			0x100b
#define BANDRICH_PRODUCT_100C			0x100c
#define BANDRICH_PRODUCT_100D			0x100d
#define BANDRICH_PRODUCT_100E			0x100e

#define BANDRICH_PRODUCT_100F			0x100f
#define BANDRICH_PRODUCT_1010			0x1010
#define BANDRICH_PRODUCT_1011			0x1011
#define BANDRICH_PRODUCT_1012			0x1012

#define QUALCOMM_VENDOR_ID			0x05C6

#define CMOTECH_VENDOR_ID			0x16d8
#define CMOTECH_PRODUCT_6008			0x6008
#define CMOTECH_PRODUCT_6280			0x6280

#define TELIT_VENDOR_ID				0x1bc7
#define TELIT_PRODUCT_UC864E			0x1003
#define TELIT_PRODUCT_UC864G			0x1004

/* ZTE PRODUCTS */
#define ZTE_VENDOR_ID				0x19d2
#define ZTE_PRODUCT_MF622			0x0001
#define ZTE_PRODUCT_MF628			0x0015
#define ZTE_PRODUCT_MF626			0x0031
#define ZTE_PRODUCT_CDMA_TECH			0xfffe
#define ZTE_PRODUCT_AC8710			0xfff1
#define ZTE_PRODUCT_AC2726			0xfff5
#define ZTE_PRODUCT_AC8710T			0xffff

#define BENQ_VENDOR_ID				0x04a5
#define BENQ_PRODUCT_H10			0x4068

#define DLINK_VENDOR_ID				0x1186
#define DLINK_PRODUCT_DWM_652			0x3e04
#define DLINK_PRODUCT_DWM_652_U5		0xce16
#define DLINK_PRODUCT_DWM_652_U5A		0xce1e

#define QISDA_VENDOR_ID				0x1da5
#define QISDA_PRODUCT_H21_4512			0x4512
#define QISDA_PRODUCT_H21_4523			0x4523
#define QISDA_PRODUCT_H20_4515			0x4515
#define QISDA_PRODUCT_H20_4518			0x4518
#define QISDA_PRODUCT_H20_4519			0x4519

/* TLAYTECH PRODUCTS */
#define TLAYTECH_VENDOR_ID			0x20B9
#define TLAYTECH_PRODUCT_TEU800			0x1682

/* TOSHIBA PRODUCTS */
#define TOSHIBA_VENDOR_ID			0x0930
#define TOSHIBA_PRODUCT_HSDPA_MINICARD		0x1302
#define TOSHIBA_PRODUCT_G450			0x0d45

#define ALINK_VENDOR_ID				0x1e0e
#define ALINK_PRODUCT_PH300			0x9100
#define ALINK_PRODUCT_3GU			0x9200

/* ALCATEL PRODUCTS */
#define ALCATEL_VENDOR_ID			0x1bbb
#define ALCATEL_PRODUCT_X060S_X200		0x0000

#define PIRELLI_VENDOR_ID			0x1266
#define PIRELLI_PRODUCT_C100_1			0x1002
#define PIRELLI_PRODUCT_C100_2			0x1003
#define PIRELLI_PRODUCT_1004			0x1004
#define PIRELLI_PRODUCT_1005			0x1005
#define PIRELLI_PRODUCT_1006			0x1006
#define PIRELLI_PRODUCT_1007			0x1007
#define PIRELLI_PRODUCT_1008			0x1008
#define PIRELLI_PRODUCT_1009			0x1009
#define PIRELLI_PRODUCT_100A			0x100a
#define PIRELLI_PRODUCT_100B			0x100b
#define PIRELLI_PRODUCT_100C			0x100c
#define PIRELLI_PRODUCT_100D			0x100d
#define PIRELLI_PRODUCT_100E			0x100e
#define PIRELLI_PRODUCT_100F			0x100f
#define PIRELLI_PRODUCT_1011			0x1011
#define PIRELLI_PRODUCT_1012			0x1012

/* Airplus products */
#define AIRPLUS_VENDOR_ID			0x1011
#define AIRPLUS_PRODUCT_MCD650			0x3198

/* Longcheer/Longsung vendor ID; makes whitelabel devices that
 * many other vendors like 4G Systems, Alcatel, ChinaBird,
 * Mobidata, etc sell under their own brand names.
 */
#define LONGCHEER_VENDOR_ID			0x1c9e

/* 4G Systems products */
/* This is the 4G XS Stick W14 a.k.a. Mobilcom Debitel Surf-Stick *
 * It seems to contain a Qualcomm QSC6240/6290 chipset            */
#define FOUR_G_SYSTEMS_PRODUCT_W14		0x9603

/* Zoom */
#define ZOOM_PRODUCT_4597			0x9607

/* Haier products */
#define HAIER_VENDOR_ID				0x201e
#define HAIER_PRODUCT_CE100			0x2009

/* Cinterion (formerly Siemens) products */
#define SIEMENS_VENDOR_ID				0x0681
#define CINTERION_VENDOR_ID				0x1e2d
#define CINTERION_PRODUCT_HC25_MDM		0x0047
#define CINTERION_PRODUCT_HC25_MDMNET	0x0040
#define CINTERION_PRODUCT_HC28_MDM		0x004C
#define CINTERION_PRODUCT_HC28_MDMNET	0x004A /* same for HC28J */
#define CINTERION_PRODUCT_EU3_E			0x0051
#define CINTERION_PRODUCT_EU3_P			0x0052
#define CINTERION_PRODUCT_PH8			0x0053

/* Olivetti products */
#define OLIVETTI_VENDOR_ID			0x0b3c
#define OLIVETTI_PRODUCT_OLICARD100		0xc000

/* Celot products */
#define CELOT_VENDOR_ID				0x211f
#define CELOT_PRODUCT_CT680M			0x6801

/* ONDA Communication vendor id */
#define ONDA_VENDOR_ID       0x1ee8

/* ONDA MT825UP HSDPA 14.2 modem */
#define ONDA_MT825UP         0x000b

/* Samsung products */
#define SAMSUNG_VENDOR_ID                       0x04e8
#define SAMSUNG_PRODUCT_GT_B3730                0x6889

/* YUGA products  www.yuga-info.com*/
#define YUGA_VENDOR_ID				0x257A
#define YUGA_PRODUCT_CEM600			0x1601
#define YUGA_PRODUCT_CEM610			0x1602
#define YUGA_PRODUCT_CEM500			0x1603
#define YUGA_PRODUCT_CEM510			0x1604
#define YUGA_PRODUCT_CEM800			0x1605
#define YUGA_PRODUCT_CEM900			0x1606

#define YUGA_PRODUCT_CEU818			0x1607
#define YUGA_PRODUCT_CEU816			0x1608
#define YUGA_PRODUCT_CEU828			0x1609
#define YUGA_PRODUCT_CEU826			0x160A
#define YUGA_PRODUCT_CEU518			0x160B
#define YUGA_PRODUCT_CEU516			0x160C
#define YUGA_PRODUCT_CEU528			0x160D
#define YUGA_PRODUCT_CEU526			0x160F

#define YUGA_PRODUCT_CWM600			0x2601
#define YUGA_PRODUCT_CWM610			0x2602
#define YUGA_PRODUCT_CWM500			0x2603
#define YUGA_PRODUCT_CWM510			0x2604
#define YUGA_PRODUCT_CWM800			0x2605
#define YUGA_PRODUCT_CWM900			0x2606

#define YUGA_PRODUCT_CWU718			0x2607
#define YUGA_PRODUCT_CWU716			0x2608
#define YUGA_PRODUCT_CWU728			0x2609
#define YUGA_PRODUCT_CWU726			0x260A
#define YUGA_PRODUCT_CWU518			0x260B
#define YUGA_PRODUCT_CWU516			0x260C
#define YUGA_PRODUCT_CWU528			0x260D
#define YUGA_PRODUCT_CWU526			0x260F

#define YUGA_PRODUCT_CLM600			0x2601
#define YUGA_PRODUCT_CLM610			0x2602
#define YUGA_PRODUCT_CLM500			0x2603
#define YUGA_PRODUCT_CLM510			0x2604
#define YUGA_PRODUCT_CLM800			0x2605
#define YUGA_PRODUCT_CLM900			0x2606

#define YUGA_PRODUCT_CLU718			0x2607
#define YUGA_PRODUCT_CLU716			0x2608
#define YUGA_PRODUCT_CLU728			0x2609
#define YUGA_PRODUCT_CLU726			0x260A
#define YUGA_PRODUCT_CLU518			0x260B
#define YUGA_PRODUCT_CLU516			0x260C
#define YUGA_PRODUCT_CLU528			0x260D
#define YUGA_PRODUCT_CLU526			0x260F

/* some devices interfaces need special handling due to a number of reasons */
enum option_blacklist_reason {
		OPTION_BLACKLIST_NONE = 0,
		OPTION_BLACKLIST_SENDSETUP = 1,
		OPTION_BLACKLIST_RESERVED_IF = 2
};

struct option_blacklist_info {
	const u32 infolen;	/* number of interface numbers on blacklist */
	const u8  *ifaceinfo;	/* pointer to the array holding the numbers */
	enum option_blacklist_reason reason;
};

static const u8 four_g_w14_no_sendsetup[] = { 0, 1 };
static const struct option_blacklist_info four_g_w14_blacklist = {
	.infolen = ARRAY_SIZE(four_g_w14_no_sendsetup),
	.ifaceinfo = four_g_w14_no_sendsetup,
	.reason = OPTION_BLACKLIST_SENDSETUP
};

static const u8 alcatel_x200_no_sendsetup[] = { 0, 1 };
static const struct option_blacklist_info alcatel_x200_blacklist = {
	.infolen = ARRAY_SIZE(alcatel_x200_no_sendsetup),
	.ifaceinfo = alcatel_x200_no_sendsetup,
	.reason = OPTION_BLACKLIST_SENDSETUP
};

static const u8 zte_k3765_z_no_sendsetup[] = { 0, 1, 2 };
static const struct option_blacklist_info zte_k3765_z_blacklist = {
	.infolen = ARRAY_SIZE(zte_k3765_z_no_sendsetup),
	.ifaceinfo = zte_k3765_z_no_sendsetup,
	.reason = OPTION_BLACKLIST_SENDSETUP
};

static const struct usb_device_id option_ids[] = {
    { USB_DEVICE(VIATELECOM_VENDOR_ID, VIATELECOM_PRODUCT_ID) },
    { USB_DEVICE(0x201e, 0x1022) },
	{ } /* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, option_ids);


/* add rawbulk suspend support  */
static int usb_serial_option_suspend(struct usb_interface *intf, pm_message_t message)
{
    int rc = 0;

    rc = usb_serial_suspend(intf, message);
    if(rc < 0)
    {
        err("usb_serial_suspend return error, errno=%d\n", rc);
        return rc;
    }
#ifdef CONFIG_USB_ANDROID_RAWBULK
    return rawbulk_suspend_host_interface(intf->cur_altsetting->desc.bInterfaceNumber, message);
#else
    return rc;
#endif
}

static int usb_serial_option_resume(struct usb_interface *intf)
{
    int rc = 0;

    rc = usb_serial_resume(intf);
    if(rc < 0)
    {
        err("usb_serial_resume return error, errno=%d\n", rc);
        return rc;
    }

#ifdef CONFIG_USB_ANDROID_RAWBULK
    return rawbulk_resume_host_interface(intf->cur_altsetting->desc.bInterfaceNumber);
#else
    return rc;
#endif
}
/* End of temp added */

static int usb_serial_option_probe(struct usb_interface *interface,
        const struct usb_device_id *id);
static void usb_serial_option_disconnect(struct usb_interface *interface);
static struct usb_driver option_driver = {
	.name       = "via_option",
	.probe      = usb_serial_option_probe,
	.disconnect = usb_serial_option_disconnect,
#ifdef CONFIG_PM
	.suspend    = usb_serial_option_suspend,
	.resume     = usb_serial_option_resume,
	.supports_autosuspend =	1,
#endif
	.id_table   = option_ids,
	.no_dynamic_id = 	1,
};

static int usb_wwan_option_write(struct tty_struct *tty, struct usb_serial_port
        *port, const unsigned char *buf, int count);


/* The card has three separate interfaces, which the serial driver
 * recognizes separately, thus num_port=1.
 */
static struct usb_serial_driver option_1port_device = {
	.driver = {
		.owner =	THIS_MODULE,
		.name =		"via_option1",
	},
	.description       = "GSM modem (1-port)",
	.usb_driver        = &option_driver,
	.id_table          = option_ids,
	.num_ports         = 1,
	.probe             = option_probe,
	.open              = usb_wwan_open,
	.close             = usb_wwan_close,
	.dtr_rts           = usb_wwan_dtr_rts,
    .write             = usb_wwan_option_write,
	.write_room        = usb_wwan_write_room,
	.chars_in_buffer   = usb_wwan_chars_in_buffer,
	.set_termios       = usb_wwan_set_termios,
	.tiocmget          = usb_wwan_tiocmget,
	.tiocmset          = usb_wwan_tiocmset,
	.ioctl             = usb_wwan_ioctl,
	.attach            = usb_wwan_startup,
	.disconnect        = usb_wwan_disconnect,
	.release           = usb_wwan_release,
	.read_int_callback = option_instat_callback,
#ifdef CONFIG_PM
	.suspend           = usb_wwan_suspend,
	.resume            = usb_wwan_resume,
#endif
};

static int debug;

/* This is stupid coding, why not use the same private portdata with usb_wwan?? */
#if 0
#define N_IN_URB 4
#define N_OUT_URB 4
#define IN_BUFLEN 4096
#define OUT_BUFLEN 4096

struct option_port_private {
	/* Input endpoints and buffer for this port */
	struct urb *in_urbs[N_IN_URB];
	u8 *in_buffer[N_IN_URB];
	/* Output endpoints and buffer for this port */
	struct urb *out_urbs[N_OUT_URB];
	u8 *out_buffer[N_OUT_URB];
	unsigned long out_busy;		/* Bit vector of URBs in use */
	int opened;
	struct usb_anchor delayed;

	/* Settings for the port */
	int rts_state;	/* Handshaking pins (outputs) */
	int dtr_state;
	int cts_state;	/* Handshaking pins (inputs) */
	int dsr_state;
	int dcd_state;
	int ri_state;

	unsigned long tx_start_time[N_OUT_URB];
};
#else
#define option_port_private usb_wwan_port_private
#endif

/* Functions used by new usb-serial code. */
static int __init option_init(void)
{
	int retval;
	retval = usb_serial_register(&option_1port_device);
	if (retval)
		goto failed_1port_device_register;
	retval = usb_register(&option_driver);
	if (retval)
		goto failed_driver_register;

	printk(KERN_INFO KBUILD_MODNAME ": " DRIVER_VERSION ":"
	       DRIVER_DESC "\n");
	
	wake_lock_init(&ets_wake_lock, WAKE_LOCK_SUSPEND, "ETS_Prevent_Suspend");

	usb_ap_sync_cbp_init();

	return 0;

failed_driver_register:
	usb_serial_deregister(&option_1port_device);
failed_1port_device_register:
	return retval;
}

static void __exit option_exit(void)
{
	wake_lock_destroy(&ets_wake_lock);
	usb_deregister(&option_driver);
	usb_serial_deregister(&option_1port_device);
}

module_init(option_init);
module_exit(option_exit);

static int usb_wwan_option_write(struct tty_struct *tty, struct usb_serial_port *port,
		   const unsigned char *buf, int count)
{
#ifdef CONFIG_USB_ANDROID_RAWBULK
    int inception;
    struct usb_wwan_port_private *portdata = usb_get_serial_port_data(port);
    spin_lock(&portdata->incept_lock);
    inception = portdata->inception;
    spin_unlock(&portdata->incept_lock);

    if (inception)
        return 0;
#endif

    return usb_wwan_write(tty, port, buf, count);
}

#ifdef CONFIG_USB_ANDROID_RAWBULK
#if defined(CONFIG_USB_SUSPEND) && defined(CONFIG_AP2MODEM_VIATELECOM)
extern void ap_wake_modem(void);
extern void ap_sleep_modem(void);
#endif

#ifdef CONFIG_VIATELECOM_SYNC_CBP
static int option_rawbulk_asc_notifier(int msg, void *data) {
    int opened;
    struct usb_serial *serial = data;
    struct usb_serial_port *port = serial->port[0];
    struct usb_wwan_port_private *portdata = usb_get_serial_port_data(port);
    struct usb_wwan_intf_private *intfdata = serial->private;

    spin_lock_irq(&intfdata->susp_lock);
    opened = portdata->opened;
    spin_unlock_irq(&intfdata->susp_lock);

    if (!opened && msg == ASC_NTF_RX_PREPARE)
        asc_rx_confirm_ready(USB_RX_HD_NAME, 1);
    return 0;
}
#endif

static int option_rawbulk_intercept(struct usb_interface *interface, unsigned int flags) {
    int rc = 0;
    struct usb_serial *serial = usb_get_intfdata(interface);
    struct usb_serial_port *port = serial->port[0];
    struct usb_wwan_port_private *portdata = usb_get_serial_port_data(port);
    struct usb_wwan_intf_private *intfdata = serial->private;
    int nif = interface->cur_altsetting->desc.bInterfaceNumber;
    int enable = flags & RAWBULK_INCEPT_FLAG_ENABLE;
    int opened;
    int n;

    spin_lock(&portdata->incept_lock);
    if (portdata->inception == !!enable) {
        spin_unlock(&portdata->incept_lock);
        return -ENOENT;
    }
    spin_unlock(&portdata->incept_lock);

    spin_lock_irq(&intfdata->susp_lock);
    opened = portdata->opened;
    spin_unlock_irq(&intfdata->susp_lock);

    printk(KERN_DEBUG "rawbulk do %s on port %d (%s)\n", enable? "incept":
            "unincept", port->number, opened? "opened": "not-in-use");

    if (enable) {
#if defined(CONFIG_VIATELECOM_SYNC_CBP)
        struct asc_infor user = {
            .notifier = option_rawbulk_asc_notifier,
            .data = serial,
        };
        snprintf(user.name, ASC_NAME_LEN, "%s%d", RAWBULK_RX_USER_NAME, nif);
        asc_rx_add_user(USB_RX_HD_NAME, &user);
        asc_tx_auto_ready(USB_TX_HD_NAME, 0);
#endif

#ifdef CONFIG_PM
        rc = usb_autopm_get_interface(interface);
        if (rc < 0)
            printk(KERN_ERR "incept failed while geting interface#%d\n", nif);
#endif
        /* Stop reading/writing urbs */
        if (opened) {
            if (!(flags & RAWBULK_INCEPT_FLAG_PUSH_WAY))
                for (n = 0; n < portdata->n_in_urb; n++)
                    usb_kill_urb(portdata->in_urbs[n]);
            for (n = 0; n < portdata->n_out_urb; n++)
                usb_kill_urb(portdata->out_urbs[n]);
        }
        /* Start urb for push-way */
        if (flags & RAWBULK_INCEPT_FLAG_PUSH_WAY) {
            for (n = 0; n < portdata->n_in_urb; n ++) {
                struct urb *urb = portdata->in_urbs[n];
                if (!urb || !urb->dev)
                    continue;
                /* reset urb */
                usb_kill_urb(urb);
                usb_clear_halt(urb->dev, urb->pipe);
                rc = usb_submit_urb(urb, GFP_KERNEL);
                if (rc < 0)
                    break;
            }
            if (rc < 0)
                printk(KERN_ERR "incept %d while re-submitting " \
                        "pushable urbs %d\n", nif, rc);
        }
    } else {
#if defined(CONFIG_VIATELECOM_SYNC_CBP)
        char asc_user[ASC_NAME_LEN];
#endif
        rc = usb_autopm_get_interface(interface);
        if (rc < 0)
            printk(KERN_ERR "unincept while get interface#%d\n", nif);
        if (!intfdata->suspended && opened) {
            for (n = 0; n < portdata->n_in_urb; n ++) {
                struct urb *urb = portdata->in_urbs[n];
                if (!urb || !urb->dev)
                    continue;
                /* reset urb */
                usb_kill_urb(urb);
                usb_clear_halt(urb->dev, urb->pipe);
                rc = usb_submit_urb(urb, GFP_KERNEL);
                if (rc < 0)
                    break;
            }
            if (rc < 0)
                printk(KERN_ERR "unincept %d while reseting " \
                        "bulk IN urbs %d\n", nif, rc);
        }
#ifdef CONFIG_PM
        usb_autopm_put_interface(interface);
#endif
#if defined(CONFIG_VIATELECOM_SYNC_CBP)
        snprintf(asc_user, ASC_NAME_LEN, "%s%d", ASC_PATH(USB_RX_HD_NAME, RAWBULK_RX_USER_NAME), nif);
        asc_rx_del_user(asc_user);
#endif
    }

    if (!rc) {
        spin_lock(&portdata->incept_lock);
        portdata->inception = !!enable;
        spin_unlock(&portdata->incept_lock);
    } else
        printk(KERN_ERR "failed(%d) to %s inception on interface#%d\n",
                rc, enable? "enable": "disable", nif);

    return rc;
}

#endif

int serial_ntcall(struct notifier_block *nb, unsigned long val, void *nodata)
{
	struct usb_wwan_intf_private *data = container_of(nb,struct usb_wwan_intf_private,pm_nb);
     
    switch (val) {
        case PM_SUSPEND_PREPARE:
            usb_disable_autosuspend(data->udev);
		return NOTIFY_DONE;
        case PM_POST_SUSPEND:
            usb_enable_autosuspend(data->udev);
        return NOTIFY_DONE;
	}
    return NOTIFY_DONE;
}


static int option_probe(struct usb_serial *serial,
			const struct usb_device_id *id)
{
	struct usb_wwan_intf_private *data;
	/* D-Link DWM 652 still exposes CD-Rom emulation interface in modem mode */
	if (serial->dev->descriptor.idVendor == DLINK_VENDOR_ID &&
		serial->dev->descriptor.idProduct == DLINK_PRODUCT_DWM_652 &&
		serial->interface->cur_altsetting->desc.bInterfaceClass == 0x8)
		return -ENODEV;

	/* Bandrich modem and AT command interface is 0xff */
	if ((serial->dev->descriptor.idVendor == BANDRICH_VENDOR_ID ||
		serial->dev->descriptor.idVendor == PIRELLI_VENDOR_ID) &&
		serial->interface->cur_altsetting->desc.bInterfaceClass != 0xff)
		return -ENODEV;

	/* Don't bind network interfaces on Huawei K3765, K4505 & K4605 */
	if (serial->dev->descriptor.idVendor == HUAWEI_VENDOR_ID &&
		(serial->dev->descriptor.idProduct == HUAWEI_PRODUCT_K3765 ||
			serial->dev->descriptor.idProduct == HUAWEI_PRODUCT_K4505 ||
			serial->dev->descriptor.idProduct == HUAWEI_PRODUCT_K4605) &&
		(serial->interface->cur_altsetting->desc.bInterfaceNumber == 1 ||
			serial->interface->cur_altsetting->desc.bInterfaceNumber == 2))
		return -ENODEV;

	/* Don't bind network interface on Samsung GT-B3730, it is handled by a separate module */
	if (serial->dev->descriptor.idVendor == SAMSUNG_VENDOR_ID &&
		serial->dev->descriptor.idProduct == SAMSUNG_PRODUCT_GT_B3730 &&
		serial->interface->cur_altsetting->desc.bInterfaceClass != USB_CLASS_CDC_DATA)
		return -ENODEV;

	data = serial->private = kzalloc(sizeof(struct usb_wwan_intf_private), GFP_KERNEL);

	if (!data)
		return -ENOMEM;

    usb_enable_autosuspend(serial->dev);
#if 0   //need check
    serial->dev->autosuspend_delay = 3 * HZ;
#endif
    serial->interface->needs_remote_wakeup = 0;
    
    if ((serial->dev->descriptor.idVendor == VIATELECOM_VENDOR_ID &&
        serial->dev->descriptor.idProduct == VIATELECOM_PRODUCT_ID)) {
        data->send_setup = viatelecom_send_setup;
    } else
        data->send_setup = option_send_setup;

	spin_lock_init(&data->susp_lock);
	data->private = (void *)id->driver_info;

	data->udev = serial->dev;
    data->pm_nb.notifier_call = serial_ntcall;
    return register_pm_notifier(&data->pm_nb);
}

#define USB_CBP_TESTMODE_ENABLE         0x1
#define USB_CBP_TESTMODE_QUERY          0x2
#define USB_CBP_TESTMODE_UNSOLICTED     0x3
static ssize_t option_port_testmode_show(struct device *dev, struct device_attribute
        *attr, char *buf)
{
    int rc;
    struct usb_serial_port *port0 = container_of(dev, struct usb_serial_port, dev);
    struct usb_device *udev = port0->serial->dev;
    struct usb_interface *interface = port0->serial->interface;
    int ifnum = interface->cur_altsetting->desc.bInterfaceNumber;
    int feedback = -1;
    rc = usb_autopm_get_interface(interface);
    if (rc < 0) {
        printk(KERN_ERR "failed to awake cp-usb when query testmode. %d\n", rc);
        return -ENODEV;
    }
    rc = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0), USB_CBP_TESTMODE_QUERY,
            USB_DIR_IN|USB_TYPE_VENDOR|USB_RECIP_INTERFACE,
            0, ifnum, &feedback, 2, USB_CTRL_GET_TIMEOUT * 5);
    usb_autopm_put_interface(interface);
    if ((feedback & 0xff) == 0x1)
        return sprintf(buf, "%s", "enabled");
    if ((feedback & 0xff) == 0x0)
        return sprintf(buf, "%s", "disabled");
    printk(KERN_ERR "connot query cp testmode %d, feedback %04x\n", rc, feedback & 0xffff);
    return -EOPNOTSUPP;
}

static ssize_t option_port_testmode_store(struct device *dev, struct device_attribute
        *attr, const char *buf, size_t count)
{
    int rc = 0;
    int enable = -1;
    struct usb_serial_port *port0 = container_of(dev, struct usb_serial_port, dev);
    struct usb_device *udev = port0->serial->dev;
    struct usb_interface *interface = port0->serial->interface;
    int ifnum = interface->cur_altsetting->desc.bInterfaceNumber;

    if (!strncmp(buf, "enable", 6))
        enable = 1;
    else if (!strncmp(buf, "disable", 7))
        enable = 0;
    if (enable >= 0) {
        rc = usb_autopm_get_interface(interface);
        if (rc < 0) {
            printk(KERN_ERR "failed to awake cp-usb when set testmode. %d\n", rc);
            return -ENODEV;
        }
        rc = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
                USB_CBP_TESTMODE_ENABLE,
                USB_DIR_OUT|USB_TYPE_VENDOR|USB_RECIP_INTERFACE,
                enable, ifnum, NULL, 0, USB_CTRL_SET_TIMEOUT);
        usb_autopm_put_interface(interface);
        if (rc < 0) {
            printk(KERN_DEBUG "failed to sent testmode contorl msg %d\n", rc);
            return rc;
        }
    } else if (!strncmp(buf, "unsolited", 9)) {
        /* rc = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
           USB_CBP_TESTMODE_UNSOLICTED,
           USB_DIR_OUT|USB_TYPE_VENDOR|USB_RECIP_INTERFACE,
           0, ifnum, &param, sizeof(param), USB_CTRL_SET_TIMEOUT); */
        printk(KERN_DEBUG "unsolited test mode for cp does not supported yet.\n");
    }

    return count;
}

static DEVICE_ATTR(testmode, S_IRUGO | S_IWUSR,
        option_port_testmode_show,
        option_port_testmode_store);

static int usb_serial_option_probe(struct usb_interface *interface,
        const struct usb_device_id *id) {
    struct usb_serial *serial;
    int ret = usb_serial_probe(interface, id);
    if (ret < 0)
        return ret;

#ifdef CONFIG_USB_ANDROID_RAWBULK
    ret = rawbulk_bind_host_interface(interface, option_rawbulk_intercept);
    if (ret < 0)
        return ret;
#endif

    serial = usb_get_intfdata(interface);
    return sysfs_create_file(&serial->port[0]->dev.kobj,
            &dev_attr_testmode.attr);
}

static void usb_serial_option_disconnect(struct usb_interface *interface) {
    struct usb_serial *serial = usb_get_intfdata(interface);
	struct usb_wwan_intf_private *data = serial->private;

    unregister_pm_notifier(&data->pm_nb);

    sysfs_remove_file(&serial->port[0]->dev.kobj, &dev_attr_testmode.attr);
#ifdef CONFIG_USB_ANDROID_RAWBULK
    rawbulk_unbind_host_interface(interface);
#endif

    usb_serial_disconnect(interface);
}

static enum option_blacklist_reason is_blacklisted(const u8 ifnum,
				const struct option_blacklist_info *blacklist)
{
	const u8  *info;
	int i;

	if (blacklist) {
		info = blacklist->ifaceinfo;

		for (i = 0; i < blacklist->infolen; i++) {
			if (info[i] == ifnum)
				return blacklist->reason;
		}
	}
	return OPTION_BLACKLIST_NONE;
}

static void option_instat_callback(struct urb *urb)
{
	int err;
	int status = urb->status;
	struct usb_serial_port *port =  urb->context;
	struct option_port_private *portdata = usb_get_serial_port_data(port);

	dbg("%s", __func__);
	dbg("%s: urb %p port %p has data %p", __func__, urb, port, portdata);

	if (status == 0) {
		struct usb_ctrlrequest *req_pkt =
				(struct usb_ctrlrequest *)urb->transfer_buffer;

		if (!req_pkt) {
			dbg("%s: NULL req_pkt", __func__);
			return;
		}
		if ((req_pkt->bRequestType == 0xA1) &&
				(req_pkt->bRequest == 0x20)) {
			int old_dcd_state;
			unsigned char signals = *((unsigned char *)
					urb->transfer_buffer +
					sizeof(struct usb_ctrlrequest));

			dbg("%s: signal x%x", __func__, signals);

			old_dcd_state = portdata->dcd_state;
			portdata->cts_state = 1;
			portdata->dcd_state = ((signals & 0x01) ? 1 : 0);
			portdata->dsr_state = ((signals & 0x02) ? 1 : 0);
			portdata->ri_state = ((signals & 0x08) ? 1 : 0);

			if (old_dcd_state && !portdata->dcd_state) {
				struct tty_struct *tty =
						tty_port_tty_get(&port->port);
				if (tty && !C_CLOCAL(tty))
					tty_hangup(tty);
				tty_kref_put(tty);
			}
		} else {
			dbg("%s: type %x req %x", __func__,
				req_pkt->bRequestType, req_pkt->bRequest);
		}
	} else
		err("%s: error %d", __func__, status);

	/* Resubmit urb so we continue receiving IRQ data */
	if (status != -ESHUTDOWN && status != -ENOENT) {
		err = usb_submit_urb(urb, GFP_ATOMIC);
		if (err)
			dbg("%s: resubmit intr urb failed. (%d)",
				__func__, err);
	}
}

/** send RTS/DTR state to the port.
 *
 * This is exactly the same as SET_CONTROL_LINE_STATE from the PSTN
 * CDC.
*/
static int option_send_setup(struct usb_serial_port *port)
{
	struct usb_serial *serial = port->serial;
	struct usb_wwan_intf_private *intfdata =
		(struct usb_wwan_intf_private *) serial->private;
	struct option_port_private *portdata;
	int ifNum = serial->interface->cur_altsetting->desc.bInterfaceNumber;
	int val = 0;
	dbg("%s", __func__);

	if (is_blacklisted(ifNum,
			   (struct option_blacklist_info *) intfdata->private)
	    == OPTION_BLACKLIST_SENDSETUP) {
		dbg("No send_setup on blacklisted interface #%d\n", ifNum);
		return -EIO;
	}

	portdata = usb_get_serial_port_data(port);

	if (portdata->dtr_state)
		val |= 0x01;
	if (portdata->rts_state)
		val |= 0x02;

	return usb_control_msg(serial->dev,
		usb_rcvctrlpipe(serial->dev, 0),
		0x22, 0x21, val, ifNum, NULL, 0, USB_CTRL_SET_TIMEOUT);
}

static int viatelecom_send_setup(struct usb_serial_port *port)
{
	struct usb_serial *serial = port->serial;
	struct option_port_private *portdata = usb_get_serial_port_data(port);
	int ifNum = serial->interface->cur_altsetting->desc.bInterfaceNumber;
	dbg("%s", __func__);

    if (ifNum != 0) /* control DATA Port only */
        return 0;

    /* VIA-Telecom CBP DTR format */
    return usb_control_msg(serial->dev,
            usb_sndctrlpipe(serial->dev, 0),
            0x01, 0x40, portdata->dtr_state? 1: 0, ifNum,
            NULL, 0, USB_CTRL_SET_TIMEOUT);
}

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");

//module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Debug messages");
