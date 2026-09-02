#include "cli.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "bsp_uart.h"
#include "cmsis_os.h"
#include "sensor_manager.h"

#define CLI_OUTPUT_BUFFER_SIZE 128U

static void CLI_Execute(CliContext_t *context);
static void CLI_SendText(const char *text, uint16_t length);
static void CLI_SendStatus(void);
static void CLI_SendSensor(void);

void CLI_Init(CliContext_t *context)
{
    if (context == NULL)
    {
        return;
    }

    context->commandBuffer[0] = '\0';
    context->length = 0U;
    context->overflow = 0U;
}

void CLI_ProcessByte(CliContext_t *context, uint8_t data)
{
    if (context == NULL)
    {
        return;
    }

    if ((data == '\r') || (data == '\n'))
    {
        if ((context->length != 0U) || (context->overflow != 0U))
        {
            CLI_Execute(context);
        }

        CLI_Init(context);
        return;
    }

    if ((data < 0x20U) || (data > 0x7EU))
    {
        return;
    }

    if (context->overflow != 0U)
    {
        return;
    }

    if (context->length < (CLI_COMMAND_BUFFER_SIZE - 1U))
    {
        context->commandBuffer[context->length] = (char)data;
        context->length++;
        context->commandBuffer[context->length] = '\0';
    }
    else
    {
        context->overflow = 1U;
    }
}

static void CLI_Execute(CliContext_t *context)
{
    static const char helpText[] =
        "Commands:\r\n"
        "  help\r\n"
        "  status\r\n"
        "  sensor\r\n"
        "  version\r\n"
        "  reboot\r\n";
    static const char versionText[] = "F407_IndustrialMonitor V0.1.1\r\n";
    static const char unknownText[] = "Unknown command. Type 'help'.\r\n";
    static const char tooLongText[] = "Command too long.\r\n";
    static const char rebootText[] = "Rebooting...\r\n";

    if (context->overflow != 0U)
    {
        CLI_SendText(tooLongText, (uint16_t)(sizeof(tooLongText) - 1U));
        return;
    }

    if (strcmp(context->commandBuffer, "help") == 0)
    {
        CLI_SendText(helpText, (uint16_t)(sizeof(helpText) - 1U));
    }
    else if (strcmp(context->commandBuffer, "status") == 0)
    {
        CLI_SendStatus();
    }
    else if (strcmp(context->commandBuffer, "sensor") == 0)
    {
        CLI_SendSensor();
    }
    else if (strcmp(context->commandBuffer, "version") == 0)
    {
        CLI_SendText(versionText, (uint16_t)(sizeof(versionText) - 1U));
    }
    else if (strcmp(context->commandBuffer, "reboot") == 0)
    {
        CLI_SendText(rebootText, (uint16_t)(sizeof(rebootText) - 1U));

        /* osDelay is called here only from CommTask, never from an ISR. */
        /* Source: https://arm-software.github.io/CMSIS_6/latest/RTOS2/group__CMSIS__RTOS__Wait.html */
        osDelay(20U);
        NVIC_SystemReset();
    }
    else
    {
        CLI_SendText(unknownText, (uint16_t)(sizeof(unknownText) - 1U));
    }
}

static void CLI_SendText(const char *text, uint16_t length)
{
    if ((text != NULL) && (length != 0U))
    {
        (void)BSP_UART_Send((const uint8_t *)text, length);
    }
}

static void CLI_SendStatus(void)
{
    char output[CLI_OUTPUT_BUFFER_SIZE];
    int outputLength;

    outputLength = snprintf(output,
                            sizeof(output),
                            "System Status\r\n"
                            "Uptime : %lu ms\r\n"
                            "Sensor : OK\r\n"
                            "UART   : OK\r\n",
                            (unsigned long)HAL_GetTick());

    if ((outputLength > 0) && (outputLength < (int)sizeof(output)))
    {
        CLI_SendText(output, (uint16_t)outputLength);
    }
}

static void CLI_SendSensor(void)
{
    SensorData_t sensorData;
    char output[CLI_OUTPUT_BUFFER_SIZE];
    int outputLength;

    SensorManager_GetData(&sensorData);
    outputLength = snprintf(output,
                            sizeof(output),
                            "Raw     : %u\r\n"
                            "Voltage : %.3f V\r\n"
                            "Temp    : %.1f C\r\n"
                            "Tick    : %lu\r\n",
                            (unsigned int)sensorData.adcRaw,
                            (double)sensorData.voltage,
                            (double)sensorData.temperature,
                            (unsigned long)sensorData.timestamp);

    if ((outputLength > 0) && (outputLength < (int)sizeof(output)))
    {
        CLI_SendText(output, (uint16_t)outputLength);
    }
}
