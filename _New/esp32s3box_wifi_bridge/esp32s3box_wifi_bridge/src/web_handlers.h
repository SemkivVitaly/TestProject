/**
 * web_handlers.h — объявление инициализации веб-сервера (обработчики маршрутов).
 *
 * КТО ВЫЗЫВАЕТ:
 *   — main.cpp в setup() при определённом WEB_SERVER вызывает webSetup(webServer).
 *   webSetup() регистрирует все маршруты (/, /params, /api/status, /api/settings и т.д.)
 *   и привязывает их к функциям в web_handlers.cpp. Обработка запросов — в loop() через webServer.handleClient().
 */
#ifndef WEB_HANDLERS_H
#define WEB_HANDLERS_H

#include "config.h"

#ifdef WEB_SERVER
#include <WebServer.h>

/** Регистрирует все HTTP-маршруты на переданном WebServer и вызывает server.begin(). Вызывать из setup() после LittleFS.begin(). */
void webSetup(WebServer& server);
#endif

#endif
