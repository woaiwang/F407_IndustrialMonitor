#ifndef BSP_W25Q128_H
#define BSP_W25Q128_H

#include <stdint.h>

#include "main.h"

#define W25Q128_CAPACITY_BYTES    (16U * 1024U * 1024U)
#define W25Q128_PAGE_SIZE         256U
#define W25Q128_SECTOR_SIZE       4096U
#define W25Q128_TEST_ADDRESS      0x00FFF000U

typedef struct
{
    uint32_t jedecId;
    uint32_t testAddress;

    uint32_t eraseCount;
    uint32_t writeCount;
    uint32_t readCount;

    uint32_t verifyErrorCount;

    uint8_t initPassed;
    uint8_t erasePassed;
    uint8_t writePassed;
    uint8_t readPassed;
    uint8_t testPassed;
} W25Q128_Debug_t;

HAL_StatusTypeDef BSP_W25Q128_Init(void);

uint32_t BSP_W25Q128_ReadJedecId(void);

HAL_StatusTypeDef BSP_W25Q128_Read(uint32_t address,
                                    uint8_t *data,
                                    uint32_t length);

HAL_StatusTypeDef BSP_W25Q128_Write(uint32_t address,
                                     const uint8_t *data,
                                     uint32_t length);

HAL_StatusTypeDef BSP_W25Q128_PageProgram(uint32_t address,
                                           const uint8_t *data,
                                           uint16_t length);

HAL_StatusTypeDef BSP_W25Q128_SectorErase(uint32_t address);

HAL_StatusTypeDef BSP_W25Q128_WaitBusy(uint32_t timeoutMs);

void BSP_W25Q128_SelfTest(void);

extern volatile W25Q128_Debug_t g_w25q128Debug;

#endif /* BSP_W25Q128_H */
