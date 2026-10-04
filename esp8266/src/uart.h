#pragma once
#include <Arduino.h>

/* ==== Коды команд протокола (совпадают со STM32) ==== */
#define CMD_TELEMETRY        0x01
#define CMD_EVENT_PUMP       0x02
#define CMD_SETTINGS         0x03
#define CMD_PUMP_ACTION      0x10
#define CMD_SET_PRESSURE     0x11

/* Через сколько мс без валидного кадра считаем связь потерянной */
#define UART_LINK_TIMEOUT_MS 2000

/* ==== Последние принятые данные ==== */
extern float g_pressure;
extern float g_current1;
extern float g_current2;
extern uint8_t g_error;

extern uint8_t g_pump1_cmd;
extern uint8_t g_pump1_actual;
extern uint32_t g_pump1_time;
extern float    g_temperature;   
extern uint8_t  g_aroundStatus;  

extern uint8_t g_pump2_cmd;
extern uint8_t g_pump2_actual;
extern uint32_t g_pump2_time;

extern float g_p_on;
extern float g_p_off;
extern uint32_t g_pumpMax;

void UART_Init(uint32_t baud);
void UART_Loop(void);

void UART_SendPumpAction(uint8_t action);
void UART_SendPressure(float pmin, float pmax);

/* true, если валидный кадр от STM32 приходил недавно (< UART_LINK_TIMEOUT_MS) */
bool UART_IsConnected(void);