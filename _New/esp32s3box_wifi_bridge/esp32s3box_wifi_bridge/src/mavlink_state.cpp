/**
 * mavlink_state.cpp — разбор MAVLink и отправка команд по параметрам.
 *
 * ПОТОК ДАННЫХ:
 *   Входящие байты приходят из uart-task (SerialUART.read) → mavlinkProcessBytes().
 *   Парсер mavlink_parse_char() собирает пакеты; при полном пакете обрабатываем msgid (HEARTBEAT, PARAM_VALUE)
 *   и проверяем gap в msg.seq — это настоящие потери MAVLink (а не parse_error).
 *   Исходящие команды (PARAM_REQUEST_READ, PARAM_SET) пишутся в SerialUART из этого файла.
 *
 * ВЗАИМОДЕЙСТВИЕ:
 *   — bridge_log: bridgeLogSetConnected() при установке/потере связи.
 *   — esp_log: espLogPrintf() для событий в кольцевой лог ESP32.
 *
 * ПОТОКОБЕЗОПАСНОСТЬ:
 *   mavlinkProcessBytes() вызывается ТОЛЬКО из uart-task (FreeRTOS task на core 1).
 *   Статичные переменные парсера (mavlink_status_t, s_prevSeq) не разделяются между потоками.
 *   Счётчики-атомики (std::atomic) читаются из web-task без гонки.
 */
#include <Arduino.h>
#include <string.h>
#include <atomic>
#include "config.h"
#include <MAVLink.h>
#include "mavlink_state.h"
#include "bridge_log.h"
#include "esp_log.h"

extern HardwareSerial SerialUART;  /* UART к автопилоту (объявлен в main.cpp). */

/* Состояние подключения к автопилоту: true после первого принятого HEARTBEAT. */
bool mavlinkConnected = false;
uint32_t lastHeartbeatMs = 0;
uint8_t autopilotSysId = 1;
uint8_t autopilotCompId = 1;

/* Параметры SERVO из PARAM_VALUE (для веб-страницы /params). */
float paramServo1Revers = 0.0f;
float paramServo3Trim = 0.0f;
float paramServo4Trim = 0.0f;
bool paramServo1ReversKnown = false;
bool paramServo3TrimKnown = false;
bool paramServo4TrimKnown = false;

/* Атомарные счётчики: uint64_t — без wrap на годы работы. */
std::atomic<uint64_t> mavlinkRxPkts{0};
std::atomic<uint64_t> mavlinkRxLost{0};
std::atomic<uint64_t> mavlinkParseErr{0};
std::atomic<uint64_t> mavlinkBridgeTxPkts{0};
std::atomic<uint64_t> mavlinkBytesFromUart{0};

/* Счётчики по типам сообщений (msgid) для единого лога. Обновляются только из uart-task → без атомиков. */
uint32_t mavlinkRxByMsgid[256] = {0};
uint32_t mavlinkTxByMsgid[256] = {0};

/* Кольцевой лог событий MAVLink. */
char mavlinkLog[MAVLINK_LOG_SIZE][MAVLINK_LOG_ENTRY_LEN];
uint8_t mavlinkLogHead = 0;

/* Ограничение частоты записи HEARTBEAT в кольцевой лог (раз в 10 с). */
static uint32_t s_lastHeartbeatLogMs = 0;
#define HEARTBEAT_LOG_INTERVAL_MS 10000

uint32_t mavlinkHeartbeatIntervalMs = 0;
static uint32_t s_prevHeartbeatArrivalMs = 0;

/* Состояние детектора gap по seq. seq — 8-битовое поле, переполняется каждые 256 пакетов. */
static bool s_prevSeqValid = false;
static uint8_t s_prevSeq = 0;
static uint16_t s_prevParseErrSnapshot = 0;

/* System ID и Component ID, с которыми мы (GCS/мост) отправляем команды автопилоту. */
static const uint8_t MAVLINK_GCS_SYSID = 255;
static const uint8_t MAVLINK_GCS_COMPID = 190;

void mavlinkInitLog(void) {
    memset(mavlinkLog, 0, sizeof(mavlinkLog));
}

const char* mavlinkGetMsgName(uint8_t msgid) {
    static char s_name[24];
    switch (msgid) {
        case 0:  return "HEARTBEAT";
        case 1:  return "SYS_STATUS";
        case 2:  return "SYSTEM_TIME";
        case 4:  return "PING";
        case 20: return "PARAM_REQUEST_READ";
        case 22: return "PARAM_VALUE";
        case 23: return "PARAM_SET";
        case 24: return "GPS_RAW_INT";
        case 30: return "ATTITUDE";
        case 33: return "GLOBAL_POSITION_INT";
        case 35: return "RC_CHANNELS_RAW";
        case 147: return "BATTERY_STATUS";
        case 253: return "STATUSTEXT";
        default:
            snprintf(s_name, sizeof(s_name), "msg_%u", (unsigned)msgid);
            return s_name;
    }
}

void mavlinkGetCountersString(char* buf, size_t bufSize) {
    if (!buf || bufSize < 2) return;
    buf[0] = '\0';
    size_t pos = 0;
    for (int i = 0; i < 256 && pos + 32 < bufSize; i++) {
        if (mavlinkRxByMsgid[i] == 0 && mavlinkTxByMsgid[i] == 0) continue;
        const char* name = mavlinkGetMsgName((uint8_t)i);
        int n = snprintf(buf + pos, bufSize - pos, "%s RX=%lu TX=%lu; ",
                        name, (unsigned long)mavlinkRxByMsgid[i], (unsigned long)mavlinkTxByMsgid[i]);
        if (n > 0) pos += (size_t)n;
    }
    if (pos > 0 && buf[pos - 1] == ' ') buf[pos - 1] = '\0';
}

void mavlinkAddLog(const char* event) {
    snprintf(mavlinkLog[mavlinkLogHead], MAVLINK_LOG_ENTRY_LEN, "%lu %s",
             (unsigned long)(millis() / 1000), event);
    mavlinkLogHead = (mavlinkLogHead + 1) % MAVLINK_LOG_SIZE;
}

/**
 * Разбирает байты: для каждого полного пакета проверяет gap в msg.seq (настоящие потери),
 * аккумулирует parse_error (накопительный delta от status.packet_rx_drop_count, поскольку
 * поле сбрасывается парсером MAVLink для каждого пакета).
 * Вызывается ТОЛЬКО из uart-task.
 */
void mavlinkProcessBytes(const uint8_t* data, uint16_t len) {
    mavlink_message_t msg;
    static mavlink_status_t status;

    mavlinkBytesFromUart.fetch_add((uint64_t)len, std::memory_order_relaxed);

    for (uint16_t i = 0; i < len; i++) {
        uint8_t r = mavlink_parse_char(MAVLINK_COMM_0, data[i], &msg, &status);

        /* packet_rx_drop_count накапливается парсером, но сбрасывается при новом заголовке.
         * Берём дельту, чтобы получить аккумулятивный счётчик. Если значение уменьшилось — начался новый пакет. */
        uint16_t curErr = (uint16_t)status.packet_rx_drop_count;
        if (curErr >= s_prevParseErrSnapshot) {
            uint16_t delta = curErr - s_prevParseErrSnapshot;
            if (delta > 0)
                mavlinkParseErr.fetch_add((uint64_t)delta, std::memory_order_relaxed);
        }
        s_prevParseErrSnapshot = (r != 0) ? 0 : curErr; /* на полном пакете парсер обнулит, учитываем это */

        if (!r) continue;

        mavlinkRxPkts.fetch_add(1, std::memory_order_relaxed);
        if (msg.msgid < 256)
            mavlinkRxByMsgid[msg.msgid]++;

        /* gap-детект по seq (по модулю 256). Ограничиваем «скачок» 128, чтобы не считать back-to-front как потерю всей шкалы. */
        if (s_prevSeqValid) {
            uint8_t expected = (uint8_t)(s_prevSeq + 1);
            if (msg.seq != expected) {
                uint8_t gap = (uint8_t)(msg.seq - expected); /* wrap-around автоматически */
                if (gap > 0 && gap < 128)
                    mavlinkRxLost.fetch_add((uint64_t)gap, std::memory_order_relaxed);
            }
        }
        s_prevSeq = msg.seq;
        s_prevSeqValid = true;

        switch (msg.msgid) {
            case MAVLINK_MSG_ID_HEARTBEAT: {
                bool wasDisconnected = !mavlinkConnected;
                mavlinkConnected = true;
                {
                    uint32_t nowHb = millis();
                    if (s_prevHeartbeatArrivalMs != 0U && nowHb > s_prevHeartbeatArrivalMs) {
                        uint32_t dt = nowHb - s_prevHeartbeatArrivalMs;
                        if (dt >= 50U && dt < 60000U) {
                            if (mavlinkHeartbeatIntervalMs == 0U)
                                mavlinkHeartbeatIntervalMs = dt;
                            else
                                mavlinkHeartbeatIntervalMs = (mavlinkHeartbeatIntervalMs * 3U + dt) / 4U;
                        }
                    }
                    s_prevHeartbeatArrivalMs = nowHb;
                    lastHeartbeatMs = nowHb;
                }
                autopilotSysId = msg.sysid;
                autopilotCompId = msg.compid;
                bridgeLogSetConnected(true);
                if (wasDisconnected)
                    espLogPrintf("[MAVLink] connected sysid=%u", (unsigned)msg.sysid);
                if (millis() - s_lastHeartbeatLogMs >= HEARTBEAT_LOG_INTERVAL_MS) {
                    mavlinkAddLog("RX HEARTBEAT");
                    s_lastHeartbeatLogMs = millis();
                }
                break;
            }
            case MAVLINK_MSG_ID_PARAM_VALUE: {
                mavlink_param_value_t pv;
                mavlink_msg_param_value_decode(&msg, &pv);
                pv.param_id[15] = '\0';
                if (strcmp(pv.param_id, "SERVO1_REVERS") == 0 || strcmp(pv.param_id, "SERVO1_REVERSED") == 0) {
                    paramServo1Revers = pv.param_value;
                    paramServo1ReversKnown = true;
                    mavlinkAddLog("RX PARAM_VALUE SERVO1_REVERSED");
                } else if (strcmp(pv.param_id, "SERVO3_TRIM") == 0) {
                    paramServo3Trim = pv.param_value;
                    paramServo3TrimKnown = true;
                    mavlinkAddLog("RX PARAM_VALUE SERVO3_TRIM");
                } else if (strcmp(pv.param_id, "SERVO4_TRIM") == 0) {
                    paramServo4Trim = pv.param_value;
                    paramServo4TrimKnown = true;
                    mavlinkAddLog("RX PARAM_VALUE SERVO4_TRIM");
                }
                break;
            }
            default:
                break;
        }
    }
}

void mavlinkCheckDisconnect(void) {
    if (!mavlinkConnected)
        return;
    if (millis() - lastHeartbeatMs <= MAVLINK_HEARTBEAT_TIMEOUT_MS)
        return;
    mavlinkConnected = false;
    mavlinkHeartbeatIntervalMs = 0;
    s_prevHeartbeatArrivalMs = 0;
    s_prevSeqValid = false; /* при разрыве связи seq-состояние неактуально */
    bridgeLogSetConnected(false);
    espLogPrintf("[MAVLink] disconnected (no heartbeat)");
    mavlinkAddLog("DISCONNECT (no heartbeat)");
}

void mavlinkSendParamRequest(const char* param_id) {
    mavlink_message_t msg;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    mavlink_msg_param_request_read_pack(MAVLINK_GCS_SYSID, MAVLINK_GCS_COMPID, &msg,
                                        autopilotSysId, autopilotCompId, param_id, -1);
    uint16_t n = mavlink_msg_to_send_buffer(buf, &msg);
    SerialUART.write(buf, n);
    mavlinkBridgeTxPkts.fetch_add(1, std::memory_order_relaxed);
    if (msg.msgid < 256)
        mavlinkTxByMsgid[msg.msgid]++;
}

void mavlinkRequestServoParams(void) {
    mavlinkSendParamRequest("SERVO1_REVERSED");
    mavlinkSendParamRequest("SERVO3_TRIM");
    mavlinkSendParamRequest("SERVO4_TRIM");
    mavlinkAddLog("TX PARAM_REQUEST_READ (SERVO1/3/4)");
}

void mavlinkSendParamSet(const char* param_id, float value) {
    mavlink_message_t msg;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    mavlink_msg_param_set_pack(MAVLINK_GCS_SYSID, MAVLINK_GCS_COMPID, &msg,
                               autopilotSysId, autopilotCompId, param_id, value, MAV_PARAM_TYPE_REAL32);
    uint16_t n = mavlink_msg_to_send_buffer(buf, &msg);
    SerialUART.write(buf, n);
    mavlinkBridgeTxPkts.fetch_add(1, std::memory_order_relaxed);
    if (msg.msgid < 256)
        mavlinkTxByMsgid[msg.msgid]++;
    char ev[MAVLINK_LOG_ENTRY_LEN];
    snprintf(ev, sizeof(ev), "TX PARAM_SET %s", param_id);
    mavlinkAddLog(ev);
}
