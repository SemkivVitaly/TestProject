/**
 * esp_log.h — кольцевой буфер логов ESP32 для сохранения/скачивания.
 */
#ifndef ESP_LOG_H
#define ESP_LOG_H

#include <stddef.h>

#define ESP_LOG_SIZE  2048  /* размер буфера в байтах */
#define ESP_LOG_ENTRY 128   /* макс. длина одной строки */

#ifdef __cplusplus
extern "C" {
#endif

/** Добавить строку в лог (обрезается по ESP_LOG_ENTRY). */
void espLogPrint(const char* line);

/** Добавить форматированную строку (как printf). */
void espLogPrintf(const char* fmt, ...);

/** Получить содержимое лога в buf (макс. bufSize). Возвращает длину. */
size_t espLogGetText(char* buf, size_t bufSize);

/** Очистить лог. */
void espLogClear(void);

#ifdef __cplusplus
}
#endif

#endif /* ESP_LOG_H */
