#include "../include/usb.h"
#include "../include/string.h"

static usb_device_t g_usb_devices[USB_MAX_DEVICES];
static int g_usb_count = 0;
static u8 g_hid_buffer[USB_HID_BUF_SIZE];

static void mem_zero(u8 *buf, int len) {
    for (int i = 0; i < len; i++) buf[i] = 0;
}

void usb_init(void) {
    mem_zero((u8*)g_usb_devices, sizeof(g_usb_devices));
    g_usb_count = 0;
}

int usb_enumerate(void) {
    /* UEFI ortaminda gercek USB enumeration UEFI USB I/O Protocols ile yapilir.
       Simdi lik gercek donanim sorgulanamaz, bos donduruyoruz. */
    g_usb_count = 0;
    return g_usb_count;
}

int usb_get_device_count(void) { return g_usb_count; }

int usb_get_device_info(int index, usb_device_t *out) {
    if (index < 0 || index >= g_usb_count || !out) return -1;
    *out = g_usb_devices[index];
    return 0;
}

int usb_hid_read(int dev_index, usb_hid_keyreport_t *report) {
    if (dev_index < 0 || dev_index >= g_usb_count || !report) return -1;
    mem_zero(g_hid_buffer, USB_HID_BUF_SIZE);
    mem_zero((u8*)report, sizeof(usb_hid_keyreport_t));
    return 0;
}

int usb_hid_poll_key(int dev_index) {
    if (dev_index < 0 || dev_index >= g_usb_count) return 0;
    return 0;
}

int usb_mass_read_sector(int dev_index, u32 sector, u8 *buf) {
    if (dev_index < 0 || dev_index >= g_usb_count || !buf) return -1;
    mem_zero(buf, 512);
    return 0;
}

int usb_mass_write_sector(int dev_index, u32 sector, const u8 *buf) {
    if (dev_index < 0 || dev_index >= g_usb_count || !buf) return -1;
    return 0;
}

int usb_is_present(void) {
    return g_usb_count > 0;
}
