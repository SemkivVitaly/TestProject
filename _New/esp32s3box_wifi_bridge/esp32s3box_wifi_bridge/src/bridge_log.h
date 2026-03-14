/**
 * bridge_log.h — единый лог моста: ID, подключение, статистика пакетов, WiFi dBm, примеры RX/TX.
 */
#ifndef BRIDGE_LOG_H
#define BRIDGE_LOG_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/** Уникальный ID модуля (на основе MAC). Записать в buf, макс. bufSize символов. */
void bridgeLogGetUniqueId(char* buf, size_t bufSize);

/** Отметить успешное подключение ESP32–MissionPlanner–MAVLink. */
void bridgeLogSetConnected(bool connected);

/** Обновить статистику пакетов. */
void bridgeLogUpdateStats(uint32_t sent, uint32_t received, uint32_t lost, uint32_t total);

/** Обновить уровень сигнала WiFi (dBm). */
void bridgeLogUpdateRssi(int8_t rssi_dbm);

/** Сохранить последний принятый пакет (образец, до sampleLen байт). */
void bridgeLogSetLastRx(const uint8_t* data, uint16_t len);

/** Сохранить последний отправленный пакет (образец). */
void bridgeLogSetLastTx(const uint8_t* data, uint16_t len);

/** Сформировать полный текст лога в buf (макс. bufSize). Возвращает длину. */
size_t bridgeLogGetText(char* buf, size_t bufSize);

/** Установить/получить последнюю ошибку (для /api/status и единого лога). */
void bridgeLogSetLastError(const char* err);
void bridgeLogGetLastError(char* buf, size_t bufSize);

/** Установить последнюю ошибку UART (overflow/framing), если драйвер сообщает. */
void bridgeLogSetLastUartError(const char* err);

#endif /* BRIDGE_LOG_H */
