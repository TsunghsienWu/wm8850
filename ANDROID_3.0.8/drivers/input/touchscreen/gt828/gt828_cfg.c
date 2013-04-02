#include <linux/syscalls.h>
#include <asm/unistd.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/slab.h>

#include "gt828.h"

static int parse_cfg_file(u8 *pcontext,int plen, struct gtp_data *ts)
{
    int ret,i=0;
    int val[4];
    u8 *p,*s;
    
    if(!pcontext && !ts->cfg_buf){
        dbg_err("Null pointer!\n");
        return -EFAULT;
    }
    
    p = pcontext;
    s = ts->cfg_buf;
    s[i++] = GTP_REG_CONFIG_DATA >> 8;
    s[i++] = GTP_REG_CONFIG_DATA & 0xff;
    
    while(p && *p && p <(pcontext+plen)){
        ret = sscanf(p,"%x,%x,%x,%x\n", &val[0],&val[1],&val[2],&val[3]);
        
        if(ret==4){
            s[i++] = val[0];
            s[i++] = val[1];
            s[i++] = val[2];
            s[i++] = val[3];
        }else{
            if(ret == 1){
                sscanf(p,"%x\n",&val[0]);
                s[i++] = val[0];
            }else if (ret == 2){
                sscanf(p,"%x,%x\n",&val[0],&val[1]);
                s[i++] = val[0];
                s[i++] = val[1];
            }else if(ret == 3){
                sscanf(p,"%x,%x,%x\n",&val[0],&val[1],&val[2]);
                s[i++] = val[0];
                s[i++] = val[1];
                s[i++] = val[2];
            }else{
                printk("configure file format error.\n");
                return -EINVAL;
            }
        }
        
        p = strchr(p, '\n');
        if(p) p++;
    }

    return 0;
}

static int get_cfg_size(u8* fw, int flen)
{
    int cnt=0;
    u8 *p;

    if(!fw) return 0;

    p = fw;
    while(*p){
        if(!strncmp(p++,"0x",2)) cnt++;
    }
    
    return cnt;
}

int read_cfg_file(struct gtp_data *ts)
{
	struct file *pfile = NULL;
	struct inode *inode;
	unsigned long magic;
	off_t fsize;
	loff_t pos;
	mm_segment_t fs;
    u8 *data = NULL;


	pfile = filp_open(ts->cfg_name, O_RDONLY, 0);
	if (IS_ERR(pfile)) {
		dbg_err("File open error: %s.\n", ts->cfg_name);
		return -EIO;
	}
    
	inode = pfile->f_dentry->d_inode;
	magic = inode->i_sb->s_magic;
	fsize = inode->i_size;

    data = kzalloc(fsize, GFP_KERNEL);
    if(!data){
        dbg_err("Memery request failed.\n");
        filp_close(pfile, NULL);
        return -ENOMEM;
    }
    
	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_read(pfile, data, fsize, &pos);
	filp_close(pfile, NULL);
	set_fs(fs);

    ts->cfg_len = get_cfg_size(data, fsize)+GTP_ADDR_LENGTH;
    ts->cfg_buf = kzalloc(ts->cfg_len+1, GFP_KERNEL);
    if(ts->cfg_buf) 
        parse_cfg_file(data, fsize, ts);
    
    kfree(data);
    
	return 0;
}



