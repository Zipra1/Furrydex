#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/assigned_numbers.h>
#include <zephyr/bluetooth/services/nus.h>
#include <string.h>
#include "radio.h"
#include "../lua_thread.h"

// initialization //

static bool advertising_data_has_nus(const uint8_t *ad, size_t ad_len)
{
    static const uint8_t nus_uuid[] = {BT_UUID_NUS_SRV_VAL};
    size_t offset = 0;

    while (offset < ad_len)
    {
        uint8_t field_len = ad[offset++];
        if (field_len == 0)
        {
            break;
        }
        if ((size_t)field_len > ad_len - offset)
        {
            return false;
        }

        uint8_t type = ad[offset];
        const uint8_t *field_data = &ad[offset + 1];
        size_t field_data_len = field_len - 1;

        if (type == BT_DATA_UUID128_SOME || type == BT_DATA_UUID128_ALL)
        {
            for (size_t uuid_offset = 0;
                 uuid_offset + sizeof(nus_uuid) <= field_data_len;
                 uuid_offset += sizeof(nus_uuid))
            {
                if (memcmp(&field_data[uuid_offset], nus_uuid,
                           sizeof(nus_uuid)) == 0)
                {
                    return true;
                }
            }
        }

        offset += field_len;
    }

    return false;
}

int ble_core_init(void)
{
    int ret = bt_enable(NULL);
    if (ret)
    {
        printk("bt_enable failed: %d\n", ret);
    }
    else
    {
        printk("Bluetooth initialized\n");
    }
    return ret;
}

// scanning //

static void scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *buf)
{
    bool connectable = (info->adv_props & BT_GAP_ADV_PROP_CONNECTABLE) != 0;
    if (!advertising_data_has_nus(buf->data, buf->len))
    {
        connectable = false;
    }

    struct ble_scan_event ev;

    ev.addr = *info->addr;
    ev.rssi = info->rssi;
    ev.nus_connectable = connectable;
    ev.ad_len = MIN(buf->len, BLE_MAX_AD_LEN);
    memcpy(ev.ad_data, buf->data, ev.ad_len);

    if (buf->len > BLE_MAX_AD_LEN)
    {
        printk("ble: advertizement truncated (%u > %u bytes)\n", buf->len, BLE_MAX_AD_LEN);
    }
    ble_fifo_put(ev);
}

static struct bt_le_scan_cb ble_scan_callbacks = {
    .recv = scan_recv,
};

static bool ble_scanning = false;

int ble_scan_start(void)
{
    struct bt_le_scan_param scan_param = {
        .type = BT_LE_SCAN_TYPE_PASSIVE,
        .options = BT_LE_SCAN_OPT_CODED,
        .interval = BT_GAP_SCAN_FAST_INTERVAL,
        .window = BT_GAP_SCAN_FAST_WINDOW,
    };
    bt_le_scan_cb_register(&ble_scan_callbacks);

    int ret = bt_le_scan_start(&scan_param, NULL);
    if (ret == 0)
    {
        printk("ble: Starting BLE scan\n");
        ble_scanning = true;
    }
    else
    {
        printk("ble: Failed to start BLE scan: %d\n", ret);
    }
    return ret;
}

int ble_scan_stop(void)
{
    int ret = 1;
    if (ble_scanning)
    {
        ret = bt_le_scan_stop();
        if (ret == 0)
        {
            printk("ble: Stopped BLE scan\n");
            ble_scanning = false;
        }
        else
        {
            printk("ble: Failed to stop BLE scan: %d\n", ret);
        }
    }
    return ret;
}

static int ble_fifo_in;
static int ble_fifo_out;
static int ble_fifo_count;
static struct k_spinlock ble_fifo_lock;
struct ble_scan_event ble_fifo[BLE_FIFO_LENGTH] = {};

void ble_fifo_put(struct ble_scan_event new_item)
{
    k_spinlock_key_t key = k_spin_lock(&ble_fifo_lock);
    if (ble_fifo_count == BLE_FIFO_LENGTH)
    {
        ble_fifo[ble_fifo_out] = new_item;
        ble_fifo_out = (ble_fifo_out + 1) % BLE_FIFO_LENGTH;
        ble_fifo_in = (ble_fifo_in + 1) % BLE_FIFO_LENGTH;
    }
    else
    {
        ble_fifo[ble_fifo_in] = new_item;
        ble_fifo_in = (ble_fifo_in + 1) % BLE_FIFO_LENGTH;
        ble_fifo_count++;
    }
    k_spin_unlock(&ble_fifo_lock, key);
    for (int i = 0; i < CONFIG_LUA_MAX_THREADS; i++)
    {
        if (atomic_get(&lua_slots[i].ble_fifo_depth) < BLE_FIFO_LENGTH - 1)
        {
            atomic_inc(&lua_slots[i].ble_fifo_depth);
        }
    }

    return;
}

int ble_fifo_peek(struct ble_scan_event *peeked_item, int depth)
{
    if (peeked_item == NULL || depth < 0)
    {
        return -1;
    }

    k_spinlock_key_t key = k_spin_lock(&ble_fifo_lock);
    if (depth >= ble_fifo_count)
    {
        k_spin_unlock(&ble_fifo_lock, key);
        return -1;
    }

    int index = (ble_fifo_out + depth) % BLE_FIFO_LENGTH;
    *peeked_item = ble_fifo[index];
    k_spin_unlock(&ble_fifo_lock, key);
    return 0;
}

int ble_fifo_get(struct ble_scan_event *pulled_item)
{
    if (pulled_item == NULL)
    {
        return -1;
    }

    k_spinlock_key_t key = k_spin_lock(&ble_fifo_lock);
    if (ble_fifo_count == 0)
    {
        k_spin_unlock(&ble_fifo_lock, key);
        return -1;
    }

    *pulled_item = ble_fifo[ble_fifo_out];
    ble_fifo_out = (ble_fifo_out + 1) % BLE_FIFO_LENGTH;
    ble_fifo_count--;
    k_spin_unlock(&ble_fifo_lock, key);
    return 0;
}

// advertizing //

int ble_adv_start(struct bt_le_adv_param *adv_param, struct bt_le_ext_adv **ble_adv, struct bt_data *ad, size_t ad_len)
{
    int ret;

    ret = bt_le_ext_adv_create(adv_param, NULL, ble_adv);
    if (ret)
    {
        printk("ext_adv_create failed: %d\n", ret);
        return ret;
    }

    ret = bt_le_ext_adv_set_data(*ble_adv, ad, ad_len, NULL, 0);
    if (ret)
    {
        printk("ext_adv_set_data failed: %d\n", ret);
        return ret;
    }

    ret = bt_le_ext_adv_start(*ble_adv, BT_LE_EXT_ADV_START_DEFAULT);
    if (ret)
    {
        printk("ext_adv_start failed: %d\n", ret);
        return ret;
    }

    printk("BLE advertizing started\n");
    return 0;
}

int ble_adv_stop(struct bt_le_ext_adv **ble_adv)
{
    if (ble_adv == NULL || *ble_adv == NULL)
    {
        return -1;
    }
    bt_le_ext_adv_stop(*ble_adv);
    bt_le_ext_adv_delete(*ble_adv);
    *ble_adv = NULL;
    printk("BLE advertizing stopped\n");
    return 0;
}