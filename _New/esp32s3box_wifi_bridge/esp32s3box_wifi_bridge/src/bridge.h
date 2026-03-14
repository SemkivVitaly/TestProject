/**
 * bridge.h — объявления (API) моста «сеть (TCP/UDP/BT) ↔ UART».
 *
 * КТО ЧТО ВЫЗЫВАЕТ:
 *   — main.cpp в setup() вызывает bridgeSetup().
 *   — main.cpp в loop() по очереди: bridgeAcceptClient(), bridgePollDisconnects(), bridgePollNetworkToUart();
 *     после чтения из SerialUART вызывает bridgeSendUartToNetwork(data, len).
 *   — web_handlers.cpp читает bridgeBytesTxNetwork, bridgeBytesRxNetwork, bridgeGetTcpConnectedCount() и т.д. для /api/status, /api/link.
 *
 * ЧТО ПЕРЕДАЁТСЯ: в bridgeSendUartToNetwork() — указатель на буфер данных и длина в байтах; в bridgeSetUdpClient() — IP и порт UDP-клиента.
 */
#ifndef BRIDGE_H
#define BRIDGE_H

#include "config.h"
#include <stdint.h>

/** Инициализация TCP-сервера (порт из config.h), UDP-сокета и при BLUETOOTH — Bluetooth. Вызывать из setup() после настройки WiFi. */
void bridgeSetup(void);

/** Проверить, есть ли новый TCP-клиент на сервере; если есть свободный слот — принять его. Вызывать в loop(). */
void bridgeAcceptClient(void);

/** Прочитать накопленные данные из всех подключённых TCP-клиентов (и BT) и записать их в SerialUART к автопилоту. Вызывать в loop(). */
void bridgePollNetworkToUart(void);

/** Отправить байты (с UART от автопилота) во все активные каналы: всем TCP-клиентам, UDP-клиенту, при BT — Bluetooth. Вызывать после mavlinkProcessBytes(). Параметры: data — указатель на буфер, len — число байт. */
void bridgeSendUartToNetwork(const uint8_t* data, uint16_t len);

/** Счётчики для веб-интерфейса: сколько байт прошло через мост (для метрик на странице статуса). */
extern uint32_t bridgeBytesTxNetwork;   /* Всего байт отправлено в сеть (TCP/UDP/BT). */
extern uint32_t bridgeBytesRxNetwork;   /* Всего байт принято из сети и записано в UART. */
extern uint32_t bridgeBytesFromUart;    /* Всего байт принято с UART (от автопилота). */

/** Возвращает число подключённых TCP-клиентов (0..MAX_NMEA_CLIENTS). */
uint8_t bridgeGetTcpConnectedCount(void);
/** Возвращает 0 или 1 — есть ли зарегистрированный UDP-клиент (тот, кто уже отправил данные). */
uint8_t bridgeGetUdpClientCount(void);

/** Сбросить известный UDP-клиент. Вызывается из веб-обработчика DELETE /api/settings/clients/clear_udp. */
void bridgeClearUdpClient(void);

/** Задать UDP-клиента вручную по IP и порту (например из веб POST /api/settings/clients/udp с JSON {"ip":"192.168.2.100","port":14550}). */
void bridgeSetUdpClient(IPAddress ip, uint16_t port);

/** Вызвать в loop(): отключённые TCP-клиенты удаляются из слотов; UDP-клиент сбрасывается, если долго не было пакетов (таймаут). */
void bridgePollDisconnects(void);

/** Записать в buf строку "IP:port" текущего UDP-клиента. Возвращает true, если клиент известен; bufSize — размер буфера. */
bool bridgeGetUdpClientInfo(char* buf, size_t bufSize);

#endif /* BRIDGE_H */
