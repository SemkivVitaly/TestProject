/**
 * bridge_log.h — единый лог моста: ID устройства, статус подключения, статистика, образцы RX/TX.
 *
 * Назначение: собрать в один текстовый отчёт всё, что нужно для диагностики (ID, MAVLink подключён или нет,
 * счётчики пакетов, RSSI, последние байты RX/TX, события MAVLink и ESP32). Этот текст отдаётся по /api/log/file.
 *
 * КТО ВЫЗЫВАЕТ:
 *   — bridge.cpp: bridgeLogSetLastRx/LastTx при пересылке; bridgeLogSetLastError при ошибке UDP.
 *   — mavlink_state.cpp: bridgeLogSetConnected() при HEARTBEAT/таймауте.
 *   — web_handlers: bridgeLogUpdateStats(), bridgeLogUpdateRssi() при /api/system/stats; bridgeLogGetText() для /api/log/file.
 */
#ifndef BRIDGE_LOG_H
#define BRIDGE_LOG_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

void bridgeLogGetUniqueId(char* buf, size_t bufSize);   /* Уникальный ID (на основе MAC). */
void bridgeLogSetConnected(bool connected);              /* Отметить подключение/отключение MAVLink. */
void bridgeLogUpdateStats(uint32_t sent, uint32_t received, uint32_t lost, uint32_t total);  /* Обновить счётчики пакетов. */
void bridgeLogUpdateRssi(int8_t rssi_dbm);              /* Обновить уровень сигнала Wi‑Fi (dBm). */
void bridgeLogSetLastRx(const uint8_t* data, uint16_t len);  /* Сохранить образец последнего принятого пакета. */
void bridgeLogSetLastTx(const uint8_t* data, uint16_t len); /* Сохранить образец последнего отправленного. */
size_t bridgeLogGetText(char* buf, size_t bufSize);      /* Сформировать полный текст лога в buf; возвращает длину. */
void bridgeLogSetLastError(const char* err);
void bridgeLogGetLastError(char* buf, size_t bufSize);    /* Последняя ошибка (для /api/status). */
void bridgeLogSetLastUartError(const char* err);         /* Ошибка UART (overflow/framing). */

#endif /* BRIDGE_LOG_H */
