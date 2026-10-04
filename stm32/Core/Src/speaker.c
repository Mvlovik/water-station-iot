#include "speaker.h"

/* Включить/выключить сам тон (запуск-остановка ШИМ-канала таймера) */
static void _toneStart(Speaker_Handle *spk)
{
    spk->toneOn = true;
    HAL_TIM_PWM_Start(spk->htim, spk->channel);
}

static void _toneStop(Speaker_Handle *spk)
{
    spk->toneOn = false;
    HAL_TIM_PWM_Stop(spk->htim, spk->channel);
}

void Speaker_Init(Speaker_Handle *spk, TIM_HandleTypeDef *htim, uint32_t channel)
{
    spk->htim    = htim;
    spk->channel = channel;

    spk->pattern           = SPEAKER_PATTERN_NONE;
    spk->toneOn            = false;
    spk->lastToggleMs      = 0;
    spk->onMs              = 0;
    spk->offMs             = 0;
    spk->remainingOnPhases = 0;

    HAL_TIM_PWM_Stop(spk->htim, spk->channel);
}

void Speaker_BeepStartup(Speaker_Handle *spk)
{
    spk->pattern           = SPEAKER_PATTERN_BEEP_ONCE;
    spk->onMs              = 150;
    spk->offMs             = 0;      // после единственного "ON" сразу гасим и выходим из паттерна
    spk->remainingOnPhases = 1;      // одна фаза "пищим"

    spk->lastToggleMs = HAL_GetTick();
    _toneStart(spk);
}



void Speaker_StartAlarm(Speaker_Handle *spk)
{
    if (spk->pattern == SPEAKER_PATTERN_ALARM) return; // уже пищит - не перезапускаем фазу

    spk->pattern           = SPEAKER_PATTERN_ALARM;
    spk->onMs              = 200;
    spk->offMs             = 200;
    spk->remainingOnPhases = 0; // для ALARM не используется (бесконечно)

    spk->lastToggleMs = HAL_GetTick();
    _toneStart(spk);
}

void Speaker_Stop(Speaker_Handle *spk)
{
    if (spk->pattern != SPEAKER_PATTERN_BEEP_ONCE)
    	spk->pattern = SPEAKER_PATTERN_NONE;
    _toneStop(spk);
}

void Speaker_Update(Speaker_Handle *spk)
{
    if (spk->pattern == SPEAKER_PATTERN_NONE) return;

    uint32_t now       = HAL_GetTick();
    uint32_t elapsed    = now - spk->lastToggleMs;
    uint32_t threshold  = spk->toneOn ? spk->onMs : spk->offMs;

    if (elapsed < threshold) return;

    if (spk->toneOn)
    {
        /* Заканчиваем фазу "пищим" */
        _toneStop(spk);
        spk->lastToggleMs = now;

        if (spk->pattern != SPEAKER_PATTERN_ALARM)
        {
            if (spk->remainingOnPhases > 0) spk->remainingOnPhases--;
            if (spk->remainingOnPhases == 0)
            {
                spk->pattern = SPEAKER_PATTERN_NONE; // короткий сигнал закончен
            }
        }
        /* для ALARM просто уходим в паузу offMs, дальше снова "пищим" */
    }
    else
    {
        /* Пауза закончилась - снова пищим (актуально только для ALARM,
           т.к. BEEP_ONCE уже завершился выше и сюда не попадёт) */
        _toneStart(spk);
        spk->lastToggleMs = now;
    }
}

bool Speaker_IsActive(const Speaker_Handle *spk)
{
    return spk->pattern != SPEAKER_PATTERN_NONE;
}
