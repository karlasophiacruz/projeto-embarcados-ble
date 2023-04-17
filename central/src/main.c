#include <console/console.h>
#include <string.h>
#include <sys/printk.h>
#include <version.h>
#include <zephyr.h>

void main(void) {
  printk("Hello! I'm using Zephyr %s on %s, a %s board. \n\n",
         KERNEL_VERSION_STRING, CONFIG_BOARD, CONFIG_ARCH);

  console_getline_init();
  printk("Enter al line finishing with Enter:\n");

  while (1) {
    printk(">");
    char *s = console_getline();

    printk("Typed line: %s\n", s);
    printk("Last char was: 0x%x\n", s[strlen(s) - 1]);
  }
}

/**
 * @brief Renode CLI
 *
 */
// renode -e "include @scripts/single-node/nrf52840.resc;emulation
// CreateServerSocketTerminal 1234 'externalUART';connector Connect sysbus.uart0
// externalUART;sysbus LoadELF
// @C:\Users\Pedro\Repos\projeto-embarcados-ble\central\.pio\build\nrf52840_dk\firmware.elf;start"