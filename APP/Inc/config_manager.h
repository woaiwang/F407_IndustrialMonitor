#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdint.h>

#include "main.h"

#define CONFIG_MANAGER_SELF_TEST_ENABLE  0

#define CONFIG_MAGIC          0x43464731UL
#define CONFIG_VERSION        1U
#define CONFIG_FLASH_ADDRESS  0x00FFE000UL

typedef struct
{
    float temperatureHighLimit;
    float temperatureLowLimit;
    uint32_t samplePeriodMs;
    uint8_t sensorEnabled;
    uint8_t reserved[3];
} DeviceConfig_t;

typedef enum
{
    CONFIG_SAVE_ERROR_NONE = 0,
    CONFIG_SAVE_ERROR_INVALID_PARAM,
    CONFIG_SAVE_ERROR_ERASE,
    CONFIG_SAVE_ERROR_WRITE,
    CONFIG_SAVE_ERROR_READBACK,
    CONFIG_SAVE_ERROR_VERIFY
} ConfigSaveError_t;

typedef enum
{
    CONFIG_SAVE_STEP_NONE = 0,
    CONFIG_SAVE_STEP_VALIDATE = 1,
    CONFIG_SAVE_STEP_BUILD_RECORD = 2,
    CONFIG_SAVE_STEP_SECTOR_ERASE = 3,
    CONFIG_SAVE_STEP_FLASH_WRITE = 4,
    CONFIG_SAVE_STEP_READ_BACK = 5,
    CONFIG_SAVE_STEP_VERIFY = 6,
    CONFIG_SAVE_STEP_SUCCESS = 7
} ConfigSaveStep_t;

typedef struct
{
    uint32_t loadCount;
    uint32_t saveAttemptCount;
    uint32_t saveSuccessCount;
    uint32_t saveFailCount;
    uint32_t defaultCount;

    uint32_t lastSaveStep;
    uint32_t lastSaveStatus;
    uint32_t lastSaveError;

    uint32_t crcStored;
    uint32_t crcCalculated;

    uint32_t magicRead;
    uint16_t versionRead;

    uint8_t magicValid;
    uint8_t versionValid;
    uint8_t lengthValid;
    uint8_t crcValid;

    uint8_t loadPassed;
    uint8_t savePassed;
    uint8_t saveInProgress;
    uint8_t usingDefault;
} ConfigDebug_t;

void ConfigManager_Init(void);

const DeviceConfig_t *ConfigManager_Get(void);

HAL_StatusTypeDef ConfigManager_Save(const DeviceConfig_t *config);

HAL_StatusTypeDef ConfigManager_Load(void);

HAL_StatusTypeDef ConfigManager_ResetToDefault(void);

void ConfigManager_GetDefault(DeviceConfig_t *config);

void ConfigManager_SelfTest(void);

extern volatile ConfigDebug_t g_configDebug;

#endif /* CONFIG_MANAGER_H */
