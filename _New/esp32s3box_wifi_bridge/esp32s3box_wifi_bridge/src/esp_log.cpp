/**
 * esp_log.cpp — кольцевой буфер логов: каждая запись — строка с префиксом [uptime_sec].
 * При добавлении новой записи, если буфер полон, запись идёт поверх старых данных (кольцо).
 */
#include "esp_log.h"
#include <Arduino.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

static char s_logBuf[ESP_LOG_SIZE];
static size_t s_logHead = 0;   /* Индекс следующего байта для записи (кольцо). */
static size_t s_logUsed = 0;   /* Текущее количество байт в буфере (до ESP_LOG_SIZE). */

/** Добавляет строку line с префиксом [millis/1000] в кольцевой буфер; обрезает по ESP_LOG_ENTRY. */
void espLogPrint(const char* line) {
    if (!line) return;
    unsigned long uptime = millis() / 1000;
    char pref[20];
    int plen = snprintf(pref, sizeof(pref), "[%lu] ", (unsigned long)uptime);
    if (plen <= 0) plen = 0;
    size_t len = strlen(line);
    if (plen + len > ESP_LOG_ENTRY - 2) len = (ESP_LOG_ENTRY - 2) - plen;
    if (len == 0 && plen == 0) return;
    for (int i = 0; i < plen; i++) {
        s_logBuf[s_logHead] = pref[i];
        s_logHead = (s_logHead + 1) % ESP_LOG_SIZE;
        if (s_logUsed < ESP_LOG_SIZE) s_logUsed++;
    }
    for (size_t i = 0; i < len; i++) {
        s_logBuf[s_logHead] = line[i];
        s_logHead = (s_logHead + 1) % ESP_LOG_SIZE;
        if (s_logUsed < ESP_LOG_SIZE) s_logUsed++;
    }
    s_logBuf[s_logHead] = '\n';
    s_logHead = (s_logHead + 1) % ESP_LOG_SIZE;
    if (s_logUsed < ESP_LOG_SIZE) s_logUsed++;
}

/** Форматирует строку по fmt и аргументам (как printf) и добавляет её через espLogPrint. */
void espLogPrintf(const char* fmt, ...) {
    char line[ESP_LOG_ENTRY];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (n > 0) espLogPrint(line);
}

/** Копирует содержимое кольцевого буфера в buf (макс. bufSize-1 байт), добавляет '\0'. Возвращает длину без '\0'. */
size_t espLogGetText(char* buf, size_t bufSize) {
    if (!buf || bufSize == 0) return 0;
    if (s_logUsed == 0) {
        buf[0] = '\0';
        return 0;
    }
    size_t start = s_logUsed < ESP_LOG_SIZE ? 0 : s_logHead;
    size_t copied = 0;
    for (size_t i = 0; i < s_logUsed && copied < bufSize - 1; i++) {
        size_t idx = (start + i) % ESP_LOG_SIZE;
        buf[copied++] = s_logBuf[idx];
    }
    buf[copied] = '\0';
    return copied;
}

void espLogClear(void) {
    s_logHead = 0;
    s_logUsed = 0;
}

#ifdef __cplusplus
}
#endif
