#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include "types.h"

#define BT_MAX_DEVICES       16
#define BT_NAME_MAX_LEN      32
#define BT_ADDR_LEN          6

typedef struct {
    u8  addr[BT_ADDR_LEN];
    char name[BT_NAME_MAX_LEN];
    s8  rssi;
    u8  type;
    u8  active;
} bt_device_t;

typedef struct {
    u8  addr[BT_ADDR_LEN];
    char name[BT_NAME_MAX_LEN];
    u8  paired;
    u8  connected;
} bt_bond_t;

#define BT_TYPE_CLASSIC  0
#define BT_TYPE_BLE      1

void bluetooth_init(void);
int  bluetooth_enable(void);
int  bluetooth_disable(void);
int  bluetooth_is_enabled(void);
int  bluetooth_scan(bt_device_t *results, int max_results);
int  bluetooth_pair(const u8 *addr);
int  bluetooth_connect(const u8 *addr);
int  bluetooth_disconnect(void);
int  bluetooth_get_bonded(bt_bond_t *list, int max_entries);
int  bluetooth_send_data(const u8 *data, int len);

#endif
