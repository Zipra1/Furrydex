#ifndef RADIO_H
#define RADIO_H

int ble_core_init(void);
int streetpass_adv_start(void);
void streetpass_adv_stop(void);

int ble_scan_start(void);

#include <zephyr/bluetooth/bluetooth.h>
#define BLE_MAX_AD_LEN 92

struct ble_scan_event
{
    bt_addr_le_t addr;
    int8_t rssi;
    uint8_t ad_len;
    uint8_t ad_data[BLE_MAX_AD_LEN];
};

extern struct k_msgq ble_scan_msgq;

#endif