/*
 * Bootloader HAL configuration.
 *
 * This file follows the CubeMX-generated Application configuration structure.
 * Keep Bootloader/Core/Inc first in the Keil C/C++ include path so this is the
 * only stm32f4xx_hal_conf.h selected by the Bootloader target.
 */

#ifndef __STM32F4xx_HAL_CONF_H
#define __STM32F4xx_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ########################## Module Selection ############################## */
#define HAL_MODULE_ENABLED

#define HAL_RCC_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED

/* ########################## HSE/HSI Values adaptation ##################### */
#if !defined(HSE_VALUE)
#define HSE_VALUE                       8000000U
#endif

#if !defined(HSE_STARTUP_TIMEOUT)
#define HSE_STARTUP_TIMEOUT             100U
#endif

#if !defined(HSI_VALUE)
#define HSI_VALUE                       ((uint32_t)16000000U)
#endif

#if !defined(LSI_VALUE)
#define LSI_VALUE                       32000U
#endif

#if !defined(LSE_VALUE)
#define LSE_VALUE                       32768U
#endif

#if !defined(LSE_STARTUP_TIMEOUT)
#define LSE_STARTUP_TIMEOUT             5000U
#endif

#if !defined(EXTERNAL_CLOCK_VALUE)
#define EXTERNAL_CLOCK_VALUE            12288000U
#endif

/* ########################### System Configuration ######################### */
#define VDD_VALUE                       3300U
#define TICK_INT_PRIORITY               15U
#define USE_RTOS                        0U
#define PREFETCH_ENABLE                 1U
#define INSTRUCTION_CACHE_ENABLE        1U
#define DATA_CACHE_ENABLE               1U

#define USE_HAL_UART_REGISTER_CALLBACKS 0U

/* ########################## Assert Selection ############################## */
/* #define USE_FULL_ASSERT    1U */

/* Includes ------------------------------------------------------------------ */
/* Keep the conditional module-header inclusion used by the Application
 * stm32f4xx_hal_conf.h. stm32f4xx_hal_rcc.h pulls in stm32f4xx_hal_def.h,
 * which defines HAL_StatusTypeDef before stm32f4xx_hal.h declares HAL APIs.
 */
#ifdef HAL_RCC_MODULE_ENABLED
#include "stm32f4xx_hal_rcc.h"
#endif

#ifdef HAL_GPIO_MODULE_ENABLED
#include "stm32f4xx_hal_gpio.h"
#endif

#ifdef HAL_DMA_MODULE_ENABLED
#include "stm32f4xx_hal_dma.h"
#endif

#ifdef HAL_CORTEX_MODULE_ENABLED
#include "stm32f4xx_hal_cortex.h"
#endif

#ifdef HAL_FLASH_MODULE_ENABLED
#include "stm32f4xx_hal_flash.h"
#endif

#ifdef HAL_PWR_MODULE_ENABLED
#include "stm32f4xx_hal_pwr.h"
#endif

#ifdef HAL_UART_MODULE_ENABLED
#include "stm32f4xx_hal_uart.h"
#endif

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line);
#define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
#else
#define assert_param(expr) ((void)0U)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __STM32F4xx_HAL_CONF_H */
