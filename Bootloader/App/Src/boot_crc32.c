#include "boot_crc32.h"

#include <stddef.h>

#define BOOT_CRC32_INITIAL_VALUE         0xFFFFFFFFUL
#define BOOT_CRC32_FINAL_XOR_VALUE       0xFFFFFFFFUL
#define BOOT_CRC32_REVERSED_POLYNOMIAL   0xEDB88320UL

uint32_t Boot_Crc32_Begin(void)
{
    return BOOT_CRC32_INITIAL_VALUE;
}

uint32_t Boot_Crc32_Update(uint32_t crc, const uint8_t *data, uint32_t length)
{
    uint32_t index;
    uint8_t bit;

    if ((data == NULL) && (length != 0U))
    {
        return crc;
    }

    for (index = 0U; index < length; index++)
    {
        crc ^= data[index];

        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 1U) != 0U)
            {
                crc = (crc >> 1U) ^ BOOT_CRC32_REVERSED_POLYNOMIAL;
            }
            else
            {
                crc >>= 1U;
            }
        }
    }

    return crc;
}

uint32_t Boot_Crc32_Finish(uint32_t crc)
{
    return crc ^ BOOT_CRC32_FINAL_XOR_VALUE;
}

uint32_t Boot_Crc32_Calculate(const uint8_t *data, uint32_t length)
{
    uint32_t crc;

    if ((data == NULL) && (length != 0U))
    {
        return 0U;
    }

    crc = Boot_Crc32_Begin();
    crc = Boot_Crc32_Update(crc, data, length);

    return Boot_Crc32_Finish(crc);
}
