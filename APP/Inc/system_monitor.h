#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <stdbool.h>
#include <stdint.h>

#include "error_manager.h"

#define SYSTEM_MONITOR_SENSOR_TIMEOUT_MS       3000U
#define SYSTEM_MONITOR_COMM_TIMEOUT_MS         2000U
#define SYSTEM_MONITOR_MONITOR_TIMEOUT_MS      2000U
#define SYSTEM_MONITOR_STARTUP_GRACE_MS        3000U

#define SYSTEM_MONITOR_FAULT_INJECTION_ENABLE  0

typedef enum
{
    SYSTEM_TASK_SENSOR = 0,
    SYSTEM_TASK_COMM,
    SYSTEM_TASK_MONITOR,
    SYSTEM_TASK_COUNT
} SystemTaskId_t;

typedef struct
{
    uint32_t sensorHeartbeatCount;
    uint32_t commHeartbeatCount;
    uint32_t monitorHeartbeatCount;

    uint8_t sensorAlive;
    uint8_t commAlive;
    uint8_t monitorAlive;

    uint8_t allHealthy;

    uint32_t watchdogRefreshCount;

    ErrorCode_t lastError;
    uint32_t totalErrorCount;

    uint8_t wasIwdgReset;
    uint8_t wasSoftwareReset;
    uint8_t wasPowerReset;
    uint8_t wasPinReset;
} SystemMonitorDebug_t;

void SystemMonitor_Init(void);

void SystemMonitor_Heartbeat(SystemTaskId_t taskId);

void SystemMonitor_Process(void);

bool SystemMonitor_AllCriticalTasksHealthy(void);

extern volatile SystemMonitorDebug_t g_systemMonitorDebug;

#endif /* SYSTEM_MONITOR_H */
