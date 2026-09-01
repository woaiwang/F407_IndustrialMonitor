#include "bsp_uart.h"

#include <stddef.h>

#include "usart.h"

#define BSP_UART_RX_RING_BUFFER_SIZE 1024U
#define BSP_UART_TX_TIMEOUT_MS       100U

static uint8_t s_rxByte;
static uint8_t s_rxRingStorage[BSP_UART_RX_RING_BUFFER_SIZE];
static RingBuffer_t s_rxRingBuffer;

volatile uint32_t g_uartRxByteCount = 0;
volatile uint32_t g_uartRingOverflowCount = 0;
volatile uint32_t g_uartRxErrorCount = 0;

void BSP_UART_Init(void)
{
    /*
     * FreeRTOS syscall boundary is priority 5 (BASEPRI = 0x50), which masks
     * priorities 5 through 15 in critical sections. Current stable RX uses
     * byte interrupt + RingBuffer; USART1 IRQ performs HAL RX/error handling.
     * Keep the CubeMX USART1 NVIC setting synchronized with this value.
     */
    HAL_NVIC_SetPriority(USART1_IRQn, 4, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

    RingBuffer_Init(&s_rxRingBuffer,
                    s_rxRingStorage,
                    BSP_UART_RX_RING_BUFFER_SIZE);

    /* UART RX DMA temporarily disabled at application level. */
    (void)HAL_UART_Receive_IT(&huart1, &s_rxByte, 1U);
}

void BSP_UART_ProcessRx(void)
{
    /* Interrupt RX requires no polling. */
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

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart != &huart1)
    {
        return;
    }

    if (RingBuffer_Put(&s_rxRingBuffer, s_rxByte))
    {
        g_uartRxByteCount++;
    }
    else
    {
        g_uartRingOverflowCount++;
    }

    (void)HAL_UART_Receive_IT(&huart1, &s_rxByte, 1U);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart != &huart1)
    {
        return;
    }

    g_uartRxErrorCount++;
    (void)HAL_UART_Receive_IT(&huart1, &s_rxByte, 1U);
}
