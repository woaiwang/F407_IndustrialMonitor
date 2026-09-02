#ifndef BOOT_CONFIG_H
#define BOOT_CONFIG_H

#include <stdint.h>

#define BOOT_FLASH_START                 0x08000000UL
#define BOOT_BOOTLOADER_ADDRESS          BOOT_FLASH_START
#define BOOT_BOOTLOADER_END              0x08010000UL

#define BOOT_APP_ADDRESS                 BOOT_BOOTLOADER_END
#define BOOT_APP_END                     0x08100000UL
#define BOOT_APP_MAX_SIZE                (BOOT_APP_END - BOOT_APP_ADDRESS)

#define BOOT_APP_FIRST_FLASH_SECTOR      4U
#define BOOT_APP_LAST_FLASH_SECTOR       11U

#define BOOT_SRAM_START                  0x20000000UL
#define BOOT_SRAM_END                    0x20020000UL
#define BOOT_MINIMUM_STACK_MARGIN         0x00000100UL

#define BOOT_UART_BAUD_RATE              115200U
#define BOOT_UART_RX_TIMEOUT_MS          1000U
#define BOOT_UART_TX_TIMEOUT_MS          100U
#define BOOT_UPGRADE_HANDSHAKE_BYTE      0x7FU
#define BOOT_UPGRADE_ACK_BYTE            0x79U

/* STM32F407 has 82 external interrupt lines, requiring three NVIC register banks. */
#define BOOT_NVIC_REGISTER_COUNT         3U

#endif /* BOOT_CONFIG_H */
