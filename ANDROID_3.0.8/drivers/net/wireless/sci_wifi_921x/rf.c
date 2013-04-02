/*
 * Radio tuning for s102 on SCI9211
 *
 * Copyright 2010 Thomas Luo <shbluo@sci-inc.com.cn>
 * Copyright 2010 DongMei Ou <dmou@sci-inc.com.cn>
 *
 * Based on the r8187 driver, which is:
 * Copyright 2005 Andrea Merello <andreamrl@tiscali.it>, et al.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/usb.h>
#include <net/mac80211.h>

#include "sci9211.h"
#include "rf.h"
#include "MAC_Control.h"


void s102_rf_set_tx_power(struct ieee80211_hw *dev, int channel)
{
    struct sci9211_priv *priv = dev->priv;
    u8 cck_power, ofdm_power;

    cck_power = priv->channels[channel - 1].hw_value & 0xF;
    ofdm_power = priv->channels[channel - 1].hw_value >> 4;

    cck_power = min(cck_power, (u8)11);
    if (ofdm_power > (u8)15)
        ofdm_power = 25;
    else
        ofdm_power += 10;

    msleep(1); // FIXME: optional?

    /* anaparam2 on */


    msleep(1);
}

void s102_rf_init(struct ieee80211_hw *dev)
{
    return;
}

void s102_rf_stop(struct ieee80211_hw *dev)
{
    return;
}

void s102_rf_set_channel(struct ieee80211_hw *dev,
                   struct ieee80211_conf *conf)
{
    struct sci9211_priv *priv = dev->priv;
    int chan = ieee80211_frequency_to_channel(conf->channel->center_freq);

    //INFO("%s :chan %d \n",__func__,chan);

    if (priv->rf->init == s102_rf_init)
        s102_rf_set_tx_power(dev, chan);

    HWSwitchChannel(priv->hw_control,chan);
    msleep(10);
}

static const struct sci921x_rf_ops s102_ops = {
    .name       = "s102",
    .init       = s102_rf_init,
    .stop       = s102_rf_stop,
    .set_chan   = s102_rf_set_channel
};

void s103_rf_set_tx_power(struct ieee80211_hw *dev, int channel)
{
    struct sci9211_priv *priv = dev->priv;
    u8 cck_power, ofdm_power;

    cck_power = priv->channels[channel - 1].hw_value & 0xF;
    ofdm_power = priv->channels[channel - 1].hw_value >> 4;

    cck_power = min(cck_power, (u8)11);
    if (ofdm_power > (u8)15)
        ofdm_power = 25;
    else
        ofdm_power += 10;
}

#if MORE_AP
static void sci9211_scan_work(struct work_struct *work)  //dmou 05042011
{
    struct sci9211_priv *priv = container_of(work, struct sci9211_priv,
                    scan_work);
    unsigned int ret;

    mutex_lock(&priv->conf_mutex);

    //sci921x_iowrite32(priv,(__le32 *)0x0900,0x00000007);
    if((priv->modtype == 0x001B) ||( priv->modtype == 0x001C))
    {
        ret = sci921x_ioread32(priv,(__le32 *)0x0B00);
        ret &= 0xFF80FFFF;
        ret |= 0x00600000; 
        sci921x_iowrite32(priv,(__le32 *)0x0B00,ret);  //R_ED_CTL_WORD "0X68"
        INFO("%s  0x0B00  : 0x%x \n",__func__,ret);
    }
    mutex_unlock(&priv->conf_mutex);
}

int scan_timer_func(unsigned long data)
{
    struct sci9211_priv *priv = (struct sci9211_priv *)data;
    
    //DEG("Enter func %s\n", __func__);
    if (priv == NULL ) {
        ERR("%s called with NULL data\n",__func__);
        return -EINVAL;
    }
    schedule_work(&priv->scan_work);

    return 0;
}
#endif

void s103_rf_set_channel(struct ieee80211_hw *dev,
                   struct ieee80211_conf *conf)
{
    struct sci9211_priv *priv = dev->priv;
    int chan = ieee80211_frequency_to_channel(conf->channel->center_freq);
#if MORE_AP
    int ret;
#endif
    INFO("%s :chan %d \n",__func__,chan);

    if (priv->rf->init == s103_rf_init)
        s103_rf_set_tx_power(dev, chan);

    HWSwitchChannel(priv->hw_control,chan);

    if(priv->assoc_flag)
    {
        priv->chan_current = chan;
        if(priv->chan_current != priv->chan_last)
        {
            VariableSel(priv,chan);
            priv->chan_last = priv->chan_current;
        }
    }

#if MORE_AP
    if(priv->scan_flag)
    {
        //INFO("%s  jiffies  : %ld \n",__func__,jiffies);
        if (timer_pending(&priv->scan_timer)) 
        {
            //priv->scan_timer.expires = jiffies + HZ/5;
            INFO("%s  mod_timer  \n",__func__);
            mod_timer(&priv->scan_timer,jiffies + HZ/10);
        }else{
            if((priv->modtype == 0x001B) ||( priv->modtype == 0x001C))
            {
                ret = sci921x_ioread32(priv,(__le32 *)0x0B00);
                ret &= 0xFF80FFFF;
                ret |= 0x00680000; //0x00680000
                sci921x_iowrite32(priv,(__le32 *)0x0B00,ret);  //R_ED_CTL_WORD "0X68"
                INFO("%s  0x0B00  : 0x%x ( %d tick /HZ:%d tick )\n",__func__,ret,(HZ/10),HZ);
            }

            priv->scan_timer.expires = jiffies + HZ/10;
            INFO("%s  add_timer  \n",__func__);
            add_timer(&priv->scan_timer);
        }
    }else
#endif
        msleep(10);

}

void s103_rf_stop(struct ieee80211_hw *dev)
{
#if MORE_AP
    struct sci9211_priv *priv = dev->priv;
    /*u32 ret;

    sci921x_iowrite32(priv,(__le32 *)0X0900,0X00000000);

    ret = sci921x_ioread32(priv,(__le32 *)0X090c);
    ret &= 0xFFFF1000;
    ret |= 0x27DF;
    sci921x_iowrite32(priv,(__le32 *)0X090c,ret);
*/

    cancel_work_sync(&priv->scan_work);
    INFO("%s ----->  cancel_scan_work_sync \n",__func__);  //08182012 dmou
    del_timer(&priv->scan_timer); //dmou
    INFO("%s ----->  del_scan_timer \n",__func__);  //08182012 dmou
#endif

    return;
}

void s103_rf_init(struct ieee80211_hw *dev)
{
#if MORE_AP
    struct sci9211_priv *priv = dev->priv;
    /*u32 ret;

    sci921x_iowrite32(priv,(__le32 *)0X0900,0X00000037);

    ret = sci921x_ioread32(priv,(__le32 *)0x090c);
    ret |= 0x6C10;
    ret &= 0xFFFFFFDF;
    sci921x_iowrite32(priv,(__le32 *)0x090c,ret);
*/

    init_timer(&priv->scan_timer);
    priv->scan_timer.data = (unsigned long) priv;
    priv->scan_timer.function =(void *)scan_timer_func;
    priv->scan_timer.expires = jiffies + HZ/10;

    INIT_WORK(&priv->scan_work,sci9211_scan_work);
#endif
    return;
}

static const struct sci921x_rf_ops s103_ops = {
    .name       = "s103",
    .init       = s103_rf_init,
    .stop       = s103_rf_stop,
    .set_chan   = s103_rf_set_channel
};

const struct sci921x_rf_ops * sci9211_detect_rf(struct ieee80211_hw *dev)
{
    struct sci9211_priv *priv = dev->priv;

	return &s103_ops;

	INFO("priv->hw_control->rftype == %d\n", priv->hw_control->rftype);
    if((priv->hw_control->rftype == RFIC_SCIS1022) || (priv->hw_control->rftype == RFIC_SCIS102))
    {
        return &s102_ops;
    }
    else if(priv->hw_control->rftype == RFIC_SCIS1032)
    {
        return &s103_ops;
    }else
    {
        return NULL;
    }
}
