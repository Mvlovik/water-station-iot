#include "wifi_manager.h"
#include <ESP8266WiFi.h>

// Настройки точки доступа
const char* ap_ssid = "WaterStaion";
const char* ap_password = "WaterStaionAbuse";

// Настройки для режима клиента (если нужен)
const char* sta_ssid = "WIFI_SSID";
const char* sta_password = "WIFI_PASSWORD";

WiFiModeType currentMode = WIFI_MODE_STA;

void wifiInit(WiFiModeType mode) {
    wifiSetMode(mode);
}

void wifiSetMode(WiFiModeType mode) {
    currentMode = mode;
    
    if (mode == WIFI_MODE_AP) {
        // Режим точки доступа
        WiFi.mode(WIFI_AP);
        
        IPAddress local_ip(192, 168, 4, 1);
        IPAddress gateway(192, 168, 4, 1);
        IPAddress subnet(255, 255, 255, 0);
        WiFi.softAPConfig(local_ip, gateway, subnet);
        
        WiFi.softAP(ap_ssid, ap_password);
        
        Serial.println();
        Serial.println("=== Wi-Fi Access Point ===");
        Serial.print("SSID: ");
        Serial.println(ap_ssid);
        Serial.print("Password: ");
        Serial.println(ap_password);
        Serial.print("IP Address: ");
        Serial.println(WiFi.softAPIP());
        Serial.println("===========================");
    } else {
        // Режим клиента
        WiFi.mode(WIFI_STA);
        WiFi.begin(sta_ssid, sta_password);
        
        Serial.println();
        Serial.println("=== Wi-Fi Client ===");
        Serial.print("Connecting to: ");
        Serial.println(sta_ssid);
        
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println();
            Serial.println("Connected!");
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
        } else {
            Serial.println();
            Serial.println("Failed to connect! Switching to AP mode...");
            wifiSetMode(WIFI_MODE_AP);
        }
        Serial.println("===========================");
    }
}

String wifiGetIP() {
    if (currentMode == WIFI_MODE_AP) {
        return WiFi.softAPIP().toString();
    } else {
        return WiFi.localIP().toString();
    }
}