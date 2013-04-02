/*
 * Linux device driver for sci9211
 *
 * Based on the r8187 driver, which is:
 * Copyright 2005 Andrea Merello <andreamrl@tiscali.it>, et al.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/usb.h>
#include <linux/delay.h>
//#include <linux/version.h>
#include <linux/etherdevice.h>
#include <net/mac80211.h>
#include <asm/cacheflush.h>
#include <linux/time.h>  
#include <linux/kernel.h>  
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <mach/hardware.h>

#include "sci9211.h"
#include "rf.h"
#include "MAC_Control.h"

#define USB_MONITER 1  //for test I/O error

MODULE_AUTHOR("Thomas Luo <shbluo@sci-inc.com.cn>");
MODULE_AUTHOR("DongMei Ou <dmou@sci-inc.com.cn>");
MODULE_AUTHOR("Karin Zhang <kzhang@sci-inc.com.cn>");
MODULE_DESCRIPTION("sci9211 USB wireless driver");
MODULE_LICENSE("GPL");

#if DEBUG_ON
//int tx_data_num = 0;  //for debug
int tx_num = 0;                    //for debug
int tx_call_num = 0;  //for debug
bool data_flag = false;  //for debug data frame flag
int rx_data_flag = 0;
int prob_resp = 0;   //07252011 dmou for debug scanlist error
int rx_cb_num = 0;  //for debug
int rx_queue =0;   //for debug
#endif
static struct workqueue_struct * temperature_workqueue = NULL;

static struct usb_device_id sci9211_table[] __devinitdata = {
    {USB_DEVICE(0x2310, 0x5678)/*, .driver_info = SCI92118*/},
    {}
};

MODULE_DEVICE_TABLE(usb, sci9211_table);

static const struct ieee80211_rate sci921x_rates_1[] = {
    { .bitrate = 10, .hw_value = 0, },
    { .bitrate = 20, .hw_value = 3, },  //.hw_value_short = 1,},
    { .bitrate = 55, .hw_value = 5, },  //.hw_value_short = 1,},
    { .bitrate = 110, .hw_value = 7, },  //.hw_value_short = 1,},
    { .bitrate = 480, .hw_value = 8, },
    { .bitrate = 240, .hw_value = 9, },
    { .bitrate = 120, .hw_value = 10, },
    { .bitrate = 60, .hw_value = 11, },
    { .bitrate = 540, .hw_value = 12, },
    { .bitrate = 360, .hw_value = 13, },
    { .bitrate = 180, .hw_value = 14, },
    { .bitrate = 90, .hw_value = 15, },
};

static const struct ieee80211_rate sci921x_rates_2[] = {
    { .bitrate = 10, .hw_value = 0, },
    { .bitrate = 20, .hw_value = 2, },  //.hw_value_short = 0,},
    { .bitrate = 55, .hw_value = 4, },  //.hw_value_short = 0,},
    { .bitrate = 110, .hw_value = 6, },  //.hw_value_short = 0,},
    { .bitrate = 480, .hw_value = 8, },
    { .bitrate = 240, .hw_value = 9, },
    { .bitrate = 120, .hw_value = 10, },
    { .bitrate = 60, .hw_value = 11, },
    { .bitrate = 540, .hw_value = 12, },
    { .bitrate = 360, .hw_value = 13, },
    { .bitrate = 180, .hw_value = 14, },
    { .bitrate = 90, .hw_value = 15, },
};

static const struct ieee80211_rate sci921x_rates_3[] = {
    { .bitrate = 10, .hw_value = 0, },
    { .bitrate = 20, .hw_value = 1, },
    { .bitrate = 55, .hw_value = 2, },
    { .bitrate = 110, .hw_value = 3, },
    { .bitrate = 60, .hw_value = 4, },
    { .bitrate = 90, .hw_value = 5, },
    { .bitrate = 120, .hw_value = 6, },
    { .bitrate = 180, .hw_value = 7, },
    { .bitrate = 240, .hw_value = 8, },
    { .bitrate = 360, .hw_value = 9, },
    { .bitrate = 480, .hw_value = 10, },
    { .bitrate = 540, .hw_value = 11, },
};

static const struct ieee80211_channel sci921x_channels[] = {
    { .center_freq = 2412 },
    { .center_freq = 2417 },
    { .center_freq = 2422 },
    { .center_freq = 2427 },
    { .center_freq = 2432 },
    { .center_freq = 2437 },
    { .center_freq = 2442 },
    { .center_freq = 2447 },
    { .center_freq = 2452 },
    { .center_freq = 2457 },
    { .center_freq = 2462 },
    { .center_freq = 2467 },
    { .center_freq = 2472 },
    { .center_freq = 2484 },
};

#if 0
int sci_culc_frame_duration(struct ieee80211_local *local, size_t len,
                 int rate, int erp, int short_preamble)
{
    int dur;

    /* calculate duration (in microseconds, rounded up to next higher
     * integer if it includes a fractional microsecond) to send frame of
     * len bytes (does not include FCS) at the given rate. Duration will
     * also include SIFS.
     *
     * rate is in 100 kbps, so divident is multiplied by 10 in the
     * DIV_ROUND_UP() operations.
     */

    if (local->hw.conf.channel->band == IEEE80211_BAND_5GHZ || erp) {
        /*
         * OFDM:
         *
         * N_DBPS = DATARATE x 4
         * N_SYM = Ceiling((16+8xLENGTH+6) / N_DBPS)
         *  (16 = SIGNAL time, 6 = tail bits)
         * TXTIME = T_PREAMBLE + T_SIGNAL + T_SYM x N_SYM + Signal Ext
         *
         * T_SYM = 4 usec
         * 802.11a - 17.5.2: aSIFSTime = 16 usec
         * 802.11g - 19.8.4: aSIFSTime = 10 usec +
         *  signal ext = 6 usec
         */
        dur = 16; /* SIFS + signal ext */
        dur += 16; /* 17.3.2.3: T_PREAMBLE = 16 usec */
        dur += 4; /* 17.3.2.3: T_SIGNAL = 4 usec */
        dur += 4 * DIV_ROUND_UP((16 + 8 * (len + 4) + 6) * 10,
                    4 * rate); /* T_SYM x N_SYM */
    } else {
        /*
         * 802.11b or 802.11g with 802.11b compatibility:
         * 18.3.4: TXTIME = PreambleLength + PLCPHeaderTime +
         * Ceiling(((LENGTH+PBCC)x8)/DATARATE). PBCC=0.
         *
         * 802.11 (DS): 15.3.3, 802.11b: 18.3.4
         * aSIFSTime = 10 usec
         * aPreambleLength = 144 usec or 72 usec with short preamble
         * aPLCPHeaderLength = 48 usec or 24 usec with short preamble
         */
        dur = 10; /* aSIFSTime = 10 usec */
        dur += short_preamble ? (72 + 24) : (144 + 48);

        dur += DIV_ROUND_UP(8 * (len + 4) * 10, rate);
    }

    return dur;
}

u16 sci_generic_frame_duration(struct ieee80211_hw *hw,
                    struct ieee80211_vif *vif,
                    size_t frame_len,
                    /*struct ieee80211_rate *rate*/
                               u16 rate_id)
{
    struct ieee80211_local *local = hw_to_local(hw);
    //struct ieee80211_sub_if_data *sdata;
    struct sci9211_priv *priv = hw->priv;
    u16 dur;
    int erp;
    bool short_preamble = false;

    erp = 0;
    if (vif) {
        //sdata = vif_to_sdata(vif);
        short_preamble = priv->short_pre;  //sdata->vif.bss_conf.use_short_preamble;
        /*if (sdata->flags & IEEE80211_SDATA_OPERATING_GMODE)
            erp = rate->flags & IEEE80211_RATE_ERP_G;*/
    }

    dur = sci_culc_frame_duration(local, frame_len, rate_id, erp,
                       short_preamble);

    return cpu_to_le16(dur);
}
#endif

int array_compare(int len1,u8 array1[], int len2, u8 array2[])
{
    int ret = 0;

    //sci_deb_hex( " array1 ", array1,len1);  //added by dmou
    //sci_deb_hex( " array2 ", array2,len2);  //added by dmou
    while(len1>0&&len2>0)
    {
        if (array1[len1-1]!=array2[len2-1]){
            //INFO("%s array1[%d-1] 0x%x ",__func__,len1,array1[len1-1]);
            //INFO("%s array2[%d-1] 0x%x ",__func__,len2,array2[len2-1]);
            //INFO("%s ret -1 ",__func__);
            return -1;
        }
        len1--;
        len2--;
    }

    //INFO("%s ret %d ",__func__,ret);
    return ret;
}


int HWEEPROMReadRFBBMType(struct sci9211_priv *priv)
{
    unsigned short /*ver,*/RF_BB_M;
    unsigned long rftype,mactype;

    if(priv->ver == 0x01)
    {
         INFO("SCI.info: Read Ver Success: 0x%x \n", priv->ver);

         RF_BB_M = (unsigned short)HWEEPROMReadRF_BB_M(priv->hw_control);                                               //Read RF_BB_M from EEPROM
         INFO("SCI.info: RF_BB_M: 0x%x\n", RF_BB_M);

         if((RF_BB_M == 0x0C) || (RF_BB_M == 0x0D) )
             sci921x_iowrite32(priv,(__le32 *)0x0A04,0x8000FF32);  //FOR swm9001G1

         if((RF_BB_M == 0x01) || (RF_BB_M == 0x03) || (RF_BB_M == 0x05) || (RF_BB_M == 0x07)|| (RF_BB_M == 0x09)|| (RF_BB_M == 0x0B)|| (RF_BB_M == 0x0D)|| (RF_BB_M == 0x0F))
         {
            INFO("SCI.info: Read RF_BB_M Success:\n");

            INFO("SCI.info: Load S1032 RF and S9013 BaseBand:\n");
            priv->rftype = RFIC_SCIS1032;
            priv->mactype = BASEBAND_SCI9013;
            priv->modtype = 0x001B;
         }
         else if((RF_BB_M == 0x02) || (RF_BB_M == 0x04) || (RF_BB_M == 0x06)|| (RF_BB_M == 0x08)|| (RF_BB_M == 0x0A)|| (RF_BB_M == 0x0C)|| (RF_BB_M == 0x0E))
         {
            INFO("SCI.info: Read RF_BB_M Success:\n");

            INFO("SCI.info: Load S1032 RF and S9012 BaseBand:\n");
            priv->rftype = RFIC_SCIS1032;
            priv->mactype = BASEBAND_SCI9012;
            priv->modtype = 0x001B;
         }
         else
         {
            INFO("SCI.info: Read RF_BB_M Fail:\n");
            return -EINVAL;
         }
     }
     else
     {
        rftype = HWEEPROMReadRFType(priv->hw_control);
        INFO("SCI.info: RFType Read from EEPROM : 0x%lx\n",rftype);
        
        if(rftype == 0xFFFF)   //
            priv->rftype = RFIC_SCIS1022;      //No methed to read the EEPROM it must be SCISI1022
        if((rftype == 0x1032)||(rftype == 5))
            priv->rftype = RFIC_SCIS1032;
        INFO("SCI.info: Real RFType: 0x%x\n",priv->rftype);
        
        mactype = HWEEPROMReadMACType(priv->hw_control);
        if(mactype == 0x9012)
            priv->mactype = BASEBAND_SCI9012;
        else if(mactype == 0x9013)
            priv->mactype = BASEBAND_SCI9013;
        else
            priv->mactype = BASEBAND_SCI9011;
        INFO("SCI.info: MACType: 0x%x\n",priv->mactype);
        
        priv->modtype = HWEEPROMReadModuleType(priv->hw_control);
        INFO("SCI.info: ModType: 0x%x\n",priv->modtype);
     }

     return 0;
}
 
void HWEEPROMReadRssiRefer(struct sci9211_priv *priv)
{
    struct _EEPROM_STATE_STRUC state;

    state = HWEEPROMReadState(priv->hw_control);

    if(priv->ver == 0x01)
    {
        if(state.RX_POWER_CAL_DONE == 0x01)
        {
            priv->RX_POWER = (int8_t)((HWEEPROMRead(priv->hw_control, EEPROM_RSSI_REFERENCE_OFFSET)>>8)&0xFF);
            priv->AGC_WORD = (unsigned char)(HWEEPROMRead(priv->hw_control, EEPROM_RSSI_REFERENCE_OFFSET)&0xFF);
        }
        else
        {
            priv->RX_POWER = -66;
            priv->AGC_WORD = 0x64;
        }
    }
    else
    {
        priv->RX_POWER = -66;
        priv->AGC_WORD = 0x64;
    }

    INFO("%s priv->RX_POWER: 0x%x(%d) +++++\n", __func__,priv->RX_POWER,priv->RX_POWER);
    INFO("%s priv->AGC_WORD: %d  +++++\n", __func__,priv->AGC_WORD);
}

void HWEEPROMReadTemprature(struct sci9211_priv *priv)
{
    if(priv->ver == 0x01)
    {
        priv->T_CAL = (int8_t)HWEEPROMRead(priv->hw_control, EEPROM_T_CAL_OFFSET)& 0xFF;
        priv->T_SAMPLE = (unsigned short)HWEEPROMRead(priv->hw_control, EEPROM_T_SAMPLE_OFFSET);
    }else
    {
        priv->T_CAL = 38;
        priv->T_SAMPLE = 480;
    }
}
void HWInitTemperture(struct sci9211_priv *priv )
{
    unsigned int ret;
    unsigned int val = 0;
    unsigned int S_INITIAL = 0;

    //DEG("Enter func %s\n", __func__);
    HWEEPROMReadTemprature(priv);

    /*step 1 : set reg*/
    ret = sci921x_ioread32(priv,(__le32 *)0x090C);
    ret &= 0xFFFFDFEF;
    ret |= 0x00100000;
    sci921x_iowrite32(priv,(__le32 *)0x090C,ret);  //PHY_REG_CLK_EN '1' AD140_DIS "0" TSSIAD_GATE_EN "0"

    msleep(5);

    ret = sci921x_ioread32(priv,(__le32 *)0x0B88);
    ret |= 0x10;
    sci921x_iowrite32(priv,(__le32 *)0x0B88,ret);  //SCIADC140_CHSEL = "1"

    ret = sci921x_ioread32(priv,(__le32 *)0x0B3C);
    ret |= 0x2;
    sci921x_iowrite32(priv,(__le32 *)0x0B3C,ret);  //TEMP_SENSOR_EN "1"

    /*STEP 2 :*/
    val = sci921x_ioread32(priv,(__le32 *)0x0B18);
    val &= 0x1;

    if(val == 1)
    {     
        ret = sci921x_ioread32(priv,(__le32 *)0x0B3C);
        ret &= 0xFFFFFFFD;     
        sci921x_iowrite32(priv,(__le32 *)0x0B3C,ret);  //TEMP_SENSOR_EN "0"

        ret = sci921x_ioread32(priv,(__le32 *)0x0B18);
        ret &= 0xFFFFFFFE;     
        sci921x_iowrite32(priv,(__le32 *)0x0B18,ret); //TX_POWER_DATA_READY 0    
        S_INITIAL = sci921x_ioread32(priv,(__le32 *)0x0B44); 

        /*step 3 : culculate initial temperarure */
        priv->T_INITIAL = priv->T_CAL + (int)(S_INITIAL - priv->T_SAMPLE)/4;  
    }

    /*step 4 : set reg to be end */
    ret = sci921x_ioread32(priv,(__le32 *)0x090C);
    ret &= 0xFFEFFFFF;      //PHY_REG_CLK_EN 0
    ret |= 0x00002010;   //AD140_DIS 1; TSSIAD_GATE_EN 1
    sci921x_iowrite32(priv,(__le32 *)0x090C,ret);  

    INFO("%s priv->T_INITIAL: %d \n", __func__,priv->T_INITIAL);
    INFO("%s T_CAL %d  ! \n",__func__,priv->T_CAL);  
    INFO("%s T_SAMPLE %d  ! \n",__func__,priv->T_SAMPLE); 
}

#if 0

static void debug_work(struct work_struct *work)
{
    struct sci9211_priv *priv = container_of(work, struct sci9211_priv,
                    debug_work.work);
    unsigned int reg ;

    reg = sci921x_ioread32(priv,(__le32 *)0xE78);
    INFO("%s  0xE78 : 0x%x \n",__func__,reg);
}

int debug_timer_func(unsigned long data)
{
    struct sci9211_priv *priv = (struct sci9211_priv *)data;
    
    //DEG("Enter func %s\n", __func__);
    if (priv == NULL ) {
        ERR("calculate_temperature() called with NULL data\n");
        return -EINVAL;
    }

    queue_delayed_work(debug_workqueue,&priv->debug_work,0);

    //INFO("%s expires : %lu  ! \n",__func__,priv->cal_temp_timer.expires);

    priv->debug_timer.expires = jiffies + HZ/1000*100;
    mod_timer(&priv->debug_timer,priv->debug_timer.expires);

    return 0;
}
#endif

/*if the channel changed when it working except for scanning, call this function */
void VariableSel(struct sci9211_priv *priv,u8 CH_NUM)
{
    priv->RF_GAIN_NEW = priv->CHQAM64Gain.CH1_QAM64_GAIN;
    //INFO("%s chan : %d priv->RF_GAIN_NEW %d \n",__func__,CH_NUM,priv->RF_GAIN_NEW);

    if(CH_NUM == 1){
        priv->RF_TX_GAIN_INI = priv->CHQAM64Gain.CH1_QAM64_GAIN;
        priv->CH_DIFF_POWER = priv->CHDiffPower.CH1_DIFF_POWER;
    }else if(CH_NUM == 2){
        priv->RF_TX_GAIN_INI = priv->CHQAM64Gain.CH2_QAM64_GAIN;
        priv->CH_DIFF_POWER = priv->CHDiffPower.CH2_DIFF_POWER;
    }else if(CH_NUM == 3){
        priv->RF_TX_GAIN_INI = priv->CHQAM64Gain.CH3_QAM64_GAIN;
        priv->CH_DIFF_POWER = priv->CHDiffPower.CH3_DIFF_POWER;
    }else if(CH_NUM == 4){
        priv->RF_TX_GAIN_INI = priv->CHQAM64Gain.CH4_QAM64_GAIN;
        priv->CH_DIFF_POWER = priv->CHDiffPower.CH4_DIFF_POWER;
    }else if(CH_NUM == 5){
        priv->RF_TX_GAIN_INI = priv->CHQAM64Gain.CH5_QAM64_GAIN;
        priv->CH_DIFF_POWER = priv->CHDiffPower.CH5_DIFF_POWER;
    }else if(CH_NUM == 6){
        priv->RF_TX_GAIN_INI = priv->CHQAM64Gain.CH6_QAM64_GAIN;
        priv->CH_DIFF_POWER = priv->CHDiffPower.CH6_DIFF_POWER;
    }else if(CH_NUM == 7){
        priv->RF_TX_GAIN_INI = priv->CHQAM64Gain.CH7_QAM64_GAIN;
        priv->CH_DIFF_POWER = priv->CHDiffPower.CH7_DIFF_POWER;
    }else if(CH_NUM == 8){
        priv->RF_TX_GAIN_INI = priv->CHQAM64Gain.CH8_QAM64_GAIN;
        priv->CH_DIFF_POWER = priv->CHDiffPower.CH8_DIFF_POWER;
    }else if(CH_NUM == 9){
        priv->RF_TX_GAIN_INI = priv->CHQAM64Gain.CH9_QAM64_GAIN;
        priv->CH_DIFF_POWER = priv->CHDiffPower.CH9_DIFF_POWER;
    }else if(CH_NUM == 10){
        priv->RF_TX_GAIN_INI = priv->CHQAM64Gain.CH10_QAM64_GAIN;
        priv->CH_DIFF_POWER = priv->CHDiffPower.CH10_DIFF_POWER;
    }else if(CH_NUM == 11){
        priv->RF_TX_GAIN_INI = priv->CHQAM64Gain.CH11_QAM64_GAIN;
        priv->CH_DIFF_POWER = priv->CHDiffPower.CH11_DIFF_POWER;
    }else if(CH_NUM == 12){
        priv->RF_TX_GAIN_INI = priv->CHQAM64Gain.CH12_QAM64_GAIN;
        priv->CH_DIFF_POWER = priv->CHDiffPower.CH12_DIFF_POWER;
    }else if(CH_NUM == 13){
        priv->RF_TX_GAIN_INI = priv->CHQAM64Gain.CH13_QAM64_GAIN;
        priv->CH_DIFF_POWER = priv->CHDiffPower.CH13_DIFF_POWER;
    }else if(CH_NUM == 14){
        priv->RF_TX_GAIN_INI = priv->CHQAM64Gain.CH14_QAM64_GAIN;
        priv->CH_DIFF_POWER = priv->CHDiffPower.CH14_DIFF_POWER;
    }
    //INFO("%s priv->RF_TX_GAIN_INI %d \n",__func__,priv->RF_TX_GAIN_INI);
    //INFO("%s priv->CH_DIFF_POWER %d \n",__func__,priv->CH_DIFF_POWER);
}

/* adjust the TX power gain based on Temperture*/
void tx_power_alg(struct sci9211_priv *priv)
{
    int8_t TEMP_OFFSET,GAIN_OFFSET;
    int8_t TEMP_DEC_FINAL[7];
    int8_t TEMP_INC_FINAL[11];
    u8 GAIN_HIGH,GAIN_LOW;
    int i;

    /* confirm the TEMP_OFFSET based on CH_DIFF_POWER */
    if(priv->CH_DIFF_POWER < -77)
        TEMP_OFFSET = -10;
    else if(priv->CH_DIFF_POWER < -69)
        TEMP_OFFSET = -9;
    else if(priv->CH_DIFF_POWER < -61)
        TEMP_OFFSET = -8;
    else if(priv->CH_DIFF_POWER < -54)
        TEMP_OFFSET = -7;
    else if(priv->CH_DIFF_POWER < -46)
        TEMP_OFFSET = -6;
    else if(priv->CH_DIFF_POWER < -38)
        TEMP_OFFSET = -5;
    else if(priv->CH_DIFF_POWER < -31)
        TEMP_OFFSET = -4;
    else if(priv->CH_DIFF_POWER < -23)
        TEMP_OFFSET = -3;
    else if(priv->CH_DIFF_POWER < -15)
        TEMP_OFFSET = -2;
    else if(priv->CH_DIFF_POWER < -8)
        TEMP_OFFSET = -1;
    else if(priv->CH_DIFF_POWER < 8)
        TEMP_OFFSET = 0;
    else if(priv->CH_DIFF_POWER < 15)
        TEMP_OFFSET = 1;
    else if(priv->CH_DIFF_POWER < 23)
        TEMP_OFFSET = 2;
    else if(priv->CH_DIFF_POWER < 31)
        TEMP_OFFSET = 3;
    else if(priv->CH_DIFF_POWER < 38)
        TEMP_OFFSET = 4;
    else if(priv->CH_DIFF_POWER < 46)
        TEMP_OFFSET = 5;
    else if(priv->CH_DIFF_POWER < 54)
        TEMP_OFFSET = 6;
    else if(priv->CH_DIFF_POWER < 61)
        TEMP_OFFSET = 7;
    else if(priv->CH_DIFF_POWER < 69)
        TEMP_OFFSET = 8;
    else if(priv->CH_DIFF_POWER < 77)
        TEMP_OFFSET = 9;
    else
        TEMP_OFFSET = 10;

    //INFO("%s TEMP_OFFSET %d \n",__func__,TEMP_OFFSET);

    /* confirm the TEMP_DEC_X_FINAL */
    for(i=0;i<7;i++)
        TEMP_DEC_FINAL[i] = priv->TEMP_DEC[i] + TEMP_OFFSET;

    for(i=0;i<11;i++)
        TEMP_INC_FINAL[i] = priv->TEMP_INC[i] + TEMP_OFFSET;

    /* confirm the GAIN_OFFSET based on DIFF_CAL_CURRENT */
    if(priv->DIFF_CAL_CURRENT < TEMP_DEC_FINAL[6])
        GAIN_OFFSET = -7;
    else if(priv->DIFF_CAL_CURRENT < TEMP_DEC_FINAL[5])
        GAIN_OFFSET = -6;
    else if(priv->DIFF_CAL_CURRENT < TEMP_DEC_FINAL[4])
        GAIN_OFFSET = -5;
    else if(priv->DIFF_CAL_CURRENT < TEMP_DEC_FINAL[3])
        GAIN_OFFSET = -4;
    else if(priv->DIFF_CAL_CURRENT < TEMP_DEC_FINAL[2])
        GAIN_OFFSET = -3;
    else if(priv->DIFF_CAL_CURRENT < TEMP_DEC_FINAL[1])
        GAIN_OFFSET = -2;
    else if(priv->DIFF_CAL_CURRENT < TEMP_DEC_FINAL[0])
        GAIN_OFFSET = -1;
    else if(priv->DIFF_CAL_CURRENT < TEMP_INC_FINAL[0])
        GAIN_OFFSET = 0;
    else if(priv->DIFF_CAL_CURRENT < TEMP_INC_FINAL[1])
        GAIN_OFFSET = 1;
    else if(priv->DIFF_CAL_CURRENT < TEMP_INC_FINAL[2])
        GAIN_OFFSET = 2;
    else if(priv->DIFF_CAL_CURRENT < TEMP_INC_FINAL[3])
        GAIN_OFFSET = 3;
    else if(priv->DIFF_CAL_CURRENT < TEMP_INC_FINAL[4])
        GAIN_OFFSET = 4;
    else if(priv->DIFF_CAL_CURRENT < TEMP_INC_FINAL[5])
        GAIN_OFFSET = 5;
    else if(priv->DIFF_CAL_CURRENT < TEMP_INC_FINAL[6])
        GAIN_OFFSET = 6;
    else if(priv->DIFF_CAL_CURRENT < TEMP_INC_FINAL[7])
        GAIN_OFFSET = 7;
    else if(priv->DIFF_CAL_CURRENT < TEMP_INC_FINAL[8])
        GAIN_OFFSET = 8;
    else if(priv->DIFF_CAL_CURRENT < TEMP_INC_FINAL[9])
        GAIN_OFFSET = 9;
    else if(priv->DIFF_CAL_CURRENT < TEMP_INC_FINAL[10])
        GAIN_OFFSET = 10;
    else
        GAIN_OFFSET = 11;

    //INFO("%s GAIN_OFFSET %d \n",__func__,GAIN_OFFSET);

    /* confirm the GAIN_HIGH and GAIN_LOW */
    GAIN_HIGH = ( priv->RF_TX_GAIN_INI/2 ) * 2 + 1;
    GAIN_LOW = ( priv->RF_TX_GAIN_INI/2 ) * 2;
    //INFO("%s priv->RF_TX_GAIN_INI %d \n",__func__,priv->RF_TX_GAIN_INI);
    //INFO("%s GAIN_HIGH %d \n",__func__,GAIN_HIGH);
    //INFO("%s GAIN_LOW %d \n",__func__,GAIN_LOW);

    /* confirm the RF_GAIN_NEW */
    if(priv->RF_TX_GAIN_INI > 0x1F)
        priv->RF_GAIN_NEW = priv->RF_TX_GAIN_INI + GAIN_OFFSET;
    else if((priv->RF_TX_GAIN_INI == 0x1F)||(priv->RF_TX_GAIN_INI == 0x1E)){
        if(GAIN_OFFSET > 0)
            priv->RF_GAIN_NEW = 0x1F + GAIN_OFFSET;
        else
            priv->RF_GAIN_NEW = 0x1E + GAIN_OFFSET;
    }else{
        if(GAIN_OFFSET > 0)
            priv->RF_GAIN_NEW = GAIN_HIGH + GAIN_OFFSET;
        else
            priv->RF_GAIN_NEW = GAIN_LOW + GAIN_OFFSET;
    }

    //INFO("%s priv->RF_GAIN_NEW %d \n",__func__,priv->RF_GAIN_NEW);
}

/* call this function when AVG_RSSI changed */
void power_save_adjust(struct sci9211_priv *priv)
{
    int8_t RF_QAM64_GAIN;
    int8_t RF_QAM16_GAIN;
    int8_t RF_QPSK_GAIN;
    int8_t RF_BPSK_GAIN;
    int8_t RF_CCK_GAIN ;

    int8_t RF_QAM16_GAIN_TEMP;
    int8_t RF_QPSK_GAIN_TEMP;
    int8_t RF_BPSK_GAIN_TEMP;
    int8_t RF_CCK_GAIN_TEMP;

    int8_t RF_QAM16_GAIN_LIMIT;
    int8_t RF_QPSK_GAIN_LIMIT;
    int8_t RF_BPSK_GAIN_LIMIT;
    int8_t RF_CCK_GAIN_LIMIT;

    //INFO("%s priv->RF_GAIN_NEW %d \n",__func__,priv->RF_GAIN_NEW);
    /* confirm the TX GAIN based on RX singnal */
    if(priv->POWER_SAVE_ADJUST_EN == 0){
        RF_QAM64_GAIN = priv->RF_GAIN_NEW;
        RF_QAM16_GAIN = priv->RF_GAIN_NEW;
        RF_QPSK_GAIN  = priv->RF_GAIN_NEW;
        RF_BPSK_GAIN  = priv->RF_GAIN_NEW;
        RF_CCK_GAIN   = priv->RF_GAIN_NEW + 8;
    }else{
        if(priv->AVG_RSSI_current > priv->RX_POWER_LIMIT[0]){
            RF_QAM64_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[0];
            RF_QAM16_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[0];
            RF_QPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[0];
            RF_BPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[0];
            RF_CCK_GAIN   = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[0];
        }else if(priv->AVG_RSSI_current > priv->RX_POWER_LIMIT[1]){
            RF_QAM64_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[1];
            RF_QAM16_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[1];
            RF_QPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[1];
            RF_BPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[1];
            RF_CCK_GAIN   = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[1];
        }else if(priv->AVG_RSSI_current > priv->RX_POWER_LIMIT[2]){
            RF_QAM64_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[2];
            RF_QAM16_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[2];
            RF_QPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[2];
            RF_BPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[2];
            RF_CCK_GAIN   = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[2];
        }else if(priv->AVG_RSSI_current > priv->RX_POWER_LIMIT[3]){
            RF_QAM64_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[3];
            RF_QAM16_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[3];
            RF_QPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[3];
            RF_BPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[3];
            RF_CCK_GAIN   = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[3];
        }else if(priv->AVG_RSSI_current > priv->RX_POWER_LIMIT[4]){
            RF_QAM64_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[4];
            RF_QAM16_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[4];
            RF_QPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[4];
            RF_BPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[4];
            RF_CCK_GAIN   = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[4];
        }else if(priv->AVG_RSSI_current > priv->RX_POWER_LIMIT[5]){
            RF_QAM64_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[5];
            RF_QAM16_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[5];
            RF_QPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[5];
            RF_BPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[5];
            RF_CCK_GAIN   = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[5];
        }else if(priv->AVG_RSSI_current > priv->RX_POWER_LIMIT[6]){
            RF_QAM64_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[6];
            RF_QAM16_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[6];
            RF_QPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[6];
            RF_BPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[6];
            RF_CCK_GAIN   = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[6];
        }else if(priv->AVG_RSSI_current > priv->RX_POWER_LIMIT[7]){
            RF_QAM64_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[7];
            RF_QAM16_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[7];
            RF_QPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[7];
            RF_BPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[7];
            RF_CCK_GAIN   = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[7];
        }else if(priv->AVG_RSSI_current > priv->RX_POWER_LIMIT[8]){
            RF_QAM64_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[8];
            RF_QAM16_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[8];
            RF_QPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[8];
            RF_BPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[8];
            RF_CCK_GAIN   = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[8];
        }else if(priv->AVG_RSSI_current > priv->RX_POWER_LIMIT[9]){
            RF_QAM64_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[9];
            RF_QAM16_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[9];
            RF_QPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[9];
            RF_BPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[9];
            RF_CCK_GAIN   = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[9];
        }else if(priv->AVG_RSSI_current > priv->RX_POWER_LIMIT[10]){
            RF_QAM64_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[10];
            RF_QAM16_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[10];
            RF_QPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[10];
            RF_BPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[10];
            RF_CCK_GAIN   = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[10];
        }else if(priv->AVG_RSSI_current > priv->RX_POWER_LIMIT[11]){
            RF_QAM64_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[11];
            RF_QAM16_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[11];
            RF_QPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[11];
            RF_BPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[11];
            RF_CCK_GAIN   = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[11];
        }else if(priv->AVG_RSSI_current > priv->RX_POWER_LIMIT[12]){
            RF_QAM64_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[12];
            RF_QAM16_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[12];
            RF_QPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[12];
            RF_BPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[12];
            RF_CCK_GAIN   = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[12];
        }else if(priv->AVG_RSSI_current > priv->RX_POWER_LIMIT[13]){
            RF_QAM64_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[13];
            RF_QAM16_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[13];
            RF_QPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[13];
            RF_BPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[13];
            RF_CCK_GAIN   = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[13];
        }else if(priv->AVG_RSSI_current > priv->RX_POWER_LIMIT[14]){
            RF_QAM64_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[14];
            RF_QAM16_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[14];
            RF_QPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[14];
            RF_BPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[14];
            RF_CCK_GAIN   = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[14];
        }else if(priv->AVG_RSSI_current > priv->RX_POWER_LIMIT[15]){
            RF_QAM64_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[15];
            RF_QAM16_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[15];
            RF_QPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[15];
            RF_BPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[15];
            RF_CCK_GAIN   = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[15];
        }else if(priv->AVG_RSSI_current > priv->RX_POWER_LIMIT[16]){
            RF_QAM64_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[16];
            RF_QAM16_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[16];
            RF_QPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[16];
            RF_BPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[16];
            RF_CCK_GAIN   = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[16];
        }else if(priv->AVG_RSSI_current > priv->RX_POWER_LIMIT[17]){
            RF_QAM64_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[17];
            RF_QAM16_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[17];
            RF_QPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[17];
            RF_BPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[17];
            RF_CCK_GAIN   = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[17];
        }else {
            RF_QAM64_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[18];
            RF_QAM16_GAIN = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[18];
            RF_QPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[18];
            RF_BPSK_GAIN  = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[18];
            RF_CCK_GAIN   = priv->RF_GAIN_NEW - priv->RF_GAIN_STEP[18];
        }
    }

    /*INFO("%s RF_QAM64_GAIN %d ",__func__,RF_QAM64_GAIN);
    INFO("%s RF_QAM16_GAIN %d ",__func__,RF_QAM16_GAIN);
    INFO("%s RF_QPSK_GAIN  %d ",__func__,RF_QPSK_GAIN);
    INFO("%s RF_BPSK_GAIN  %d ",__func__,RF_BPSK_GAIN);
    INFO("%s RF_CCK_GAIN   %d ",__func__,RF_CCK_GAIN);*/
  
    /* confirm the RF Gain Limit */
    RF_QAM16_GAIN_TEMP = priv->RF_GAIN_NEW + 6;
    RF_QPSK_GAIN_TEMP = RF_QAM16_GAIN_TEMP + 6;
    RF_BPSK_GAIN_TEMP = RF_QPSK_GAIN_TEMP + 6;
    RF_CCK_GAIN_TEMP = RF_BPSK_GAIN_TEMP + 4;

    if(RF_QAM16_GAIN_TEMP > priv->RF_TX_GAIN_LIMIT)
        RF_QAM16_GAIN_LIMIT = priv->RF_TX_GAIN_LIMIT;
    else
        RF_QAM16_GAIN_LIMIT = RF_QAM16_GAIN_TEMP;

    if(RF_QPSK_GAIN_TEMP > priv->RF_TX_GAIN_LIMIT)
        RF_QPSK_GAIN_LIMIT = priv->RF_TX_GAIN_LIMIT;
    else
        RF_QPSK_GAIN_LIMIT = RF_QPSK_GAIN_TEMP;

    if(RF_BPSK_GAIN_TEMP > priv->RF_TX_GAIN_LIMIT)
        RF_BPSK_GAIN_LIMIT = priv->RF_TX_GAIN_LIMIT;
    else
        RF_BPSK_GAIN_LIMIT = RF_BPSK_GAIN_TEMP;

    if(RF_CCK_GAIN_TEMP > priv->RF_TX_GAIN_LIMIT)
        RF_CCK_GAIN_LIMIT = priv->RF_TX_GAIN_LIMIT;
    else
        RF_CCK_GAIN_LIMIT = RF_CCK_GAIN_TEMP;

    /*INFO("%s RF_QAM16_GAIN_LIMIT %d ",__func__,RF_QAM16_GAIN_LIMIT);
    INFO("%s RF_QPSK_GAIN_LIMIT  %d ",__func__,RF_QPSK_GAIN_LIMIT);
    INFO("%s RF_BPSK_GAIN_LIMIT  %d ",__func__,RF_BPSK_GAIN_LIMIT);
    INFO("%s RF_CCK_GAIN_LIMIT   %d ",__func__,RF_CCK_GAIN_LIMIT);*/

    /* confirm the RF Gain Final */
    if(RF_QAM64_GAIN > priv->RF_GAIN_NEW)
        priv->RF_QAM64_GAIN_FINAL = priv->RF_GAIN_NEW;
    else if(RF_QAM64_GAIN < 0)
        priv->RF_QAM64_GAIN_FINAL = 0x00;
    else
        priv->RF_QAM64_GAIN_FINAL = RF_QAM64_GAIN;

    if(RF_QAM16_GAIN > RF_QAM16_GAIN_LIMIT)
        priv->RF_QAM16_GAIN_FINAL = RF_QAM16_GAIN_LIMIT;
    else if(RF_QAM16_GAIN < 0)
        priv->RF_QAM16_GAIN_FINAL = 0x00;
    else
        priv->RF_QAM16_GAIN_FINAL = RF_QAM16_GAIN;

    if(RF_QPSK_GAIN > RF_QPSK_GAIN_LIMIT)
        priv->RF_QPSK_GAIN_FINAL = RF_QPSK_GAIN_LIMIT;
    else if(RF_QPSK_GAIN < 0)
        priv->RF_QPSK_GAIN_FINAL = 0x00;
    else
        priv->RF_QPSK_GAIN_FINAL = RF_QPSK_GAIN;

    if(RF_BPSK_GAIN > RF_BPSK_GAIN_LIMIT)
        priv->RF_BPSK_GAIN_FINAL = RF_BPSK_GAIN_LIMIT;
    else if(RF_BPSK_GAIN < 0)
        priv->RF_BPSK_GAIN_FINAL = 0x00;
    else
        priv->RF_BPSK_GAIN_FINAL = RF_BPSK_GAIN;

    if(RF_CCK_GAIN > RF_CCK_GAIN_LIMIT)
        priv->RF_CCK_GAIN_FINAL = RF_CCK_GAIN_LIMIT;
    else if(RF_CCK_GAIN < 0)
        priv->RF_CCK_GAIN_FINAL = 0x00;
    else
        priv->RF_CCK_GAIN_FINAL = RF_CCK_GAIN;

    /*INFO("%s priv->RF_QAM64_GAIN_FINAL %d ",__func__,priv->RF_QAM64_GAIN_FINAL);
    INFO("%s priv->RF_QAM16_GAIN_FINAL %d ",__func__,priv->RF_QAM16_GAIN_FINAL);
    INFO("%s priv->RF_QPSK_GAIN_FINAL  %d ",__func__,priv->RF_QPSK_GAIN_FINAL);
    INFO("%s priv->RF_BPSK_GAIN_FINAL  %d ",__func__,priv->RF_BPSK_GAIN_FINAL);
    INFO("%s priv->RF_CCK_GAIN_FINAL   %d ",__func__,priv->RF_CCK_GAIN_FINAL);*/

}

#if 1
//2012.7.16 for temperature calculate
static void sci9211_temperture_work(struct work_struct *work)
{
    struct sci9211_priv *priv = container_of(work, struct sci9211_priv,
                    temperature_work);
    unsigned int reg ;
    unsigned int S_CURRENT = 0;
    int val = 0;

    //DEG("Enter func %s\n", __func__);
    mutex_lock(&priv->conf_mutex);

    /*Step 1 : set regester*/
    reg = sci921x_ioread32(priv,(__le32 *)0x90C);
    reg |= 0x00100000;      //PHY_REG_CLK_EN 1
    reg &= 0xFFFFDFEF;   //AD140_DIS 0; TSSIAD_GATE_EN 0
    sci921x_iowrite32(priv,(__le32 *)0x90C,reg);

    msleep(5);

    reg = sci921x_ioread32(priv,(__le32 *)0xB88);
    reg |= 0x00000010;      //SCIADC140_CHSEL 1
    sci921x_iowrite32(priv,(__le32 *)0xB88,reg);

    reg = sci921x_ioread32(priv,(__le32 *)0xB3C);
    reg |= 0x00000002;   //TEMP_SENSOR_EN 1
    sci921x_iowrite32(priv,(__le32 *)0xB3C,reg);

    /*Step 2 : set regester*/
    val = sci921x_ioread32(priv,(__le32 *)0xB18);
    val &= 0x1;

    if(val == 1)
    {
        reg = sci921x_ioread32(priv,(__le32 *)0xB3C);
        reg &= 0xFFFFFFFD;        //TEMP_SENSOR_EN 0
        sci921x_iowrite32(priv,(__le32 *)0xB3C,reg);

        reg = sci921x_ioread32(priv,(__le32 *)0xB18);
        reg &= 0xFFFFFFFE;     //TX_POWER_DATA_READY 0
        sci921x_iowrite32(priv,(__le32 *)0xB18,reg);

        S_CURRENT = sci921x_ioread32(priv,(__le32 *)0xB44);

        /*Step 3 : culculate diff value*/
        priv->T_CURRENT = priv->T_CAL + (int)(S_CURRENT - priv->T_SAMPLE)/4;
        priv->DIFF_INI_CURRENT = priv->T_CURRENT - priv->T_INITIAL;
        priv->DIFF_CAL_CURRENT = priv->T_CURRENT - priv->T_CAL;
    }

    /*Step 4 : set regester*/
    reg = sci921x_ioread32(priv,(__le32 *)0x90C);
    reg &= 0xFFEFFFFF;      //PHY_REG_CLK_EN 0
    reg |= 0x00002010;   //AD140_DIS 1; TSSIAD_GATE_EN 1
    sci921x_iowrite32(priv,(__le32 *)0x90C,reg);

    //INFO("%s T_INITIAL %d  ! \n",__func__,priv->T_INITIAL);  
    //INFO("%s T_CAL %d  ! \n",__func__,priv->T_CAL);  
    //INFO("%s T_SAMPLE %d  ! \n",__func__,priv->T_SAMPLE);  
    //INFO("%s S_CURRENT %d  ! \n",__func__,S_CURRENT);  
    //INFO("%s DIFF_CAL_CURRENT %d  ! \n",__func__,priv->DIFF_CAL_CURRENT);  
    //INFO("%s DIFF_INI_CURRENT %d  ! \n",__func__,priv->DIFF_INI_CURRENT);  
    //INFO("%s T_CURRENT %d  ! \n",__func__,priv->T_CURRENT);  

    mutex_unlock(&priv->conf_mutex);

    tx_power_alg(priv);

    //DEG("Exit func %s\n", __func__);
}

int calculate_temperature(unsigned long data)
{
    struct sci9211_priv *priv = (struct sci9211_priv *)data;
    
    //DEG("Enter func %s\n", __func__);
    if (priv == NULL ) {
        ERR("calculate_temperature() called with NULL data\n");
        return -EINVAL;
    }

    //2012.7.16 for temperature calculate
    /* params information for calculate temperarure is only available by
     * reading a register in the device. We are in interrupt mode
     * here, thus queue the skb and finish on a work queue. */
    queue_work(temperature_workqueue,&priv->temperature_work);

    //INFO("%s expires : %lu  ! \n",__func__,priv->cal_temp_timer.expires);

    if(priv->assoc)
        mod_timer(&priv->cal_temp_timer,jiffies + HZ*2);

    //DEG("Exit func %s\n", __func__);

    return 0;
}
#endif

int calculate_rssi(struct sci9211_priv *priv,int8_t RSSI)
{
    unsigned char LNA_GAIN;
    int8_t DIFF_GAIN_PER_DEGREE;
    unsigned char VGA_GAIN;
    int DIFF_TEMP_GAIN;
    int TOTAL_GAIN;
    unsigned char REF_LNA_GAIN;
    unsigned char REF_VGA_GAIN;
    unsigned char REF_TOTAL_GAIN;
    int RECEIVE_POWER;

    //INFO("%s IN RSSI: %d  +++++\n", __func__,RSSI);

    if(((RSSI >> 5)&0x7) == 0x3)
    {
        LNA_GAIN = 42;
        DIFF_GAIN_PER_DEGREE = 0;  //0.0231;
    }
    else if(((RSSI >> 5)&0x7) == 0x2)
    {
        LNA_GAIN = 23;
        DIFF_GAIN_PER_DEGREE = 0;  //0.0462;
    }
    else
    {
        LNA_GAIN = 0;
        DIFF_GAIN_PER_DEGREE = 0;  //0.0154;
    }

    VGA_GAIN = (RSSI & 0x1F)*2;
    DIFF_TEMP_GAIN = priv->DIFF_CAL_CURRENT * DIFF_GAIN_PER_DEGREE;
    TOTAL_GAIN = LNA_GAIN + VGA_GAIN - DIFF_TEMP_GAIN;

    if(((priv->AGC_WORD >> 5)&0x7) == 0x3)
        REF_LNA_GAIN = 42;
    else if(((priv->AGC_WORD >> 5)&0x7) == 0x2)
        REF_LNA_GAIN = 23;
    else
        REF_LNA_GAIN = 0;

    REF_VGA_GAIN = (priv->AGC_WORD & 0x1F)*2;
    REF_TOTAL_GAIN = REF_LNA_GAIN + REF_VGA_GAIN;
    RECEIVE_POWER = priv->RX_POWER - (TOTAL_GAIN - REF_TOTAL_GAIN);

    //INFO("%s RECEIVE_POWER %d  ! \n",__func__,RECEIVE_POWER);  

    return RECEIVE_POWER;  
}

static u8 tx_rate_transfer_0(u8 rate_id,u8 short_pre)
{
    u8 data_tx_rate;

    switch(rate_id){
    case RATE_1:(data_tx_rate) =0 ;break;
    case RATE_2:(short_pre==1)?((data_tx_rate) =3) :((data_tx_rate) =2);break;
    case RATE_5_5:(short_pre==1)?((data_tx_rate) =5) :((data_tx_rate) =4);break;
    case RATE_11:(short_pre==1)?((data_tx_rate) =7 ):((data_tx_rate) =6);break;
    case RATE_6:(data_tx_rate) =11 ;break;
    case RATE_9:(data_tx_rate) =15 ;break;
    case RATE_12:(data_tx_rate) =10 ;break;
    case RATE_18:(data_tx_rate) =14 ;break;
    case RATE_24:(data_tx_rate) =9 ;break;
    case RATE_36:(data_tx_rate) =13 ;break;
    case RATE_48:(data_tx_rate) =8 ;break;
    case RATE_54:(data_tx_rate) =12;break;
    default:(data_tx_rate)=0;
    }

    return data_tx_rate;
}

static u8 tx_rate_transfer(u8 rate_id)
{
    u8 data_tx_rate;

    switch(rate_id){
    case RATE_1:(data_tx_rate) =0 ;break;
    case RATE_2:(data_tx_rate) =1;break;
    case RATE_5_5:(data_tx_rate) =2;break;
    case RATE_11:(data_tx_rate) =3 ;break;
    case RATE_6:(data_tx_rate) =4 ;break;
    case RATE_9:(data_tx_rate) =5 ;break;
    case RATE_12:(data_tx_rate) =6 ;break;
    case RATE_18:(data_tx_rate) =7 ;break;
    case RATE_24:(data_tx_rate) =8 ;break;
    case RATE_36:(data_tx_rate) =9 ;break;
    case RATE_48:(data_tx_rate) =10 ;break;
    case RATE_54:(data_tx_rate) =11;break;
    default:(data_tx_rate)=0;
    }

    return data_tx_rate;
}

static u8 rx_rate_transfer(u8 rate_id)
{
    u8 rx_rate = 0;
    //convert rate
    switch(rate_id){
    case 0:rx_rate = RATE_1;break;
    case 2:rx_rate = RATE_2;break;
    case 3:rx_rate = RATE_2;break;
    case 4:rx_rate = RATE_5_5;break;
    case 5:rx_rate = RATE_5_5;break;
    case 6:rx_rate = RATE_11;break;
    case 7:rx_rate = RATE_11;break;
    case 11:rx_rate = RATE_6;break;
    case 15:rx_rate = RATE_9;break;
    case 10:rx_rate = RATE_12;break;
    case 14:rx_rate = RATE_18;break;
    case 9:rx_rate = RATE_24;break;
    case 13:rx_rate = RATE_36;break;
    case 8:rx_rate = RATE_48;break;
    case 12:rx_rate = RATE_54;break;
    default:;
    }

    return rx_rate;
}

#if 0
static void sci9211_iowrite_async_cb(struct urb *urb)
{

    kfree(urb->context);
    usb_free_urb(urb);
}

static void sci9211_iowrite_async(struct sci9211_priv *priv, __le16 addr,
                  void *data, u16 len)
{

    struct usb_ctrlrequest *dr;
    struct urb *urb;
    struct sci9211_async_write_data {
        u8 data[4];
        struct usb_ctrlrequest dr;
    } *buf;
    int rc;

    buf = kmalloc(sizeof(*buf), GFP_ATOMIC);
    if (!buf)
        return;

    urb = usb_alloc_urb(0, GFP_ATOMIC);
    if (!urb) {
        kfree(buf);
        return;
    }

    dr = &buf->dr;

    dr->bRequestType = SCI9211_REQT_WRITE;
    dr->bRequest = SCI9211_REQ_SET_REG;
    dr->wValue = addr;
    dr->wIndex = 0;
    dr->wLength = cpu_to_le16(len);

    memcpy(buf, data, len);

    usb_fill_control_urb(urb, priv->udev, usb_sndctrlpipe(priv->udev, 0),
                 (unsigned char *)dr, buf, len,
                 sci9211_iowrite_async_cb, buf);
    rc = usb_submit_urb(urb, GFP_ATOMIC);
    if (rc < 0) {
        kfree(buf);
        usb_free_urb(urb);
    }
}

static inline void sci921x_iowrite32_async(struct sci9211_priv *priv,
                       __le32 *addr, u32 val)
{
    __le32 buf = cpu_to_le32(val);

    sci9211_iowrite_async(priv, cpu_to_le16((unsigned long)addr),
                  &buf, sizeof(buf));
}

void sci9211_write_phy(struct ieee80211_hw *dev, u8 addr, u32 data)
{

    data <<= 8;
    data |= addr | 0x80;

    msleep(1);
}
#endif

//06032011 dmou
static inline void ChangeOctets(unsigned char data[], int len) 
{
    int a;
    int num = len/2;
    unsigned char temp;

    for(a = 0; a < num; a++)
    {
        temp = data[a];
        data[a] = data[len-1-a];
        data[len-1-a] = temp;
    }
}

static void sci9211_add_shared_key(struct sci9211_priv *priv,struct ieee80211_key_conf *key,u8 CipherAlg)
{
    int word = 0;
    s8 KeyIdx = key->keyidx;
    u8 *Key = key->key;
	
    DEG("Enter func %s\n", __func__);
    if (CipherAlg == SCI9211_SEC_ALGO_AES)     //special for sci
        ChangeOctets(Key, MAX_LEN_OF_SHARE_KEY);

    HWSetKeyAddr(priv->hw_control, KeyIdx, Key);

    word = HWGetWmmKeyType(priv->hw_control);

    switch (CipherAlg){
    case SCI9211_SEC_ALGO_NONE:
        word |= 0x0&0x7;  //PkeyType
        word |= 0x0<<3&0x38;   //GkeyType
        break;
    case SCI9211_SEC_ALGO_WEP40:
        word |= 0x24;
        word &= 0xFFFFFFE4;
        break;
    case SCI9211_SEC_ALGO_WEP104:
        word |= 0x2D;
        word &= 0xFFFFFFED;
        break;
    case SCI9211_SEC_ALGO_TKIP:
        if(key->flags & IEEE80211_KEY_FLAG_PAIRWISE)         //unicast
        {
            //printk(KERN_INFO " unicast\n");
            word |= 0x2;
            word &= 0xFFFFFFFA;
        }
        else                                                    //multicast
        {
            //printk(KERN_INFO " multi \n");
            word |= 0x10;
            word &= 0xFFFFFFD7;
        }
        break;
    case SCI9211_SEC_ALGO_AES:
        if(key->flags & IEEE80211_KEY_FLAG_PAIRWISE)
        {
            //printk(KERN_INFO " unicast\n");
            word |= 0x1;
            word &= 0xFFFFFFF9;
        }
        else
        {
            //printk(KERN_INFO " multi\n");
            word |= 0x8;
            word &= 0xFFFFFFCF;
        }
        break;
    default:
        break;
    }

    word |= 0x40;
    HWSetWmmKeyType(priv->hw_control,word);
    DEG("Exit func %s\n", __func__);
    //sci_print_reg(priv);
}

static void sci9211_add_shared_key_wapi(struct sci9211_priv *priv,struct ieee80211_key_conf *key,u8 CipherAlg)
{
    int word = 0;
    s8 KeyIdx ; 
    u8 Key[16];
	u8 TempMicKey[16];  
	
	DEG("Enter func %s\n", __func__);
    if(CipherAlg == SCI9211_SEC_ALGO_SMS4){
        memcpy(Key,&key->key,16);
        if(key->key[16] != 0)
            memcpy(TempMicKey, &key->key[16], 16);
    }else{
        return;
    }
    ChangeOctets(Key, 16);
    ChangeOctets(TempMicKey, 16);
    
    if(key->flags & IEEE80211_KEY_FLAG_PAIRWISE)
        KeyIdx = 0;
    else
        KeyIdx = 1;

    HWSetKeyAddr(priv->hw_control, KeyIdx, Key);
    HWSetKeyAddr(priv->hw_control, KeyIdx+ 2, TempMicKey);

    word = HWGetWmmKeyType(priv->hw_control);
    switch (CipherAlg){
    case SCI9211_SEC_ALGO_SMS4:
        if(key->flags & IEEE80211_KEY_FLAG_PAIRWISE)
        {
            //printk(KERN_INFO " unicast\n");
            word |= 0x3;
            word &= 0xFFFFFFFB;
        }
        else
        {
            //printk(KERN_INFO " multi\n");
            word |= 0x18;
            word &= 0xFFFFFFDF;
        }
        
        word |= 0x100;  //set bit_8
        break;

    default:;
    }

    word |= 0x40;

    HWSetWmmKeyType(priv->hw_control,word);

    sci_print_reg(priv);
    DEG("Exit func %s\n", __func__);
}


void sci9211_rm_shared_key(struct sci9211_priv *priv,s8 KeyIdx)
{
    unsigned int val;

    DEG("Enter func %s\n", __func__);
    HWResetKeyAddr(priv->hw_control,KeyIdx);

    val = HWGetWmmKeyType(priv->hw_control);
    val &= 0xBF;      // turrn off   the valid bit
    HWSetWmmKeyType(priv->hw_control,val);
    priv->wapi = 0;  //06032011 dmou
    DEG("Exit func %s\n", __func__);
}

//for Qos
void sci9211_wmm_set(struct sci9211_priv *priv,bool enable)
{
    unsigned int val;

    val = HWGetWmmKeyType(priv->hw_control);
    if (enable) {
        val |= 0x80;      // turrn on   the WMM_EN bit
    }else
        val &= 0x7F;      // turrn off   the WMM_EN bit

    HWSetWmmKeyType(priv->hw_control,val);
}

static void sci9211_work(struct work_struct *work)  //dmou 05042011
{
    /* The SCI9211 returns the retry count through register 0x0914. In
     * addition, it appears to be a cumulative retry count, not the
     * value for the current TX packet. When multiple TX entries are
     * queued, the retry count will be valid for the last one in the queue.
     * The "error" should not matter for purposes of rate setting. */
    struct sci9211_priv *priv = container_of(work, struct sci9211_priv,
                    work.work);
    struct ieee80211_tx_info *info;
    struct ieee80211_hw *dev = priv->dev;
    u16 tmp =0;
#if MORE_AP
    unsigned int ret;
#endif

    mutex_lock(&priv->conf_mutex);

    //if(priv->sbssid_flag)
    if(priv->auth_flag)
        HWSetBSSIDAddr(priv->hw_control,priv->bssid);  //11222011 dmou

#if MORE_AP
    if(priv->auth_flag)
    {
        //sci921x_iowrite32(priv,(__le32 *)0x0900,0x00000007);
        if((priv->modtype == 0x001B) ||( priv->modtype == 0x001C))
        {
            ret = sci921x_ioread32(priv,(__le32 *)0x0B00);
            ret &= 0xFF80FFFF;
            ret |= 0x00600000; 
            sci921x_iowrite32(priv,(__le32 *)0x0B00,ret);  //R_ED_CTL_WORD "0X68"
            INFO("%s  AUTH 0x0B00  : 0x%x \n",__func__,ret);
        }
    }
#endif

    while (skb_queue_len(&priv->tx_status.queue) > 0) 
    {
        struct sk_buff *old_skb;

        old_skb = skb_dequeue(&priv->tx_status.queue);
        info = IEEE80211_SKB_CB(old_skb);
        info->status.rates[0].count = tmp;
        ieee80211_tx_status_irqsafe(dev, old_skb);
    }
    mutex_unlock(&priv->conf_mutex);
}

static void sci9211_tx_cb(struct urb *urb)
{
    struct sk_buff *skb = (struct sk_buff *)urb->context;
    struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
    struct ieee80211_hw *hw = info->rate_driver_data[0];
    struct sci9211_priv *priv = hw->priv;
    struct ieee80211_hdr *ie80211_hdr = (struct ieee80211_hdr *)info->rate_driver_data[2];

#if DEBUG_ON
    tx_call_num ++;    //for debug
#endif

    //DEG("Enter func %s \n", __func__);

#if MORE_AP
    if((((ie80211_hdr->frame_control >> 4) & 0xf) == 4) && (((ie80211_hdr->frame_control >> 2) & 0x3) == 0)) 
    {
        INFO("%s TX 04 frame !\n",__func__);
    }
#endif

    if(((((ie80211_hdr->frame_control >> 4) & 0xf) & 0x8) == 1) && (((ie80211_hdr->frame_control >> 2) & 0x3) == 2)) 
    {
        skb_push(skb,2);
        memcpy(skb->data,info->rate_driver_data[2],sizeof(struct ieee80211_qos_hdr));
    }else{
        memcpy(skb->data,info->rate_driver_data[2],sizeof(struct ieee80211_hdr));  //dmou 02162011
    }
    skb_pull(skb, sizeof(struct _HW_TXD_STRUC));  //12302011 dmou
    ieee80211_tx_info_clear_status(info);

    if (!(urb->status) && !(info->flags & IEEE80211_TX_CTL_NO_ACK)) 
        info->flags |= IEEE80211_TX_STAT_ACK;

    /* Retry information for the sci9211 is only available by
     * reading a register in the device. We are in interrupt mode
     * here, thus queue the skb and finish on a work queue. */
    skb_queue_tail(&priv->tx_status.queue, skb);
    ieee80211_queue_delayed_work(hw, &priv->work, 0);

    //DEG("Exit func %s\n", __func__);
}


#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37)) 
static int sci9211_tx(struct ieee80211_hw *dev, struct sk_buff *skb)
#else
static void sci9211_tx(struct ieee80211_hw *dev, struct sk_buff *skb)
#endif
{
    struct sci9211_priv *priv = dev->priv;
    struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
    struct ieee80211_hdr *pieee80211hdr = (struct ieee80211_hdr *)skb->data;
    struct ieee80211_hdr ieee80211hdr;
    struct  _HW_TXD_STRUC tx_hdr;
    unsigned int ep;
    void *buf;
    struct urb *urb;
    __le16 rts_dur = 0;
    u32 flags;
    int rc;
    struct ieee80211_qos_hdr buf_hdr;
    u16 flag1 = 0;
	u16 flag2 = 0;  //wapi
    u8 *txbuff  = (u8 *)skb->data;  //wapi
	
    //DEG("Enter func %s\n", __func__);

    //INFO(" skb-0_len %d \n",skb->len);
    //sci_deb_hex( " skb-0  ", skb->data,skb->len);  //added by dmou

    memcpy(&ieee80211hdr,pieee80211hdr,sizeof(struct ieee80211_hdr));
    memcpy(&buf_hdr,pieee80211hdr,sizeof(struct ieee80211_qos_hdr));   //qos_data hdr
    memcpy(&tx_hdr,pieee80211hdr,sizeof(struct _HW_TXD_STRUC));

    if((((ieee80211hdr.frame_control >> 2) & 0x3)== 0)&&(((ieee80211hdr.frame_control >> 4) & 0xf) == 0x4))
    {
        INFO("@@@@@@@@@ %s TX PROB REQUEST \n",__func__);
#if MORE_AP
        priv->scan_flag = 1;   //dmou
        
    }else{
        priv->scan_flag = 0;   //dmou
#endif
    }

    if((((ieee80211hdr.frame_control >> 2) & 0x3)== 0)&&(((ieee80211hdr.frame_control >> 4) & 0xf) == 0xB))
    {
        INFO("@@@@@@@@@ %s TX AUTH \n",__func__);
        memcpy(priv->bssid,ieee80211hdr.addr3,6);  //for SETBSSID
        priv->auth_flag = 1;
    }else{
        priv->auth_flag = 0;
    }
        
    if((((ieee80211hdr.frame_control >> 2) & 0x3)== 0)&&(((ieee80211hdr.frame_control >> 4) & 0xf) == 0x0))
    {
        INFO("@@@@@@@@@ %s TX ASSOCIATION \n",__func__);
        priv->assoc_flag = 1;
    }else
        priv->assoc_flag = 0;

    if((((ieee80211hdr.frame_control >> 2) & 0x3)== 0)&&(((ieee80211hdr.frame_control >> 4) & 0xf) == 0xa))
    {
        INFO("@@@@@@@@@ %s TX DEASSOCIATION :<\n",__func__);
    }

    if((((ieee80211hdr.frame_control >> 2) & 0x3)== 0)&&(((ieee80211hdr.frame_control >> 4) & 0xf) == 0xc))
    {
        INFO("@@@@@@@@@ %s TX DEAUTH :< \n",__func__);
    }

#if PS
    if((priv->assoc)&&(((ieee80211hdr.frame_control >> 2) & 0x3)== 2)){
        priv->tx_data_num ++;
    }
#endif

    urb = usb_alloc_urb(0, GFP_ATOMIC);
    if (!urb) {
        kfree_skb(skb);
	ERR("%s NO MEM for urb\n", __func__);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37))
	return -ENOMEM;
#else
        return;
#endif
    }

    flags = skb->len;
    flags |= SCI921X_TX_DESC_FLAG_NO_ENC;

    flags |= ieee80211_get_tx_rate(dev, info)->hw_value << 24;
    if (ieee80211_has_morefrags(((struct ieee80211_hdr *)skb->data)->frame_control))
        flags |= SCI921X_TX_DESC_FLAG_MOREFRAG;

    if (info->control.rates[0].flags & IEEE80211_TX_RC_USE_RTS_CTS) {
        flags |= SCI921X_TX_DESC_FLAG_RTS;
        flags |= ieee80211_get_rts_cts_rate(dev, info)->hw_value << 19;
        rts_dur = ieee80211_rts_duration(dev, priv->vif,
                         skb->len, info);

    } else if (info->control.rates[0].flags & IEEE80211_TX_RC_USE_CTS_PROTECT) {
        flags |= SCI921X_TX_DESC_FLAG_CTS;
        flags |= ieee80211_get_rts_cts_rate(dev, info)->hw_value << 19;
    }

    tx_hdr.Reserved1 = 0;  //15;   //TX

    //convert tx_rate
    if(((ieee80211hdr.frame_control >> 2) & 0x3) == 2)
    {
        //printk(KERN_INFO "%s TX: info->control.rates[0].idx: %d,\n",__func__,info->control.rates[0].idx);
        if(priv->mactype == BASEBAND_SCI9013 || priv->mactype == BASEBAND_SCI9012 ){
            switch(info->control.rates[0].idx){
            case RATE_1:(tx_hdr.DataTxRate) =4 ;break;       \
            case RATE_2:(tx_hdr.DataTxRate) =4 ;break;       \
            case RATE_5_5:(tx_hdr.DataTxRate) =4 ;break;     \
            case RATE_11:(tx_hdr.DataTxRate) =4 ;break;      \
            case RATE_6:(tx_hdr.DataTxRate) =4 ;break;       \
            case RATE_9:(tx_hdr.DataTxRate) =5 ;break;       \
            case RATE_12:(tx_hdr.DataTxRate) =6 ;break;      \
            case RATE_18:(tx_hdr.DataTxRate) =7 ;break;      \
            case RATE_24:(tx_hdr.DataTxRate) =8 ;break;      \
            case RATE_36:(tx_hdr.DataTxRate) =9 ;break;      \
            case RATE_48:(tx_hdr.DataTxRate) =10 ;break;     \
            case RATE_54:(tx_hdr.DataTxRate) =11;break;      \
            default:(tx_hdr.DataTxRate)=0;                   \
            }
        }else{
            switch(info->control.rates[0].idx){
            case RATE_1:(tx_hdr.DataTxRate) =11 ;break;      \
            case RATE_2:(tx_hdr.DataTxRate) =11 ;break;      \
            case RATE_5_5:(tx_hdr.DataTxRate) =11 ;break;    \
            case RATE_11:(tx_hdr.DataTxRate) =11 ;break;     \
            case RATE_6:(tx_hdr.DataTxRate) =11 ;break;      \
            case RATE_9:(tx_hdr.DataTxRate) =15 ;break;      \
            case RATE_12:(tx_hdr.DataTxRate) =10 ;break;     \
            case RATE_18:(tx_hdr.DataTxRate) =14 ;break;     \
            case RATE_24:(tx_hdr.DataTxRate) =9 ;break;      \
            case RATE_36:(tx_hdr.DataTxRate) =13 ;break;     \
            case RATE_48:(tx_hdr.DataTxRate) =8 ;break;      \
            case RATE_54:(tx_hdr.DataTxRate) =12;break;      \
            default:(tx_hdr.DataTxRate)=0;                   \
            }
        }
        //tx_hdr.DataTxRate = 9;
        //INFO(" idx: %d,\n",tx_hdr.DataTxRate);
    }else{
        if(priv->mactype == BASEBAND_SCI9013 || priv->mactype == BASEBAND_SCI9012 ){
            tx_hdr.DataTxRate = 0;
        }else
            tx_hdr.DataTxRate = 11;
        //tx_hdr.DataTxRate = tx_rate_transfer(info->control.rates[0].idx);
    }

    if(priv->mactype == BASEBAND_SCI9013 || priv->mactype == BASEBAND_SCI9012 )
        tx_hdr.RtsCtsTxRate = tx_rate_transfer(info->control.rts_cts_rate_idx);  //1Mbps @ 0
    else
        tx_hdr.RtsCtsTxRate = tx_rate_transfer_0(info->control.rts_cts_rate_idx,priv->short_pre);  //1Mbps @ 0
	
    tx_hdr.Burst = 0;
    tx_hdr.IsGroup = 0;
    tx_hdr.NeedCts = 0;
    tx_hdr.NeedRts = 0;
    tx_hdr.Reserved2 = 0;
    //word 1
    tx_hdr.LenEndBurst = skb->len-24;
    tx_hdr.LengthBurst = 0;
    tx_hdr.FragsBurst = 1;
    tx_hdr.Reserved3 = 0;

    //word 2
    tx_hdr.RtsCtsDur = rts_dur; //0;  11162011 dmou
    tx_hdr.FrameSequence = (ieee80211hdr.seq_ctrl >> 4) & 0xfff;
    tx_hdr.Reserved4 = 0;
    
    if((priv->vif) && (priv->vif->type == NL80211_IFTYPE_ADHOC  ))
    {
        if(((ieee80211hdr.frame_control >> 2) & 0x3)==RT_80211_FRAME_TYPE_DATA)  //data frame
        {
            //INFO("@@@@@@@@ AD-HOC DATA  @@@@@@@@@@@\n");
            //word 3
            memcpy(&(tx_hdr.RtsDAL),ieee80211hdr.addr1,4);
 
            //word 4
            tx_hdr.RtsDAH = ieee80211hdr.addr1[5]<<8 | ieee80211hdr.addr1[4];
            if((((ieee80211hdr.frame_control >> 4) & 0xf)& 0x8) == 0x8) {
                tx_hdr.QosCtl_Up = skb->data[24] & 0x7;
                tx_hdr.QosCtl_Ack = (skb->data[24] >> 5 )& 0x3 ;
                tx_hdr.DataByteCnt = skb->len-26;
            }else{
                tx_hdr.QosCtl_Up = 0;
                tx_hdr.QosCtl_Ack = tx_hdr.IsGroup;
                tx_hdr.DataByteCnt = skb->len-24;
            }
        }
    
        else if(((ieee80211hdr.frame_control >> 2) & 0x3)==RT_80211_FRAME_TYPE_MANAGEMENT)   //management frame
        {
            //word 3
            memcpy(&(tx_hdr.RtsDAL),ieee80211hdr.addr1,4);

            //word 4
            tx_hdr.RtsDAH = ieee80211hdr.addr1[5]<<8 | ieee80211hdr.addr1[4];
            tx_hdr.QosCtl_Up = 0;
            tx_hdr.QosCtl_Ack = tx_hdr.IsGroup;
            tx_hdr.DataByteCnt = skb->len-24;
        }
    }
    else   //NOT AD-HOC
    {
        if(((ieee80211hdr.frame_control >> 2) & 0x3)==RT_80211_FRAME_TYPE_DATA)  //data frame
        {
            //word 3
            memcpy(&(tx_hdr.RtsDAL),ieee80211hdr.addr3,4);

            //word 4
            tx_hdr.RtsDAH = ieee80211hdr.addr3[5]<<8 | ieee80211hdr.addr3[4];
            if((((ieee80211hdr.frame_control >> 4) & 0xf)& 0x8) == 0x8) {
                tx_hdr.QosCtl_Up = skb->data[24] & 0x7;
                tx_hdr.QosCtl_Ack = (skb->data[24] >> 5 )& 0x3 ;
                tx_hdr.DataByteCnt = skb->len-26;
            }else{
                tx_hdr.QosCtl_Up = 0;
                tx_hdr.QosCtl_Ack = tx_hdr.IsGroup;
                tx_hdr.DataByteCnt = skb->len-24;
            }
        }
        else if(((ieee80211hdr.frame_control >> 2) & 0x3)==RT_80211_FRAME_TYPE_MANAGEMENT)   //management frame
        {
            //word 3
            memcpy(&(tx_hdr.RtsDAL),ieee80211hdr.addr1,4);

            //word 4
            tx_hdr.RtsDAH = ieee80211hdr.addr1[5]<<8 | ieee80211hdr.addr1[4];
            tx_hdr.QosCtl_Up = 0;
            tx_hdr.QosCtl_Ack = tx_hdr.IsGroup;
            tx_hdr.DataByteCnt = skb->len-24;
        }
    }
    
    tx_hdr.Reserved5 = 0;
    //word 5
    tx_hdr.Type = (ieee80211hdr.frame_control >> 2) & 0x3;
    tx_hdr.Subtype =  (ieee80211hdr.frame_control >> 4) & 0xf;
    tx_hdr.ToDs =  (ieee80211hdr.frame_control >> 8) & 0x1;
    tx_hdr.MoreFrag =  (ieee80211hdr.frame_control >> 10) & 0x1;
    tx_hdr.PwrMgt =  (ieee80211hdr.frame_control >> 12) & 0x1;    //NO Power-save
    //tx_hdr.WEP =  (ieee80211hdr.frame_control >> 14) & 0x1;
    tx_hdr.Order =  (ieee80211hdr.frame_control >> 15) & 0x1;
    tx_hdr.Dur = ieee80211hdr.duration_id;
    tx_hdr.KeyId = 0;   //support key 1 in AP Config SET
    
    tx_hdr.Reserved6 = 0;
    //05252011 dmou for wapi data frame .wep bit (handshake data frame must set to be 0,others data frame to be 1)

    if(tx_hdr.Type == 2)
    {    
        if((tx_hdr.Subtype & 0x8) == 0x8){
            flag1 = (*(txbuff+32))&0xff;
            flag2 = (*(txbuff+33))&0xff;
        }else{
            flag1 = (*(txbuff+30))&0xff;
            flag2 = (*(txbuff+31))&0xff;
        }
    }
    
    if(tx_hdr.Type == 2)
    {
        if((flag1== 0x88)&&(flag2== 0xb4)) {
            INFO(" TX WAPI 88B4 Data !!!!!!!! \n");
            tx_hdr.WEP =  0;   //wapi handshake data frame
            priv->wapi = 1;    //dmou 07142011
        }else  if(priv->wapi)
            tx_hdr.WEP = 1;//wapi other data frame
        else
            tx_hdr.WEP = (ieee80211hdr.frame_control >> 14) & 0x1;
    }else{
        tx_hdr.WEP = (ieee80211hdr.frame_control >> 14) & 0x1;
    }    
    
    /******************************************/
    /* For some AP don't respond the uinicast , 
     So make it to be broadcast */
    if(tx_hdr.Type == 0 && tx_hdr.Subtype == 4)
    {
        tx_hdr.RtsDAL = 0xffffffff;
        tx_hdr.RtsDAH = 0xffff;
        tx_hdr.IsGroup = 1;
    }

    //for qos   
    if((tx_hdr.Type == 2)&&((tx_hdr.Subtype & 0x8) == 0x8))
    {
        //modified by hzzhou on April7,2012
        skb_pull(skb,2);
        memcpy(skb->data,&tx_hdr,sizeof(struct _HW_TXD_STRUC));
        buf = skb->data;
        info->rate_driver_data[2] = &buf_hdr;
        //INFO("txqos \n");
        //INFO("TXD-Len %d \n",skb->len);
        //sci_deb_hex( " buf  ", buf,skb->len);  //added by dmou
    }
    else
    {
        memcpy(skb->data,&tx_hdr,sizeof(struct _HW_TXD_STRUC));
        buf = skb->data;
        info->rate_driver_data[2] = &ieee80211hdr;
    }

    ep = 2;
    info->rate_driver_data[0] = dev;
    info->rate_driver_data[1] = urb;

    /*if((tx_hdr.Type == 0)&&(tx_hdr.Subtype == 11))
    {
        priv->sbssid_flag = 1;
        memcpy(priv->bssid,ieee80211hdr.addr3,6);
        //sci_deb_hex("BSSIDTX ",ieee80211hdr.addr3,6);
    }else
        priv->sbssid_flag = 0;*/
    //////////////////////
    
    usb_fill_bulk_urb(urb, priv->udev, usb_sndbulkpipe(priv->udev, ep),
              buf, skb->len, sci9211_tx_cb, skb);
    urb->transfer_flags |= URB_ZERO_PACKET;

    usb_anchor_urb(urb, &priv->anchored);
    rc = usb_submit_urb(urb, GFP_ATOMIC);
    if (rc < 0) {
        ERR(" TX_urb ret2 %d ! \n",rc);
        //ERR(" urb->pipe %d ! \n",urb->pipe);   //08022011 dmou
        usb_unanchor_urb(urb);
        kfree_skb(skb);
        //usb_free_urb(urb);
        DEG("Exit func %s @_@!!\n", __func__);
    //#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37))  
    //	return rc;
    //#else
    //	return;
    //#endif
    }
    usb_free_urb(urb);
   
    //DEG("Exit func %s ^_^\n", __func__);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37))  
    return 0;
#else
    return;
#endif
}

#if PS
static void sci9211_rx_work(struct work_struct *work)  //dmou 05042011
{
    struct sci9211_priv *priv = container_of(work, struct sci9211_priv,
                    rx_work);
    //unsigned int ret;

    mutex_lock(&priv->conf_mutex);

    /*INFO("priv->time_stamp.vv.low_part : 0x%lx \n",priv->time_stamp.vv.low_part);
    INFO("priv->time_stamp.vv.high_part : 0x%lx \n",priv->time_stamp.vv.high_part);
    INFO("priv->beacon_int : 0x%x \n",priv->beacon_int);*/
    

    //do_gettimeofday(&priv->setreg_time);  //
    //INFO("%s setreg_time %ld us \n",__func__,priv->setreg_time.tv_usec);
    //priv->diff_time = priv->setreg_time.tv_usec - priv->beacon_time.tv_usec;
    //INFO("%s diff_time %ld us \n",__func__,priv->diff_time);


    /* for qos and PS cannot coexist */
    sci9211_wmm_set(priv,0);
    priv->time_stamp.quad_part = priv->time_stamp.quad_part >> 10;
    priv->time_stamp.quad_part = priv->time_stamp.quad_part + 2*priv->beacon_int - 20 ; 
    sci921x_iowrite32(priv,(__le32 *)TBTTTIMER,priv->time_stamp.vv.low_part);
    sci921x_iowrite32(priv,(__le32 *)(TBTTTIMER+4),priv->time_stamp.vv.high_part);

    /*ret = sci921x_ioread32(priv,(__le32 *)0x0500);
    ret |= 0x20;            //PS enable
    ret |= 0x100000;            //PMB_SEL
    sci921x_iowrite32(priv,(__le32 *)0x0500,ret);
    //INFO("0x0500 : 0x%x \n",ret);*/

    /*INFO("priv->beacon_int : 0x%x \n",priv->beacon_int);
    INFO("%s diff_time %ld us \n",__func__,priv->diff_time);
    INFO("0x0808 : 0x%x \n",sci921x_ioread32(priv,(__le32 *)0x0808));
    INFO("0x080C : 0x%x \n",sci921x_ioread32(priv,(__le32 *)0x080c));*/
    //HWDbgPrintReg(priv->hw_control);  //add by dmou for debug

    mutex_unlock(&priv->conf_mutex);
}
#endif

/**
 * @brief Change the RXD frame into ieee802.11 frame
 */

#define PWMG 0
#define MOREDATA 0

static void sci9211_rx_to_80211(struct sk_buff *skb,struct ieee80211_hw *dev )
{
    struct sci9211_priv *priv = dev->priv;
    PHW_RXD_STRUC prxd = (PHW_RXD_STRUC)skb->data;
    struct ieee80211_vif *vif = priv->vif;
    HW_RXD_STRUC rxd;
    struct ieee80211_qos_hdr *i802_11_hdr_qos = kzalloc(sizeof(struct ieee80211_qos_hdr),GFP_ATOMIC);
    int retcmp1;
    int i;
#if PS
    struct ieee802_11_elems elems;
#endif
    //DEG("Enter func %s\n", __func__);
    
    priv->rx_qos = 0;  //01042012 dmou
    
    memcpy(&rxd,prxd,sizeof(HW_RXD_STRUC));

#if 0
   // printk(KERN_INFO "RX_Typ: 0x%x,\n",rxd.Type);
    //printk(KERN_INFO "RX_Sub: 0x%x,\n",rxd.Subtype);
    //sci_deb_hex( "rxd ", &rxd,sizeof(HW_RXD_STRUC));  //added by dmou

    //if(((rxd.Type == 0) && (rxd.Subtype == 11))||((rxd.Type == 0) && (rxd.Subtype == 0))) {
    if(rxd.Type == 2){

        //printk(KERN_INFO "RX_Typ: 0x%x,\n",rxd.Type);
        printk(KERN_INFO "RX_Sub: 0x%x,\n",rxd.Subtype);
        printk(KERN_INFO "RXL-1 %d \n",skb->len);   //added by dmou
        //sci_deb_hex( "sci9211_RX_CB-1 ", skb->data,skb->len);  //added by dmou
        //sci_deb_hex( "rxd ", &rxd,sizeof(HW_RXD_STRUC));  //added by dmou

        //printk(KERN_INFO "DataByteCnt: 0x%x,\n",rxd.DataByteCnt);
        //printk(KERN_INFO "RxRate: 0x%x,\n",rxd.RxRate);
        //printk(KERN_INFO "PlcpRssi: 0x%x,\n",rxd.PlcpRssi);

        //printk(KERN_INFO "qos %d \n",qos);   //added by dmou
        //printk(KERN_INFO "MoreFrag: 0x%x,\n",rxd.MoreFrag);
        //printk(KERN_INFO "Retry: 0x%x,\n",rxd.Retry);
        //printk(KERN_INFO "WEP: 0x%x,\n",rxd.WEP);
        //printk(KERN_INFO "Order: 0x%x,\n",rxd.Order);
        //printk(KERN_INFO "RX_FRAG: 0x%x,\n",rxd.RX_FRAG);
        //printk(KERN_INFO "FrameSequence: 0x%x,\n",rxd.FrameSequence);

        //sci_deb_hex( "RxSA:  ", rxd.RxSA,6);  //added by dmou
        //sci_deb_hex( "RSDA:  ", rxd.RxDA,6);  //added by dmou
    }

#endif

    

    //for DEBUG 07252011 dmou
    if((prxd->Type == 0)&&(prxd->Subtype == 5)) { 
        INFO("********* skb_len: %d (RSSI: %d)\n", skb->len,prxd->PlcpRssi);   //10112011
    }
    if((prxd->Type == 0)&&(prxd->Subtype == 0x0c)) { 
        INFO("@@@@@@@@@ %s RX DEAUTH :< \n",__func__);
    }
    //////////////////////////////////////
    
    /******************************************************************
     * Attention:
     * data frame addr
     *      IFTYPE     ToDS      FromDS   addr1   addr2    addr3
     *      AD-HOC      0         0        DA       SA      BSSID
     *(to AP)STATION    1         0        BSSID    SA      DA
     *      STATION     0         1        DA      BSSID    SA
     * managent frame
     *      AD-HOC      0         0        DA       SA      BSSID
     *      STATION     0         0        DA       SA      BSSID
     *
     *****************************************************************/

    i802_11_hdr_qos->seq_ctrl = (rxd.RX_FRAG & 0xF)|(rxd.FrameSequence<<4&0xfff0);
    //i802_11_hdr_qos->duration_id = sci_generic_frame_duration(dev,priv->vif,skb->len,rate);  //dmou  problem

    if(vif &&(vif->type == NL80211_IFTYPE_ADHOC))
    {
        if(rxd.Type== RT_80211_FRAME_TYPE_DATA)  //data frame
        {
            i802_11_hdr_qos->frame_control = (0x0&0x3)|(rxd.Type<<2&0xc)|(rxd.Subtype<<4&0xf0)
                    |(0<<8&0x100)|(0<<9&0x200)|(rxd.MoreFrag<<10&0x400)
                    |(rxd.Retry<<11&0x800)|(PWMG<<12&0x1000)|(MOREDATA<<13&0x2000)
                    |(0<<14&0x4000)|(rxd.Order<<15&0x8000) ;  //WEP flag = 0 for the data from hw is DECRYPTED
            memcpy(i802_11_hdr_qos->addr1,rxd.RxDA,sizeof(i802_11_hdr_qos->addr1)); //RDA
            memcpy(i802_11_hdr_qos->addr2, rxd.RxSA,sizeof(i802_11_hdr_qos->addr2));
            memcpy(i802_11_hdr_qos->addr3, priv->bssid,sizeof(i802_11_hdr_qos->addr3));

            if((rxd.Subtype & 0x8) == 0x8) {  //qos
                i802_11_hdr_qos->qos_ctrl |= (rxd.QosCtl_Up & 0x7) ;
                priv->rx_qos = 1; 
                skb_push(skb,2);
                memcpy(skb->data, i802_11_hdr_qos, sizeof(struct ieee80211_qos_hdr));
            }else{
                memcpy(skb->data,i802_11_hdr_qos,sizeof(HW_RXD_STRUC));
            }
        }
        else if(rxd.Type== RT_80211_FRAME_TYPE_MANAGEMENT)   //management frame
        {
            i802_11_hdr_qos->frame_control = (0x0&0x3)|(rxd.Type<<2&0xc)|(rxd.Subtype<<4&0xf0)
		    |(0<<8&0x100)|(0<<9&0x200)|(rxd.MoreFrag<<10&0x400)
                    |(rxd.Retry<<11&0x800)|(PWMG<<12&0x1000)|(MOREDATA<<13&0x2000)
                    |(rxd.WEP<<14&0x4000)|(rxd.Order<<15&0x8000) ;
            memcpy(i802_11_hdr_qos->addr1, dev->wiphy->perm_addr,sizeof(i802_11_hdr_qos->addr1));
            memcpy(i802_11_hdr_qos->addr2, rxd.RxSA,sizeof(i802_11_hdr_qos->addr2));
            memcpy(i802_11_hdr_qos->addr3, rxd.RxDA,sizeof(i802_11_hdr_qos->addr3));

            memcpy(skb->data,i802_11_hdr_qos,sizeof(HW_RXD_STRUC));
        }
    }
    else
    {
        if(rxd.Type== RT_80211_FRAME_TYPE_DATA)  //data frame
        {
            i802_11_hdr_qos->frame_control = (0x0&0x3)|(rxd.Type<<2&0xc)|(rxd.Subtype<<4&0xf0)
                    |(0<<8&0x100)|(1<<9&0x200)|(rxd.MoreFrag<<10&0x400)
                    |(rxd.Retry<<11&0x800)|(PWMG<<12&0x1000)|(MOREDATA<<13&0x2000)
                    |(0<<14&0x4000)|(rxd.Order<<15&0x8000) ;  //WEP flag = 0 for the data from hw is DECRYPTED
            memcpy(i802_11_hdr_qos->addr1,rxd.RxDA,sizeof(i802_11_hdr_qos->addr1)); //RDA
            memcpy(i802_11_hdr_qos->addr2, priv->bssid,sizeof(i802_11_hdr_qos->addr2));
            memcpy(i802_11_hdr_qos->addr3, rxd.RxSA,sizeof(i802_11_hdr_qos->addr3));

            //printk(KERN_INFO "qos %d \n",qos);   //added by dmou
            if((rxd.Subtype & 0x8) == 0x8) {  //qos
               // printk(KERN_INFO "rxd.QosCtl_Up 0x%x \n",rxd.QosCtl_Up);   //added by dmou
                //printk(KERN_INFO "DataByteCnt:%d,\n",rxd.DataByteCnt);
                i802_11_hdr_qos->qos_ctrl |= (rxd.QosCtl_Up & 0x7) ;
                //printk(KERN_INFO "i802_11_hdr_qos->qos_ctrl 0x%x \n",i802_11_hdr_qos->qos_ctrl);   //added by dmou

                //printk(KERN_INFO "RXL-2-data_bpush %d \n",skb->len);   //added by dmou
                //sci_deb_hex( "sci9211_RX_CB-2-data_bpush ", skb->data,skb->len);  //added by dmou
                priv->rx_qos = 1;  //01042012 dmou
                skb_push(skb,2);
                memcpy(skb->data, i802_11_hdr_qos, sizeof(struct ieee80211_qos_hdr));
				//INFO( "RX qos  \n");   //added by dmou
                //INFO( "RXL  %d \n",skb->len);   //added by dmou
                //sci_deb_hex( "RXD ", skb->data,skb->len);  //added by dmou
            }else{
                memcpy(skb->data,i802_11_hdr_qos,sizeof(HW_RXD_STRUC));
            }
        }
        else if(rxd.Type== RT_80211_FRAME_TYPE_MANAGEMENT)   //management frame
        {
            i802_11_hdr_qos->frame_control = (0x0&0x3)|(rxd.Type<<2&0xc)|(rxd.Subtype<<4&0xf0)
                    |(0<<8&0x100)|(0<<9&0x200)|(rxd.MoreFrag<<10&0x400)
                    |(rxd.Retry<<11&0x800)|(PWMG<<12&0x1000)|(MOREDATA<<13&0x2000)
                    |(rxd.WEP<<14&0x4000)|(rxd.Order<<15&0x8000) ;
            memcpy(i802_11_hdr_qos->addr1, dev->wiphy->perm_addr,sizeof(i802_11_hdr_qos->addr1));
            memcpy(i802_11_hdr_qos->addr2, rxd.RxSA,sizeof(i802_11_hdr_qos->addr2));
            memcpy(i802_11_hdr_qos->addr3, rxd.RxDA,sizeof(i802_11_hdr_qos->addr3));

            memcpy(skb->data,i802_11_hdr_qos,sizeof(HW_RXD_STRUC));
#if PS           
            //ap no qos
            if(((rxd.Subtype == 0x5)||(rxd.Subtype == 0x8))&&(skb->len>36)){
                ieee80211_parse_elems(&skb->data[36], skb->len-36,&elems);
            }
#endif
            /* AVG_RSSI */
            if((priv->assoc)&&(rxd.Subtype == 0x8))
            {
                retcmp1 = array_compare(6,priv->bssid,6,rxd.RxSA);

                if((retcmp1 == 0)&&(skb->len>36)){
                   //sci_deb_hex( "beacon  ", skb->data,skb->len);  //added by dmou
#if PS
                   //do_gettimeofday(&priv->beacon_time);  //
                   //INFO("%s beacon_time %ld us \n",__func__,priv->beacon_time.tv_usec);

                    priv->time_stamp.vv.low_part = ((*(skb->data+24))&0xff) | 
                                             ((*(skb->data+25) << 8)&0xff00) | 
                                             ((*(skb->data+26) << 16)&0xff0000) | 
                                             ((*(skb->data+27) << 24)&0xff000000) ;
                    priv->time_stamp.vv.high_part = ((*(skb->data+28))&0xff) | 
                                              ((*(skb->data+29) << 8)&0xff00) | 
                                              ((*(skb->data+30) << 16)&0xff0000) | 
                                              ((*(skb->data+31) << 24)&0xff000000) ;
                    priv->beacon_int = ((*(skb->data+32))&0xff) | 
                                 ((*(skb->data+33) << 8)&0xff00);
                    
                    /*INFO("priv->time_stamp.vv.low_part : 0x%lx \n",priv->time_stamp.vv.low_part);
                    INFO("priv->time_stamp.vv.high_part : 0x%lx \n",priv->time_stamp.vv.high_part);
                    INFO("priv->beacon_int : 0x%x \n",priv->beacon_int);*/
                    
                    schedule_work(&priv->rx_work);
					priv->beacon_num_ps ++;
#endif					
					
                    priv->beacon_num ++;
                    
                    if(priv->beacon_num >= 9)
                        priv->beacon_num = 8;

                    if(priv->beacon_num == 1){
                        for(i=0;i<8;i++)
                        {
                            priv->receive_rssi[i] = calculate_rssi(priv,rxd.PlcpRssi);
                        }
                        
                        priv->AVG_RSSI_current = priv->AVG_RSSI_last = priv->receive_rssi[0];
                    } else{
                        for(i=7;i>0;i--)
                        {
                            priv->receive_rssi[i] = priv->receive_rssi[i-1];
                        }
                        priv->receive_rssi[0] = calculate_rssi(priv,rxd.PlcpRssi);
                        priv->AVG_RSSI_current = (priv->receive_rssi[0]+priv->receive_rssi[1]+
                                              priv->receive_rssi[2]+priv->receive_rssi[3]+
                                              priv->receive_rssi[4]+priv->receive_rssi[5]+
                                              priv->receive_rssi[6]+priv->receive_rssi[7])/8;
                        //INFO("%s priv->AVG_RSSI %d dbm",__func__,priv->AVG_RSSI_current);
                        if(priv->AVG_RSSI_current != priv->AVG_RSSI_last)
                        {
                            power_save_adjust(priv);
                        }
                        priv->AVG_RSSI_last = priv->AVG_RSSI_current;
                    }
                }

            }
            /////////////////////////
        }
    }
    //DEG("Exit func %s\n", __func__);
    kfree(i802_11_hdr_qos);
}

static void sci9211_rx_cb(struct urb *urb)
{
    struct sk_buff *skb = (struct sk_buff *)urb->context;
    struct sci9211_rx_info *info = (struct sci9211_rx_info *)skb->cb;
    struct ieee80211_hw *dev = info->dev;
    struct sci9211_priv *priv = dev->priv;
    struct ieee80211_rx_status rx_status = { 0 };
    PHW_RXD_STRUC  hdr;
    int rate = 0;
    int signal;
    u32 flags = 0;
    unsigned long f;
	
    spin_lock_irqsave(&priv->rx_queue.lock, f);
    __skb_unlink(skb, &priv->rx_queue);
    spin_unlock_irqrestore(&priv->rx_queue.lock, f);
    
    if(priv->is_disconnect)
    {
        WAR("%s usb disconnect ! \n",__func__);
        dev_kfree_skb_irq(skb);
        DEG("Exit func %s @_@!!\n", __func__);
        return;
    }

    if (unlikely(urb->status)) {
        dev_kfree_skb_irq(skb);
        INFO("usta %d ! \n",urb->status);  
        DEG("Exit func %s @_@!!\n", __func__);
        return;
    }

    skb_put(skb, urb->actual_length);

    hdr = (PHW_RXD_STRUC)(skb->data);
    flags = le32_to_cpu(hdr->DataByteCnt | hdr->Rsv0<<12 |
                         hdr->RxInfo<<16 | hdr->TxInfo<<17 |
                         hdr->AtimInfo<<18 | hdr->Rsv1<<19 );//zyxie remove

    if(priv->mactype == BASEBAND_SCI9013 || priv->mactype == BASEBAND_SCI9012 )
    {
        //new hw init calculate_rssi ,2012.7.5 dmou
        signal = calculate_rssi(priv,(int8_t)(hdr->PlcpRssi & 0xFF));
    }
    else
        signal = (-25) - hdr->PlcpRssi;    //added by dmou 10112011 for singnal

    /*if(hdr->Type == 2)
    {
        //sci_deb_hex(" AP ",hdr->RxSA,6);
        //INFO("++++   RSSI IN: %d  OUT: %d(dbm)  ++++++\n",hdr->PlcpRssi,signal);
    }*/

    rx_status.antenna = 1;   //added by dmou 10112011 for singnal
    rx_status.mactime = jiffies/HZ*1000;
    priv->noise = 0;  
    rate = rx_rate_transfer(hdr->RxRate);

    rx_status.signal = signal;
    priv->signal = signal;
    
    sci9211_rx_to_80211(skb,dev);

    //01042012 dmou
    if(priv->rx_qos)
    {
        skb_trim(skb, (flags & 0x0FFF)+26);
    }else
        skb_trim(skb, (flags & 0x0FFF)+24);  
    //////////////////////////
    //skb_trim(skb, flags & 0x0FFF);  //remove by zyxie
    
    rx_status.rate_idx = rate;
    rx_status.freq = dev->conf.channel->center_freq;
    rx_status.band = dev->conf.channel->band;
    rx_status.flag  = RX_FLAG_DECRYPTED;

    memcpy(IEEE80211_SKB_RXCB(skb), &rx_status, sizeof(rx_status));   //dmou 04012011

    ieee80211_rx_irqsafe(dev, skb);

    skb = dev_alloc_skb(SCI9211_MAX_RX);
    if (unlikely(!skb)) {
        //usb_free_urb(urb);
        ERR(" TODO check rx queue length and refill *somewhere*\n");  // 10102011 dmou
        /* TODO check rx queue length and refill *somewhere* */
	//DEG("Exit func %s @_@!!\n", __func__);
        return;
    }

    info = (struct sci9211_rx_info *)skb->cb;
    info->urb = urb;
    info->dev = dev;
    urb->transfer_buffer = skb_tail_pointer(skb);  //dmou
    urb->context = skb;
    skb_queue_tail(&priv->rx_queue, skb);

    //10282011 dmou
    //usb_submit_urb(urb, GFP_ATOMIC);
    usb_anchor_urb(urb, &priv->anchored);
    if (usb_submit_urb(urb, GFP_ATOMIC)) {
        usb_unanchor_urb(urb);
        skb_unlink(skb, &priv->rx_queue);
        dev_kfree_skb_irq(skb);
    }
    //DEG("Exit func %s ^_^\n", __func__);
    //////////////////////////////////////////////
}

static int sci9211_init_urbs(struct ieee80211_hw *dev)
{
    struct sci9211_priv *priv = dev->priv;
    struct urb *entry = NULL;
    struct sk_buff *skb;
    struct sci9211_rx_info *info;
    int ret = 0;

    DEG("Enter func %s\n", __func__);

    while (skb_queue_len(&priv->rx_queue) < 16) {    //10282011 dmou
        skb = __dev_alloc_skb(SCI9211_MAX_RX, GFP_KERNEL);
        if (!skb) {
            ret = -ENOMEM;
            goto err;
        }
        entry = usb_alloc_urb(0,GFP_KERNEL );  
        if (!entry) {
            ret = -ENOMEM;
            goto err;
        }

        usb_fill_bulk_urb(entry, priv->udev,
              usb_rcvbulkpipe(priv->udev,
              1),
              skb_tail_pointer(skb),   
              SCI9211_MAX_RX, sci9211_rx_cb, skb);
    
        info = (struct sci9211_rx_info *)skb->cb;
        info->urb = entry;
        info->dev = dev;
    
        skb_queue_tail(&priv->rx_queue, skb);
    
        usb_anchor_urb(entry, &priv->anchored);
        ret = usb_submit_urb(entry, GFP_KERNEL);
        if (ret) 
        {
            skb_unlink(skb, &priv->rx_queue);
            usb_unanchor_urb(entry);
            goto err;
        }
        usb_free_urb(entry);
        ///////////////////////////////////////////////
    }
    DEG("Exit func %s ^_^\n", __func__);
    return ret;

err:
    usb_free_urb(entry);
    kfree_skb(skb);
    usb_kill_anchored_urbs(&priv->anchored);
    ERR("Exit func %s @_@!!\n", __func__);
    return ret;
}


#if PS
static void sci9211_ps_work(struct work_struct *work)  //dmou 05042011
{
    struct sci9211_priv *priv = container_of(work, struct sci9211_priv,
                    ps_work);
    unsigned int ret = 0;

    mutex_lock(&priv->conf_mutex);

    if(priv->tx_data_num > 6){
        priv->tx_low_cal = 0;

        if(priv->set_ps){
            ret = sci921x_ioread32(priv,(__le32 *)0x0500);
            ret &= 0xFFFFFFDF;            //PS disable
            sci921x_iowrite32(priv,(__le32 *)0x0500,ret);
            priv->set_ps = 0;

            INFO("0x0500 : 0x%x \n",ret);
        }
    }else{
        priv->tx_low_cal++;
        
        if(priv->tx_low_cal >= 5){ //low tx_data_num lasting 5 seconds
            if(priv->set_ps == 0){
                ret = sci921x_ioread32(priv,(__le32 *)0x0500);
                ret |= 0x20;            //PS enable
                ret |= 0x100000;            //PMB_SEL
                sci921x_iowrite32(priv,(__le32 *)0x0500,ret);
                priv->set_ps = 1;

                INFO("0x0500 : 0x%x \n",ret);
            }
        }
    }
    INFO("priv->tx_data_num : %d \n",priv->tx_data_num);

    priv->tx_data_num = 0;  //counter clear

    if(priv->assoc)
        mod_timer(&priv->ps_timer,jiffies + HZ);

    mutex_unlock(&priv->conf_mutex);
}

int ps_timer_func(unsigned long data)
{
    struct sci9211_priv *priv = (struct sci9211_priv *)data;
    
    //DEG("Enter func %s\n", __func__);
    if (priv == NULL ) {
        ERR("%s called with NULL data\n",__func__);
        return -EINVAL;
    }

    schedule_work(&priv->ps_work);

    return 0;
}

static void sci9211_force_ps_work(struct work_struct *work)  //dmou 05042011
{
    struct sci9211_priv *priv = container_of(work, struct sci9211_priv,
                    force_ps_work);
    unsigned int ret = 0;

    mutex_lock(&priv->conf_mutex);

    if(priv->beacon_num_ps){
        if(priv->force_awake){
            ret = sci921x_ioread32(priv,(__le32 *)0x090C);
            ret &= 0xFFFDFFFF;            //disable FORCE_AWAKE
            sci921x_iowrite32(priv,(__le32 *)0x090C,ret);
            priv->force_awake = 0;
            INFO("0x090c : 0x%x \n",ret);
            INFO("priv->beacon_num_ps : %d \n",priv->beacon_num_ps);
        }
        
        priv->beacon_num_ps = 0;
    }else{
        INFO("NO BEACON RECEIVE !!! \n");
        if(priv->force_awake==0){
            ret = sci921x_ioread32(priv,(__le32 *)0x090C);
            ret |= 0x20000;            //FORCE_AWAKE
            sci921x_iowrite32(priv,(__le32 *)0x090C,ret);
            priv->force_awake = 1;

            INFO("0x090c : 0x%x (FORCE_AWAKE)\n",sci921x_ioread32(priv,(__le32 *)0x090C));
            INFO("0x500 : 0x%x \n",sci921x_ioread32(priv,(__le32 *)0x0500));
        }
    }

    if(priv->assoc)
        mod_timer(&priv->force_ps_timer,jiffies + 3*priv->beacon_int);  //HZ*300/1000
    
    mutex_unlock(&priv->conf_mutex);
}

int force_ps_timer_func(unsigned long data)
{
    struct sci9211_priv *priv = (struct sci9211_priv *)data;
    
    //DEG("Enter func %s\n", __func__);
    if (priv == NULL ) {
        ERR("%s called with NULL data\n",__func__);
        return -EINVAL;
    }

    schedule_work(&priv->force_ps_work);

    return 0;
}
#endif


static int sci9211_init_hw(struct ieee80211_hw *dev)
{
    struct sci9211_priv *priv = dev->priv;

    priv->rf->init(dev);

    return 0;
}

static int sci9211_start(struct ieee80211_hw *dev)
{
    struct sci9211_priv *priv = NULL;
    int ret = 0;
	
    DEG("Enter func %s\n",__func__);  //11282011 dmou
    if(dev == NULL){
        ERR("Can not handle NULL pointer\n");
        return -1;
    }
    else{
        priv = dev->priv;
    }
    
    mutex_lock(&priv->conf_mutex);

    ret = sci9211_init_hw(dev);
    if (ret)
    {
        ERR(" sci9211 init hw error !\n");    
        return ret;
    }

    init_usb_anchor(&priv->anchored);
    priv->dev = dev;

    sci9211_init_urbs(dev);

    INIT_DELAYED_WORK(&priv->work, sci9211_work);  //dmou 05042011

#if 1
    //2012.7.16 for temperature calculate
    temperature_workqueue = create_singlethread_workqueue("temperature_workqueue");
    if(!temperature_workqueue)
    {
        mutex_unlock(&priv->conf_mutex);
        ERR("Can not create singlethread temperature_workqueue\n");
        return -1;
    }
    INIT_WORK(&priv->temperature_work,sci9211_temperture_work);
 
    init_timer(&priv->cal_temp_timer);
    priv->cal_temp_timer.data = (unsigned long) priv;
    priv->cal_temp_timer.function =(void *)calculate_temperature;
    priv->cal_temp_timer.expires = jiffies + HZ*2;
    

#if 0
    debug_workqueue = create_singlethread_workqueue("debug_workqueue");
    if(!debug_workqueue)
    {
        mutex_unlock(&priv->conf_mutex);
        ERR("Can not create singlethread debug_workqueue\n");
        return -1;
    }
    INIT_DELAYED_WORK(&priv->debug_work,debug_work);
    init_timer(&priv->debug_timer);
    priv->debug_timer.data = (unsigned long) priv;
    priv->debug_timer.function =(void *)debug_timer_func;
    priv->debug_timer.expires = jiffies + HZ/1000*100;
    add_timer(&priv->debug_timer);
#endif
#endif
#if PS
    INIT_WORK(&priv->rx_work,sci9211_rx_work);

    init_timer(&priv->ps_timer);
    priv->ps_timer.data = (unsigned long) priv;
    priv->ps_timer.function =(void *)ps_timer_func;
    priv->ps_timer.expires = jiffies + HZ;

    INIT_WORK(&priv->ps_work,sci9211_ps_work);

    init_timer(&priv->force_ps_timer);
    priv->force_ps_timer.data = (unsigned long) priv;
    priv->force_ps_timer.function =(void *)force_ps_timer_func;
    priv->force_ps_timer.expires = jiffies + HZ*300/1000;

    INIT_WORK(&priv->force_ps_work,sci9211_force_ps_work);
#endif

    mutex_unlock(&priv->conf_mutex);

    return ret;
}

static void sci9211_stop(struct ieee80211_hw *dev)
{
    struct sci9211_priv *priv = dev->priv;
    struct sk_buff *skb;
    
    DEG("[ %s ] Enter func %s \n", current->comm, __func__);

    //mutex_lock(&priv->conf_mutex); //would you tell me why??

    INFO("Setup 1 -----> stop rf \n");
    priv->rf->stop(dev);

    INFO("Setup 2 -----> kill anchore urbs \n");
    usb_kill_anchored_urbs(&priv->anchored);   //dmou

    //msleep(1000);  //1
    INFO("Setup 3 -----> free skb \n");
    while ((skb = skb_dequeue(&priv->tx_status.queue)))
            dev_kfree_skb_any(skb);
    
    //mutex_unlock(&priv->conf_mutex);

    INFO("Setup 4 -----> cancel the delay work \n");
    cancel_delayed_work_sync(&priv->work);  //dmou05042011

    //2012.7.16 for temperature calculate
    INFO("Setup 5 -----> destroy the temperature_workqueue \n");
    destroy_workqueue(temperature_workqueue);
    INFO("Setup 6 ----->  del_cal_temp_timer \n"); 
    del_timer(&priv->cal_temp_timer); 
#if PS
    cancel_work_sync(&priv->rx_work);
    INFO("Setup 7 ----->  cancel_rx_work_sync \n");  
    cancel_work_sync(&priv->ps_work);
    INFO("Setup 8 ----->  cancel_ps_work_sync \n"); 
    del_timer(&priv->ps_timer); //dmou
    INFO("Setup 9 ----->  del_ps_timer \n"); 
    
    cancel_work_sync(&priv->force_ps_work);
    INFO("Setup 10 ----->  cancel_force_ps_work_sync \n"); 
    del_timer(&priv->force_ps_timer); //dmou
    INFO("Setup 11 ----->  del_force_ps_timer \n");  
#endif	
	
#if 0
    del_timer(&priv->debug_timer); //dmou
    INFO("[%s] after debug_timer \n",__func__);  //11212011 dmou
    destroy_workqueue(debug_workqueue);
    cancel_delayed_work_sync(&priv->debug_work);
#endif
    DEG("Exit func %s \n",__func__);
    msleep(1000);  //2
}

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,33))
static int sci9211_add_interface(struct ieee80211_hw *dev,
		struct ieee80211_if_init_conf *conf)
#else
static int sci9211_add_interface(struct ieee80211_hw *dev,
                 struct ieee80211_vif *vif)
#endif
{
    struct sci9211_priv *priv = dev->priv;
    u8  addr[ETH_ALEN];
    int i;
    int ret = -EOPNOTSUPP;

    DEG("Enter func  %s\n",__func__);  //11282011 dmou

    mutex_lock(&priv->conf_mutex);
    if (priv->vif)
        goto exit;
 
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,33))
    switch (conf->type){
#else
	switch (vif->type){
#endif
    case NL80211_IFTYPE_STATION:
        break;
    
    case NL80211_IFTYPE_ADHOC:
        break;

    case NL80211_IFTYPE_AP:
        break;
    default:
        goto exit;
    }

    ret = 0;
	
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,33))
    priv->vif = conf->vif;
#else
	 priv->vif = vif;
#endif

    for (i = 0; i < ETH_ALEN; i++){
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,33))
		addr[i] = ((u8 *)conf->mac_addr)[i];
#else
	 	addr[i] = ((u8 *)vif->addr)[i];
#endif
		//printk("addr[%d]=0x%x\n", i,addr[i]);
    }

    HWSetMacAddr(priv->hw_control,addr);

    DEG("Exit func %s\n",__func__);  //11282011 dmou
exit:
    mutex_unlock(&priv->conf_mutex);
    return ret;
}

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,33))
static void sci9211_remove_interface(struct ieee80211_hw *dev,
                     struct ieee80211_if_init_conf *conf)
#else
static void sci9211_remove_interface(struct ieee80211_hw *dev,
                     struct ieee80211_vif *vif)
#endif
{
    struct sci9211_priv *priv = dev->priv;
    INFO("Enter func %s \n",__func__);  //08022011 dmou

    mutex_lock(&priv->conf_mutex);    //11182011 dmou
    priv->vif = NULL;
    mutex_unlock(&priv->conf_mutex);    //11182011 dmou
    INFO("Exit func %s \n",__func__);  //08022011 dmou
}

static int sci9211_config(struct ieee80211_hw *dev,u32 changed/*struct ieee80211_conf *conf*/)
{
    struct sci9211_priv *priv = dev->priv;
    struct ieee80211_conf *conf = &dev->conf;

    //test_bit(IEEE80211_CONF_CHANGE_CHANNEL,changed);
    INFO("Enter func %s changed : 0x%x \n",__func__,changed);  //08022011 dmou

    mutex_lock(&priv->conf_mutex);
    priv->rf->set_chan(dev, conf);
    msleep(10);
    mutex_unlock(&priv->conf_mutex);

    //INFO("Exit func %s \n",__func__);  //08022011 dmou
    return 0;
}

/*
    ==========================================================================
    Description:
        Pre-build a BEACON frame in the shared memory
    ==========================================================================
*/
unsigned long sci9211_make_ibss_beacon(struct ieee80211_hw *dev,struct sk_buff *beacon)
{
    struct sci9211_priv *priv = dev->priv;
    CAPINFO_VALUE_STRUC CapInfo;
    u16 beacon_period;
    bool privacy;
    struct ieee802_11_elems elems;
    u32 tmp_word;
    LARGE_INTEGEREX    time_stamp;
    u8 *pos = kzalloc(sizeof(u8)*(beacon->len-24),GFP_ATOMIC);

    privacy = (beacon->data[34]&0x10)>>4;

    CapInfo.word = HWGetCapInfo(priv->hw_control);
    CapInfo.field.ESS = 0;
    CapInfo.field.IBSS = 1;
    CapInfo.field.Privacy = privacy;
    CapInfo.field.ShortPreable = 0;
    CapInfo.field.ShortSlotTime = 0;

    HWSetCapInfo(priv->hw_control,CapInfo.word);

    beacon_period = (beacon->data[33]<<8) | beacon->data[32];
    HWSetBcnInerv(priv->hw_control,beacon_period);

    memcpy(pos,&beacon->data[36],beacon->len-24);

    ieee80211_parse_elems(pos, beacon->len-36,&elems);

    HWSetAtimPeriod(priv->hw_control,*(elems.ibss_params));

    HWSetSuppRate(priv->hw_control,elems.supp_rates_len,(elems.supp_rates));

    HWSetDsChannel(priv->hw_control,*(elems.ds_params));
    HWSetSsidLen(priv->hw_control,elems.ssid_len);
    HWSetSsid(priv->hw_control,elems.ssid_len,(elems.ssid));

    if (elems.ext_supp_rates_len)
    {
        tmp_word = HWGetErpInfo(priv->hw_control);
        tmp_word |= elems.ext_supp_rates_len;
        HWSetErpInfo(priv->hw_control,tmp_word);

        HWSetExtRate(priv->hw_control,elems.ext_supp_rates_len,(elems.ext_supp_rates));
    }

    HWSetBcnPrspLng(priv->hw_control,beacon->len+3);

    time_stamp.vv.low_part = HWGetTsfBaseLow(priv->hw_control);
    time_stamp.vv.high_part = HWGetTsfBaseHig(priv->hw_control);
    time_stamp.quad_part=time_stamp.quad_part >> 10;
    time_stamp.quad_part=time_stamp.quad_part+2*beacon_period-3;

    HWSetTbttTimer(priv->hw_control,time_stamp.vv.low_part,time_stamp.vv.high_part);

    kfree(pos);
    return beacon->len+3;
}

#if 0
static int sci9211_config_interface(struct ieee80211_hw *dev,
                    struct ieee80211_vif *vif,
                    struct ieee80211_if_conf *conf)
{
    struct sci9211_priv *priv = dev->priv;
    int ret = 0;
    u32 reg;
    unsigned long framelen;

    mutex_lock(&priv->conf_mutex);

    HWSetBSSIDAddr(priv->hw_control,conf->bssid);
    memcpy(priv->bssid,conf->bssid,6);

     //ADHOC mode
    if ((conf->changed & IEEE80211_IFCC_BEACON )&&
        ((vif->type == NL80211_IFTYPE_ADHOC) ||
         (vif->type == NL80211_IFTYPE_AP)) )
    {
        struct sk_buff *beacon = ieee80211_beacon_get(dev, vif);
        if (!beacon) {
            while(1);  //for compile BUG !!!!!!
            ret = -ENOMEM;
            goto unlock;
        }

        //set mod
        reg = HWGetSystemConfig(priv->hw_control);
        reg &= 0xFFFD;

        HWSetSystemConfig(priv->hw_control,reg);

        /*set reg for beacon tx*/
        framelen = sci9211_make_ibss_beacon(dev,beacon);

        reg = HWGetTxRxConfig(priv->hw_control);
        reg &= 0xFFDF;

    }

unlock:
    mutex_unlock(&priv->conf_mutex);

    return ret;
}
#endif

static void sci9211_configure_filter(struct ieee80211_hw *dev,
                     unsigned int changed_flags,
                     unsigned int *total_flags,
                     u64 multicast/*int mc_count, struct dev_addr_list *mclist*/)
{
    struct sci9211_priv *priv = dev->priv;
    DEG( "Enter func %s\n" , __func__);
    //INFO("changed_flags=0x%x\n", changed_flags);  //11282011 dmou

    if (changed_flags & FIF_FCSFAIL)
        priv->rx_conf ^= SCI921X_RX_CONF_FCS;
    if (changed_flags & FIF_CONTROL)
        priv->rx_conf ^= SCI921X_RX_CONF_CTRL;
    if (changed_flags & FIF_OTHER_BSS)
        priv->rx_conf ^= SCI921X_RX_CONF_MONITOR;
    if (*total_flags & FIF_ALLMULTI || multicast/*mc_count*/ > 0)
        priv->rx_conf |= SCI921X_RX_CONF_MULTICAST;
    else
        priv->rx_conf &= ~SCI921X_RX_CONF_MULTICAST;

    *total_flags = 0;

    if (priv->rx_conf & SCI921X_RX_CONF_FCS)
        *total_flags |= FIF_FCSFAIL;
    if (priv->rx_conf & SCI921X_RX_CONF_CTRL)
        *total_flags |= FIF_CONTROL;
    if (priv->rx_conf & SCI921X_RX_CONF_MONITOR)
        *total_flags |= FIF_OTHER_BSS;
    if (priv->rx_conf & SCI921X_RX_CONF_MULTICAST)
        *total_flags |= FIF_ALLMULTI;

    //INFO("priv->rx_conf: 0x%x, *total_flags: 0x%x\n", priv->rx_conf, *total_flags);
    DEG( "Exit func %s\n" , __func__);
}

static int sci9211_conf_tx(struct ieee80211_hw *dev, u16 queue,
               const struct ieee80211_tx_queue_params *params)
{
    struct sci9211_priv *priv = dev->priv;
    AIFSN_VALUE_STRUC Aifsn;
    CWMIN_VALUE_STRUC Cwmin;
    CWMAX_VALUE_STRUC Cwmax;
    TXOPBEBK_VALUE_STRUC TxopBEBK;
    TXOPVOVI_VALUE_STRUC TxopVOVI;
    u8 cw_min, cw_max;
    u16 aci;
    INFO("Enter %s  queue : %d \n",__func__,queue);  //11282011 dmou
    if (queue > 3)
        return -EINVAL;

    /*printk(KERN_INFO " params->aifs  0x%x,\n",params->aifs);
    printk(KERN_INFO " params->cw_max  0x%x,\n",params->cw_max);
    printk(KERN_INFO " params->cw_min  0x%x,\n",params->cw_min);
    printk(KERN_INFO " params->txop  0x%x,\n",params->txop);*/

    cw_min = fls(params->cw_min);
    cw_max = fls(params->cw_max);

    /*printk(KERN_INFO " cw_max  0x%x,\n",cw_max);
    printk(KERN_INFO " cw_min  0x%x,\n",cw_min);
    printk(KERN_INFO " queue  0x%x,\n",queue);*/
    switch (queue) {
        case 3: /* AC_BK */
            aci = 1;
            break;
        case 1: /* AC_VI */
            aci = 2;
                break;
        case 0: /* AC_VO */
            aci = 3;
            break;
        case 2: /* AC_BE */
        default:
            aci = 0;
            break;
    }

    priv->pEdcaParm.bACM[aci]  = (((params->aifs) & 0x10)>>4);   // b4 is ACM
    priv->pEdcaParm.Aifsn[aci] = (params->aifs) & 0x0f;               // b0~3 is AIFSN
    priv->pEdcaParm.Cwmin[aci] = cw_min;             // b0~4 is Cwmin
    priv->pEdcaParm.Cwmax[aci] = cw_max;               // b5~8 is Cwmax
    priv->pEdcaParm.Txop[aci]  = params->txop; // in unit of 32-us

    Aifsn.field.AIFSN_BK=priv->pEdcaParm.Aifsn[0];
    Aifsn.field.AIFSN_BE=priv->pEdcaParm.Aifsn[1];
    Aifsn.field.AIFSN_VI=priv->pEdcaParm.Aifsn[2];
    Aifsn.field.AIFSN_VO=priv->pEdcaParm.Aifsn[3];
    Aifsn.field.Res = 0;
    Cwmin.field.CWMIN_BK=priv->pEdcaParm.Cwmin[0];
    Cwmin.field.CWMIN_BE=priv->pEdcaParm.Cwmin[1];
    Cwmin.field.CWMIN_VI=priv->pEdcaParm.Cwmin[2];
    Cwmin.field.CWMIN_VO=priv->pEdcaParm.Cwmin[3];
    Cwmin.field.Res = 0;
    Cwmax.field.CWMAX_BK=priv->pEdcaParm.Cwmax[0];
    Cwmax.field.CWMAX_BE=priv->pEdcaParm.Cwmax[1];
    Cwmax.field.CWMAX_VI=priv->pEdcaParm.Cwmax[2];
    Cwmax.field.CWMAX_VO=priv->pEdcaParm.Cwmax[3];
    Cwmax.field.Res = 0;
    TxopBEBK.field.TXOP_BK=priv->pEdcaParm.Txop[0];
    TxopBEBK.field.TXOP_BE=priv->pEdcaParm.Txop[1];
    TxopVOVI.field.TXOP_VI=priv->pEdcaParm.Txop[2];
    TxopVOVI.field.TXOP_VO=priv->pEdcaParm.Txop[3];

    //set hardware register
#if !PS
    sci9211_wmm_set(priv,1);
	//INFO("!PS sci9211_wmm_set(priv,1)\n");
#endif	
    sci921x_iowrite32(priv,(__le32 *)AIFSN,Aifsn.word); //0x2273); //
    sci921x_iowrite32(priv,(__le32 *)CWMIN,Cwmin.word); //0x2344); //
    sci921x_iowrite32(priv,(__le32 *)CWMAX,Cwmax.word);  //0x34aa); //
    sci921x_iowrite32(priv,(__le32 *)TXOPBEBK,TxopBEBK.word); //0);   //
    sci921x_iowrite32(priv,(__le32 *)TXOPVOVI,TxopVOVI.word); //0x2f005e); //

    //HWDbgPrintReg(priv->hw_control);
    INFO("Exit %s\n",__func__);  //11282011 dmou
    return 0;
}

void sci9211_bss_info_changed(struct ieee80211_hw *hw,
                 struct ieee80211_vif *vif,
                 struct ieee80211_bss_conf *info,
                 u32 changed)
{
    struct sci9211_priv *priv = hw->priv;
    u32 reg;
    unsigned long framelen;
	
    DEG("Enter func %s\n", __func__);
    INFO("[%s] info->assoc %d , changed 0x%x", __func__, info->assoc, changed);  //08022011 dmou

    priv->assoc = info->assoc;
    
    mutex_lock(&priv->conf_mutex);
    if (WARN_ON(priv->vif != vif)){
        WAR("The vif do not match\n");
        goto unlock;
    }
    
    if (changed & BSS_CHANGED_BSSID)
    {
        memcpy(priv->bssid,info->bssid,6);   
        if (is_valid_ether_addr(info->bssid)){
            sci_deb_hex("BSSID ",info->bssid,6);
            HWSetBSSIDAddr(priv->hw_control,(UCHAR *)info->bssid);

            reg = sci921x_ioread32(priv,(__le32 *)0x0500);
            reg |= 0x8;            //join enable
            sci921x_iowrite32(priv,(__le32 *)0x0500,reg);

            //INFO("0x0500 : 0x%x \n",reg);
           /*reg = sci921x_ioread32(priv,(__le32 *)0x0708);
            reg &= 0xc000;
            reg |= info->aid;            //aid set low-14bit
            sci921x_iowrite32(priv,(__le32 *)0x0708,reg);
            INFO("0x0708 : 0x%x \n",reg);*/ //will sleep not active
        }
    }

    /*
	 * This needs to be after setting the BSSID in case
	 * mac80211 decides to do both changes at once because
	 * it will invoke post_associate.
	 */
	if (vif->type == NL80211_IFTYPE_ADHOC &&
	    changed & BSS_CHANGED_BEACON) {
		struct sk_buff *beacon = ieee80211_beacon_get(hw, vif);

		if (!beacon) {
            while(1);  //for compile BUG !!!!!!
            goto unlock;
        }

        //set mod
        reg = sci921x_ioread32(priv,(__le32 *)0x0500);
        reg &= 0xFFFD;
        sci921x_iowrite32(priv,(__le32 *)0x0500,reg);

        /*set reg for beacon tx*/
        framelen = sci9211_make_ibss_beacon(hw,beacon);
        //INFO("AD-HOC beacon framelen : %ld \n",framelen);
	}

    if(priv->assoc){
        if (timer_pending(&priv->cal_temp_timer)){
#if PS
            goto ps_timer;
#else
			goto unlock;
#endif
        }else{
            priv->cal_temp_timer.expires = jiffies + HZ*2;
            add_timer(&priv->cal_temp_timer);
            INFO("%s  add_cal_temp_timer  \n",__func__);
        }
#if PS
ps_timer:
        if (timer_pending(&priv->ps_timer)){
            goto force_ps_timer;
        }else{
            priv->ps_timer.expires = jiffies + HZ;
            add_timer(&priv->ps_timer);
            INFO("%s  add_ps_timer  \n",__func__);
        }

force_ps_timer:
        if (timer_pending(&priv->force_ps_timer)){
            goto unlock;
        }else{
            priv->force_ps_timer.expires = jiffies + 3*priv->beacon_int;  //HZ*300/1000;
            add_timer(&priv->force_ps_timer);
            INFO("%s  add_force_ps_timer  \n",__func__);
        }
#endif
    }else{
        priv->beacon_num = 0;
#if PS
        priv->beacon_num_ps = 0;

        if (timer_pending(&priv->cal_temp_timer))
            del_timer(&priv->cal_temp_timer); 
        if (timer_pending(&priv->ps_timer))
            del_timer(&priv->ps_timer); 
        if (timer_pending(&priv->force_ps_timer))
            del_timer(&priv->force_ps_timer); 
    
        if(priv->force_awake){
            reg = sci921x_ioread32(priv,(__le32 *)0x090C);
            reg &= 0xFFFDFFFF;            //disable FORCE_AWAKE
            sci921x_iowrite32(priv,(__le32 *)0x090C,reg);
            priv->force_awake = 0;
            INFO("0x090c : 0x%x (Disassoc,so disable FORCE_AWAKE) \n",reg);
        }

        if(priv->set_ps){
            reg = sci921x_ioread32(priv,(__le32 *)0x0500);
            reg &= 0xFFFFFFD7;            //DISABLE PS enable ,disable join 
            sci921x_iowrite32(priv,(__le32 *)0x0500,reg);
            priv->set_ps = 0;
            INFO("0x0500 : 0x%x (DISABLE PS_EN JOIN )\n",reg);
        }
#endif
    }

 unlock:
    mutex_unlock(&priv->conf_mutex);
    DEG("Exit func %s\n", __func__);
}

static int sci9211_set_key(struct ieee80211_hw *hw, enum set_key_cmd cmd,
               struct ieee80211_vif *vif, struct ieee80211_sta *sta,
               struct ieee80211_key_conf *key)
{
    struct sci9211_priv *priv = hw->priv;
    u8 algorithm;
    int ret = 0;
    DEG("Enter func %s\n",__func__);  //11282011 dmou
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37))
    switch(key->alg) {
#else
    switch(key->cipher){
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37))
    case  ALG_WEP:
        if (key->keylen == 5)
            algorithm = SCI9211_SEC_ALGO_WEP40;
        else
            algorithm = SCI9211_SEC_ALGO_WEP104;
        break;
#else
    case WLAN_CIPHER_SUITE_WEP40:
        algorithm = SCI9211_SEC_ALGO_WEP40;
        break;
    case WLAN_CIPHER_SUITE_WEP104:
        algorithm = SCI9211_SEC_ALGO_WEP104;
        break;
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37))
    case ALG_TKIP:
#else
    case WLAN_CIPHER_SUITE_TKIP:
#endif
        //06032011 dmou
        if(priv->wapi) {
            algorithm = SCI9211_SEC_ALGO_SMS4;
            break;
        }
        ////////////////////////////////////////
        algorithm = SCI9211_SEC_ALGO_TKIP;
        break;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37)
    case ALG_CCMP:
#else
    case WLAN_CIPHER_SUITE_CCMP:
#endif
        algorithm = SCI9211_SEC_ALGO_AES;
        break;
    default:
        WARN_ON(1);
        return -EINVAL;
    }

    mutex_lock(&priv->conf_mutex);

    switch (cmd) {
    case SET_KEY:
        if (algorithm == SCI9211_SEC_ALGO_WEP40 ||
            algorithm == SCI9211_SEC_ALGO_WEP104)
        {
            sci9211_add_shared_key(priv,key,algorithm);
        }

        if (algorithm == SCI9211_SEC_ALGO_TKIP)
        {
            key->flags |= IEEE80211_KEY_FLAG_GENERATE_MMIC;  //set flg
            sci9211_add_shared_key(priv,key,algorithm);
            //HWDbgPrintReg(priv->hw_control);
        }

        if (algorithm == SCI9211_SEC_ALGO_AES)
        {
            INFO(" AES !!!!!!!!!!!! \n");
            sci9211_add_shared_key(priv,key,algorithm);
            //HWDbgPrintReg(priv->hw_control);
        }

        //05242011 DMOU
         if (algorithm == SCI9211_SEC_ALGO_SMS4)
        {
            INFO(" SMS4 key->keyidx : %d !!!!!!!!!!!! \n",key->keyidx);
            sci9211_add_shared_key_wapi(priv,key,algorithm);
        }
        //////////////////////////////////////////////////////////////////

        key->hw_key_idx = key->keyidx;
        break;
    case DISABLE_KEY:
        sci9211_rm_shared_key(priv,key->hw_key_idx);
        break;

    default:
        ret = -EINVAL;
        goto unlock;
    }

unlock:
    mmiowb();
    mutex_unlock(&priv->conf_mutex);
    DEG("Exit func %s\n", __func__);
    return ret;

}

static u64 sci9211_get_tsf(struct ieee80211_hw *hw)
{
    struct sci9211_priv *priv = hw->priv;
    unsigned int word1, word2;       
    //DEG("Enter func %s\n",__func__);  //11282011 dmou

    word1 = HWGetTsfBaseLow(priv->hw_control);
    word2 = HWGetTsfBaseHig(priv->hw_control);

    return ((u64)word2<<32)|word1;
}

static const struct ieee80211_ops sci9211_ops = {
    .tx			= sci9211_tx,
    .start		= sci9211_start,
    .stop		= sci9211_stop,
    .add_interface      = sci9211_add_interface,
    .remove_interface   = sci9211_remove_interface,
    .config		= sci9211_config,
    .configure_filter   = sci9211_configure_filter,
    .conf_tx            =  sci9211_conf_tx,
    .bss_info_changed   =  sci9211_bss_info_changed,
    .set_key            =  sci9211_set_key,
    .get_tsf            =  sci9211_get_tsf,
    .reset_tsf          =  NULL,
    .tx_last_beacon     =  NULL,
};
void set_wifi_name(char * name);

static int __devinit sci9211_probe(struct usb_interface *intf,
                   const struct usb_device_id *id)
{
    struct usb_device *udev = interface_to_usbdev(intf);
    struct ieee80211_hw *dev;
    struct sci9211_priv *priv;
    int err,reg;
    struct ieee80211_rate sci921x_rates[12];
	set_wifi_name("sci9211.ko");

    DEG("Enter func %s\n", __func__);
    dev = ieee80211_alloc_hw(sizeof(*priv), &sci9211_ops);
    if (!dev) {
        ERR("sci9211: ieee80211 alloc failed\n");
        return -ENOMEM;
    }else{
	priv = (struct sci9211_priv *)dev->priv;
    }

    priv->is_disconnect = 0;   //11212011 dmou
    INFO("%s usb connect ! \n",__func__);  

    SET_IEEE80211_DEV(dev, &intf->dev);	//set the device for 802.11 hardware
    usb_set_intfdata(intf, dev);		//save the 802.11 hardware in the usb_interface
    priv->udev = udev;					//save the usb device in priv data

    /* Initialize driver private data */
    dev->wiphy->interface_modes =
        BIT(NL80211_IFTYPE_STATION) |
        BIT(NL80211_IFTYPE_ADHOC)/*|
        BIT(NL80211_IFTYPE_AP)*/;

#if MORE_AP
    priv->scan_flag = 0;
    priv->auth_flag = 0;
    //priv->scan_active = 0;
    //priv->diff_scan_time = 0;
#endif
    priv->assoc_flag = 0;
    priv->beacon_num = 0;
    priv->assoc = 0;
    priv->chan_current = priv->chan_last = 1;
#if PS
    priv->tx_data_num = 0;
    priv->tx_low_cal = 0;
    priv->set_ps = 0;
	priv->beacon_num_ps = 0;
    priv->force_awake = 0;
#endif

    /********************* HW INIT ****************************/
    /*******To get the real RF chip first 
    (here RFIC_SCIS1022 can be changed to any one else)******/
    priv->hw_control = HWOpen(RFIC_SCIS1022,WIRELESS_MODE_G,(UINT)priv);

    priv->ver = HWEEPROMReadVER(priv->hw_control);
    err = HWEEPROMReadRFBBMType(priv);
    if(err < 0)
    {
        ERR("sci9211: Hw Init failed !!!");   //11162011 dmou
        return -EINVAL;
    }

    HWClose(priv->hw_control);
    priv->hw_control = HWInit(priv->modtype,priv->rftype,priv->mactype,WIRELESS_MODE_G,priv->ver,(UINT)priv);

    /*Init variable*/
    priv->CHQAM64Gain = HWEEPROMReadCH_QAM64_GAIN(priv->hw_control);
    priv->CHDiffPower = HWEEPROMReadCH_DIFF(priv->hw_control,priv->ver);

    priv->T_INITIAL = 0;
    priv->T_CURRENT = 25;
    priv->DIFF_CAL_CURRENT = 0;
    priv->DIFF_INI_CURRENT = 0;
    priv->AVG_RSSI_current = -60;
    priv->AVG_RSSI_last = -60;

    priv->TEMP_DEC[0] = -7;
    priv->TEMP_DEC[1] = -18;
    priv->TEMP_DEC[2] = -29;
    priv->TEMP_DEC[3] = -39;
    priv->TEMP_DEC[4] = -50;
    priv->TEMP_DEC[5] = -62;
    priv->TEMP_DEC[6] = -73;

    priv->TEMP_INC[0] = 7;
    priv->TEMP_INC[1] = 17;
    priv->TEMP_INC[2] = 26;
    priv->TEMP_INC[3] = 35;
    priv->TEMP_INC[4] = 43;
    priv->TEMP_INC[5] = 50;
    priv->TEMP_INC[6] = 58;
    priv->TEMP_INC[7] = 65;
    priv->TEMP_INC[8] = 73;
    priv->TEMP_INC[9] = 79;
    priv->TEMP_INC[10] = 85;

    
    priv->POWER_SAVE_ADJUST_EN = 1;

    priv->RX_POWER_LIMIT[0] = -25;
    priv->RX_POWER_LIMIT[1] = -28;
    priv->RX_POWER_LIMIT[2] = -31;
    priv->RX_POWER_LIMIT[3] = -34;
    priv->RX_POWER_LIMIT[4] = -37;
    priv->RX_POWER_LIMIT[5] = -40;
    priv->RX_POWER_LIMIT[6] = -43;
    priv->RX_POWER_LIMIT[7] = -46;
    priv->RX_POWER_LIMIT[8] = -49;
    priv->RX_POWER_LIMIT[9] = -52;
    priv->RX_POWER_LIMIT[10] = -55;
    priv->RX_POWER_LIMIT[11] = -58;
    priv->RX_POWER_LIMIT[12] = -61;
    priv->RX_POWER_LIMIT[13] = -64;
    priv->RX_POWER_LIMIT[14] = -67;
    priv->RX_POWER_LIMIT[15] = -70;
    priv->RX_POWER_LIMIT[16] = -73;
    priv->RX_POWER_LIMIT[17] = -76;

    priv->RF_GAIN_STEP[0] = 38;
    priv->RF_GAIN_STEP[1] = 33;
    priv->RF_GAIN_STEP[2] = 29;
    priv->RF_GAIN_STEP[3] = 24;
    priv->RF_GAIN_STEP[4] = 20;
    priv->RF_GAIN_STEP[5] = 15;
    priv->RF_GAIN_STEP[6] = 10;
    priv->RF_GAIN_STEP[7] = 6;
    priv->RF_GAIN_STEP[8] = 2;
    priv->RF_GAIN_STEP[9] = 0;
    priv->RF_GAIN_STEP[10] = 0;
    priv->RF_GAIN_STEP[11] = 0;
    priv->RF_GAIN_STEP[12] = 0;
    priv->RF_GAIN_STEP[13] = 0;
    priv->RF_GAIN_STEP[14] = 0;
    priv->RF_GAIN_STEP[15] = -5;
    priv->RF_GAIN_STEP[16] = -9;
    priv->RF_GAIN_STEP[17] = -13;
    priv->RF_GAIN_STEP[18] = -18;

    priv->RF_TX_GAIN_LIMIT = 0x39;
    priv->RF_CCK_GAIN_SCAN = 0x39;

    HWInitTemperture(priv);
    VariableSel(priv,1);   //default channal = 1 
    //HWDbgPrintReg(priv->hw_control);  //add by dmou for debug
    HWEEPROMReadRssiRefer(priv);
    /********************* HW INIT end ************************/

    usb_get_dev(udev);
    skb_queue_head_init(&priv->rx_queue);

    if(priv->mactype == BASEBAND_SCI9013 || priv->mactype == BASEBAND_SCI9012 )
    {
        memcpy(sci921x_rates, sci921x_rates_3, sizeof(sci921x_rates));
    }
    else
    {
        reg = HWGetCapInfo(priv->hw_control);
        reg &= 0x20;  //Short Preamble bit
        if(reg>>5)
            priv->short_pre = 1;
        else
            priv->short_pre = 0;
        INFO("sci9211: short_pre %d \n",priv->short_pre);   //11162011 dmou
    
        if(priv->short_pre)
            memcpy(sci921x_rates, sci921x_rates_1, sizeof(sci921x_rates));
        else
            memcpy(sci921x_rates, sci921x_rates_2, sizeof(sci921x_rates));
    }
    
    BUILD_BUG_ON(sizeof(priv->channels) != sizeof(sci921x_channels));
    BUILD_BUG_ON(sizeof(priv->rates) != sizeof(sci921x_rates));

    memcpy(priv->channels, sci921x_channels, sizeof(sci921x_channels));
    memcpy(priv->rates, sci921x_rates, sizeof(sci921x_rates));

    priv->band.band = IEEE80211_BAND_2GHZ;
    priv->band.channels = priv->channels;
    priv->band.n_channels = ARRAY_SIZE(sci921x_channels);
    priv->band.bitrates = priv->rates;
    priv->band.n_bitrates = ARRAY_SIZE(sci921x_rates);
    dev->wiphy->bands[IEEE80211_BAND_2GHZ] = &priv->band;
    dev->flags = IEEE80211_HW_HOST_BROADCAST_PS_BUFFERING | IEEE80211_HW_SIGNAL_DBM ;

    udelay(10);

    HWEEPROMReadMACAddr(priv->hw_control,(dev->wiphy->perm_addr),priv->modtype);  //06072012 dmou
    if (!is_valid_ether_addr(dev->wiphy->perm_addr)) {
        WAR("sci9211: Invalid hwaddr! Using randomly "
               "generated MAC address\n");
        random_ether_addr(dev->wiphy->perm_addr);
    }

    priv->rf = sci9211_detect_rf(dev);

    dev->extra_tx_headroom = sizeof(struct sci9211_tx_hdr);

    dev->queues = 4;
	
    /*
       must init mutex before ieee80211_register_hw 
       for after calling ieee80211_register_hw, the ieee80211_ops will be called immediately
    */
    mutex_init(&priv->conf_mutex);
	
    err = ieee80211_register_hw(dev);
    if (err) {
        ERR("Cannot register device\n");
        goto err_free_dev;
    }

    INFO("%s: sci9211 V%d + %s\n",
    wiphy_name(dev->wiphy),
    priv->asic_rev, priv->rf->name);
    sci_deb_hex("MAC ",dev->wiphy->perm_addr,ETH_ALEN);

    skb_queue_head_init(&priv->tx_status.queue); //dmou 05042011
	
    DEG("Exit func %s ^_^\n", __func__);
    return 0;

err_free_dev:
    HWClose(priv->hw_control);  //11182011 dmou
    usb_set_intfdata(intf, NULL);
    usb_put_dev(udev);
    ieee80211_free_hw(dev);
    
    ERR("Exit func %s @v@!! \n", __func__);
    return err;
}

static void __devexit sci9211_disconnect(struct usb_interface *intf)
{
    struct ieee80211_hw *dev = usb_get_intfdata(intf);
    struct sci9211_priv *priv = NULL;
	
    DEG("Enter func %s\n", __func__);
    if (!dev){
        ERR("Invaild argumemt, can not handle NULL pointer\n");
        return;
    }
    priv = dev->priv;
    INFO("[%s] \n",__func__);  //08022011 dmou

    priv->is_disconnect = 1;   //11212011 dmou
    INFO("[%s] usb disconnect ! \n",__func__);  

    ieee80211_unregister_hw(dev);
    INFO("[%s] after unregister ieee80211_hw\n",__func__);  //11212011 dmou
    
    HWClose(priv->hw_control);  //11182011 dmou
    INFO("[%s] after HW close \n",__func__);  //11212011 dmou

    usb_reset_device(priv->udev);     //07292011 dmou
    INFO("[%s] after reset device \n",__func__);  //11212011 dmou
    usb_put_dev(interface_to_usbdev(intf));
    INFO("[%s] after put usb dev \n",__func__);  //11212011 dmou
    ieee80211_free_hw(dev);
    INFO("[%s] after free ieee80211_hw 2\n",__func__);  //08022011 dmou
	
    DEG("Exit func %s\n", __func__);
}

#if USB_MONITER
static int sci9211_suspend(struct usb_interface *intf, pm_message_t message)
{
    DEG("%s \n",__func__);  //11182011 dmou
    return 0;
}
static int sci9211_resume(struct usb_interface *intf)
{
    DEG("%s \n",__func__);  //11182011 dmou
    return 0;
}
static int sci9211_reset_resume(struct usb_interface *intf)
{
    DEG("%s \n",__func__);  //11182011 dmou
    return 0;
}

static int sci9211_pre_reset(struct usb_interface *intf)
{
    DEG("%s \n",__func__);  //11182011 dmou
    return 0;
}
static int sci9211_post_reset(struct usb_interface *intf)
{
    DEG("%s \n",__func__);  //11182011 dmou
    return 0;
}
#endif  //USB_MONITER

static struct usb_driver sci9211_driver = {
    .name			= "sci9211",
    .id_table		= sci9211_table,
    .probe			= sci9211_probe,
    .disconnect		= __devexit_p(sci9211_disconnect),
#if USB_MONITER
    .suspend		= sci9211_suspend,
    .resume			= sci9211_resume,
    .reset_resume	= sci9211_reset_resume,
    .pre_reset		= sci9211_pre_reset,
    .post_reset		= sci9211_post_reset,
#endif
};

#if 1
extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);


struct wifi_gpio {
	int name;
	int active;
	unsigned int bmp;
	unsigned int ctraddr;
	unsigned int ocaddr;
	unsigned int odaddr;
};

static struct wifi_gpio l_wifi_gpio = {
	.name = 6,
	.active = 1,
	.bmp = (1<<0x6),
	.ctraddr = 0xD8110040 ,
	.ocaddr = 0xD8110080 ,
	.odaddr = 0xD81100C0 ,
};

static void excute_gpio_op(int on)
{

	unsigned int vibtime = 0;
	printk("wifi excute_gpio_op %d\n",on);
	REG32_VAL(l_wifi_gpio.ctraddr) |= l_wifi_gpio.bmp;
	REG32_VAL(l_wifi_gpio.ocaddr) |= l_wifi_gpio.bmp;


	if(on){
		if (l_wifi_gpio.active == 0)
			REG32_VAL(l_wifi_gpio.odaddr) &= ~l_wifi_gpio.bmp; /* low active */
		else
			REG32_VAL(l_wifi_gpio.odaddr) |= l_wifi_gpio.bmp; /* high active */
	}else{
		if (l_wifi_gpio.active == 0)
			REG32_VAL(l_wifi_gpio.odaddr) |= l_wifi_gpio.bmp;
		else
			REG32_VAL(l_wifi_gpio.odaddr) &= ~l_wifi_gpio.bmp;
	}


}
void wifi_power_ctrl(int open)
{
	int varlen = 127;
    int retval;
    char buf[200]={0};
	printk("wifi_power_ctrl %d\n",open);



        retval = wmt_getsyspara("wmt.gpo.wifi", buf, &varlen);
        if(!retval)
		{
			sscanf(buf, "%x:%x:%x:%x:%x:%x",
						       &(l_wifi_gpio.name),
						       &(l_wifi_gpio.active),
						       &(l_wifi_gpio.bmp),
						       &(l_wifi_gpio.ctraddr),
						       &(l_wifi_gpio.ocaddr),
						       &(l_wifi_gpio.odaddr));
			l_wifi_gpio.bmp = 1<<l_wifi_gpio.bmp;

			printk("wifi power up:%s\n", buf);
		}

	//add offset for M512
	l_wifi_gpio.ctraddr|=(GPIO_BASE_ADDR&0xFE000000);
	l_wifi_gpio.ocaddr|=(GPIO_BASE_ADDR&0xFE000000);
	l_wifi_gpio.odaddr|=(GPIO_BASE_ADDR&0xFE000000);		

		
	    excute_gpio_op(open);
		if(open){
			//wait 1 sec to hw init
			//printk("sleep 1 sec\n");
			msleep(1000);
		}
		

}
#endif
static int __init sci9211_init(void)
{
    int ret;
    DEG("Enter func %s\n", __func__);
	wifi_power_ctrl(1);

#if PS
    printk("[SCI].info : Load SCI921X driver V0.1.52-PS \n ");
#else
	printk("[SCI].info : Load SCI921X driver V0.1.52 \n ");
#endif
    /*#if (LINUX_VERSION_CODE == KERNEL_VERSION(2,6,35))
    	wifi_power(1);
    #endif*/
    ret = usb_register(&sci9211_driver);
    INFO("Exit func %s, retval: %d\n", __func__, ret);
    return ret;
}

static void __exit sci9211_exit(void)
{
    DEG("Enter func %s\n", __func__);
    /*#if (LINUX_VERSION_CODE == KERNEL_VERSION(2,6,35))
          wifi_power(0);
    #endif*/
    usb_deregister(&sci9211_driver);
    DEG("Exit func %s\n", __func__);
	wifi_power_ctrl(0);
}

module_init(sci9211_init);
module_exit(sci9211_exit);
#if 0
int rockchip_wifi_init_module(void)
{
	INFO("<------------LR----SCI9211WIFI INIT------------->\n");
    return sci9211_init();
}

void rockchip_wifi_exit_module(void)
{
	INFO("<------------LR----SCI9211WIFI EXIT------------->\n");

    sci9211_exit();
}

EXPORT_SYMBOL(rockchip_wifi_init_module);
EXPORT_SYMBOL(rockchip_wifi_exit_module);
#endif
