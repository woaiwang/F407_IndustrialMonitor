#ifndef BSP_UART_H
#define BSP_UART_H

#include <stdint.h>

#include "main.h"
#include "ring_buffer.h"

void BSP_UART_Init(void);
void BSP_UART_ProcessRx(void);
HAL_StatusTypeDef BSP_UART_Send(const uint8_t *data, uint16_t len);
RingBuffer_t *BSP_UART_GetRxRingBuffer(void);

extern volatile uint32_t g_uartRxByteCount;
extern volatile uint32_t g_uartRingOverflowCount;
extern volatile uint32_t g_uartRxErrorCount;

#endif /* BSP_UART_H */
