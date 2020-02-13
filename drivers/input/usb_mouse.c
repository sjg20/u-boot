// SPDX-License-Identifier: GPL-2.0+
/*
 * USB mouse driver (parts taken from usb_kbd.c)
 *
 * (C) Copyright 2001
 * Denis Peter, MPL AG Switzerland
 *
 * Part of this source has been derived from the Linux USB
 * project.
 *
 * Copyright 2020 Google LLC
 */

#define LOG_CATEGORY UCLASS_MOUSE

#include <common.h>
#include <dm.h>
#include <log.h>
#include <malloc.h>
#include <mouse.h>
#include <usb.h>

enum {
	RPT_BUTTON,
	RPT_XREL,
	RPT_YREL,
	RPT_SCROLLY,
};

struct usb_mouse_priv {
	unsigned long	intpipe;
	int		intpktsize;
	int		intinterval;
	unsigned long	last_report;
	struct int_queue *intq;

	u32	repeat_delay;

	int xrel;
	int yrel;
	int x;
	int y;
	int buttons;
	int old_buttons;
	int yscroll;
	/*
	 * TODO(sjg@chromium.org): Use an array instead, with the
	 * DM_FLAG_ALLOC_PRIV_DMA flag
	  */
	s8		*buf;

	u8		flags;
};

/* Interrupt service routine */
static int usb_mouse_irq_worker(struct udevice *dev)
{
	struct usb_mouse_priv *priv = dev_get_priv(dev);
	s8 *buf = priv->buf;

	priv->buttons = buf[RPT_BUTTON];
	priv->xrel = buf[RPT_XREL];
	if (priv->xrel < -127)
		priv->xrel = 0;
	priv->yrel = buf[RPT_YREL];
	if (priv->yrel < -127)
		priv->yrel = 0;
	priv->yscroll = buf[RPT_SCROLLY];

	return 1;
}

/* Mouse interrupt handler */
static int usb_mouse_irq(struct usb_device *udev)
{
	struct udevice *dev = udev->dev;

	if (udev->irq_status || udev->irq_act_len != USB_MOUSE_BOOT_REPORT_SIZE) {
		log_warning("Error %lx, len %d\n", udev->irq_status,
			    udev->irq_act_len);
		return 1;
	}

	return usb_mouse_irq_worker(dev);
}

/* Interrupt polling */
static void usb_mouse_poll_for_event(struct udevice *dev)
{
	struct usb_device *udev = dev_get_parent_priv(dev);
	struct usb_mouse_priv *priv = dev_get_priv(dev);
	int ret;

	if (IS_ENABLED(CONFIG_SYS_USB_EVENT_POLL)) {
		/* Submit an interrupt transfer request */
		if (usb_int_msg(udev, priv->intpipe, priv->buf,
				priv->intpktsize, priv->intinterval, true) >= 0)
			usb_mouse_irq_worker(dev);
	} else if (IS_ENABLED(CONFIG_SYS_USB_EVENT_POLL_VIA_CONTROL_EP) ||
		   IS_ENABLED(CONFIG_SYS_USB_EVENT_POLL_VIA_INT_QUEUE)) {
		bool got_report = false;

		if (IS_ENABLED(CONFIG_SYS_USB_EVENT_POLL_VIA_CONTROL_EP)) {
			struct usb_interface *iface;

			iface = &udev->config.if_desc[0];
			ret = usb_get_report(udev, iface->desc.bInterfaceNumber,
					     1, 0, priv->buf,
					     USB_MOUSE_BOOT_REPORT_SIZE);
			printf("control ret=%d\b", ret);
		} else {
			if (poll_int_queue(udev, priv->intq)) {
				usb_mouse_irq_worker(dev);
				/* We've consumed all queued int packets, create new */
				destroy_int_queue(udev, priv->intq);
				priv->intq = create_int_queue(udev,
					priv->intpipe, 1,
					USB_MOUSE_BOOT_REPORT_SIZE, priv->buf,
					priv->intinterval);
				got_report = true;
			}
		}
		if (got_report)
			priv->last_report = get_timer(0);
	}
}

static int usb_mouse_get_event(struct udevice *dev, struct mouse_event *event)
{
	struct usb_mouse_priv *priv = dev_get_priv(dev);

	if (priv->buttons != priv->old_buttons) {
		struct mouse_button *but = &event->button;
		u8 diff;
		int i;

		event->type = MOUSE_EV_BUTTON;
		diff = priv->buttons ^ priv->old_buttons;
		log_debug("buttons=%d, old=%d, diff=%d\n", priv->buttons,
			  priv->old_buttons, diff);
		for (i = 0; i < 3; i++) {
			u8 mask = 1 << i;

			if (diff && mask) {
				but->button = i;
				but->press_state = priv->buttons & mask;
				but->clicks = 1;
				but->x = priv->x;
				but->y = priv->y;
				priv->old_buttons ^= mask;
				break;
			}
		}
		log_debug(" end: buttons=%d, old=%d, diff=%d\n", priv->buttons,
			  priv->old_buttons, diff);
	} else if (priv->xrel || priv->yrel) {
		struct mouse_motion *motion = &event->motion;

		priv->x += priv->xrel;
		priv->x = max(priv->x, 0);
		priv->x = min(priv->x, 0xffff);

		priv->y += priv->yrel;
		priv->y = max(priv->y, 0);
		priv->y = min(priv->y, 0xffff);

		event->type = MOUSE_EV_MOTION;
		motion->state = priv->buttons;
		motion->x = priv->x;
		motion->y = priv->y;
		motion->xrel = priv->xrel;
		motion->yrel = priv->yrel;
		priv->xrel = 0;
		priv->yrel = 0;
	} else {
		usb_mouse_poll_for_event(dev);
		return -EAGAIN;
	}

	return 0;
}

static int check_mouse(struct usb_device *udev, int ifnum)
{
	struct usb_endpoint_descriptor *ep;
	struct usb_interface *iface;

	if (udev->descriptor.bNumConfigurations != 1)
		return log_msg_ret("numcfg", -EINVAL);

	iface = &udev->config.if_desc[ifnum];

	if (iface->desc.bInterfaceClass != USB_CLASS_HID)
		return log_msg_ret("if class", -EINVAL);

	if (iface->desc.bInterfaceSubClass != USB_SUB_HID_BOOT)
		return log_msg_ret("if subclass", -EINVAL);

	if (iface->desc.bInterfaceProtocol != USB_PROT_HID_MOUSE)
		return log_msg_ret("if protocol", -EINVAL);

	if (iface->desc.bNumEndpoints != 1)
		return log_msg_ret("num endpoints", -EINVAL);

	ep = &iface->ep_desc[0];

	/* Check if endpoint 1 is interrupt endpoint */
	if (!(ep->bEndpointAddress & 0x80))
		return log_msg_ret("ep not irq", -EINVAL);

	if ((ep->bmAttributes & 3) != 3)
		return log_msg_ret("ep attr", -EINVAL);

	return 0;
}

/* probes the USB device dev for mouse type */
static int usb_mouse_probe(struct udevice *dev)
{
	struct usb_device *udev = dev_get_parent_priv(dev);
	struct usb_mouse_priv *priv = dev_get_priv(dev);
	struct usb_endpoint_descriptor *ep;
	struct usb_interface *iface;
	const int ifnum = 0;
	int ret;

	ret = check_mouse(udev, ifnum);
	if (ret) {
		log_warning("Mouse detect fail (err=%d)\n", ret);
		return log_msg_ret("probe", ret);
	}
	log_debug("USB mouse: found set protocol...\n");

	/* allocate input buffer aligned and sized to USB DMA alignment */
	priv->buf = memalign(USB_DMA_MINALIGN,
		roundup(USB_MOUSE_BOOT_REPORT_SIZE, USB_DMA_MINALIGN));

	/* Insert private data into USB device structure */
	udev->privptr = priv;

	/* Set IRQ handler */
	udev->irq_handle = usb_mouse_irq;

	iface = &udev->config.if_desc[ifnum];
	ep = &iface->ep_desc[0];
	priv->intpipe = usb_rcvintpipe(udev, ep->bEndpointAddress);
	priv->intpktsize = min(usb_maxpacket(udev, priv->intpipe),
			       USB_MOUSE_BOOT_REPORT_SIZE);
	priv->intinterval = ep->bInterval;
	priv->last_report = -1;

	/* We found a USB Keyboard, install it. */
	usb_set_protocol(udev, iface->desc.bInterfaceNumber, 0);

	log_debug("Found set idle...\n");
	usb_set_idle(udev, iface->desc.bInterfaceNumber, 0, 0);

	log_debug("Enable interrupt pipe...\n");
	if (IS_ENABLED(CONFIG_SYS_USB_EVENT_POLL_VIA_INT_QUEUE)) {
		priv->intq = create_int_queue(udev, priv->intpipe, 1,
					      USB_MOUSE_BOOT_REPORT_SIZE,
					      priv->buf, priv->intinterval);
		printf("priv->intq %p\n", priv->intq);
		ret = priv->intq ? 0 : -EBUSY;
	} else if (IS_ENABLED(CONFIG_SYS_USB_EVENT_POLL_VIA_CONTROL_EP)) {
		ret = usb_get_report(udev, iface->desc.bInterfaceNumber, 1, 0,
				     priv->buf, USB_MOUSE_BOOT_REPORT_SIZE);
	} else {
		ret = usb_int_msg(udev, priv->intpipe, priv->buf,
				  priv->intpktsize, priv->intinterval, false);
	}
	if (ret < 0) {
		log_warning("Failed to get mouse state from device %04x:%04x (err=%d)\n",
			    udev->descriptor.idVendor,
			    udev->descriptor.idProduct, ret);
		/* Abort, we don't want to use that non-functional keyboard */
		return ret;
	}
	log_info("USB mouse OK\n");

	/* Success */
	return 0;
}

static int usb_mouse_remove(struct udevice *dev)
{
	struct usb_device *udev = dev_get_parent_priv(dev);
	struct usb_mouse_priv *priv = dev_get_priv(dev);

	if (IS_ENABLED(CONFIG_SYS_USB_EVENT_POLL_VIA_INT_QUEUE))
		destroy_int_queue(udev, priv->intq);
	free(priv->buf);

	return 0;
}

const struct mouse_ops usb_mouse_ops = {
	.get_event	= usb_mouse_get_event,
};

static const struct udevice_id usb_mouse_ids[] = {
	{ .compatible = "usb-mouse" },
	{ }
};

U_BOOT_DRIVER(usb_mouse) = {
	.name	= "usb_mouse",
	.id	= UCLASS_MOUSE,
	.of_match = usb_mouse_ids,
	.ops	= &usb_mouse_ops,
	.probe = usb_mouse_probe,
	.remove = usb_mouse_remove,
	.priv_auto_alloc_size	= sizeof(struct usb_mouse_priv),
};

static const struct usb_device_id mouse_id_table[] = {
	{
		.match_flags = USB_DEVICE_ID_MATCH_INT_CLASS |
			USB_DEVICE_ID_MATCH_INT_SUBCLASS |
			USB_DEVICE_ID_MATCH_INT_PROTOCOL,
		.bInterfaceClass = USB_CLASS_HID,
		.bInterfaceSubClass = USB_SUB_HID_BOOT,
		.bInterfaceProtocol = USB_PROT_HID_MOUSE,
	},
	{ }		/* Terminating entry */
};

U_BOOT_USB_DEVICE(usb_mouse, mouse_id_table);
