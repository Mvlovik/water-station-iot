#include "settings.h"
#include "uart.h"
#include <string.h>

// ----------------------------------------------------------------
// Простой CRC32 без таблиц
// ----------------------------------------------------------------
static uint32_t _crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++) {
            crc = (crc >> 1) ^ (0xEDB88320UL & -(crc & 1));
        }
    }
    return ~crc;
}

static uint32_t _calcCrc(const Settings *s)
{
    // Считаем CRC по всем полям кроме самого crc
    return _crc32((const uint8_t *)s, offsetof(Settings, crc));
}

// ----------------------------------------------------------------
// Публичные функции
// ----------------------------------------------------------------
void Settings_SetDefaults(Settings *s)
{
    s->magic       = SETTINGS_MAGIC;
    s->pressureOn  = SETTINGS_DEFAULT_P_ON;
    s->pressureOff = SETTINGS_DEFAULT_P_OFF;
    s->pumpMaxTimeSession = SETTINGS_DEFAULT_PUMP_MAX_TIME_SESSION;
    s->crc         = _calcCrc(s);
}

uint8_t Settings_IsValid(const Settings *s)
{
    if (s->magic != SETTINGS_MAGIC)  return 0;
    if (s->crc   != _calcCrc(s))     return 0;
    if (s->pressureOn  <= 0.0f)      return 0;
    if (s->pressureOff <= 0.0f)      return 0;
    if (s->pumpMaxTimeSession <= 0)  return 0;
    if (s->pressureOn  >= s->pressureOff) return 0;

    // Дополнительные проверки на разумность
    if (s->pressureOn > 10.0f) return 0;      // Не более 10 бар
    if (s->pressureOff > 15.0f) return 0;     // Не более 15 бар
    if (s->pumpMaxTimeSession > 86400) return 0; // Не более 24 часов

    return 1;
}

HAL_StatusTypeDef Settings_Load(Settings *s)
{
    // Читаем прямо из Flash — она маппируется в адресное пространство
    memcpy(s, (const void *)SETTINGS_FLASH_PAGE_ADDR, sizeof(Settings));

    HAL_StatusTypeDef result = HAL_OK;

    if (!Settings_IsValid(s)) {
        Settings_SetDefaults(s);
        result = HAL_ERROR;   // Вернули дефолт — сигнализируем что было пусто
    }

    // ESP8266 должна знать текущие настройки сразу после загрузки —
    // неважно, валидные они были во Flash или подставлены дефолтные.
    // ВАЖНО: UART_Init() должен быть вызван ДО Settings_Load(), иначе
    // это сообщение будет молча потеряно (s_huart ещё не установлен).
    UART_sendSettings(s->pressureOn, s->pressureOff, s->pumpMaxTimeSession);

    return result;
}

HAL_StatusTypeDef Settings_Save(const Settings *s)
{
    HAL_StatusTypeDef status;

    // Пересчитываем CRC перед записью
    Settings tmp = *s;
    tmp.crc = _calcCrc(&tmp);

    // Разблокируем Flash
    status = HAL_FLASH_Unlock();
    if (status != HAL_OK) return status;

    // Стираем страницу
    FLASH_EraseInitTypeDef eraseInit = {
        .TypeErase   = FLASH_TYPEERASE_PAGES,
        .PageAddress = SETTINGS_FLASH_PAGE_ADDR,
        .NbPages     = 1
    };
    uint32_t pageError = 0;
    status = HAL_FLASHEx_Erase(&eraseInit, &pageError);
    if (status != HAL_OK) {
        HAL_FLASH_Lock();
        return status;
    }

    // Пишем по 4 байта (STM32F0 пишет halfword/word)
    const uint32_t *src  = (const uint32_t *)&tmp;
    uint32_t        addr = SETTINGS_FLASH_PAGE_ADDR;
    uint32_t        words = (sizeof(Settings) + 3) / 4;

    for (uint32_t i = 0; i < words; i++) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, src[i]);
        if (status != HAL_OK) {
            HAL_FLASH_Lock();
            return status;
        }
        addr += 4;
    }

    HAL_FLASH_Lock();

    // Настройки успешно сохранены — сразу сообщаем актуальные значения на ESP8266
    UART_sendSettings(tmp.pressureOn, tmp.pressureOff, tmp.pumpMaxTimeSession);

    return HAL_OK;
}
