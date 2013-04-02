/*
 * Definitions for sci9211 hardware
 *
 * Copyright 2010 Thomas Luo <shbluo@sci-inc.com.cn>
 * Copyright 2010 DongMei Ou <dmou@sci-inc.com.cn>
 *
 * Based on the r8187 driver, which is:
 * Copyright 2005 Andrea Merello <andreamrl@tiscali.it>, et al.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef SCI9211_H
#define SCI9211_H

#include <linux/version.h>
#include "sci921x.h"
#include <../net/mac80211/ieee80211_i.h>
#include "MAC_Control.h"
#include <linux/crc32.h>


#if 0
#define SCI9211_EEPROM_TXPWR_BASE   0x05
#define SCI9211_EEPROM_MAC_ADDR     0x07
#define SCI9211_EEPROM_TXPWR_CHAN_1 0x16    /* 3 channels */
#define SCI9211_EEPROM_TXPWR_CHAN_6 0x1B    /* 2 channels */
#define SCI9211_EEPROM_TXPWR_CHAN_4 0x3D    /* 2 channels */
#endif
#define SCI9211_REQT_READ   0xC0
#define SCI9211_REQT_WRITE  0x40
//#define SCI9211_REQ_GET_REG 0x05
//#define SCI9211_REQ_SET_REG 0x05

#define SCI9211_MAX_RX      0x1000

#define MORE_AP 0   //for scan more AP
#define DEBUG_ON 0
#define PS  0

#if DEBUG_ON
#define DEG(...) printk(KERN_DEBUG "[SCI].debug " __VA_ARGS__)
#define INFO(...) printk(KERN_INFO "[SCI].info " __VA_ARGS__)
#define WAR(...) printk(KERN_WARNING "[SCI].waring " __VA_ARGS__)
#define ERR(...) printk(KERN_ERR "[SCI].error " __VA_ARGS__)
#else
#define DEG(...) ((void)0)
#define INFO(...) ((void)0)
#define WAR(...) ((void)0)
#define ERR(...) printk(KERN_ERR "[SCI].error " __VA_ARGS__)
#endif

struct sci9211_rx_info {
    struct urb *urb;
    struct ieee80211_hw *dev;
};

struct sci9211_rx_hdr {
    __le32 flags;
    u8 noise;
    u8 signal;
    u8 agc;
    u8 reserved;
    __le64 mac_time;
} __attribute__((packed));

/* {sci9211,sci9212}_tx_info is in skb */

struct sci9211_tx_hdr {
    __le32 flags;
    __le16 rts_duration;
    __le16 len;
    __le32 retry;
} __attribute__((packed));

struct sci_tx_buf {
    struct list_head    list;
    struct sk_buff *skb;
};

// EDCA configuration from AP's BEACON/ProbeRsp
typedef struct PACKED {
    u8       Aifsn[4];       // 0:AC_BK, 1:AC_BE, 2:AC_VI, 3:AC_VO
    u8       Cwmin[4];
    u8       Cwmax[4];
    u16      Txop[4];      // in unit of 32-us
    bool     bACM[4];      // 1: Admission Control of AC_BK is mandattory
} EDCA_PARM, *PEDCA_PARM;


typedef union _LARGE_INTEGEREX {
#ifdef BIG_ENDIAN
    struct {
        long high_part;
        unsigned long low_part;
    }vv;
    struct {
        long high_part;
        unsigned long low_part;
    } u;
#else
    struct {
        unsigned long low_part;
        long high_part;
    }vv;
    struct {
        unsigned long low_part;
        long high_part;
    } u;
#endif

    s64 quad_part;
} LARGE_INTEGEREX;


struct sci9211_priv {
    const struct sci921x_rf_ops *rf;
    struct ieee80211_vif *vif;
    int mode;
    /* The mutex protects the TX loopback state.
     * Any attempt to set channels concurrently locks the device.
     */
    struct mutex conf_mutex;
    struct ieee80211_channel channels[14];
    struct ieee80211_rate rates[12];
    struct ieee80211_supported_band band;
    struct usb_device *udev;
    u32 rx_conf;
    u16 txpwr_base;
    u16 seqno;
    u8 asic_rev;
    struct sk_buff_head rx_queue;
    u8 signal;
    u8 quality;
    u8 noise;    
    /* handle for mac control */
    u32 hmac_control;  
    u8 bssid[6];
    PMAC_CONTROL hw_control;  //handle for HW**
    //u32 retry_count,retry_fail;
    u8 short_pre;    //for rate control
    struct ieee80211_hw *dev;
    struct usb_anchor anchored;
    EDCA_PARM pEdcaParm ;  //wmm param
    struct delayed_work work;   //for reg control in tx loop
    struct {
        __le64 buf;
        struct sk_buff_head queue;
    } tx_status; 
    int wapi;   
    u8 rftype;  //RF
    u8 mactype;  //MAC
    u8 modtype;  //PHY
    u8 is_disconnect;  //for usb disconnect
    //u8 sbssid_flag;  
    u8 rx_qos;   //rx qos data flag
    unsigned short ver;  
    int8_t           T_CAL;
    unsigned short   T_SAMPLE;
    int T_INITIAL;
    int T_CURRENT;
    int DIFF_INI_CURRENT;
    int DIFF_CAL_CURRENT;
    u8 AGC_WORD;
    int8_t RX_POWER; 
    int receive_rssi[8];
    int AVG_RSSI_last;
    int AVG_RSSI_current;
    /*int CURRENT_VCO;
    int UPPER_TEMP_LIMIT;
    int UP_TEMP_LIMIT;
    int LOW_TEMP_LIMIT;*/
    int8_t TEMP_DEC[7];
    int8_t TEMP_INC[11];
    u_int8_t RF_GAIN_NEW;

    u8 POWER_SAVE_ADJUST_EN;
    int8_t RX_POWER_LIMIT[18];
    int8_t RF_GAIN_STEP[19];
    u8 RF_TX_GAIN_LIMIT;
    u8 RF_CCK_GAIN_SCAN;
    u8 RF_TX_GAIN_INI;
    int8_t CH_DIFF_POWER;
    struct _CH_QAM64_GAIN CHQAM64Gain;
    struct _CH_DIFF_POWER CHDiffPower;

    u8 RF_QAM64_GAIN_FINAL;
    u8 RF_QAM16_GAIN_FINAL;
    u8 RF_QPSK_GAIN_FINAL;
    u8 RF_BPSK_GAIN_FINAL;
    u8 RF_CCK_GAIN_FINAL;

    struct work_struct temperature_work;
    struct timer_list cal_temp_timer;
#if MORE_AP
    u8 scan_flag;   //tx probe request frame
    struct work_struct scan_work;
    struct timer_list scan_timer;
    //struct timeval scan_begin_time1 ;
    //struct timeval scan_begin_time2 ;
    //long diff_scan_time ;
#endif    
    u8 auth_flag;    //tx auth frame
    u8 assoc_flag;    //tx assoc frame
    u8 assoc;    //associated status
    u8 beacon_num;    //count num of beacon for rssi  after associated
    u8 chan_last;     //channel of the AP last time associated 
    u8 chan_current;  //channel of the AP current associated 
    //struct delayed_work debug_work;
    //struct timer_list debug_timer;
#if PS
    LARGE_INTEGEREX time_stamp;  //associated beacon time_stamp
    u16 beacon_int;  //associated beacon inter
    struct work_struct rx_work;   //for set ps reg outside rx loop
    u8 qos;       //assosiciation response include AP info of wmm
    //struct timeval beacon_time ;
    //struct timeval setreg_time ;
    //long diff_time ;
    
    struct work_struct ps_work;
    struct timer_list ps_timer;
    u32 tx_data_num;   //num of tx data per second
    u8 tx_low_cal;     //calculate the num of low tx_data_num
    u8 set_ps;   //flag of set PS_EN
	struct work_struct force_ps_work;
    struct timer_list force_ps_timer;
    u32 beacon_num_ps;   //count for ps every 300ms after assoc
    u32 force_awake;   //reg 90c
#endif



};

static inline u32 sci921x_ioread32_idx(struct sci9211_priv *priv,
                       __le32 *addr, u8 idx)
{
   __le32 val;
   void *temp = kmalloc(8, GFP_KERNEL);
   if(temp == NULL) {
       return 0;
   }

    usb_control_msg(priv->udev, usb_rcvctrlpipe(priv->udev, 0),
            2, SCI9211_REQT_READ,
            0, (unsigned long)addr, temp,
            sizeof(val), HZ );

    memcpy(&val,temp,4);
    //INFO ("ioread: ADDR: 0x%lx  VAL = 0x%x  !\n",(unsigned long)addr,(unsigned int)val);
    kfree(temp);

    return le32_to_cpu(val);
}

static inline u32 sci921x_ioread32(struct sci9211_priv *priv, __le32 *addr)
{
    return sci921x_ioread32_idx(priv, addr, 0);
}


static inline void sci921x_iowrite32_idx(struct sci9211_priv *priv,
                     __le32 *addr, u32 val, u8 idx)
{
    u32 add;
    u8 a[8];
    add = (unsigned long) addr;

    a[0] = add & 0x00FF; 
	a[1] = add >> 8; 
	a[2] = 0x0; 
	a[3] = val & 0x00FF;
    a[4] = (val >> 8) & 0x00FF; 
	a[5] = (val >> 16) & 0x00FF; 
	a[6] = val >> 24;
    a[7] = 0x07;

    usb_control_msg(priv->udev, usb_sndctrlpipe(priv->udev, 0),
            0x01, SCI9211_REQT_WRITE,
            (unsigned long)0, 0, a, 8,
            HZ );
    //printk(KERN_INFO "iowrite : ADDR: 0x%x  VAL = 0x%x  !\n",add,val);
}

static inline void sci921x_iowrite32(struct sci9211_priv *priv, __le32 *addr,
                     u32 val)
{
    sci921x_iowrite32_idx(priv, addr, val, 0);
}


/************* for debug *************/
#if DEBUG_ON
static inline void sci_deb_hex(const char *prompt, const u8 *buf, int len)
{
    int i = 0;

    if (len)
    {
        for (i = 1; i <= len; i++) {
            if ((i & 0xf) == 1) {
                if (i != 1)
                    printk("\n");
                INFO( " %s: ", prompt);
            }
            printk("%02x ", (u8) * buf);
            buf++;
        }
        printk("\n");
    }
}
#else
#define sci_deb_hex(prompt,buf,len) do {} while (0)
#endif

static inline void sci_print_reg(struct sci9211_priv *priv)
{
    u32 i;
    u16 addr=0x0;

    addr=0x50c;
    for(i=0;i<=0x40;i=i+4)
    {
       sci921x_ioread32(priv,(__le32 *)(addr+i));
    }

    addr=0x644;
    for(i=0;i<=0x10;i=i+4)
    {
       sci921x_ioread32(priv,(__le32 *)(addr+i));
    }
}

#if 0   //new
static u32 sci_ieee80211_parse_elems_crc(u8 *start, size_t len,
                   struct ieee802_11_elems *elems,
                   u64 filter, u32 crc)
{
    size_t left = len;
    u8 *pos = start;
    bool calc_crc = filter != 0;

    memset(elems, 0, sizeof(*elems));
    elems->ie_start = start;
    elems->total_len = len;

    while (left >= 2) {
        u8 id, elen;

        id = *pos++;
        elen = *pos++;
        left -= 2;

        if (elen > left)
            break;

        if (calc_crc && id < 64 && (filter & (1ULL << id)))
            crc = crc32_be(crc, pos - 2, elen + 2);

        switch (id) {
        case WLAN_EID_SSID:
            elems->ssid = pos;
            elems->ssid_len = elen;
            break;
        case WLAN_EID_SUPP_RATES:
            elems->supp_rates = pos;
            elems->supp_rates_len = elen;
            break;
        case WLAN_EID_FH_PARAMS:
            elems->fh_params = pos;
            elems->fh_params_len = elen;
            break;
        case WLAN_EID_DS_PARAMS:
            elems->ds_params = pos;
            elems->ds_params_len = elen;
            break;
        case WLAN_EID_CF_PARAMS:
            elems->cf_params = pos;
            elems->cf_params_len = elen;
            break;
        case WLAN_EID_TIM:
            if (elen >= sizeof(struct ieee80211_tim_ie)) {
                elems->tim = (void *)pos;
                elems->tim_len = elen;
            }
            break;
        case WLAN_EID_IBSS_PARAMS:
            elems->ibss_params = pos;
            elems->ibss_params_len = elen;
            break;
        case WLAN_EID_CHALLENGE:
            elems->challenge = pos;
            elems->challenge_len = elen;
            break;
        case WLAN_EID_VENDOR_SPECIFIC:
            if (elen >= 4 && pos[0] == 0x00 && pos[1] == 0x50 &&
                pos[2] == 0xf2) {
                /* Microsoft OUI (00:50:F2) */

                if (calc_crc)
                    crc = crc32_be(crc, pos - 2, elen + 2);

                if (pos[3] == 1) {
                    /* OUI Type 1 - WPA IE */
                    elems->wpa = pos;
                    elems->wpa_len = elen;
                } else if (elen >= 5 && pos[3] == 2) {
                    /* OUI Type 2 - WMM IE */
                    if (pos[4] == 0) {
                        elems->wmm_info = pos;
                        elems->wmm_info_len = elen;
                    } else if (pos[4] == 1) {
                        elems->wmm_param = pos;
                        elems->wmm_param_len = elen;
                    }
                }
            }
            break;
        case WLAN_EID_RSN:
            elems->rsn = pos;
            elems->rsn_len = elen;
            break;
        case WLAN_EID_ERP_INFO:
            elems->erp_info = pos;
            elems->erp_info_len = elen;
            break;
        case WLAN_EID_EXT_SUPP_RATES:
            elems->ext_supp_rates = pos;
            elems->ext_supp_rates_len = elen;
            break;
        case WLAN_EID_HT_CAPABILITY:
            if (elen >= sizeof(struct ieee80211_ht_cap))
                elems->ht_cap_elem = (void *)pos;
            break;
        case WLAN_EID_HT_INFORMATION:
            if (elen >= sizeof(struct ieee80211_ht_info))
                elems->ht_info_elem = (void *)pos;
            break;
        case WLAN_EID_MESH_ID:
            elems->mesh_id = pos;
            elems->mesh_id_len = elen;
            break;
        case WLAN_EID_MESH_CONFIG:
            if (elen >= sizeof(struct ieee80211_meshconf_ie))
                elems->mesh_config = (void *)pos;
            break;
        case WLAN_EID_PEER_LINK:
            elems->peer_link = pos;
            elems->peer_link_len = elen;
            break;
        case WLAN_EID_PREQ:
            elems->preq = pos;
            elems->preq_len = elen;
            break;
        case WLAN_EID_PREP:
            elems->prep = pos;
            elems->prep_len = elen;
            break;
        case WLAN_EID_PERR:
            elems->perr = pos;
            elems->perr_len = elen;
            break;
        case WLAN_EID_RANN:
            if (elen >= sizeof(struct ieee80211_rann_ie))
                elems->rann = (void *)pos;
            break;
        case WLAN_EID_CHANNEL_SWITCH:
            elems->ch_switch_elem = pos;
            elems->ch_switch_elem_len = elen;
            break;
        case WLAN_EID_QUIET:
            if (!elems->quiet_elem) {
                elems->quiet_elem = pos;
                elems->quiet_elem_len = elen;
            }
            elems->num_of_quiet_elem++;
            break;
        case WLAN_EID_COUNTRY:
            elems->country_elem = pos;
            elems->country_elem_len = elen;
            break;
        case WLAN_EID_PWR_CONSTRAINT:
            elems->pwr_constr_elem = pos;
            elems->pwr_constr_elem_len = elen;
            break;
        case WLAN_EID_TIMEOUT_INTERVAL:
            elems->timeout_int = pos;
            elems->timeout_int_len = elen;
            break;
        default:
            break;
        }

        left -= elen;
        pos += elen;
    }

    return crc;
}

static inline void ieee80211_parse_elems(u8 *start, size_t len,
                struct ieee802_11_elems *elems)
{
    sci_ieee80211_parse_elems_crc(start, len, elems, 0, 0);
}
#endif


#if 1
static inline void ieee80211_parse_elems(u8 *start, size_t len,
                struct ieee802_11_elems *elems)
{
    size_t left = len;
    u8 *pos = start;

    memset(elems, 0, sizeof(*elems));
    elems->ie_start = start;
    elems->total_len = len;

    while (left >= 2) {
        u8 id, elen;

        id = *pos++;
        elen = *pos++;
        left -= 2;

        if (elen > left)
            return;

        switch (id) {
        case 0:  //WLAN_EID_SSID:
            elems->ssid = pos;
            elems->ssid_len = elen;
            break;
        case 1:  //WLAN_EID_SUPP_RATES:
            elems->supp_rates = pos;
            elems->supp_rates_len = elen;
            break;
        case 2:  //WLAN_EID_FH_PARAMS:
            elems->fh_params = pos;
            elems->fh_params_len = elen;
            break;
        case 3:  //WLAN_EID_DS_PARAMS:
            elems->ds_params = pos;
            elems->ds_params_len = elen;
            break;
        case 4:  //WLAN_EID_CF_PARAMS:
            elems->cf_params = pos;
            elems->cf_params_len = elen;
            break;
        case 5:  //WLAN_EID_TIM:            
            if (elen >= sizeof(struct ieee80211_tim_ie)) {
                elems->tim = (void *)pos;
                elems->tim_len = elen;
            }
            break;
        case 6:  //WLAN_EID_IBSS_PARAMS:
            elems->ibss_params = pos;
            elems->ibss_params_len = elen;
            break;
        case 16:  //WLAN_EID_CHALLENGE:
            elems->challenge = pos;
            elems->challenge_len = elen;
            break;
        case 221:  //WLAN_EID_WPA:
            if (elen >= 4 && pos[0] == 0x00 && pos[1] == 0x50 &&
                pos[2] == 0xf2) {
                /* Microsoft OUI (00:50:F2) */
                if (pos[3] == 1) {
                    /* OUI Type 1 - WPA IE */
                    elems->wpa = pos;
                    elems->wpa_len = elen;
                } else if (elen >= 5 && pos[3] == 2) {
                    if (pos[4] == 0) {
                        elems->wmm_info = pos;
                        elems->wmm_info_len = elen;
                    } else if (pos[4] == 1) {
                        elems->wmm_param = pos;
                        elems->wmm_param_len = elen;
#if PS
                        *(pos-2) = 0xAB;   //NO QOS AP
#endif
                    }
                }
            }
            break;
        case 48:  //WLAN_EID_RSN:
            elems->rsn = pos;
            elems->rsn_len = elen;
            break;
        case 42:  //WLAN_EID_ERP_INFO:
            elems->erp_info = pos;
            elems->erp_info_len = elen;
            break;
        case 50:  //WLAN_EID_EXT_SUPP_RATES:
            elems->ext_supp_rates = pos;
            elems->ext_supp_rates_len = elen;
            break;
        case 45:  //WLAN_EID_HT_CAPABILITY:
            if (elen >= sizeof(struct ieee80211_ht_cap))
                elems->ht_cap_elem = (void *)pos;
            break;
      //  case 61:  //WLAN_EID_HT_EXTRA_INFO:
      //      if (elen >= sizeof(struct ieee80211_ht_addt_info))
       //         elems->ht_info_elem = (void *)pos;
      //      break;
        case 52:  //WLAN_EID_MESH_ID:
            elems->mesh_id = pos;
            elems->mesh_id_len = elen;
            break;
       // case 51:  //WLAN_EID_MESH_CONFIG:
        //    elems->mesh_config = pos;
        //    elems->mesh_config_len = elen;
            break;
        case 55:  //WLAN_EID_PEER_LINK:
            elems->peer_link = pos;
            elems->peer_link_len = elen;
            break;
        case 68:  //WLAN_EID_PREQ:
            elems->preq = pos;
            elems->preq_len = elen;
            break;
        case 69:  //WLAN_EID_PREP:
            elems->prep = pos;
            elems->prep_len = elen;
            break;
        case 70:  //WLAN_EID_PERR:
            elems->perr = pos;
            elems->perr_len = elen;
            break;
        case 37:  //WLAN_EID_CHANNEL_SWITCH:
            elems->ch_switch_elem = pos;
            elems->ch_switch_elem_len = elen;
            break;
        case 40:  //WLAN_EID_QUIET:
            if (!elems->quiet_elem) {
                elems->quiet_elem = pos;
                elems->quiet_elem_len = elen;
            }
            elems->num_of_quiet_elem++;
            break;
        case 7:  //WLAN_EID_COUNTRY:
            elems->country_elem = pos;
            elems->country_elem_len = elen;
            break;
        case 32:  //WLAN_EID_PWR_CONSTRAINT:
            elems->pwr_constr_elem = pos;
            elems->pwr_constr_elem_len = elen;
            break;
        default:
            break;
        }

        left -= elen;
        pos += elen;
    }
}
#endif

enum RT_80211_FRAME_TYPE {
    RT_80211_FRAME_TYPE_MANAGEMENT = 0,
    RT_80211_FRAME_TYPE_CNTR = 1,
    RT_80211_FRAME_TYPE_DATA = 2,
};

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,33))
struct ieee80211_qos_hdr {
	__le16 frame_control;
	__le16 duration_id;
	u8 addr1[6];
	u8 addr2[6];
	u8 addr3[6];
	__le16 seq_ctrl;
	__le16 qos_ctrl;
} __attribute__ ((packed));

#endif

typedef union   _CAPINFO_VALUE_STRUC    {
    struct  {
        u16  ESS:1;
        u16  IBSS:1;
        u16  CFPollable:1;
        u16  CFPollReq:1;
        u16  Privacy:1;
        u16  ShortPreable:1;
        u16  PBCC:1;
        u16  ChannelAgility:1;
        u16  Res1:2;
        u16  ShortSlotTime:1;
        u16  Res2:2;
        u16  DsssOfdm:1;
        u16  Res3:2;
    }   field;
    unsigned long           word;
}   CAPINFO_VALUE_STRUC, *PCAPINFO_VALUE_STRUC;

typedef union   _ERPINFO_VALUE_STRUC    {
    struct  {
        unsigned long  ExtRateLen:8;
        unsigned long  ErpInfo:8;
        unsigned long  Res:16;
    }   field;
    unsigned long           word;
} ERPINFO_VALUE_STRUC, *PERPINFO_VALUE_STRUC;




// QBSS LOAD information from QAP's BEACON/ProbeRsp
typedef struct _QOS_CONTROL {
#ifdef BIG_ENDIAN
    USHORT      Txop_QueueSize:8;
    USHORT      Rsv:1;
    USHORT      AckPolicy:2;
    USHORT      EOSP:1;
    USHORT      TID:4;
#else
    USHORT      TID:4;
    USHORT      EOSP:1;
    USHORT      AckPolicy:2;
    USHORT      Rsv:1;
    USHORT      Txop_QueueSize:8;
#endif
}  __attribute__((packed)) QOS_CONTROL, *PQOS_CONTROL;

typedef union   _AIFSN_VALUE_STRUC  {
    struct  {
        ULONG  AIFSN_BK:4;
        ULONG  AIFSN_BE:4;
        ULONG  AIFSN_VI:4;
        ULONG  AIFSN_VO:4;
        ULONG  Res:16;
    }   field;
    ULONG           word;
}    __attribute__((packed)) AIFSN_VALUE_STRUC, *PAIFSN_VALUE_STRUC;

typedef union   _CWMIN_VALUE_STRUC  {
    struct  {
        ULONG  CWMIN_BK:4;
        ULONG  CWMIN_BE:4;
        ULONG  CWMIN_VI:4;
        ULONG  CWMIN_VO:4;
        ULONG  Res:16;
    }   field;
    ULONG           word;
}    __attribute__((packed)) CWMIN_VALUE_STRUC, *PCWMIN_VALUE_STRUC;

typedef union   _CWMAX_VALUE_STRUC  {
    struct  {
        ULONG  CWMAX_BK:4;
        ULONG  CWMAX_BE:4;
        ULONG  CWMAX_VI:4;
        ULONG  CWMAX_VO:4;
        ULONG  Res:16;
    }   field;
    ULONG           word;
}    __attribute__((packed)) CWMAX_VALUE_STRUC, *PCWMAX_VALUE_STRUC;

typedef union   _TXOPBEBK_VALUE_STRUC   {
    struct  {
        USHORT  TXOP_BK;
        USHORT  TXOP_BE;
    }   field;
    ULONG           word;
}    __attribute__((packed)) TXOPBEBK_VALUE_STRUC, *PTXOPBEBK_VALUE_STRUC;

typedef union   _TXOPVOVI_VALUE_STRUC   {
    struct  {
        USHORT  TXOP_VI;
        USHORT  TXOP_VO;
    }   field;
    ULONG           word;
}    __attribute__((packed)) TXOPVOVI_VALUE_STRUC, *PTXOPVOVI_VALUE_STRUC;

/***********************************************************/
#define MAC_ADDR_LEN   6
#define MAX_LEN_OF_SHARE_KEY   16
#define MAX_LEN_OF_SUPPORTED_RATES        12    // 1, 2, 5.5, 11, 6, 9, 12, 18, 24, 36, 48, 54

//u8 BROADCAST_ADDR[MAC_ADDR_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
//u8 ZERO_MAC_ADDR[MAC_ADDR_LEN]  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#if 0
// value domain of MacHdr.subtype, which is b7..4 of the 1st-byte of MAC header
// Management frame
#define SUBTYPE_ASSOC_REQ           0
#define SUBTYPE_ASSOC_RSP           1
#define SUBTYPE_REASSOC_REQ         2
#define SUBTYPE_REASSOC_RSP         3
#define SUBTYPE_PROBE_REQ           4
#define SUBTYPE_PROBE_RSP           5
#define SUBTYPE_BEACON              8
#define SUBTYPE_ATIM                9
#define SUBTYPE_DISASSOC            10
#define SUBTYPE_AUTH                11
#define SUBTYPE_DEAUTH              12
#define SUBTYPE_ACTION              13
#endif

#define RATE_1                  0
#define RATE_2                  1
#define RATE_5_5                2
#define RATE_11                 3
#define RATE_6                  4   // OFDM
#define RATE_9                  5   // OFDM
#define RATE_12                 6   // OFDM
#define RATE_18                 7   // OFDM
#define RATE_24                 8   // OFDM
#define RATE_36                 9   // OFDM
#define RATE_48                 10  // OFDM
#define RATE_54                 11  // OFDM


void VariableSel(struct sci9211_priv *priv,u8 CH_NUM);

#endif /* sci9211_H */
