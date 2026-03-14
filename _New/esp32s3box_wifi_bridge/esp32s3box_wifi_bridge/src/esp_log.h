/**
 * esp_log.h — кольцевой буфер логов ESP32 (события прошивки с префиксом [uptime]).
 *
 * Назначение: хранить последние строки лога в RAM, чтобы их можно было скачать по /api/log/esp32.
 * При переполнении старые записи затираются (кольцевой буфер).
 *
 * КТО ВЫЗЫВАЕТ:
 *   — main.cpp, bridge.cpp, mavlink_state.cpp: espLogPrintf() для важных событий (WiFi, TCP/UDP, MAVLink).
 *   — web_handlers: espLogGetText() для ответа на GET /api/log/esp32.
 */
#ifndef ESP_LOG_H
#define ESP_LOG_H

#include <stddef.h>

#define ESP_LOG_SIZE  2048  /* Размер буфера в байтах. */
#define ESP_LOG_ENTRY 128   /* Макс. длина одной строки (с префиксом [uptime] ). */

#ifdef __cplusplus
extern "C" {
#endif

void espLogPrint(const char* line);       /* Добавить строку в лог (обрезается по ESP_LOG_ENTRY). */
void espLogPrintf(const char* fmt, ...);  /* Форматированная строка (как printf). */
size_t espLogGetText(char* buf, size_t bufSize);  /* Скопировать содержимое лога в buf; возвращает длину. */
void espLogClear(void);                   /* Очистить буфер. */

#ifdef __cplusplus
}
#endif

#endif /* ESP_LOG_H */
