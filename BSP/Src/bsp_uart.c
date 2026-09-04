#include "bsp_uart.h"

#include <stddef.h>

#include "usart.h"

#define BSP_UART_RX_RING_BUFFER_SIZE 1024U
#define BSP_UART_TX_TIMEOUT_MS       100U

static uint8_t s_rxByte;
static uint8_t s_rxRingStorage[BSP_UART_RX_RING_BUFFER_SIZE];
static RingBuffer_t s_rxRingBuffer;

volatile uint32_t rxArmCount = 0;
volatile uint32_t rxCallbackCount = 0;
volatile uint32_t rxByteCount = 0;
volatile uint32_t rxErrorCount = 0;
volatile uint32_t lastRxByte = 0;
volatile uint32_t lastHalStatus = (uint32_t)HAL_OK;
volatile uint32_t uartRxState = (uint32_t)HAL_UART_STATE_RESET;
volatile uint32_t uartErrorCode = (uint32_t)HAL_UART_ERROR_NONE;

volatile uint32_t g_uartRingOverflowCount = 0;

static HAL_StatusTypeDef BSP_UART_ArmReceive(void);

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

    /* UART RX DMA remains disabled; application RX uses one-byte interrupt mode. */
    (void)BSP_UART_ArmReceive();
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

    rxCallbackCount++;
    rxByteCount++;
    lastRxByte = s_rxByte;

    if (!RingBuffer_Put(&s_rxRingBuffer, s_rxByte))
    {
        g_uartRingOverflowCount++;
    }

    /* HAL restores RxState to READY before this callback; arm the next byte. */
    (void)BSP_UART_ArmReceive();
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart != &huart1)
    {
        return;
    }

    rxErrorCount++;
    uartErrorCode = (uint32_t)huart1.ErrorCode;
    uartRxState = (uint32_t)huart1.RxState;

    /* ORE aborts reception and leaves RxState READY; FE/NE can leave it armed. */
    if (huart1.RxState == HAL_UART_STATE_READY)
    {
        (void)BSP_UART_ArmReceive();
    }
}

static HAL_StatusTypeDef BSP_UART_ArmReceive(void)
{
    HAL_StatusTypeDef status;

    rxArmCount++;
    status = HAL_UART_Receive_IT(&huart1, &s_rxByte, 1U);
    lastHalStatus = (uint32_t)status;
    uartRxState = (uint32_t)huart1.RxState;

    return status;
}
