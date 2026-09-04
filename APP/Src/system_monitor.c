#include "system_monitor.h"

#include <string.h>

#include "iwdg.h"
#include "main.h"

typedef struct
{
    uint32_t lastHeartbeatTick;
    uint32_t timeoutMs;
    uint32_t heartbeatCount;
    uint8_t isAlive;
    uint8_t timeoutReported;
    uint8_t deadlineMissed;
} SystemMonitorTaskState_t;

volatile SystemMonitorDebug_t g_systemMonitorDebug;

static volatile SystemMonitorTaskState_t s_taskStates[SYSTEM_TASK_COUNT];
static uint32_t s_startupTick;
static uint8_t s_allCriticalTasksHealthy;

static ErrorCode_t SystemMonitor_GetTimeoutError(SystemTaskId_t taskId)
{
    switch (taskId)
    {
        case SYSTEM_TASK_SENSOR:
            return ERROR_SENSOR_TASK_TIMEOUT;

        case SYSTEM_TASK_COMM:
            return ERROR_COMM_TASK_TIMEOUT;

        case SYSTEM_TASK_MONITOR:
            return ERROR_MONITOR_TASK_TIMEOUT;

        default:
            return ERROR_NONE;
    }
}

static void SystemMonitor_UpdateDebug(void)
{
    g_systemMonitorDebug.sensorHeartbeatCount =
        s_taskStates[SYSTEM_TASK_SENSOR].heartbeatCount;
    g_systemMonitorDebug.commHeartbeatCount =
        s_taskStates[SYSTEM_TASK_COMM].heartbeatCount;
    g_systemMonitorDebug.monitorHeartbeatCount =
        s_taskStates[SYSTEM_TASK_MONITOR].heartbeatCount;

    g_systemMonitorDebug.sensorAlive = s_taskStates[SYSTEM_TASK_SENSOR].isAlive;
    g_systemMonitorDebug.commAlive = s_taskStates[SYSTEM_TASK_COMM].isAlive;
    g_systemMonitorDebug.monitorAlive = s_taskStates[SYSTEM_TASK_MONITOR].isAlive;
    g_systemMonitorDebug.allHealthy = s_allCriticalTasksHealthy;

    g_systemMonitorDebug.lastError = ErrorManager_GetLastError();
    g_systemMonitorDebug.totalErrorCount = g_errorManager.totalErrorCount;
}

static void SystemMonitor_EvaluateTask(SystemTaskId_t taskId, uint32_t now)
{
    volatile SystemMonitorTaskState_t *taskState = &s_taskStates[taskId];
    uint8_t alive;

    if ((taskState->deadlineMissed != 0U) ||
        (taskState->heartbeatCount == 0U) ||
        ((uint32_t)(now - taskState->lastHeartbeatTick) > taskState->timeoutMs))
    {
        alive = 0U;
    }
    else
    {
        alive = 1U;
    }

    taskState->isAlive = alive;

    if (alive != 0U)
    {
        taskState->timeoutReported = 0U;
    }
    else if (taskState->timeoutReported == 0U)
    {
        taskState->deadlineMissed = 1U;
        ErrorManager_Report(SystemMonitor_GetTimeoutError(taskId));
        taskState->timeoutReported = 1U;
    }
}

void SystemMonitor_Init(void)
{
    uint32_t now = HAL_GetTick();

    (void)memset((void *)s_taskStates, 0, sizeof(s_taskStates));
    (void)memset((void *)&g_systemMonitorDebug, 0, sizeof(g_systemMonitorDebug));

    g_systemMonitorDebug.wasIwdgReset =
        __HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) ? 1U : 0U;
    g_systemMonitorDebug.wasSoftwareReset =
        __HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) ? 1U : 0U;
    g_systemMonitorDebug.wasPowerReset =
        ((__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST) ||
          __HAL_RCC_GET_FLAG(RCC_FLAG_BORRST)) ? 1U : 0U);
    g_systemMonitorDebug.wasPinReset =
        __HAL_RCC_GET_FLAG(RCC_FLAG_PINRST) ? 1U : 0U;
    __HAL_RCC_CLEAR_RESET_FLAGS();

    s_startupTick = now;
    s_taskStates[SYSTEM_TASK_SENSOR].timeoutMs =
        SYSTEM_MONITOR_SENSOR_TIMEOUT_MS;
    s_taskStates[SYSTEM_TASK_COMM].timeoutMs =
        SYSTEM_MONITOR_COMM_TIMEOUT_MS;
    s_taskStates[SYSTEM_TASK_MONITOR].timeoutMs =
        SYSTEM_MONITOR_MONITOR_TIMEOUT_MS;

    s_taskStates[SYSTEM_TASK_SENSOR].isAlive = 1U;
    s_taskStates[SYSTEM_TASK_COMM].isAlive = 1U;
    s_taskStates[SYSTEM_TASK_MONITOR].isAlive = 1U;
    s_allCriticalTasksHealthy = 1U;

    SystemMonitor_UpdateDebug();
}

void SystemMonitor_Heartbeat(SystemTaskId_t taskId)
{
    volatile SystemMonitorTaskState_t *taskState;
    uint32_t now;

    if (taskId >= SYSTEM_TASK_COUNT)
    {
        return;
    }

    taskState = &s_taskStates[taskId];
    now = HAL_GetTick();

    if ((taskState->heartbeatCount != 0U) &&
        ((uint32_t)(now - taskState->lastHeartbeatTick) > taskState->timeoutMs))
    {
        taskState->deadlineMissed = 1U;
    }

    taskState->lastHeartbeatTick = now;
    taskState->heartbeatCount++;
    if (taskState->deadlineMissed == 0U)
    {
        taskState->isAlive = 1U;
        taskState->timeoutReported = 0U;
    }
}

void SystemMonitor_Process(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t withinStartupGrace =
        ((uint32_t)(now - s_startupTick) < SYSTEM_MONITOR_STARTUP_GRACE_MS) ?
        1U : 0U;

    if (withinStartupGrace != 0U)
    {
        s_taskStates[SYSTEM_TASK_SENSOR].isAlive = 1U;
        s_taskStates[SYSTEM_TASK_COMM].isAlive = 1U;
        s_taskStates[SYSTEM_TASK_MONITOR].isAlive = 1U;
        s_allCriticalTasksHealthy = 1U;
    }
    else
    {
        SystemMonitor_EvaluateTask(SYSTEM_TASK_SENSOR, now);
        SystemMonitor_EvaluateTask(SYSTEM_TASK_COMM, now);
        SystemMonitor_EvaluateTask(SYSTEM_TASK_MONITOR, now);

        s_allCriticalTasksHealthy =
            ((s_taskStates[SYSTEM_TASK_SENSOR].isAlive != 0U) &&
             (s_taskStates[SYSTEM_TASK_COMM].isAlive != 0U) &&
             (s_taskStates[SYSTEM_TASK_MONITOR].isAlive != 0U)) ? 1U : 0U;
    }

    /* IWDG may be intentionally uninitialized during UART debug verification. */
    if ((hiwdg.Instance == IWDG) && (s_allCriticalTasksHealthy != 0U))
    {
        if (HAL_IWDG_Refresh(&hiwdg) == HAL_OK)
        {
            g_systemMonitorDebug.watchdogRefreshCount++;
        }
    }

    SystemMonitor_UpdateDebug();
}

bool SystemMonitor_AllCriticalTasksHealthy(void)
{
    return (s_allCriticalTasksHealthy != 0U);
}
