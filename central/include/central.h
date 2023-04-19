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
 * @brief Callback function for when the MTU (Maximum Transmission Unit) is updated.
 * @param conn The Bluetooth connection.
 * @param tx The maximum number of bytes that the local device can transmit in a single
 * packet.
 * @param rx The maximum number of bytes that the remote device can transmit in a single
 * packet.
 */
void mtu_updated(struct bt_conn *conn, uint16_t tx, uint16_t rx);

/**
* @brief Callback function to handle the service discovery results.
* @param data Pointer to a structure containing information about the scanned BLE service.
* @param user_data Pointer to user-specific data passed to this function.
* @return true if the service is not of interest or if the service data is invalid.
* @return false if the service is of interest and the connection attempt was initiated.
*/
static bool svc_found(struct bt_data *data, void *user_data);

/**
 * @brief Callback function for handling BLE device found events.
 * @param addr [in] Pointer to the Bluetooth LE address of the device found.
 * @param rssi [in] RSSI value of the advertising packet from the device.
 * @param type [in] Type of advertising event.
 * @param ad [in] Pointer to the advertising data of the device.
 */
static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                         struct net_buf_simple *ad);

/**
 * @brief Starts a BLE scan with the specified scan parameters and callback function for
 * discovered devices.
 * @param err A pointer to an integer that will contain any errors encountered during
 * scanning.
 */
static void start_scan(int err);

/**
* @brief Callback function to handle BLE GATT notifications.
* @param conn The connection object.
* @param params Pointer to the subscription parameters object.
* @param buf Pointer to the buffer containing the received data.
* @param length Length of the received data.
* @return BT_GATT_ITER_CONTINUE.
*/
static uint8_t central_notify(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
                      const void *buf, uint16_t length);

/**
 * @brief Callback function to discover Bluetooth characteristics.
 * @param[in] conn - pointer to the Bluetooth connection.
 * @param[in] attr - pointer to the Bluetooth GATT attribute.
 * @param[in] params - pointer to the discover parameters for the GATT client.
 * @return BT_GATT_ITER_STOP when the discovery is complete, otherwise
 * BT_GATT_ITER_CONTINUE.
 * */
static uint8_t discover_characteristics(struct bt_conn *conn,
                                        const struct bt_gatt_attr *attr,
                                        struct bt_gatt_discover_params *params);

/**
* @brief Callback function called when a Bluetooth Low Energy (BLE) connection is
established or fails to connect.
* @param conn Pointer to the Bluetooth Low Energy connection object.
* @param err The error code received from the connection attempt. A value of 0 indicates
successful connection, while any other value indicates an error.
* @return void
*/
static void connected(struct bt_conn *conn, uint8_t err);

/**
* @brief Callback function to handle a disconnection event from a Bluetooth Low Energy
(BLE) device.
* @param conn The connection object.
* @param reason The reason for the disconnection.
* @return None.
*/
static void disconnected(struct bt_conn *conn, uint8_t reason);

/**
 * @brief Task to handle user input and send it to the BLE central device via GATT.
 * @return void.
 */
static void input_task(void);

/**
* @brief Main function of the program.
* @return Returns 0 if initialization is successful.
*/
int main(void);

/** @brief Default Bluetooth connection object */
static struct bt_conn *default_conn = NULL;

/** @brief Bluetooth GATT callback object */
static struct bt_gatt_cb gatt_cb = {
    .att_mtu_updated = mtu_updated,
};

/** @brief Bluetooth connection callback object */
struct bt_conn_cb conn_cb = {
    .connected    = connected,
    .disconnected = disconnected,
};

/** @brief Thread object to handle user input */
K_THREAD_DEFINE(input, 1024, input_task, NULL, NULL, NULL, 1, 0, 1000);

/** @brief Bluetooth GATT discover parameters */
static struct bt_gatt_discover_params discover_params = {0};

/** @brief Bluetooth UUID object for UART service */
static struct bt_uuid_16 uuid_t = BT_UUID_INIT_16(0);

/** @brief Bluetooth GATT subscribe parameters */
static struct bt_gatt_subscribe_params subscribe_params = {0};

/** @brief Write buffer for UART service */
static uint16_t uart_write = 0;