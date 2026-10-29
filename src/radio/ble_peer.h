#ifndef FURRYDEX_BLE_PEER_H
#define FURRYDEX_BLE_PEER_H

#include <stdbool.h>
#include <stddef.h>
#include <zephyr/bluetooth/addr.h>

int ble_peer_init(void);
int ble_peer_listen(void);
int ble_peer_connect(const bt_addr_le_t *peer_addr);
int ble_peer_stop(void);
int ble_peer_send_line(const char *line, size_t length);
void ble_peer_get_status(bool *enabled, bool *connected);

#endif
