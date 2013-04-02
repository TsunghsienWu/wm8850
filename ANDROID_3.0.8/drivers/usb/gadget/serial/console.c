/*
 * Copyright 2012 WonderMedia Technologies, Inc. All Rights Reserved. 
 *  
 * This PROPRIETARY SOFTWARE is the property of WonderMedia Technologies, Inc. 
 * and may contain trade secrets and/or other confidential information of 
 * WonderMedia Technologies, Inc. This file shall not be disclosed to any third party, 
 * in whole or in part, without prior written consent of WonderMedia. 
 *  
 * THIS PROPRIETARY SOFTWARE AND ANY RELATED DOCUMENTATION ARE PROVIDED AS IS, 
 * WITH ALL FAULTS, AND WITHOUT WARRANTY OF ANY KIND EITHER EXPRESS OR IMPLIED, 
 * AND WonderMedia TECHNOLOGIES, INC. DISCLAIMS ALL EXPRESS OR IMPLIED WARRANTIES 
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.  
 */
 
#include <linux/kernel.h>
#include <linux/utsname.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/console.h>


void gs_port_flush_char(struct gs_port	*port)
{
	unsigned long	flags;	
	spin_lock_irqsave(&port->port_lock, flags);
	if (port->port_usb)
		gs_start_tx(port);
	spin_unlock_irqrestore(&port->port_lock, flags);
}

/*NOTE: can not add any print information, or it will printk a lot of print info for ever*/
void ttyGs_console_write(struct console *co,const char *buf, unsigned count)
{
	int total = 0;
	int port_num = 0;
	struct gs_port	*port;	
	unsigned long	flags;
	int retval=0;

  if(co == NULL)  // if console unregister, then exit
  		return;

	//mutex_lock(&ports[port_num].lock);	
	port = ports[port_num].port;
	//mutex_unlock(&ports[port_num].lock);
	if (!(port->port_usb))  //if usb cable unplug, exit immediately
	     return;
	total = count;
	while(total) {
		unsigned int i;
		unsigned int lf;
		/*search for LF so we can insert CR if necessary */
		for (i = 0, lf = 0 ; i < total ; i++) {
			if (*(buf + i) == 10) {
				lf = 1;
				i++;
				break;
			}
		}
		
	   spin_lock_irqsave(&port->port_lock, flags);
	   if (i)
	   	retval = gs_buf_put(&port->port_write_buf, buf, i);

	   if (port->port_usb)  //if usb cable unplug, exit immediately
	     gs_start_tx(port);
	   else
	   	 break;
     
     //TODO:  if usb disconnect, then we should exit also.
     //there is no sense, when have no listenning on host
     	
          
     if(lf) {
     	/* append CR after LF */
     	unsigned char cr = 13;
     	retval = gs_buf_put(&port->port_write_buf, &cr, 1);
     }
     
	   spin_unlock_irqrestore(&port->port_lock, flags);
	   total -= i;
	   buf += i;
  }
//TODO: flush remaining strings
	gs_port_flush_char(port);		
	return;
}

static struct tty_driver *Gserial_console_device(struct console *co, int *index)
{
	struct tty_driver **p = (struct tty_driver **)co->data;
	if (!*p)
		return NULL;
				
	*index = co->index;
	return *p;		
}

/*
 *alloc read and write request for usb io
 *and init this request with read and write complete funciton
 */
static int gs_start_io_mine(struct gs_port *port)
{
	struct list_head	*head = &port->read_pool;
	struct usb_ep		*ep = port->port_usb->out;
	int			status;
	unsigned		started;

	/* Allocate RX and TX I/O buffers.  We can't easily do this much
	 * earlier (with GFP_KERNEL) because the requests are coupled to
	 * endpoints, as are the packet sizes we'll be using.  Different
	 * configurations may use different endpoints with a given port;
	 * and high speed vs full speed changes packet sizes too.
	 */
	status = gs_alloc_requests(ep, head, gs_read_complete,
		&port->read_allocated);
	if (status)
		return status;

	status = gs_alloc_requests(port->port_usb->in, &port->write_pool,
			gs_write_complete, &port->write_allocated);
	if (status) {
		gs_free_requests(ep, head, &port->read_allocated);
		return status;
	}

}
/*
 *alloc circular buffer for write
 *
*/
static int Gserial_console_setup(struct console *co, char *options)
{
	int status;
	int port_num = 0;
	struct gs_port	*port;	
	mutex_lock(&ports[port_num].lock);	
	port = ports[port_num].port;
	mutex_unlock(&ports[port_num].lock);
	pr_devel("enter:%s\n",__func__);
	spin_lock_irq(&port->port_lock);
	/* allocate circular buffer on first open */
	if (port->port_write_buf.buf_buf == NULL) {

		spin_unlock_irq(&port->port_lock);
		status = gs_buf_alloc(&port->port_write_buf, WRITE_BUF_SIZE);
		spin_lock_irq(&port->port_lock);

		if (status) {
			pr_debug("Gserial_console_setup: gs_port%d no buffer\n",port->port_num);
			//port->openclose = false;
			goto exit_unlock_port;
		}
	}
	
	/* if connected, start the I/O stream */
	if (port->port_usb) {
		struct gserial	*gser = port->port_usb;
		pr_debug("gs_open: start gserial%d\n", port->port_num);
		gs_start_io_mine(port);

		if (gser->connect)
			gser->connect(gser);
	}
	status = 0;
  pr_devel("exit Gserial_console_setup\n");
exit_unlock_port:
	spin_unlock_irq(&port->port_lock);
	return status;	

}
/*
 * acturally it do nothing, just for debug, display more info
 *
 */
int Gserial_early_setup(void)
{
#if 1	
	  struct console *pcon;
		//printk("enter:%s\n",__func__);
		for_each_console(pcon){
			pr_devel("console name:%s,index:%d,flag:0x%x\n",
			pcon->name,pcon->index,pcon->flags);
		}
#endif
   return 0x0;		
}
struct console Gserial_console = {
	.name	= "ttyGS",   //this name must match the arguments of "console=" in bootargs
	.write	= ttyGs_console_write,
	.device = Gserial_console_device,
	.early_setup = Gserial_early_setup,
	.setup = Gserial_console_setup,
	/*
	 *CON_PRINTBUFFER: will print buffered messages
	 *CON_CONSDEV: keep the preferred console at the head of console drivers list.
	 *CON_ENABLED: if you enable console prepared before register this console, then you should set this flag.
   *	then you maybe not implement setup member funcitons
	*/
	.flags	= CON_PRINTBUFFER,
	.index	= -1,
	.data = &gs_tty_driver,
};


/*
 *
 * NOTE: the following function can not called in interruption context.
 *
 */
void Gserial_console_init(void)
{
	register_console(&Gserial_console);
}

void Gserial_console_exit(void)
{
	unregister_console(&Gserial_console);
}

static void Gserial_console_control(struct work_struct *work)
{
	int port_num = 0;
	struct gs_port	*port;	
	mutex_lock(&ports[port_num].lock);	
	port = ports[port_num].port;
	mutex_unlock(&ports[port_num].lock);
	if (port->port_usb) {  //usb cable plugin
	  Gserial_console_init();
	  port->misc_flag |= CONSOLE_REGISTERED; //set console register flag	  
	} else {               //usb cable unplugin
		Gserial_console_exit();
		port->misc_flag &= ~CONSOLE_REGISTERED;//unset console register flag
	}
}