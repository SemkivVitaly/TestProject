/**
 * mavlink_state.cpp — разбор MAVLink и отправка параметров.
 */
#include <Arduino.h>
#include <string.h>
#include "config.h"
#include <MAVLink.h>
#include "mavlink_state.h"

extern HardwareSerial SerialUART;

bool mavlinkConnected = false;
uint32_t lastHeartbeatMs = 0;
uint8_t autopilotSysId = 1;
uint8_t autopilotCompId = 1;
float paramServo1Revers = 0.0f;
float paramServo3Trim = 0.0f;
float paramServo4Trim = 0.0f;
bool paramServo1ReversKnown = false;
bool paramServo3TrimKnown = false;
bool paramServo4TrimKnown = false;
uint32_t mavlinkPacketsRx = 0;
uint32_t mavlinkPacketsTx = 0;
uint32_t mavlinkPacketDrops = 0;
char mavlinkLog[MAVLINK_LOG_SIZE][MAVLINK_LOG_ENTRY_LEN];
uint8_t mavlinkLogHead = 0;

static const uint8_t MAVLINK_GCS_SYSID = 255;
static const uint8_t MAVLINK_GCS_COMPID = 190;

void mavlinkInitLog(void) {
    memset(mavlinkLog, 0, sizeof(mavlinkLog));
}

void mavlinkAddLog(const char* event) {
    snprintf(mavlinkLog[mavlinkLogHead], MAVLINK_LOG_ENTRY_LEN, "%lu %s",
             (unsigned long)(millis() / 1000), event);
    mavlinkLogHead = (mavlinkLogHead + 1) % MAVLINK_LOG_SIZE;
}

void mavlinkProcessBytes(const uint8_t* data, uint16_t len) {
    mavlink_message_t msg;
    static mavlink_status_t status;
    for (uint16_t i = 0; i < len; i++) {
        if (!mavlink_parse_char(MAVLINK_COMM_0, data[i], &msg, &status))
            continue;
        mavlinkPacketsRx++;
        switch (msg.msgid) {
            case MAVLINK_MSG_ID_HEARTBEAT:
                mavlinkConnected = true;
                lastHeartbeatMs = millis();
                autopilotSysId = msg.sysid;
                autopilotCompId = msg.compid;
                mavlinkAddLog("RX HEARTBEAT");
                break;
            case MAVLINK_MSG_ID_PARAM_VALUE: {
                mavlink_param_value_t pv;
                mavlink_msg_param_value_decode(&msg, &pv);
                pv.param_id[15] = '\0';
                if (strcmp(pv.param_id, "SERVO1_REVERS") == 0) {
                    paramServo1Revers = pv.param_value;
                    paramServo1ReversKnown = true;
                    mavlinkAddLog("RX PARAM_VALUE SERVO1_REVERS");
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
    mavlinkPacketDrops = (uint32_t)status.packet_rx_drop_count;
}

void mavlinkCheckDisconnect(void) {
    if (!mavlinkConnected)
        return;
    if (millis() - lastHeartbeatMs <= MAVLINK_HEARTBEAT_TIMEOUT_MS)
        return;
    mavlinkConnected = false;
    mavlinkAddLog("DISCONNECT (no heartbeat)");
}

void mavlinkSendParamRequest(const char* param_id) {
    mavlink_message_t msg;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    mavlink_msg_param_request_read_pack(MAVLINK_GCS_SYSID, MAVLINK_GCS_COMPID, &msg,
                                        autopilotSysId, autopilotCompId, param_id, -1);
    uint16_t n = mavlink_msg_to_send_buffer(buf, &msg);
    SerialUART.write(buf, n);
    mavlinkPacketsTx++;
}

void mavlinkRequestServoParams(void) {
    mavlinkSendParamRequest("SERVO1_REVERS");
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
    char ev[MAVLINK_LOG_ENTRY_LEN];
    snprintf(ev, sizeof(ev), "TX PARAM_SET %s", param_id);
    mavlinkAddLog(ev);
}
