#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>

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

static struct bt_le_ext_adv *streetpass_adv;

// Supposedly S=2, dunno if Zephyr can do S=8
static const struct bt_le_adv_param adv_param = {
    .options      = BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_CODED,
    .interval_min = BT_GAP_ADV_SLOW_INT_MIN,
    .interval_max = BT_GAP_ADV_SLOW_INT_MAX,
};

int streetpass_adv_start(void) {
    int err;

    err = bt_le_ext_adv_create(&adv_param, NULL, &streetpass_adv);
    if (err) {
        printk("ext_adv_create failed (err %d)\n", err);
        return err;
    }

    err = bt_le_ext_adv_set_data(streetpass_adv, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("ext_adv_set_data failed (err %d)\n", err);
        return err;
    }

    err = bt_le_ext_adv_start(streetpass_adv, BT_LE_EXT_ADV_START_DEFAULT);
    if (err) {
        printk("ext_adv_start failed (err %d)\n", err);
        return err;
    }

    printk("Streetpass advertising started (Coded PHY)\n");
    return 0;
}

void streetpass_adv_stop(void) {
    if (streetpass_adv) {
        bt_le_ext_adv_stop(streetpass_adv);
        bt_le_ext_adv_delete(streetpass_adv);
        streetpass_adv = NULL;
    }
}