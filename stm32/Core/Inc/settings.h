// settings.h
#ifndef INC_SETTINGS_H_
#define INC_SETTINGS_H_

#include "stm32f0xx_hal.h"
#include <stdint.h>

// Последняя страница Flash STM32F042K6 (32KB, страницы по 1KB)
#define SETTINGS_FLASH_PAGE_ADDR   0x08007C00UL
#define SETTINGS_FLASH_PAGE_SIZE   1024U

#define SETTINGS_MAGIC             0xDEADBEEFUL

// Значения по умолчанию (бар)
#define SETTINGS_DEFAULT_P_ON      2.0f
#define SETTINGS_DEFAULT_P_OFF     3.5f
#define SETTINGS_DEFAULT_PUMP_MAX_TIME_SESSION    600

typedef struct {
    uint32_t magic;        // Признак валидности
    float    pressureOn;   // Нижний порог — включить насос
    float    pressureOff;  // Верхний порог — выключить насос
    uint32_t pumpMaxTimeSession;  // Макс. время сессии в СЕКУНДАХ
    uint32_t crc;          // CRC32 для проверки целостности
} Settings;

HAL_StatusTypeDef Settings_Load(Settings *s);
HAL_StatusTypeDef Settings_Save(const Settings *s);
void              Settings_SetDefaults(Settings *s);
uint8_t           Settings_IsValid(const Settings *s);

#endif /* INC_SETTINGS_H_ */
