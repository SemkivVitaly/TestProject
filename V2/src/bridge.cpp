/**
 * bridge.cpp — мост: сеть (TCP/UDP/BT) ↔ UART.
 */
#include <Arduino.h>
#include "config.h"
#include "bridge.h"

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
static bool udpClientKnown = false;
#endif

#ifdef BLUETOOTH
#include <BluetoothSerial.h>
static BluetoothSerial SerialBT;
static uint8_t bufBT[BUFFERSIZE];
static uint16_t lenBT = 0;
#endif

static uint8_t bufToUART[BUFFERSIZE];
static uint16_t lenToUART = 0;

uint32_t bridgeBytesTxNetwork = 0;
uint32_t bridgeBytesRxNetwork = 0;
uint32_t bridgeBytesFromUart = 0;

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
                udpFromIP = pkt.remoteIP();
                udpFromPort = pkt.remotePort();
                udpClientKnown = true;
                bridgeBytesRxNetwork += pkt.length();
                SerialUART.write(pkt.data(), pkt.length());
            }
        });
        debug.print(F("UDP port "));
        debug.println(SERIAL_UDP_PORT);
    } else {
        debug.println(F("UDP listen failed"));
    }
#endif

#ifdef BLUETOOTH
    SerialBT.begin(SSID);
    debug.println(F("BT on"));
#endif
}

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
            break;
        }
    }
    if (!placed)
        c.stop();
#endif
}

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
}
#else
void bridgeClearUdpClient(void) {}
void bridgeSetUdpClient(IPAddress ip, uint16_t port) { (void)ip; (void)port; }
#endif

/** Отправка телеметрии в сеть; flush() — чтобы данные сразу уходили в GCS, без задержки буфера. */
void bridgeSendUartToNetwork(const uint8_t* data, uint16_t len) {
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
