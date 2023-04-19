#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/gap.h>
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

void mtu_updated(struct bt_conn *conn, uint16_t tx, uint16_t rx)
{
    printk("MTU was updated. TX:%d RX:%d bytes.\n", tx, rx);
}

static bool svc_found(struct bt_data *data, void *user_data)
{
    bt_addr_le_t *addr = user_data;
    int i;
    printk("D: %u L: %u.\n", data->type, data->data_len);

    switch (data->type) {
    case BT_DATA_UUID16_SOME:
    case BT_DATA_UUID16_ALL:
        if (data->data_len % sizeof(uint16_t) != 0U) {
            printk("Advertisement error.\n");
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
                printk("Fail: Scan couldn't stop. Error: %d.\n", err);
                continue;
            }

            bt_param = BT_LE_CONN_PARAM_DEFAULT;
            err =
                bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, bt_param, &default_conn);
            if (err) {
                printk("Fail: Couldn't create conn. Error: %d.\n", err);
                start_scan(0);
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
    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));


    if (type != BT_GAP_ADV_TYPE_ADV_IND && type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
        return;
    }

    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));

    printk("New device was found. Address: %s. RSSI: %d.\n", addr_str, rssi);

    if (rssi < -70) {
        return;
    }
   
    bt_data_parse(ad, svc_found, (void *) addr);
}

static void start_scan(int err)
{
    struct bt_le_scan_param scan_param = {
        .type     = BT_LE_SCAN_TYPE_ACTIVE,
        .options  = BT_LE_SCAN_OPT_NONE,
        .interval = BT_GAP_SCAN_FAST_INTERVAL,
        .window   = BT_GAP_SCAN_FAST_WINDOW,
    };

    err = bt_le_scan_start(&scan_param, device_found);
    if (err) {
        printk("Fail: Start Scan. Error: %d\n", err);
        return;
    }

    printk("Success: Scanning started\n");
}

static uint8_t central_notify(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
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

    printk("Success: Notification Received. Data: %s. Length: %u.\n", data, length);

    return BT_GATT_ITER_CONTINUE;
}

static uint8_t discover_characteristics(struct bt_conn *conn,
                                        const struct bt_gatt_attr *attr,
                                        struct bt_gatt_discover_params *params)
{
    int err;

    if (!attr) {
        printk("Success: Characteristics were discovered.\n");
        (void) memset(params, 0, sizeof(*params));
        return BT_GATT_ITER_STOP;
    }

    printk("Attribute handle: %u.\n", attr->handle);

    /* Características do Serviço Bluetooth UART. */
    if (!bt_uuid_cmp(discover_params.uuid, BT_UART_SVC_UUID)) {
        memcpy(&uuid_t, BT_UART_NOTIFY_CHAR_UUID, sizeof(uuid_t));
        discover_params.uuid         = &uuid_t.uuid;
        discover_params.start_handle = attr->handle + 1;
        discover_params.type         = BT_GATT_DISCOVER_CHARACTERISTIC;

        /* Característica de notificação. */
        err = bt_gatt_discover(conn, &discover_params);
        if (err) {
            printk("Fail. Error: %d.\n", err);
        }
    } else if (!bt_uuid_cmp(discover_params.uuid, BT_UART_NOTIFY_CHAR_UUID)) {
        memcpy(&uuid_t, BT_UART_WRITE_CHAR_UUID, sizeof(uuid_t));
        discover_params.uuid          = &uuid_t.uuid;
        discover_params.start_handle  = attr->handle + 1;
        discover_params.type          = BT_GATT_DISCOVER_CHARACTERISTIC;
        subscribe_params.value_handle = bt_gatt_attr_value_handle(attr);

        /* Característica de escrita. */
        err = bt_gatt_discover(conn, &discover_params);

        if (err) {
            printk("Fail. Error: %d.\n", err);
        }
    } else if (!bt_uuid_cmp(discover_params.uuid, BT_UART_WRITE_CHAR_UUID)) {
        memcpy(&uuid_t, BT_UUID_GATT_CCC, sizeof(uuid_t));
        discover_params.uuid         = &uuid_t.uuid;
        discover_params.start_handle = attr->handle + 1;
        discover_params.type         = BT_GATT_DISCOVER_DESCRIPTOR;
        uart_write                   = bt_gatt_attr_value_handle(attr);

        /* Descritor do serviço . */
        err = bt_gatt_discover(conn, &discover_params);
        if (err) {
            printk("Fail. Error: %d.\n", err);
        }
    } else {
        subscribe_params.notify     = central_notify;
        subscribe_params.value      = BT_GATT_CCC_NOTIFY;
        subscribe_params.ccc_handle = attr->handle;
        
        err = bt_gatt_subscribe(conn, &subscribe_params);
        if (err && err != -EALREADY) {
            printk("Fail. Error: %d.\n", err);
        } else {
            printk("Success.\n");
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
        printk("Fail: Couldn't connect. Error: %s(%u)\n", addr, err);

        bt_conn_unref(default_conn);
        default_conn = NULL;

        start_scan(0);
        return;
    }

    printk("Connected: %s\n", addr);
    
    if (conn == default_conn) {
        printk("Success: Connected. %s\n", addr);
        memcpy(&uuid_t, BT_UART_SVC_UUID, sizeof(uuid_t));
        discover_params.uuid = &uuid_t.uuid;
        /* Serviços -> Características */
        discover_params.func         = discover_characteristics;
        discover_params.start_handle = 0x0001;
        discover_params.end_handle   = 0xffff;
        discover_params.type         = BT_GATT_DISCOVER_PRIMARY;

        err = bt_gatt_discover(default_conn, &discover_params);
        if (err) {
            printk("Fail. Error: %d.\n", err);
            return;
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

    printk("Device %s disconnected. Reason: 0x%02x\n", addr, reason);

    bt_conn_unref(default_conn);
    default_conn = NULL;

    start_scan(0);
}

static void input_task(void)
{
    int err          = 0;
    char *recvd_line = NULL;

    console_getline_init();

    while (true) {
        k_sleep(K_MSEC(200));

        printk("Write the desired input: ");
        recvd_line = console_getline();

        if (recvd_line == NULL) {
            printk("Error receiving user input!\n");
            continue;
        }

        printk("Sending input: %s\n", recvd_line);

        if (default_conn == NULL) {
            printk("No device connected. Try again.");
            return;
        }

        err = bt_gatt_write_without_response(default_conn, uart_write, recvd_line,
                                             strlen(recvd_line), false);
        if (err) {
            printk("Fail: Couldn't write succesfullt. Error: %d\n", err);
        }
    }
}

int main(void)
{
    int err;

    bt_conn_cb_register(&conn_cb);
    bt_gatt_cb_register(&gatt_cb);
    err = bt_enable(start_scan);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }

    printk("Bluetooth initialized\n");


    printk("Hello! I'm using Zephyr %s on %s, a %s board. \n\n", KERNEL_VERSION_STRING,
           CONFIG_BOARD, CONFIG_ARCH);

    return 0;
}