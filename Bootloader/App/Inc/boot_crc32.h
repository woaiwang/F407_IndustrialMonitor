#ifndef BOOT_CRC32_H
#define BOOT_CRC32_H

#include <stdint.h>

uint32_t Boot_Crc32_Begin(void);
uint32_t Boot_Crc32_Update(uint32_t crc, const uint8_t *data, uint32_t length);
uint32_t Boot_Crc32_Finish(uint32_t crc);
uint32_t Boot_Crc32_Calculate(const uint8_t *data, uint32_t length);

#endif /* BOOT_CRC32_H */
