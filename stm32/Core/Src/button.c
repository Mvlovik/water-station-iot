#include "button.h"

void Button_Init(Button *btn, GPIO_TypeDef *port, uint16_t pin)
{
    btn->port           = port;
    btn->pin            = pin;
    btn->debounceMs     = BTN_DEBOUNCE_MS;
    btn->lastChangeTime = 0;
    btn->pressed        = 0;

    // Читаем начальное состояние
    btn->lastRaw     = HAL_GPIO_ReadPin(port, pin);
    btn->stableState = btn->lastRaw;
}

ButtonEvent Button_Update(Button *btn)
{
    uint8_t raw = HAL_GPIO_ReadPin(btn->port, btn->pin);

    // Зафиксировали изменение — сбрасываем таймер
    if (raw != btn->lastRaw) {
        btn->lastChangeTime = HAL_GetTick();
        btn->lastRaw = raw;
    }

    // Состояние стабильно дольше debounceMs
    if ((HAL_GetTick() - btn->lastChangeTime) >= btn->debounceMs) {
        if (raw != btn->stableState) {
            btn->stableState = raw;

            // Подтяжка вверх: LOW = нажата, HIGH = отпущена
            if (raw == GPIO_PIN_RESET) {
                btn->pressed = 1;
                return BTN_EVENT_PRESS;
            } else {
                btn->pressed = 0;
                return BTN_EVENT_RELEASE;
            }
        }

        // Кнопка удерживается
        if (btn->pressed && raw == GPIO_PIN_RESET) {
            return BTN_EVENT_HOLD;
        }
    }

    return BTN_EVENT_NONE;
}

uint8_t Button_IsPressed(Button *btn)
{
    return btn->pressed;
}
