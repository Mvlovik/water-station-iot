#include "relay_control.h"
#include "uart.h"

/* ─── ID насосов для событий по UART ─────────────────────────────────────── */
#define PUMP_ID_RECEIVER  1
#define PUMP_ID_WATERING  2

/* ─── Минимальный интервал между физическими переключениями реле ────────── */
#define RELAY_MIN_INTERVAL_MS  1000u

/* ─── Пороги давления ─────────────────────────────────────────────────────── */
static float s_pressureOn  = 0.03f;
static float s_pressureOff = 0.05f;

/* ─── Счётчики времени (отслеживают ЖЕЛАЕМОЕ/commanded состояние) ───────── */
static RelayTimer_Handle s_pumpTimer     = {0};
static RelayTimer_Handle s_wateringTimer = {0};

/* ─── Реальное физическое состояние реле + анти-дребезг ──────────────────── */
static bool     s_pumpPhysicalOn      = false;
static bool     s_wateringPhysicalOn  = false;
static uint32_t s_pumpLastToggleMs     = 0;
static uint32_t s_wateringLastToggleMs = 0;
static bool s_wateringPriority = false;

/* ─── Вспомогательные функции счётчика ───────────────────────────────────── */

static void Timer_Start(RelayTimer_Handle *t)
{
    if (t->isActive) return;
    t->isActive          = true;
    t->sessionStartMs    = HAL_GetTick();
    t->currentSessionSeconds = 0;
    t->startCount++;
}

static uint32_t Timer_Stop(RelayTimer_Handle *t)
{
    if (!t->isActive) return 0;
    uint32_t elapsed = (HAL_GetTick() - t->sessionStartMs) / 1000U;
    t->currentSessionSeconds = elapsed;
    t->totalSeconds         += elapsed;
    t->isActive              = false;
    return elapsed;
}

static uint32_t Timer_GetSessionSeconds(const RelayTimer_Handle *t)
{
    if (t->isActive)
        return (HAL_GetTick() - t->sessionStartMs) / 1000U;
    return t->currentSessionSeconds;
}

static uint32_t Timer_GetTotalSeconds(const RelayTimer_Handle *t)
{
    if (t->isActive)
        return t->totalSeconds + (HAL_GetTick() - t->sessionStartMs) / 1000U;
    return t->totalSeconds;
}

/* @brief  Прошло ли достаточно времени с последнего физического
 *         переключения этого реле (защита от дребезга/износа контактов). */
static bool CanToggle(uint32_t lastToggleMs)
{
    return (HAL_GetTick() - lastToggleMs) >= RELAY_MIN_INTERVAL_MS;
}
/* ─── Управление приоритетом ─────────────────────────────────────────────── */
void RelayControl_SetWateringPriority(bool priority)
{
    s_wateringPriority = priority;
}

bool RelayControl_IsWateringPriority(void)
{
    return s_wateringPriority;
}

void RelayControl_ResetPriority(void)
{
    s_wateringPriority = false;
    RelayControl_WateringOff();
}
/* ══════════════════════════════════════════════════════════════════════════════
 * Публичный API
 * ══════════════════════════════════════════════════════════════════════════════ */

void RelayControl_Init(void)
{
    HAL_GPIO_WritePin(RELAY_PUMP_PORT,     RELAY_PUMP_PIN,     GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RELAY_WATERING_PORT, RELAY_WATERING_PIN, GPIO_PIN_RESET);

    s_pumpTimer     = (RelayTimer_Handle){0};
    s_wateringTimer = (RelayTimer_Handle){0};

    s_pumpPhysicalOn      = false;
    s_wateringPhysicalOn  = false;
    s_pumpLastToggleMs     = 0;
    s_wateringLastToggleMs = 0;
    s_wateringPriority = false;
}

void RelayControl_SetThresholds(float pressureOn, float pressureOff)
{
    s_pressureOn  = pressureOn;
    s_pressureOff = pressureOff;
}

/* ─── Автоматика насоса ресивера: только ЖЕЛАЕМОЕ состояние, без GPIO ────── */
void RelayControl_UpdatePump(float pressureBar)
{
    if (!s_pumpTimer.isActive && pressureBar <= s_pressureOn)
    {
        Timer_Start(&s_pumpTimer);
        UART_sendEventPump(PUMP_ID_RECEIVER, true, s_pumpPhysicalOn, 0);
    }
    else if (s_pumpTimer.isActive && pressureBar >= s_pressureOff)
    {
        uint32_t duration = Timer_Stop(&s_pumpTimer);
        UART_sendEventPump(PUMP_ID_RECEIVER, false, s_pumpPhysicalOn, duration);
    }
}


void RelayControl_Tick(void)
{
    bool pumpWanted     = s_pumpTimer.isActive;
    bool wateringWanted = s_wateringTimer.isActive;

    bool pumpDesiredPhysical;
    bool wateringDesiredPhysical;

    if (s_wateringPriority)
    {
        wateringDesiredPhysical = wateringWanted;

        bool wateringBlocksPump = wateringWanted || s_wateringPhysicalOn;
        pumpDesiredPhysical = pumpWanted && !wateringBlocksPump;
    }
    else
    {
        pumpDesiredPhysical = pumpWanted;

        bool pumpBlocksWatering = pumpWanted || s_pumpPhysicalOn;
        wateringDesiredPhysical = wateringWanted && !pumpBlocksWatering;
    }

    /* Насос ресивера */
    if (pumpDesiredPhysical != s_pumpPhysicalOn && CanToggle(s_pumpLastToggleMs))
    {
        s_pumpPhysicalOn = pumpDesiredPhysical;
        HAL_GPIO_WritePin(RELAY_PUMP_PORT, RELAY_PUMP_PIN,
                           pumpDesiredPhysical ? GPIO_PIN_SET : GPIO_PIN_RESET);
        s_pumpLastToggleMs = HAL_GetTick();
        UART_sendEventPump(PUMP_ID_RECEIVER, pumpWanted, s_pumpPhysicalOn, 0);
    }

    /* Насос полива */
    if (wateringDesiredPhysical != s_wateringPhysicalOn && CanToggle(s_wateringLastToggleMs))
    {
        s_wateringPhysicalOn = wateringDesiredPhysical;
        HAL_GPIO_WritePin(RELAY_WATERING_PORT, RELAY_WATERING_PIN,
                           wateringDesiredPhysical ? GPIO_PIN_SET : GPIO_PIN_RESET);
        s_wateringLastToggleMs = HAL_GetTick();
        UART_sendEventPump(PUMP_ID_WATERING, wateringWanted, s_wateringPhysicalOn,
                            Timer_GetSessionSeconds(&s_wateringTimer));
    }
}
/* ─── Управление насосом ресивера: только ЖЕЛАЕМОЕ состояние ─────────────── */
void RelayControl_PumpOn(void)
{
    if (s_pumpTimer.isActive) return;
    Timer_Start(&s_pumpTimer);
    UART_sendEventPump(PUMP_ID_RECEIVER, true, s_pumpPhysicalOn, 0);
}

void RelayControl_PumpOff(void)
{
    if (!s_pumpTimer.isActive) return;
    uint32_t duration = Timer_Stop(&s_pumpTimer);
    UART_sendEventPump(PUMP_ID_RECEIVER, false, s_pumpPhysicalOn, duration);
}

void RelayControl_TogglePump(void)
{
    if (s_pumpTimer.isActive)
        RelayControl_PumpOff();
    else
        RelayControl_PumpOn();
}

/* ─── Управление насосом полива: только ЖЕЛАЕМОЕ состояние ───────────────── */
void RelayControl_WateringOn(void)
{
    if (s_wateringTimer.isActive) return;
    Timer_Start(&s_wateringTimer);
    UART_sendEventPump(PUMP_ID_WATERING, true, s_wateringPhysicalOn, 0);
}

void RelayControl_WateringOff(void)
{
    if (!s_wateringTimer.isActive) return;
    uint32_t duration = Timer_Stop(&s_wateringTimer);
    UART_sendEventPump(PUMP_ID_WATERING, false, s_wateringPhysicalOn, duration);
}

void RelayControl_ToggleWatering(void)
{
    if (s_wateringTimer.isActive)
        RelayControl_WateringOff();
    else
        RelayControl_WateringOn();
}

/* ─── Состояние насосов ──────────────────────────────────────────────────── */
bool RelayControl_IsPumpActive(void)     { return s_pumpTimer.isActive; }
bool RelayControl_IsWateringActive(void) { return s_wateringTimer.isActive; }

bool RelayControl_IsPumpActuallyOn(void)     { return s_pumpPhysicalOn; }
bool RelayControl_IsWateringActuallyOn(void) { return s_wateringPhysicalOn; }

/* ─── Время работы насоса ресивера ───────────────────────────────────────── */
uint32_t RelayControl_GetPumpTotalSeconds(void)   { return Timer_GetTotalSeconds(&s_pumpTimer); }
uint32_t RelayControl_GetPumpSessionSeconds(void) { return Timer_GetSessionSeconds(&s_pumpTimer); }

/* ─── Время работы насоса полива ─────────────────────────────────────────── */
uint32_t RelayControl_GetWateringTotalSeconds(void)   { return Timer_GetTotalSeconds(&s_wateringTimer); }
uint32_t RelayControl_GetWateringSessionSeconds(void) { return Timer_GetSessionSeconds(&s_wateringTimer); }

/* ─── Сброс счётчиков ────────────────────────────────────────────────────── */
void RelayControl_ResetPumpCounters(void)
{
    uint32_t startMs   = s_pumpTimer.sessionStartMs;
    bool     isActive  = s_pumpTimer.isActive;
    s_pumpTimer        = (RelayTimer_Handle){0};
    if (isActive) {
        s_pumpTimer.isActive       = true;
        s_pumpTimer.sessionStartMs = startMs;
    }
}

void RelayControl_ResetWateringCounters(void)
{
    uint32_t startMs  = s_wateringTimer.sessionStartMs;
    bool     isActive = s_wateringTimer.isActive;
    s_wateringTimer   = (RelayTimer_Handle){0};
    if (isActive) {
        s_wateringTimer.isActive       = true;
        s_wateringTimer.sessionStartMs = startMs;
    }
}
