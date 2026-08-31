#include "bsp_uart.h"

#include <stddef.h>

#include "usart.h"

#define BSP_UART_RX_DMA_BUFFER_SIZE  128U
#define BSP_UART_RX_RING_BUFFER_SIZE 256U
#define BSP_UART_TX_TIMEOUT_MS       100U

static uint8_t s_rxDmaBuffer[BSP_UART_RX_DMA_BUFFER_SIZE];
static uint8_t s_rxRingStorage[BSP_UART_RX_RING_BUFFER_SIZE];
static RingBuffer_t s_rxRingBuffer;
static volatile uint8_t s_rxRestartPending;

static HAL_StatusTypeDef BSP_UART_StartReceiveToIdle(void);
static void BSP_UART_StoreReceivedData(uint16_t size);

void BSP_UART_Init(void)
{
    RingBuffer_Init(&s_rxRingBuffer,
                    s_rxRingStorage,
                    BSP_UART_RX_RING_BUFFER_SIZE);

    s_rxRestartPending = 1U;
    BSP_UART_ProcessRx();
}

void BSP_UART_ProcessRx(void)
{
    if (s_rxRestartPending != 0U)
    {
        s_rxRestartPending = 0U;

        if (BSP_UART_StartReceiveToIdle() != HAL_OK)
        {
            s_rxRestartPending = 1U;
        }
    }
}

HAL_StatusTypeDef BSP_UART_Send(const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0U))
    {
        return HAL_ERROR;
    }

    /* The first protocol revision uses blocking TX to avoid DMA busy handling. */
    return HAL_UART_Transmit(&huart1, data, len, BSP_UART_TX_TIMEOUT_MS);
}

RingBuffer_t *BSP_UART_GetRxRingBuffer(void)
{
    return &s_rxRingBuffer;
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
    if (huart != &huart1)
    {
        return;
    }

    BSP_UART_StoreReceivedData(size);
    s_rxRestartPending = 1U;
    BSP_UART_ProcessRx();
}

static HAL_StatusTypeDef BSP_UART_StartReceiveToIdle(void)
{
    HAL_StatusTypeDef status;

    /*
     * HAL reports received length through HAL_UARTEx_RxEventCallback().
     * Source: https://github.com/STMicroelectronics/stm32f4xx-hal-driver/blob/master/Src/stm32f4xx_hal_uart.c
     */
    status = HAL_UARTEx_ReceiveToIdle_DMA(&huart1,
                                          s_rxDmaBuffer,
                                          BSP_UART_RX_DMA_BUFFER_SIZE);

    if ((status == HAL_OK) && (huart1.hdmarx != NULL))
    {
        /* Do not process the normal-DMA half-transfer event as a completed frame. */
        __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
    }

    return status;
}

static void BSP_UART_StoreReceivedData(uint16_t size)
{
    uint16_t index;

    if (size > BSP_UART_RX_DMA_BUFFER_SIZE)
    {
        size = BSP_UART_RX_DMA_BUFFER_SIZE;
    }

    for (index = 0U; index < size; index++)
    {
        (void)RingBuffer_Put(&s_rxRingBuffer, s_rxDmaBuffer[index]);
    }
}
