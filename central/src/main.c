#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/gatt.h>
#include <bluetooth/hci.h>
#include <bluetooth/uuid.h>
#include <console/console.h>
#include <errno.h>
#include <kernel.h>
#include <stddef.h>
#include <string.h>
#include <sys/byteorder.h>
#include <sys/printk.h>
#include <version.h>
#include <zephyr.h>
#include <zephyr/types.h>

#include "central.h"


static void start_scan(void);
void ble_central_mtu_updated(struct bt_conn *conn, uint16_t tx, uint16_t rx);

static struct bt_conn *default_conn;
static struct bt_gatt_cb gatt_cb = {
    .att_mtu_updated = ble_central_mtu_updated,
};


static struct bt_gatt_discover_params discover_params;
static struct bt_uuid_16 uuid;
struct bt_gatt_subscribe_params subscribe_params;
uint16_t uart_write;

void ble_central_mtu_updated(struct bt_conn *conn, uint16_t tx, uint16_t rx)
{
    printk("|BLE CENTRAL| Updated MTU. TX:%d RX:%d bytes.\n", tx, rx);
}
/**
 * @brief Verificação de serviços BLE.
 *
 * @param data
 * @param user_data
 * @return true
 * @return false
 */
static bool svc_found(struct bt_data *data, void *user_data)
{
    bt_addr_le_t *addr = user_data;
    int i;

    printk("D: %u data_len %u.\n", data->type, data->data_len);

    switch (data->type) {
    case BT_DATA_UUID16_SOME:
    case BT_DATA_UUID16_ALL:
        if (data->data_len % sizeof(uint16_t) != 0U) {
            printk("AD error.\n");
            return true;
        }

        for (i = 0; i < data->data_len; i += sizeof(uint16_t)) {
            struct bt_le_conn_param *bt_param;
            struct bt_uuid *uuid;
            uint16_t u16;
            int err;

            memcpy(&u16, &data->data[i], sizeof(u16));
            uuid = BT_UUID_DECLARE_16(sys_le16_to_cpu(u16));
            if (bt_uuid_cmp(uuid, BT_UART_SVC_UUID)) {
                continue;
            }

            err = bt_le_scan_stop();
            if (err) {
                printk("Stop LE scan failed (err %d).\n", err);
                continue;
            }

            bt_param = BT_LE_CONN_PARAM_DEFAULT;
            err =
                bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, bt_param, &default_conn);
            if (err) {
                printk("Create conn failed (err %d).\n", err);
                start_scan();
            }

            return false;
        }
    }

    return true;
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                         struct net_buf_simple *ad)
{
    char addr_str[BT_ADDR_LE_STR_LEN];
    int err;

    if (default_conn) {
        return;
    }

    /* We're only interested in connectable events */
    if (type != BT_GAP_ADV_TYPE_ADV_IND && type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
        return;
    }

    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
    printk("Device found: %s (RSSI %d)\n", addr_str, rssi);

    /* connect only to devices in close proximity */
    if (rssi < -70) {
        return;
    }

    if (bt_le_scan_stop()) {
        return;
    }

    bt_data_parse(ad, svc_found, (void *) addr);
}

static void start_scan(void)
{
    int err;

    /* This demo doesn't require active scan */
    err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
    if (err) {
        printk("Scanning failed to start (err %d)\n", err);
        return;
    }

    printk("Scanning successfully started\n");
}

static uint8_t notify(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
                      const void *buf, uint16_t length)
{
    if (!buf) {
        printk("Unsubscribed.\n");
        params->value_handle = 0U;
        return BT_GATT_ITER_CONTINUE;
    }

    char data[length + 1];

    memcpy(data, buf, length);
    data[length] = '\0';

    printk("Notification Received data: %s. Length %u.\n", data, length);

    return BT_GATT_ITER_CONTINUE;
}

static uint8_t discover_characteristics(struct bt_conn *conn,
                                        const struct bt_gatt_attr *attr,
                                        struct bt_gatt_discover_params *params)
{
    int err;

    if (!attr) {
        printk(" Discover complete.\n");
        (void) memset(params, 0, sizeof(*params));
        return BT_GATT_ITER_STOP;
    }

    printk(" Discover attribute handle: %u.\n", attr->handle);

    /* Características do Serviço Bluetooth UART. */
    if (!bt_uuid_cmp(discover_params.uuid, BT_UART_SVC_UUID)) {
        memcpy(&uuid, BT_UART_NOTIFY_CHAR_UUID, sizeof(uuid));
        discover_params.uuid         = &uuid.uuid;
        discover_params.start_handle = attr->handle + 1;
        discover_params.type         = BT_GATT_DISCOVER_CHARACTERISTIC;

        /* Característica de notificação. */
        err = bt_gatt_discover(conn, &discover_params);
        if (err) {
            printk("Discover failed (err %d).\n", err);
        }
    } else if (!bt_uuid_cmp(discover_params.uuid, BT_UART_NOTIFY_CHAR_UUID)) {
        memcpy(&uuid, BT_UART_WRITE_CHAR_UUID, sizeof(uuid));
        discover_params.uuid         = &uuid.uuid;
        discover_params.start_handle = attr->handle + 1;
        discover_params.type         = BT_GATT_DISCOVER_CHARACTERISTIC;

        /* Característica de escrita. */
        err = bt_gatt_discover(conn, &discover_params);

        subscribe_params.value_handle = bt_gatt_attr_value_handle(attr);
        if (err) {
            printk("Discover failed (err %d).\n", err);
        }
    } else if (!bt_uuid_cmp(discover_params.uuid, BT_UART_WRITE_CHAR_UUID)) {
        memcpy(&uuid, BT_UUID_GATT_CCC, sizeof(uuid));
        discover_params.uuid         = &uuid.uuid;
        discover_params.start_handle = attr->handle + 1;
        discover_params.type         = BT_GATT_DISCOVER_DESCRIPTOR;
        uart_write                   = bt_gatt_attr_value_handle(attr);

        /* Descritor da . */
        err = bt_gatt_discover(conn, &discover_params);
        if (err) {
            printk("Discover failed (err %d).\n", err);
        }
    } else {
        subscribe_params.notify     = notify;
        subscribe_params.value      = BT_GATT_CCC_NOTIFY;
        subscribe_params.ccc_handle = attr->handle;

        err = bt_gatt_subscribe(conn, &subscribe_params);
        if (err && err != -EALREADY) {
            printk("Subscribe failed (err %d).\n", err);
        } else {
            printk("Subscribed!\n");
        }

        return BT_GATT_ITER_STOP;
    }

    return BT_GATT_ITER_STOP;
}


static void connected(struct bt_conn *conn, uint8_t err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (err) {
        printk("Failed to connect to %s (%u)\n", addr, err);

        bt_conn_unref(default_conn);
        default_conn = NULL;

        start_scan();
        return;
    }

    if (conn != default_conn) {
        return;
    }

    printk("Connected: %s\n", addr);
    printk("Enter al line finishing with Enter:\n");

    /* Inicializa identificação das características do dispositivos pareado. */
    if (conn == default_conn) {
        memcpy(&uuid, BT_UART_SVC_UUID, sizeof(uuid));
        discover_params.uuid = &uuid.uuid;
        /* Serviços -> Características */
        discover_params.func         = discover_characteristics;
        discover_params.start_handle = 0x0001;
        discover_params.end_handle   = 0xffff;
        discover_params.type         = BT_GATT_DISCOVER_PRIMARY;

        err = bt_gatt_discover(default_conn, &discover_params);
        if (err) {
            printk("Discover failed(err %d).\n", err);
            return;
        }
    }

    while (1) {
        k_sleep(K_MSEC(100));
        printk(">");
        char *s = console_getline();

        if (s == NULL) {
            printk("Error receiving line!\n");
            continue;
        }
        printk("Typed line: %s\n", s);
        printk("Last char was: 0x%x\n", s[strlen(s) - 1]);

        err = bt_gatt_write_without_response(default_conn, uart_write, s, strlen(s),
                                             false);
        if (err) {
            printk("%s: Write cmd failed (%d).\n", __func__, err);
        }
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    if (conn != default_conn) {
        return;
    }

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    printk("Disconnected: %s (reason 0x%02x)\n", addr, reason);

    bt_conn_unref(default_conn);
    default_conn = NULL;

    start_scan();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected    = connected,
    .disconnected = disconnected,
};

int main(void)
{
    int err;


    bt_gatt_cb_register(&gatt_cb);
    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }

    printk("Bluetooth initialized\n");

    start_scan();
    printk("Hello! I'm using Zephyr %s on %s, a %s board. \n\n", KERNEL_VERSION_STRING,
           CONFIG_BOARD, CONFIG_ARCH);

    console_getline_init();
    return 0;
}

/**
 * @brief Renode CLI
 *
 */
// renode -e "include @scripts/single-node/nrf52840.resc;
// emulation CreateServerSocketTerminal 1234 'externalUART';
// connector Connect sysbus.uart0 externalUART;
// sysbus LoadELF @C:\Users\Pedro\Repos\projeto-embarcados-ble\central\.pio\build\nrf52840_dk\firmware.elf;start"