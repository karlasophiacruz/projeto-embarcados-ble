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
    printk("MTU was updated. Max Transmit Bytes (TX): %d\nMax Receive Bytes (RX):%d.\n", tx, rx);
}

static bool found_service_handler(struct bt_data *data, void *user_data)
{
    bt_addr_le_t *addr = user_data;
    int i;
    printk("Data type: %u\nData length: %u.\n", data->type, data->data_len);

    if(data->type == BT_DATA_UUID16_SOME || data->type == BT_DATA_UUID16_ALL) {
        data->data_len % sizeof(uint16_t) != 0U ? printk("Advertisement error.\n"), true : false;
        
        uint16_t* data_ptr = (uint16_t*) data->data;
        int num_elems = data->data_len / sizeof(uint16_t);

        for (i = 0; i < num_elems; i++) {
            struct bt_le_conn_param *bt_param;
            struct bt_uuid *uuid;
            uint16_t u16;
            int err;

            u16 = sys_le16_to_cpu(*data_ptr);
            uuid = BT_UUID_DECLARE_16(u16);
            if (bt_uuid_cmp(uuid, BT_UART_SVC_UUID)) {
                data_ptr++;
                continue;
            }

            err = bt_le_scan_stop();
            if (err) {
                printk("Fail: Scan couldn't stop. Error: %d.\n", err);
                data_ptr++;
                continue;
            }

            bt_param = BT_LE_CONN_PARAM_DEFAULT;
            err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, bt_param, &default_conn);
            if (err) {
                printk("Fail: Couldn't create conn. Error: %d.\n", err);
                scanBluetoothDevices(0);
            }

            return false;
        }

    }

    return true;
}

static void found_device_handler(const bt_addr_le_t *device_address, int8_t rssi,
                                 uint8_t type, struct net_buf_simple *advertising_data)
{
    char device_address_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(device_address, device_address_str, sizeof(device_address_str));

    if (type != BT_GAP_ADV_TYPE_ADV_IND && type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
        return;
    }

    printk("New device found with address: %s and RSSI: %d.\n", device_address_str, rssi);

    if (rssi < -70) {
        return;
    }
   
    bt_data_parse(advertising_data, found_service_handler, (void *) device_address);
}


void scanBluetoothDevices(int error) {
    struct bt_le_scan_param scanParameters = {
        .type = BT_LE_SCAN_TYPE_ACTIVE,
        .options = BT_LE_SCAN_OPT_NONE,
        .interval = BT_GAP_SCAN_FAST_INTERVAL,
        .window = BT_GAP_SCAN_FAST_WINDOW,
    };

    error = bt_le_scan_start(&scanParameters, found_device_handler);
    if (error) {
        printk("Error: Unable to start scanning. Error code: %d\n", error);
        return;
    }

    printk("Success: Scanning started\n");
}

static uint8_t central_notification_handler(struct bt_conn *connection,
                                             struct bt_gatt_subscribe_params *params,
                                             const void *notification_buffer, uint16_t buffer_length)
{
    if (!notification_buffer) {
        printk("Unsubscribed.\n");
        params->value_handle = 0U;
        return BT_GATT_ITER_CONTINUE;
    }

    char notification_data[buffer_length + 1];
    memcpy(notification_data, notification_buffer, buffer_length);
    notification_data[buffer_length] = '\0';

    printk("Notification Received. Data: %s. Length: %u.\n", notification_data, buffer_length);

    return BT_GATT_ITER_CONTINUE;
}


static uint8_t discover_characteristics(struct bt_conn *conn,
                                        const struct bt_gatt_attr *attr,
                                        struct bt_gatt_discover_params *parameters)
{
    int err;

    if (!attr) {
        printk("All characteristics have been discovered.\n");
        memset(parameters, 0, sizeof(struct bt_gatt_discover_params));
        return BT_GATT_ITER_STOP;
    }

    printk("Attribute handle: %u.\n", attr->handle);

    if (!bt_uuid_cmp(discover_params.uuid, BT_UART_SVC_UUID)) {
        memcpy(&uuid_t, BT_UART_NOTIFY_CHAR_UUID, sizeof(uuid_t));
        discover_params.uuid         = &uuid_t.uuid;
        discover_params.start_handle = attr->handle + 1;
        discover_params.type         = BT_GATT_DISCOVER_CHARACTERISTIC;

        err = bt_gatt_discover(conn, &discover_params);
        if (err) {
            printk("Failed to discover. Error code: %d.\n", err);
        }

    } else if (!bt_uuid_cmp(discover_params.uuid, BT_UART_NOTIFY_CHAR_UUID)) {
        memcpy(&uuid_t, BT_UART_WRITE_CHAR_UUID, sizeof(uuid_t));
        discover_params.uuid          = &uuid_t.uuid;
        discover_params.start_handle  = attr->handle + 1;
        discover_params.type          = BT_GATT_DISCOVER_CHARACTERISTIC;
        subscribe_params.value_handle = bt_gatt_attr_value_handle(attr);

        err = bt_gatt_discover(conn, &discover_params);

        if (err) {
            printk("Failed to discover. Error code: %d.\n", err);
        }

    } else if (!bt_uuid_cmp(discover_params.uuid, BT_UART_WRITE_CHAR_UUID)) {
        memcpy(&uuid_t, BT_UUID_GATT_CCC, sizeof(uuid_t));
        discover_params.uuid         = &uuid_t.uuid;
        discover_params.start_handle = attr->handle + 1;
        discover_params.type         = BT_GATT_DISCOVER_DESCRIPTOR;
        uart_write                   = bt_gatt_attr_value_handle(attr);

        err = bt_gatt_discover(conn, &discover_params);
        if (err) {
            printk("Failed to discover. Error code: %d.\n", err);
        }

    } else {
        subscribe_params.notify     = central_notification_handler;
        subscribe_params.value      = BT_GATT_CCC_NOTIFY;
        subscribe_params.ccc_handle = attr->handle;
        
        err = bt_gatt_subscribe(conn, &subscribe_params);
        if (err && err != -EALREADY) {
            printk("Failed to subscribe. Error code: %d.\n", err);
        } else {
            printk("Subscribed sucessful.\n");
        }

        return BT_GATT_ITER_STOP;
    }

    return BT_GATT_ITER_STOP;
}

static void connected(struct bt_conn *connection, uint8_t error)
{
    char address[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(connection), address, sizeof(address));

    if (error) {
        printk("Failed to connect. Address: %s. Error code: %u\n", address, error);

        bt_conn_unref(default_conn);
        default_conn = NULL;

        scanBluetoothDevices(0);
        return;
    }

    printk("Connected to device with address: %s\n", address);

    if (connection == default_conn) {
        printk("Connected successfully. Address: %s\n", address);

        memcpy(&uuid_t, BT_UART_SVC_UUID, sizeof(uuid_t));
        discover_params.uuid         = &uuid_t.uuid;
        discover_params.func         = discover_characteristics;
        discover_params.start_handle = 0x0001;
        discover_params.end_handle   = 0xffff;
        discover_params.type         = BT_GATT_DISCOVER_PRIMARY;

        error = bt_gatt_discover(default_conn, &discover_params);
        if (error) {
            printk("Failed to discover characteristics. Error code: %d.\n", error);
            return;
        }
    }
}

static void disconnected(struct bt_conn *connection, uint8_t reason)
{
    char address[BT_ADDR_LE_STR_LEN];

    if (connection != default_conn) {
        return;
    }

    bt_addr_le_to_str(bt_conn_get_dst(connection), address, sizeof(address));

    printk("Device with address %s disconnected. Reason: 0x%02x\n", address, reason);

    bt_conn_unref(default_conn);
    default_conn = NULL;

    scanBluetoothDevices(0);
}


static void input_task(void)
{
    int err = 0;
    char *input = NULL;

    console_getline_init();

    while (true) {
        k_sleep(K_MSEC(200));

        printk("Enter desired input: ");
        input = console_getline();

        if (input == NULL) {
            printk("Error receiving user input!\n");
            continue;
        }

        printk("Sending input: %s\n", input);

        if (default_conn == NULL) {
            printk("No device connected. Please connect to a device first.\n");
            continue;
        }

        err = bt_gatt_write_without_response(default_conn, uart_write, input,
                                            strlen(input), false);
        if (err) {
            printk("Failed to write. Error: %d\n", err);
        }
    }
}

int main(void)
{
    int err;
    printk("Hello! I'm using Zephyr %s on %s, a %s board. \n\n", KERNEL_VERSION_STRING,
           CONFIG_BOARD, CONFIG_ARCH);

    bt_conn_cb_register(&conn_cb);
    bt_gatt_cb_register(&gatt_cb);
    err = bt_enable(scanBluetoothDevices);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }

    printk("Bluetooth initialized\n");


    return 0;
}