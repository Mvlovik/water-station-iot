#include "error_store.h"
#include <string.h>
#include <stddef.h>
extern uint32_t _flash_user_start;

static inline void _checkFlashLayout(void) {
    if ((uint32_t)&_flash_user_start != ERROR_STORE_FLASH_PAGE_ADDR) {
        while(1) {};
    }
}

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

static uint32_t _calcCrc(const ErrorRecord *r)
{
    return _crc32((const uint8_t *)r, offsetof(ErrorRecord, crc));
}

static HAL_StatusTypeDef _writeRecord(uint32_t errorState)
{
    ErrorRecord rec;
    rec.magic      = ERROR_STORE_MAGIC;
    rec.errorState = errorState;
    rec.crc        = 0;
    rec.crc        = _calcCrc(&rec);

    __disable_irq();

	HAL_StatusTypeDef status = HAL_FLASH_Unlock();
	if (status != HAL_OK) { __enable_irq(); return status; }

	FLASH_EraseInitTypeDef eraseInit = {
        .TypeErase   = FLASH_TYPEERASE_PAGES,
        .PageAddress = ERROR_STORE_FLASH_PAGE_ADDR,
        .NbPages     = 1
    };
    uint32_t pageError = 0;
    status = HAL_FLASHEx_Erase(&eraseInit, &pageError);
    if (status != HAL_OK) { HAL_FLASH_Lock(); return status; }

    const uint32_t *src   = (const uint32_t *)&rec;
    uint32_t        addr  = ERROR_STORE_FLASH_PAGE_ADDR;
    uint32_t        words = (sizeof(ErrorRecord) + 3) / 4;

    for (uint32_t i = 0; i < words; i++) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, src[i]);
        if (status != HAL_OK) { HAL_FLASH_Lock(); return status; }
        addr += 4;
    }

    HAL_FLASH_Lock();
    __enable_irq();
    ErrorRecord verify;
    memcpy(&verify, (const void *)ERROR_STORE_FLASH_PAGE_ADDR, sizeof(ErrorRecord));
    if (verify.magic != rec.magic || verify.errorState != rec.errorState || verify.crc != rec.crc) {
        return HAL_ERROR;
    }

    return HAL_OK;
}

void ErrorStore_Load(uint32_t *errorState)
{
    ErrorRecord rec;
    memcpy(&rec, (const void *)ERROR_STORE_FLASH_PAGE_ADDR, sizeof(ErrorRecord));

    if (rec.magic == ERROR_STORE_MAGIC && rec.crc == _calcCrc(&rec)) {
        *errorState = rec.errorState;
    } else {
        *errorState = 0; // пустая/битая страница - считаем "ошибок не было"
    }
}

HAL_StatusTypeDef ErrorStore_Save(uint32_t errorState)
{
    return _writeRecord(errorState);
}

HAL_StatusTypeDef ErrorStore_Clear(void)
{
    return _writeRecord(0);
}
