#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <stdint.h>

typedef struct
{
    uint16_t adcRaw;
    float voltage;
    float temperature;
    uint32_t timestamp;
} SensorData_t;

void SensorManager_Init(void);
void SensorManager_GetData(SensorData_t *data);

#endif /* SENSOR_MANAGER_H */
