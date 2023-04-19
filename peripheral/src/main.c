#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/gatt.h>
#include <bluetooth/hci.h>
#include <bluetooth/uuid.h>
#include <console/console.h>
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

void mtu_updated(struct bt_conn *conn, uint16_t tx, uint16_t rx);
static void change_notify(const struct bt_gatt_attr *attr, uint16_t value);
static int write_uart(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                      const void *buf, uint16_t len, uint16_t offset, uint8_t flags);
static void connected(struct bt_conn *conn, uint8_t err);
static void disconnected(struct bt_conn *conn, uint8_t reason);


struct bt_conn *default_conn = NULL;
static struct bt_gatt_cb gatt_cb = {
    .att_mtu_updated = mtu_updated,
};

struct bt_conn_cb conn_cb= {
    .connected = connected,
    .disconnected = disconnected,
};

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UART_UUID_SVC_VAL), ),
};

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
    int err = 0;
    char data[len + 1];

    /* Copia dados recebidos. */
    memcpy(data, buf, len);
    data[len] = '\0';

    printk("|BLE PERIPHERAL| Received data %s.\n", data);

    /* Converte letras minúsculas para maiúsculas. */
    for (int i = 0; i < len; i++) {
        if ((data[i] >= 'a') && ((data[i] <= 'z'))) {
            data[i] = 'A' + (data[i] - 'a');
        }
    }

    printk("|BLE PERIPHERAL| Sending data %s.\n", data);

    /* Notifica Central com o dados convertidos. */
    err = bt_gatt_notify(NULL, &bt_uart.attrs[1], data, len);
    if (err) {
        printk("|BLE PERIPHERAL| Error notifying.\n");
    }

    return 0;
}

void mtu_updated(struct bt_conn *conn, uint16_t tx, uint16_t rx)
{
    printk("Updated MTU. TX:%d RX:%d bytes.\n", tx, rx);
}

static void connected(struct bt_conn *conn, uint8_t err)
{
    if(conn == default_conn){
        return;
    }
    
    if (err) {
        printk("Peripheral Connection failed (err %u).\n", err);
    } else {
        default_conn = bt_conn_ref(conn);
        printk("Connected.\n");
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    int err = 0;
    printk("Disconnected, reason %u.\n", reason);

    /* Decrementa conexão anterior do contador. */
    if (default_conn) {
        bt_conn_unref(default_conn);
        default_conn = NULL;
    }

    /* Volta a realizar o adversiting. */
    err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Advertising failed to start (err %d).\n", err);
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected    = connected,
    .disconnected = disconnected,
};


void main(void)
{
    int err;
    bt_conn_cb_register(&conn_cb);
    bt_gatt_cb_register(&gatt_cb);
    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }

    printk("Bluetooth initialized\n");

    err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Advertising failed to start (err %d).\n", err);
        return;
    }

    printk("Started advertising.\n");
    return;
}
