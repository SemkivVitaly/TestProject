/**
 * bridge_log.cpp — единый лог: ID, подключение, статистика, WiFi dBm, 1 RX и 1 TX пакет.
 */
#include "bridge_log.h"
#include "esp_log.h"
#include "mavlink_state.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <stdio.h>
#include <string.h>

#define SAMPLE_MAX 64
#define HEX_LINE 16
#define LAST_ERROR_LEN 64
#define LAST_UART_ERROR_LEN 32
#define MAVLINK_LOG_TAIL 20

static char s_uniqueId[24];
static bool s_connected = false;
static uint32_t s_sent = 0, s_received = 0, s_lost = 0, s_total = 0;
static int8_t s_rssi = 0;
static uint8_t s_rxSample[SAMPLE_MAX];
static uint8_t s_txSample[SAMPLE_MAX];
static uint16_t s_rxLen = 0, s_txLen = 0;
static bool s_idDone = false;
static char s_lastError[LAST_ERROR_LEN] = "";
static char s_lastUartError[LAST_UART_ERROR_LEN] = "none";

static void ensureUniqueId(void) {
    if (s_idDone) return;
    s_idDone = true;
    /* MAC уникален для устройства; без ESP.h для совместимости с IntelliSense. */
    String mac = WiFi.macAddress();
    snprintf(s_uniqueId, sizeof(s_uniqueId), "OPTIONAL_%s", mac.c_str());
}

void bridgeLogGetUniqueId(char* buf, size_t bufSize) {
    ensureUniqueId();
    strncpy(buf, s_uniqueId, bufSize - 1);
    buf[bufSize - 1] = '\0';
}

void bridgeLogSetConnected(bool connected) {
    s_connected = connected;
}

void bridgeLogUpdateStats(uint32_t sent, uint32_t received, uint32_t lost, uint32_t total) {
    s_sent = sent;
    s_received = received;
    s_lost = lost;
    s_total = total;
}

void bridgeLogUpdateRssi(int8_t rssi_dbm) {
    s_rssi = rssi_dbm;
}

void bridgeLogSetLastRx(const uint8_t* data, uint16_t len) {
    if (!data) return;
    s_rxLen = len < SAMPLE_MAX ? len : SAMPLE_MAX;
    memcpy(s_rxSample, data, s_rxLen);
}

void bridgeLogSetLastTx(const uint8_t* data, uint16_t len) {
    if (!data) return;
    s_txLen = len < SAMPLE_MAX ? len : SAMPLE_MAX;
    memcpy(s_txSample, data, s_txLen);
}

void bridgeLogSetLastError(const char* err) {
    if (!err) return;
    strncpy(s_lastError, err, LAST_ERROR_LEN - 1);
    s_lastError[LAST_ERROR_LEN - 1] = '\0';
}

void bridgeLogGetLastError(char* buf, size_t bufSize) {
    if (!buf || bufSize == 0) return;
    strncpy(buf, s_lastError[0] ? s_lastError : "none", bufSize - 1);
    buf[bufSize - 1] = '\0';
}

void bridgeLogSetLastUartError(const char* err) {
    if (!err) return;
    strncpy(s_lastUartError, err, LAST_UART_ERROR_LEN - 1);
    s_lastUartError[LAST_UART_ERROR_LEN - 1] = '\0';
}

static void appendHexLine(char* buf, size_t* pos, size_t maxLen, const uint8_t* data, uint16_t len) {
    for (uint16_t i = 0; i < len && *pos + 4 < maxLen; i++)
        *pos += (size_t)snprintf(buf + *pos, maxLen - *pos, "%02X ", data[i]);
    if (len > 0 && *pos < maxLen - 1) {
        buf[(*pos)++] = '\n';
    }
    buf[*pos] = '\0';
}

size_t bridgeLogGetText(char* buf, size_t bufSize) {
    if (!buf || bufSize < 64) return 0;
    ensureUniqueId();
    size_t pos = 0;
    unsigned long uptime = millis() / 1000;

    pos += (size_t)snprintf(buf + pos, bufSize - pos, "=== Bridge Log ===\n");
    pos += (size_t)snprintf(buf + pos, bufSize - pos, "Сформировано: uptime %lu s\n", (unsigned long)uptime);
    pos += (size_t)snprintf(buf + pos, bufSize - pos, "ID: %s\n", s_uniqueId);
    pos += (size_t)snprintf(buf + pos, bufSize - pos, "MAVLink/MissionPlanner: %s\n",
                           s_connected ? "подключено успешно" : "не подключено");
    pos += (size_t)snprintf(buf + pos, bufSize - pos, "Последняя ошибка: %s\n", s_lastError[0] ? s_lastError : "none");
    pos += (size_t)snprintf(buf + pos, bufSize - pos, "UART ошибки: %s\n", s_lastUartError);
    pos += (size_t)snprintf(buf + pos, bufSize - pos, "--- Статистика пакетов ---\n");
    pos += (size_t)snprintf(buf + pos, bufSize - pos, "Отправленные | Полученные | Потерянные | Всего\n");
    pos += (size_t)snprintf(buf + pos, bufSize - pos, "%lu | %lu | %lu | %lu\n",
                             (unsigned long)s_sent, (unsigned long)s_received,
                             (unsigned long)s_lost, (unsigned long)s_total);
    pos += (size_t)snprintf(buf + pos, bufSize - pos, "WiFi RSSI: %d dBm\n", (int)s_rssi);
    pos += (size_t)snprintf(buf + pos, bufSize - pos, "--- Счётчики по типам MAVLink ---\n");
    if (bufSize > pos + 64) {
        char cntBuf[384];
        mavlinkGetCountersString(cntBuf, sizeof(cntBuf));
        pos += (size_t)snprintf(buf + pos, bufSize - pos, "%s\n", cntBuf);
    }
    pos += (size_t)snprintf(buf + pos, bufSize - pos, "--- 1 пакет RX (образец) ---\n");
    appendHexLine(buf, &pos, bufSize, s_rxSample, s_rxLen);
    pos += (size_t)snprintf(buf + pos, bufSize - pos, "--- 1 пакет TX (образец) ---\n");
    appendHexLine(buf, &pos, bufSize, s_txSample, s_txLen);
    pos += (size_t)snprintf(buf + pos, bufSize - pos, "--- Последние MAVLink события ---\n");
    for (uint8_t n = 0; n < MAVLINK_LOG_TAIL && pos < bufSize - MAVLINK_LOG_ENTRY_LEN - 2; n++) {
        uint8_t idx = (mavlinkLogHead + MAVLINK_LOG_SIZE - 1 - n) % MAVLINK_LOG_SIZE;
        if (mavlinkLog[idx][0] == '\0') continue;
        pos += (size_t)snprintf(buf + pos, bufSize - pos, "%s\n", mavlinkLog[idx]);
    }
    pos += (size_t)snprintf(buf + pos, bufSize - pos, "--- События ESP32 ---\n");
    if (bufSize > pos + 32) {
        size_t rest = bufSize - pos - 1;
        size_t n = espLogGetText(buf + pos, rest);
        pos += n;
    }
    buf[pos] = '\0';
    return pos;
}
