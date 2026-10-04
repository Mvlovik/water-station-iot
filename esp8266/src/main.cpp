#include <Arduino.h>
#include "wifi_manager.h"
#include "websocket.h"
#include "uart.h"

void setup() {
    Serial.begin(115200);
    
    // Инициализация UART
    UART_Init(19200);
    
    // Инициализация Wi-Fi в режиме точки доступа
    wifiInit(WIFI_MODE_STA);  // Используем WIFI_MODE_AP из нашего enum
    
    // Инициализация WebSocket
    wsInit();
    
    // Выводим информацию о подключении
    Serial.println();
    Serial.println("=== System Ready ===");
    Serial.print("Wi-Fi Mode: ");
    Serial.println(currentMode == WIFI_MODE_AP ? "Access Point" : "Client");
    Serial.print("IP Address: ");
    Serial.println(wifiGetIP());
    Serial.print("WebSocket port: 81");
    Serial.println();
    Serial.println("=== Connect to: ===");
    Serial.print("http://");
    Serial.print(wifiGetIP());
    Serial.println(":81");
    Serial.println("====================");
}

void loop() {
    wsLoop();
    UART_Loop();
}