#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <Arduino.h>

void wsInit();
void wsLoop();

// Функции для логирования в консоль
void wsLog(const String& message, const String& category = "event");
void wsLogRx(const String& data);
void wsLogTx(const String& data);
void wsLogError(const String& message);
void wsLogWarning(const String& message);

#endif