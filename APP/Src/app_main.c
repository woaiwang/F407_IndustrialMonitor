#include "app_main.h"

#include "main.h"
#include "usart.h"

#include <string.h>

void App_Init(void)
{
    const char msg[] =
        "\r\n"
        "================================\r\n"
        "F407 Industrial Monitor\r\n"
        "Firmware Version: 0.1.0\r\n"
        "System Init OK\r\n"
        "================================\r\n";

    HAL_UART_Transmit(&huart1,
                      (uint8_t *)msg,
                      strlen(msg),
                      HAL_MAX_DELAY);
}

void App_Loop(void)
{
    HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port,
                       LED_GREEN_Pin);

    const char msg[] = "Heartbeat\r\n";

    HAL_UART_Transmit(&huart1,
                      (uint8_t *)msg,
                      strlen(msg),
                      HAL_MAX_DELAY);

    HAL_Delay(1000);
}