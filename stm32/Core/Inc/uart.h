#ifndef UART_H
#define UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f0xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ===================== Кодировка байта pumpAction ===================== */
#define PUMP_ACTION_NONE      0x00u
#define PUMP1_ON              0x01u
#define PUMP1_OFF              0x02u
#define PUMP2_ON              0x04u
#define PUMP2_OFF              0x08u

/* ===================== Инициализация ===================== */
void UART_Init(UART_HandleTypeDef *huart);

/* Вызывать РАЗ ЗА ИТЕРАЦИЮ главного цикла (например, сразу после Button_Update).
 * Следит за "зависшей" передачей (обрыв линии, ESP не читает и т.п.)
 * и принудительно сбрасывает флаг занятости по таймауту, чтобы
 * UART_sendFrame() не отказывался слать кадры вечно. */
void UART_Poll(void);

/* ===================== Отправка (STM32 -> ESP8266), НЕБЛОКИРУЮЩАЯ =====================
 * Все функции ниже используют HAL_UART_Transmit_IT и возвращаются немедленно.
 * Если предыдущая передача ещё не завершилась - новый кадр просто
 * не отправляется (тихо пропускается), это ожидаемо и безопасно:
 * телеметрия шлётся каждые 20 мс, потерянный кадр не критичен. */

void UART_sendTelemetry(float pressure, float current1A, float current2A, uint8_t errorState, float lastTempC, uint8_t status);
void UART_sendEventPump(uint8_t pumpId, bool commandedState, bool actualState, uint32_t durationSec);
void UART_sendSettings(float pressureOn, float pressureOff, uint32_t pumpMaxTimeSessionSec);


typedef enum {
    UART_CMD_NONE = 0,
    UART_CMD_SET_PRESSURE_RANGE,
    UART_CMD_PUMP_ACTION
} UART_CmdType;

bool UART_isAvailable(void);
UART_CmdType UART_read(float *newPressureMin, float *newPressureMax, uint8_t *pumpAction);

#ifdef __cplusplus
}
#endif

#endif /* UART_H */
