#ifndef ERROR_MANAGER_H
#define ERROR_MANAGER_H

#include <stdint.h>

typedef enum
{
    ERROR_NONE = 0,
    ERROR_SENSOR_TASK_TIMEOUT,
    ERROR_COMM_TASK_TIMEOUT,
    ERROR_MONITOR_TASK_TIMEOUT,
    ERROR_FLASH_INIT,
    ERROR_CONFIG_LOAD,
    ERROR_UART,
    ERROR_COUNT
} ErrorCode_t;

typedef struct
{
    ErrorCode_t lastError;
    uint32_t totalErrorCount;
    uint32_t errorCount[ERROR_COUNT];
    uint32_t lastErrorTick;
} ErrorManager_t;

void ErrorManager_Init(void);

void ErrorManager_Report(ErrorCode_t error);

ErrorCode_t ErrorManager_GetLastError(void);

extern volatile ErrorManager_t g_errorManager;

#endif /* ERROR_MANAGER_H */
