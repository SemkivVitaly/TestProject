/**
 * web_handlers.h — API веб-сервера (локальный доступ по Wi‑Fi платы).
 */
#ifndef WEB_HANDLERS_H
#define WEB_HANDLERS_H

#include "config.h"

#ifdef WEB_SERVER
#include <WebServer.h>

void webSetup(WebServer& server);
#endif

#endif
