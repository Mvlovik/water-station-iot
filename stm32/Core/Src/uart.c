#include "uart.h"
#include <string.h>

/* ===================== Константы протокола ===================== */

#define UART_SOF            0xAAu
#define UART_MAX_PAYLOAD    14u

#define CMD_TELEMETRY            0x01u
#define CMD_EVENT_PUMP           0x02u
#define CMD_SETTINGS              0x03u

#define CMD_PUMP_ACTION           0x10u
#define CMD_SET_PRESSURE_RANGE    0x11u

/* Если TX "висит" дольше этого - считаем линию оборванной и сбрасываем busy,
 * чтобы отправка не блокировалась навсегда. */
#define UART_TX_STUCK_TIMEOUT_MS  200u
#define UART_EVENT_QUEUE_SIZE 4u

/* ===================== Внутреннее состояние ===================== */

static UART_HandleTypeDef *s_huart = NULL;
static uint8_t s_rxByte = 0;

/* --- TX (неблокирующая, по прерываниям) --- */
static uint8_t          s_txBuf[3 + UART_MAX_PAYLOAD + 1];
static volatile bool     s_txBusy      = false;
static volatile uint32_t s_txStartTick = 0;

/* --- RX парсер --- */
typedef enum {
    PARSE_WAIT_SOF,
    PARSE_WAIT_LEN,
    PARSE_WAIT_CMD,
    PARSE_WAIT_PAYLOAD,
    PARSE_WAIT_CRC
} ParseState;

typedef struct {
    ParseState state;
    uint8_t    len;
    uint8_t    cmd;
    uint8_t    payload[UART_MAX_PAYLOAD];
    uint8_t    payloadIdx;
} FrameParser;


static FrameParser s_parser = { PARSE_WAIT_SOF, 0, 0, {0}, 0 };

typedef struct {
    volatile bool     pendingPressureRange;
    volatile uint16_t pressureMin_centi;
    volatile uint16_t pressureMax_centi;

    volatile bool    pendingPumpAction;
    volatile uint8_t pumpAction;
} UART_RxState;

static volatile UART_RxState s_rxState = {0};

/* ===================== CRC8 (Dallas/Maxim, poly 0x31) ===================== */

static uint8_t Crc8_Update(uint8_t crc, uint8_t data)
{
    crc ^= data;
    for (uint8_t i = 0; i < 8; i++)
        crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    return crc;
}

static uint8_t Crc8_Calc(uint8_t cmd, uint8_t len, const uint8_t *payload)
{
    uint8_t crc = 0;
    crc = Crc8_Update(crc, len);
    crc = Crc8_Update(crc, cmd);
    for (uint8_t i = 0; i < len; i++)
        crc = Crc8_Update(crc, payload[i]);
    return crc;
}


static void UART_sendFrame(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    if (len > UART_MAX_PAYLOAD) return;

    /* Предыдущая передача ещё не завершилась - тихо пропускаем этот кадр.
     * НЕ ждём и не блокируем главный цикл. */
    if (s_txBusy) return;

    uint8_t idx = 0;
    s_txBuf[idx++] = UART_SOF;
    s_txBuf[idx++] = len;
    s_txBuf[idx++] = cmd;
    for (uint8_t i = 0; i < len; i++)
        s_txBuf[idx++] = payload[i];
    s_txBuf[idx++] = Crc8_Calc(cmd, len, payload);

    s_txBusy      = true;
    s_txStartTick = HAL_GetTick();

    if (HAL_UART_Transmit_IT(s_huart, s_txBuf, idx) != HAL_OK) {
        s_txBusy = false; /* не удалось поставить в очередь - освобождаем сразу */
    }
}

/* ===================== Разбор принятой команды ===================== */

static void UART_Dispatch(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    switch (cmd) {

    case CMD_SET_PRESSURE_RANGE:
        if (len == 4) {
            uint16_t mn = (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
            uint16_t mx = (uint16_t)payload[2] | ((uint16_t)payload[3] << 8);
            s_rxState.pressureMin_centi = mn;
            s_rxState.pressureMax_centi = mx;
            s_rxState.pendingPressureRange = true;
        }
        break;

    case CMD_PUMP_ACTION:
        if (len == 1) {
            s_rxState.pumpAction = payload[0];
            s_rxState.pendingPumpAction = true;
        }
        break;

    default:
        break;
    }
}

/* ===================== Конечный автомат парсера ===================== */

static void UART_ParserFeed(uint8_t byte)
{
    switch (s_parser.state) {

    case PARSE_WAIT_SOF:
        if (byte == UART_SOF) s_parser.state = PARSE_WAIT_LEN;
        break;

    case PARSE_WAIT_LEN:
        if (byte > UART_MAX_PAYLOAD) {
            s_parser.state = PARSE_WAIT_SOF;
            break;
        }
        s_parser.len = byte;
        s_parser.payloadIdx = 0;
        s_parser.state = PARSE_WAIT_CMD;
        break;

    case PARSE_WAIT_CMD:
        s_parser.cmd = byte;
        s_parser.state = (s_parser.len == 0) ? PARSE_WAIT_CRC : PARSE_WAIT_PAYLOAD;
        break;

    case PARSE_WAIT_PAYLOAD:
        s_parser.payload[s_parser.payloadIdx++] = byte;
        if (s_parser.payloadIdx >= s_parser.len)
            s_parser.state = PARSE_WAIT_CRC;
        break;

    case PARSE_WAIT_CRC:
        if (Crc8_Calc(s_parser.cmd, s_parser.len, s_parser.payload) == byte) {
            UART_Dispatch(s_parser.cmd, s_parser.payload, s_parser.len);
        }
        s_parser.state = PARSE_WAIT_SOF;
        break;

    default:
        s_parser.state = PARSE_WAIT_SOF;
        break;
    }
}

/* ===================== HAL callbacks ===================== */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (s_huart != NULL && huart->Instance == s_huart->Instance) {
        UART_ParserFeed(s_rxByte);
        HAL_UART_Receive_IT(s_huart, &s_rxByte, 1);
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (s_huart != NULL && huart->Instance == s_huart->Instance) {
        s_txBusy = false;
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (s_huart != NULL && huart->Instance == s_huart->Instance) {
        s_parser.state = PARSE_WAIT_SOF;
        s_txBusy = false;
        __HAL_UART_CLEAR_PEFLAG(s_huart);
        __HAL_UART_CLEAR_FEFLAG(s_huart);
        __HAL_UART_CLEAR_NEFLAG(s_huart);
        __HAL_UART_CLEAR_OREFLAG(s_huart);
        HAL_UART_Receive_IT(s_huart, &s_rxByte, 1);
    }
}

/* ===================== Публичное API ===================== */

void UART_Init(UART_HandleTypeDef *huart)
{
    s_huart = huart;
    s_parser.state = PARSE_WAIT_SOF;
    s_txBusy = false;
    memset((void *)&s_rxState, 0, sizeof(s_rxState));
    HAL_UART_Receive_IT(s_huart, &s_rxByte, 1);
}

void UART_Poll(void)
{
    if (s_txBusy && (HAL_GetTick() - s_txStartTick > UART_TX_STUCK_TIMEOUT_MS)) {
        /* Обрыв линии / ESP не читает - принудительно освобождаем TX,
         * чтобы UART_sendFrame() не отказывался слать кадры вечно. */
        s_txBusy = false;
    }
}
void UART_sendTelemetry(float pressure, float current1A, float current2A,
                        uint8_t errorState, float lastTempC, uint8_t aroundStatus)
{
    uint16_t pressure_centi =
        (uint16_t)(pressure * 100.0f + 0.5f);

    int16_t current1_mA =
        (int16_t)(current1A * 1000.0f +
                  (current1A >= 0.0f ? 0.5f : -0.5f));

    int16_t current2_mA =
        (int16_t)(current2A * 1000.0f +
                  (current2A >= 0.0f ? 0.5f : -0.5f));

    int16_t temp_centi =
        (int16_t)(lastTempC * 100.0f +
                  (lastTempC >= 0.0f ? 0.5f : -0.5f));

    uint32_t tick = HAL_GetTick();

    uint8_t buf[14];

    // Pressure, 0.01 единицы
    buf[0] = (uint8_t)(pressure_centi & 0xFF);
    buf[1] = (uint8_t)(pressure_centi >> 8);

    // Current 1, mA
    buf[2] = (uint8_t)((uint16_t)current1_mA & 0xFF);
    buf[3] = (uint8_t)((uint16_t)current1_mA >> 8);

    // Current 2, mA
    buf[4] = (uint8_t)((uint16_t)current2_mA & 0xFF);
    buf[5] = (uint8_t)((uint16_t)current2_mA >> 8);

    // Error state
    buf[6] = errorState;

    // Temperature, 0.01 °C
    buf[7] = (uint8_t)((uint16_t)temp_centi & 0xFF);
    buf[8] = (uint8_t)((uint16_t)temp_centi >> 8);

    // Around status
    buf[9] = aroundStatus;

    // HAL_GetTick(), milliseconds
    buf[10] = (uint8_t)(tick & 0xFF);
    buf[11] = (uint8_t)((tick >> 8) & 0xFF);
    buf[12] = (uint8_t)((tick >> 16) & 0xFF);
    buf[13] = (uint8_t)((tick >> 24) & 0xFF);

    UART_sendFrame(CMD_TELEMETRY, buf, sizeof(buf));
}

void UART_sendEventPump(uint8_t pumpId, bool commandedState, bool actualState, uint32_t durationSec)
{
    uint8_t buf[7];
    buf[0] = pumpId;
    buf[1] = commandedState ? 1u : 0u;
    buf[2] = actualState ? 1u : 0u;
    buf[3] = (uint8_t)(durationSec & 0xFF);
    buf[4] = (uint8_t)((durationSec >> 8) & 0xFF);
    buf[5] = (uint8_t)((durationSec >> 16) & 0xFF);
    buf[6] = (uint8_t)((durationSec >> 24) & 0xFF);

    UART_sendFrame(CMD_EVENT_PUMP, buf, sizeof(buf));
}

void UART_sendSettings(float pressureOn, float pressureOff, uint32_t pumpMaxTimeSessionSec)
{
    uint16_t pOn_centi  = (uint16_t)(pressureOn  * 100.0f + 0.5f);
    uint16_t pOff_centi = (uint16_t)(pressureOff * 100.0f + 0.5f);

    uint8_t buf[8];
    buf[0] = (uint8_t)(pOn_centi & 0xFF);
    buf[1] = (uint8_t)(pOn_centi >> 8);
    buf[2] = (uint8_t)(pOff_centi & 0xFF);
    buf[3] = (uint8_t)(pOff_centi >> 8);
    buf[4] = (uint8_t)(pumpMaxTimeSessionSec & 0xFF);
    buf[5] = (uint8_t)((pumpMaxTimeSessionSec >> 8) & 0xFF);
    buf[6] = (uint8_t)((pumpMaxTimeSessionSec >> 16) & 0xFF);
    buf[7] = (uint8_t)((pumpMaxTimeSessionSec >> 24) & 0xFF);

    UART_sendFrame(CMD_SETTINGS, buf, sizeof(buf));
}

bool UART_isAvailable(void)
{
    return s_rxState.pendingPressureRange || s_rxState.pendingPumpAction;
}

UART_CmdType UART_read(float *newPressureMin, float *newPressureMax, uint8_t *pumpAction)
{
    __disable_irq();

    if (s_rxState.pendingPressureRange) {
        uint16_t mn = s_rxState.pressureMin_centi;
        uint16_t mx = s_rxState.pressureMax_centi;
        s_rxState.pendingPressureRange = false;
        __enable_irq();

        if (newPressureMin) *newPressureMin = (float)mn / 100.0f;
        if (newPressureMax) *newPressureMax = (float)mx / 100.0f;
        return UART_CMD_SET_PRESSURE_RANGE;
    }

    if (s_rxState.pendingPumpAction) {
        uint8_t act = s_rxState.pumpAction;
        s_rxState.pendingPumpAction = false;
        __enable_irq();

        if (pumpAction) *pumpAction = act;
        return UART_CMD_PUMP_ACTION;
    }

    __enable_irq();
    return UART_CMD_NONE;
}
