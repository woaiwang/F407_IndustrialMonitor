#include "main.h"

#include "boot_application.h"
#include "boot_config.h"
#include "boot_protocol.h"
#include "usart.h"

#define BOOT_SEND_LITERAL(text) \
    Boot_Send((const uint8_t *)(text), (uint16_t)(sizeof(text) - 1U))

#define BOOT_PROTOCOL_ENTRY_CLEANUP_TIMEOUT_MS 30U

static const uint8_t bootUpgradeAck = BOOT_UPGRADE_ACK_BYTE;

static void Boot_Send(const uint8_t *data, uint16_t length)
{
    (void)HAL_UART_Transmit(&huart1, data, length, BOOT_UART_TX_TIMEOUT_MS);
}

static bool Boot_UpgradeRequested(void)
{
    uint8_t receivedByte = 0U;

    if (HAL_UART_Receive(&huart1,
                         &receivedByte,
                         1U,
                         BOOT_UART_RX_TIMEOUT_MS) != HAL_OK)
    {
        return false;
    }

    return (receivedByte == BOOT_UPGRADE_HANDSHAKE_BYTE);
}

static void Boot_DiscardHandshakeResidue(void)
{
    uint8_t receivedByte;
    uint32_t startTick = HAL_GetTick();

    while ((HAL_GetTick() - startTick) <
           BOOT_PROTOCOL_ENTRY_CLEANUP_TIMEOUT_MS)
    {
        if (HAL_UART_Receive(&huart1, &receivedByte, 1U, 1U) != HAL_OK)
        {
            continue;
        }

        if (receivedByte != BOOT_UPGRADE_HANDSHAKE_BYTE)
        {
            break;
        }
    }
}

static void Boot_EnterProtocolMode(void)
{
    Boot_Send(&bootUpgradeAck, 1U);
    Boot_DiscardHandshakeResidue();
    Boot_Protocol_Run();
}

static void Boot_StayInBootloader(void)
{
    uint8_t receivedByte;

    while (1)
    {
        if (HAL_UART_Receive(&huart1,
                             &receivedByte,
                             1U,
                             BOOT_UART_RX_TIMEOUT_MS) == HAL_OK)
        {
            if (receivedByte == BOOT_UPGRADE_HANDSHAKE_BYTE)
            {
                Boot_EnterProtocolMode();
            }
        }
    }
}

int main(void)
{
    HAL_Init();
    MX_USART1_UART_Init();

    BOOT_SEND_LITERAL("\r\nBootloader Start\r\n");

    if (Boot_UpgradeRequested())
    {
        Boot_EnterProtocolMode();
    }

    if (Boot_ApplicationIsValid())
    {
        BOOT_SEND_LITERAL("APP Valid\r\nJump APP\r\n");
        Boot_JumpToApplication();
    }

    BOOT_SEND_LITERAL("APP Invalid\r\nStay Bootloader\r\n");
    Boot_StayInBootloader();
}

void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}
