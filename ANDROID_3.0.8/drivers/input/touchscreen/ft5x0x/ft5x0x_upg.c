#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/mount.h>
#include <linux/earlysuspend.h>
#include <linux/syscalls.h>
#include <asm/unistd.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/slab.h>
#include "ft5x0x.h"

typedef enum
{
    ERR_OK,
    ERR_MODE,
    ERR_READID,
    ERR_ERASE,
    ERR_STATUS,
    ERR_ECC,
    ERR_DL_ERASE_FAIL,
    ERR_DL_PROGRAM_FAIL,
    ERR_DL_VERIFY_FAIL,
    ERR_FMID
}E_UPGRADE_ERR_TYPE;

#define FT5X_CTPM_ID_L   0X79
#define FT5X_CTPM_ID_H   0X03

#define FT56_CTPM_ID_L   0X79
#define FT56_CTPM_ID_H   0X06

#define FTS_PACKET_LENGTH  128

extern struct ft5x0x_data *pContext;
extern int ft5x0x_i2c_rxdata(char *rxdata, int length);
extern int ft5x0x_i2c_txdata(char *txdata, int length);

static int ft5x0x_write_reg(u8 addr, u8 para)
{
    u8 buf[2];
    int ret = -1;

    buf[0] = addr;
    buf[1] = para;
    ret = ft5x0x_i2c_txdata(buf, 2);
    if (ret <= 0) {
        printk("write reg failed! %x ret: %d", buf[0], ret);
        return -1;
    }
    
    return 0;
}

static int ft5x0x_read_reg(u8 addr, u8 *pdata)
{
    int ret;
    u8 buf[2];
    struct i2c_msg msgs[2];

    //
    buf[0] = addr;    //register address
    
    msgs[0].addr = pContext->addr;
    msgs[0].flags = 0;
    msgs[0].len = 1;
    msgs[0].buf = buf;
    
    msgs[1].addr = pContext->addr;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = 1;
    msgs[1].buf = pdata;

    ret = wmt_i2c_xfer_continue_if_4(msgs, 2, FT5X0X_I2C_BUS);
    if (ret <= 0)
        printk("msg %s i2c read error: %d\n", __func__, ret);

    return ret;
  
}


/*
[function]: 
    send a command to ctpm.
[parameters]:
    btcmd[in]        :command code;
    btPara1[in]    :parameter 1;    
    btPara2[in]    :parameter 2;    
    btPara3[in]    :parameter 3;    
    num[in]        :the valid input parameter numbers, if only command code needed and no parameters followed,then the num is 1;    
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/
static u8 cmd_write(u8 *cmd,u8 num)
{
    return ft5x0x_i2c_txdata(cmd, num);
}

/*
[function]: 
    write data to ctpm , the destination address is 0.
[parameters]:
    pbt_buf[in]    :point to data buffer;
    bt_len[in]        :the data numbers;    
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/
static u8 byte_write(u8* pbt_buf, int dw_len)
{
    
    return ft5x0x_i2c_txdata( pbt_buf, dw_len);
}

/*
[function]: 
    read out data from ctpm,the destination address is 0.
[parameters]:
    pbt_buf[out]    :point to data buffer;
    bt_len[in]        :the data numbers;    
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/
static u8 byte_read(u8* pbt_buf, u8 bt_len)
{
    int ret;
	struct i2c_msg msg[1];
	
	msg[0].addr = pContext->addr;
	msg[0].flags = I2C_M_RD;
	msg[0].len = bt_len;
	msg[0].buf = pbt_buf;

	ret = wmt_i2c_xfer_continue_if_4(msg, 1, FT5X0X_I2C_BUS);
	if (ret <= 0)
		printk("msg i2c read error: %d\n", ret);
	
	return ret;
}


/*
[function]: 
    burn the FW to ctpm.
[parameters]:(ref. SPEC)
    pbt_buf[in]    :point to Head+FW ;
    dw_lenth[in]:the length of the FW + 6(the Head length);    
    bt_ecc[in]    :the ECC of the FW
[return]:
    ERR_OK        :no error;
    ERR_MODE    :fail to switch to UPDATE mode;
    ERR_READID    :read id fail;
    ERR_ERASE    :erase chip fail;
    ERR_STATUS    :status error;
    ERR_ECC        :ecc error.
*/
static E_UPGRADE_ERR_TYPE ft5x0x_fw_upgrade(struct ft5x0x_data *ft5x0x, u8* pbt_buf, int dw_lenth)
{
    int i = 0,j = 0,i_ret;
    int packet_number;
    int temp,lenght;
    u8 packet_buf[FTS_PACKET_LENGTH + 6];
    u8 auc_i2c_write_buf[10];
    u8 reg_val[2] = {0};
    u8 ctpm_id[2] = {0};
    u8 cmd[4];
    u8 bt_ecc;

    /*********Step 1:Reset  CTPM *****/
    /*write 0xaa to register 0xfc*/
    ft5x0x_write_reg(0xfc,0xaa);
    msleep(50);
    /*write 0x55 to register 0xfc*/
    ft5x0x_write_reg(0xfc,0x55);
    printk("[FTS] Step 1: Reset CTPM.\n");
    msleep(30);   

    /*********Step 2:Enter upgrade mode *****/
    auc_i2c_write_buf[0] = 0x55;
    auc_i2c_write_buf[1] = 0xaa;
    do{
        i ++;
        i_ret = byte_write(auc_i2c_write_buf, 2);
        mdelay(5);
    }while(i_ret <= 0 && i < 5 );
    msleep(20);
    
    /*********Step 3:check READ-ID**********/        
    if(ft5x0x->id == FT5606){
        ctpm_id[0] = FT56_CTPM_ID_L;
        ctpm_id[1] = FT56_CTPM_ID_H;
    }else{
        ctpm_id[0] = FT5X_CTPM_ID_L;
        ctpm_id[1] = FT5X_CTPM_ID_H;
    }

    cmd[0] = 0x90;
    cmd[1] = 0x00;
    cmd[2] = 0x00;
    cmd[3] = 0x00;
    cmd_write(cmd,4);
    byte_read(reg_val,2);
    if (reg_val[0] == ctpm_id[0] && reg_val[1] == ctpm_id[1]){
        printk("[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
    }else{
        printk("[FTS] ID_ERROR: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
        return ERR_READID;
    }

    cmd[0] = 0xcd;
    cmd_write(cmd,1);
    byte_read(reg_val,1);
    printk("[FTS] bootloader version = 0x%x\n", reg_val[0]);

    /******Step 4:erase app and panel paramenter area *********/
    cmd[0] = 0x61;
    cmd_write(cmd,1); //erase app area
    msleep(1500); 
    cmd[0] = 0x63;
    cmd_write(cmd,1); //erase panel parameter area
    msleep(100);
    printk("[FTS] Step 4: erase. \n");

    /*********Step 5:write firmware(FW) to ctpm flash*********/
    bt_ecc = 0;
    printk("[FTS] Step 5: start upgrade. \n");
    dw_lenth = dw_lenth - 8;
    packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
    packet_buf[0] = 0xbf;
    packet_buf[1] = 0x00;
    for (j=0;j<packet_number;j++){
        temp = j * FTS_PACKET_LENGTH;
        packet_buf[2] = (u8)(temp>>8);
        packet_buf[3] = (u8)temp;
        lenght = FTS_PACKET_LENGTH;
        packet_buf[4] = (u8)(lenght>>8);
        packet_buf[5] = (u8)lenght;

        for (i=0;i<FTS_PACKET_LENGTH;i++){
            packet_buf[6+i] = pbt_buf[j*FTS_PACKET_LENGTH + i]; 
            bt_ecc ^= packet_buf[6+i];
        }
        
        byte_write(&packet_buf[0],FTS_PACKET_LENGTH + 6);
        mdelay(FTS_PACKET_LENGTH/6 + 1);
        if ((j * FTS_PACKET_LENGTH % 1024) == 0){
              printk("[FTS] upgrade the 0x%x th byte.\n", ((unsigned int)j) * FTS_PACKET_LENGTH);
        }
    }

    if ((dw_lenth) % FTS_PACKET_LENGTH > 0){
        temp = packet_number * FTS_PACKET_LENGTH;
        packet_buf[2] = (u8)(temp>>8);
        packet_buf[3] = (u8)temp;

        temp = (dw_lenth) % FTS_PACKET_LENGTH;
        packet_buf[4] = (u8)(temp>>8);
        packet_buf[5] = (u8)temp;

        for (i=0;i<temp;i++){
            packet_buf[6+i] = pbt_buf[ packet_number*FTS_PACKET_LENGTH + i]; 
            bt_ecc ^= packet_buf[6+i];
        }

        byte_write(&packet_buf[0],temp+6);    
        mdelay(20);
    }

    //send the last six byte
    for (i = 0; i<6; i++){
        temp = 0x6ffa + i;
        packet_buf[2] = (u8)(temp>>8);
        packet_buf[3] = (u8)temp;
        temp =1;
        packet_buf[4] = (u8)(temp>>8);
        packet_buf[5] = (u8)temp;
        packet_buf[6] = pbt_buf[ dw_lenth + i]; 
        bt_ecc ^= packet_buf[6];

        byte_write(&packet_buf[0],7);  
        mdelay(20);
    }

    /*********Step 6: read out checksum********************/
    /*send the opration head*/
    cmd[0] = 0xcc;
    cmd_write(cmd,1);
    byte_read(reg_val,1);
    printk("[FTS] Step 6:read ECC 0x%x, firmware ECC 0x%x. \n", reg_val[0], bt_ecc);
    if(reg_val[0] != bt_ecc){
        return ERR_ECC;
    }

    /*********Step 7: reset the new FW***********************/
    cmd[0] = 0x07;
    cmd_write(cmd,1);

    msleep(300);  //make sure CTP startup normally
    
    return ERR_OK;
}

int ft5x0x_auto_clb(void)
{
    u8 uc_temp;
    u8 i ;

    printk("[FTS] start auto CLB.\n");
    msleep(200);
    ft5x0x_write_reg(0, 0x40);  
    msleep(100);   //make sure already enter factory mode
    ft5x0x_write_reg(2, 0x4);  //write command to start calibration
    msleep(300);
    for(i=0;i<100;i++){
        ft5x0x_read_reg(0,&uc_temp);
        if ( ((uc_temp&0x70)>>4) == 0x0){ //return to normal mode, calibration finish
            break;
        }
        msleep(200);
        printk("[FTS] waiting calibration %d\n",i);
    }
    printk("[FTS] calibration OK.\n");
    
    msleep(300);
    ft5x0x_write_reg(0, 0x40);  //goto factory mode
    msleep(100);   //make sure already enter factory mode
    ft5x0x_write_reg(2, 0x5);  //store CLB result
    msleep(300);
    ft5x0x_write_reg(0, 0x0); //return to normal mode 
    msleep(300);
    printk("[FTS] store CLB result OK.\n");
    return 0;
}

static int ft5x0x_get_bin_ver(const u8 *fw, int fw_szie)
{
    if (fw_szie > 2){
        return fw[fw_szie - 2];
    }else{
        return 0xff; //default value
    }
    return 0xff;
}

int ft5x0x_read_fw_ver(void)
{
    u8 ver=0;
    int ret=0;
    
    ret = ft5x0x_read_reg(FT5X0X_REG_FIRMID, &ver);
    if(ret > 0) 
        return ver;

    return ret;
}


static int ft5x0x_get_fw_szie(const char *fw_name)
{
	struct file *pfile = NULL;
	struct inode *inode;
	unsigned long magic;
	off_t fsize = 0;

    if(fw_name == NULL){
        dbg_err("Firmware name error.\n");
        return -EFAULT;
    }

	if (NULL == pfile)
		pfile = filp_open(fw_name, O_RDONLY, 0);

	if (IS_ERR(pfile)) {
		dbg_err("File open error: %s.\n", fw_name);
		return -EIO;
	}

	inode = pfile->f_dentry->d_inode;
	magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	filp_close(pfile, NULL);
	return fsize;
}

static int ft5x0x_read_fw(const char *fw_name, u8 *buf)
{
	struct file *pfile = NULL;
	struct inode *inode;
	unsigned long magic;
	off_t fsize;
	loff_t pos;
	mm_segment_t fs;

	if(fw_name == NULL){
        dbg_err("Firmware name error.\n");
        return -EFAULT;
    }

    if (NULL == pfile)
		pfile = filp_open(fw_name, O_RDONLY, 0);
	if (IS_ERR(pfile)) {
		dbg_err("File open error: %s.\n", fw_name);
		return -EIO;
	}

	inode = pfile->f_dentry->d_inode;
	magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_read(pfile, buf, fsize, &pos);
	filp_close(pfile, NULL);
	set_fs(fs);

	return 0;
}

#define FW_SUFFFIX ".bin"
#define SD_UPG_BIN_PATH "/sdcard/_wmt_ft5x0x_fw_app.bin"
#define FS_UPG_BIN_PATH "/lib/firmware/"

int ft5x0x_upg_fw_bin(struct ft5x0x_data *ft5x0x, int check_ver)
{
	int i_ret = 0;
	int fwsize = 0;
    int hw_fw_ver;
    int bin_fw_ver;
    int do_upg;
    u8 *pbt_buf = NULL;
    u8 fw_path[128] = {0};

    if(ft5x0x->upg) 
        sprintf(fw_path,"%s%s%s", FS_UPG_BIN_PATH, ft5x0x->fw_name,FW_SUFFFIX);//get fw binary file from filesystem
    else 
        strcpy(fw_path,SD_UPG_BIN_PATH); //get fw binary file from SD card
    
    fwsize = ft5x0x_get_fw_szie(fw_path); 
	if (fwsize <= 0) {
		dbg_err("Get firmware size failed\n");
		return -EIO;
	}

	if (fwsize < 8 || fwsize > 32 * 1024) {
		dbg_err("FW length error\n");
		return -EIO;
	}

	pbt_buf = kmalloc(fwsize + 1, GFP_KERNEL);
	if (ft5x0x_read_fw(fw_path, pbt_buf)) {
		dbg_err("Request_firmware failed\n");
		i_ret = -EIO;
        goto exit;
	}
    
    hw_fw_ver =ft5x0x_read_fw_ver();
    if(hw_fw_ver <= 0){
        dbg_err("Read firmware version failed\n");
        i_ret = hw_fw_ver;
        goto exit;
    }
    
    bin_fw_ver = ft5x0x_get_bin_ver(pbt_buf, fwsize);
    printk("[FTS] hardware fw ver 0x%0x, binary ver 0x%0x\n",hw_fw_ver, bin_fw_ver);

    if(check_ver){
        if(hw_fw_ver == 0xa6 || hw_fw_ver < bin_fw_ver)
            do_upg = 1;
        else
            do_upg = 0;
    }else{
        do_upg = 1;
    }
    
    if(do_upg){
	    if ((pbt_buf[fwsize - 8] ^ pbt_buf[fwsize - 6]) == 0xFF && 
            (pbt_buf[fwsize - 7] ^ pbt_buf[fwsize - 5]) == 0xFF &&
            (pbt_buf[fwsize - 3] ^ pbt_buf[fwsize - 4]) == 0xFF) {
		    i_ret = ft5x0x_fw_upgrade(ft5x0x, pbt_buf, fwsize);
		    if (i_ret)
			    dbg_err("Upgrade failed, i_ret=%d\n",i_ret);
		    else {
                hw_fw_ver = ft5x0x_read_fw_ver();
                printk("[FTS] upgrade to new version 0x%x\n", hw_fw_ver);
		    }
	    } else {
		    dbg_err("FW format error\n");
	    }
    }

    ft5x0x_auto_clb();/*start auto CLB*/
    
exit:    
    kfree(pbt_buf);
	return i_ret;
}


