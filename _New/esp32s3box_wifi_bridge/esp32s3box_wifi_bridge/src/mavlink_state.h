/**
 * mavlink_state.h — состояние MAVLink и API разбора/отправки.
 *
 * Парсинг входящих HEARTBEAT и PARAM_VALUE; отправка PARAM_REQUEST_READ
 * и PARAM_SET в UART к автопилоту. Данные используются веб-интерфейсом
 * (статус подключения, параметры SERVO, метрики пакетов/потерь).
 */
#ifndef MAVLINK_STATE_H
#define MAVLINK_STATE_H

#include "config.h"
#include <stddef.h>
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
extern uint32_t mavlinkPacketDrops;  /* Пакеты с ошибкой CRC (для расчёта потерь в веб). */

/** Счётчики приёма/передачи по типу сообщения (msgid 0..255). Для единого лога. */
extern uint32_t mavlinkRxByMsgid[256];
extern uint32_t mavlinkTxByMsgid[256];

extern char mavlinkLog[MAVLINK_LOG_SIZE][MAVLINK_LOG_ENTRY_LEN];
extern uint8_t mavlinkLogHead;

/** Краткое имя типа сообщения для лога (HEARTBEAT, PARAM_VALUE, msg_30 и т.д.). */
const char* mavlinkGetMsgName(uint8_t msgid);

/** Записать в buf строку со счётчиками по типам (RX/TX) для единого лога. */
void mavlinkGetCountersString(char* buf, size_t bufSize);

void mavlinkInitLog(void);
void mavlinkProcessBytes(const uint8_t* data, uint16_t len);
void mavlinkCheckDisconnect(void);
void mavlinkSendParamRequest(const char* param_id);
void mavlinkRequestServoParams(void);
void mavlinkSendParamSet(const char* param_id, float value);
void mavlinkAddLog(const char* event);

#endif /* MAVLINK_STATE_H */
