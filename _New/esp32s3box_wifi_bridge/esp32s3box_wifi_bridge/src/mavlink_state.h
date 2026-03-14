/**
 * mavlink_state.h — состояние MAVLink и API разбора/отправки.
 *
 * ЧТО ДЕЛАЕТ МОДУЛЬ:
 *   Парсит входящие байты с UART (от автопилота): HEARTBEAT — считаем связь установленной,
 *   PARAM_VALUE — извлекаем параметры SERVO1_REVERSED, SERVO3_TRIM, SERVO4_TRIM.
 *   Отправка в UART: PARAM_REQUEST_READ (запрос параметра), PARAM_SET (установка) — по запросу веб-интерфейса.
 *
 * КТО ВЫЗЫВАЕТ:
 *   — main.cpp в loop(): mavlinkProcessBytes(buf, len) для каждого блока с UART; mavlinkCheckDisconnect().
 *   — web_handlers: mavlinkRequestServoParams(), mavlinkSendParamSet(); читает mavlinkConnected, paramServo* и т.д. для /api/status, /params.
 */
#ifndef MAVLINK_STATE_H
#define MAVLINK_STATE_H

#include "config.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

extern bool mavlinkConnected;       /* true после получения хотя бы одного HEARTBEAT от автопилота. */
extern uint32_t lastHeartbeatMs;    /* Время последнего HEARTBEAT (для таймаута отключения). */
extern uint8_t autopilotSysId;     /* System ID автопилота из HEARTBEAT. */
extern uint8_t autopilotCompId;    /* Component ID автопилота. */

extern float paramServo1Revers;    /* Значение параметра SERVO1_REVERSED из PARAM_VALUE. */
extern float paramServo3Trim;
extern float paramServo4Trim;
extern bool paramServo1ReversKnown; /* true, если значение уже получено с автопилота. */
extern bool paramServo3TrimKnown;
extern bool paramServo4TrimKnown;

extern uint32_t mavlinkPacketsRx;   /* Всего принято корректных MAVLink-пакетов. */
extern uint32_t mavlinkPacketsTx;   /* Всего отправлено (PARAM_REQUEST_READ, PARAM_SET и т.д.). */
extern uint32_t mavlinkPacketDrops; /* Пакеты с ошибкой CRC — для расчёта % потерь в веб. */

/** Счётчики приёма/передачи по типу сообщения (msgid 0..255). Используются в едином логе. */
extern uint32_t mavlinkRxByMsgid[256];
extern uint32_t mavlinkTxByMsgid[256];

extern char mavlinkLog[MAVLINK_LOG_SIZE][MAVLINK_LOG_ENTRY_LEN];  /* Кольцевой лог событий. */
extern uint8_t mavlinkLogHead;  /* Индекс следующей записи в кольце. */

const char* mavlinkGetMsgName(uint8_t msgid);  /* Краткое имя типа (HEARTBEAT, PARAM_VALUE, msg_30 ...). */
void mavlinkGetCountersString(char* buf, size_t bufSize);  /* Строка со счётчиками по типам для лога. */

void mavlinkInitLog(void);   /* Обнулить кольцевой лог. Вызывается из main setup(). */
void mavlinkProcessBytes(const uint8_t* data, uint16_t len);  /* Разобрать байты, обновить состояние и счётчики. */
void mavlinkCheckDisconnect(void);  /* Если прошло > MAVLINK_HEARTBEAT_TIMEOUT_MS без HEARTBEAT — mavlinkConnected = false. */
void mavlinkSendParamRequest(const char* param_id);  /* Отправить PARAM_REQUEST_READ в UART. */
void mavlinkRequestServoParams(void);  /* Запросить SERVO1_REVERSED, SERVO3_TRIM, SERVO4_TRIM. */
void mavlinkSendParamSet(const char* param_id, float value);  /* Отправить PARAM_SET в UART. */
void mavlinkAddLog(const char* event);  /* Добавить запись в кольцевой лог. */

#endif /* MAVLINK_STATE_H */
