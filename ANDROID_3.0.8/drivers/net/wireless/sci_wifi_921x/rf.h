
/*
 * Radio tuning definitions for s102 on SCI9211
 *
 *
 * Based on the r8187 driver, which is:
 * Copyright 2005 Andrea Merello <andreamrl@tiscali.it>, et al.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef RF_H
#define RF_H

const struct sci921x_rf_ops * sci9211_detect_rf(struct ieee80211_hw *dev);
void s102_rf_init(struct ieee80211_hw *dev);
void s102_rf_set_channel(struct ieee80211_hw *dev,struct ieee80211_conf *conf);
void s102_rf_stop(struct ieee80211_hw *dev);
void s102_rf_set_tx_power(struct ieee80211_hw *dev, int channel);

void s103_rf_init(struct ieee80211_hw *dev);
void s103_rf_set_channel(struct ieee80211_hw *dev,struct ieee80211_conf *conf);
void s103_rf_stop(struct ieee80211_hw *dev);
void s103_rf_set_tx_power(struct ieee80211_hw *dev, int channel);

#endif /* RF_H */
