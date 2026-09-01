#include "bsp_w25q128.h"

#include <string.h>

#include "spi.h"

#define W25Q128_CMD_WRITE_ENABLE       0x06U
#define W25Q128_CMD_READ_STATUS_REG1   0x05U
#define W25Q128_CMD_READ_JEDEC_ID      0x9FU
#define W25Q128_CMD_READ_DATA          0x03U
#define W25Q128_CMD_PAGE_PROGRAM       0x02U
#define W25Q128_CMD_SECTOR_ERASE       0x20U

#define W25Q128_STATUS_BUSY_MASK               0x01U
#define W25Q128_SPI_TIMEOUT_MS                  100U
#define W25Q128_PAGE_PROGRAM_TIMEOUT_MS         100U
#define W25Q128_SECTOR_ERASE_TIMEOUT_MS         3000U
#define W25Q128_MAX_SPI_TRANSFER_SIZE           W25Q128_PAGE_SIZE
#define W25Q128_SELF_TEST_DATA_LENGTH           300U

volatile W25Q128_Debug_t g_w25q128Debug;

static uint8_t s_selfTestWriteData[W25Q128_SELF_TEST_DATA_LENGTH];
static uint8_t s_selfTestReadData[W25Q128_SELF_TEST_DATA_LENGTH];
static uint8_t s_readDummyData[W25Q128_MAX_SPI_TRANSFER_SIZE];

static inline void W25Q128_CS_Low(void)
{
    HAL_GPIO_WritePin(W25Q_CS_GPIO_Port, W25Q_CS_Pin, GPIO_PIN_RESET);
}

static inline void W25Q128_CS_High(void)
{
    HAL_GPIO_WritePin(W25Q_CS_GPIO_Port, W25Q_CS_Pin, GPIO_PIN_SET);
}

static uint8_t W25Q128_IsAddressRangeValid(uint32_t address, uint32_t length)
{
    if (address >= W25Q128_CAPACITY_BYTES)
    {
        return 0U;
    }

    return (length <= (W25Q128_CAPACITY_BYTES - address)) ? 1U : 0U;
}

static uint8_t W25Q128_IsJedecIdValid(uint32_t jedecId)
{
    return ((jedecId != 0x000000U) && (jedecId != 0xFFFFFFU)) ? 1U : 0U;
}

static HAL_StatusTypeDef W25Q128_ReadStatusRegister1(uint8_t *statusRegister)
{
    uint8_t command = W25Q128_CMD_READ_STATUS_REG1;
    HAL_StatusTypeDef status;

    if (statusRegister == NULL)
    {
        return HAL_ERROR;
    }

    W25Q128_CS_Low();

    *statusRegister = 0xFFU;
    status = HAL_SPI_Transmit(&hspi1,
                              &command,
                              1U,
                              W25Q128_SPI_TIMEOUT_MS);
    if (status == HAL_OK)
    {
        status = HAL_SPI_Receive(&hspi1,
                                 statusRegister,
                                 1U,
                                 W25Q128_SPI_TIMEOUT_MS);
    }

    W25Q128_CS_High();

    return status;
}

static HAL_StatusTypeDef W25Q128_WriteEnable(void)
{
    uint8_t command = W25Q128_CMD_WRITE_ENABLE;
    HAL_StatusTypeDef status;

    W25Q128_CS_Low();
    status = HAL_SPI_Transmit(&hspi1,
                              &command,
                              1U,
                              W25Q128_SPI_TIMEOUT_MS);
    W25Q128_CS_High();

    return status;
}

static void W25Q128_BuildAddressCommand(uint8_t command,
                                        uint32_t address,
                                        uint8_t commandBuffer[4])
{
    commandBuffer[0] = command;
    commandBuffer[1] = (uint8_t)(address >> 16U);
    commandBuffer[2] = (uint8_t)(address >> 8U);
    commandBuffer[3] = (uint8_t)address;
}

HAL_StatusTypeDef BSP_W25Q128_Init(void)
{
    uint32_t jedecId;

    g_w25q128Debug.jedecId = 0U;
    g_w25q128Debug.testAddress = W25Q128_TEST_ADDRESS;
    g_w25q128Debug.eraseCount = 0U;
    g_w25q128Debug.writeCount = 0U;
    g_w25q128Debug.readCount = 0U;
    g_w25q128Debug.verifyErrorCount = 0U;
    g_w25q128Debug.initPassed = 0U;
    g_w25q128Debug.erasePassed = 0U;
    g_w25q128Debug.writePassed = 0U;
    g_w25q128Debug.readPassed = 0U;
    g_w25q128Debug.testPassed = 0U;

    W25Q128_CS_High();
    jedecId = BSP_W25Q128_ReadJedecId();
    g_w25q128Debug.jedecId = jedecId;

    if (W25Q128_IsJedecIdValid(jedecId) == 0U)
    {
        return HAL_ERROR;
    }

    g_w25q128Debug.initPassed = 1U;

    return HAL_OK;
}

uint32_t BSP_W25Q128_ReadJedecId(void)
{
    uint8_t transmitData[4] = {W25Q128_CMD_READ_JEDEC_ID, 0xFFU, 0xFFU, 0xFFU};
    uint8_t receiveData[4] = {0U};
    HAL_StatusTypeDef status;

    W25Q128_CS_Low();
    status = HAL_SPI_TransmitReceive(&hspi1,
                                     transmitData,
                                     receiveData,
                                     (uint16_t)sizeof(transmitData),
                                     W25Q128_SPI_TIMEOUT_MS);
    W25Q128_CS_High();

    if (status != HAL_OK)
    {
        return 0U;
    }

    return ((uint32_t)receiveData[1] << 16U) |
           ((uint32_t)receiveData[2] << 8U) |
           (uint32_t)receiveData[3];
}

HAL_StatusTypeDef BSP_W25Q128_Read(uint32_t address,
                                    uint8_t *data,
                                    uint32_t length)
{
    uint8_t commandBuffer[4];
    HAL_StatusTypeDef status;
    uint32_t transferLength;

    if (length == 0U)
    {
        return HAL_OK;
    }

    if ((data == NULL) || (W25Q128_IsAddressRangeValid(address, length) == 0U))
    {
        return HAL_ERROR;
    }

    while (length > 0U)
    {
        transferLength = (length > W25Q128_MAX_SPI_TRANSFER_SIZE) ?
                         W25Q128_MAX_SPI_TRANSFER_SIZE : length;
        W25Q128_BuildAddressCommand(W25Q128_CMD_READ_DATA, address, commandBuffer);

        W25Q128_CS_Low();
        status = HAL_SPI_Transmit(&hspi1,
                                  commandBuffer,
                                  (uint16_t)sizeof(commandBuffer),
                                  W25Q128_SPI_TIMEOUT_MS);
        if (status == HAL_OK)
        {
            status = HAL_SPI_TransmitReceive(&hspi1,
                                             s_readDummyData,
                                             data,
                                             (uint16_t)transferLength,
                                             W25Q128_SPI_TIMEOUT_MS);
        }
        W25Q128_CS_High();

        if (status != HAL_OK)
        {
            return status;
        }

        address += transferLength;
        data += transferLength;
        length -= transferLength;
    }

    g_w25q128Debug.readCount++;

    return HAL_OK;
}

HAL_StatusTypeDef BSP_W25Q128_Write(uint32_t address,
                                     const uint8_t *data,
                                     uint32_t length)
{
    uint32_t pageRemaining;
    uint32_t programLength;
    HAL_StatusTypeDef status;

    if (length == 0U)
    {
        return HAL_OK;
    }

    if ((data == NULL) || (W25Q128_IsAddressRangeValid(address, length) == 0U))
    {
        return HAL_ERROR;
    }

    while (length > 0U)
    {
        pageRemaining = W25Q128_PAGE_SIZE - (address & (W25Q128_PAGE_SIZE - 1U));
        programLength = (length < pageRemaining) ? length : pageRemaining;

        status = BSP_W25Q128_PageProgram(address, data, (uint16_t)programLength);
        if (status != HAL_OK)
        {
            return status;
        }

        address += programLength;
        data += programLength;
        length -= programLength;
    }

    return HAL_OK;
}

HAL_StatusTypeDef BSP_W25Q128_PageProgram(uint32_t address,
                                           const uint8_t *data,
                                           uint16_t length)
{
    uint8_t commandBuffer[4];
    HAL_StatusTypeDef status;

    if ((data == NULL) ||
        (length == 0U) ||
        (length > W25Q128_PAGE_SIZE) ||
        (W25Q128_IsAddressRangeValid(address, length) == 0U) ||
        (((address & (W25Q128_PAGE_SIZE - 1U)) + length) > W25Q128_PAGE_SIZE))
    {
        return HAL_ERROR;
    }

    status = W25Q128_WriteEnable();
    if (status != HAL_OK)
    {
        return status;
    }

    W25Q128_BuildAddressCommand(W25Q128_CMD_PAGE_PROGRAM, address, commandBuffer);

    W25Q128_CS_Low();
    status = HAL_SPI_Transmit(&hspi1,
                              commandBuffer,
                              (uint16_t)sizeof(commandBuffer),
                              W25Q128_SPI_TIMEOUT_MS);
    if (status == HAL_OK)
    {
        status = HAL_SPI_Transmit(&hspi1,
                                  data,
                                  length,
                                  W25Q128_SPI_TIMEOUT_MS);
    }
    W25Q128_CS_High();

    if (status != HAL_OK)
    {
        return status;
    }

    status = BSP_W25Q128_WaitBusy(W25Q128_PAGE_PROGRAM_TIMEOUT_MS);
    if (status == HAL_OK)
    {
        g_w25q128Debug.writeCount++;
    }

    return status;
}

HAL_StatusTypeDef BSP_W25Q128_SectorErase(uint32_t address)
{
    uint8_t commandBuffer[4];
    HAL_StatusTypeDef status;

    if ((W25Q128_IsAddressRangeValid(address, 1U) == 0U) ||
        ((address & (W25Q128_SECTOR_SIZE - 1U)) != 0U))
    {
        return HAL_ERROR;
    }

    status = W25Q128_WriteEnable();
    if (status != HAL_OK)
    {
        return status;
    }

    W25Q128_BuildAddressCommand(W25Q128_CMD_SECTOR_ERASE, address, commandBuffer);

    W25Q128_CS_Low();
    status = HAL_SPI_Transmit(&hspi1,
                              commandBuffer,
                              (uint16_t)sizeof(commandBuffer),
                              W25Q128_SPI_TIMEOUT_MS);
    W25Q128_CS_High();

    if (status != HAL_OK)
    {
        return status;
    }

    status = BSP_W25Q128_WaitBusy(W25Q128_SECTOR_ERASE_TIMEOUT_MS);
    if (status == HAL_OK)
    {
        g_w25q128Debug.eraseCount++;
    }

    return status;
}

HAL_StatusTypeDef BSP_W25Q128_WaitBusy(uint32_t timeoutMs)
{
    uint8_t statusRegister;
    uint32_t startTick = HAL_GetTick();
    HAL_StatusTypeDef status;

    do
    {
        status = W25Q128_ReadStatusRegister1(&statusRegister);
        if (status != HAL_OK)
        {
            return status;
        }

        if ((statusRegister & W25Q128_STATUS_BUSY_MASK) == 0U)
        {
            return HAL_OK;
        }

        HAL_Delay(1U);
    } while ((HAL_GetTick() - startTick) < timeoutMs);

    return HAL_TIMEOUT;
}

void BSP_W25Q128_SelfTest(void)
{
    uint32_t index;
    HAL_StatusTypeDef status;

    if (g_w25q128Debug.initPassed == 0U)
    {
        return;
    }

    g_w25q128Debug.testAddress = W25Q128_TEST_ADDRESS;
    g_w25q128Debug.erasePassed = 0U;
    g_w25q128Debug.writePassed = 0U;
    g_w25q128Debug.readPassed = 0U;
    g_w25q128Debug.testPassed = 0U;

    for (index = 0U; index < W25Q128_SELF_TEST_DATA_LENGTH; index++)
    {
        s_selfTestWriteData[index] = (uint8_t)((index * 37U) + 0x5AU);
        s_selfTestReadData[index] = 0U;
    }

    status = BSP_W25Q128_SectorErase(W25Q128_TEST_ADDRESS);
    if (status != HAL_OK)
    {
        return;
    }
    g_w25q128Debug.erasePassed = 1U;

    status = BSP_W25Q128_Write(W25Q128_TEST_ADDRESS,
                                s_selfTestWriteData,
                                W25Q128_SELF_TEST_DATA_LENGTH);
    if (status != HAL_OK)
    {
        return;
    }
    g_w25q128Debug.writePassed = 1U;

    status = BSP_W25Q128_Read(W25Q128_TEST_ADDRESS,
                               s_selfTestReadData,
                               W25Q128_SELF_TEST_DATA_LENGTH);
    if (status != HAL_OK)
    {
        return;
    }
    g_w25q128Debug.readPassed = 1U;

    if (memcmp(s_selfTestWriteData,
               s_selfTestReadData,
               W25Q128_SELF_TEST_DATA_LENGTH) != 0)
    {
        g_w25q128Debug.verifyErrorCount++;
        return;
    }

    g_w25q128Debug.testPassed = 1U;
}
