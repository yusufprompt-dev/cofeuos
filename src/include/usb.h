#ifndef USB_H
#define USB_H

#include "types.h"

#define USB_MAX_DEVICES     8
#define USB_EP_MAX          4
#define USB_DESC_BUF_SIZE   256
#define USB_HID_BUF_SIZE    64

typedef struct {
    u8  port;
    u8  class;
    u8  subclass;
    u8  protocol;
    u16 vendor_id;
    u16 product_id;
    u8  ep_in;
    u8  ep_out;
    u8  max_packet;
    u8  active;
} usb_device_t;

typedef struct {
    u8  modifiers;
    u8  reserved;
    u8  keys[6];
} usb_hid_keyreport_t;

#define USB_CLASS_HID       0x03
#define USB_CLASS_MASS      0x08
#define USB_CLASS_CDC       0x02
#define USB_CLASS_HUB       0x09

void usb_init(void);
int  usb_enumerate(void);
int  usb_get_device_count(void);
int  usb_get_device_info(int index, usb_device_t *out);
int  usb_hid_read(int dev_index, usb_hid_keyreport_t *report);
int  usb_hid_poll_key(int dev_index);
int  usb_mass_read_sector(int dev_index, u32 sector, u8 *buf);
int  usb_mass_write_sector(int dev_index, u32 sector, const u8 *buf);
int  usb_is_present(void);

#endif
