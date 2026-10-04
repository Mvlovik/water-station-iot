#include "websocket.h"
#include "uart.h"
#include "web_page.h"

#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>

ESP8266WebServer server(80);
WebSocketsServer ws(81);

static uint32_t lastPush = 0;

#define HISTORY_SIZE 1500

struct TelemetryPoint {
    uint32_t time;
    float pressure;
    float current1;
    float current2;
    float temperature;
    uint8_t error;
    uint8_t status;
};

static TelemetryPoint history[HISTORY_SIZE];
static volatile uint16_t historyHead = 0;
static volatile uint16_t historyCount = 0;

static void addHistoryPoint(float p, float c1, float c2, float temp, uint8_t err, uint8_t stat) {
    history[historyHead].time = millis() / 1000;
    history[historyHead].pressure = p;
    history[historyHead].current1 = c1;
    history[historyHead].current2 = c2;
    history[historyHead].temperature = temp;
    history[historyHead].error = err;
    history[historyHead].status = stat;
    historyHead = (historyHead + 1) % HISTORY_SIZE;
    if (historyCount < HISTORY_SIZE) historyCount++;
}

static void sendHistory(uint8_t clientNum) {
    if (historyCount == 0) {
        ws.sendTXT(clientNum, "{\"type\":\"history\",\"data\":[]}");
        return;
    }

    const int CHUNK_SIZE = 50;
    uint16_t start = (historyHead - historyCount + HISTORY_SIZE) % HISTORY_SIZE;
    uint16_t total = historyCount;
    uint16_t sent = 0;

    while (sent < total && ws.clientIsConnected(clientNum)) {
        uint16_t chunk = (total - sent > CHUNK_SIZE) ? CHUNK_SIZE : (total - sent);
        String json = "{\"type\":\"history\",\"data\":[";
        for (uint16_t i = 0; i < chunk; i++) {
            uint16_t idx = (start + sent + i) % HISTORY_SIZE;
            if (i > 0) json += ",";
            json += "{";
            json += "\"t\":" + String(history[idx].time) + ",";
            json += "\"p\":" + String(history[idx].pressure, 2) + ",";
            json += "\"c1\":" + String(history[idx].current1, 3) + ",";
            json += "\"c2\":" + String(history[idx].current2, 3) + ",";
            json += "\"temp\":" + String(history[idx].temperature, 1) + ",";
            json += "\"e\":" + String(history[idx].error) + ",";
            json += "\"s\":" + String(history[idx].status);
            json += "}";
        }
        json += "]}";
        ws.sendTXT(clientNum, json);
        sent += chunk;
        delay(20);
    }
}

// Функции логирования
static void wsSendLog(const String& message, const String& category) {
    String json = "{";
    json += "\"type\":\"log\",";
    json += "\"category\":\"" + category + "\",";
    json += "\"message\":\"" + message + "\",";
    json += "\"time\":" + String(millis());
    json += "}";
    ws.broadcastTXT(json);
}

void wsLog(const String& message, const String& category) {
    wsSendLog(message, category);
}

void wsLogRx(const String& data) {
    wsSendLog("📥 " + data, "rx");
}

void wsLogTx(const String& data) {
    wsSendLog("📤 " + data, "tx");
}

void wsLogError(const String& message) {
    wsSendLog("❌ " + message, "error");
}

void wsLogWarning(const String& message) {
    wsSendLog("⚠️ " + message, "warning");
}

void handleRoot() {
    server.send(200, "text/html", MAIN_page);
}

void handleWs(uint8_t num, uint8_t *payload) {
    String msg = String((char*)payload);
    wsLogRx("Команда: " + msg);

    if (msg == "pump_on") {
        UART_SendPumpAction(0x01);
        wsLogTx("Отправка PUMP1_ON (0x01)");
    }
    else if (msg == "pump_off") {
        UART_SendPumpAction(0x02);
        wsLogTx("Отправка PUMP1_OFF (0x02)");
    }
    else if (msg == "irr_on") {
        UART_SendPumpAction(0x04);
        wsLogTx("Отправка PUMP2_ON (0x04)");
    }
    else if (msg == "irr_off") {
        UART_SendPumpAction(0x08);
        wsLogTx("Отправка PUMP2_OFF (0x08)");
    }
    else if (msg.startsWith("set:")) {
        int c = msg.indexOf(',');
        float pmin = msg.substring(4, c).toFloat();
        float pmax = msg.substring(c+1).toFloat();
        UART_SendPressure(pmin, pmax);
        wsLogTx("Отправка настроек давления: " + String(pmin) + " / " + String(pmax));
    }
    else if (msg == "get_settings") {
        wsLogTx("Запрос настроек STM32 (отправка не реализована)");
    }
    else {
        wsLogWarning("Неизвестная команда: " + msg);
    }
}

void wsEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    if (type == WStype_TEXT) {
        handleWs(num, payload);
    } else if (type == WStype_CONNECTED) {
        wsLog("Клиент подключен к WebSocket", "event");
        Serial.println("WebSocket client connected!");
        sendHistory(num);
    } else if (type == WStype_DISCONNECTED) {
        wsLog("Клиент отключен от WebSocket", "event");
        Serial.println("WebSocket client disconnected!");
    }
}

void wsInit() {
    server.on("/", handleRoot);
    server.begin();
    Serial.println("HTTP server started on port 80");

    ws.begin();
    ws.onEvent(wsEvent);
    Serial.println("WebSocket server started on port 81");

    wsLog("WebSocket инициализирован", "event");
}

void wsLoop() {
    server.handleClient();
    ws.loop();

    UART_Loop();

    if (millis() - lastPush > 200) {
        lastPush = millis();

        float p = g_pressure;
        float c1 = fabs(g_current1);
        float c2 = fabs(g_current2);
        float temp = g_temperature;
        uint8_t err = g_error;
        uint8_t stat = g_aroundStatus;

        addHistoryPoint(p, c1, c2, temp, err, stat);

        String json = "{";
        json += "\"type\":\"telemetry\",";
        json += "\"p\":" + String(p,2) + ",";
        json += "\"c1\":" + String(c1,3) + ",";
        json += "\"c2\":" + String(c2,3) + ",";
        json += "\"e\":" + String(err) + ",";
        json += "\"temp\":" + String(temp,1) + ",";
        json += "\"status\":" + String(stat) + ",";
        json += "\"p1c\":" + String(g_pump1_cmd) + ",";
        json += "\"p1a\":" + String(g_pump1_actual) + ",";
        json += "\"p2c\":" + String(g_pump2_cmd) + ",";
        json += "\"p2a\":" + String(g_pump2_actual) + ",";
        json += "\"t1\":" + String(g_pump1_time) + ",";
        json += "\"t2\":" + String(g_pump2_time) + ",";
        json += "\"pon\":" + String(g_p_on,2) + ",";
        json += "\"poff\":" + String(g_p_off,2) + ",";
        json += "\"tmax\":" + String(g_pumpMax) + ",";
        json += "\"online\":" + String(UART_IsConnected() ? "true" : "false");
        json += "}";

        ws.broadcastTXT(json);
    }
}