#include "sensor_manager.h"

#include <stddef.h>

#include "adc.h"

#define SENSOR_MANAGER_DMA_BUFFER_LENGTH  32U
#define SENSOR_MANAGER_VDDA_VOLTAGE        3.3f
#define SENSOR_MANAGER_ADC_MAX_VALUE       4095.0f
#define SENSOR_MANAGER_TEMPERATURE_V25     0.76f
#define SENSOR_MANAGER_TEMPERATURE_SLOPE   0.0025f

static volatile uint16_t s_adcDmaBuffer[SENSOR_MANAGER_DMA_BUFFER_LENGTH];

void SensorManager_Init(void)
{
    /* HAL uses uint32_t * for the ADC DMA destination API. */
    (void)HAL_ADC_Start_DMA(&hadc1,
                            (uint32_t *)s_adcDmaBuffer,
                            SENSOR_MANAGER_DMA_BUFFER_LENGTH);
}

void SensorManager_GetData(SensorData_t *data)
{
    uint32_t sum = 0U;
    uint32_t index;

    if (data == NULL)
    {
        return;
    }

    for (index = 0U; index < SENSOR_MANAGER_DMA_BUFFER_LENGTH; index++)
    {
        sum += s_adcDmaBuffer[index];
    }

    data->adcRaw = (uint16_t)(sum / SENSOR_MANAGER_DMA_BUFFER_LENGTH);
    data->voltage = ((float)data->adcRaw * SENSOR_MANAGER_VDDA_VOLTAGE) /
                    SENSOR_MANAGER_ADC_MAX_VALUE;

    /*
     * V25 and Avg_Slope are STM32F407 typical values. This internal
     * temperature estimate is for demonstration only, not precision measurement.
     * Source: https://www.st.com/resource/en/datasheet/stm32f407zg.pdf (Table 69).
     */
    data->temperature = ((data->voltage - SENSOR_MANAGER_TEMPERATURE_V25) /
                         SENSOR_MANAGER_TEMPERATURE_SLOPE) + 25.0f;
    data->timestamp = HAL_GetTick();
}
