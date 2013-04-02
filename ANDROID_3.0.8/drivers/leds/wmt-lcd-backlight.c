/**************************************************************
Copyright (c) 2008 WonderMedia Technologies, Inc.

Module Name:
    $Workfile: wmt-lcd-backlight.c $
Abstract:
    This program is the WMT LCD backlight driver for Android 1.6 system.
    Andriod1.6 API adjusts the LCD backlight by writing follwing file:
        /sys/class/leds/lcd-backlight/brightness
    Use WMT PWM to control the LCD backlight
Revision History:
    Jan.08.2010 First Created by HowayHuo
	
**************************************************************/
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#include "../char/wmt-pwm.h"
#include "../video/wmt/lcd.h"

static int s_pwm_no;  // 0 ~ 3: PWM0 ~ PWM3
static unsigned int s_pwm_period;
struct wmt_pwm_reg_t {
	unsigned int scalar;
	unsigned int period;
	unsigned int duty;
	unsigned int config;
};

struct wmt_brightness_range_t{
	 int  flag;
     int  actual_min_bright;        
     int  actual_middle_bright;   
     int  actual_max_bright;   
     int  fake_min_bright;   
     int  fake_middle_bright;   
     int  fake_max_bright;    
};  

struct led_operation_t {
	unsigned int light;
	unsigned int act;
	unsigned int bitmap;
	unsigned int ctl;
	unsigned int oc;
	unsigned int od;
};

static struct wmt_brightness_range_t s_range;
static int s_limit_brightness_range; 
static int s_enable_led_control;

static struct wmt_pwm_reg_t s_pwm_setting;
static int s_skip_backlight = 1;
static struct proc_dir_entry *s_led_proc_file;
static int s_hdmi_enable = 1;
struct led_operation_t s_led_control_pin;

#define BIT_DONOT_CLOSEBRIGHT_WHEN_LESSTHAN_MINBRIGHT BIT0 // don' t close baclk light when backlight less than minimal backlight
#define LED_PROC_FILE "logoled"
#define ENV_LOGO_LED_CONTROL "wmt.gpo.logoled"
#define GPIO_PHY_BASE_ADDR (GPIO_BASE_ADDR-WMT_MMAP_OFFSET)

static void dump_brightness_range_paramter(void)
{
	printk(KERN_INFO "brightness range change: [%d,%d,%d] -> [%d,%d,%d], flag = 0x%x\n", 
		s_range.actual_min_bright,
		s_range.actual_middle_bright,
		s_range.actual_max_bright,
		s_range.fake_min_bright,
		s_range.fake_middle_bright,
		s_range.fake_max_bright,
		s_range.flag);	
}

static void parse_brightness_range_paramter(void)
{
	int ret;
    char buf[64];                                                                                                  
    char varname[] = "wmt.display.brightrange";                                                                                
    int varlen = 64;

	s_limit_brightness_range = 0;
	
	ret = wmt_getsyspara(varname, buf, &varlen);
	if(ret)		
		return;

	s_range.actual_min_bright = 50;
	s_range.actual_middle_bright = 162;
	s_range.actual_max_bright = 255;
	
	ret = sscanf(buf,"%x:%d:%d:%d",
		&s_range.flag,
		&s_range.fake_min_bright,																			  
		&s_range.fake_middle_bright,																			  
		&s_range.fake_max_bright);
	
	//printk(KERN_INFO "parse_brightness_range_paramter: ret = %d\n", ret);	
	if(ret != 4)
	{
		printk(KERN_ERR "Invalid brightness range paramter 1, the format of parameter [%s] is wrong\n", buf);
		return;
	}
	
	dump_brightness_range_paramter();

	//only middle bright can less than zero. If middle_bright < 0, the middle bright is ignored.
	if(s_range.fake_min_bright < 0 || s_range.fake_max_bright < 0)
	{
		//dump_brightness_range_paramter();
		printk(KERN_ERR "Invalid brightness range paramter 2, min_bright < 0 or max_bright < 0\n");
		return;
	}

	if(s_range.fake_max_bright > 255)
	{
		//dump_brightness_range_paramter();
		printk(KERN_ERR "Invalid brightness range paramter 3, max_bright > 255\n");
		return;
	}

	if(s_range.fake_middle_bright >= 0)
	{
		if(s_range.fake_middle_bright <= s_range.fake_min_bright 
			|| s_range.fake_max_bright <= s_range.fake_middle_bright)
		{
			//dump_brightness_range_paramter();
			printk(KERN_ERR "Invalid brightness range paramter 4, middle_bright <= min_bright or max_bright <= middle_bright\n");
			return;
		}			
	}
	else //middle_bright < 0, ignore middle_bright
	{
		if(s_range.fake_max_bright <= s_range.fake_min_bright)
		{
			//dump_brightness_range_paramter();
			printk(KERN_ERR "Invalid brightness range paramter 5, max_brgiht <= min_bright\n");
			return;
		}
	}
	
	s_limit_brightness_range = 1;
	
}
	
static void adjust_brightness_in_limited_range(enum led_brightness *p)
{
	if(s_limit_brightness_range)
	{
		if(s_range.fake_middle_bright < 0) // the middle bright isn't set
		{
			if (*p > 0 && *p < s_range.actual_min_bright)
			{
				if(!(s_range.flag & BIT_DONOT_CLOSEBRIGHT_WHEN_LESSTHAN_MINBRIGHT))
					*p = 0;
			}
			else if(*p >= s_range.actual_min_bright && *p <= s_range.actual_max_bright)
				*p = s_range.fake_min_bright  + (*p - s_range.actual_min_bright) 
				* (s_range.fake_max_bright - s_range.fake_min_bright) 
				/ (s_range.actual_max_bright - s_range.actual_min_bright);				
		}
		else // the middle bright is set
		{
			if (*p > 0 && *p < s_range.actual_min_bright)
			{
				if(!(s_range.flag & BIT_DONOT_CLOSEBRIGHT_WHEN_LESSTHAN_MINBRIGHT))
					*p = 0;
			}
			else if(*p >= s_range.actual_min_bright && *p <= s_range.actual_middle_bright)
				*p = s_range.fake_min_bright + (*p - s_range.actual_min_bright)
				* (s_range.fake_middle_bright - s_range.fake_min_bright)
				/ (s_range.actual_middle_bright - s_range.actual_min_bright);
			else if(*p > s_range.actual_middle_bright && *p <= s_range.actual_max_bright)
				*p = s_range.fake_middle_bright + (*p - s_range.actual_middle_bright)
				* (s_range.fake_max_bright - s_range.fake_middle_bright)
				/ (s_range.actual_max_bright - s_range.actual_middle_bright);				
		}
	}	
}
/*
extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);

struct back_light_limit {
	int max;
	int min;
	int diff;
	int step;
};

struct back_light_limit g_backlight_limit;

void backlight_get_env(void)
{
	unsigned char buf[100];
	int varlen = 100;
	int max = -1, min = -1;
	int val = -1;

	memset((char *)&g_backlight_limit, 0, sizeof(struct back_light_limit));

	g_backlight_limit.max = 90;
	g_backlight_limit.min = 10;
}
*/

static void logo_led_light(int light);

/*
 * For simplicity, we use "brightness" as if it were a linear function
 * of PWM duty cycle.  However, a logarithmic function of duty cycle is
 * probably a better match for perceived brightness: two is half as bright
 * as four, four is half as bright as eight, etc
 */
static void lcd_brightness(struct led_classdev *cdev, enum led_brightness b)
{
    unsigned int duty,  pwm_enable;

	printk(KERN_ALERT "[%s]pwm_no = %d pwm_period = %d b= %d\n", __func__, s_pwm_no, s_pwm_period, b);
	if(s_skip_backlight)
	{
		s_skip_backlight = 0;
		//The Android will set duty to 0 at start up. It will result in the LCD flick. So we skip the first time. 
		if(b == 0)
		{
			printk("skip adjust backlight at the first time\n");
			return;
		}
	}


	if(s_limit_brightness_range)
	{
		adjust_brightness_in_limited_range(&b);
	}	


	duty = (b *  s_pwm_period) / 255;
    if(duty) {
		if(--duty > 0xFFF)
	    	duty = 0xFFF;
    }


    pwm_set_duty(s_pwm_no, duty);
	pwm_enable = pwm_get_enable(s_pwm_no);
	
    if(duty) {
		if(!pwm_enable) {
			printk(KERN_ALERT "[%s]lcd turn on -->\n",__func__);
			pwm_set_control(s_pwm_no, 0x5);
			pwm_set_gpio(s_pwm_no,1);
			
			if(g_vpp.virtual_display)
			{
				if(!s_hdmi_enable)
				{
					hdmi_resume(1);
					s_hdmi_enable = 1;
				}
			}

			cs8556_enable();
			
			if(s_enable_led_control)
				logo_led_light(1);
        }
    } else {
    	if(pwm_enable) {
        	printk(KERN_ALERT "[%s]lcd turn off  <--\n",__func__);
			pwm_set_gpio(s_pwm_no,0);
			pwm_set_control(s_pwm_no,0x8);
			
			if(g_vpp.virtual_display)
			{
				if(hdmi_get_plugin())
				{
					hdmi_suspend(0);
					s_hdmi_enable = 0;
				}
			}
			
			cs8556_disable();
			
			if(s_enable_led_control)
				logo_led_light(0);
    	}
    }
    return;
}

/*
 * NOTE:  we reuse the platform_data structure of GPIO leds,
 * but repurpose its "gpio" number as a PWM channel number.
 */
static int lcd_backlight_probe(struct platform_device *pdev)
{
	const struct gpio_led_platform_data	*pdata;
	struct led_classdev				*leds;
	int					i;
	int					status;
	unsigned int brightness;

	pdata = pdev->dev.platform_data;
	if (!pdata || pdata->num_leds < 1)
		return -ENODEV;

	leds = kcalloc(pdata->num_leds, sizeof(*leds), GFP_KERNEL);
	if (!leds)
		return -ENOMEM;

	auto_pll_divisor(DEV_PWM,CLK_ENABLE,0,0);
	s_pwm_no = lcd_blt_id;

	if((s_pwm_no < 0) || (s_pwm_no > 3)) //WM8850 only has 4 PWM
	    s_pwm_no = 0; //default set to 0 when pwm_no is invalid

	s_pwm_period  = pwm_get_period(s_pwm_no) + 1;

/*
	backlight_get_env();
	g_backlight_limit.diff = (g_backlight_limit.max - g_backlight_limit.min);
	g_backlight_limit.max = (pwm_period*g_backlight_limit.max)/100;
	g_backlight_limit.min = (pwm_period*g_backlight_limit.min)/100;
	g_backlight_limit.step = (g_backlight_limit.max - g_backlight_limit.min) / g_backlight_limit.diff;
*/

	/*calculate the default brightness*/
	brightness = ((pwm_get_duty(s_pwm_no) + 1) * 255) / s_pwm_period;

	for (i = 0; i < pdata->num_leds; i++) {
		struct led_classdev		*led = leds + i;
		const struct gpio_led	*dat = pdata->leds + i;

		led->name = dat->name;
		led->brightness = brightness;
		led->brightness_set = lcd_brightness;
		led->default_trigger = dat->default_trigger;

		/* Hand it over to the LED framework */
		status = led_classdev_register(&pdev->dev, led);
		if (status < 0) {
			goto err;
		}
	}

	platform_set_drvdata(pdev, leds);

	parse_brightness_range_paramter();
	
	return 0;

err:
	if (i > 0) {
		for (i = i - 1; i >= 0; i--) {
			led_classdev_unregister(leds + i);
		}
	}
	kfree(leds);

	return status;
}

static int lcd_backlight_remove(struct platform_device *pdev)
{
	const struct gpio_led_platform_data	*pdata;
	struct led_classdev				*leds;
	unsigned				i;

	pdata = pdev->dev.platform_data;
	leds = platform_get_drvdata(pdev);

	for (i = 0; i < pdata->num_leds; i++) {
		struct led_classdev		*led = leds + i;

		led_classdev_unregister(led);
	}

	kfree(leds);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static int lcd_backlight_suspend
(
	struct platform_device *pdev,     /*!<; // a pointer point to struct device */
	pm_message_t state		/*!<; // suspend state */
)
{
	unsigned int addr;

	addr = PWM_CTRL_REG_ADDR + (0x10 * s_pwm_no);
	s_pwm_setting.config = (REG32_VAL(addr) & 0xFF);

	addr = PWM_PERIOD_REG_ADDR + (0x10 * s_pwm_no);
	s_pwm_setting.period = (REG32_VAL(addr) & 0xFFF);
	
	addr = PWM_SCALAR_REG_ADDR + (0x10 * s_pwm_no);
	s_pwm_setting.scalar = (REG32_VAL(addr) & 0xFFF);

	addr = PWM_DUTY_REG_ADDR + (0x10 * s_pwm_no);
	s_pwm_setting.duty = (REG32_VAL(addr) & 0xFFF);

	pwm_set_duty(s_pwm_no, 0);

	pwm_set_control(s_pwm_no,0x8);
	pwm_set_gpio(s_pwm_no, 0);
	//pwm_set_enable(s_pwm_no,0);	
	//g_pwm_setting.duty = 0; // for android , AP will set duty to 0

	return 0;
}

static int lcd_backlight_resume
(
	struct platform_device *pdev 	/*!<; // a pointer point to struct device */
)
{

	//if( (REG32_VAL(PMCEL_ADDR) & BIT10) == 0 )	// check pwm power on
	//	printk("PWM power is off!!!!!!!!!\n");

	pwm_set_scalar(s_pwm_no, s_pwm_setting.scalar);
	pwm_set_period(s_pwm_no, s_pwm_setting.period);
	pwm_set_duty(s_pwm_no, s_pwm_setting.duty);

	if(s_pwm_setting.duty == 0)
	{
		pwm_set_gpio(s_pwm_no,0);
		pwm_set_control(s_pwm_no, 0x8);
	}
	else
	{
		pwm_set_control(s_pwm_no, 0x5);
		pwm_set_gpio(s_pwm_no,1);
	}

	//pwm_set_duty(pwm_no, 1);
	//pwm_set_control(pwm_no, g_pwm_setting.config);
	//pwm_set_gpio(pwm_no, 1);

	return 0;
}

static int led_proc_write( struct file   *file,
                           const char    *buffer,
                           unsigned long count,
                           void          *data )
{
    char value[20];
    int len = count;
	unsigned int duty,  pwm_enable;
	 
    if( len >= sizeof(value))
        len = sizeof(value) - 1;

    if(copy_from_user(value, buffer, len))
        return -EFAULT;

    value[len] = '\0';

    //printk("procfile_write get %s\n", value);

    if(value[0] == '0')
		s_enable_led_control = 0;
	else
		s_enable_led_control = 1;

	duty = pwm_get_duty(s_pwm_no);
	pwm_enable = pwm_get_enable(s_pwm_no);

	if(s_enable_led_control)
	{
    	if(duty && pwm_enable)
			logo_led_light(1);
		else
			logo_led_light(0);
	}
	else
		logo_led_light(0);
	
    return count;
}

static int led_proc_read(char *page, char **start, off_t off, int count,
    int *eof, void *data) {
    
	int len;

	len = sprintf(page, "%d\n", s_enable_led_control);
	
	return len;
}


static void logo_led_light(int light)
{
	unsigned char val;

	if ((s_led_control_pin.ctl == 0) || (s_led_control_pin.oc == 0) 
		|| (s_led_control_pin.od == 0)) 
		return;

	val = 1 << s_led_control_pin.bitmap;
	
	if (s_led_control_pin.act == 0)
		light = ~light;

	if (light) {
		REG8_VAL(s_led_control_pin.od) |= val;
	} else {
		REG8_VAL(s_led_control_pin.od) &= ~val;
	}

	REG8_VAL(s_led_control_pin.oc) |= val;
	REG8_VAL(s_led_control_pin.ctl) |= val; 
}

static struct proc_dir_entry * create_led_proc_file(void)
{
    s_led_proc_file = create_proc_entry(LED_PROC_FILE, 0666, NULL);

    if(s_led_proc_file != NULL )
    {
        s_led_proc_file->write_proc = led_proc_write;
		s_led_proc_file->read_proc =  led_proc_read;
    }
    else
        printk("[wmt-lcd-backlight]Can not create /proc/%s file", LED_PROC_FILE);
    return s_led_proc_file;
}

static int logo_led_init(void)
{
	unsigned char buf[100];
	int buflen = 100;
	
	if(wmt_getsyspara(ENV_LOGO_LED_CONTROL, buf, &buflen))
		return -1;
	
	memset(&s_led_control_pin, 0, sizeof(struct led_operation_t));
		
	sscanf(buf,"%x:%x:%x:%x:%x:%x",&s_led_control_pin.light, &s_led_control_pin.act, \
		&s_led_control_pin.bitmap, &s_led_control_pin.ctl, \
		&s_led_control_pin.oc, &s_led_control_pin.od);

	if(!s_led_control_pin.ctl || !s_led_control_pin.oc || !s_led_control_pin.od)
		return -1;

	if ((s_led_control_pin.ctl&0xffff0000) == GPIO_PHY_BASE_ADDR)
		s_led_control_pin.ctl += WMT_MMAP_OFFSET;
	if ((s_led_control_pin.oc&0xffff0000) == GPIO_PHY_BASE_ADDR)
		s_led_control_pin.oc += WMT_MMAP_OFFSET;
	if ((s_led_control_pin.od&0xffff0000) == GPIO_PHY_BASE_ADDR)
		s_led_control_pin.od += WMT_MMAP_OFFSET;

	create_led_proc_file();	
		
	return 0;
}

static struct gpio_led s_lcd_pwm[] = {
	{
		.name			= "lcd-backlight",
	}, 
};


static struct gpio_led_platform_data lcd_backlight_data = {
	.leds		= s_lcd_pwm,
	.num_leds	= ARRAY_SIZE(s_lcd_pwm),
};

static struct platform_device lcd_backlight_device = {
	.name           = "lcd-backlight",
	.id             = 0,
	.dev            = 	{	.platform_data = &lcd_backlight_data,			
	},
};

static struct platform_driver lcd_backlight_driver = {
	.driver = {
		.name =		"lcd-backlight",
		.owner =	THIS_MODULE,
	},
	/* REVISIT add suspend() and resume() methods */
	.probe	=	lcd_backlight_probe,
	.remove =	lcd_backlight_remove, //__exit_p(lcd_backlight_remove),
	.suspend        = lcd_backlight_suspend,
	.resume         = lcd_backlight_resume
};

static int __init lcd_backlight_init(void)
{
	int ret;		

	ret = platform_device_register(&lcd_backlight_device);
	if(ret) {
		printk("[lcd_backlight_init]Error: Can not register LCD backlight device\n");
		return ret;
	}
	//ret = platform_driver_probe(&lcd_backlight_driver, lcd_backlight_probe);
	ret = platform_driver_register(&lcd_backlight_driver);
	if(ret) {
		printk("[lcd_backlight_init]Error: Can not register LCD backlight driver\n");
		platform_device_unregister(&lcd_backlight_device);
		return ret;
	}

	logo_led_init();
	
	return 0; 
}
module_init(lcd_backlight_init);

static void __exit lcd_backlight_exit(void)
{
	platform_driver_unregister(&lcd_backlight_driver);
	platform_device_unregister(&lcd_backlight_device);
}
module_exit(lcd_backlight_exit);

MODULE_DESCRIPTION("Driver for LCD with PWM-controlled brightness");
MODULE_LICENSE("GPL");

