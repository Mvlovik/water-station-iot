#include "current_sensor.h"
#include <math.h>

/* Окно накопления для RMS. 40 мс = 2 периода сети 50 Гц. */
#define ACS_RMS_WINDOW_MS 40
#define STICKING_SETTLE_MS   (ACS_RMS_WINDOW_MS * 2)

void ACS_Init(ACS_Handle* h, ADC_HandleTypeDef* hadc, uint32_t channel)
{
    h->hadc = hadc;
    h->channel = channel;

    h->vref = 3.3f;

    h->sensitivity = 0.066f;

    h->dividerK = 0.625f;

    // НОЛЬ НЕ ЗАДАЁМ ЖЁСТКО
    h->zeroOffset = 0.0f;

    h->sumSq = 0.0f;
    h->sampleCount = 0;
    h->windowStartTick = HAL_GetTick();
    h->currentRMS = 0.0f;
}

uint16_t ACS_ReadRaw(ACS_Handle* h)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    h->hadc->Instance->CHSELR = 0;

    sConfig.Channel = h->channel;
    sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
    sConfig.SamplingTime = ADC_SAMPLETIME_41CYCLES_5;

    HAL_ADC_ConfigChannel(h->hadc, &sConfig);

    HAL_ADC_Start(h->hadc);
    HAL_ADC_PollForConversion(h->hadc, HAL_MAX_DELAY);
    uint16_t value = HAL_ADC_GetValue(h->hadc);
    HAL_ADC_Stop(h->hadc);

    return value;
}

float ACS_ReadVoltage(ACS_Handle* h)
{
    uint16_t raw = ACS_ReadRaw(h);
    return ((float)raw / 4095.0f) * h->vref;
}
void ACS_Update(ACS_Handle* h)
{
    float v_adc = ACS_ReadVoltage(h);

    // УБРАНО: возврат к "v_sensor"
    float dv = v_adc - h->zeroOffset;

    // Переводим в ток:
    // делитель уменьшил сигнал → компенсируем
    float i_inst = (dv / h->dividerK) / h->sensitivity;

    h->sumSq += i_inst * i_inst;
    h->sampleCount++;

    uint32_t now = HAL_GetTick();

    if ((now - h->windowStartTick) >= ACS_RMS_WINDOW_MS)
    {
        if (h->sampleCount > 0)
            h->currentRMS = sqrtf(h->sumSq / h->sampleCount);
        else
            h->currentRMS = 0.0f;

        h->sumSq = 0.0f;
        h->sampleCount = 0;
        h->windowStartTick = now;
    }
}
float ACS_GetCurrent(ACS_Handle* h)
{
    return h->currentRMS;
}
float ACS_CalibrateZero(ACS_Handle* h, uint16_t samples)
{
    float sum = 0.0f;

    for (uint16_t i = 0; i < samples; i++)
    {
        sum += ACS_ReadVoltage(h);
        HAL_Delay(2);
    }

    h->zeroOffset = sum / samples;

    return h->zeroOffset;
}
