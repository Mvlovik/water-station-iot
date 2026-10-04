#ifndef INC_CURRENT_SENSOR_H_
#define INC_CURRENT_SENSOR_H_
#include <stdbool.h>
#include "stm32f0xx_hal.h"

typedef struct {
    ADC_HandleTypeDef* hadc;
    uint32_t channel;

    float vref;        // обычно 3.3
    float sensitivity; // В/А (например 0.066 для ACS712-30A)
    float zeroOffset;  // напряжение при 0А (после делителя!), середина синусоиды
    float dividerK;    // коэффициент делителя (например 0.625)

    // --- накопление RMS без блокировки ---
    float    sumSq;          // сумма квадратов мгновенных токов в текущем окне
    uint32_t sampleCount;    // сколько выборок набрано в текущем окне
    uint32_t windowStartTick;// HAL_GetTick() начала текущего окна
    float    currentRMS;     // последнее посчитанное действующее значение, А
} ACS_Handle;

void ACS_Init(ACS_Handle* h, ADC_HandleTypeDef* hadc, uint32_t channel);
uint16_t ACS_ReadRaw(ACS_Handle* h);
float ACS_ReadVoltage(ACS_Handle* h);

/* Вызывать каждую итерацию главного цикла. Берёт одну выборку АЦП,
   копит её в окно RMS. Не блокирует. */
void ACS_Update(ACS_Handle* h);

/* Возвращает последнее посчитанное RMS-значение тока (кэш, обновляется
   раз в ACS_RMS_WINDOW_MS внутри ACS_Update). Вызов мгновенный. */
float ACS_GetCurrent(ACS_Handle* h);

/* Калибровка нуля — вызывается один раз при старте, до входа в while(1).
   Внутри блокирующая (HAL_Delay), но это нормально, т.к. не в главном цикле. */
float ACS_CalibrateZero(ACS_Handle* h, uint16_t samples);
#endif
