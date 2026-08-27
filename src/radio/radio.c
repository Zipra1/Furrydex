#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <string.h>
#include "radio.h"

// initialization //

static void bt_ready(int err)
{
    if (err)
    {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }
    printk("Bluetooth initialized\n");
}

int ble_core_init(void)
{
    int err = bt_enable(bt_ready);
    if (err)
    {
        printk("bt_enable failed (err %d)\n", err);
    }
    return err;
}

// streetpass //

#define FURRYDEX_COMPANY_ID 0xFFFF // use real/registered ID before shipping

static uint8_t mfg_data[] = {
    FURRYDEX_COMPANY_ID & 0xFF, FURRYDEX_COMPANY_ID >> 8,
    'F', 'D',
    6, 2, 1, 4, 2, 0, 6, 9 // unique ID
};

static const struct bt_data ad[] = {
    BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg_data, sizeof(mfg_data)),
};

static struct bt_le_ext_adv *streetpass_adv;

// Supposedly S=2, dunno if Zephyr can do S=8
static const struct bt_le_adv_param adv_param = {
    .options = BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_CODED,
    .interval_min = BT_GAP_ADV_SLOW_INT_MIN,
    .interval_max = BT_GAP_ADV_SLOW_INT_MAX,
};

int streetpass_adv_start(void)
{
    int err;

    err = bt_le_ext_adv_create(&adv_param, NULL, &streetpass_adv);
    if (err)
    {
        printk("ext_adv_create failed (err %d)\n", err);
        return err;
    }

    err = bt_le_ext_adv_set_data(streetpass_adv, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err)
    {
        printk("ext_adv_set_data failed (err %d)\n", err);
        return err;
    }

    err = bt_le_ext_adv_start(streetpass_adv, BT_LE_EXT_ADV_START_DEFAULT);
    if (err)
    {
        printk("ext_adv_start failed (err %d)\n", err);
        return err;
    }

    printk("Streetpass advertising started (Coded PHY)\n");
    return 0;
}

void streetpass_adv_stop(void)
{
    if (streetpass_adv)
    {
        bt_le_ext_adv_stop(streetpass_adv);
        bt_le_ext_adv_delete(streetpass_adv);
        streetpass_adv = NULL;
    }
}

K_MSGQ_DEFINE(ble_scan_msgq, sizeof(struct ble_scan_event), 8, 1);
struct ble_scan_event ble_scan_garbage[BLE_MAX_AD_LEN];
// garbage variable is bad, is there any way to just remove a message off the top of the queue?

static void scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *buf)
{
    struct ble_scan_event ev;

    ev.addr = *info->addr;
    ev.rssi = info->rssi;
    ev.ad_len = MIN(buf->len, BLE_MAX_AD_LEN);
    memcpy(ev.ad_data, buf->data, ev.ad_len);

    if (buf->len > BLE_MAX_AD_LEN)
    {
        printk("ble: advertizement truncated (%u > %u bytes)\n", buf->len, BLE_MAX_AD_LEN);
    }
try_msgq_put:
    if (k_msgq_put(&ble_scan_msgq, &ev, K_NO_WAIT) != 0)
    {
        // printk("ble: scan msgq full, dropping top event to make room\n");
        k_msgq_get(&ble_scan_msgq, &ble_scan_garbage, K_NO_WAIT);
        goto try_msgq_put;
    }
}

static struct bt_le_scan_cb ble_scan_callbacks = {
    .recv = scan_recv,
};

int ble_scan_start(void) {
    struct bt_le_scan_param scan_param = {
        .type     = BT_LE_SCAN_TYPE_PASSIVE,
        .options  = BT_LE_SCAN_OPT_CODED | BT_LE_SCAN_OPT_NO_1M,
        .interval = BT_GAP_SCAN_FAST_INTERVAL,
        .window   = BT_GAP_SCAN_FAST_WINDOW,
    };
    bt_le_scan_cb_register(&ble_scan_callbacks);
    return bt_le_scan_start(&scan_param, NULL);
}