#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/gatt.h>
#include <sys/byteorder.h>
#include <sys/printk.h>
#include <zephyr.h>

#include "stdint.h"
#include "stdlib.h"
#include "string.h"

/**
 * @brief Valor do UUID do serviço BT UART.
 *
 */
#define BT_UART_UUID_SVC_VAL 0x2BC4

/**
 * @brief UUID do serviço BT UART.
 *
 */
#define BT_UART_SVC_UUID BT_UUID_DECLARE_16(BT_UART_UUID_SVC_VAL)

/**
 * @brief Valor do UUID da característica de notificação do BT UART.
 *
 */
#define BT_UART_NOTIFY_CHAR_UUID_VAL 0x2BC5

/**
 * @brief UUID da caracterísitica de notificação do BT UART.
 *
 */
#define BT_UART_NOTIFY_CHAR_UUID BT_UUID_DECLARE_16(BT_UART_NOTIFY_CHAR_UUID_VAL)

/**
 * @brief Valor do UUID da característica de escrita do BT UART.
 *
 */
#define BT_UART_WRITE_CHAR_UUID_VAL 0x2BC6

/**
 * @brief UUID da característica de escrita do BT UART.
 *
 */
#define BT_UART_WRITE_CHAR_UUID BT_UUID_DECLARE_16(BT_UART_WRITE_CHAR_UUID_VAL)

/**
 * @brief Callback function for when the CCC (Client Characteristic Configuration) value
 * is changed.
 * @param attr The GATT attribute that triggered the change.
 * @param value The new value of the CCC.
 */
static void change_notify(const struct bt_gatt_attr *attr, uint16_t value);


/**
 * @brief Callback function for when data is written to the UART (Universal Asynchronous
 * Receiver/Transmitter).
 * @param conn Pointer to the Bluetooth connection where the write occurred.
 * @param attr Pointer to the GATT attribute that triggered the write.
 * @param buf Pointer to the buffer containing the data that was written.
 * @param len Length of the data that was written.
 * @param offset Offset within the attribute value.
 * @param flags Flags associated with the write operation.
 * @return 0 if the write was successful, otherwise a negative error code.
 */
static int write_uart(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                      const void *buf, uint16_t len, uint16_t offset, uint8_t flags);

/**
 * @brief Callback function for when the MTU (Maximum Transmission Unit) is updated.
 * @param conn Pointer to the Bluetooth connection.
 * @param tx The maximum number of bytes that the local device can transmit in a single
 * packet.
 * @param rx The maximum number of bytes that the remote device can transmit in a single
 * packet.
 */
void mtu_updated(struct bt_conn *conn, uint16_t tx, uint16_t rx);

/**
 * @brief Callback function for when a peripheral is connected.
 * @param conn Pointer to the Bluetooth connection.
 * @param err Error code associated with the connection attempt.
 */
static void connected(struct bt_conn *conn, uint8_t err);

/**
 * @brief Callback function for when a peripheral is connected.
 * @param conn Pointer to the Bluetooth connection.
 * @param reason Reason for the disconnection.
 */
static void disconnected(struct bt_conn *conn, uint8_t reason);

/**
 * @brief Main function of the program.
 * @return 0 if the program was successful, otherwise a negative error code.
 */
void main(void);

/** @brief Default Bluetooth connection object. */
struct bt_conn *default_conn = NULL;

/** @brief GATT callback object. */
static struct bt_gatt_cb gatt_cb = {
    .att_mtu_updated = mtu_updated,
};

/** @brief Connection callback object. */
struct bt_conn_cb conn_cb = {
    .connected    = connected,
    .disconnected = disconnected,
};

/** @brief Advertisement data to be broadcasted by the device. */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UART_UUID_SVC_VAL), ),
};
