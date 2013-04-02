/*
 * Definitions for SCI921x hardware
 *
 * Based on the r8187 driver, which is:
 * Copyright 2005 Andrea Merello <andreamrl@tiscali.it>, et al.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef SCI921X_H
#define SCI921X_H

struct sci921x_csr {
    u8	MSR;
#define SCI921X_MSR_NO_LINK		(0 << 2)
#define SCI921X_MSR_ADHOC		(1 << 2)
#define SCI921X_MSR_INFRA		(2 << 2)
#define SCI921X_MSR_MASTER		(3 << 2)
#define SCI921X_MSR_ENEDCA		(4 << 2)
    __le32  RX_CONF;
#if 0
#define SCI921X_RX_CONF_UNICAST   (1 <<  0)
#define SCI921X_RX_CONF_MULTICAST   (1 <<  1)
#define SCI921X_RX_CONF_BROADCAST   (1 <<  2)
#define SCI921X_RX_CONF_CTRL   (1 <<  3)
#define SCI921X_RX_CONF_BCN_EN   (1 <<  4)
#define SCI921X_RX_CONF_BCN_AEN   (1 <<  5)
#define SCI921X_RX_CONF_SNF_EN   (1 <<  6)
#define SCI921X_RX_CONF_PRB_REQ_EN   (1 <<  7)
#define SCI921X_RX_CONF_PRB_RSP_EN   (1 <<  8)
#define SCI921X_RX_CONF_CTS_TM     (1 <<  16)
#define SCI921X_RX_CONF_CTS_TST        (1 << 20)
#define SCI921X_RX_CONF_ACK_TM     (1 <<  21)
#define SCI921X_RX_CONF_ACK_TST        (1 << 25)
#endif

/////////////////////////////////////////////
#define SCI921X_RX_CONF_MONITOR		(1 <<  0)
#define SCI921X_RX_CONF_NICMAC		(1 <<  1)
#define SCI921X_RX_CONF_MULTICAST	(1 <<  2)
#define SCI921X_RX_CONF_BROADCAST	(1 <<  3)
#define SCI921X_RX_CONF_FCS		(1 <<  5)
#define SCI921X_RX_CONF_DATA		(1 << 18)
#define SCI921X_RX_CONF_CTRL		(1 << 19)
#define SCI921X_RX_CONF_MGMT		(1 << 20)
#define SCI921X_RX_CONF_ADDR3		(1 << 21)
#define SCI921X_RX_CONF_PM		(1 << 22)
#define SCI921X_RX_CONF_BSSID		(1 << 23)
#define SCI921X_RX_CONF_RX_AUTORESETPHY	(1 << 28)
#define SCI921X_RX_CONF_CSDM1		(1 << 29)
#define SCI921X_RX_CONF_CSDM2		(1 << 30)
#define SCI921X_RX_CONF_ONLYERLPKT	(1 << 31)
//////////////////////////////////////////////

} __attribute__((packed));

struct ieee80211_hw;
struct ieee80211_conf;

struct sci921x_rf_ops {
    char *name;
    void (*init)(struct ieee80211_hw *);
    void (*stop)(struct ieee80211_hw *);
    void (*set_chan)(struct ieee80211_hw *, struct ieee80211_conf *);
};

/* Tx/Rx flags are common between SCI921X chips */

enum sci921x_tx_desc_flags {
    SCI921X_TX_DESC_FLAG_NO_ENC = (1 << 15),
    SCI921X_TX_DESC_FLAG_TX_OK  = (1 << 15),
    SCI921X_TX_DESC_FLAG_SPLCP  = (1 << 16),
    SCI921X_TX_DESC_FLAG_RX_UNDER   = (1 << 16),
    SCI921X_TX_DESC_FLAG_MOREFRAG   = (1 << 17),
    SCI921X_TX_DESC_FLAG_CTS    = (1 << 18),
    SCI921X_TX_DESC_FLAG_RTS    = (1 << 23),
    SCI921X_TX_DESC_FLAG_LS     = (1 << 28),
    SCI921X_TX_DESC_FLAG_FS     = (1 << 29),
    SCI921X_TX_DESC_FLAG_DMA    = (1 << 30),
    SCI921X_TX_DESC_FLAG_OWN    = (1 << 31)
};

enum sci921x_rx_desc_flags {
    SCI921X_RX_DESC_FLAG_ICV_ERR    = (1 << 12),
    SCI921X_RX_DESC_FLAG_CRC32_ERR  = (1 << 13),
    SCI921X_RX_DESC_FLAG_PM     = (1 << 14),
    SCI921X_RX_DESC_FLAG_RX_ERR = (1 << 15),
    SCI921X_RX_DESC_FLAG_BCAST  = (1 << 16),
    SCI921X_RX_DESC_FLAG_PAM    = (1 << 17),
    SCI921X_RX_DESC_FLAG_MCAST  = (1 << 18),
    SCI921X_RX_DESC_FLAG_QOS    = (1 << 19), /* sci9211(B) only */
    SCI921X_RX_DESC_FLAG_TRSW   = (1 << 24), /* sci9211(B) only */
    SCI921X_RX_DESC_FLAG_SPLCP  = (1 << 25),
    SCI921X_RX_DESC_FLAG_FOF    = (1 << 26),
    SCI921X_RX_DESC_FLAG_DMA_FAIL   = (1 << 27),
    SCI921X_RX_DESC_FLAG_LS     = (1 << 28),
    SCI921X_RX_DESC_FLAG_FS     = (1 << 29),
    SCI921X_RX_DESC_FLAG_EOR    = (1 << 30),
    SCI921X_RX_DESC_FLAG_OWN    = (1 << 31)
};

/* Security algorithms. */
enum {
    SCI9211_SEC_ALGO_NONE = 0,  /* unencrypted, as of TX header. */
    SCI9211_SEC_ALGO_WEP40,
    SCI9211_SEC_ALGO_TKIP,
    SCI9211_SEC_ALGO_AES,
    SCI9211_SEC_ALGO_WEP104,
    SCI9211_SEC_ALGO_AES_LEGACY,
    SCI9211_SEC_ALGO_SMS4,   //05242011 DMOU
};


#endif /* SCI921X_H */
