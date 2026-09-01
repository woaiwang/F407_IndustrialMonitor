#include "config_manager.h"

#include <stddef.h>
#include <string.h>

#include "bsp_w25q128.h"
#include "crc32.h"

#define CONFIG_MIN_SAMPLE_PERIOD_MS  100U
#define CONFIG_MAX_SAMPLE_PERIOD_MS  60000U

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t length;

    DeviceConfig_t config;

    uint32_t crc32;
} ConfigFlashRecord_t;

volatile ConfigDebug_t g_configDebug;

static DeviceConfig_t s_currentConfig;

static uint8_t ConfigManager_IsConfigValid(const DeviceConfig_t *config)
{
    if (config == NULL)
    {
        return 0U;
    }

    if (!(config->temperatureLowLimit < config->temperatureHighLimit))
    {
        return 0U;
    }

    if ((config->samplePeriodMs < CONFIG_MIN_SAMPLE_PERIOD_MS) ||
        (config->samplePeriodMs > CONFIG_MAX_SAMPLE_PERIOD_MS))
    {
        return 0U;
    }

    if (config->sensorEnabled > 1U)
    {
        return 0U;
    }

    return 1U;
}

static uint8_t ConfigManager_IsDefault(const DeviceConfig_t *config)
{
    if (config == NULL)
    {
        return 0U;
    }

    return ((config->temperatureHighLimit == 80.0f) &&
            (config->temperatureLowLimit == -20.0f) &&
            (config->samplePeriodMs == 1000U) &&
            (config->sensorEnabled == 1U)) ? 1U : 0U;
}

static uint32_t ConfigManager_CalculateRecordCrc(const ConfigFlashRecord_t *record)
{
    return CRC32_Calculate((const uint8_t *)record,
                           (uint32_t)offsetof(ConfigFlashRecord_t, crc32));
}

static void ConfigManager_ClearDebug(void)
{
    g_configDebug.loadCount = 0U;
    g_configDebug.saveAttemptCount = 0U;
    g_configDebug.saveSuccessCount = 0U;
    g_configDebug.saveFailCount = 0U;
    g_configDebug.defaultCount = 0U;
    g_configDebug.lastSaveStep = CONFIG_SAVE_STEP_NONE;
    g_configDebug.lastSaveStatus = HAL_OK;
    g_configDebug.lastSaveError = CONFIG_SAVE_ERROR_NONE;
    g_configDebug.crcStored = 0U;
    g_configDebug.crcCalculated = 0U;
    g_configDebug.magicRead = 0U;
    g_configDebug.versionRead = 0U;
    g_configDebug.magicValid = 0U;
    g_configDebug.versionValid = 0U;
    g_configDebug.lengthValid = 0U;
    g_configDebug.crcValid = 0U;
    g_configDebug.loadPassed = 0U;
    g_configDebug.savePassed = 0U;
    g_configDebug.saveInProgress = 0U;
    g_configDebug.usingDefault = 0U;
}

static HAL_StatusTypeDef ConfigManager_SaveFailed(ConfigSaveError_t error,
                                                  HAL_StatusTypeDef status)
{
    g_configDebug.saveFailCount++;
    g_configDebug.lastSaveError = (uint32_t)error;
    g_configDebug.lastSaveStatus = (uint32_t)status;
    g_configDebug.savePassed = 0U;
    g_configDebug.saveInProgress = 0U;

    return status;
}

static void ConfigManager_UpdateRecordDebug(const ConfigFlashRecord_t *record)
{
    g_configDebug.magicRead = record->magic;
    g_configDebug.versionRead = record->version;
    g_configDebug.crcStored = record->crc32;
}

static void ConfigManager_BuildRecord(const DeviceConfig_t *config,
                                      ConfigFlashRecord_t *record)
{
    (void)memset(record, 0, sizeof(*record));

    record->magic = CONFIG_MAGIC;
    record->version = CONFIG_VERSION;
    record->length = (uint16_t)sizeof(DeviceConfig_t);
    record->config = *config;
    record->crc32 = ConfigManager_CalculateRecordCrc(record);
}

void ConfigManager_GetDefault(DeviceConfig_t *config)
{
    if (config == NULL)
    {
        return;
    }

    (void)memset(config, 0, sizeof(*config));
    config->temperatureHighLimit = 80.0f;
    config->temperatureLowLimit = -20.0f;
    config->samplePeriodMs = 1000U;
    config->sensorEnabled = 1U;
}

void ConfigManager_Init(void)
{
    DeviceConfig_t defaultConfig;

    ConfigManager_ClearDebug();

    if (ConfigManager_Load() == HAL_OK)
    {
        return;
    }

    ConfigManager_GetDefault(&defaultConfig);
    if (ConfigManager_ResetToDefault() != HAL_OK)
    {
        /* Keep the system usable even if the default record cannot be persisted. */
        s_currentConfig = defaultConfig;
        g_configDebug.defaultCount++;
        g_configDebug.usingDefault = 1U;
    }
}

const DeviceConfig_t *ConfigManager_Get(void)
{
    return &s_currentConfig;
}

HAL_StatusTypeDef ConfigManager_Save(const DeviceConfig_t *config)
{
    ConfigFlashRecord_t writeRecord;
    ConfigFlashRecord_t readRecord;
    HAL_StatusTypeDef status;

    g_configDebug.saveAttemptCount++;
    g_configDebug.lastSaveStep = CONFIG_SAVE_STEP_VALIDATE;
    g_configDebug.lastSaveError = CONFIG_SAVE_ERROR_NONE;
    g_configDebug.saveInProgress = 1U;

    if (ConfigManager_IsConfigValid(config) == 0U)
    {
        return ConfigManager_SaveFailed(CONFIG_SAVE_ERROR_INVALID_PARAM,
                                        HAL_ERROR);
    }

    g_configDebug.lastSaveStep = CONFIG_SAVE_STEP_BUILD_RECORD;
    ConfigManager_BuildRecord(config, &writeRecord);

    g_configDebug.lastSaveStep = CONFIG_SAVE_STEP_SECTOR_ERASE;
    status = BSP_W25Q128_SectorErase(CONFIG_FLASH_ADDRESS);
    if (status != HAL_OK)
    {
        return ConfigManager_SaveFailed(CONFIG_SAVE_ERROR_ERASE, status);
    }

    g_configDebug.lastSaveStep = CONFIG_SAVE_STEP_FLASH_WRITE;
    status = BSP_W25Q128_Write(CONFIG_FLASH_ADDRESS,
                               (const uint8_t *)&writeRecord,
                               (uint32_t)sizeof(writeRecord));
    if (status != HAL_OK)
    {
        return ConfigManager_SaveFailed(CONFIG_SAVE_ERROR_WRITE, status);
    }

    g_configDebug.lastSaveStep = CONFIG_SAVE_STEP_READ_BACK;
    status = BSP_W25Q128_Read(CONFIG_FLASH_ADDRESS,
                               (uint8_t *)&readRecord,
                               (uint32_t)sizeof(readRecord));
    if (status != HAL_OK)
    {
        return ConfigManager_SaveFailed(CONFIG_SAVE_ERROR_READBACK, status);
    }

    g_configDebug.lastSaveStep = CONFIG_SAVE_STEP_VERIFY;
    ConfigManager_UpdateRecordDebug(&readRecord);
    g_configDebug.magicValid = (readRecord.magic == CONFIG_MAGIC) ? 1U : 0U;
    g_configDebug.versionValid = (readRecord.version == CONFIG_VERSION) ? 1U : 0U;
    g_configDebug.lengthValid =
        (readRecord.length == (uint16_t)sizeof(DeviceConfig_t)) ? 1U : 0U;
    g_configDebug.crcCalculated = ConfigManager_CalculateRecordCrc(&readRecord);
    g_configDebug.crcValid =
        (readRecord.crc32 == g_configDebug.crcCalculated) ? 1U : 0U;

    if ((memcmp(&writeRecord, &readRecord, sizeof(writeRecord)) != 0) ||
        (g_configDebug.crcValid == 0U))
    {
        return ConfigManager_SaveFailed(CONFIG_SAVE_ERROR_VERIFY, HAL_ERROR);
    }

    s_currentConfig = *config;
    g_configDebug.saveSuccessCount++;
    g_configDebug.lastSaveStep = CONFIG_SAVE_STEP_SUCCESS;
    g_configDebug.lastSaveStatus = HAL_OK;
    g_configDebug.lastSaveError = CONFIG_SAVE_ERROR_NONE;
    g_configDebug.savePassed = 1U;
    g_configDebug.saveInProgress = 0U;
    g_configDebug.usingDefault = ConfigManager_IsDefault(config);

    return HAL_OK;
}

HAL_StatusTypeDef ConfigManager_Load(void)
{
    ConfigFlashRecord_t record;
    HAL_StatusTypeDef status;

    g_configDebug.loadCount++;
    g_configDebug.magicValid = 0U;
    g_configDebug.versionValid = 0U;
    g_configDebug.lengthValid = 0U;
    g_configDebug.crcValid = 0U;
    g_configDebug.loadPassed = 0U;

    status = BSP_W25Q128_Read(CONFIG_FLASH_ADDRESS,
                              (uint8_t *)&record,
                              (uint32_t)sizeof(record));
    if (status != HAL_OK)
    {
        return status;
    }

    ConfigManager_UpdateRecordDebug(&record);

    if (record.magic != CONFIG_MAGIC)
    {
        return HAL_ERROR;
    }
    g_configDebug.magicValid = 1U;

    if (record.version != CONFIG_VERSION)
    {
        return HAL_ERROR;
    }
    g_configDebug.versionValid = 1U;

    if (record.length != (uint16_t)sizeof(DeviceConfig_t))
    {
        return HAL_ERROR;
    }
    g_configDebug.lengthValid = 1U;

    g_configDebug.crcCalculated = ConfigManager_CalculateRecordCrc(&record);
    if (record.crc32 != g_configDebug.crcCalculated)
    {
        return HAL_ERROR;
    }
    g_configDebug.crcValid = 1U;

    if (ConfigManager_IsConfigValid(&record.config) == 0U)
    {
        return HAL_ERROR;
    }

    s_currentConfig = record.config;
    g_configDebug.loadPassed = 1U;
    g_configDebug.usingDefault = ConfigManager_IsDefault(&record.config);

    return HAL_OK;
}

HAL_StatusTypeDef ConfigManager_ResetToDefault(void)
{
    DeviceConfig_t defaultConfig;
    HAL_StatusTypeDef status;

    ConfigManager_GetDefault(&defaultConfig);
    status = ConfigManager_Save(&defaultConfig);
    if (status == HAL_OK)
    {
        g_configDebug.defaultCount++;
        g_configDebug.usingDefault = 1U;
    }

    return status;
}

void ConfigManager_SelfTest(void)
{
    DeviceConfig_t testConfig;
    DeviceConfig_t expectedConfig;

    if (ConfigManager_ResetToDefault() != HAL_OK)
    {
        return;
    }

    testConfig = *ConfigManager_Get();
    testConfig.temperatureHighLimit = 66.5f;
    testConfig.temperatureLowLimit = 10.5f;
    testConfig.samplePeriodMs = 1500U;

    if (ConfigManager_Save(&testConfig) != HAL_OK)
    {
        return;
    }
    expectedConfig = testConfig;

    (void)memset(&s_currentConfig, 0, sizeof(s_currentConfig));

    if (ConfigManager_Load() != HAL_OK)
    {
        return;
    }

    if (memcmp(&s_currentConfig, &expectedConfig, sizeof(expectedConfig)) != 0)
    {
        return;
    }

    (void)ConfigManager_ResetToDefault();
}
