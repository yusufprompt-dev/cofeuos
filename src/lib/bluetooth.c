#include "../include/bluetooth.h"
#include "../include/string.h"

static int g_bt_enabled = 0;
static bt_bond_t g_bonds[BT_MAX_DEVICES];
static int g_bond_count = 0;

static void mem_copy(u8 *dst, const u8 *src, int len) {
    for (int i = 0; i < len; i++) dst[i] = src[i];
}

void bluetooth_init(void) {
    g_bt_enabled = 0;
    g_bond_count = 0;
}

int bluetooth_enable(void) {
    g_bt_enabled = 1;
    return 0;
}

int bluetooth_disable(void) {
    g_bt_enabled = 0;
    return 0;
}

int bluetooth_is_enabled(void) { return g_bt_enabled; }

int bluetooth_scan(bt_device_t *results, int max_results) {
    (void)results; (void)max_results;
    /* UEFI ortaminda Bluetooth donanimi mevcut degil.
       Gercek HCI tarayisi icin aygit surucusu gerekir. */
    return 0;
}

int bluetooth_pair(const u8 *addr) {
    if (!g_bt_enabled || !addr) return -1;
    if (g_bond_count >= BT_MAX_DEVICES) return -2;
    bt_bond_t *b = &g_bonds[g_bond_count];
    mem_copy(b->addr, addr, BT_ADDR_LEN);
    b->name[0] = '\0';
    b->paired = 1;
    b->connected = 0;
    g_bond_count++;
    return 0;
}

int bluetooth_connect(const u8 *addr) {
    if (!g_bt_enabled || !addr) return -1;
    for (int i = 0; i < g_bond_count; i++) {
        int match = 1;
        for (int j = 0; j < BT_ADDR_LEN; j++) { if (g_bonds[i].addr[j] != addr[j]) { match = 0; break; } }
        if (match) { g_bonds[i].connected = 1; return 0; }
    }
    return -2;
}

int bluetooth_disconnect(void) {
    for (int i = 0; i < g_bond_count; i++) g_bonds[i].connected = 0;
    return 0;
}

int bluetooth_get_bonded(bt_bond_t *list, int max_entries) {
    if (!list || max_entries <= 0) return 0;
    int c = (g_bond_count < max_entries) ? g_bond_count : max_entries;
    for (int i = 0; i < c; i++) list[i] = g_bonds[i];
    return c;
}

int bluetooth_send_data(const u8 *data, int len) {
    if (!g_bt_enabled || !data || len <= 0) return -1;
    return len;
}
