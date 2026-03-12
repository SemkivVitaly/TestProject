/**
 * web_handlers.h — регистрация маршрутов и обработчиков веб-сервера.
 */
#ifndef WEB_HANDLERS_H
#define WEB_HANDLERS_H

#include "config.h"

#ifdef WEB_SERVER
#include <WebServer.h>

void webSetup(WebServer& server);
#endif

#endif /* WEB_HANDLERS_H */
