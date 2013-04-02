/* drivers/input/touchscreen/zet6221_i2c.c
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * ZEITEC Semiconductor Co., Ltd
 * Tel: +886-3-579-0045
 * Fax: +886-3-579-9960
 * http://www.zeitecsemi.com
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/jiffies.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/errno.h>

#include <linux/input/mt.h>
#include "wmt_ts.h"
//fw update.
//#include "zet6221_fw.h"

/* -------------- global variable definition -----------*/
#define _MACH_MSM_TOUCH_H_

#define ZET_TS_ID_NAME "zet6221-ts"

#define MJ5_TS_NAME "zet6221_touchscreen"

//#define TS_INT_GPIO		S3C64XX_GPN(9)  /*s3c6410*/
//#define TS1_INT_GPIO        AT91_PIN_PB17 /*AT91SAM9G45 external*/
//#define TS1_INT_GPIO        AT91_PIN_PA27 /*AT91SAM9G45 internal*/
//#define TS_RST_GPIO		S3C64XX_GPN(10)

//#define MT_TYPE_B

#define TS_RST_GPIO
#define TPINFO	1
#define X_MAX	800 //1024
#define Y_MAX	480 //576
#define FINGER_NUMBER 5
#define KEY_NUMBER 3 //0
//#define P_MAX	1
#define P_MAX	255 //modify 2013-1-1
#define D_POLLING_TIME	25000
#define U_POLLING_TIME	25000
#define S_POLLING_TIME  100
#define REPORT_POLLING_TIME  5

#define MAX_KEY_NUMBER      	8
#define MAX_FINGER_NUMBER	16
#define TRUE 		1
#define FALSE 		0

//#define debug_mode 1
//#define DPRINTK(fmt,args...)	do { if (debug_mode) printk(KERN_EMERG "[%s][%d] "fmt"\n", __FUNCTION__, __LINE__, ##args);} while(0)

//#define TRANSLATE_ENABLE 1
#define TOPRIGHT 	0
#define TOPLEFT  	1
#define BOTTOMRIGHT	2
#define BOTTOMLEFT	3
#define ORIGIN		BOTTOMRIGHT

#define TIME_CHECK_CHARGE 3000

struct msm_ts_platform_data {
	unsigned int x_max;
	unsigned int y_max;
	unsigned int pressure_max;
};

struct zet6221_tsdrv {
	struct i2c_client *i2c_ts;
	struct work_struct work1;
	struct input_dev *input;
	struct timer_list polling_timer;
	struct delayed_work work; // for polling
	struct workqueue_struct *queue;
	struct early_suspend early_suspend;
	unsigned int gpio; /* GPIO used for interrupt of TS1*/
	unsigned int irq;
	unsigned int x_max;
	unsigned int y_max;
	unsigned int pressure_max;
};

struct zet6221_tsdrv * l_ts = NULL;
static int l_suspend = 0; // 1:suspend, 0:normal state

static int resetCount = 0;          //albert++ 20120807


//static u16 polling_time = S_POLLING_TIME;

static int l_powermode = -1;
static struct mutex i2c_mutex;


//static int __devinit zet6221_ts_probe(struct i2c_client *client, const struct i2c_device_id *id);
//static int __devexit zet6221_ts_remove(struct i2c_client *dev);

extern int  zet6221_downloader( struct i2c_client *client/*, unsigned short ver, unsigned char * data */);
extern u8 zet6221_ts_version(void);
extern u8 zet6221_ts_get_report_mode_t(struct i2c_client *client);
extern int zet6221_load_fw(void);
extern int zet6221_free_fwmem(void);

void zet6221_ts_charger_mode_disable(void);
void zet6221_ts_charger_mode(void);




static int filterCount = 0; 
static u32 filterX[MAX_FINGER_NUMBER][2], filterY[MAX_FINGER_NUMBER][2]; 

static u8  key_menu_pressed = 0x1;
static u8  key_back_pressed = 0x1;
static u8  key_search_pressed = 0x1;

static u16 ResolutionX=X_MAX;
static u16 ResolutionY=Y_MAX;
static u16 FingerNum=0;
static u16 KeyNum=0;
static int bufLength=0;	
static u8 xyExchange=0;
static u16 inChargerMode = 0;
static struct i2c_client *this_client;
struct workqueue_struct *ts_wq = NULL;
static int l_tskey[4][2] = {
	{KEY_BACK,0},
	{KEY_MENU,0},
	{KEY_HOME,0},
	{KEY_SEARCH,0},
};

u8 pc[8];
// {IC Model, FW Version, FW version,Codebase Type=0x08, Customer ID, Project ID, Config Board No, Config Serial No}

//Touch Screen
/*static const struct i2c_device_id zet6221_ts_idtable[] = {
       { ZET_TS_ID_NAME, 0 },
       { }
};

static struct i2c_driver zet6221_ts_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name  = ZET_TS_ID_NAME,
	},
	.probe	  = zet6221_ts_probe,
	.remove		= __devexit_p(zet6221_ts_remove),
	.id_table = zet6221_ts_idtable,
};
*/

void zet6221_set_tskey(int index,int key)
{
	l_tskey[index][0] = key;
}

extern unsigned int wmt_bat_is_batterypower(void);
/***********************************************************************
    [function]: 
		        callback: Timer Function if there is no interrupt fuction;
    [parameters]:
			    arg[in]:  arguments;
    [return]:
			    NULL;
************************************************************************/

static void polling_timer_func(struct work_struct *work)
{
	struct zet6221_tsdrv *ts = l_ts;
	//schedule_work(&ts->work1);
	//queue_work(ts_wq,&ts->work1);
	//dbg("check mode!\n");
	if (wmt_bat_is_batterypower() != l_powermode)
	{
		mutex_lock(&i2c_mutex);
		if (wmt_bat_is_batterypower())
		{
			klog("disable_mode\n");
			zet6221_ts_charger_mode_disable();
		} else {
			klog("charge mode\n");
			zet6221_ts_charger_mode();
		}
		mutex_unlock(&i2c_mutex);
		l_powermode = wmt_bat_is_batterypower();
	}
	queue_delayed_work(ts->queue, &ts->work, msecs_to_jiffies(TIME_CHECK_CHARGE));

	
	
	//mod_timer(&ts->polling_timer,jiffies + msecs_to_jiffies(TIME_CHECK_CHARGE));
}

extern int wmt_i2c_xfer_continue_if_4(struct i2c_msg *msg, unsigned int num,int bus_id);
/***********************************************************************
    [function]: 
		        callback: read data by i2c interface;
    [parameters]:
			    client[in]:  struct i2c_client — represent an I2C slave device;
			    data [out]:  data buffer to read;
			    length[in]:  data length to read;
    [return]:
			    Returns negative errno, else the number of messages executed;
************************************************************************/
int zet6221_i2c_read_tsdata(struct i2c_client *client, u8 *data, u8 length)
{
	struct i2c_msg msg;
	msg.addr = client->addr;
	msg.flags = I2C_M_RD;
	msg.len = length;
	msg.buf = data;
	return i2c_transfer(client->adapter,&msg, 1);
	
	/*int rc = 0;
	
	memset(data, 0, length);
	rc = i2c_master_recv(client, data, length);
	if (rc <= 0)
	{
		errlog("error!\n");
		return -EINVAL;
	} else if (rc != length)
	{
		dbg("want:%d,real:%d\n", length, rc);
	}
	return rc;*/
}

/***********************************************************************
    [function]: 
		        callback: write data by i2c interface;
    [parameters]:
			    client[in]:  struct i2c_client — represent an I2C slave device;
			    data [out]:  data buffer to write;
			    length[in]:  data length to write;
    [return]:
			    Returns negative errno, else the number of messages executed;
************************************************************************/
int zet6221_i2c_write_tsdata(struct i2c_client *client, u8 *data, u8 length)
{
	struct i2c_msg msg;
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = length;
	msg.buf = data;
	return i2c_transfer(client->adapter,&msg, 1);
	
	/*int ret = i2c_master_recv(client, data, length);
	if (ret <= 0)
	{
		errlog("error!\n");
	}
	return ret;
	*/
}

/***********************************************************************
    [function]: 
		        callback: coordinate traslating;
    [parameters]:
			    px[out]:  value of X axis;
			    py[out]:  value of Y axis;
				p [in]:   pressed of released status of fingers;
    [return]:
			    NULL;
************************************************************************/
void touch_coordinate_traslating(u32 *px, u32 *py, u8 p)
{
	int i;
	u8 pressure;

	#if ORIGIN == TOPRIGHT
	for(i=0;i<MAX_FINGER_NUMBER;i++){
		pressure = (p >> (MAX_FINGER_NUMBER-i-1)) & 0x1;
		if(pressure)
		{
			px[i] = X_MAX - px[i];
		}
	}
	#elif ORIGIN == BOTTOMRIGHT
	for(i=0;i<MAX_FINGER_NUMBER;i++){
		pressure = (p >> (MAX_FINGER_NUMBER-i-1)) & 0x1;
		if(pressure)
		{
			px[i] = X_MAX - px[i];
			py[i] = Y_MAX - py[i];
		}
	}
	#elif ORIGIN == BOTTOMLEFT
	for(i=0;i<MAX_FINGER_NUMBER;i++){
		pressure = (p >> (MAX_FINGER_NUMBER-i-1)) & 0x1;
		if(pressure)
		{
			py[i] = Y_MAX - py[i];
		}
	}
	#endif
}

/***********************************************************************
    [function]: 
		        callback: reset function;
    [parameters]:
			    void;
    [return]:
			    void;
************************************************************************/
void ctp_reset(void)
{
#if defined(TS_RST_GPIO)
	//reset mcu
   /* gpio_direction_output(TS_RST_GPIO, 1);
	msleep(1);
    gpio_direction_output(TS_RST_GPIO, 0);
	msleep(10);
	gpio_direction_output(TS_RST_GPIO, 1);
	msleep(20);*/
	wmt_rst_output(1);
	msleep(1);
	wmt_rst_output(0);
	msleep(10);
	wmt_rst_output(1);
	msleep(20);
	dbg("has done\n");
#else
	u8 ts_reset_cmd[1] = {0xb0};
	zet6221_i2c_write_tsdata(this_client, ts_reset_cmd, 1);
#endif

}

/***********************************************************************
    [function]: 
		        callback: read finger information from TP;
    [parameters]:
    			client[in]:  struct i2c_client — represent an I2C slave device;
			    x[out]:  values of X axis;
			    y[out]:  values of Y axis;
			    z[out]:  values of Z axis;
				pr[out]:  pressed of released status of fingers;
				ky[out]:  pressed of released status of keys;
    [return]:
			    Packet ID;
************************************************************************/
u8 zet6221_ts_get_xy_from_panel(struct i2c_client *client, u32 *x, u32 *y, u32 *z, u32 *pr, u32 *ky)
{
	u8  ts_data[70];
	int ret;
	int i;
	
	memset(ts_data,0,70);

	ret=zet6221_i2c_read_tsdata(client, ts_data, bufLength);
	
	*pr = ts_data[1];
	*pr = (*pr << 8) | ts_data[2];
		
	for(i=0;i<FingerNum;i++)
	{
		x[i]=(u8)((ts_data[3+4*i])>>4)*256 + (u8)ts_data[(3+4*i)+1];
		y[i]=(u8)((ts_data[3+4*i]) & 0x0f)*256 + (u8)ts_data[(3+4*i)+2];
		z[i]=(u8)((ts_data[(3+4*i)+3]) & 0x0f);
	}
		
	//if key enable
	if(KeyNum > 0)
		*ky = ts_data[3+4*FingerNum];

	return ts_data[0];
}

/***********************************************************************
    [function]: 
		        callback: get dynamic report information;
    [parameters]:
    			client[in]:  struct i2c_client — represent an I2C slave device;

    [return]:
			    1;
************************************************************************/
u8 zet6221_ts_get_report_mode(struct i2c_client *client)
{
	u8 ts_report_cmd[1] = {0xb2};
	//u8 ts_reset_cmd[1] = {0xb0};
	u8 ts_in_data[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	
	int ret;
	int i;
	int count=0;

	ret=zet6221_i2c_write_tsdata(client, ts_report_cmd, 1);

	if (ret > 0)
	{
		while(1)
		{
			msleep(1);

			//if (gpio_get_value(TS_INT_GPIO) == 0)
			if (wmt_ts_irqinval() == 0)
			{
				dbg( "int low\n");
				ret=zet6221_i2c_read_tsdata(client, ts_in_data, 17);
				
				if(ret > 0)
				{
				
					for(i=0;i<8;i++)
					{
						pc[i]=ts_in_data[i] & 0xff;
					}				
					
					if(pc[3] != 0x08)
					{
						errlog("=============== zet6221_ts_get_report_mode report error ===============\n");
						return 0;
					}

					xyExchange = (ts_in_data[16] & 0x8) >> 3;
					if(xyExchange == 1)
					{
						ResolutionY= ts_in_data[9] & 0xff;
						ResolutionY= (ResolutionY << 8)|(ts_in_data[8] & 0xff);
						ResolutionX= ts_in_data[11] & 0xff;
						ResolutionX= (ResolutionX << 8) | (ts_in_data[10] & 0xff);
					}
					else
					{
						ResolutionX = ts_in_data[9] & 0xff;
						ResolutionX = (ResolutionX << 8)|(ts_in_data[8] & 0xff);
						ResolutionY = ts_in_data[11] & 0xff;
						ResolutionY = (ResolutionY << 8) | (ts_in_data[10] & 0xff);
					}
					
					FingerNum = (ts_in_data[15] & 0x7f);
					KeyNum = (ts_in_data[15] & 0x80);

					if(KeyNum==0)
						bufLength  = 3+4*FingerNum;
					else
						bufLength  = 3+4*FingerNum+1;

					//DPRINTK( "bufLength=%d\n",bufLength);
				
					break;
				
				}else
				{
					errlog ("=============== zet6221_ts_get_report_mode read error ===============\n");
					return 0;
				}
				
			}else
			{
				//DPRINTK( "int high\n");
				if(count++ > 2000)
				{
					errlog ("=============== zet6221_ts_get_report_mode time out ===============\n");
					return 0;
				}
				
			}
		}

	}
	return 1;
}

static int zet6221_is_ts(struct i2c_client *client)
{
	/*u8 ts_in_data[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

	ctp_reset();
	if (zet6221_i2c_read_tsdata(client, ts_in_data, 17) <= 0)
	{
		return 0;
	}
	return 1;*/
	return 1;
}

/***********************************************************************
    [function]: 
		        callback: get dynamic report information with timer delay;
    [parameters]:
    			client[in]:  struct i2c_client represent an I2C slave device;

    [return]:
			    1;
************************************************************************/

u8 zet6221_ts_get_report_mode_t(struct i2c_client *client)
{
	u8 ts_report_cmd[1] = {0xb2};
	u8 ts_in_data[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	
	int ret;
	int i;
	
	ret=zet6221_i2c_write_tsdata(client, ts_report_cmd, 1);

	dbg("ret=%d,i2c_addr=0x%x\n", ret, client->addr);
	if (ret > 0)
	{
			//mdelay(10);
			//msleep(10);
			dbg("=============== zet6221_ts_get_report_mode_t ===============\n");
			ret=zet6221_i2c_read_tsdata(client, ts_in_data, 17);
			
			if(ret > 0)
			{
				
				for(i=0;i<8;i++)
				{
					pc[i]=ts_in_data[i] & 0xff;
				}
				
				if(pc[3] != 0x08)
				{
					errlog("=============== zet6221_ts_get_report_mode_t report error ===============\n");
					return 0;
				}

				xyExchange = (ts_in_data[16] & 0x8) >> 3;
				if(xyExchange == 1)
				{
					ResolutionY= ts_in_data[9] & 0xff;
					ResolutionY= (ResolutionY << 8)|(ts_in_data[8] & 0xff);
					ResolutionX= ts_in_data[11] & 0xff;
					ResolutionX= (ResolutionX << 8) | (ts_in_data[10] & 0xff);
				}
				else
				{
					ResolutionX = ts_in_data[9] & 0xff;
					ResolutionX = (ResolutionX << 8)|(ts_in_data[8] & 0xff);
					ResolutionY = ts_in_data[11] & 0xff;
					ResolutionY = (ResolutionY << 8) | (ts_in_data[10] & 0xff);
				}
					
				FingerNum = (ts_in_data[15] & 0x7f);
				KeyNum = (ts_in_data[15] & 0x80);
				inChargerMode = (ts_in_data[16] & 0x2) >> 1;

				if(KeyNum==0)
					bufLength  = 3+4*FingerNum;
				else
					bufLength  = 3+4*FingerNum+1;
				
			}else
			{
				errlog ("=============== zet6221_ts_get_report_mode_t READ ERROR ===============\n");
				return 0;
			}
							
	}else
	{
		errlog("=============== zet6221_ts_get_report_mode_t WRITE ERROR ===============\n");
		return 0;
	}
	return 1;
}

/***********************************************************************
    [function]: 
		        callback: interrupt function;
    [parameters]:
    			irq[in]:  irq value;
    			dev_id[in]: dev_id;

    [return]:
			    NULL;
************************************************************************/
static irqreturn_t zet6221_ts_interrupt(int irq, void *dev_id)
{
	struct zet6221_tsdrv *ts_drv = dev_id;
	int j = 0;
	if (wmt_is_tsint())
		{
			wmt_clr_int();
			if (wmt_is_tsirq_enable())
			{
				wmt_disable_gpirq();
				dbg("begin..\n");
				//if (!work_pending(&l_tsdata.pen_event_work))
				if (wmt_ts_irqinval() == 0)
				{
					queue_work(ts_wq, &ts_drv->work1);
				} else {
					if(KeyNum > 0)
					{
						//if (0 == ky)
						{
							for (j=0;j<4;j++)
							{
								if (l_tskey[j][1] != 0)
								{
									l_tskey[j][1] = 0;
								}
							}
							dbg("finish one key report!\n");
						} 
					}
					wmt_enable_gpirq();
				}
			}
			return IRQ_HANDLED;
		}
	
	return IRQ_NONE;

	/*//polling_time	= D_POLLING_TIME;

		if (gpio_get_value(TS_INT_GPIO) == 0)
		{
			// IRQ is triggered by FALLING code here
			struct zet6221_tsdrv *ts_drv = dev_id;
			schedule_work(&ts_drv->work1);
			//DPRINTK("TS1_INT_GPIO falling\n");
		}else
		{
			//DPRINTK("TS1_INT_GPIO raising\n");
		}

	return IRQ_HANDLED;*/
}

/***********************************************************************
    [function]: 
		        callback: touch information handler;
    [parameters]:
    			_work[in]:  struct work_struct;

    [return]:
			    NULL;
************************************************************************/
static void zet6221_ts_work(struct work_struct *_work)
{
	u32 x[MAX_FINGER_NUMBER], y[MAX_FINGER_NUMBER], z[MAX_FINGER_NUMBER], pr, ky, points;
	u32 px,py,pz;
	u8 ret;
	u8 pressure;
	int i,j;
	int tx,ty;
	int xmax,ymax;
	int realnum = 0;
	struct zet6221_tsdrv *ts =
		container_of(_work, struct zet6221_tsdrv, work1);

	struct i2c_client *tsclient1 = ts->i2c_ts;


	if (bufLength == 0)
	{
		wmt_enable_gpirq();
		return;
	}
	/*if(resetCount == 1)
	{
		resetCount = 0;
		wmt_enable_gpirq();
		return;
	}*/

	//if (gpio_get_value(TS_INT_GPIO) != 0)
	if (wmt_ts_irqinval() != 0)
	{
		/* do not read when IRQ is triggered by RASING*/
		//DPRINTK("INT HIGH\n");
		dbg("INT HIGH....\n");
		wmt_enable_gpirq();
		return;
	}
	mutex_lock(&i2c_mutex);
	ret = zet6221_ts_get_xy_from_panel(tsclient1, x, y, z, &pr, &ky);
	mutex_unlock(&i2c_mutex);

	if(ret == 0x3C)
	{

		dbg( "x1= %d, y1= %d x2= %d, y2= %d [PR] = %d [KY] = %d\n", x[0], y[0], x[1], y[1], pr, ky);
		
		points = pr;
		
		#if defined(TRANSLATE_ENABLE)
		touch_coordinate_traslating(x, y, points);
		#endif
		realnum = 0;

		for(i=0;i<FingerNum;i++){
			pressure = (points >> (MAX_FINGER_NUMBER-i-1)) & 0x1;
			dbg( "valid=%d pressure[%d]= %d x= %d y= %d\n",points , i, pressure,x[i],y[i]);

			if(pressure)
			{
				px = x[i];
				py = y[i];
				pz = z[i];

				dbg("raw%d(%d,%d)\n", i, px, py);

				//input_report_abs(ts->input, ABS_MT_TRACKING_ID, i);
	    		//input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, P_MAX);
	    		//input_report_abs(ts->input, ABS_MT_POSITION_X, x[i]);
	    		//input_report_abs(ts->input, ABS_MT_POSITION_Y, y[i]);
	    		if (wmt_ts_get_xaxis() == 0)
	    		{
	    			tx = px;
	    			ty = py;
	    			xmax = ResolutionX;
	    			ymax = ResolutionY;
	    		} else {
	    			tx = py;
	    			ty = px;
	    			xmax = ResolutionY;
	    			ymax = ResolutionX;
	    		}
	    		if (wmt_ts_get_xdir() == -1)
	    		{
	    			tx = xmax - tx;
	    		}
	    		if (wmt_ts_get_ydir() == -1)
	    		{
	    			ty = ymax - ty;
	    		}
	    		//tx = ResolutionY - py;
	    		//ty = px;
	    		dbg("rpt%d(%d,%d)\n", i, tx, ty);
	    		//add for cross finger 2013-1-10
	    	#ifdef MT_TYPE_B
				input_mt_slot(ts->input, i);
				input_mt_report_slot_state(ts->input, MT_TOOL_FINGER,true);
        #endif
        input_report_abs(ts->input, ABS_MT_TRACKING_ID, i);
        input_report_key(ts->input, BTN_TOUCH, 1);
	    	input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, pz);
          //*******************************
	    		
	    		input_report_abs(ts->input, ABS_MT_POSITION_X, tx /*px*/);
	    		input_report_abs(ts->input, ABS_MT_POSITION_Y, ty /*py*/);
	    	
	    	#ifndef MT_TYPE_B	
	    		input_mt_sync(ts->input);
	    	#endif	
	    		realnum++;
	    		if (wmt_ts_ispenup())
	    		{
	    			wmt_ts_set_penup(0);
	    		}

			}else
			{
				//input_report_abs(ts->input, ABS_MT_TRACKING_ID, i);
				//input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, 0);
				//input_mt_sync(ts->input);
				#ifdef MT_TYPE_B
				input_mt_slot(ts->input, i);
				input_mt_report_slot_state(ts->input, MT_TOOL_FINGER,false);
				input_report_abs(ts->input, ABS_MT_TRACKING_ID, -1);
        #endif		//add cross finger 2013-1-10
				dbg("p%d not pen down\n",i);				
			}
		}
		
		#ifdef MT_TYPE_B
		input_mt_report_pointer_emulation(ts->input, true);
   #endif  //add finger cross 2013-1-10
		//printk("<<<realnum %d\n", realnum);
		if (realnum != 0)
		{
			input_sync(ts->input);
			dbg("report one point group\n");
		} else if (!wmt_ts_ispenup())
		{//********here no finger press 2013-1-10
			//add 2013-1-10 cross finger issue!
			#ifdef MT_TYPE_B
				for(i=0;i<FingerNum;i++){
					input_mt_slot(ts->input, i);
					input_mt_report_slot_state(ts->input, MT_TOOL_FINGER,false);
					input_report_abs(ts->input, ABS_MT_TRACKING_ID, -1);
				}
				input_mt_report_pointer_emulation(ts->input, true);
     #else
				//input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, 0);
				//input_mt_sync(ts->input);
				input_report_abs(ts->input, ABS_MT_TRACKING_ID, -1);
				input_report_key(ts->input, BTN_TOUCH, 0);
     #endif
			//**********************************
			input_mt_sync(ts->input);
			input_sync(ts->input);
			dbg("real pen up!\n");
			wmt_ts_set_penup(1);
		}

		if(KeyNum > 0)
		{
			//for(i=0;i<MAX_KEY_NUMBER;i++)
			if (0 == ky)
			{
				for (j=0;j<4;j++)
				{
					if (l_tskey[j][1] != 0)
					{
						l_tskey[j][1] = 0;
					}
				}
				dbg("finish one key report!\n");
			} else {
				for(i=0;i<4;i++)
				{			
					pressure = ky & ( 0x01 << i );
					if (pressure)
					{
						dbg("key%d\n", i);
						if (0 == l_tskey[i][1])
						{
							l_tskey[i][1] = 1; // key down
							input_report_key(ts->input, l_tskey[i][0], 1);
							input_report_key(ts->input, l_tskey[i][0], 0);
							input_sync(ts->input);
							dbg("report key_%d\n", l_tskey[i][0]);
							break;
						}
					}

				}
			}
		}
			
		dbg("normal end...\n");
		
		msleep(1);
	}else {
		dbg("do nothing!\n");
		if(KeyNum > 0)
		{
			//if (0 == ky)
			{
				for (j=0;j<4;j++)
				{
					if (l_tskey[j][1] != 0)
					{
						l_tskey[j][1] = 0;
					}
				}
				dbg("finish one key report!\n");
			} 
		}
	}
	wmt_enable_gpirq();

}

/***********************************************************************
    [function]: 
		        callback: charger mode enable;
    [parameters]:
    			void

    [return]:
			    void
************************************************************************/
void zet6221_ts_charger_mode()
{		
	//struct zet6221_tsdrv *zet6221_ts;
	u8 ts_write_charge_cmd[1] = {0xb5}; 
	int ret=0;
	ret=zet6221_i2c_write_tsdata(this_client, ts_write_charge_cmd, 1);
}
EXPORT_SYMBOL_GPL(zet6221_ts_charger_mode);

/***********************************************************************
    [function]: 
		        callback: charger mode disable;
    [parameters]:
    			void

    [return]:
			    void
************************************************************************/
void zet6221_ts_charger_mode_disable(void)
{
	struct zet6221_tsdrv *zet6221_ts;
	u8 ts_write_cmd[1] = {0xb6}; 
	int ret=0;
	ret=zet6221_i2c_write_tsdata(this_client, ts_write_cmd, 1);
}
EXPORT_SYMBOL_GPL(zet6221_ts_charger_mode_disable);


static void ts_early_suspend(struct early_suspend *handler)
{
	//Sleep Mode
/*	u8 ts_sleep_cmd[1] = {0xb1}; 
	int ret=0;
	ret=zet6221_i2c_write_tsdata(this_client, ts_sleep_cmd, 1);
        return;
       */
	wmt_disable_gpirq();
	l_suspend = 1;
	//del_timer(&l_ts->polling_timer);

}

static void ts_late_resume(struct early_suspend *handler)
{
	resetCount = 1;
	//if (l_suspend != 0)
	{
		//wmt_disable_gpirq();
		//ctp_reset();
		//wmt_set_gpirq(IRQ_TYPE_EDGE_FALLING);
		wmt_enable_gpirq();
		l_suspend = 0;
	}
	//l_powermode = -1; //comment by rambo 2013-3-7 to prevent tp act bad sometimes
	//mod_timer(&l_ts->polling_timer,jiffies + msecs_to_jiffies(TIME_CHECK_CHARGE));

}

static int zet_ts_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int zet_ts_resume(struct platform_device *pdev)
{
	wmt_disable_gpirq();
	ctp_reset();
	wmt_set_gpirq(IRQ_TYPE_EDGE_FALLING);
	//wmt_enable_gpirq();
	return 0;
}
static int zet6221_ts_probe(struct i2c_client *client/*, const struct i2c_device_id *id*/)
{
	int result;
	struct input_dev *input_dev;
	struct zet6221_tsdrv *zet6221_ts;
	
	int count=0;

	dbg( "[TS] zet6221_ts_probe \n");
	
	zet6221_ts = kzalloc(sizeof(struct zet6221_tsdrv), GFP_KERNEL);
	l_ts = zet6221_ts;
	zet6221_ts->i2c_ts = client;
	//zet6221_ts->gpio = TS_INT_GPIO; /*s3c6410*/
	//zet6221_ts->gpio = TS1_INT_GPIO;
	
	this_client = client;
	
	i2c_set_clientdata(client, zet6221_ts);

	//client->driver = &zet6221_ts_driver;
	ts_wq = create_singlethread_workqueue("zet6221ts_wq");
	if (!ts_wq)
	{
		errlog("Failed to create workqueue!\n");
		goto err_create_wq;
	}

	INIT_WORK(&zet6221_ts->work1, zet6221_ts_work);

	input_dev = input_allocate_device();
	if (!input_dev || !zet6221_ts) {
		result = -ENOMEM;
		goto fail_alloc_mem;
	}
	
	//i2c_set_clientdata(client, zet6221_ts);

	input_dev->name = MJ5_TS_NAME;
	input_dev->phys = "zet6221_touch/input0";
	input_dev->id.bustype = BUS_HOST;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x0002;
	input_dev->id.version = 0x0100;
//bootloader
zet_download:
	if (wmt_ts_if_updatefw())
	{
		if (zet6221_load_fw())
		{
			//goto fail_load_fw;
			errlog("Can't load the firmware of zet6221!\n");
			zet6221_free_fwmem();
		} else {		
			zet6221_downloader(client);
			ctp_reset();
		}
	}
	
#if defined(TPINFO)
	udelay(100);
	
	count=0;
	do{
		
		ctp_reset();
		
		//if(zet6221_ts_get_report_mode(client)==0)    //get IC info by waiting for INT=low
		if(zet6221_ts_get_report_mode_t(client)==0)  //get IC info by delay 
		{
			ResolutionX = X_MAX;
			ResolutionY = Y_MAX;
			FingerNum = FINGER_NUMBER;
			KeyNum = KEY_NUMBER;
			if(KeyNum==0)
				bufLength  = 3+4*FingerNum;
			else
				bufLength  = 3+4*FingerNum+1;
		}else
		{
			//bootloader
			if (wmt_ts_if_updatefw())
			{
				if(zet6221_ts_version()!=0)
				{
					dbg("get report mode ok!\n");
					break;
				}
			} else {
				break;
			}
		}
		
		count++;
	}while(count<REPORT_POLLING_TIME);
	
	//bootloader
	if (wmt_ts_if_updatefw())
	{
		if(count==REPORT_POLLING_TIME)
			goto zet_download;
	}
	
#else
	ResolutionX = X_MAX;
	ResolutionY = Y_MAX;
	FingerNum = FINGER_NUMBER;
	KeyNum = KEY_NUMBER;
	if(KeyNum==0)
		bufLength  = 3+4*FingerNum;
	else
		bufLength  = 3+4*FingerNum+1;
#endif

	dbg( "ResolutionX=%d ResolutionY=%d FingerNum=%d KeyNum=%d\n",ResolutionX,ResolutionY,FingerNum,KeyNum);

	input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
    set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, ResolutionY/*ResolutionX*/, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, ResolutionX, 0, 0);

	set_bit(KEY_BACK, input_dev->keybit);
	set_bit(KEY_HOME, input_dev->keybit);
	set_bit(KEY_MENU, input_dev->keybit);
	
	//*******************************add 2013-1-10
	set_bit(ABS_MT_TRACKING_ID, input_dev->absbit);
	set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit); 
	input_set_abs_params(input_dev,ABS_MT_TRACKING_ID, 0, FingerNum, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, P_MAX, 0, 0);
	set_bit(BTN_TOUCH, input_dev->keybit);
	
	#ifdef MT_TYPE_B
	input_mt_init_slots(input_dev, FingerNum);	
#endif
	//set_bit(KEY_SEARCH, input_dev->keybit);

	//input_dev->evbit[0] = BIT(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	//input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

	result = input_register_device(input_dev);
	if (result)
		goto fail_ip_reg;

	zet6221_ts->input = input_dev;

	input_set_drvdata(zet6221_ts->input, zet6221_ts);
	mutex_init(&i2c_mutex);
	zet6221_ts->queue = create_singlethread_workqueue("ts_check_charge_queue");
	INIT_DELAYED_WORK(&zet6221_ts->work, polling_timer_func);

	//setup_timer(&zet6221_ts->polling_timer, polling_timer_func, (unsigned long)zet6221_ts);
	//mod_timer(&zet6221_ts->polling_timer,jiffies + msecs_to_jiffies(TIME_CHECK_CHARGE));
	
	
	//s3c6410
	//result = gpio_request(zet6221_ts->gpio, "GPN"); 
	wmt_set_gpirq(IRQ_TYPE_EDGE_FALLING);
	wmt_disable_gpirq();
	/*result = gpio_request(zet6221_ts->gpio, "GPN"); 
	if (result)
		goto gpio_request_fail;
	*/

	zet6221_ts->irq = wmt_get_tsirqnum();//gpio_to_irq(zet6221_ts->gpio);
	dbg( "[TS] zet6221_ts_probe.gpid_to_irq [zet6221_ts->irq=%d]\n",zet6221_ts->irq);

	result = request_irq(zet6221_ts->irq, zet6221_ts_interrupt,IRQF_SHARED /*IRQF_TRIGGER_FALLING*/, 
				ZET_TS_ID_NAME, zet6221_ts);
	if (result)
	{
		errlog("Can't alloc ts irq=%d\n", zet6221_ts->irq);
		goto request_irq_fail;
	}


	zet6221_ts->early_suspend.suspend = ts_early_suspend,
	zet6221_ts->early_suspend.resume = ts_late_resume,
	zet6221_ts->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;//,EARLY_SUSPEND_LEVEL_DISABLE_FB + 2;
	register_early_suspend(&zet6221_ts->early_suspend);
	//disable_irq(zet6221_ts->irq);
	ctp_reset();
	wmt_enable_gpirq();
	queue_delayed_work(zet6221_ts->queue, &zet6221_ts->work, msecs_to_jiffies(TIME_CHECK_CHARGE));
	//mod_timer(&zet6221_ts->polling_timer,jiffies + msecs_to_jiffies(TIME_CHECK_CHARGE));
	dbg("ok\n");
	return 0;

request_irq_fail:
	destroy_workqueue(zet6221_ts->queue);
	cancel_delayed_work_sync(&zet6221_ts->work);
	//gpio_free(zet6221_ts->gpio);
//gpio_request_fail:
	free_irq(zet6221_ts->irq, zet6221_ts);
	input_unregister_device(input_dev);
	input_dev = NULL;
fail_ip_reg:
fail_alloc_mem:
	input_free_device(input_dev);
	destroy_workqueue(ts_wq);
	cancel_work_sync(&zet6221_ts->work1);
	zet6221_free_fwmem();
err_create_wq:
	kfree(zet6221_ts);
	return result;
}

static int zet6221_ts_remove(void /*struct i2c_client *dev*/)
{
	struct zet6221_tsdrv *zet6221_ts = l_ts;//i2c_get_clientdata(dev);

	//del_timer(&zet6221_ts->polling_timer);
	unregister_early_suspend(&zet6221_ts->early_suspend);
	free_irq(zet6221_ts->irq, zet6221_ts);
	//gpio_free(zet6221_ts->gpio);
	//del_timer_sync(&zet6221_ts->polling_timer);		
	destroy_workqueue(zet6221_ts->queue);
	cancel_delayed_work_sync(&zet6221_ts->work);
	input_unregister_device(zet6221_ts->input);	
	cancel_work_sync(&zet6221_ts->work1);
	destroy_workqueue(ts_wq);
	zet6221_free_fwmem();
	kfree(zet6221_ts);

	return 0;
}
static int zet6221_ts_init(void)
{
	//u8  ts_data[70];
	//int ret;

	/*ctp_reset();
	memset(ts_data,0,70);
	ret=zet6221_i2c_read_tsdata(ts_get_i2c_client(), ts_data, 8);
	if (ret <= 0)
	{
		dbg("Can't find zet6221!\n");
		return -1;
	}
	if (!zet6221_is_ts(ts_get_i2c_client()))
	{
		dbg("isn't zet6221!\n");
		return -1;
	}*/
	if (zet6221_ts_probe(ts_get_i2c_client()))
	{
		return -1;
	}
	
	//i2c_add_driver(&zet6221_ts_driver);
	return 0;
}
//module_init(zet6221_ts_init);

static void zet6221_ts_exit(void)
{
	zet6221_ts_remove();
    //i2c_del_driver(&zet6221_ts_driver);
}
//module_exit(zet6221_ts_exit);

void zet6221_set_ts_mode(u8 mode)
{
	dbg( "[Touch Screen]ts mode = %d \n", mode);
}
//EXPORT_SYMBOL_GPL(zet6221_set_ts_mode);

struct wmtts_device zet6221_tsdev = {
		.driver_name = WMT_TS_I2C_NAME,
		.ts_id = "ZET6221",
		.init = zet6221_ts_init,
		.exit = zet6221_ts_exit,
		.suspend = zet_ts_suspend,
		.resume = zet_ts_resume,
};



MODULE_DESCRIPTION("ZET6221 I2C Touch Screen driver");
MODULE_LICENSE("GPL v2");
