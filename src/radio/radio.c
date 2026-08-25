#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>

// initialization //

static void bt_ready(int err) {
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }
    printk("Bluetooth initialized\n");
}

int ble_core_init(void) {
    int err = bt_enable(bt_ready);
    if (err) {
        printk("bt_enable failed (err %d)\n", err);
    }
    return err;
}


// streetpass //

#define FURRYDEX_COMPANY_ID 0xFFFF   // use real/registered ID before shipping

static uint8_t mfg_data[] = {
    FURRYDEX_COMPANY_ID & 0xFF, FURRYDEX_COMPANY_ID >> 8,
    'F', 'D',
    6,2,1,4,2,0,6,9 // unique ID
};

static const struct bt_data ad[] = {
    BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg_data, sizeof(mfg_data)),
};

int streetpass_adv_start(void) {
    return bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), NULL, 0);
}

void streetpass_adv_stop(void) {
    bt_le_adv_stop();
}