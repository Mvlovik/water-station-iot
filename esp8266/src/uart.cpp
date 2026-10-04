#include "uart.h"
#include <SoftwareSerial.h>
#include "websocket.h"

/* ==== пины ==== */
#define UART_RX_PIN 12
#define UART_TX_PIN 14

SoftwareSerial swSerial(UART_RX_PIN, UART_TX_PIN);

/* ==== протокол ==== */
#define SOF         0xAA
#define MAX_PAYLOAD 14

/* ==== глобальные данные (определения) ==== */
float    g_pressure     = 0;
float    g_current1     = 0;
float    g_current2     = 0;
uint8_t  g_error        = 0;
float    g_temperature  = 0;       // °C
uint8_t  g_aroundStatus = 0;       // битовое поле статуса

uint8_t  g_pump1_cmd    = 0;
uint8_t  g_pump1_actual = 0;
uint32_t g_pump1_time   = 0;

uint8_t  g_pump2_cmd    = 0;
uint8_t  g_pump2_actual = 0;
uint32_t g_pump2_time   = 0;

float    g_p_on         = 0;
float    g_p_off        = 0;
uint32_t g_pumpMax      = 0;

/* ==== связь ==== */
static uint32_t s_lastRxMillis = 0;   /* время последнего ВАЛИДНОГО кадра (CRC ok) */

bool UART_IsConnected(void)
{
    return (millis() - s_lastRxMillis) < UART_LINK_TIMEOUT_MS;
}

/* ==== CRC8 (Dallas/Maxim, poly 0x31) ==== */
static uint8_t crc8_update(uint8_t crc, uint8_t data)
{
    crc ^= data;
    for (uint8_t i = 0; i < 8; i++)
        crc = (crc & 0x80) ? ((crc << 1) ^ 0x31) : (crc << 1);
    return crc;
}

static uint8_t crc8_calc(uint8_t cmd, uint8_t len, uint8_t *p)
{
    uint8_t crc = 0;
    crc = crc8_update(crc, len);
    crc = crc8_update(crc, cmd);
    for (uint8_t i = 0; i < len; i++)
        crc = crc8_update(crc, p[i]);
    return crc;
}

/* ==== парсер ==== */
enum {
    WAIT_SOF,
    WAIT_LEN,
    WAIT_CMD,
    WAIT_PAYLOAD,
    WAIT_CRC
};

static uint8_t state = WAIT_SOF;
static uint8_t len   = 0;
static uint8_t cmd   = 0;
static uint8_t buf[MAX_PAYLOAD];
static uint8_t idx   = 0;

/* ==== декодирование кадра ==== */
static void handleFrame(uint8_t cmd, uint8_t *p, uint8_t len)
{
    /* валидный кадр – обновляем "живость" связи */
    s_lastRxMillis = millis();

    String logMsg = "Получен кадр CMD=0x" + String(cmd, HEX) + " LEN=" + String(len);
    wsLogRx(logMsg);

    if (cmd == CMD_TELEMETRY && len == 14)
    {
        uint16_t pr = p[0] | (p[1] << 8);
        int16_t  c1 = p[2] | (p[3] << 8);
        int16_t  c2 = p[4] | (p[5] << 8);
        int16_t  temp = p[7] | (p[8] << 8);   // температура в сотых °C

        g_pressure = pr / 100.0f;
        g_current1 = c1 / 1000.0f;
        g_current2 = c2 / 1000.0f;
        g_error = p[6];
        g_temperature = temp / 100.0f;
        g_aroundStatus = p[9];                // битовое поле статуса

        // байты 10..13 содержат временную метку (игнорируем)

        wsLogRx("Телеметрия: P=" + String(g_pressure,2) +
                " I1=" + String(g_current1,3) +
                " I2=" + String(g_current2,3) +
                " Err=0x" + String(g_error, HEX) +
                " T=" + String(g_temperature,2) +
                " Status=0x" + String(g_aroundStatus, HEX));
    }
    else if (cmd == CMD_EVENT_PUMP && len == 7)
    {
        uint8_t id = p[0];
        uint8_t cmdState = p[1];
        uint8_t actState = p[2];
        uint32_t t = p[3] | (p[4]<<8) | (p[5]<<16) | (p[6]<<24);

        if (id == 1)
        {
            g_pump1_cmd = cmdState;
            g_pump1_actual = actState;
            g_pump1_time = t;
            wsLogRx("Событие насоса1: CMD=" + String(cmdState) + " ACT=" + String(actState) + " TIME=" + String(t));
        }
        else if (id == 2)
        {
            g_pump2_cmd = cmdState;
            g_pump2_actual = actState;
            g_pump2_time = t;
            wsLogRx("Событие насоса2: CMD=" + String(cmdState) + " ACT=" + String(actState) + " TIME=" + String(t));
        }
    }
    else if (cmd == CMD_SETTINGS && len == 8)
    {
        uint16_t pOn  = p[0] | (p[1] << 8);
        uint16_t pOff = p[2] | (p[3] << 8);
        uint32_t tMax = p[4] | (p[5]<<8) | (p[6]<<16) | (p[7]<<24);

        g_p_on = pOn / 100.0f;
        g_p_off = pOff / 100.0f;
        g_pumpMax = tMax;
        wsLogRx("Настройки: P_ON=" + String(g_p_on,2) + " P_OFF=" + String(g_p_off,2) + " T_MAX=" + String(tMax));
    }
    else
    {
        wsLogWarning("Неизвестная команда: 0x" + String(cmd, HEX));
    }
}

/* ==== обработка одного байта (конечный автомат) ==== */
static void parseByte(uint8_t b)
{
    switch (state)
    {
        case WAIT_SOF:
            if (b == SOF) {
                state = WAIT_LEN;
            }
            break;

        case WAIT_LEN:
            if (b > MAX_PAYLOAD)
            {
                state = WAIT_SOF;
                wsLogWarning("Некорректная длина кадра: " + String(b));
            }
            else
            {
                len = b;
                idx = 0;
                state = WAIT_CMD;
            }
            break;

        case WAIT_CMD:
            cmd = b;
            state = (len == 0) ? WAIT_CRC : WAIT_PAYLOAD;
            break;

        case WAIT_PAYLOAD:
            buf[idx++] = b;
            if (idx >= len)
                state = WAIT_CRC;
            break;

        case WAIT_CRC:
        {
            uint8_t crc = crc8_calc(cmd, len, buf);
            if (crc == b) {
                handleFrame(cmd, buf, len);
            } else {
                wsLogError("Ошибка CRC! Ожидался 0x" + String(crc, HEX) + ", получен 0x" + String(b, HEX));
            }
            state = WAIT_SOF;
        } break;
    }
}

/* ==== API инициализации ==== */
void UART_Init(uint32_t baud)
{
    swSerial.begin(baud);
    wsLog("UART инициализирован с baud=" + String(baud), "event");
}

/* ==== цикл обработки входящих байтов (вызывать в loop()) ==== */
void UART_Loop(void)
{
    while (swSerial.available())
    {
        uint8_t b = swSerial.read();
        parseByte(b);
    }
}

/* ==== отправка кадра (блокирующая, но кадры маленькие) ==== */
static void sendFrame(uint8_t cmd, uint8_t *payload, uint8_t len)
{
    uint8_t crc = crc8_calc(cmd, len, payload);

    String logMsg = "Отправка кадра CMD=0x" + String(cmd, HEX) + " LEN=" + String(len);
    for (int i = 0; i < len; i++) {
        logMsg += " " + String(payload[i], HEX);
    }
    logMsg += " CRC=0x" + String(crc, HEX);
    wsLogTx(logMsg);

    swSerial.write(SOF);
    swSerial.write(len);
    swSerial.write(cmd);

    for (uint8_t i = 0; i < len; i++)
        swSerial.write(payload[i]);

    swSerial.write(crc);
}

/* ==== публичные функции отправки ==== */
void UART_SendPumpAction(uint8_t action)
{
    uint8_t p[1] = { action };
    sendFrame(CMD_PUMP_ACTION, p, 1);
}

void UART_SendPressure(float pmin, float pmax)
{
    uint16_t p1 = (uint16_t)(pmin * 100);
    uint16_t p2 = (uint16_t)(pmax * 100);

    uint8_t p[4];
    p[0] = p1 & 0xFF;
    p[1] = p1 >> 8;
    p[2] = p2 & 0xFF;
    p[3] = p2 >> 8;

    sendFrame(CMD_SET_PRESSURE, p, 4);
}