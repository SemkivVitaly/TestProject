/**
 * mavlink_state.cpp — разбор MAVLink и отправка команд по параметрам.
 *
 * Принимает байты с UART (от автопилота), парсит HEARTBEAT и PARAM_VALUE,
 * обновляет состояние (подключение, параметры SERVO). Отправка в UART
 * выполняется через extern SerialUART (PARAM_REQUEST_READ, PARAM_SET).
 * Счётчики пакетов и потерь используются веб-интерфейсом для метрик.
 */
#include <Arduino.h>
#include <string.h>
#include "config.h"
#include <MAVLink.h>
#include "mavlink_state.h"
#include "bridge_log.h"
#include "esp_log.h"

extern HardwareSerial SerialUART;

/* Состояние подключения к автопилоту (по HEARTBEAT). */
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

/* Метрики для веб-интерфейса: принято/отправлено пакетов, ошибки CRC (потери). */
uint32_t mavlinkPacketsRx = 0;
uint32_t mavlinkPacketsTx = 0;
uint32_t mavlinkPacketDrops = 0;

/* Счётчики по типам сообщений (msgid) для единого лога. */
uint32_t mavlinkRxByMsgid[256] = {0};
uint32_t mavlinkTxByMsgid[256] = {0};

/* Кольцевой лог событий MAVLink для /api/log и страницы параметров. */
char mavlinkLog[MAVLINK_LOG_SIZE][MAVLINK_LOG_ENTRY_LEN];
uint8_t mavlinkLogHead = 0;

/* Ограничение частоты записи HEARTBEAT в кольцевой лог (раз в 10 с). */
static uint32_t s_lastHeartbeatLogMs = 0;
#define HEARTBEAT_LOG_INTERVAL_MS 10000

/* Идентификаторы GCS в MAVLink (Mission Planner и др.). */
static const uint8_t MAVLINK_GCS_SYSID = 255;
static const uint8_t MAVLINK_GCS_COMPID = 190;

void mavlinkInitLog(void) {
    memset(mavlinkLog, 0, sizeof(mavlinkLog));
}

/** Краткое имя типа сообщения для лога. */
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

/** Сформировать строку со счётчиками по типам для единого лога. */
void mavlinkGetCountersString(char* buf, size_t bufSize) {
    if (!buf || bufSize < 32) return;
    size_t pos = 0;
    for (int i = 0; i < 256 && pos < bufSize - 32; i++) {
        if (mavlinkRxByMsgid[i] == 0 && mavlinkTxByMsgid[i] == 0) continue;
        const char* name = mavlinkGetMsgName((uint8_t)i);
        int n = snprintf(buf + pos, bufSize - pos, "%s RX=%lu TX=%lu; ",
                        name, (unsigned long)mavlinkRxByMsgid[i], (unsigned long)mavlinkTxByMsgid[i]);
        if (n > 0) pos += (size_t)n;
    }
    if (pos > 0 && buf[pos - 1] == ' ') buf[pos - 1] = '\0';
}

/** Добавить запись в кольцевой лог (время в секундах + текст). */
void mavlinkAddLog(const char* event) {
    snprintf(mavlinkLog[mavlinkLogHead], MAVLINK_LOG_ENTRY_LEN, "%lu %s",
             (unsigned long)(millis() / 1000), event);
    mavlinkLogHead = (mavlinkLogHead + 1) % MAVLINK_LOG_SIZE;
}

/**
 * Разобрать байты из UART: парсинг MAVLink, обновление состояния и счётчиков.
 * Вызывается из main loop() для каждого блока данных, прочитанных с SerialUART.
 */
void mavlinkProcessBytes(const uint8_t* data, uint16_t len) {
    mavlink_message_t msg;
    static mavlink_status_t status;
    for (uint16_t i = 0; i < len; i++) {
        if (!mavlink_parse_char(MAVLINK_COMM_0, data[i], &msg, &status))
            continue;
        mavlinkPacketsRx++;
        if (msg.msgid < 256)
            mavlinkRxByMsgid[msg.msgid]++;
        switch (msg.msgid) {
            case MAVLINK_MSG_ID_HEARTBEAT: {
                mavlinkConnected = true;
                lastHeartbeatMs = millis();
                autopilotSysId = msg.sysid;
                autopilotCompId = msg.compid;
                bridgeLogSetConnected(true);
                espLogPrintf("[MAVLink] connected (HEARTBEAT)");
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
                /* Остальные типы учитываются в mavlinkRxByMsgid[]; в кольцевой лог не пишем, чтобы не затирать редкие события. */
                break;
        }
    }
    mavlinkPacketDrops = (uint32_t)status.packet_rx_drop_count;
}

/** Проверить таймаут HEARTBEAT; при превышении сбросить mavlinkConnected. Вызывать в loop(). */
void mavlinkCheckDisconnect(void) {
    if (!mavlinkConnected)
        return;
    if (millis() - lastHeartbeatMs <= MAVLINK_HEARTBEAT_TIMEOUT_MS)
        return;
    mavlinkConnected = false;
    bridgeLogSetConnected(false);
    espLogPrintf("[MAVLink] disconnected (no heartbeat)");
    mavlinkAddLog("DISCONNECT (no heartbeat)");
}

/** Сформировать и отправить PARAM_REQUEST_READ для одного параметра (в UART к автопилоту). */
void mavlinkSendParamRequest(const char* param_id) {
    mavlink_message_t msg;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    mavlink_msg_param_request_read_pack(MAVLINK_GCS_SYSID, MAVLINK_GCS_COMPID, &msg,
                                        autopilotSysId, autopilotCompId, param_id, -1);
    uint16_t n = mavlink_msg_to_send_buffer(buf, &msg);
    SerialUART.write(buf, n);
    mavlinkPacketsTx++;
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
    mavlinkPacketsTx++;
    if (msg.msgid < 256)
        mavlinkTxByMsgid[msg.msgid]++;
    char ev[MAVLINK_LOG_ENTRY_LEN];
    snprintf(ev, sizeof(ev), "TX PARAM_SET %s", param_id);
    mavlinkAddLog(ev);
}
