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