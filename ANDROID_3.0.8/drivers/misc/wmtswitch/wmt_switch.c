/*++ 
 * linux/sound/soc/codecs/vt1603.c
 * WonderMedia audio driver for ALSA
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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <asm/irq.h>
#include <linux/workqueue.h>
#include <linux/switch.h>
#include <linux/slab.h>

#include <mach/hardware.h>
#include <mach/wmt_gpio.h>
#include <mach/gpio-cs.h>

/////////////////////////////////////////////////////////////////

#define WMT_SWITCH_TIMER_INTERVAL 300 // ms

struct wmt_switch_priv {
	struct switch_dev sim_switch;
	struct switch_dev sim2_switch;
	struct switch_dev hs_switch;
	
	struct work_struct work;
	struct timer_list timer;
};

struct wmt_switch_gpo {
	int enable;
	int idx;
	int bit;
	int active;
};

static struct wmt_switch_gpo wmt_sim_detect;
static struct wmt_switch_gpo wmt_sim2_detect;
static struct wmt_switch_gpo wmt_hs_detect;

extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);

/////////////////////////////////////////////////////////////////

static void wmt_switch_do_work(struct work_struct *work)
{
	struct wmt_switch_priv *priv = container_of(work, struct wmt_switch_priv, work);
	static int old_sim_state = -1;
	static int old_sim2_state = -1;
	static int old_hs_state = -1;
	int state;

	if (wmt_sim_detect.enable) {
		state = gpio_get_value(wmt_sim_detect.idx, wmt_sim_detect.bit);
		if (old_sim_state != state) {
			if (state == wmt_sim_detect.active)
				switch_set_state(&priv->sim_switch, 1);
			else
				switch_set_state(&priv->sim_switch, 0);
		}
		old_sim_state = state;
	}

	if (wmt_sim2_detect.enable) {
		state = gpio_get_value(wmt_sim2_detect.idx, wmt_sim2_detect.bit);
		if (old_sim2_state != state) {
			if (state == wmt_sim2_detect.active)
				switch_set_state(&priv->sim2_switch, 1);
			else
				switch_set_state(&priv->sim2_switch, 0);
		}
		old_sim2_state = state;
	}

	if (wmt_hs_detect.enable) {
		state = gpio_get_value(wmt_hs_detect.idx, wmt_hs_detect.bit);
		if (old_hs_state != state) {
			if (state == wmt_hs_detect.active)
				switch_set_state(&priv->hs_switch, 1);
			else
				switch_set_state(&priv->hs_switch, 0);
		}
		old_hs_state = state;
	}
}

static void wmt_switch_timer_handler(unsigned long data)
{
	struct wmt_switch_priv *priv = (struct wmt_switch_priv *)data;
	
	schedule_work(&priv->work);
	mod_timer(&priv->timer, jiffies + WMT_SWITCH_TIMER_INTERVAL*HZ/1000);
}

static int __devinit wmt_switch_probe(struct platform_device *pdev)
{
	struct wmt_switch_priv *wmt_switch;

	wmt_switch = kzalloc(sizeof(struct wmt_switch_priv), GFP_KERNEL);
	if (wmt_switch == NULL)
		return -ENOMEM;
	platform_set_drvdata(pdev, wmt_switch);

	INIT_WORK(&wmt_switch->work, wmt_switch_do_work);
	init_timer(&wmt_switch->timer);
	wmt_switch->timer.data = (unsigned long)wmt_switch;
	wmt_switch->timer.function = wmt_switch_timer_handler;

	if (wmt_sim_detect.enable) {
		wmt_switch->sim_switch.name  = "sim";
		switch_dev_register(&wmt_switch->sim_switch);
	
		gpio_enable(wmt_sim_detect.idx, wmt_sim_detect.bit, INPUT);
	}

	if (wmt_sim2_detect.enable) {
		wmt_switch->sim2_switch.name  = "sim2";
		switch_dev_register(&wmt_switch->sim2_switch);
		
		gpio_enable(wmt_sim2_detect.idx, wmt_sim2_detect.bit, INPUT);
	}

	if (wmt_hs_detect.enable) {
		wmt_switch->hs_switch.name  = "h2w";
		switch_dev_register(&wmt_switch->hs_switch);
	
		gpio_enable(wmt_hs_detect.idx, wmt_hs_detect.bit, INPUT);
	}

	mod_timer(&wmt_switch->timer, jiffies + WMT_SWITCH_TIMER_INTERVAL*HZ/1000);
	
	return 0;
}

static int __devexit wmt_switch_remove(struct platform_device *pdev)
{
	struct wmt_switch_priv *priv = platform_get_drvdata(pdev);

	if (wmt_sim_detect.enable)
		switch_dev_unregister(&priv->sim_switch);
	if (wmt_sim2_detect.enable)
		switch_dev_unregister(&priv->sim2_switch);
	if (wmt_hs_detect.enable)
		switch_dev_unregister(&priv->hs_switch);
	
	del_timer_sync(&priv->timer);
	return 0;
}

static int wmt_switch_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct wmt_switch_priv *priv = platform_get_drvdata(pdev);
	
	del_timer_sync(&priv->timer);
	return 0;
}

static int wmt_switch_resume(struct platform_device *pdev)
{
	struct wmt_switch_priv *priv = platform_get_drvdata(pdev);
	
	mod_timer(&priv->timer, jiffies + 500*HZ/1000);
	return 0;
}

/////////////////////////////////////////////////////////////////

static struct platform_driver wmt_switch_driver = {
	.driver = {
		   .name = "wmt-switch",
		   .owner = THIS_MODULE,
		   },
	.probe   = wmt_switch_probe,
	.remove  = __devexit_p(wmt_switch_remove),
	.suspend = wmt_switch_suspend,
	.resume  = wmt_switch_resume,
};

static __init int wmt_switch_init(void)
{
	int ret, len = 64;
	char buf[64];
	
    // parse u-boot parameter
	ret = wmt_getsyspara("wmt.gpo.sim.detect", buf, &len);
	if (ret == 0) {
		sscanf(buf, "%d:%d:%d:%d", 
			&wmt_sim_detect.enable, &wmt_sim_detect.idx, &wmt_sim_detect.bit, &wmt_sim_detect.active);
	}
	else
		wmt_sim_detect.enable = 0;

	ret = wmt_getsyspara("wmt.gpo.sim2.detect", buf, &len);
	if (ret == 0) {
		sscanf(buf, "%d:%d:%d:%d", 
			&wmt_sim2_detect.enable, &wmt_sim2_detect.idx, &wmt_sim2_detect.bit, &wmt_sim2_detect.active);
	}
	else
		wmt_sim2_detect.enable = 0;

	ret = wmt_getsyspara("wmt.gpo.hs.detect", buf, &len);
	if (ret == 0) {
		sscanf(buf, "%d:%d:%d:%d", 
			&wmt_hs_detect.enable, &wmt_hs_detect.idx, &wmt_hs_detect.bit, &wmt_hs_detect.active);
	}
	else
		wmt_hs_detect.enable = 0;

	if (!wmt_sim_detect.enable && !wmt_sim2_detect.enable && !wmt_hs_detect.enable)
		return -EIO;
	
	return platform_driver_register(&wmt_switch_driver);
}
module_init(wmt_switch_init);

static __exit void wmt_switch_exit(void)
{
	platform_driver_unregister(&wmt_switch_driver);
}
module_exit(wmt_switch_exit);

MODULE_DESCRIPTION("WMT GPIO Switch");
MODULE_AUTHOR("WonderMedia Technologies, Inc.");
MODULE_LICENSE("GPL");
