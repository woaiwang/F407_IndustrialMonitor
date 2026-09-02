#ifndef BOOT_FLASH_H
#define BOOT_FLASH_H

#include <stdbool.h>
#include <stdint.h>

bool Boot_FlashPrepareUpdate(uint32_t firmwareSize);
bool Boot_FlashEraseApplication(void);
bool Boot_FlashWriteData(const uint8_t *data, uint16_t length);
bool Boot_FlashVerify(uint32_t expectedCrc32);
uint32_t Boot_FlashGetReceivedBytes(void);

#endif /* BOOT_FLASH_H */
