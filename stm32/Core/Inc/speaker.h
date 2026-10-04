#ifndef INC_SPEAKER_H_
#define INC_SPEAKER_H_

#include "stm32f0xx_hal.h"
#include <stdbool.h>

typedef enum {
    SPEAKER_PATTERN_NONE = 0,
    SPEAKER_PATTERN_BEEP_ONCE,  // короткий одиночный сигнал (инициализация)
    SPEAKER_PATTERN_ALARM       // непрерывное периодическое пиканье (авария)
} Speaker_Pattern;

typedef struct {
    TIM_HandleTypeDef *htim;     // таймер, генерирующий тон (PWM)
    uint32_t            channel; // канал таймера, например TIM_CHANNEL_1

    Speaker_Pattern pattern;
    bool            toneOn;        // сейчас идёт фаза "звук" или "тишина"
    uint32_t        lastToggleMs;
    uint32_t        onMs;
    uint32_t        offMs;
    uint16_t        remainingOnPhases;
} Speaker_Handle;

void Speaker_Init(Speaker_Handle *spk, TIM_HandleTypeDef *htim, uint32_t channel);

void Speaker_BeepStartup(Speaker_Handle *spk);

void Speaker_StartAlarm(Speaker_Handle *spk);

void Speaker_Stop(Speaker_Handle *spk);

void Speaker_Update(Speaker_Handle *spk);

bool Speaker_IsActive(const Speaker_Handle *spk);

#endif /* INC_SPEAKER_H_ */
