
#include "xdb401.h"
#include <string.h>
#include "main.h"

#define XDB401_CONVERSION_MS 40

static float _convertPressure(uint32_t adcValue) {
    float norm;
    if (adcValue > 8388608UL) {
        norm = (adcValue - 16777216.0f) / 8388608.0f;
    } else {
        norm = (float)adcValue / 8388608.0f;
    }
    return norm * XDB401_FULLSCALE;  // MPa
}

static float _convertTemperature(uint16_t adcValue) {
    if (adcValue > 32768) {
        return (adcValue - 65536.0f) / 256.0f;
    }
    return (float)adcValue / 256.0f;
}

static bool _i2cBusRecoveryStep(XDB401_Handle *dev)
{
    uint32_t now = HAL_GetTick();

    /* SETUP и RESTORE выполняются мгновенно, остальные фазы
       требуют паузы I2C_RECOVERY_STEP_MS между переключениями пина */
    if (dev->recoveryPhase != RECOVERY_PHASE_SETUP &&
        dev->recoveryPhase != RECOVERY_PHASE_RESTORE &&
        (now - dev->recoveryStepTick) < I2C_RECOVERY_STEP_MS) {
        return false; /* ждём */
    }
    dev->recoveryStepTick = now;

    GPIO_InitTypeDef gpio = {0};

    switch (dev->recoveryPhase) {

    case RECOVERY_PHASE_SETUP:
        gpio.Mode  = GPIO_MODE_OUTPUT_OD;
        gpio.Pull  = GPIO_PULLUP;
        gpio.Speed = GPIO_SPEED_FREQ_LOW;
        gpio.Pin   = GPIO_PIN_6 | GPIO_PIN_7;
        HAL_GPIO_Init(GPIOB, &gpio);
        dev->recoveryClockCount = 0;
        dev->recoveryPhase = RECOVERY_PHASE_CLOCK_LOW;
        break;

    case RECOVERY_PHASE_CLOCK_LOW:
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); // SCL = 0
        dev->recoveryPhase = RECOVERY_PHASE_CLOCK_HIGH;
        break;

    case RECOVERY_PHASE_CLOCK_HIGH:
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);   // SCL = 1
        dev->recoveryClockCount++;
        dev->recoveryPhase = (dev->recoveryClockCount >= I2C_RECOVERY_CLOCK_PULSES)
                              ? RECOVERY_PHASE_STOP_SDA_LOW
                              : RECOVERY_PHASE_CLOCK_LOW;
        break;

    case RECOVERY_PHASE_STOP_SDA_LOW:
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET); // SDA = 0
        dev->recoveryPhase = RECOVERY_PHASE_STOP_SCL_HIGH;
        break;

    case RECOVERY_PHASE_STOP_SCL_HIGH:
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);   // SCL = 1
        dev->recoveryPhase = RECOVERY_PHASE_STOP_SDA_HIGH;
        break;

    case RECOVERY_PHASE_STOP_SDA_HIGH:
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);   // SDA = 1 (STOP)
        dev->recoveryPhase = RECOVERY_PHASE_RESTORE;
        break;

    case RECOVERY_PHASE_RESTORE:
        gpio.Mode      = GPIO_MODE_AF_OD;
        gpio.Pull      = GPIO_PULLUP;
        gpio.Speed     = GPIO_SPEED_FREQ_LOW;
        gpio.Alternate = GPIO_AF1_I2C1;
        gpio.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
        HAL_GPIO_Init(GPIOB, &gpio);

        __HAL_RCC_I2C1_FORCE_RESET();
        __HAL_RCC_I2C1_RELEASE_RESET();
        MX_I2C1_Init();

        dev->recoveryPhase = RECOVERY_PHASE_DONE;
        return true;

    default:
        dev->recoveryPhase = RECOVERY_PHASE_DONE;
        return true;
    }

    return false;
}
static float _applyFilter(XDB401_Handle *dev, float newValue) {
    dev->pressureHistory[dev->historyIndex] = newValue;
    dev->historyIndex = (dev->historyIndex + 1) % XDB401_FILTER_SIZE;

    if (dev->historyCount < XDB401_FILTER_SIZE) dev->historyCount++;

    float sum = 0;
    for (uint8_t i = 0; i < dev->historyCount; i++) {
        sum += dev->pressureHistory[i];
    }
    return sum / dev->historyCount;
}
void XDB401_Init(XDB401_Handle *dev, I2C_HandleTypeDef *hi2c) {
    dev->hi2c = hi2c;
    dev->historyIndex = 0;
    dev->historyCount = 0;
    memset(dev->pressureHistory, 0, sizeof(dev->pressureHistory));

    dev->state = XDB401_IDLE;
    dev->convStartTick = 0;
    dev->lastAttemptTick = 0;
    dev->consecutiveFailures = 0;
	dev->implausibleStreak = 0;
	dev->hasValidReading = false;


    dev->lastValidReadTick = HAL_GetTick();

    dev->lastPressureBar = 0.0f;
    dev->lastTemperatureC = 0.0f;
}
static HAL_StatusTypeDef _startConversion(XDB401_Handle *dev) {
    uint8_t cmd[2] = {0x30, 0x0A};
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(dev->hi2c, XDB401_I2C_ADDR, cmd, 2, 100);
    if (status != HAL_OK) {
        if (dev->consecutiveFailures < 255) dev->consecutiveFailures++;
        return status;
    }

    dev->state = XDB401_WAITING_CONVERSION;
    dev->convStartTick = HAL_GetTick();
    return HAL_BUSY;
}

static HAL_StatusTypeDef _finishConversion(XDB401_Handle *dev) {
    HAL_StatusTypeDef status;

    uint8_t reg = 0x06;
    uint8_t pressureData[3] = {0};
    status = HAL_I2C_Master_Transmit(dev->hi2c, XDB401_I2C_ADDR, &reg, 1, 100);
    if (status != HAL_OK) { dev->state = XDB401_IDLE; if (dev->consecutiveFailures < 255) dev->consecutiveFailures++; return status; }
    status = HAL_I2C_Master_Receive(dev->hi2c, XDB401_I2C_ADDR, pressureData, 3, 100);
    if (status != HAL_OK) { dev->state = XDB401_IDLE; if (dev->consecutiveFailures < 255) dev->consecutiveFailures++; return status; }

    /* Давление успешно прочитано - обрабатываем и защёлкиваем СРАЗУ,
       независимо от исхода чтения температуры дальше */
    uint32_t pressureRAW = ((uint32_t)pressureData[0] << 16)
                             | ((uint32_t)pressureData[1] << 8)
                             | pressureData[2];
	float pressureMPa    = _convertPressure(pressureRAW);
	float pressureBarRaw = pressureMPa * 10.0f;

	bool plausible = !dev->hasValidReading
				   || (fabsf(pressureBarRaw - dev->lastPressureBar) <= XDB401_MAX_STEP_BAR)
				   || (dev->implausibleStreak >= XDB401_PLAUSIBILITY_OVERRIDE);

	if (!plausible) {
		/* Похоже на помеху на линии, а не реальный скачок давления -
		   не пускаем в усредняющий фильтр, но I2C-транзакция сама
		   по себе была успешной, поэтому это не increments consecutiveFailures
		   и не трогает lastValidReadTick/hasValidReading */
		dev->implausibleStreak++;
		dev->state = XDB401_IDLE;
		return HAL_OK;
	}
	dev->implausibleStreak = 0;

	float filtered = _applyFilter(dev, pressureMPa);

	dev->lastPressureBar     = filtered * 10.0f;
	dev->consecutiveFailures = 0;
	dev->hasValidReading     = true;
	dev->lastValidReadTick   = HAL_GetTick();

    /* Температура - best-effort. Если не удалось прочитать,
       просто оставляем предыдущее значение lastTemperatureC,
       но НЕ считаем это провалом всей конверсии. */
    reg = 0x09;
    uint8_t tempData[2] = {0};
    status = HAL_I2C_Master_Transmit(dev->hi2c, XDB401_I2C_ADDR, &reg, 1, 100);
    if (status == HAL_OK) {
        status = HAL_I2C_Master_Receive(dev->hi2c, XDB401_I2C_ADDR, tempData, 2, 100);
    }
    if (status == HAL_OK) {
        uint16_t tempRAW = ((uint16_t)tempData[0] << 8) | tempData[1];
        dev->lastTemperatureC = _convertTemperature(tempRAW);
    }
    /* иначе - lastTemperatureC остаётся прежним, не трогаем */

    dev->state = XDB401_IDLE;
    return HAL_OK;
}

HAL_StatusTypeDef XDB401_Poll(XDB401_Handle *dev) {
    switch (dev->state) {
        case XDB401_IDLE: {
            if (dev->consecutiveFailures >= XDB401_RECOVERY_THRESHOLD) {
                dev->state             = XDB401_RECOVERING;
                dev->recoveryPhase     = RECOVERY_PHASE_SETUP;
                dev->recoveryClockCount = 0;
                dev->recoveryStepTick   = HAL_GetTick();
                return HAL_BUSY;
            }

            if (dev->consecutiveFailures > 0 &&
                (HAL_GetTick() - dev->lastAttemptTick) < XDB401_RETRY_INTERVAL_MS) {
                return HAL_BUSY;
            }
            dev->lastAttemptTick = HAL_GetTick();
            return _startConversion(dev);
        }

        case XDB401_RECOVERING:
            if (_i2cBusRecoveryStep(dev)) {
                dev->consecutiveFailures = XDB401_FAIL_THRESHOLD; // остаёмся invalid, пробуем заново
                dev->lastAttemptTick     = HAL_GetTick();
                dev->state               = XDB401_IDLE;
            }
            return HAL_BUSY;

        case XDB401_WAITING_CONVERSION:
            if ((HAL_GetTick() - dev->convStartTick) < XDB401_CONVERSION_MS) {
                return HAL_BUSY;
            }
            return _finishConversion(dev);

        default:
            dev->state = XDB401_IDLE;
            return HAL_ERROR;
    }
}
bool XDB401_IsStale(const XDB401_Handle *dev, uint32_t timeoutMs) {
    return (HAL_GetTick() - dev->lastValidReadTick) >= timeoutMs;
}
bool XDB401_IsDataValid(const XDB401_Handle *dev) {
    return dev->hasValidReading && (dev->consecutiveFailures < XDB401_FAIL_THRESHOLD);
}
