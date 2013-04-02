/**
 * Author: Aimar Ma <AimarMa@wondermedia.com.cn>
 *
 * Show animation during kernel boot stage
 *
 *
 **/

#include <linux/mm.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/version.h>
#include <linux/module.h>
#include <asm/io.h>
#include <linux/proc_fs.h>
#include <linux/ioport.h>
#include "animation.h"
#include "buffer.h"
#include "LzmaDec.h"
#include "../memblock.h"
#include "../hw/wmt-vpp-hw.h"
//#include "anim_data.h"


#define ANIM_STOP_PROC_FILE "kernel_animation"

#define     MAX_CLIP_COUNT          6

extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);
//extern void clear_animation_fb(void * p);
//extern void flip_animation_fb(int pingpong);
extern int vpp_calc_align(int value,int align);



#define THE_MB_USER "Boot-Animation"

#define ENV_KERNEL_ANIMATION "wmt.display.kernelanim"
#define ENV_DISPLAY_DIRECTION "wmt.org.direction"

#define DEFAULT_BUF_IMAGES  4       //default buffered image count

#undef THIS_DEBUG
//#define THIS_DEBUG

//Display Direction definition:
// 0: portrait, the back button under the screen  ( ÊúÆÁ, back ¼üÔÚÏÂ)
// 1: landscape, the back button on the right of the screnn ( ºáÆÁ, back¼üÔÚÓÒ)
// 2: portrait, the back button above the screen    ( ÊúÆÁ, back¼üÔÚÉÏ)
// 3: landscape, the back button on the left of the screen ( ºáÆÁ, back¼üÔÚ×ó)
static int s_display_direction = 1;

#ifdef THIS_DEBUG
#define LOG_DBG(fmt,args...)	printk(KERN_INFO "[Boot Animation] " fmt , ## args)
#define LOG_INFO(fmt,args...)	printk(KERN_INFO "[Boot Animation] " fmt, ## args)
#define LOG_ERROR(fmt,args...)	printk(KERN_ERR "[Boot Animation] " fmt , ## args)
#else
#define LOG_DBG(fmt,args...)
#define LOG_INFO(fmt,args...)   printk(KERN_INFO "[Boot Animation] " fmt, ## args)
#define LOG_ERROR(fmt,args...)	printk(KERN_ERR "[Boot Animation] " fmt , ## args)
#endif


// MUST match Windows PC tool. Don't change it.
struct animation_clip_header{
	int  xres;
	int  yres;
    int  linesize;
	unsigned char x_mode;
	unsigned char y_mode;
	short x_offset;
	short y_offset;
	unsigned char repeat;
	unsigned char reserved;
	int  interval;
	int  image_count;
	int  data_len;
};

// MUST match Windows PC tool. Don't change it.
struct file_header {
	int maigc;
	unsigned short version;
	unsigned char clip_count;
	unsigned char color_format;
    unsigned int  file_len;
};



struct play_context {
    struct animation_clip_header *clip;
    int xpos;           //  top postion
    int ypos;           //  left postion

    volatile int  play_thread;
    animation_buffer buf;
};


//  globe value to stop the animation loop
static volatile int g_logo_stop = 0;
static struct       animation_fb_info fb;
static int s_is_first_frame_of_clip;

static void *SzAlloc(void *p, size_t size)
{
    unsigned int addr = mb_alloc(size);
    LOG_DBG("alloc: size 0x%x, addr = 0x%x\n", size, addr);
    return (void *)mb_phys_to_virt(addr);
}

static void SzFree(void *p, void *address) {
    if (address != 0) {
        LOG_DBG("free: address = %p \n", address);
        mb_free((unsigned long)mb_virt_to_phys(address));
    }
}

static ISzAlloc g_Alloc = { SzAlloc, SzFree };

static void clear_fb_padding(int xpos, int ypos, int xres, int yres)
{
	unsigned char * dest;
	int screen_linesize = fb.width * (fb.color_fmt + 1) * 2;
	int i;

	if(ypos > 0)
	{
		if(ypos < fb.height)
			memset(fb.addr, 0,  ypos * screen_linesize);
		else
			memset(fb.addr, 0, fb.height * screen_linesize);
	}

	if(ypos + yres < fb.height)
		memset(fb.addr + (ypos + yres) * screen_linesize, 0, (fb.height - (ypos + yres)) * screen_linesize);


	if(xpos > 0)
	{
		dest = fb.addr + ypos * screen_linesize;
		if(xpos < fb.width)
			for (i = 0; i < yres; i++)
			{
				memset(dest, 0, xpos * (fb.color_fmt + 1) * 2);
				dest += screen_linesize;
			}
		else
			for (i = 0; i < yres; i++)
			{
				memset(dest, 0, screen_linesize);
				dest += screen_linesize;
			}
	}

	if(xpos + xres < fb.width)
	{
		dest = fb.addr + ypos * screen_linesize;
		dest += (xpos + xres) * (fb.color_fmt + 1) * 2;
		for (i = 0; i < yres; i++)
		{
			memset(dest, 0, (fb.width - (xpos + xres)) * (fb.color_fmt + 1) * 2);
			dest += screen_linesize;
		}
	}

}

static int show_frame(struct play_context *ctx, unsigned char *data)
{
    unsigned char * dest;
    int screen_linesize = vpp_calc_align(fb.width * (fb.color_fmt + 1) * 2, VPP_FB_WIDTH_ALIGN);
    int i = 0;
    struct animation_clip_header *clip = ctx->clip;
    if (g_logo_stop)
		return 0;

/*
	if(s_first_show_frame)
	{
		//clean framebuffer
		memset(fb.addr, 0, fb.width * fb.height * (fb.color_fmt + 1) * 2);
		s_first_show_frame = 0;
	}
*/
    dest = fb.addr;

//    printk(KERN_INFO "dest = 0x%p src = 0x%p\n", dest, data);

    if(data)
    {
        LOG_DBG("dest %p, data %p (%d,%d) (%dx%d) linesize(%d,%d)", dest, data,
                ctx->xpos, ctx->ypos, clip->xres, clip->yres, clip->linesize, linesize);

        if(s_display_direction == 1)
        {
            dest += ctx->ypos * screen_linesize;
            dest += ctx->xpos * (fb.color_fmt + 1) * 2;

            if(s_is_first_frame_of_clip)
            {
                clear_fb_padding(ctx->xpos, ctx->ypos, clip->xres, clip->yres);
                s_is_first_frame_of_clip = 0;
            }

            for (i = 0; i <  clip->yres; i++)
            {
                memcpy(dest, data, clip->xres * (fb.color_fmt + 1) * 2);
            	dest += screen_linesize;
                data += clip->linesize;
            }
        }
        else //need circumgyrate image
        {
			int xpos, ypos, xres, yres, x_mode, y_mode, x_offset, y_offset, image_linesize;

			//init
			xres = clip->xres;
			yres = clip->yres;
			x_mode = clip->x_mode;
			y_mode = clip->y_mode;
			x_offset = clip->x_offset;
			y_offset = clip->y_offset;
			image_linesize = clip->linesize;

			switch(s_display_direction)
			{
				case 0:
					xres = clip->yres;
					yres = clip->xres;

					if(clip->x_mode == 1 && clip->y_mode == 1) //in the center of screen
					{  //still in the center after image circumvolve
						x_mode = 1;
						y_mode = 1;
					}
					else if(clip->x_mode == 0 && clip->y_mode == 0) //in the top left corner
					{  //in the bottom left corner after image circumvolve
						x_mode = 0;
						y_mode = 2;
					}
					else if(clip->x_mode == 2 && clip->y_mode == 0) //in the top right corner
					{  //in the top left corner after image circumvolve
						x_mode = 0;
						y_mode = 0;
					}
					else if(clip->x_mode == 2 && clip->y_mode == 2)  //in the bottom right corner
					{  //in the top right corner after image circumvolve
						x_mode = 2;
						y_mode = 0;
					}
					else //in the bottom left corner
					{  //in the bottom right corner after image circumvolve
						x_mode = 2;
						y_mode = 2;
					}

					//ignore offset. Fix me in the future
					x_offset = 0;
					y_offset = 0;
				break;

				case 2:
					xres = clip->yres;
					yres = clip->xres;

					if(clip->x_mode == 1 && clip->y_mode == 1) //in the center of screen
					{  //still in the center after image circumvolve
						x_mode = 1;
						y_mode = 1;
					}
					else if(clip->x_mode == 0 && clip->y_mode == 0) //in the top left corner
					{  //in the top right corner after image circumvolve
						x_mode = 2;
						y_mode = 0;
					}
					else if(clip->x_mode == 2 && clip->y_mode == 0) //in the top right corner
					{  //in the bottom right corner after image circumvolve
						x_mode = 2;
						y_mode = 2;
					}
					else if(clip->x_mode == 2 && clip->y_mode == 2)  //in the bottom right corner
					{  //in the bottom left corner after image circumvolve
						x_mode = 0;
						y_mode = 2;
					}
					else //in the bottom left corner
					{  //in the top left corner after image circumvolve
						x_mode = 0;
						y_mode = 0;
					}

					//ignore offset. Fix me in the future
					x_offset = 0;
					y_offset = 0;

				break;

				case 3:
					xres = clip->xres;
					yres = clip->yres;

					if(clip->x_mode == 1 && clip->y_mode == 1) //in the center of screen
					{  //still in the center after image circumvolve
						x_mode = 1;
						y_mode = 1;
					}
					else if(clip->x_mode == 0 && clip->y_mode == 0) //in the top left corner
					{  //in the bottom right corner after image circumvolve
						x_mode = 2;
						y_mode = 2;
					}
					else if(clip->x_mode == 2 && clip->y_mode == 0) //in the top right corner
					{  //in the bottom left corner after image circumvolve
						x_mode = 0;
						y_mode = 2;
					}
					else if(clip->x_mode == 2 && clip->y_mode == 2)  //in the bottom right corner
					{  //in the top left corner after image circumvolve
						x_mode = 0;
						y_mode = 0;
					}
					else //in the bottom left corner
					{  //in the top right corner after image circumvolve
						x_mode = 2;
						y_mode = 0;
					}

					//ignore offset. Fix me in the future
					x_offset = 0;
					y_offset = 0;

				break;
			}

			xpos = x_mode * (fb.width  / 2	- xres / 2) + x_offset;
			ypos = y_mode * (fb.height / 2	- yres / 2) + y_offset;

			if(xpos < 0)
				xpos = 0;

			if(xpos > fb.width - 1)
				xpos = fb.width - 1;

			if(ypos < 0)
				ypos = 0;

			if(ypos > fb.height -1)
				ypos = fb.height -1;

			image_linesize = xres * (fb.color_fmt + 1) * 2;

			if(s_is_first_frame_of_clip)
			{
				clear_fb_padding(xpos, ypos, xres, yres);
				s_is_first_frame_of_clip = 0;
			}

			dest += ypos * screen_linesize;
			dest += xpos * (fb.color_fmt + 1) * 2;

			for (i = 0; i < yres; i++)
			{
				memcpy(dest, data, xres * (fb.color_fmt + 1) * 2);
				dest += screen_linesize;
				data += image_linesize;
			}
		}//if (s_display_direction != 1)

    }

    LOG_DBG("show_frame data %p, fb.addr %p\n", data, fb.addr);
    return 0;
}

static int decompress(struct play_context * ctx, unsigned char *src, unsigned int src_len)
{
	SRes res = 0;
	CLzmaDec state;
    size_t inPos = 0;
    unsigned char * inBuf, *tmpOutBuf = NULL;
    SizeT inProcessed;

	// 1)  read LZMA properties (5 bytes) and uncompressed size (8 bytes, little-endian) to header
	UInt64 unpackSize = 0;
	int i;

	unsigned char * header = src;
	for (i = 0; i < 8; i++)
		unpackSize += (UInt64)header[LZMA_PROPS_SIZE + i] << (i * 8);

	//  2) Allocate CLzmaDec structures (state + dictionary) using LZMA properties


	LzmaDec_Construct(&state);
    RINOK(LzmaDec_Allocate(&state, header, LZMA_PROPS_SIZE, &g_Alloc));
	if (res != SZ_OK)
		return res;

	//  3) Init LzmaDec structure before any new LZMA stream. And call LzmaDec_DecodeToBuf in loop
	LzmaDec_Init(&state);


	inBuf = header + LZMA_PROPS_SIZE + 8;

	if(s_display_direction != 1)  //need circumgyrate image
		tmpOutBuf = (unsigned char * ) g_Alloc.Alloc(&g_Alloc, ctx->buf.frame_size);


	for (;;)
	{
		unsigned int outSize;
		unsigned char * outBuf = animation_buffer_get_writable(&ctx->buf, &outSize);

		unsigned int frame_size = outSize;
		unsigned int decoded = 0;
		ELzmaFinishMode finishMode = LZMA_FINISH_ANY;
		ELzmaStatus status;
		while(1) {
            inProcessed = src_len - LZMA_PROPS_SIZE - 8 - inPos;

			if(s_display_direction == 1)
			res = LzmaDec_DecodeToBuf(&state, outBuf + frame_size - outSize, &outSize,
				inBuf + inPos, &inProcessed, finishMode, &status);
			else  //need circumgyrate image
				res = LzmaDec_DecodeToBuf(&state, tmpOutBuf + frame_size - outSize, &outSize,
					inBuf + inPos, &inProcessed, finishMode, &status);


			inPos += inProcessed;
			decoded += outSize;
            unpackSize -= outSize;
			outSize = frame_size - decoded;

            LOG_DBG("Decoded %d bytes, inPos = %d\n", decoded, inPos);

			if(res != SZ_OK)
				break;

			if (outSize == 0)
				break;
		}

		if(s_display_direction != 1) //Do circumgyrate image
		{
			struct animation_clip_header *clip = ctx->clip;
			int bytesPerPixel = (fb.color_fmt + 1) * 2;
			int srcWidth = clip->xres, srcHeight = clip->yres, srcLinesize = clip->linesize;
			int destWidth, destHeight, destLinesize;
			int j, k;

			switch(s_display_direction)
			{
				case 0:
					destWidth = srcHeight;
					destHeight = srcWidth;
					destLinesize = destWidth * bytesPerPixel;
					for(i = 0; i < destWidth; i++)
						for (j = 0; j < destHeight; j++)
							for(k = 0; k < bytesPerPixel; k++)
								*(outBuf + j * destLinesize + i * bytesPerPixel + k)
								  = *(tmpOutBuf + i * srcLinesize + (srcWidth - 1 - j) * bytesPerPixel + k);
				break;

				case 2:
					destWidth = srcHeight;
					destHeight = srcWidth;
					destLinesize = destWidth * bytesPerPixel;
					for(i = 0; i < destWidth; i++)
						for (j = 0; j < destHeight; j++)
							for(k = 0; k < bytesPerPixel; k++)
								*(outBuf + j * destLinesize + i * bytesPerPixel + k)
								  = *(tmpOutBuf + (srcHeight - 1 - i) * srcLinesize + j * bytesPerPixel + k);
				break;

				case 3:
					destWidth = srcWidth;
					destHeight = srcHeight;
					destLinesize = destWidth * bytesPerPixel;
					for(i = 0; i < destWidth; i++)
						for (j = 0; j < destHeight; j++)
							for(k = 0; k < bytesPerPixel; k++)
								*(outBuf + j * destLinesize + i * bytesPerPixel + k)
								  = *(tmpOutBuf + (srcHeight - 1 - j) * srcLinesize + (srcWidth - 1 - i) * bytesPerPixel + k);
				break;

				default:
					destWidth = srcWidth;
					destHeight = srcHeight;
					destLinesize = destWidth * bytesPerPixel;
					for(i = 0; i < destWidth; i++)
						for (j = 0; j < destHeight; j++)
							for(k = 0; k < bytesPerPixel; k++)
								*(outBuf + j * destLinesize + i * bytesPerPixel + k)
								  = *(tmpOutBuf + j * srcLinesize + i * bytesPerPixel + k);
				break;
			}
		}

		animation_buffer_write_finish(&ctx->buf, outBuf);

		if (res != SZ_OK || unpackSize == 0 || g_logo_stop)
            break;
	}


	if(tmpOutBuf)
	{
		g_Alloc.Free(&g_Alloc, tmpOutBuf);
		tmpOutBuf = NULL;
	}

    //  4) decompress finished, do clean job
    LzmaDec_Free(&state, &g_Alloc);
	return res;
}

static int animation_play(void * arg)
{
    unsigned char * data;
    static int not_first_play;
    struct play_context *ctx = (struct play_context *)arg;

    LOG_DBG( "animation_play thread start...\n");

    if(!not_first_play) {
        msleep(500);        // sleep a while to wait deocde few frames
//        clear_animation_fb(fb.addr);
        not_first_play = 1;
    }


    //  try to get a valid frame and show it
    while(!g_logo_stop) {
        data = animation_buffer_get_readable(&ctx->buf);
        if(data) {
            show_frame(ctx, data);
            animation_buffer_read_finish(&ctx->buf, data);
        }
        else {
            if( ctx->buf.eof )       //no data and reach eof
                break;
            LOG_DBG("animation_buffer_get_readable return NULL\n");
        }

        if(g_logo_stop)
            break;

        //else
        msleep(ctx->clip->interval);
    }

    LOG_DBG( "animation_play thread End\n");
    animation_buffer_stop(&ctx->buf);
    ctx->play_thread = 0;
    return 0;
}


static void decode_clip(struct animation_clip_header *clip, unsigned char * data, int index)
{
	//	start timer for animation playback
    struct play_context ctx;
    int buf_images;

    LOG_DBG("Start playing clip %d, %dx%d, linesize %d, image %d, data_len %d\n",
            index, clip->xres, clip->yres, clip->linesize, clip->image_count, clip->data_len);

    ctx.clip = clip;

    //  init the decompress buffer
	if (clip->repeat == 0) {
        buf_images = DEFAULT_BUF_IMAGES;
        if(buf_images > clip->image_count)
            buf_images = clip->image_count;
	}
	else {
        //  for the repeat clip, alloc a big memory to store all the frames
		buf_images = clip->image_count;
	}

    if( 0 != animation_buffer_init(&ctx.buf, clip->linesize * clip->yres, buf_images, &g_Alloc)){
        LOG_ERROR("Can't init animation buffer %dx%d\n", clip->linesize * clip->yres, buf_images);
        return;
    }


    ctx.xpos = clip->x_mode * (fb.width  / 2  - clip->xres / 2) + clip->x_offset;
    ctx.ypos = clip->y_mode * (fb.height / 2  - clip->yres / 2) + clip->y_offset;

    s_is_first_frame_of_clip = 1;
    kthread_run(animation_play, &ctx, "wmt-boot-play");
    ctx.play_thread = 1;

	LOG_DBG("Start Decompressing ... \n");
    decompress(&ctx, data, clip->data_len);

    if (clip->repeat) {
		while (!g_logo_stop) {
			//  Fake decompress for REPEAT mode. (Only decompress the clip once so we can save more CPU)
			unsigned int outSize;
			unsigned char * outBuf;

            outBuf = animation_buffer_get_writable(&ctx.buf, &outSize);
			animation_buffer_write_finish(&ctx.buf, outBuf);
        }
	}

    LOG_DBG("Decompress finished!\n");
    ctx.buf.eof = 1;
    //wait the play thread exit
    while(ctx.play_thread) {
        msleep(10);
    }

    LOG_DBG("Play clip %d finished\n",  index);
	animation_buffer_release(&ctx.buf, &g_Alloc);
}


static int animation_main(void * arg)
{
    unsigned char * clip;
    int i;

    struct file_header *header = (struct file_header *)arg;
    int clip_count = header->clip_count;
	struct animation_clip_header clip_headers[MAX_CLIP_COUNT];
    unsigned char  *             clip_datas[MAX_CLIP_COUNT];

    if( clip_count > MAX_CLIP_COUNT)
        clip_count  = MAX_CLIP_COUNT;
    LOG_DBG( "animation_main thread start, clip_cout = %d\n", clip_count);


	clip = (unsigned char *)(header + 1);
    for (i = 0; i< clip_count; i++){
        memcpy(&clip_headers[i], clip, sizeof(struct animation_clip_header));
        clip += sizeof(struct animation_clip_header);
        clip_datas[i] = clip;
		clip += clip_headers[i].data_len;
	}

    LOG_DBG( "Found %d clip(s)\n", clip_count);

	for (i = 0; i < clip_count; i++) {
        if (!g_logo_stop)
            decode_clip(&clip_headers[i], clip_datas[i], i);
	}

	//--> modified by howayhuo
    //g_Alloc.Free(&g_Alloc, arg);
    iounmap((void __iomem *)arg);
	 //<-- end modification
    LOG_DBG( "animation_main thread finished \n");
	return 0;
}


static int stop_proc_write( struct file   *file,
                           const char    *buffer,
                           unsigned long count,
                           void          *data )
{
    /*
    char value[20];
    int len = count;
    if( len >= sizeof(value))
        len = sizeof(value) - 1;

    if(copy_from_user(value, buffer, len))
        return -EFAULT;

    value[len] = '\0';

    LOG_DBG("procfile_write get %s\n", value);
    */

    //anything will stop the boot animation
    animation_stop();

    return count; // discard other chars
}

struct proc_dir_entry *stop_proc_file;

static struct proc_dir_entry * create_stop_proc_file(void)
{
    stop_proc_file = create_proc_entry(ANIM_STOP_PROC_FILE, 0644, NULL);

    if( stop_proc_file != NULL )
        stop_proc_file->write_proc = stop_proc_write;
    else
        LOG_ERROR("Can not create /proc/%s file", ANIM_STOP_PROC_FILE);
    return stop_proc_file;
}

static void __iomem *get_animation_data(void)
{
	unsigned char buf[80];
	int buflen = 80;
	char *varname = "wmt.kernel.animation.addr";
	int err = wmt_getsyspara(varname, buf, &buflen);
	unsigned long addr;
	void __iomem *data;

	data = NULL;
	if (!err) {
		/* addr = simple_strtoul(buf, &endp, 16); */
		if (strict_strtoul(buf, 16, &addr) < 0)
			return NULL;
		/*
		 * Someone writes the logo.data here temporarily in U-Boot,
		 * and assume the max length is 1 MB
		 * FIXME: What if n is mapped by other modules?
		 */
		data = ioremap(addr, 0x100000); /* 1MB */
		if (!data)
			printk(KERN_ERR "%s: ioremap fail at 0x%lx\n",
			       varname, addr);
		else {
			printk(KERN_INFO "%s = 0x%s, 0x%lx map to 0x%p\n",
			       varname, buf, addr, data);
		}
	} else {
		printk(KERN_INFO "No %s found\n", varname);
		data = NULL;
		/*
		 * Note: call stop_animation is safe even no start_animation
		 */
	}
	return data;
}


int animation_start(struct animation_fb_info *info)
{
    struct file_header *header;
    unsigned char * buffer;
	char buf[100] = {0};
	int varlen = 100;
	int ret;
    void __iomem * animation_data;

	if(wmt_getsyspara(ENV_KERNEL_ANIMATION,(unsigned char *)buf,&varlen) == 0)
	{
		if(!strcmp(buf, "0"))
		{
			LOG_INFO("Don't display kernel animation\n");
			return 0;
		}
	}

	animation_data = get_animation_data();//anim_data;
    if (animation_data == NULL)
        return -1;

    if( !create_stop_proc_file() )
    {
    	iounmap(animation_data);
        return -1;
    }

    header = (struct file_header *)animation_data;

    if (header->maigc != 0x12344321) {
        LOG_ERROR("It's not a valid Animation file at 0x%p, first 4 bytes: 0x%x\n", animation_data, header->maigc);
		iounmap(animation_data);
        return -1;
    }

	if( wmt_getsyspara(ENV_DISPLAY_DIRECTION,(unsigned char *)buf,&varlen) == 0 )
	{
		ret = sscanf(buf, "%d", &s_display_direction);

		if(ret == 1)
		{
			if(s_display_direction < 0 || s_display_direction > 3)
				s_display_direction = 1;
		}
	}
	//--> removed by howayhuo
	/*
    buffer = g_Alloc.Alloc(&g_Alloc, header->file_len);
    if(!buffer) {
        LOG_ERROR ("Can't alloc enough memory, length %d\n", header->file_len);
        return -1;
    }
	*/
	//<-- end removed
    memcpy(&fb, info, sizeof(fb));


	//--> modified by howayhuo
	//--original code
    //copy it to the new buffer and start the play thread
    //memcpy(buffer, header, header->file_len);
    //--new code
    buffer = (char *)animation_data;
	//<-- end add

    g_logo_stop = 0;

    LOG_DBG("Start animation_main thread ...\n");
    kthread_run(animation_main, buffer, "wmt-boot-anim");
    return 0;
}

int animation_stop(void)
{
    LOG_INFO("animation_stop\n");
    g_logo_stop = 1;

	//--> removed by howayhuo
	/*
    if( stop_proc_file ) {
        remove_proc_entry(ANIM_STOP_PROC_FILE,stop_proc_file);
        stop_proc_file = NULL;
    }
	*/
	//<-- end removed
    return 0;
}

EXPORT_SYMBOL(animation_start);
EXPORT_SYMBOL(animation_stop);


