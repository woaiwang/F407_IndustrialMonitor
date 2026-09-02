#include "boot_flash.h"

#include <stddef.h>

#include "boot_config.h"
#include "boot_crc32.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_flash_ex.h"

#define BOOT_FLASH_WORD_SIZE             4U
#define BOOT_FLASH_SECTOR_4_END          0x08020000UL
#define BOOT_FLASH_SECTOR_5_END          0x08040000UL
#define BOOT_FLASH_SECTOR_6_END          0x08060000UL
#define BOOT_FLASH_SECTOR_7_END          0x08080000UL
#define BOOT_FLASH_SECTOR_8_END          0x080A0000UL
#define BOOT_FLASH_SECTOR_9_END          0x080C0000UL
#define BOOT_FLASH_SECTOR_10_END         0x080E0000UL

typedef struct
{
    uint32_t firmwareSize;
    uint32_t receivedBytes;
    uint8_t stagedWord[BOOT_FLASH_WORD_SIZE];
    uint8_t stagedLength;
    bool prepared;
    bool erased;
} Boot_FlashContext_t;

static Boot_FlashContext_t bootFlashContext;

static void Boot_FlashResetStagedWord(void);
static uint32_t Boot_FlashGetLastSector(uint32_t firmwareSize);
static bool Boot_FlashProgramStagedWord(void);

bool Boot_FlashPrepareUpdate(uint32_t firmwareSize)
{
    if ((firmwareSize == 0U) || (firmwareSize > BOOT_APP_MAX_SIZE))
    {
        return false;
    }

    bootFlashContext.firmwareSize = firmwareSize;
    bootFlashContext.receivedBytes = 0U;
    bootFlashContext.prepared = true;
    bootFlashContext.erased = false;
    Boot_FlashResetStagedWord();

    return true;
}

bool Boot_FlashEraseApplication(void)
{
    FLASH_EraseInitTypeDef eraseInit = {0};
    uint32_t lastSector;
    uint32_t sectorError = 0U;
    bool eraseSucceeded;

    if (!bootFlashContext.prepared ||
        (bootFlashContext.receivedBytes != 0U))
    {
        return false;
    }

    lastSector = Boot_FlashGetLastSector(bootFlashContext.firmwareSize);
    if ((lastSector < BOOT_APP_FIRST_FLASH_SECTOR) ||
        (lastSector > BOOT_APP_LAST_FLASH_SECTOR))
    {
        return false;
    }

    eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
    eraseInit.Sector = BOOT_APP_FIRST_FLASH_SECTOR;
    eraseInit.NbSectors = lastSector - BOOT_APP_FIRST_FLASH_SECTOR + 1U;
    eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return false;
    }

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR |
                           FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR |
                           FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    eraseSucceeded = (HAL_FLASHEx_Erase(&eraseInit, &sectorError) == HAL_OK);
    (void)HAL_FLASH_Lock();

    if (!eraseSucceeded)
    {
        return false;
    }

    bootFlashContext.erased = true;
    Boot_FlashResetStagedWord();

    return true;
}

bool Boot_FlashWriteData(const uint8_t *data, uint16_t length)
{
    uint16_t index;
    bool writeSucceeded = true;

    if (!bootFlashContext.prepared || !bootFlashContext.erased ||
        (data == NULL) || (length == 0U) ||
        (length > (bootFlashContext.firmwareSize - bootFlashContext.receivedBytes)))
    {
        return false;
    }

    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return false;
    }

    for (index = 0U; index < length; index++)
    {
        bootFlashContext.stagedWord[bootFlashContext.stagedLength] = data[index];
        bootFlashContext.stagedLength++;
        bootFlashContext.receivedBytes++;

        if ((bootFlashContext.stagedLength == BOOT_FLASH_WORD_SIZE) ||
            (bootFlashContext.receivedBytes == bootFlashContext.firmwareSize))
        {
            if (!Boot_FlashProgramStagedWord())
            {
                writeSucceeded = false;
                break;
            }
        }
    }

    (void)HAL_FLASH_Lock();

    return writeSucceeded;
}

bool Boot_FlashVerify(uint32_t expectedCrc32)
{
    const uint8_t *firmware = (const uint8_t *)BOOT_APP_ADDRESS;

    if (!bootFlashContext.prepared || !bootFlashContext.erased ||
        (bootFlashContext.receivedBytes != bootFlashContext.firmwareSize) ||
        (bootFlashContext.stagedLength != 0U))
    {
        return false;
    }

    return (Boot_Crc32_Calculate(firmware, bootFlashContext.firmwareSize) ==
            expectedCrc32);
}

uint32_t Boot_FlashGetReceivedBytes(void)
{
    return bootFlashContext.receivedBytes;
}

static void Boot_FlashResetStagedWord(void)
{
    uint8_t index;

    for (index = 0U; index < BOOT_FLASH_WORD_SIZE; index++)
    {
        bootFlashContext.stagedWord[index] = 0xFFU;
    }

    bootFlashContext.stagedLength = 0U;
}

static uint32_t Boot_FlashGetLastSector(uint32_t firmwareSize)
{
    uint32_t firmwareEnd = BOOT_APP_ADDRESS + firmwareSize;

    if (firmwareEnd <= BOOT_FLASH_SECTOR_4_END)
    {
        return FLASH_SECTOR_4;
    }

    if (firmwareEnd <= BOOT_FLASH_SECTOR_5_END)
    {
        return FLASH_SECTOR_5;
    }

    if (firmwareEnd <= BOOT_FLASH_SECTOR_6_END)
    {
        return FLASH_SECTOR_6;
    }

    if (firmwareEnd <= BOOT_FLASH_SECTOR_7_END)
    {
        return FLASH_SECTOR_7;
    }

    if (firmwareEnd <= BOOT_FLASH_SECTOR_8_END)
    {
        return FLASH_SECTOR_8;
    }

    if (firmwareEnd <= BOOT_FLASH_SECTOR_9_END)
    {
        return FLASH_SECTOR_9;
    }

    if (firmwareEnd <= BOOT_FLASH_SECTOR_10_END)
    {
        return FLASH_SECTOR_10;
    }

    return FLASH_SECTOR_11;
}

static bool Boot_FlashProgramStagedWord(void)
{
    uint32_t address;
    uint32_t word;

    address = BOOT_APP_ADDRESS + bootFlashContext.receivedBytes -
              bootFlashContext.stagedLength;

    if ((address < BOOT_APP_ADDRESS) ||
        (address > (BOOT_APP_END - BOOT_FLASH_WORD_SIZE)))
    {
        return false;
    }

    word = (uint32_t)bootFlashContext.stagedWord[0] |
           ((uint32_t)bootFlashContext.stagedWord[1] << 8U) |
           ((uint32_t)bootFlashContext.stagedWord[2] << 16U) |
           ((uint32_t)bootFlashContext.stagedWord[3] << 24U);

    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, word) != HAL_OK)
    {
        return false;
    }

    Boot_FlashResetStagedWord();

    return true;
}
