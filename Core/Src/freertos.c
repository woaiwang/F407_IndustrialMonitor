/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usart.h"
#include "sensor_manager.h"
#include "bsp_uart.h"
#include "cli.h"
#include "protocol.h"
#include <stdio.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for SensorTask */
osThreadId_t SensorTaskHandle;
const osThreadAttr_t SensorTask_attributes = {
  .name = "SensorTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for CommTask */
osThreadId_t CommTaskHandle;
const osThreadAttr_t CommTask_attributes = {
  .name = "CommTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for MonitorTask */
osThreadId_t MonitorTaskHandle;
const osThreadAttr_t MonitorTask_attributes = {
  .name = "MonitorTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for sensorQueue */
osMessageQueueId_t sensorQueueHandle;
const osMessageQueueAttr_t sensorQueue_attributes = {
  .name = "sensorQueue"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static HAL_StatusTypeDef CommTask_SendResponse(uint8_t command,
                                               const uint8_t *payload,
                                               uint8_t payloadLength);
static void CommTask_HandleFrame(const ProtocolFrame_t *frame);
static void CommTask_ProcessRxByte(uint8_t data,
                                   ProtocolParser_t *parser,
                                   ProtocolFrame_t *frame,
                                   CliContext_t *cli,
                                   uint8_t *binaryFrameActive);

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartSensorTask(void *argument);
void StartCommTask(void *argument);
void StartMonitorTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  SensorManager_Init();
  BSP_UART_Init();

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of sensorQueue */
  sensorQueueHandle = osMessageQueueNew (8, sizeof(SensorData_t), &sensorQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of SensorTask */
  SensorTaskHandle = osThreadNew(StartSensorTask, NULL, &SensorTask_attributes);

  /* creation of CommTask */
  CommTaskHandle = osThreadNew(StartCommTask, NULL, &CommTask_attributes);

  /* creation of MonitorTask */
  MonitorTaskHandle = osThreadNew(StartMonitorTask, NULL, &MonitorTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartSensorTask */
/**
* @brief Function implementing the SensorTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartSensorTask */
void StartSensorTask(void *argument)
{
  /* USER CODE BEGIN StartSensorTask */
  SensorData_t sensorData;

  /* Infinite loop */
  for(;;)
  {
    HAL_GPIO_TogglePin(LED_RED_GPIO_Port, LED_RED_Pin);

    SensorManager_GetData(&sensorData);

    (void)osMessageQueuePut(sensorQueueHandle, &sensorData, 0U, 0U);

    osDelay(1000);
  }
  /* USER CODE END StartSensorTask */
}

/* USER CODE BEGIN Header_StartCommTask */
/**
* @brief Function implementing the CommTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartCommTask */
void StartCommTask(void *argument)
{
  /* USER CODE BEGIN StartCommTask */
  ProtocolParser_t parser;
  ProtocolFrame_t frame;
  CliContext_t cli;
  SensorData_t queuedSensorData;
  uint8_t receivedData;
  uint8_t binaryFrameActive = 0U;

  Protocol_Init(&parser);
  CLI_Init(&cli);

  /* Infinite loop */
  for(;;)
  {
    BSP_UART_ProcessRx();

    while (osMessageQueueGet(sensorQueueHandle,
                             &queuedSensorData,
                             NULL,
                             0U) == osOK)
    {
      /* Keep the existing SensorTask queue from filling while UART owns CommTask. */
    }

    while (RingBuffer_Get(BSP_UART_GetRxRingBuffer(), &receivedData))
    {
      CommTask_ProcessRxByte(receivedData,
                             &parser,
                             &frame,
                             &cli,
                             &binaryFrameActive);
    }

    osDelay(5);
  }
  /* USER CODE END StartCommTask */
}

/* USER CODE BEGIN Header_StartMonitorTask */
/**
* @brief Function implementing the MonitorTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartMonitorTask */
void StartMonitorTask(void *argument)
{
  /* USER CODE BEGIN StartMonitorTask */
  /* Infinite loop */
   for(;;)
  {
    HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);

    osDelay(500);
  }

  /* USER CODE END StartMonitorTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

static HAL_StatusTypeDef CommTask_SendResponse(uint8_t command,
                                               const uint8_t *payload,
                                               uint8_t payloadLength)
{
  uint8_t txFrame[PROTOCOL_MAX_FRAME_LENGTH];
  uint16_t txLength;

  txLength = Protocol_BuildFrame(command,
                                 payload,
                                 payloadLength,
                                 txFrame,
                                 sizeof(txFrame));

  if (txLength == 0U)
  {
    return HAL_ERROR;
  }

  return BSP_UART_Send(txFrame, txLength);
}

static void CommTask_HandleFrame(const ProtocolFrame_t *frame)
{
  static const uint8_t version[] = "F407_IndustrialMonitor V0.1.0";
  static const uint8_t status[] = "STATUS:OK";
  static const uint8_t reboot[] = "REBOOTING";
  SensorData_t sensorData;
  uint8_t sensorPayload[PROTOCOL_MAX_PAYLOAD_LENGTH];
  int payloadLength;

  if (frame == NULL)
  {
    return;
  }

  switch ((ProtocolCommand_t)frame->command)
  {
    case CMD_GET_VERSION:
      (void)CommTask_SendResponse(frame->command,
                                  version,
                                  (uint8_t)(sizeof(version) - 1U));
      break;

    case CMD_GET_STATUS:
      (void)CommTask_SendResponse(frame->command,
                                  status,
                                  (uint8_t)(sizeof(status) - 1U));
      break;

    case CMD_GET_SENSOR:
      SensorManager_GetData(&sensorData);
      payloadLength = snprintf((char *)sensorPayload,
                               sizeof(sensorPayload),
                               "Raw=%u Voltage=%.3fV Temp=%.1fC Tick=%lu",
                               (unsigned int)sensorData.adcRaw,
                               (double)sensorData.voltage,
                               (double)sensorData.temperature,
                               (unsigned long)sensorData.timestamp);

      if ((payloadLength > 0) &&
          (payloadLength < (int)sizeof(sensorPayload)))
      {
        (void)CommTask_SendResponse(frame->command,
                                    sensorPayload,
                                    (uint8_t)payloadLength);
      }
      break;

    case CMD_REBOOT:
      if (CommTask_SendResponse(frame->command,
                                reboot,
                                (uint8_t)(sizeof(reboot) - 1U)) == HAL_OK)
      {
        osDelay(20);
        NVIC_SystemReset();
      }
      break;

    default:
      break;
  }
}

static void CommTask_ProcessRxByte(uint8_t data,
                                   ProtocolParser_t *parser,
                                   ProtocolFrame_t *frame,
                                   CliContext_t *cli,
                                   uint8_t *binaryFrameActive)
{
  ProtocolParseResult_t parseResult;

  if ((parser == NULL) || (frame == NULL) || (cli == NULL) ||
      (binaryFrameActive == NULL))
  {
    return;
  }

  if (*binaryFrameActive != 0U)
  {
    parseResult = Protocol_ProcessByte(parser, data, frame);

    if (parseResult == PROTOCOL_PARSE_FRAME_READY)
    {
      CommTask_HandleFrame(frame);
    }

    *binaryFrameActive = Protocol_IsReceiving(parser) ? 1U : 0U;
    return;
  }

  if (data == PROTOCOL_HEADER_BYTE_0)
  {
    (void)Protocol_ProcessByte(parser, data, frame);
    *binaryFrameActive = Protocol_IsReceiving(parser) ? 1U : 0U;
  }
  else if (((data >= 0x20U) && (data <= 0x7EU)) ||
           (data == '\r') || (data == '\n'))
  {
    CLI_ProcessByte(cli, data);
  }
}

/* USER CODE END Application */

