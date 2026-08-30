#ifndef RADIO_H
#define RADIO_H

int ble_core_init(void);
int ble_adv_start(struct bt_le_adv_param *adv_param, struct bt_le_ext_adv **ble_adv, struct bt_data *ad, size_t ad_len);
int streetpass_adv_stop(struct bt_le_ext_adv **ble_adv);

int ble_scan_start(void);

#include <zephyr/bluetooth/bluetooth.h>
#define BLE_MAX_AD_LEN 92
#define BLE_FIFO_LENGTH 8

struct ble_scan_event
{
    bt_addr_le_t addr;
    int8_t rssi;
    uint8_t ad_len;
    uint8_t ad_data[BLE_MAX_AD_LEN];
};

int ble_fifo_peek(struct ble_scan_event *peeked_item, int depth);
void ble_fifo_put(struct ble_scan_event new_item);
int ble_fifo_get(struct ble_scan_event *pulled_item);

extern struct k_msgq ble_scan_msgq;

#endif