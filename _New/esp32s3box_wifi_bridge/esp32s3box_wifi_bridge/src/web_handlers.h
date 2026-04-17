/**
 * web_handlers.h — асинхронный веб-сервер и его инициализация.
 *
 * Вся регистрация маршрутов — в webSetup(). Контекст: AsyncWebServer работает в async_tcp task (core 0).
 * Никаких блокирующих операций в хендлерах: JSON собираем в char[], большие тексты стримим чанками.
 */
#ifndef WEB_HANDLERS_H
#define WEB_HANDLERS_H

#include "config.h"

#ifdef WEB_SERVER

#ifdef __cplusplus
extern "C" {
#endif

/** Создать и запустить AsyncWebServer с зарегистрированными маршрутами. Вызывать ОДИН раз из setup(). */
void webSetup(void);

#ifdef __cplusplus
}
#endif

#endif /* WEB_SERVER */

#endif /* WEB_HANDLERS_H */
