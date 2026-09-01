#include "error_manager.h"

#include <string.h>

#include "main.h"

volatile ErrorManager_t g_errorManager;

void ErrorManager_Init(void)
{
    (void)memset((void *)&g_errorManager, 0, sizeof(g_errorManager));
    g_errorManager.lastError = ERROR_NONE;
}

void ErrorManager_Report(ErrorCode_t error)
{
    if ((error <= ERROR_NONE) || (error >= ERROR_COUNT))
    {
        return;
    }

    g_errorManager.lastError = error;
    g_errorManager.totalErrorCount++;
    g_errorManager.errorCount[error]++;
    g_errorManager.lastErrorTick = HAL_GetTick();
}

ErrorCode_t ErrorManager_GetLastError(void)
{
    return g_errorManager.lastError;
}
