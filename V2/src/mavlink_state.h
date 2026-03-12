/**
 * mavlink_state.h — состояние MAVLink и API для разбора/отправки
 */
#ifndef MAVLINK_STATE_H
#define MAVLINK_STATE_H

#include "config.h"
#include <stdint.h>
#include <stdbool.h>

extern bool mavlinkConnected;
extern uint32_t lastHeartbeatMs;
extern uint8_t autopilotSysId;
extern uint8_t autopilotCompId;
extern float paramServo1Revers;
extern float paramServo3Trim;
extern float paramServo4Trim;
extern bool paramServo1ReversKnown;
extern bool paramServo3TrimKnown;
extern bool paramServo4TrimKnown;
extern uint32_t mavlinkPacketsRx;
extern uint32_t mavlinkPacketsTx;
extern uint32_t mavlinkPacketDrops;
extern char mavlinkLog[MAVLINK_LOG_SIZE][MAVLINK_LOG_ENTRY_LEN];
extern uint8_t mavlinkLogHead;

void mavlinkInitLog(void);
void mavlinkProcessBytes(const uint8_t* data, uint16_t len);
void mavlinkCheckDisconnect(void);
void mavlinkSendParamRequest(const char* param_id);
void mavlinkRequestServoParams(void);
void mavlinkSendParamSet(const char* param_id, float value);
void mavlinkAddLog(const char* event);

#endif /* MAVLINK_STATE_H */
