/**
 * mavlink_state.h — состояние MAVLink и API разбора/отправки.
 *
 * ЧТО ДЕЛАЕТ МОДУЛЬ:
 *   Парсит входящие байты с UART (от автопилота): HEARTBEAT — считаем связь установленной,
 *   PARAM_VALUE — извлекаем параметры SERVO1_REVERSED, SERVO3_TRIM, SERVO4_TRIM.
 *   Отправка в UART: PARAM_REQUEST_READ (запрос параметра), PARAM_SET (установка) — по запросу веб-интерфейса.
 *
 * КТО ВЫЗЫВАЕТ:
 *   — UART-task (uart_task в main.cpp): mavlinkProcessBytes(buf, len) для каждого блока с UART.
 *   — loop() в main.cpp: mavlinkCheckDisconnect() для таймаута HEARTBEAT.
 *   — web_handlers: mavlinkRequestServoParams(), mavlinkSendParamSet(); читает mavlinkConnected, paramServo* и т.д.
 *
 * ПОТЕРИ / ЗАПРОСЫ:
 *   Телеметрия: gap в последовательности seq на потоке UART←автопилот — mavlinkRxLost, доля в mavlink_seq_loss_pct.
 *   Запросы Mission Planner (GCS→UART): PARAM_* / COMMAND_* считаются в gcsMavRequestsTx; ответ PARAM_VALUE /
 *   COMMAND_ACK (кроме IN_PROGRESS) закрывает ожидание — gcsMavRequestsOk / Fail; доля отказов в mavlink_loss_pct.
 *   status.packet_rx_drop_count — per-byte parse error, аккумулируем как mavlinkParseErr.
 */
#ifndef MAVLINK_STATE_H
#define MAVLINK_STATE_H

#include "config.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
#include <atomic>
#endif

extern bool mavlinkConnected;       /* true после получения хотя бы одного HEARTBEAT от автопилота. */
extern uint32_t lastHeartbeatMs;    /* Время последнего HEARTBEAT (для таймаута отключения). */
/** Сглаженный период между HEARTBEAT (мс), 0 пока не известен — для корректного отображения «задержки» в веб. */
extern uint32_t mavlinkHeartbeatIntervalMs;
extern uint8_t autopilotSysId;     /* System ID автопилота из HEARTBEAT. */
extern uint8_t autopilotCompId;    /* Component ID автопилота. */

extern float paramServo1Revers;    /* Значение параметра SERVO1_REVERSED из PARAM_VALUE. */
extern float paramServo3Trim;
extern float paramServo4Trim;
extern bool paramServo1ReversKnown; /* true, если значение уже получено с автопилота. */
extern bool paramServo3TrimKnown;
extern bool paramServo4TrimKnown;

#ifdef __cplusplus
/** Атомарные счётчики — читаются из web-task и UART-task, модифицируются из UART-task. */
extern std::atomic<uint64_t> mavlinkRxPkts;       /* Успешно декодированные MAVLink-пакеты с автопилота. */
extern std::atomic<uint64_t> mavlinkRxLost;       /* Потеряно пакетов по gap в msg.seq (реальные потери). */
extern std::atomic<uint64_t> mavlinkParseErr;     /* Накопленные parse_error (CRC/framing) — не пакеты, а события. */
extern std::atomic<uint64_t> mavlinkBridgeTxPkts; /* Пакеты, сгенерированные самим мостом (PARAM_REQUEST/SET). */
extern std::atomic<uint64_t> mavlinkBytesFromUart; /* Байт, ушедших в парсер (обычно = uartBytesRx). */
/** Запросы от GCS (Mission Planner/QGC) по TCP/UDP→UART: PARAM_READ/SET, COMMAND_LONG/INT. */
extern std::atomic<uint64_t> gcsMavRequestsTx;
extern std::atomic<uint64_t> gcsMavRequestsOk;   /* Успешный ответ: PARAM_VALUE по имени или COMMAND_ACK ACCEPTED. */
extern std::atomic<uint64_t> gcsMavRequestsFail; /* Отказ/таймаут COMMAND_ACK или ожидание PARAM. */
#endif

/** Счётчики приёма/передачи по типу сообщения (msgid 0..255). Используются в едином логе. */
extern uint32_t mavlinkRxByMsgid[256];
extern uint32_t mavlinkTxByMsgid[256];

extern char mavlinkLog[MAVLINK_LOG_SIZE][MAVLINK_LOG_ENTRY_LEN];  /* Кольцевой лог событий. */
extern uint8_t mavlinkLogHead;  /* Индекс следующей записи в кольце. */

#ifdef __cplusplus
extern "C" {
#endif

const char* mavlinkGetMsgName(uint8_t msgid);  /* Краткое имя типа (HEARTBEAT, PARAM_VALUE, msg_30 ...). */
void mavlinkGetCountersString(char* buf, size_t bufSize);  /* Строка со счётчиками по типам для лога. */

void mavlinkInitLog(void);   /* Обнулить кольцевой лог. Вызывается из main setup(). */
void mavlinkProcessBytes(const uint8_t* data, uint16_t len);  /* Разобрать байты, обновить состояние и счётчики. Вызывать ТОЛЬКО из uart-task. */
void mavlinkCheckDisconnect(void);  /* Если прошло > MAVLINK_HEARTBEAT_TIMEOUT_MS без HEARTBEAT — mavlinkConnected = false. */
void mavlinkSendParamRequest(const char* param_id);  /* Отправить PARAM_REQUEST_READ в UART. */
void mavlinkRequestServoParams(void);  /* Запросить SERVO1_REVERSED, SERVO3_TRIM, SERVO4_TRIM. */
void mavlinkSendParamSet(const char* param_id, float value);  /* Отправить PARAM_SET в UART. */
void mavlinkAddLog(const char* event);  /* Добавить запись в кольцевой лог. */
/** Разбор исходящего потока GCS→UART (TCP/UDP). Вызывать из AsyncTCP / lwip до SerialUART.write. */
void mavlinkScanGcsTxBytes(const uint8_t* data, size_t len);
/** Таймауты ожиданий ответов на запросы GCS; вызывать из loop() периодически. */
void mavlinkGcsPendingTick(void);

#ifdef __cplusplus
}
#endif

#endif /* MAVLINK_STATE_H */
