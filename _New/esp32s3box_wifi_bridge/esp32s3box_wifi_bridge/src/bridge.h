/**
 * bridge.h — объявления (API) асинхронного моста «сеть (TCP/UDP/BT) ↔ UART».
 *
 * АРХИТЕКТУРА:
 *   TCP-сервер — AsyncTCP (неблокирующий, обслуживается в отдельном async_tcp task на core 0).
 *   Для каждого TCP-клиента — StreamBuffer (стрим-буфер FreeRTOS), в который uart-task пишет данные
 *   от автопилота; AsyncTCP периодически (onAck/onPoll) опустошает его в сокет без блокировки.
 *   Для входящих данных от GCS (onData) — байты кладутся в очередь uartTxQueue и пишутся в UART из uart-task.
 *   UDP — AsyncUDP: onPacket читает пакет, пишет в UART (из LWIP task); исходящие uart-task шлёт writeTo().
 *
 * ВЗАИМОДЕЙСТВИЕ:
 *   — main.cpp в setup() вызывает bridgeSetup() и bridgeStartUartTask().
 *   — uart-task вызывает bridgeSendUartToNetwork(data, len) (неблокирующе).
 *   — main.cpp loop() вызывает bridgePollDisconnects() для таймаута UDP (изредка).
 *   — web_handlers.cpp читает atomic-счётчики и bridgeGetTcpConnectedCount() для /api/* ответов.
 */
#ifndef BRIDGE_H
#define BRIDGE_H

#include "config.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#ifdef __cplusplus
#include <atomic>
#include <IPAddress.h>
#endif

#ifdef __cplusplus
/** Агрегированные байт-счётчики. uint64_t — без wrap; atomic — для thread-safety между uart-task/web-task/lwip. */
extern std::atomic<uint64_t> uartBytesRx;        /* Прочитано из UART (от автопилота). */
extern std::atomic<uint64_t> uartBytesTx;        /* Записано в UART (от GCS). */
extern std::atomic<uint32_t> uartOverruns;        /* Зафиксировано переполнений RX-буфера UART. */
extern std::atomic<uint64_t> netBytesToGcs;      /* Ушло байт в сеть (TCP + UDP). */
extern std::atomic<uint64_t> netBytesFromGcs;    /* Пришло байт из сети (TCP + UDP). */
extern std::atomic<uint32_t> tcpConnectsTotal;   /* accept() за uptime. */
extern std::atomic<uint32_t> tcpDisconnectsTotal;/* onDisconnect за uptime. */
extern std::atomic<uint32_t> apAssocTotal;
extern std::atomic<uint32_t> apDisassocTotal;
extern std::atomic<uint32_t> staReconnectsTotal;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Инициализация TCP-сервера (AsyncTCP), UDP-сокета (AsyncUDP). Вызывать из setup() после WiFi. */
void bridgeSetup(void);

/** Стартовать FreeRTOS-task uart-task. Вызывать ПОСЛЕ bridgeSetup() и SerialUART.begin(). */
void bridgeStartUartTask(void);

/** Отправить буфер всем активным каналам (TCP-клиентам и известному UDP-клиенту). Без блокировки. Вызывать ТОЛЬКО из uart-task. */
void bridgeSendUartToNetwork(const uint8_t* data, uint16_t len);

/** Периодические проверки: UDP-таймаут и очистка слотов. Вызывать из loop() раз в ~1 c. */
void bridgePollDisconnects(void);

/** Текущее число подключённых TCP-клиентов. Thread-safe (atomic). */
uint8_t bridgeGetTcpConnectedCount(void);
/** 0 или 1 — известен ли UDP-клиент (получен хотя бы один пакет от него). */
uint8_t bridgeGetUdpClientCount(void);

/** Записать в buf строку "IP:port" текущего UDP-клиента. Возвращает true, если клиент известен. */
bool bridgeGetUdpClientInfo(char* buf, size_t bufSize);

/** Информация по слоту TCP для /api/clients. Возвращает true, если слот занят. */
typedef struct {
    bool     active;
    char     peer[32];
    uint32_t connectedMs;
    uint64_t bytesIn;
    uint64_t bytesOut;
    uint32_t dropBytes;
    size_t   queueUsed;   /* сколько байт сейчас в стрим-буфере слота, ожидают отправки */
    size_t   queueSize;
} bridge_tcp_slot_info_t;
bool bridgeGetTcpSlotInfo(uint8_t idx, bridge_tcp_slot_info_t* out);

/** Статистика UDP-клиента (последний пакет, байты). */
typedef struct {
    bool     known;
    char     peer[32];
    uint32_t lastPacketMs;
    uint64_t bytesIn;
    uint64_t bytesOut;
} bridge_udp_info_t;
void bridgeGetUdpInfo(bridge_udp_info_t* out);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
void bridgeClearUdpClient(void);                          /* Сбросить известного UDP-клиента. */
void bridgeSetUdpClient(IPAddress ip, uint16_t port);     /* Задать UDP-клиента вручную (веб-API). */
#else
void bridgeClearUdpClient(void);
#endif

#endif /* BRIDGE_H */
