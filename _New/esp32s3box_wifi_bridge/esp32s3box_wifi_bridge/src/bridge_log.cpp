/**
 * bridge_log.cpp — формирование единого текстового лога для скачивания (/api/log/file).
 *
 * Хранит: уникальный ID чипа (eFuse), флаг подключения MAVLink, счётчики пакетов, RSSI, температура кристалла,
 * образцы последнего RX/TX (hex), последние записи из mavlinkLog и espLog.
 * bridgeLogGetText() собирает всё в один буфер для ответа HTTP.
 *
 * ИДЕНТИФИКАЦИЯ УСТРОЙСТВА:
 *   Предпочитается ESP_EFUSE_OPTIONAL_UNIQUE_ID — 128-битный заводской серийник чипа ESP32-S2/S3/C3/C6.
 *   Это настоящий неизменяемый die-ID, в отличие от WiFi MAC, который можно переписать программно
 *   (esp_base_mac_addr_set). Фолбэк — ESP.getEfuseMac() (64-битный заводской base MAC из eFuse).
 */
#include "bridge_log.h"
#include "esp_log.h"
#include "mavlink_state.h"
#include "config.h"
#include <Arduino.h>
#include <cmath>
#include <stdio.h>
#include <string.h>
#include <esp_system.h>
#include <freertos/portmacro.h>
#include <esp_efuse.h>
#if __has_include(<esp_efuse_chip.h>)
#  include <esp_efuse_chip.h>
#endif
#if __has_include(<esp_efuse_table.h>)
#  include <esp_efuse_table.h>
#endif

#define SAMPLE_MAX 64          /* Макс. байт в образце RX/TX. */
#define HEX_LINE 16
#define LAST_ERROR_LEN 64
#define LAST_UART_ERROR_LEN 32
#define MAVLINK_LOG_TAIL 20    /* Сколько последних записей MAVLink выводить в лог. */

/* "UID:XXXXXXXX-XXXXXXXX-XXXXXXXX-XXXXXXXX" = 39 + NUL; с запасом 64. */
static char s_uniqueId[64];
static bool s_connected = false;
static uint32_t s_sent = 0, s_received = 0, s_lost = 0, s_total = 0;
static int8_t s_rssi = 0;
static uint8_t s_rxSample[SAMPLE_MAX];
static uint8_t s_txSample[SAMPLE_MAX];
static uint16_t s_rxLen = 0, s_txLen = 0;
static bool s_idDone = false;
static char s_lastError[LAST_ERROR_LEN] = "";
static char s_lastUartError[LAST_UART_ERROR_LEN] = "none";

/* UART-task и AsyncWebServer читают/пишут образцы с разных ядер — без мьютекса возможны гонки и «пустой» hex в /api/log/samples. */
static portMUX_TYPE s_sampleMux = portMUX_INITIALIZER_UNLOCKED;

/**
 * Формирует уникальный ID чипа один раз и кэширует. Приоритет:
 *   1) ESP_EFUSE_OPTIONAL_UNIQUE_ID — 128-битный заводской die-ID (S2/S3/C3/C6). Истинная "UnicID".
 *   2) ESP.getEfuseMac() — 64-битный заводской base MAC (всегда доступен, неизменяем).
 *
 * WiFi.macAddress() НЕ используется — он может быть переписан esp_base_mac_addr_set() и
 * не гарантирует соответствие физическому чипу.
 */
static void ensureUniqueId(void) {
    if (s_idDone) return;
    s_idDone = true;

#if defined(ESP_EFUSE_OPTIONAL_UNIQUE_ID)
    {
        uint8_t uid[16] = {0};
        esp_err_t err = esp_efuse_read_field_blob(ESP_EFUSE_OPTIONAL_UNIQUE_ID, uid, 128);
        bool nonZero = false;
        for (int i = 0; i < 16; i++) { if (uid[i] != 0) { nonZero = true; break; } }
        if (err == ESP_OK && nonZero) {
            snprintf(s_uniqueId, sizeof(s_uniqueId),
                     "UID:%02X%02X%02X%02X-%02X%02X%02X%02X-%02X%02X%02X%02X-%02X%02X%02X%02X",
                     uid[0], uid[1], uid[2],  uid[3],  uid[4],  uid[5],  uid[6],  uid[7],
                     uid[8], uid[9], uid[10], uid[11], uid[12], uid[13], uid[14], uid[15]);
            return;
        }
    }
#endif

    /* Фолбэк: заводской base MAC из eFuse (не подвержен программным изменениям). */
    uint64_t mac64 = ESP.getEfuseMac();
    uint8_t m[6] = {
        (uint8_t)(mac64 >> 40), (uint8_t)(mac64 >> 32),
        (uint8_t)(mac64 >> 24), (uint8_t)(mac64 >> 16),
        (uint8_t)(mac64 >>  8), (uint8_t)(mac64 >>  0),
    };
    snprintf(s_uniqueId, sizeof(s_uniqueId), "MAC:%02X%02X%02X%02X%02X%02X",
             m[0], m[1], m[2], m[3], m[4], m[5]);
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
    if (!data || len == 0) return;
    uint16_t n = len < SAMPLE_MAX ? len : SAMPLE_MAX;
    portENTER_CRITICAL(&s_sampleMux);
    s_rxLen = n;
    memcpy(s_rxSample, data, n);
    portEXIT_CRITICAL(&s_sampleMux);
}

void bridgeLogSetLastTx(const uint8_t* data, uint16_t len) {
    if (!data || len == 0) return;
    uint16_t n = len < SAMPLE_MAX ? len : SAMPLE_MAX;
    portENTER_CRITICAL(&s_sampleMux);
    s_txLen = n;
    memcpy(s_txSample, data, n);
    portEXIT_CRITICAL(&s_sampleMux);
}

uint16_t bridgeLogGetLastRxSample(uint8_t* buf, uint16_t bufSize) {
    if (!buf || bufSize == 0) return 0;
    portENTER_CRITICAL(&s_sampleMux);
    uint16_t n = s_rxLen < bufSize ? s_rxLen : bufSize;
    memcpy(buf, s_rxSample, n);
    portEXIT_CRITICAL(&s_sampleMux);
    return n;
}

uint16_t bridgeLogGetLastTxSample(uint8_t* buf, uint16_t bufSize) {
    if (!buf || bufSize == 0) return 0;
    portENTER_CRITICAL(&s_sampleMux);
    uint16_t n = s_txLen < bufSize ? s_txLen : bufSize;
    memcpy(buf, s_txSample, n);
    portEXIT_CRITICAL(&s_sampleMux);
    return n;
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

/** Дописывает в buf одну строку hex-дампа data (до len байт). */
static void appendHexLine(char* buf, size_t* pos, size_t maxLen, const uint8_t* data, uint16_t len) {
    for (uint16_t i = 0; i < len && *pos + 4 < maxLen; i++)
        *pos += (size_t)snprintf(buf + *pos, maxLen - *pos, "%02X ", data[i]);
    if (len > 0 && *pos < maxLen - 1) {
        buf[(*pos)++] = '\n';
    }
    buf[*pos] = '\0';
}

/** Собирает полный текст единого лога в buf (ID, подключение, статистика, RSSI, счётчики по типам MAVLink, образцы RX/TX, хвост mavlinkLog, события ESP32). Возвращает длину. */
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
    {
        float tChip = temperatureRead();
        if (std::isfinite(static_cast<double>(tChip))) {
            pos += (size_t)snprintf(buf + pos, bufSize - pos,
                                    "Температура кристалла (внутр. датчик): %.1f C\n", (double)tChip);
        } else {
            pos += (size_t)snprintf(buf + pos, bufSize - pos,
                                    "Температура кристалла (внутр. датчик): н/д\n");
        }
    }
    pos += (size_t)snprintf(buf + pos, bufSize - pos, "--- Счётчики по типам MAVLink ---\n");
    if (bufSize > pos + 64) {
        char cntBuf[384];
        mavlinkGetCountersString(cntBuf, sizeof(cntBuf));
        pos += (size_t)snprintf(buf + pos, bufSize - pos, "%s\n", cntBuf);
    }
    uint8_t rxCp[SAMPLE_MAX], txCp[SAMPLE_MAX];
    uint16_t rxLc = 0, txLc = 0;
    portENTER_CRITICAL(&s_sampleMux);
    rxLc = s_rxLen;
    txLc = s_txLen;
    if (rxLc) memcpy(rxCp, s_rxSample, rxLc);
    if (txLc) memcpy(txCp, s_txSample, txLc);
    portEXIT_CRITICAL(&s_sampleMux);
    pos += (size_t)snprintf(buf + pos, bufSize - pos, "--- Образец RX (UART от автопилота, FC→ESP) ---\n");
    appendHexLine(buf, &pos, bufSize, rxCp, rxLc);
    pos += (size_t)snprintf(buf + pos, bufSize - pos, "--- Образец TX (от GCS в UART, MP→FC) ---\n");
    appendHexLine(buf, &pos, bufSize, txCp, txLc);
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
