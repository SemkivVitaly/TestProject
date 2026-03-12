/**
 * bridge.h — мост Wi‑Fi (TCP/UDP/BT) ↔ UART.
 */
#ifndef BRIDGE_H
#define BRIDGE_H

#include "config.h"
#include <stdint.h>

void bridgeSetup(void);
void bridgeAcceptClient(void);
void bridgePollNetworkToUart(void);
void bridgeSendUartToNetwork(const uint8_t* data, uint16_t len);

extern uint32_t bridgeBytesTxNetwork;
extern uint32_t bridgeBytesRxNetwork;
extern uint32_t bridgeBytesFromUart;
uint8_t bridgeGetTcpConnectedCount(void);
uint8_t bridgeGetUdpClientCount(void);

/** Сбросить известный UDP-клиент (ответ на DELETE /api/settings/clients/clear_udp). */
void bridgeClearUdpClient(void);

/** Задать UDP-клиента вручную по IP и порту (POST /api/settings/clients/udp с телом {"ip":"x.y.z.w","port":14550}). */
void bridgeSetUdpClient(IPAddress ip, uint16_t port);

#endif /* BRIDGE_H */
