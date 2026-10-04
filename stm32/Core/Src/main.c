#include "main.h"
#include "xdb401.h"
#include "relay_control.h"
#include "button.h"
#include <stdbool.h>
#include <stdint.h>
#include "settings.h"
#include "current_sensor.h"
#include "uart.h"
#include "speaker.h"
//#include "error_store.h"
IWDG_HandleTypeDef hiwdg;

#define BTN_LONGPRESS_MS   3000u
#define BTN_RESET_HOLD_MS  7000u
#define OVERCURRENT_DEBOUNCE_MS   3000u
#define LOWCURRENT_DEBOUNCE_MS    3000u
#define XDB401_STALE_TIMEOUT_MS   10000u
#define STICKING_SETTLE_MS   400u
#define STARTUP_SETTLE_MS   2000u

#define CURRENT_PUMP_OVER 7.0f
#define CURRENT_PUMP_LOW 2.0f
#define CURRENT_WATERING_OVER 8.0f
#define CURRENT_WATERING_LOW 2.5f
#define CURRENT_PUMP_STICKING 1.5f
#define CURRENT_WATERING_STICKING 1.5f

const uint32_t PUMP_TIMEOUT_SECONDS = 10*60;


typedef enum {
    IDLE = 0,
    CRITICAL_PUMP_TIMEOUT   = 1 << 0,
    ERROR_PRESSURE_SENSOR   = 1 << 1,
    CURRENT_PUMP  	        = 1 << 2,
    CURRENT_WATERING        = 1 << 3,
    ERROR_CURRENT_SENSOR    = 1 << 4,
    STICKING                = 1 << 5,
    CURRENT2                = 1 << 6,
    CURRENT3                = 1 << 7,
} StationErrorState;

float currentPump = 0.0f;
float currentWatering = 0.0f;
float lastPressureBar = 0.0f;
float lastPressureBarPoll = 0.0f;
float lastTempC = 0.0f;

static uint32_t s_pumpOverTick          = 0;
static uint32_t s_pumpLowTick           = 0;
static uint32_t s_wateringOverTick      = 0;
static uint32_t s_wateringLowTick       = 0;

static bool     s_pumpWasActuallyOn     = false;
static uint32_t s_pumpOffTick           = 0;
static uint32_t s_wateringOnTick        = 0;
static bool     s_wateringWasActuallyOn = false;
static uint32_t s_wateringOffTick       = 0;
static uint32_t s_pumpOnTick            = 0;
bool wateringIsActive = false;
bool pupmIsActive = false;
volatile uint32_t systemTick            = 0;
volatile uint32_t lastTelemetrySend     = 0;
volatile uint32_t lastLedBlink          = 0;
const uint32_t TELEMETRY_INTERVAL_MS    = 60;
void MX_IWDG_Init(void);


static uint32_t s_btnPressStartMs = 0;
static bool s_resetFired    = false;

bool errorStatus = false;
uint8_t errorState = 0;
static bool s_wasErrorFree = true;

Button btnWatering2;  // PA9
Button btnWateringOutdoor;          // PA8
static uint32_t s_btnTogglePressStartMs = 0;   // время начала нажатия

I2C_HandleTypeDef hi2c1;
ADC_HandleTypeDef hadc;
TIM_HandleTypeDef htim2;
UART_HandleTypeDef huart2;
XDB401_Handle pressureSensor;
Speaker_Handle speaker;
Settings           settings;
TIM_HandleTypeDef htim1;
ACS_Handle acs1;
ACS_Handle acs2;
//IWDG_HandleTypeDef hiwdg;
ButtonEvent btnEvt;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);

static void MX_ADC_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM1_Speaker_Init(void);
static void MX_USART2_UART_Init(void);
static void I2C_BusReset(void);
static bool _debounce(bool conditionNow, uint32_t *sinceTick, uint32_t debounceMs);
void ledTick(void);
void calcError(void);
void telemetryTick(void);
void calcUART(void);
void buttonTick(void);

int main(void)
{
    HAL_Init();

    HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
    HAL_NVIC_SetPriority(USART2_IRQn, 2, 0);
    HAL_NVIC_SetPriority(TIM2_IRQn, 1, 0);


    MX_IWDG_Init();
    SystemClock_Config();
    __HAL_RCC_USART2_CLK_ENABLE();
    MX_GPIO_Init();

    MX_I2C1_Init();
    I2C_BusReset();
    MX_ADC_Init();
    MX_TIM2_Init();
    MX_TIM1_Speaker_Init();
    MX_USART2_UART_Init();

    HAL_NVIC_SetPriority(USART2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);

    UART_Init(&huart2);

    if (Settings_Load(&settings) != HAL_OK) Settings_Save(&settings);

    /*
    uint32_t storedError = 0;
	ErrorStore_Load(&storedError);
	errorState  = (uint8_t)storedError;
	errorStatus = (errorState != 0);
	s_wasErrorFree = !errorStatus;
	*/

    ACS_Init(&acs1, &hadc, ADC_CHANNEL_8);
    ACS_Init(&acs2, &hadc, ADC_CHANNEL_9);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4 | GPIO_PIN_5, GPIO_PIN_RESET);
    HAL_Delay(200);

    ACS_CalibrateZero(&acs1, 200);
    ACS_CalibrateZero(&acs2, 200);

    Button_Init(&btnWatering2, GPIOA, GPIO_PIN_9);
    Button_Init(&btnWateringOutdoor, GPIOA, GPIO_PIN_8);
    XDB401_Init(&pressureSensor, &hi2c1);

    RelayControl_Init();
    RelayControl_SetThresholds(settings.pressureOn, settings.pressureOff);

	Speaker_Init(&speaker, &htim1, TIM_CHANNEL_1);
	Speaker_BeepStartup(&speaker);
	if (errorStatus) Speaker_StartAlarm(&speaker);


    while (1) {
        UART_Poll();
        Speaker_Update(&speaker);
        ledTick();

        XDB401_Poll(&pressureSensor);
        ACS_Update(&acs1);
        ACS_Update(&acs2);

    	currentPump = ACS_GetCurrent(&acs1);
    	currentWatering = ACS_GetCurrent(&acs2);
    	pupmIsActive = RelayControl_IsPumpActive();
    	wateringIsActive = RelayControl_IsWateringActive();

        if (UART_isAvailable()) calcUART();

        buttonTick();

        lastPressureBar = pressureSensor.lastPressureBar;
        lastTempC = pressureSensor.lastTemperatureC;
        //if (!XDB401_IsStale(&pressureSensor, XDB401_STALE_TIMEOUT_MS)) {

		if (!errorStatus) {
			RelayControl_UpdatePump(lastPressureBar);

		} else if (RelayControl_IsPumpActive()) {
			RelayControl_PumpOff();
		}


        RelayControl_Tick();

        bool pumpActuallyOn = RelayControl_IsPumpActuallyOn();
        if (s_pumpWasActuallyOn && !pumpActuallyOn) {
            s_pumpOffTick = HAL_GetTick();
        }
        if (!s_pumpWasActuallyOn && pumpActuallyOn) {
            s_pumpOnTick = HAL_GetTick();
        }
        s_pumpWasActuallyOn = pumpActuallyOn;

        bool wateringActuallyOn = RelayControl_IsWateringActuallyOn();
        if (s_wateringWasActuallyOn && !wateringActuallyOn) {
            s_wateringOffTick = HAL_GetTick();
        }
        if (!s_wateringWasActuallyOn && wateringActuallyOn) {
            s_wateringOnTick = HAL_GetTick();
        }
        s_wateringWasActuallyOn = wateringActuallyOn;

		telemetryTick();

        if (HAL_GetTick() > 1000) calcError();

        HAL_IWDG_Refresh(&hiwdg);
    }
}

void buttonTick(void) {
    // ---- Обработка кнопки PA9 ----
    btnEvt = Button_Update(&btnWatering2);

    if (btnEvt == BTN_EVENT_PRESS) {
        s_btnPressStartMs = HAL_GetTick();
        s_resetFired      = false;
    }

    if (btnEvt == BTN_EVENT_HOLD) {
        uint32_t heldMs = HAL_GetTick() - s_btnPressStartMs;
        if (!s_resetFired && heldMs >= BTN_RESET_HOLD_MS) {
            s_resetFired = true;
            NVIC_SystemReset();
        }
    }

    if (btnEvt == BTN_EVENT_RELEASE) {
        uint32_t heldMs = HAL_GetTick() - s_btnPressStartMs;
        if (heldMs >= BTN_RESET_HOLD_MS) {
            // ничего не делаем (перезагрузка уже произошла)
        }
        else if (heldMs >= BTN_LONGPRESS_MS) {
            RelayControl_SetWateringPriority(true);
            RelayControl_WateringOn();
        }
        else {
            if (RelayControl_IsWateringActive()) {
                RelayControl_WateringOff();
                RelayControl_ResetPriority();
                Speaker_BeepStartup(&speaker);
            } else if (!errorStatus) {
                RelayControl_ResetPriority();
                RelayControl_WateringOn();
                Speaker_BeepStartup(&speaker);
            }
        }
        s_resetFired = false;
    }

    // ---- Обработка кнопки PA8 ----
    ButtonEvent evtToggle = Button_Update(&btnWateringToggle);

    if (evtToggle == BTN_EVENT_PRESS) {
        s_btnTogglePressStartMs = HAL_GetTick();
    }

    if (evtToggle == BTN_EVENT_RELEASE) {
        uint32_t heldMs = HAL_GetTick() - s_btnTogglePressStartMs;
        if (heldMs < BTN_LONGPRESS_MS) {   // короткое нажатие (< 3 с)
            if (RelayControl_IsWateringActive()) {
                RelayControl_WateringOff();
            } else {
                RelayControl_WateringOn();
            }
            Speaker_BeepStartup(&speaker);
        }

    }

}

void calcUART(void) {
	float new_pressureMin = 0.0f;
	float new_pressureMax = 0.0f;
	uint8_t pumpAction = 0;

	switch (UART_read(&new_pressureMin, &new_pressureMax, &pumpAction))
	{
	case UART_CMD_SET_PRESSURE_RANGE:
		settings.pressureOn  = new_pressureMin;
		settings.pressureOff = new_pressureMax;
		RelayControl_SetThresholds(settings.pressureOn, settings.pressureOff);
		Settings_Save(&settings);
		Speaker_BeepStartup(&speaker);
		break;

	case UART_CMD_PUMP_ACTION:
		if (pumpAction & PUMP1_ON)  RelayControl_PumpOn();
		if (pumpAction & PUMP1_OFF) RelayControl_PumpOff();
		if (pumpAction & PUMP2_ON)  RelayControl_WateringOn();
		if (pumpAction & PUMP2_OFF) RelayControl_WateringOff();
		Speaker_BeepStartup(&speaker);
		break;

	default:
		break;
	}
}

void telemetryTick(void) {
	uint32_t now = HAL_GetTick();
	if ((now - lastTelemetrySend) < TELEMETRY_INTERVAL_MS) return;
	lastTelemetrySend = now;

	uint8_t status = 0;

	if (RelayControl_IsPumpActuallyOn()) status |= (1 << 0);
	if (RelayControl_IsWateringActuallyOn()) status |= (1 << 1);
	if (RelayControl_IsPumpActive()) status |= (1 << 2);
	if (RelayControl_IsWateringActive()) status |= (1 << 3);
	if (RelayControl_IsWateringPriority()) status |= (1 << 4);
	if (btnEvt == BTN_EVENT_HOLD) status |= (1 << 5);

	UART_sendTelemetry(lastPressureBar, currentPump, currentWatering, errorState, lastTempC, status);
}

void ledTick(void) {
	uint32_t now = HAL_GetTick();
	if (errorStatus) {
		if ((now - lastLedBlink) >= 300) {
			lastLedBlink = now;
			HAL_GPIO_WritePin(
					GPIOB,
					GPIO_PIN_3,
					HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3) ? GPIO_PIN_RESET : GPIO_PIN_SET);
		}
		return;
	}

	if (wateringIsActive || pupmIsActive) HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_SET);
	else HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);
}

void calcError(void) {
    uint8_t liveErrors = 0;

    if (pupmIsActive && RelayControl_GetPumpSessionSeconds() > PUMP_TIMEOUT_SECONDS)
        liveErrors |= CRITICAL_PUMP_TIMEOUT;

    bool pumpOverNow = pupmIsActive && (currentPump > CURRENT_PUMP_OVER);
    if (_debounce(pumpOverNow, &s_pumpOverTick, OVERCURRENT_DEBOUNCE_MS))
        liveErrors |= CURRENT_PUMP;

    bool pumpLowNow = pupmIsActive
    		           && RelayControl_IsPumpActuallyOn()
                       && ((HAL_GetTick() - s_pumpOnTick) > STARTUP_SETTLE_MS)
                       && (currentPump < CURRENT_PUMP_LOW);
    if (_debounce(pumpLowNow, &s_pumpLowTick, LOWCURRENT_DEBOUNCE_MS))
        liveErrors |= CURRENT_PUMP;

    bool wateringLowNow = wateringIsActive
    		           && RelayControl_IsWateringActuallyOn()
                       && ((HAL_GetTick() - s_wateringOnTick) > STARTUP_SETTLE_MS)
                       && (currentWatering < CURRENT_WATERING_LOW);

    if (_debounce(wateringLowNow, &s_wateringLowTick, LOWCURRENT_DEBOUNCE_MS))
        liveErrors |= CURRENT_WATERING;
    bool wateringOverNow = wateringIsActive && (currentWatering > CURRENT_WATERING_OVER);
        if (_debounce(wateringOverNow, &s_wateringOverTick, OVERCURRENT_DEBOUNCE_MS))
            liveErrors |= CURRENT_WATERING;


    if (XDB401_IsStale(&pressureSensor, XDB401_STALE_TIMEOUT_MS)) {
        //liveErrors |= ERROR_PRESSURE_SENSOR;
    }

    static uint32_t s_pumpStuckTick = 0, s_wateringStuckTick = 0;

    bool pumpJustTurnedOff = (HAL_GetTick() - s_pumpOffTick) < STICKING_SETTLE_MS;
    bool pumpStuckNow = !pupmIsActive && !pumpJustTurnedOff && (currentPump > CURRENT_PUMP_STICKING);
    if (_debounce(pumpStuckNow, &s_pumpStuckTick, 1000u))
        liveErrors |= STICKING;

    bool wateringJustTurnedOff = (HAL_GetTick() - s_wateringOffTick) < STICKING_SETTLE_MS;
    bool wateringStuckNow = !wateringIsActive && !wateringJustTurnedOff && (currentWatering > CURRENT_WATERING_STICKING);
    if (_debounce(wateringStuckNow, &s_wateringStuckTick, 1000u))
        liveErrors |= STICKING;


    errorState |= liveErrors;
    errorStatus = (errorState != 0);

    if (errorStatus && s_wasErrorFree) {
        //ErrorStore_Save(errorState);
    }
    s_wasErrorFree = !errorStatus;

    if (errorStatus && !RelayControl_IsWateringPriority() && RelayControl_IsWateringActive()) {
        RelayControl_WateringOff();
    }

    if (errorStatus) {
        Speaker_StartAlarm(&speaker);
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    RCC_OscInitStruct.OscillatorType        = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_HSI14;
    RCC_OscInitStruct.HSIState              = RCC_HSI_ON;
    RCC_OscInitStruct.HSI14State            = RCC_HSI14_ON;
    RCC_OscInitStruct.HSICalibrationValue   = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.HSI14CalibrationValue = 16;
    RCC_OscInitStruct.PLL.PLLState          = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);

    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
    PeriphClkInit.I2c1ClockSelection   = RCC_I2C1CLKSOURCE_HSI;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4 | GPIO_PIN_5, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin   = GPIO_PIN_4 | GPIO_PIN_5;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin  = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);


    GPIO_InitStruct.Pin       = GPIO_PIN_8;
    GPIO_InitStruct.Mode      = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;     // подтяжка к питанию (кнопка на землю)
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);


    GPIO_InitStruct.Pin  = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF1_I2C1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin  = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin       = GPIO_PIN_2;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF1_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin   = GPIO_PIN_3;
	GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull  = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

static void MX_ADC_Init(void)
{
    hadc.Instance                   = ADC1;
    hadc.Init.ClockPrescaler        = ADC_CLOCK_ASYNC_DIV1;
    hadc.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc.Init.ScanConvMode          = ADC_SCAN_DIRECTION_FORWARD;
    hadc.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    hadc.Init.LowPowerAutoWait      = DISABLE;
    hadc.Init.LowPowerAutoPowerOff  = DISABLE;
    hadc.Init.ContinuousConvMode    = DISABLE;
    hadc.Init.DiscontinuousConvMode = DISABLE;
    hadc.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc.Init.DMAContinuousRequests = DISABLE;
    hadc.Init.Overrun               = ADC_OVR_DATA_PRESERVED;
    HAL_ADC_Init(&hadc);
}

void MX_I2C1_Init(void)
{
    hi2c1.Instance              = I2C1;
    hi2c1.Init.Timing           = 0x00201D2B;
    hi2c1.Init.OwnAddress1      = 0;
    hi2c1.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2      = 0;
    hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c1.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c1);
    HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE);
    HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0);
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 19200;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();
}

static void MX_TIM2_Init(void)
{
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 8000 - 1;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 1000 - 1;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_Base_Init(&htim2);

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig);

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig);

    HAL_NVIC_SetPriority(TIM2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);

    HAL_TIM_Base_Start_IT(&htim2);
}
static void MX_TIM1_Speaker_Init(void)
{
    __HAL_RCC_TIM1_CLK_ENABLE();

    TIM_OC_InitTypeDef sConfigOC = {0};

    htim1.Instance               = TIM1;
    htim1.Init.Prescaler         = 0;
    htim1.Init.CounterMode       = TIM_COUNTERMODE_UP;
    // SystemCoreClock (HSI=8МГц, как в вашем SystemClock_Config) / 2700Гц - 1
    htim1.Init.Period            = (SystemCoreClock / 2700) - 1;
    htim1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_PWM_Init(&htim1) != HAL_OK) Error_Handler();

    sConfigOC.OCMode       = TIM_OCMODE_PWM1;
    sConfigOC.Pulse        = htim1.Init.Period / 2; // скважность 50%
    sConfigOC.OCPolarity   = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode   = TIM_OCFAST_DISABLE;
    sConfigOC.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCIdleState  = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) Error_Handler();

    /* TIM1 - advanced-control таймер, для вывода на пин обязательно
       нужно включить главный выход (Main Output Enable) */
    HAL_TIMEx_MasterConfigSynchronization(&htim1, &(TIM_MasterConfigTypeDef){
        .MasterOutputTrigger = TIM_TRGO_RESET,
        .MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE
    });
    __HAL_TIM_MOE_ENABLE(&htim1);
}
void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

void TIM2_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim2);
}

void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        systemTick++;
    }
}

static void I2C_BusReset(void)
{
    GPIO_InitTypeDef gpio = {0};
    // Переключить пины I2C в режим Output Open-Drain
    gpio.Mode  = GPIO_MODE_OUTPUT_OD;
    gpio.Pull  = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin   = GPIO_PIN_6 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOB, &gpio);

    // 9 тактов на SCL, чтобы освободить SDA
    for (int i = 0; i < 9; i++) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); // SCL = 0
        HAL_Delay(1);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);   // SCL = 1
        HAL_Delay(1);
    }
    // Стоп-условие: SDA = 0, SCL = 1, затем SDA = 1
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);

    // Вернуть пины в режим AF_OD для I2C
    gpio.Mode      = GPIO_MODE_AF_OD;
    gpio.Alternate = GPIO_AF1_I2C1;
    HAL_GPIO_Init(GPIOB, &gpio);
}
static bool _debounce(bool conditionNow, uint32_t *sinceTick, uint32_t debounceMs)
{
    if (!conditionNow) {
        *sinceTick = 0;
        return false;
    }
    if (*sinceTick == 0) {
        *sinceTick = HAL_GetTick();
        return false;
    }
    return (HAL_GetTick() - *sinceTick) >= debounceMs;
}

void MX_IWDG_Init(void) {
    hiwdg.Instance       = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_64;
    hiwdg.Init.Reload    = 1249;                  // ~2 сек при LSI 40кГц
    hiwdg.Init.Window    = IWDG_WINDOW_DISABLE;    // явно отключить windowed-режим
    if (HAL_IWDG_Init(&hiwdg) != HAL_OK) {
        Error_Handler();
    }
}
