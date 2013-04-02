/*
 * Rawbulk Gadget Function Driver from VIA Telecom
 *
 * Copyright (C) 2011 VIA Telecom, Inc.
 * Author: Karfield Chen (kfchen@via-telecom.com)
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* #define DEBUG */
/* #define VERBOSE_DEBUG */

#define DRIVER_AUTHOR   "Karfield Chen <kfchen@via-telecom.com>"
#define DRIVER_DESC     "Rawbulk Gadget - transport data from CP to Gadget"
#define DRIVER_VERSION  "1.0.1"
#define DRIVER_NAME     "usb_rawbulk"

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/err.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>

#include <linux/usb/composite.h>
#include <linux/usb/rawbulk.h>

#define function_to_rawbulk(f) container_of(f, struct rawbulk_function, function)

static void simple_setup_complete(struct usb_ep *ep,
                struct usb_request *req) {
    ;//DO NOTHING
}

unsigned int dump_mask = 0;
static int rawbulk_function_setup(struct usb_function *f, const struct
        usb_ctrlrequest *ctrl) {
    int rc;
    struct rawbulk_function *fn = function_to_rawbulk(f);
    struct usb_composite_dev *cdev = f->config->cdev;
    struct usb_request *req = cdev->req;

    if (rawbulk_transfer_state(fn->transfer_id) < 0)
        return -EOPNOTSUPP;

    if (dump_mask & (1 << RAWBULK_TID_MODEM))
        printk(KERN_DEBUG "DUMP ctrl: bRequestType = %02X, bRequest = %02X, " \
            "wValue = %04X, wIndex = %04X, wLength = %d\n",
            ctrl->bRequestType, ctrl->bRequest, ctrl->wValue, ctrl->wIndex,
            ctrl->wLength);

    if (ctrl->bRequestType == 0xc0 && ctrl->bRequest == 0x02) {
        /* Query CD109 Status */
        if (rawbulk_check_enable(fn))
            goto forwarding;
        *(unsigned char *)req->buf = 0x02; /* Test!!! */
        req->length = 1;
        req->complete = simple_setup_complete;
        req->zero = 0;
        return usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
    } else if (ctrl->bRequestType == 0x40 && ctrl->bRequest == 0x01) {
        /* Set/Clear DTR */
        if (rawbulk_check_enable(fn))
            goto forwarding;
        req->zero = 0;
        return usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
    } else if (ctrl->bRequestType == 0xc0 && ctrl->bRequest == 0x04) {
        /* Request CBP Product Name */
        req->length = sprintf(req->buf, "CBP_KUNLUN");
        req->zero = 0;
        return usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
    }

    return -EOPNOTSUPP;
forwarding:
    rc = rawbulk_forward_ctrlrequest(ctrl);
    if (rc < 0) {
        printk(KERN_DEBUG "rawbulk forwarding failed %d\n", rc);
        return rc;
    }
    return 256 + 999;//USB_GADGET_DELAYED_STATUS;
}

static void rawbulk_auto_reconnect(int transfer_id) {
    int rc;
    struct rawbulk_function *fn = rawbulk_lookup_function(transfer_id);
    if (!fn || fn->autoreconn == 0)
        return;

    if (rawbulk_check_enable(fn) && fn->activated) {
        printk(KERN_DEBUG "start %s again automatically.\n", fn->longname);
        rc = rawbulk_start_transactions(transfer_id, fn->nups,
                fn->ndowns, fn->upsz, fn->downsz, fn->splitsz, fn->pushable);
        if (rc < 0) {
            rawbulk_disable_function(fn);
            rawbulk_tty_start_io(fn);
        }
    }
}

static struct usb_ep *match_ep(struct usb_configuration *c,
        struct rawbulk_function *fn, int in) {
    struct usb_gadget *gadget = c->cdev->gadget;
    struct usb_ep *ep;
    struct usb_endpoint_descriptor *desc =
        (struct usb_endpoint_descriptor *)(in? fn->fs_descs[BULKIN_DESC]:
                fn->fs_descs[BULKOUT_DESC]);
    return usb_ep_autoconfig(gadget, desc);
}

static int rawbulk_function_bind(struct usb_configuration *c, struct
        usb_function *f) {
    int rc, ifnum;
    struct rawbulk_function *fn = function_to_rawbulk(f);
    struct usb_gadget *gadget = c->cdev->gadget;
    struct usb_ep *ep_out, *ep_in;

    rc = usb_interface_id(c, f);
    if (rc < 0)
        return rc;
    ifnum = rc;

    fn->interface.bInterfaceNumber = cpu_to_le16(ifnum);



    usb_ep_autoconfig_reset(c->cdev->gadget); //added by griffen


    if (!(ep_out = match_ep(c, fn, 0))) {
        printk(KERN_DEBUG "unfortunately, we could not find endpoint(OUT) for %s",
                fn->longname);
        return -ENOMEM;
    }

    if (!(ep_in = match_ep(c, fn, 1))) {
        printk(KERN_DEBUG "unfortunately, we could not find endpoint(IN) for %s",
                fn->longname);
        return -ENOMEM;
    }

    ep_out->driver_data = fn;
    ep_in->driver_data = fn;
    fn->bulk_out = ep_out;
    fn->bulk_in = ep_in;

	f->descriptors = usb_copy_descriptors(fn->fs_descs);
	if (unlikely(!f->descriptors))
		return -ENOMEM;

    if (gadget_is_dualspeed(gadget)) {
        fn->hs_bulkin_endpoint.bEndpointAddress =
            fn->fs_bulkin_endpoint.bEndpointAddress;
        fn->hs_bulkout_endpoint.bEndpointAddress =
            fn->fs_bulkout_endpoint.bEndpointAddress;
		f->hs_descriptors = usb_copy_descriptors(fn->hs_descs);
		if (unlikely(!f->hs_descriptors)) {
			usb_free_descriptors(f->descriptors);
			return -ENOMEM;
		}
    }

    fn->cdev = c->cdev;
    fn->activated = 0;

    /*
    rc = rawbulk_register_tty(fn);
    if (rc < 0)
        printk(KERN_ERR "failed to register tty for %s\n", fn->longname);
        */

    return rawbulk_bind_function(fn->transfer_id, f, ep_out, ep_in,
            rawbulk_auto_reconnect);
}

static void rawbulk_function_unbind(struct usb_configuration *c, struct
        usb_function *f) {
    struct rawbulk_function *fn = function_to_rawbulk(f);

    printk(KERN_DEBUG "%s - unbind %s.\n", __func__, fn->longname);
    //rawbulk_unregister_tty(fn);
    rawbulk_unbind_function(fn->transfer_id);
}

static void do_activate(struct work_struct *data) {
    struct rawbulk_function *fn = container_of(data, struct rawbulk_function,
            activator);
    int rc;
    printk(KERN_DEBUG "%s rawbulk %s channel (enabled? %s).\n",
            fn->activated? "activte": "detach", fn->shortname,
            rawbulk_check_enable(fn)? "yes": "no");
    if (fn->activated) { /* enumerated */
        /* enabled endpoints */
        rc = usb_ep_enable(fn->bulk_out, ep_choose(fn->cdev->gadget,
                    &fn->hs_bulkout_endpoint, &fn->fs_bulkout_endpoint));
        if (rc < 0) {
            printk(KERN_ERR "failed to enable rawbulk %s %d\n",
                    fn->bulk_out->name, rc);
            return;
        }
        rc = usb_ep_enable(fn->bulk_in, ep_choose(fn->cdev->gadget,
                    &fn->hs_bulkin_endpoint, &fn->fs_bulkin_endpoint));
        if (rc < 0) {
            printk(KERN_ERR "failed to enable rawbulk %s %d\n",
                    fn->bulk_in->name, rc);
            usb_ep_disable(fn->bulk_out);
            return;
        }

        /* start rawbulk if enabled */
        if (rawbulk_check_enable(fn)) {
            wake_lock(&fn->keep_awake);
            rc = rawbulk_start_transactions(fn->transfer_id, fn->nups,
                    fn->ndowns, fn->upsz, fn->downsz, fn->splitsz, fn->pushable);
            if (rc < 0)
                rawbulk_disable_function(fn);
        }

        /* start tty io */
        rc = rawbulk_tty_alloc_request(fn);
        if (rc < 0)
            return;
        if (!rawbulk_check_enable(fn))
            rawbulk_tty_start_io(fn);
    } else { /* disconnect */
        if (rawbulk_check_enable(fn)) {
            if (fn->transfer_id == RAWBULK_TID_MODEM) {
                /* this in interrupt, but DTR need be set firstly then clear it
                 * */
                //control_dtr(0);
            }
            rawbulk_stop_transactions(fn->transfer_id);
            /* keep the enable state, so we can enable again in next time */
            //set_enable_state(fn, 0);
            wake_unlock(&fn->keep_awake);
        } else
            rawbulk_tty_stop_io(fn);

        rawbulk_tty_free_request(fn);

        usb_ep_disable(fn->bulk_out);
        usb_ep_disable(fn->bulk_in);

        fn->bulk_out->driver_data = NULL;
        fn->bulk_in->driver_data = NULL;
    }
}

static int rawbulk_function_setalt(struct usb_function *f, unsigned intf,
        unsigned alt) {
    struct rawbulk_function *fn = function_to_rawbulk(f);
    fn->activated = 1;
    schedule_work(&fn->activator);
    return 0;
}

static void rawbulk_function_disable(struct usb_function *f) {
    struct rawbulk_function *fn = function_to_rawbulk(f);
    fn->activated = 0;
    schedule_work(&fn->activator);
}

static int rawbulk_bind_config(struct usb_configuration *c, int transfer_id) {
    int rc;
    struct rawbulk_function *fn = rawbulk_lookup_function(transfer_id);

    printk(KERN_DEBUG "add %s to config.\n", fn->longname);

    if (!fn)
        return -ENOMEM;

    if (fn->string_defs[0].id == 0) {
        rc = usb_string_id(c->cdev);
        if (rc < 0)
            return rc;
        fn->string_defs[0].id = rc;
        fn->interface.iInterface = rc;
    }

    fn->function.name = fn->longname;
    fn->function.setup = rawbulk_function_setup;
    fn->function.bind = rawbulk_function_bind;
    fn->function.unbind = rawbulk_function_unbind;
    fn->function.set_alt = rawbulk_function_setalt;
    fn->function.disable = rawbulk_function_disable;

    INIT_WORK(&fn->activator, do_activate);

    rc = usb_add_function(c, &fn->function);
    if (rc < 0)
        printk(KERN_DEBUG "%s - failed to config %d.\n", __func__, rc);

    return rc;
}
