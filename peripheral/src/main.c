#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/gatt.h>
#include <bluetooth/hci.h>
#include <bluetooth/uuid.h>
#include <console/console.h>
#include <ctype.h>
#include <errno.h>
#include <kernel.h>
#include <peripheral.h>
#include <stddef.h>
#include <string.h>
#include <sys/byteorder.h>
#include <sys/printk.h>
#include <version.h>
#include <zephyr.h>
#include <zephyr/types.h>

BT_GATT_SERVICE_DEFINE(bt_uart, BT_GATT_PRIMARY_SERVICE(BT_UART_SVC_UUID),
                       BT_GATT_CHARACTERISTIC(BT_UART_NOTIFY_CHAR_UUID,
                                              BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE,
                                              NULL, NULL, NULL),
                       BT_GATT_CHARACTERISTIC(BT_UART_WRITE_CHAR_UUID, BT_GATT_CHRC_WRITE,
                                              BT_GATT_PERM_WRITE, NULL, write_uart, NULL),
                       BT_GATT_CCC(change_notify,
                                   (BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)), );

static void change_notify(const struct bt_gatt_attr *attr, uint16_t value)
{
    ARG_UNUSED(attr);

    bool notify_enabled = (value == BT_GATT_CCC_NOTIFY);

    printk("Notify %s.\n", (notify_enabled ? "enabled" : "disabled"));
}

static int write_uart(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                      const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    if (!buf || !len) {
        printk("Invalid parameter.\n");
        return -EINVAL;
    }

    char data[len + 1];
    memcpy(data, buf, len);
    data[len] = '\0';

    printk("Received data: %s\n", data);

    for (int i = 0; i < len; i++) {
        if ((data[i] >= 'a') && ((data[i] <= 'z'))) {
            data[i] = toupper(data[i]);
        }
    }

    printk("Converted data: %s\n", data);

    int err = bt_gatt_notify(NULL, &bt_uart.attrs[1], data, len);
    if (err) {
        printk("Error notifying: %d\n", err);
        return err;
    }

    return 0;
}

void mtu_updated(struct bt_conn *conn, uint16_t tx, uint16_t rx)
{
    printk("MTU was updated. Max Transmit Bytes (TX): %d\nMax Receive Bytes (RX):%d.\n", tx, rx);
}

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (conn != default_conn) {
        if (err) {
            printk("Peripheral connection failed (err %u).\n", err);
        } else {
            default_conn = bt_conn_ref(conn);
            printk("Peripheral connected.\n");
        }
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    int err = 0;
    printk("Disconnected. Reason: %u.\n", reason);

    if (default_conn) {
        bt_conn_unref(default_conn);
        default_conn = NULL;
    }

    err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Failed to start advertising. Error: %d.\n", err);
    } else {
        printk("Advertising restarted.\n");
    }
}

void main(void)
{
    int err;
    bt_conn_cb_register(&conn_cb);
    bt_gatt_cb_register(&gatt_cb);
    err = bt_enable(NULL);
    if (err) {
        printk("Fail: Bluetooth couldn't start. Error: %d\n", err);
        return;
    }

    printk("Success: Bluetooth initialized\n");

    err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Fail: Advertising failed to start. Error: %d.\n", err);
        return;
    }

    printk("Success: Started advertising.\n");
    return;
}
