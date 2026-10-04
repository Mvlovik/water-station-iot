#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <ESP8266WiFi.h>

// Используем другое имя для enum, чтобы не конфликтовать с WiFiMode из библиотеки
enum WiFiModeType {
    WIFI_MODE_AP,
    WIFI_MODE_STA
};

// Настройки
extern const char* ap_ssid;
extern const char* ap_password;
extern const char* sta_ssid;
extern const char* sta_password;

void wifiInit(WiFiModeType mode = WIFI_MODE_AP);
void wifiSetMode(WiFiModeType mode);
String wifiGetIP();

// Объявляем currentMode как extern, чтобы использовать в других файлах
extern WiFiModeType currentMode;

#endif