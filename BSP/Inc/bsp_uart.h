#ifndef BSP_UART_H
#define BSP_UART_H

#include <stdint.h>

#include "main.h"
#include "ring_buffer.h"

void BSP_UART_Init(void);
void BSP_UART_ProcessRx(void);
HAL_StatusTypeDef BSP_UART_Send(const uint8_t *data, uint16_t len);
RingBuffer_t *BSP_UART_GetRxRingBuffer(void);

/* USART1 RX debug variables. Read from the debugger; never write at runtime. */
extern volatile uint32_t rxArmCount;
extern volatile uint32_t rxCallbackCount;
extern volatile uint32_t rxByteCount;
extern volatile uint32_t rxErrorCount;
extern volatile uint32_t lastRxByte;
extern volatile uint32_t lastHalStatus;
extern volatile uint32_t uartRxState;
extern volatile uint32_t uartErrorCode;

extern volatile uint32_t g_uartRingOverflowCount;

#endif /* BSP_UART_H */
