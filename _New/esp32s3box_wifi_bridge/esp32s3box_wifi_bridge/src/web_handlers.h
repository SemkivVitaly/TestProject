/**
 * web_handlers.h — API веб-сервера (локальный доступ по Wi‑Fi платы).
 *
 * Веб-интерфейс доступен только в локальной сети: подключение к SSID ESP32,
 * затем в браузере http://192.168.2.1. Регистрация маршрутов и вызов server.begin()
 * выполняются в webSetup(). Активно только при определённом WEB_SERVER в config.h.
 */
#ifndef WEB_HANDLERS_H
#define WEB_HANDLERS_H

#include "config.h"

#ifdef WEB_SERVER
#include <WebServer.h>

/** Зарегистрировать все маршруты на server и вызвать server.begin(). Вызывать из setup() после LittleFS.begin(). */
void webSetup(WebServer& server);
#endif

#endif /* WEB_HANDLERS_H */
