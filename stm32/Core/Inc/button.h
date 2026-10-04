#ifndef INC_BUTTON_H_
#define INC_BUTTON_H_

#include "stm32f0xx_hal.h"
#include <stdint.h>

#define BTN_DEBOUNCE_MS  50

typedef enum {
    BTN_EVENT_NONE = 0,
    BTN_EVENT_PRESS,        // Момент нажатия
    BTN_EVENT_RELEASE,      // Момент отпускания
    BTN_EVENT_HOLD,         // Удержание (каждый тик пока зажата)
} ButtonEvent;

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    uint32_t      debounceMs;

    uint8_t  lastRaw;        // Последнее сырое состояние
    uint8_t  stableState;    // Стабильное состояние после антидребезга
    uint32_t lastChangeTime; // Время последнего изменения
    uint8_t  pressed;        // Текущее состояние: 1 = нажата
} Button;

void        Button_Init(Button *btn, GPIO_TypeDef *port, uint16_t pin);
ButtonEvent Button_Update(Button *btn);  // Вызывать в главном цикле
uint8_t     Button_IsPressed(Button *btn);

#endif /* INC_BUTTON_H_ */
