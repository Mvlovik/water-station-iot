#ifndef INC_RELAY_CONTROL_H_
#define INC_RELAY_CONTROL_H_

#include "stm32f0xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

/* ─── Пины реле ─────────────────────────────────────────────────────────── */
#define RELAY_PUMP_PORT     GPIOB
#define RELAY_PUMP_PIN      GPIO_PIN_4

#define RELAY_WATERING_PORT GPIOB
#define RELAY_WATERING_PIN  GPIO_PIN_5

#define BTN_WATERING_PORT   GPIOA
#define BTN_WATERING_PIN    GPIO_PIN_8

/* ─── Счётчик времени насоса ─────────────────────────────────────────────── */
typedef struct {
    uint32_t totalSeconds;
    uint32_t currentSessionSeconds;
    uint32_t sessionStartMs;
    uint32_t startCount;
    bool     isActive;
} RelayTimer_Handle;

/* ─── Инициализация ──────────────────────────────────────────────────────── */
void RelayControl_Init(void);

/* ─── Пороги давления ────────────────────────────────────────────────────── */
void RelayControl_SetThresholds(float pressureOn, float pressureOff);

/* ─── Автоматика насоса ресивера ─────────────────────────────────────────── */
/* Обновляет только ЖЕЛАЕМОЕ (commanded) состояние по давлению.
 * Физическое переключение реле НЕ делает - для этого RelayControl_Tick(). */
void RelayControl_UpdatePump(float pressureBar);

/* Вызывать РАЗ ЗА ИТЕРАЦИЮ главного цикла, после RelayControl_UpdatePump()
 * и после обработки кнопок/UART-команд. Единственное место, которое реально
 * пишет в GPIO реле. Учитывает приоритет (ресивер важнее полива) и
 * минимальный интервал между переключениями (защита реле от дребезга). */
void RelayControl_Tick(void);

/* ─── Управление насосом ресивера (pump) — задаёт ТОЛЬКО желаемое состояние */
void RelayControl_PumpOn(void);
void RelayControl_PumpOff(void);
void RelayControl_TogglePump(void);

/* ─── Управление насосом полива (watering) — задаёт ТОЛЬКО желаемое состояние */
void RelayControl_WateringOn(void);
void RelayControl_WateringOff(void);
void RelayControl_ToggleWatering(void);
void RelayControl_SetWateringPriority(bool priority);
void RelayControl_ResetPriority(void);
bool RelayControl_IsWateringPriority(void);
/* ─── Состояние насосов ───────────м───────────────────────────────────────── */
/* Желаемое (commanded) состояние - "хочет ли работать" */
bool RelayControl_IsPumpActive(void);
bool RelayControl_IsWateringActive(void);

/* Реальное физическое состояние реле прямо сейчас (после анти-дребезга) */
bool RelayControl_IsPumpActuallyOn(void);
bool RelayControl_IsWateringActuallyOn(void);

/* ─── Время работы — насос ресивера (pump) ───────────────────────────────── */
uint32_t RelayControl_GetPumpTotalSeconds(void);
uint32_t RelayControl_GetPumpSessionSeconds(void);

/* ─── Время работы — насос полива (watering) ─────────────────────────────── */
uint32_t RelayControl_GetWateringTotalSeconds(void);
uint32_t RelayControl_GetWateringSessionSeconds(void);

/* ─── Сброс счётчиков ────────────────────────────────────────────────────── */
void RelayControl_ResetPumpCounters(void);
void RelayControl_ResetWateringCounters(void);

#endif /* INC_RELAY_CONTROL_H_ */
