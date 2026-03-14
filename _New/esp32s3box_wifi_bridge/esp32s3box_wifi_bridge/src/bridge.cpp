/**
 * bridge.cpp — мост «сеть ↔ UART».
 *
 * Назначение:
 *   Пересылка байтов между Wi‑Fi (TCP/UDP) или Bluetooth и последовательным
 *   портом к автопилоту. Mission Planner подключается по TCP (порт 8880)
 *   или UDP (14550); данные без изменений проходят в обе стороны. Счётчики
 *   байт используются веб-интерфейсом для метрик (отправлено/получено).
 */
#include <Arduino.h>
#include "config.h"
#include "bridge.h"
#include "bridge_log.h"
#include "esp_log.h"

extern HardwareSerial SerialUART;

#if defined(PROTOCOL_TCP)
#include <WiFiClient.h>
static WiFiServer tcpServer(SERIAL_TCP_PORT);
static WiFiClient tcpClients[MAX_NMEA_CLIENTS];
#endif

#if defined(PROTOCOL_UDP)
#include <AsyncUDP.h>
static AsyncUDP udp;
static IPAddress udpFromIP;
static uint16_t udpFromPort = 0;
static bool udpClientKnown = false;  /* Есть ли известный UDP-клиент (GCS отправил хотя бы один пакет). */
static uint32_t lastUdpPacketMs = 0; /* Время последнего UDP-пакета для авто-отключения. */
#define UDP_CLIENT_TIMEOUT_MS 30000  /* После этого времени без пакетов считаем клиента отключённым. */
#endif

#ifdef BLUETOOTH
#include <BluetoothSerial.h>
static BluetoothSerial SerialBT;
static uint8_t bufBT[BUFFERSIZE];
static uint16_t lenBT = 0;
#endif

static uint8_t bufToUART[BUFFERSIZE];
static uint16_t lenToUART = 0;

/* Счётчики для веб-интерфейса (метрики канала). */
uint32_t bridgeBytesTxNetwork = 0;  /* Всего байт отправлено в сеть (TCP/UDP/BT). */
uint32_t bridgeBytesRxNetwork = 0;  /* Всего байт принято из сети и записано в UART. */
uint32_t bridgeBytesFromUart = 0;   /* Всего байт принято с UART (от автопилота). */

void bridgeSetup(void) {
#if defined(PROTOCOL_TCP)
    tcpServer.begin();
    tcpServer.setNoDelay(true);
    debug.print(F("TCP port "));
    debug.println(SERIAL_TCP_PORT);
#endif

#if defined(PROTOCOL_UDP)
    if (udp.listen(SERIAL_UDP_PORT)) {
        udp.onPacket([](AsyncUDPPacket pkt) {
            if (pkt.length() > 0) {
                bool wasUnknown = !udpClientKnown;
                udpFromIP = pkt.remoteIP();
                udpFromPort = pkt.remotePort();
                udpClientKnown = true;
                lastUdpPacketMs = millis();
                if (wasUnknown)
                    espLogPrintf("[bridge] UDP client set %s:%u", udpFromIP.toString().c_str(), (unsigned)udpFromPort);
                bridgeBytesRxNetwork += pkt.length();
                SerialUART.write(pkt.data(), pkt.length());
            }
        });
        debug.print(F("UDP port "));
        debug.println(SERIAL_UDP_PORT);
    } else {
        debug.println(F("UDP listen failed"));
        bridgeLogSetLastError("UDP listen failed");
    }
#endif

#ifdef BLUETOOTH
    SerialBT.begin(SSID);
    debug.println(F("BT on"));
#endif
}

/** Принять нового TCP-клиента (например Mission Planner), если слот свободен. */
void bridgeAcceptClient(void) {
#if defined(PROTOCOL_TCP)
    if (!tcpServer.hasClient())
        return;
    WiFiClient c = tcpServer.accept();
    if (!c)
        return;
    bool placed = false;
    for (byte i = 0; i < MAX_NMEA_CLIENTS; i++) {
        if (!tcpClients[i] || !tcpClients[i].connected()) {
            if (tcpClients[i])
                tcpClients[i].stop();
            tcpClients[i] = c;
            placed = true;
            debug.print(F("TCP client "));
            debug.println(c.remoteIP());
            espLogPrintf("[bridge] TCP client set %s:%u", c.remoteIP().toString().c_str(), (unsigned)c.remotePort());
            break;
        }
    }
    if (!placed)
        c.stop();
#endif
}

/** Читать данные из TCP (и при необходимости BT) и записать в UART к автопилоту. */
void bridgePollNetworkToUart(void) {
#if defined(PROTOCOL_TCP)
    for (byte i = 0; i < MAX_NMEA_CLIENTS; i++) {
        if (!tcpClients[i] || !tcpClients[i].available())
            continue;
        lenToUART = 0;
        while (tcpClients[i].available() && lenToUART < BUFFERSIZE - 1)
            bufToUART[lenToUART++] = tcpClients[i].read();
        if (lenToUART > 0) {
            bridgeBytesRxNetwork += lenToUART;
            SerialUART.write(bufToUART, lenToUART);
        }
    }
#endif

#ifdef BLUETOOTH
    if (SerialBT.hasClient()) {
        lenBT = 0;
        while (SerialBT.available() && lenBT < BUFFERSIZE - 1)
            bufBT[lenBT++] = SerialBT.read();
        if (lenBT > 0) {
            bridgeBytesRxNetwork += lenBT;
            SerialUART.write(bufBT, lenBT);
        }
    }
#endif
}

uint8_t bridgeGetTcpConnectedCount(void) {
#if defined(PROTOCOL_TCP)
    uint8_t n = 0;
    for (byte i = 0; i < MAX_NMEA_CLIENTS; i++)
        if (tcpClients[i] && tcpClients[i].connected())
            n++;
    return n;
#else
    return 0;
#endif
}

uint8_t bridgeGetUdpClientCount(void) {
#if defined(PROTOCOL_UDP)
    return udpClientKnown ? 1 : 0;
#else
    return 0;
#endif
}

/** Очистка отключённых TCP-клиентов; сброс UDP-клиента по таймауту. */
void bridgePollDisconnects(void) {
#if defined(PROTOCOL_TCP)
    for (byte i = 0; i < MAX_NMEA_CLIENTS; i++) {
        if (tcpClients[i] && !tcpClients[i].connected()) {
            String ip = tcpClients[i].remoteIP().toString();
            tcpClients[i].stop();
            tcpClients[i] = WiFiClient();
            espLogPrintf("[bridge] TCP client disconnected %s", ip.c_str());
        }
    }
#endif
#if defined(PROTOCOL_UDP)
    if (udpClientKnown && lastUdpPacketMs != 0 && (millis() - lastUdpPacketMs) > UDP_CLIENT_TIMEOUT_MS) {
        espLogPrintf("[bridge] UDP client timeout %s:%u", udpFromIP.toString().c_str(), (unsigned)udpFromPort);
        udpClientKnown = false;
        udpFromPort = 0;
        udpFromIP = IPAddress((uint32_t)0);
    }
#endif
}

#if defined(PROTOCOL_UDP)
void bridgeClearUdpClient(void) {
    udpClientKnown = false;
    udpFromPort = 0;
    udpFromIP = IPAddress((uint32_t)0);
}
void bridgeSetUdpClient(IPAddress ip, uint16_t port) {
    udpFromIP = ip;
    udpFromPort = port ? port : SERIAL_UDP_PORT;
    udpClientKnown = true;
    lastUdpPacketMs = millis();
    espLogPrintf("[bridge] UDP client set %s:%u", udpFromIP.toString().c_str(), (unsigned)udpFromPort);
}

bool bridgeGetUdpClientInfo(char* buf, size_t bufSize) {
    if (!udpClientKnown || bufSize < 2) return false;
    snprintf(buf, bufSize, "%s:%u", udpFromIP.toString().c_str(), (unsigned)udpFromPort);
    return true;
}
#else
void bridgeClearUdpClient(void) {}
void bridgeSetUdpClient(IPAddress ip, uint16_t port) { (void)ip; (void)port; }
bool bridgeGetUdpClientInfo(char* buf, size_t bufSize) { (void)buf; (void)bufSize; return false; }
#endif

/** Отправить данные (с UART от автопилота) во все активные каналы: TCP, UDP, при необходимости BT.
 *  flush() по TCP обеспечивает немедленную отправку телеметрии в GCS без задержки буфера. */
void bridgeSendUartToNetwork(const uint8_t* data, uint16_t len) {
    bridgeLogSetLastTx(data, len);
    bridgeBytesTxNetwork += len;
    bridgeBytesFromUart += len;
#if defined(PROTOCOL_TCP)
    for (byte i = 0; i < MAX_NMEA_CLIENTS; i++) {
        if (tcpClients[i] && tcpClients[i].connected()) {
            tcpClients[i].write(data, len);
            tcpClients[i].flush();
        }
    }
#endif

#if defined(PROTOCOL_UDP)
    if (udpClientKnown)
        udp.writeTo(data, len, udpFromIP, udpFromPort);
    else
        udp.broadcastTo(const_cast<uint8_t*>(data), (size_t)len, (uint16_t)SERIAL_UDP_PORT);
#endif

#ifdef BLUETOOTH
    if (SerialBT.hasClient())
        SerialBT.write(data, len);
#endif
}
