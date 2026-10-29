#include <errno.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/services/nus.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <bluetooth/gatt_dm.h>
#include <bluetooth/services/nus_client.h>

#include "../console/console_router.h"
#include "ble_peer.h"
#include "radio.h"

LOG_MODULE_REGISTER(ble_peer, LOG_LEVEL_INF);

#define BLE_PEER_LINE_MAX (LUA_SERIAL_MSG_SIZE - 1)
#define BLE_PEER_TX_QUEUE_SIZE 8
#define BLE_PEER_TX_RETRY_COUNT 20

enum ble_peer_role
{
    BLE_PEER_ROLE_STOPPED,
    BLE_PEER_ROLE_LISTENING,
    BLE_PEER_ROLE_CONNECTING,
};

static atomic_t peer_role = ATOMIC_INIT(BLE_PEER_ROLE_STOPPED);
static atomic_t peer_initialized;
static atomic_t advertising;
static atomic_t client_ready;
static atomic_t server_tx_ready;
static bt_addr_le_t peer_target_address;
static bool peer_target_valid;
static struct bt_conn *peer_connection;
static struct k_mutex connection_lock;
K_MUTEX_DEFINE(peer_mode_lock);
static struct bt_nus_client nus_client;
static struct k_sem client_write_complete;
static atomic_t client_write_error;

static char received_line[LUA_SERIAL_MSG_SIZE];
static size_t received_length;
static bool discard_received_line;
static struct k_spinlock received_line_lock;

K_MSGQ_DEFINE(peer_tx_queue, LUA_SERIAL_MSG_SIZE, BLE_PEER_TX_QUEUE_SIZE, 4);
K_THREAD_STACK_DEFINE(peer_tx_stack, 2048);
static struct k_thread peer_tx_thread_data;
static void peer_restart_work_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(peer_restart_work, peer_restart_work_handler);
static int nus_client_discovery_start(struct bt_conn *conn);

static void mtu_exchange_complete(struct bt_conn *conn, uint8_t err,
                                  struct bt_gatt_exchange_params *params)
{
    ARG_UNUSED(conn);
    ARG_UNUSED(params);
    if (err != 0)
    {
        LOG_WRN("BLE ATT MTU exchange failed: %u", err);
    }
    if (atomic_get(&peer_role) == BLE_PEER_ROLE_CONNECTING)
    {
        int ret = nus_client_discovery_start(conn);
        if (ret != 0)
        {
            LOG_ERR("Could not start NUS service discovery: %d", ret);
            bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        }
    }
}

static struct bt_gatt_exchange_params mtu_exchange_params = {
    .func = mtu_exchange_complete,
};

static const struct bt_data peer_advertising_data[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_SRV_VAL),
};

static const struct bt_data peer_scan_response_data[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void peer_received_bytes(const uint8_t *data, size_t length)
{
    if (atomic_get(&peer_role) == BLE_PEER_ROLE_STOPPED)
    {
        return;
    }

    k_spinlock_key_t key = k_spin_lock(&received_line_lock);
    bool line_overflowed = false;
    bool queue_failed = false;

    for (size_t i = 0; i < length; i++)
    {
        uint8_t c = data[i];

        if (c == '\r' || c == '\n')
        {
            if (!discard_received_line && received_length > 0)
            {
                received_line[received_length] = '\0';
                int ret = console_router_send_lua_line(received_line);
                if (ret != 0)
                {
                    queue_failed = true;
                }
            }
            received_length = 0;
            discard_received_line = false;
            continue;
        }

        if (discard_received_line)
        {
            continue;
        }

        if (c == '\0')
        {
            received_length = 0;
            discard_received_line = true;
            line_overflowed = true;
            continue;
        }

        if (received_length >= BLE_PEER_LINE_MAX)
        {
            received_length = 0;
            discard_received_line = true;
            line_overflowed = true;
            continue;
        }

        received_line[received_length++] = (char)c;
    }

    k_spin_unlock(&received_line_lock, key);
    if (line_overflowed)
    {
        LOG_WRN("Discarding invalid or overlong BLE input line");
    }
    if (queue_failed)
    {
        LOG_WRN("Could not queue BLE input for Lua");
    }
}

static void nus_server_received(struct bt_conn *conn, const void *data,
                                uint16_t length, void *context)
{
    ARG_UNUSED(conn);
    ARG_UNUSED(context);
    peer_received_bytes(data, length);
}

static void nus_server_notifications_changed(bool enabled, void *context)
{
    ARG_UNUSED(context);
    atomic_set(&server_tx_ready, enabled);
}

static struct bt_nus_cb nus_server_callbacks = {
    .received = nus_server_received,
    .notif_enabled = nus_server_notifications_changed,
};

static uint8_t nus_client_received(struct bt_nus_client *nus,
                                  const uint8_t *data, uint16_t length)
{
    ARG_UNUSED(nus);
    peer_received_bytes(data, length);
    return BT_GATT_ITER_CONTINUE;
}

static void nus_client_sent(struct bt_nus_client *nus, uint8_t err,
                            const uint8_t *data, uint16_t length)
{
    ARG_UNUSED(nus);
    ARG_UNUSED(data);
    ARG_UNUSED(length);
    atomic_set(&client_write_error, err);
    k_sem_give(&client_write_complete);
}

static const struct bt_nus_client_init_param nus_client_init = {
    .cb = {
        .received = nus_client_received,
        .sent = nus_client_sent,
    },
};

static void discovery_complete(struct bt_gatt_dm *dm, void *context);
static void discovery_service_not_found(struct bt_conn *conn, void *context);
static void discovery_error(struct bt_conn *conn, int err, void *context);

static const struct bt_gatt_dm_cb discovery_callbacks = {
    .completed = discovery_complete,
    .service_not_found = discovery_service_not_found,
    .error_found = discovery_error,
};

static int nus_client_discovery_start(struct bt_conn *conn)
{
    return bt_gatt_dm_start(conn, BT_UUID_NUS_SERVICE,
                            &discovery_callbacks, &nus_client);
}

static void discovery_complete(struct bt_gatt_dm *dm, void *context)
{
    ARG_UNUSED(context);

    struct bt_conn *conn = bt_gatt_dm_conn_get(dm);
    int ret = bt_nus_handles_assign(dm, &nus_client);
    if (ret == 0)
    {
        ret = bt_nus_subscribe_receive(&nus_client);
    }
    bt_gatt_dm_data_release(dm);

    if (ret != 0)
    {
        LOG_ERR("Could not configure BLE peer NUS client: %d", ret);
        if (conn != NULL)
        {
            bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        }
        return;
    }

    atomic_set(&client_ready, 1);
    LOG_INF("BLE peer NUS client ready");
}

static void discovery_service_not_found(struct bt_conn *conn, void *context)
{
    ARG_UNUSED(context);
    LOG_ERR("Connected BLE peer does not provide Nordic UART Service");
    bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}

static void discovery_error(struct bt_conn *conn, int err, void *context)
{
    ARG_UNUSED(context);
    LOG_ERR("BLE peer service discovery failed: %d", err);
    bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}

static int peer_connection_start_locked(void)
{
    if (!peer_target_valid)
    {
        return -EINVAL;
    }

    struct bt_conn *conn = NULL;
    int ret = bt_conn_le_create(&peer_target_address,
                                BT_CONN_LE_CREATE_CONN,
                                BT_LE_CONN_PARAM_DEFAULT,
                                &conn);
    if (ret != 0)
    {
        return ret;
    }

    k_mutex_lock(&connection_lock, K_FOREVER);
    if (peer_connection == NULL)
    {
        peer_connection = conn;
        conn = NULL;
    }
    k_mutex_unlock(&connection_lock);
    if (conn != NULL)
    {
        bt_conn_unref(conn);
    }
    return 0;
}

static void peer_connected(struct bt_conn *conn, uint8_t err)
{
    if (err != 0)
    {
        LOG_ERR("BLE peer connection failed: %u", err);
        struct bt_conn *released = NULL;
        k_mutex_lock(&connection_lock, K_FOREVER);
        if (peer_connection == conn)
        {
            released = peer_connection;
            peer_connection = NULL;
        }
        k_mutex_unlock(&connection_lock);
        if (released != NULL)
        {
            bt_conn_unref(released);
        }
        if (atomic_get(&peer_role) != BLE_PEER_ROLE_STOPPED)
        {
            k_work_reschedule(&peer_restart_work, K_MSEC(500));
        }
        return;
    }

    if (atomic_get(&peer_role) == BLE_PEER_ROLE_STOPPED)
    {
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        return;
    }

    atomic_set(&client_ready, 0);
    atomic_set(&server_tx_ready, 0);
    k_mutex_lock(&connection_lock, K_FOREVER);
    if (peer_connection == NULL)
    {
        peer_connection = bt_conn_ref(conn);
    }
    k_mutex_unlock(&connection_lock);
    atomic_set(&advertising, 0);

    if (atomic_get(&peer_role) == BLE_PEER_ROLE_CONNECTING)
    {
        int mtu_ret = bt_gatt_exchange_mtu(conn, &mtu_exchange_params);
        if (mtu_ret == 0)
        {
            return;
        }
        if (mtu_ret != -EALREADY)
        {
            LOG_WRN("Could not request BLE ATT MTU exchange: %d", mtu_ret);
        }
        int ret = nus_client_discovery_start(conn);
        if (ret != 0)
        {
            LOG_ERR("Could not start NUS service discovery: %d", ret);
            bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        }
    }
    else
    {
        LOG_INF("BLE NUS peer connected");
    }
}

static void peer_disconnected(struct bt_conn *conn, uint8_t reason)
{
    ARG_UNUSED(reason);

    struct bt_conn *released = NULL;
    k_mutex_lock(&connection_lock, K_FOREVER);
    if (peer_connection == conn)
    {
        released = peer_connection;
        peer_connection = NULL;
    }
    k_mutex_unlock(&connection_lock);

    k_spinlock_key_t key = k_spin_lock(&received_line_lock);
    received_length = 0;
    discard_received_line = false;
    k_spin_unlock(&received_line_lock, key);

    atomic_set(&server_tx_ready, 0);
    if (nus_client.conn == conn)
    {
        nus_client.conn = NULL;
        atomic_set(&client_ready, 0);
    }

    if (released == NULL)
    {
        return;
    }
    bt_conn_unref(released);
    k_msgq_purge(&peer_tx_queue);
    LOG_INF("BLE NUS peer disconnected");

    if (atomic_get(&peer_role) == BLE_PEER_ROLE_LISTENING)
    {
        k_work_reschedule(&peer_restart_work, K_MSEC(500));
    }
    else if (atomic_get(&peer_role) == BLE_PEER_ROLE_CONNECTING)
    {
        k_work_reschedule(&peer_restart_work, K_MSEC(500));
    }
}

static void peer_restart_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    k_mutex_lock(&peer_mode_lock, K_FOREVER);
    enum ble_peer_role role = atomic_get(&peer_role);
    if (role == BLE_PEER_ROLE_STOPPED)
    {
        k_mutex_unlock(&peer_mode_lock);
        return;
    }

    k_mutex_lock(&connection_lock, K_FOREVER);
    bool connected = peer_connection != NULL;
    k_mutex_unlock(&connection_lock);
    if (connected)
    {
        k_mutex_unlock(&peer_mode_lock);
        return;
    }

    int ret;
    if (role == BLE_PEER_ROLE_LISTENING)
    {
        ret = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1,
                              peer_advertising_data,
                              ARRAY_SIZE(peer_advertising_data),
                              peer_scan_response_data,
                              ARRAY_SIZE(peer_scan_response_data));
        if (ret == 0 || ret == -EALREADY)
        {
            atomic_set(&advertising, 1);
            k_mutex_unlock(&peer_mode_lock);
            return;
        }
        LOG_WRN("Could not start BLE peer advertising: %d", ret);
    }
    else
    {
        ret = peer_connection_start_locked();
        if (ret == 0)
        {
            k_mutex_unlock(&peer_mode_lock);
            return;
        }
        LOG_WRN("Could not connect to selected BLE peer: %d", ret);
    }

    k_mutex_unlock(&peer_mode_lock);
    k_work_reschedule(&peer_restart_work, K_SECONDS(1));
}

static struct bt_conn_cb peer_connection_callbacks = {
    .connected = peer_connected,
    .disconnected = peer_disconnected,
};

static void peer_tx_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    char message[LUA_SERIAL_MSG_SIZE];
    while (true)
    {
        if (k_msgq_get(&peer_tx_queue, message, K_FOREVER) != 0)
        {
            continue;
        }

        k_mutex_lock(&connection_lock, K_FOREVER);
        struct bt_conn *conn = peer_connection != NULL
                                   ? bt_conn_ref(peer_connection)
                                   : NULL;
        k_mutex_unlock(&connection_lock);

        if (conn == NULL)
        {
            LOG_WRN("Dropping Lua BLE output: no peer is connected");
            continue;
        }

        size_t length = strlen(message);
        uint16_t mtu = bt_gatt_get_mtu(conn);
        size_t max_chunk = mtu > 3 ? mtu - 3 : 20;
        size_t offset = 0;
        int ret = 0;

        while (offset < length)
        {
            if (atomic_get(&peer_role) == BLE_PEER_ROLE_STOPPED)
            {
                ret = -ECANCELED;
                break;
            }
            size_t chunk = MIN(length - offset, max_chunk);
            if (atomic_get(&peer_role) == BLE_PEER_ROLE_CONNECTING)
            {
                if (!atomic_get(&client_ready))
                {
                    ret = -ENOTCONN;
                    break;
                }

                k_sem_reset(&client_write_complete);
                ret = bt_nus_client_send(&nus_client,
                                         (const uint8_t *)&message[offset],
                                         chunk);
                if (ret == 0)
                {
                    ret = k_sem_take(&client_write_complete, K_FOREVER);
                    if (ret == 0)
                    {
                        ret = atomic_get(&client_write_error) == 0 ? 0 : -EIO;
                    }
                }
            }
            else
            {
                ret = -ENOMEM;
                for (int retry = 0; retry < BLE_PEER_TX_RETRY_COUNT; retry++)
                {
                    ret = bt_nus_send(conn, &message[offset], chunk);
                    if (ret != -ENOMEM && ret != -EAGAIN)
                    {
                        break;
                    }
                    k_msleep(10);
                }
            }

            if (ret != 0)
            {
                break;
            }
            offset += chunk;
        }

        if (ret != 0)
        {
            LOG_WRN("Could not send Lua data over BLE NUS: %d", ret);
        }
        bt_conn_unref(conn);
    }
}

int ble_peer_init(void)
{
    k_mutex_init(&connection_lock);
    k_sem_init(&client_write_complete, 0, 1);
    atomic_set(&client_ready, 0);
    atomic_set(&server_tx_ready, 0);

    int ret = bt_nus_cb_register(&nus_server_callbacks, NULL);
    if (ret != 0)
    {
        return ret;
    }

    ret = bt_nus_client_init(&nus_client, &nus_client_init);
    if (ret != 0)
    {
        return ret;
    }

    bt_conn_cb_register(&peer_connection_callbacks);
    k_thread_create(&peer_tx_thread_data, peer_tx_stack,
                    K_THREAD_STACK_SIZEOF(peer_tx_stack),
                    peer_tx_thread, NULL, NULL, NULL,
                    K_PRIO_PREEMPT(10), 0, K_NO_WAIT);
    atomic_set(&peer_initialized, 1);
    LOG_INF("BLE peer NUS initialized");
    return 0;
}

int ble_peer_listen(void)
{
    k_mutex_lock(&peer_mode_lock, K_FOREVER);
    if (!atomic_get(&peer_initialized))
    {
        k_mutex_unlock(&peer_mode_lock);
        return -EAGAIN;
    }
    k_mutex_lock(&connection_lock, K_FOREVER);
    bool connection_exists = peer_connection != NULL;
    k_mutex_unlock(&connection_lock);
    if (connection_exists)
    {
        k_mutex_unlock(&peer_mode_lock);
        return -EBUSY;
    }
    if (!atomic_cas(&peer_role, BLE_PEER_ROLE_STOPPED, BLE_PEER_ROLE_LISTENING))
    {
        k_mutex_unlock(&peer_mode_lock);
        return -EALREADY;
    }

    int ret = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1,
                              peer_advertising_data,
                              ARRAY_SIZE(peer_advertising_data),
                              peer_scan_response_data,
                              ARRAY_SIZE(peer_scan_response_data));
    if (ret == 0)
    {
        atomic_set(&advertising, 1);
    }
    else
    {
        atomic_set(&peer_role, BLE_PEER_ROLE_STOPPED);
    }
    k_mutex_unlock(&peer_mode_lock);
    return ret;
}

int ble_peer_connect(const bt_addr_le_t *peer_addr)
{
    if (peer_addr == NULL ||
        (peer_addr->type != BT_ADDR_LE_PUBLIC &&
         peer_addr->type != BT_ADDR_LE_RANDOM &&
         peer_addr->type != BT_ADDR_LE_PUBLIC_ID &&
         peer_addr->type != BT_ADDR_LE_RANDOM_ID))
    {
        return -EINVAL;
    }

    k_mutex_lock(&peer_mode_lock, K_FOREVER);
    if (!atomic_get(&peer_initialized))
    {
        k_mutex_unlock(&peer_mode_lock);
        return -EAGAIN;
    }
    k_mutex_lock(&connection_lock, K_FOREVER);
    bool connection_exists = peer_connection != NULL;
    k_mutex_unlock(&connection_lock);
    if (connection_exists)
    {
        k_mutex_unlock(&peer_mode_lock);
        return -EBUSY;
    }
    if (!atomic_cas(&peer_role, BLE_PEER_ROLE_STOPPED, BLE_PEER_ROLE_CONNECTING))
    {
        k_mutex_unlock(&peer_mode_lock);
        return -EALREADY;
    }

    int scan_ret = ble_scan_stop();
    if (scan_ret < 0)
    {
        atomic_set(&peer_role, BLE_PEER_ROLE_STOPPED);
        k_mutex_unlock(&peer_mode_lock);
        return scan_ret;
    }

    bt_addr_le_copy(&peer_target_address, peer_addr);
    peer_target_valid = true;
    int ret = peer_connection_start_locked();
    if (ret != 0)
    {
        peer_target_valid = false;
        atomic_set(&peer_role, BLE_PEER_ROLE_STOPPED);
    }
    k_mutex_unlock(&peer_mode_lock);
    return ret;
}

int ble_peer_stop(void)
{
    k_mutex_lock(&peer_mode_lock, K_FOREVER);
    atomic_set(&peer_role, BLE_PEER_ROLE_STOPPED);
    peer_target_valid = false;
    (void)k_work_cancel_delayable(&peer_restart_work);

    int ret = 0;
    if (atomic_get(&advertising))
    {
        ret = bt_le_adv_stop();
        if (ret == 0)
        {
            atomic_set(&advertising, 0);
        }
    }
    k_mutex_lock(&connection_lock, K_FOREVER);
    struct bt_conn *conn = peer_connection != NULL
                               ? bt_conn_ref(peer_connection)
                               : NULL;
    k_mutex_unlock(&connection_lock);
    if (conn != NULL)
    {
        int disconnect_ret = bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        bt_conn_unref(conn);
        if (ret == 0 && disconnect_ret != 0 && disconnect_ret != -ENOTCONN)
        {
            ret = disconnect_ret;
        }
    }

    k_msgq_purge(&peer_tx_queue);
    atomic_set(&client_ready, 0);
    atomic_set(&server_tx_ready, 0);
    k_mutex_unlock(&peer_mode_lock);
    return ret;
}

int ble_peer_send_line(const char *line, size_t length)
{
    if (line == NULL || length == 0 || length > BLE_PEER_LINE_MAX - 1)
    {
        return -EINVAL;
    }
    if (atomic_get(&peer_role) == BLE_PEER_ROLE_STOPPED)
    {
        return -ENOTCONN;
    }
    for (size_t i = 0; i < length; i++)
    {
        if (line[i] == '\0' || line[i] == '\r' || line[i] == '\n')
        {
            return -EINVAL;
        }
    }
    k_mutex_lock(&connection_lock, K_FOREVER);
    bool connected = peer_connection != NULL;
    k_mutex_unlock(&connection_lock);
    if (!connected)
    {
        return -ENOTCONN;
    }
    if ((atomic_get(&peer_role) == BLE_PEER_ROLE_CONNECTING &&
         !atomic_get(&client_ready)) ||
        (atomic_get(&peer_role) == BLE_PEER_ROLE_LISTENING &&
         !atomic_get(&server_tx_ready)))
    {
        return -EAGAIN;
    }

    char message[LUA_SERIAL_MSG_SIZE] = {0};
    memcpy(message, line, length);
    message[length] = '\n';
    return k_msgq_put(&peer_tx_queue, message, K_NO_WAIT);
}

void ble_peer_get_status(bool *enabled, bool *connected)
{
    *enabled = atomic_get(&peer_role) != BLE_PEER_ROLE_STOPPED;

    k_mutex_lock(&connection_lock, K_FOREVER);
    *connected = peer_connection != NULL;
    k_mutex_unlock(&connection_lock);
}

static int cmd_ble_peer(const struct shell *sh, size_t argc, char **argv)
{
    int ret;
    if (strcmp(argv[1], "listen") == 0)
    {
        ret = ble_peer_listen();
        if (ret != 0)
        {
            shell_error(sh, "Could not start BLE peer advertising: %d", ret);
            return ret;
        }
        shell_print(sh, "Advertising as a BLE NUS peer.");
        return 0;
    }
    if (strcmp(argv[1], "connect") == 0)
    {
        if (argc != 4)
        {
            shell_error(sh, "Usage: ble_peer connect <address> <public|random|public-id|random-id>");
            return -EINVAL;
        }
        bt_addr_le_t peer_addr;
        ret = bt_addr_le_from_str(argv[2], argv[3], &peer_addr);
        if (ret != 0)
        {
            shell_error(sh, "Invalid BLE peer address or type: %d", ret);
            return ret;
        }
        ret = ble_peer_connect(&peer_addr);
        if (ret != 0)
        {
            shell_error(sh, "Could not connect to selected BLE peer: %d", ret);
            return ret;
        }
        shell_print(sh, "Connecting to %s (%s).", argv[2], argv[3]);
        return 0;
    }
    if (strcmp(argv[1], "stop") == 0)
    {
        ret = ble_peer_stop();
        if (ret != 0)
        {
            shell_error(sh, "Could not stop BLE peer connection: %d", ret);
            return ret;
        }
        shell_print(sh, "BLE peer link stopped.");
        return 0;
    }
    if (strcmp(argv[1], "status") == 0)
    {
        bool enabled;
        bool connected;
        ble_peer_get_status(&enabled, &connected);
        bool tx_ready = atomic_get(&peer_role) == BLE_PEER_ROLE_CONNECTING
                            ? atomic_get(&client_ready)
                            : atomic_get(&server_tx_ready);
        shell_print(sh, "BLE peer: %s; connection: %s; NUS TX: %s",
                    enabled ? "enabled" : "stopped",
                    connected ? "connected" : "disconnected",
                    tx_ready ? "ready" : "not ready");
        return 0;
    }

    shell_error(sh, "Usage: ble_peer <listen|connect <address> <type>|stop|status>");
    return -EINVAL;
}

SHELL_CMD_ARG_REGISTER(ble_peer, NULL,
                       "Connect to another Furrydex over bidirectional BLE NUS",
                       cmd_ble_peer, 2, 0);
