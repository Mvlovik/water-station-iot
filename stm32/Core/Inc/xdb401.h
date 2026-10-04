#ifndef INC_XDB401_H_
#define INC_XDB401_H_

#include "stm32f0xx_hal.h"
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#define XDB401_I2C_ADDR     (0x7F << 1)
#define XDB401_FULLSCALE    0.5f
//recovery должен срабатывать позже, чем помечаются данные невалидными
#define XDB401_FILTER_SIZE  5
#define XDB401_RECOVERY_THRESHOLD  8u

#define I2C_RECOVERY_STEP_MS      1u
#define I2C_RECOVERY_CLOCK_PULSES 9u

/* Не долбим шину каждую итерацию, если датчик не отвечает */
#define XDB401_RETRY_INTERVAL_MS   100u
/* Через сколько подряд неудач считаем датчик неисправным/отключённым */
#define XDB401_FAIL_THRESHOLD      5u
#define XDB401_MAX_STEP_BAR        2.0f
#define XDB401_PLAUSIBILITY_OVERRIDE  3u

typedef enum {
    XDB401_IDLE = 0,
    XDB401_WAITING_CONVERSION,
	XDB401_RECOVERING
} XDB401_State;
typedef enum {
    RECOVERY_PHASE_SETUP = 0,
    RECOVERY_PHASE_CLOCK_LOW,
    RECOVERY_PHASE_CLOCK_HIGH,
    RECOVERY_PHASE_STOP_SDA_LOW,
    RECOVERY_PHASE_STOP_SCL_HIGH,
    RECOVERY_PHASE_STOP_SDA_HIGH,
    RECOVERY_PHASE_RESTORE,
    RECOVERY_PHASE_DONE
} I2CRecoveryPhase;
typedef struct {
    I2C_HandleTypeDef *hi2c;

    float pressureHistory[XDB401_FILTER_SIZE];
    uint8_t historyIndex;
    uint8_t historyCount;

    XDB401_State state;
    uint32_t     convStartTick;
    uint32_t     lastAttemptTick;   // для бэкоффа между попытками при ошибках
    uint8_t      consecutiveFailures;
	uint8_t      implausibleStreak; // подряд отклонённых как "невероятные" чтений
	bool         hasValidReading;   // было ли хоть одно успешное чтение
	uint32_t     lastValidReadTick;
	uint8_t  recoveryPhase;
	uint8_t  recoveryClockCount;
	uint32_t recoveryStepTick;

    float lastPressureBar;
    float lastTemperatureC;
} XDB401_Handle;

void XDB401_Init(XDB401_Handle *dev, I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef XDB401_Poll(XDB401_Handle *dev);

/* true - было хотя бы одно успешное чтение и число подряд ошибок
   не превысило порог (т.е. датчику можно доверять сейчас) */
bool XDB401_IsDataValid(const XDB401_Handle *dev);
bool XDB401_IsStale(const XDB401_Handle *dev, uint32_t timeoutMs);
#endif /* INC_XDB401_H_ */
