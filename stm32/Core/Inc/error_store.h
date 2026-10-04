#ifndef INC_ERROR_STORE_H_
#define INC_ERROR_STORE_H_

#include "stm32f0xx_hal.h"
#include <stdint.h>

/* Страница ПЕРЕД страницей настроек - не пересекается с settings.c */
#define ERROR_STORE_FLASH_PAGE_ADDR   0x08007800UL
#define ERROR_STORE_MAGIC             0xE44057E0UL

typedef struct {
    uint32_t magic;
    uint32_t errorState;   // защёлкнутая маска ошибок, 0 = ошибок не было
    uint32_t crc;
} ErrorRecord;

/* Загружает защёлку из Flash. Если данные битые/пустые - *errorState = 0. */
void ErrorStore_Load(uint32_t *errorState);

/* Сохраняет errorState в Flash.
 * ВЫЗЫВАТЬ ТОЛЬКО на переходе 0->ошибка или при явном сбросе -
 * НЕ каждую итерацию main loop, ресурс Flash ограничен (~10-100к циклов). */
HAL_StatusTypeDef ErrorStore_Save(uint32_t errorState);

/* Явный сброс - записывает errorState = 0 */
HAL_StatusTypeDef ErrorStore_Clear(void);

#endif /* INC_ERROR_STORE_H_ */
