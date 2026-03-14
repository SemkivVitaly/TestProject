/**
 * bridge.h — API моста «сеть (TCP/UDP/BT) ↔ UART».
 *
 * Вызовы из main: setup() → bridgeSetup(); в loop() — bridgeAcceptClient(),
 * bridgePollNetworkToUart(), затем после чтения из SerialUART — bridgeSendUartToNetwork().
 */
#ifndef BRIDGE_H
#define BRIDGE_H

#include "config.h"
#include <stdint.h>

/** Инициализация TCP-сервера, UDP и при необходимости Bluetooth. Вызывать из setup() после WiFi. */
void bridgeSetup(void);

/** Проверить наличие нового TCP-клиента и принять его в свободный слот. Вызывать в loop(). */
void bridgeAcceptClient(void);

/** Прочитать данные из TCP (и BT) и записать в UART к автопилоту. Вызывать в loop(). */
void bridgePollNetworkToUart(void);

/** Отправить данные (прочитанные с UART) во все активные каналы: TCP, UDP, BT. Вызывать после mavlinkProcessBytes(). */
void bridgeSendUartToNetwork(const uint8_t* data, uint16_t len);

/** Счётчики для веб-интерфейса (метрики канала). */
extern uint32_t bridgeBytesTxNetwork;   /* Всего байт отправлено в сеть. */
extern uint32_t bridgeBytesRxNetwork;   /* Всего байт принято из сети в UART. */
extern uint32_t bridgeBytesFromUart;     /* Всего байт принято с UART (от автопилота). */

uint8_t bridgeGetTcpConnectedCount(void);  /* Число подключённых TCP-клиентов (0..MAX_NMEA_CLIENTS). */
uint8_t bridgeGetUdpClientCount(void);      /* 0 или 1 — есть ли зарегистрированный UDP-клиент. */

/** Сбросить известный UDP-клиент (ответ на DELETE /api/settings/clients/clear_udp). */
void bridgeClearUdpClient(void);

/** Задать UDP-клиента вручную по IP и порту (POST /api/settings/clients/udp с телом {"ip":"x.y.z.w","port":14550}). */
void bridgeSetUdpClient(IPAddress ip, uint16_t port);

/** Вызвать в loop(): отключённые TCP-клиенты очищаются; UDP-клиент сбрасывается после таймаута без пакетов. */
void bridgePollDisconnects(void);

/** Записать в buf строку "IP:port" текущего UDP-клиента (если есть). Возвращает true, если клиент известен. */
bool bridgeGetUdpClientInfo(char* buf, size_t bufSize);

#endif /* BRIDGE_H */
