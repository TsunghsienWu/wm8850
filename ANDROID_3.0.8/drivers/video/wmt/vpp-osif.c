/*++ 
 * linux/drivers/video/wmt/osif.c
 * WonderMedia video post processor (VPP) driver
 *
 * Copyright c 2012  WonderMedia  Technologies, Inc.
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

#define VPP_OSIF_C
#undef DEBUG
//#define DEBUG
//#define DEBUG_DETAIL
/*----------------------- DEPENDENCE -----------------------------------------*/
#include "vpp.h"

/*----------------------- PRIVATE MACRO --------------------------------------*/

/*----------------------- PRIVATE CONSTANTS ----------------------------------*/
/* #define LVDS_XXXX    1     *//*Example*/

/*----------------------- PRIVATE TYPE  --------------------------------------*/
/* typedef  xxxx lvds_xxx_t; *//*Example*/

/*----------EXPORTED PRIVATE VARIABLES are defined in lvds.h  -------------*/
/*----------------------- INTERNAL PRIVATE VARIABLES - -----------------------*/
/* int  lvds_xxx;        *//*Example*/
#ifdef __KERNEL__
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,32)
spinlock_t vpp_irqlock = SPIN_LOCK_UNLOCKED;
#else
static DEFINE_SPINLOCK(vpp_irqlock);
#endif
static unsigned long vpp_lock_flags;
#endif
int vpp_dvi_i2c_id;

/*--------------------- INTERNAL PRIVATE FUNCTIONS ---------------------------*/
/* void lvds_xxx(void); *//*Example*/

/*----------------------- Function Body --------------------------------------*/
#ifdef __KERNEL__
#include <asm/io.h>
#include <linux/proc_fs.h>
#else
__inline__ U32 inl(U32 offset)
{
	return REG32_VAL(offset);
}

__inline__ void outl(U32 val,U32 offset)
{
	REG32_VAL(offset) = val;
}

static __inline__ U16 inw(U32 offset)
{
	return REG16_VAL(offset);
}

static __inline__ void outw(U16 val,U32 offset)
{
	REG16_VAL(offset) = val;
}

static __inline__ U8 inb(U32 offset)
{
	return REG8_VAL(offset);
}

static __inline__ void outb(U8 val,U32 offset)
{
	REG8_VAL(offset) = val;
}
#ifndef CFG_LOADER
int get_key(void) 
{
	int key;

	extern int get_num(unsigned int min,unsigned int max,char *message,unsigned int retry);
	key = get_num(0, 256, "Input:", 5);
	DPRINT("\n");
	return key;
}

void udelay(int us)
{
	extern void vpp_post_delay(U32 tmr);

	vpp_post_delay(us);
}

void mdelay(int ms)
{
	udelay(ms*1000);
}
#endif
#endif

//Internal functions
U8 vppif_reg8_in(U32 offset)
{
	return (inb(offset));
}

U8 vppif_reg8_out(U32 offset, U8 val)
{
	outb(val, offset);
	return (val);
}

U16 vppif_reg16_in(U32 offset)
{
	return (inw(offset));
}

U16 vppif_reg16_out(U32 offset, U16 val)
{
	outw(val, offset);
	return (val);
}

U32 vppif_reg32_in(U32 offset)
{
	return (inl(offset));
}

U32 vppif_reg32_out(U32 offset, U32 val)
{
	outl(val, offset);
	return (val);
}

U32 vppif_reg32_write(U32 offset, U32 mask, U32 shift, U32 val)
{
	U32 new_val;

#ifdef VPPIF_DEBUG
	if( val > (mask >> shift) ){
		VPPIFMSG("*E* check the parameter 0x%x 0x%x 0x%x 0x%x\n",offset,mask,shift,val);
	}
#endif	

	new_val = (inl(offset) & ~(mask)) | (((val) << (shift)) & mask);
	outl(new_val, offset);
	return (new_val);
}

U32 vppif_reg32_read(U32 offset, U32 mask, U32 shift)
{
	return ((inl(offset) & mask) >> shift);
}

U32 vppif_reg32_mask(U32 offset, U32 mask, U32 shift)
{
	return (mask);
}

int vpp_request_irq(unsigned int irq_no,void *routine,unsigned int flags,char *name,void *arg)
{
#if 0	// disable irq
	return 0;
#endif

#ifdef __KERNEL__
	if ( request_irq(irq_no,routine,flags,name,arg) ) {
		DPRINT("[VPP] *E* request irq %s fail\n",name);
		return -1;
	}
#endif
	return 0;
}

void vpp_free_irq(unsigned int irq_no,void *arg)
{
#ifdef __KERNEL__
	free_irq(irq_no,arg);
#endif
}

#ifndef __KERNEL__
int wmt_getsyspara(char *varname,char *varval, int *varlen)
{
    int i = 0;
	char *p;

    p = getenv(varname);
    if (!p) {
        printf("## Warning: %s not defined\n",varname);
        return -1;
    } 
	while( p[i] != '\0' ){
		varval[i] = p[i];
		i++;
	}
	varval[i] = '\0';
	*varlen = i;
//    printf("getsyspara: %s,len %d\n", p, *varlen);
	return 0;
}
#endif

#ifndef CONFIG_VPOST
int vpp_parse_param(char *buf,unsigned int *param,int cnt,unsigned int hex_mask)
{
	char *p;
    char * endp;
    int i = 0;

	if( *buf == '\0' )
		return 0;

	for(i=0;i<cnt;i++)
		param[i] = 0;

	p = (char *)buf;
	for(i=0;i<cnt;i++){
        param[i] = simple_strtoul(p, &endp,(hex_mask & (0x1<<i))? 16:0);
        if (*endp == '\0')
            break;
        p = endp + 1;

        if (*p == '\0')
            break;
    }
	return i+1;
}
#endif

void vpp_lock(void)
{
#ifdef __KERNEL__
	spin_lock_irqsave(&vpp_irqlock, vpp_lock_flags);
#else
#endif
}

void vpp_unlock(void)
{
#ifdef __KERNEL__
	spin_unlock_irqrestore(&vpp_irqlock, vpp_lock_flags);
#else
#endif
}

#ifdef __KERNEL__
struct i2c_adapter *vpp_i2c_adapter = NULL;
struct i2c_client *vpp_i2c_client = NULL;
struct vpp_i2c_priv {
	unsigned int var;
};
static int __devinit vpp_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct vpp_i2c_priv *vpp_i2c;
	int ret=0;

	DBGMSG("\n");
	if( vpp_i2c_client == 0 )
		return -ENODEV;
	
	if (!i2c_check_functionality(vpp_i2c_client->adapter, I2C_FUNC_I2C)) {
		DBG_ERR("need I2C_FUNC_I2C\n");
		return -ENODEV;
	}

	vpp_i2c = kzalloc(sizeof(struct vpp_i2c_priv), GFP_KERNEL);
	if (vpp_i2c == NULL) {
		DBG_ERR("kzalloc fail\n");
		return -ENOMEM;
	}
	i2c_set_clientdata(i2c, vpp_i2c);
	return ret;
}

static int  vpp_i2c_remove(struct i2c_client *client)
{
	kfree(i2c_get_clientdata(client));
	return 0;
}

static const struct i2c_device_id vpp_i2c_id[] = {
	{ "vpp_i2c", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, vpp_i2c_id);

static struct i2c_driver vpp_i2c_driver = {
		.probe = vpp_i2c_probe,
		.remove = vpp_i2c_remove,
		.id_table = vpp_i2c_id,
		.driver = { .name = "vpp_i2c", },
};

static struct i2c_board_info vpp_i2c_board_info = {
	.type          = "vpp_i2c",
	.flags         = 0x00,
	.platform_data = NULL,
	.archdata      = NULL,
	.irq           = -1,
};
#endif
int vpp_i2c_xfer(struct i2c_msg *msg, unsigned int num, int bus_id)
{
	int  ret = 1;

#ifdef __KERNEL__
	int i = 0;

	if( bus_id == 1 ){
		ret	= wmt_i2c_xfer_continue_if_4(msg,num,bus_id);
		return ret;
	}

	if (num > 1) {
		for (i = 0; i < num - 1; ++i)
			msg[i].flags |= I2C_M_NOSTART;
	}
	ret = i2c_transfer(vpp_i2c_adapter, msg, num);
	if (ret <= 0) {
		printk("[%s] fail \n", __func__);
		return ret;
	}
#else
	ret = wmt_i2c_transfer(msg,num,bus_id);
#endif
	return ret;
}

int vpp_i2c_init(int i2c_id,unsigned short addr)
{
#ifdef __KERNEL__
	struct i2c_board_info *vpp_i2c_bi = &vpp_i2c_board_info;
    int ret =0;

	DBGMSG("id %d,addr 0x%x\n",i2c_id,addr);

	vpp_dvi_i2c_id = i2c_id;
	if( i2c_id & VPP_DVI_I2C_SW_BIT )
		return 0;

	i2c_id &= VPP_DVI_I2C_ID_MASK;
	vpp_i2c_adapter = i2c_get_adapter(i2c_id);
	if (vpp_i2c_adapter == NULL) {
		DBG_ERR("can not get i2c adapter, client address error\n");
		return -ENODEV;
	}

	vpp_i2c_bi->addr = addr >> 1;

	vpp_i2c_client = i2c_new_device(vpp_i2c_adapter, vpp_i2c_bi);
	if ( vpp_i2c_client == NULL) {
		DBG_ERR("allocate i2c client failed\n");
		return -ENOMEM;
	}
	i2c_put_adapter(vpp_i2c_adapter);

	ret = i2c_add_driver(&vpp_i2c_driver);
	if (ret) {
		DBG_ERR("i2c_add_driver fail\n");
	}	
	return ret;
#else
	vpp_dvi_i2c_id = i2c_id & 0xF;
#endif
}

int vpp_i2c_release(void)
{
#ifdef __KERNEL__
	if( vpp_i2c_client ){
		i2c_del_driver(&vpp_i2c_driver);
		i2c_unregister_device(vpp_i2c_client);
		vpp_i2c_remove(vpp_i2c_client);
		vpp_i2c_client = 0;
	}
#else

#endif
	return 0;
}

#ifdef __KERNEL__
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
static DECLARE_MUTEX(vpp_i2c_sem);
#else
static DEFINE_SEMAPHORE(vpp_i2c_sem);
#endif
#endif
void vpp_i2c_set_lock(int lock)
{
#ifdef __KERNEL__
	if( lock ){
		down(&vpp_i2c_sem);
	}
	else {
		up(&vpp_i2c_sem);
	}
#endif
}

int vpp_i2c_lock;
int vpp_i2c_enhanced_ddc_read(int id,unsigned int addr,unsigned int index,char *pdata,int len) 
{
#ifdef __KERNEL__
	struct i2c_msg msg[3];
#else
	struct i2c_msg_s msg[3];
#endif
	unsigned char buf[len+1];
	unsigned char buf2[2];
	int ret = 0;

	vpp_i2c_set_lock(1);

	if( vpp_i2c_lock )
		DPRINT("*W* %s\n",__FUNCTION__);

	vpp_i2c_lock = 1;

	if( id & VPP_DVI_I2C_BIT ){
		id = vpp_dvi_i2c_id;
	}
	id = id & VPP_DVI_I2C_ID_MASK;
	buf2[0] = 0x1;
	buf2[1] = 0x0;
    msg[0].addr = (0x60 >> 1);
    msg[0].flags = 0 ;
	msg[0].flags &= ~(I2C_M_RD);
	msg[0].len = 1;
    msg[0].buf = buf2;

	addr = (addr >> 1);
	memset(buf,0x55,len+1);
    buf[0] = index;
	buf[1] = 0x0;

    msg[1].addr = addr;
    msg[1].flags = 0 ;
	msg[1].flags &= ~(I2C_M_RD);
	msg[1].len = 1;
    msg[1].buf = buf;

	msg[2].addr = addr;
	msg[2].flags = 0 ;
	msg[2].flags |= (I2C_M_RD);
	msg[2].len = len;
	msg[2].buf = buf;

    ret = vpp_i2c_xfer(msg,3,id);
	memcpy(pdata,buf,len);
	vpp_i2c_lock = 0;
	vpp_i2c_set_lock(0);
	return ret;
} /* End of vpp_i2c_enhanced_ddc_read */

int vpp_i2c_read(int id,unsigned int addr,unsigned int index,char *pdata,int len)
{
	int ret = 0;

	DBG_DETAIL("(%d,0x%x,%d,%d)\n",id,addr,index,len);
	vpp_i2c_set_lock(1);
	if( vpp_i2c_lock )
		DPRINT("*W* %s\n",__FUNCTION__);

	vpp_i2c_lock = 1;

	if( id & VPP_DVI_I2C_BIT ){
		id = vpp_dvi_i2c_id;
	}
	id = id & VPP_DVI_I2C_ID_MASK;
	switch(id){
		case 0 ... 0xF:	// hw i2c
			{
#ifdef CONFIG_KERNEL
			struct i2c_msg msg[2];
#else
			struct i2c_msg_s msg[2] ;
#endif
			unsigned char buf[len+1];

			addr = (addr >> 1);	

		    buf[0] = index;
			buf[1] = 0x0;

		    msg[0].addr = addr;	// slave address
		    msg[0].flags = 0 ;
			msg[0].flags &= ~(I2C_M_RD);
			msg[0].len = 1;
		    msg[0].buf = buf;

			msg[1].addr = addr;
			msg[1].flags = I2C_M_RD;
			msg[1].len = len;
			msg[1].buf = buf;
		    ret = vpp_i2c_xfer(msg,2,id);
			memcpy(pdata,buf,len);
			}
			break;
		default:
#ifdef CONFIG_KERNEL
			ret = vo_i2c_proc((id & 0xF),(addr | BIT0),index,pdata,len);
#endif
			break;
	}
#ifdef DEBUG
{
	int i;
	
	DBGMSG("vpp_i2c_read(addr 0x%x,index 0x%x,len %d\n",addr,index,len);
	for(i=0;i<len;i+=8){
		DBGMSG("%d : 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",i,
			pdata[i],pdata[i+1],pdata[i+2],pdata[i+3],pdata[i+4],pdata[i+5],pdata[i+6],pdata[i+7]);
	}
}
#endif
	vpp_i2c_lock = 0;
	vpp_i2c_set_lock(0);
	return ret;
}

int vpp_i2c_write(int id,unsigned int addr,unsigned int index,char *pdata,int len)
{
	int ret = 0;

	DBG_DETAIL("(%d,0x%x,%d,%d)\n",id,addr,index,len);
	vpp_i2c_set_lock(1);
	if( vpp_i2c_lock )
		DPRINT("*W* %s\n",__FUNCTION__);

	vpp_i2c_lock = 1;

	if( id & VPP_DVI_I2C_BIT ){
		id = vpp_dvi_i2c_id;
	}
	id = id & VPP_DVI_I2C_ID_MASK;
	switch(id){
		case 0 ... 0xF:	// hw i2c
			{
#ifdef CONFIG_KERNEL
		    struct i2c_msg msg[1];
#else
			struct i2c_msg_s msg[1] ;
#endif
			unsigned char buf[len+1];

			addr = (addr >> 1);
		    buf[0] = index;
			memcpy(&buf[1],pdata,len);
		    msg[0].addr = addr;	// slave address
		    msg[0].flags = 0 ;
			msg[0].flags &= ~(I2C_M_RD);
		    msg[0].len = len+1;
		    msg[0].buf = buf;
		    ret = vpp_i2c_xfer(msg,1,id);
			}
			break;
		default:
#ifdef CONFIG_KERNEL
			vo_i2c_proc((id & 0xF),(addr & ~BIT0),index,pdata,len);
#endif
			break;
	}
	
#ifdef DEBUG
{
	int i;

	DBGMSG("vpp_i2c_write(addr 0x%x,index 0x%x,len %d\n",addr,index,len);	
	for(i=0;i<len;i+=8){
		DBGMSG("%d : 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",i,
			pdata[i],pdata[i+1],pdata[i+2],pdata[i+3],pdata[i+4],pdata[i+5],pdata[i+6],pdata[i+7]);
	}
}	
#endif
	vpp_i2c_lock = 0;
	vpp_i2c_set_lock(0);
	return ret;
}

void DelayMS(int ms)
{
	mdelay(ms);
}

#ifdef __KERNEL__
EXPORT_SYMBOL(vpp_i2c_write);
EXPORT_SYMBOL(vpp_i2c_read);
EXPORT_SYMBOL(vpp_i2c_enhanced_ddc_read);
EXPORT_SYMBOL(DelayMS);
#endif

