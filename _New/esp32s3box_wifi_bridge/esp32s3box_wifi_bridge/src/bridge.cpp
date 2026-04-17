/**
 * bridge.cpp — асинхронный мост «сеть (AsyncTCP/AsyncUDP) ↔ UART».
 *
 * НАЗНАЧЕНИЕ:
 *   Неблокирующая пересылка MAVLink между GCS (Mission Planner по TCP, QGC по UDP) и UART автопилота.
 *   UART-task (см. main.cpp) читает SerialUART, вызывает mavlinkProcessBytes() и bridgeSendUartToNetwork().
 *   AsyncTCP обслуживается своим таском (CONFIG_ASYNC_TCP_RUNNING_CORE=0); все колбеки выполняются в нём.
 *
 * КРИТИЧЕСКИЙ ИНВАРИАНТ:
 *   Запись в AsyncClient (add/send) допустима ТОЛЬКО из контекста AsyncTCP task (или в защищённом сечении).
 *   Поэтому uart-task НЕ пишет напрямую в AsyncClient, а складывает данные в StreamBuffer слота;
 *   AsyncTCP дренирует буфер в onAck/onPoll/через notifyFromISR.
 *
 * ПОТОКОБЕЗОПАСНОСТЬ:
 *   — bytesIn/bytesOut/dropBytes — std::atomic.
 *   — active/peer/connectedMs пишутся только в AsyncTCP колбеках (accept/disconnect) и читаются в веб-обработчиках
 *     с защитой через portMUX_TYPE.
 */
#include <Arduino.h>
#include "config.h"
#include "bridge.h"
#include "bridge_log.h"
#include "esp_log.h"

#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>
#include <freertos/queue.h>

#if defined(PROTOCOL_TCP)
#include <AsyncTCP.h>
#endif
#if defined(PROTOCOL_UDP)
#include <AsyncUDP.h>
#endif

extern HardwareSerial SerialUART;  /* Объявлен в main.cpp. */

/* ========== Атомарные счётчики ========== */
std::atomic<uint64_t> uartBytesRx{0};
std::atomic<uint64_t> uartBytesTx{0};
std::atomic<uint32_t> uartOverruns{0};
std::atomic<uint64_t> netBytesToGcs{0};
std::atomic<uint64_t> netBytesFromGcs{0};
std::atomic<uint32_t> tcpConnectsTotal{0};
std::atomic<uint32_t> tcpDisconnectsTotal{0};
std::atomic<uint32_t> apAssocTotal{0};
std::atomic<uint32_t> apDisassocTotal{0};
std::atomic<uint32_t> staReconnectsTotal{0};

/* ========== TCP слоты ========== */
#if defined(PROTOCOL_TCP)
struct TcpSlot {
    AsyncClient*             client;         /* nullptr когда слот свободен. Менять только в AsyncTCP-task. */
    StreamBufferHandle_t     txBuf;          /* данные UART->GCS для этого клиента, ждут отправки */
    std::atomic<uint64_t>    bytesIn;        /* от GCS → в UART */
    std::atomic<uint64_t>    bytesOut;       /* UART → этот клиент */
    std::atomic<uint32_t>    dropBytes;      /* переполнение txBuf */
    uint32_t                 connectedAtMs;
    char                     peer[32];
};
static TcpSlot g_slots[MAX_NMEA_CLIENTS];
static AsyncServer* g_server = nullptr;
/* portMUX_TYPE для мини-секций вокруг записи peer/active поля. Атомики используют свои примитивы. */
static portMUX_TYPE g_slotMux = portMUX_INITIALIZER_UNLOCKED;
static std::atomic<uint8_t> g_tcpActiveCount{0};

/** Вернуть индекс слота для данного AsyncClient* (в AsyncTCP task). -1 если не найден. */
static int findSlotByClient(AsyncClient* c) {
    for (int i = 0; i < MAX_NMEA_CLIENTS; i++)
        if (g_slots[i].client == c) return i;
    return -1;
}

/** Слить всё, что можно, из txBuf слота в AsyncClient. Вызывается из AsyncTCP-task. */
static void flushSlot(TcpSlot* slot) {
    if (!slot || !slot->client || !slot->client->connected()) return;
    if (!slot->txBuf) return;
    /* Пока есть место в TCP-сокете и данные в буфере — переливаем. */
    while (slot->client->canSend()) {
        size_t space = slot->client->space();
        if (space < TCP_CLIENT_SPACE_MIN) break;
        size_t available = xStreamBufferBytesAvailable(slot->txBuf);
        if (available == 0) break;
        size_t chunk = space < available ? space : available;
        /* Ограничиваем кусок: избегаем больших выделений стека. */
        uint8_t tmp[1024];
        size_t toRead = chunk < sizeof(tmp) ? chunk : sizeof(tmp);
        size_t got = xStreamBufferReceive(slot->txBuf, tmp, toRead, 0);
        if (got == 0) break;
        size_t added = slot->client->add((const char*)tmp, got, ASYNC_WRITE_FLAG_COPY);
        if (added < got) {
            /* Неожиданно — возвращаем обратно часть; AsyncTCP внутренне положит в свой mbuf. */
            /* В этом проекте add() обычно принимает всё, что помещается в space(). */
        }
        slot->bytesOut.fetch_add((uint64_t)added, std::memory_order_relaxed);
        netBytesToGcs.fetch_add((uint64_t)added, std::memory_order_relaxed);
    }
    slot->client->send();
}

/** onDisconnect: освобождает слот, лог. */
static void onTcpDisconnect(void* arg, AsyncClient* c) {
    (void)arg;
    int idx = findSlotByClient(c);
    if (idx < 0) {
        /* Неизвестный клиент — всё равно удалим объект. */
        delete c;
        return;
    }
    TcpSlot* s = &g_slots[idx];
    espLogPrintf("[bridge] TCP client disconnected %s (slot %d)", s->peer, idx);
    tcpDisconnectsTotal.fetch_add(1);
    if (g_tcpActiveCount.load() > 0)
        g_tcpActiveCount.fetch_sub(1);
    portENTER_CRITICAL(&g_slotMux);
    s->client = nullptr;
    s->peer[0] = '\0';
    portEXIT_CRITICAL(&g_slotMux);
    if (s->txBuf) xStreamBufferReset(s->txBuf);
    delete c;
}

static void onTcpError(void* arg, AsyncClient* c, int8_t error) {
    (void)arg;
    espLogPrintf("[bridge] TCP error %d on %s", (int)error, c ? c->remoteIP().toString().c_str() : "?");
}

static void onTcpTimeout(void* arg, AsyncClient* c, uint32_t time) {
    (void)arg; (void)time;
    if (c) c->close();
}

/** onData: данные от GCS → пишем в UART напрямую. Контекст AsyncTCP-task, SerialUART.write thread-safe. */
static void onTcpData(void* arg, AsyncClient* c, void* data, size_t len) {
    (void)arg;
    int idx = findSlotByClient(c);
    if (len == 0) return;
    netBytesFromGcs.fetch_add((uint64_t)len, std::memory_order_relaxed);
    if (idx >= 0)
        g_slots[idx].bytesIn.fetch_add((uint64_t)len, std::memory_order_relaxed);
    /* SerialUART.write блокирует только если TX-буфер переполнен; выставлен 8 КБ. */
    uartBytesTx.fetch_add((uint64_t)len, std::memory_order_relaxed);
    bridgeLogSetLastTx((const uint8_t*)data, (uint16_t)(len > 0xFFFF ? 0xFFFF : len));
    SerialUART.write((const uint8_t*)data, len);
}

static void onTcpAck(void* arg, AsyncClient* c, size_t len, uint32_t time) {
    (void)arg; (void)len; (void)time;
    int idx = findSlotByClient(c);
    if (idx >= 0) flushSlot(&g_slots[idx]);
}

static void onTcpPoll(void* arg, AsyncClient* c) {
    (void)arg;
    int idx = findSlotByClient(c);
    if (idx >= 0) flushSlot(&g_slots[idx]);
}

static void onNewTcpClient(void* arg, AsyncClient* c) {
    (void)arg;
    if (!c) return;
    /* Найти свободный слот. */
    int free_idx = -1;
    for (int i = 0; i < MAX_NMEA_CLIENTS; i++) {
        if (!g_slots[i].client) { free_idx = i; break; }
    }
    if (free_idx < 0) {
        espLogPrintf("[bridge] TCP reject (no free slots) from %s", c->remoteIP().toString().c_str());
        c->close(true);
        delete c;
        return;
    }
    TcpSlot* s = &g_slots[free_idx];
    if (!s->txBuf) {
        s->txBuf = xStreamBufferCreate(TCP_SLOT_TX_BUFFER, 1);
        if (!s->txBuf) {
            espLogPrintf("[bridge] TCP slot %d: no heap for stream buffer", free_idx);
            c->close(true);
            delete c;
            return;
        }
    } else {
        xStreamBufferReset(s->txBuf);
    }
    portENTER_CRITICAL(&g_slotMux);
    s->client = c;
    s->connectedAtMs = millis();
    snprintf(s->peer, sizeof(s->peer), "%s:%u",
             c->remoteIP().toString().c_str(), (unsigned)c->remotePort());
    portEXIT_CRITICAL(&g_slotMux);
    s->bytesIn.store(0);
    s->bytesOut.store(0);
    s->dropBytes.store(0);

    c->setNoDelay(true);
    c->setRxTimeout(60); /* секунды — если клиент молчит долго, закрыть */
    c->onData(onTcpData, nullptr);
    c->onDisconnect(onTcpDisconnect, nullptr);
    c->onError(onTcpError, nullptr);
    c->onTimeout(onTcpTimeout, nullptr);
    c->onAck(onTcpAck, nullptr);
    c->onPoll(onTcpPoll, nullptr);

    tcpConnectsTotal.fetch_add(1);
    g_tcpActiveCount.fetch_add(1);
    espLogPrintf("[bridge] TCP client connected %s (slot %d)", s->peer, free_idx);
}
#endif /* PROTOCOL_TCP */

/* ========== UDP ========== */
#if defined(PROTOCOL_UDP)
static AsyncUDP g_udp;
static IPAddress g_udpPeer;
static uint16_t g_udpPeerPort = 0;
static std::atomic<bool> g_udpKnown{false};
static std::atomic<uint32_t> g_udpLastMs{0};
static std::atomic<uint64_t> g_udpBytesIn{0};
static std::atomic<uint64_t> g_udpBytesOut{0};
static portMUX_TYPE g_udpMux = portMUX_INITIALIZER_UNLOCKED;

static void handleUdpPacket(AsyncUDPPacket& pkt) {
    size_t n = pkt.length();
    if (n == 0) return;
    bool wasUnknown = !g_udpKnown.load();
    portENTER_CRITICAL(&g_udpMux);
    g_udpPeer = pkt.remoteIP();
    g_udpPeerPort = pkt.remotePort();
    portEXIT_CRITICAL(&g_udpMux);
    g_udpKnown.store(true);
    g_udpLastMs.store(millis());
    g_udpBytesIn.fetch_add((uint64_t)n, std::memory_order_relaxed);
    netBytesFromGcs.fetch_add((uint64_t)n, std::memory_order_relaxed);
    uartBytesTx.fetch_add((uint64_t)n, std::memory_order_relaxed);
    if (wasUnknown) {
        char s[32];
        snprintf(s, sizeof(s), "%s:%u", g_udpPeer.toString().c_str(), (unsigned)g_udpPeerPort);
        espLogPrintf("[bridge] UDP client set %s", s);
    }
    bridgeLogSetLastTx(pkt.data(), (uint16_t)(n > 0xFFFF ? 0xFFFF : n));
    /* Пишем в UART. AsyncUDP колбек крутится в lwip-task; SerialUART.write thread-safe. */
    SerialUART.write(pkt.data(), n);
}
#endif

/* ========== Инициализация ========== */
void bridgeSetup(void) {
#if defined(PROTOCOL_TCP)
    for (int i = 0; i < MAX_NMEA_CLIENTS; i++) {
        g_slots[i].client = nullptr;
        g_slots[i].txBuf = nullptr;
        g_slots[i].peer[0] = '\0';
        g_slots[i].bytesIn.store(0);
        g_slots[i].bytesOut.store(0);
        g_slots[i].dropBytes.store(0);
        g_slots[i].connectedAtMs = 0;
    }
    g_server = new AsyncServer(SERIAL_TCP_PORT);
    g_server->onClient(&onNewTcpClient, nullptr);
    g_server->begin();
    debug.print(F("TCP (async) port "));
    debug.println(SERIAL_TCP_PORT);
#endif

#if defined(PROTOCOL_UDP)
    if (g_udp.listen(SERIAL_UDP_PORT)) {
        g_udp.onPacket(handleUdpPacket);
        debug.print(F("UDP port "));
        debug.println(SERIAL_UDP_PORT);
    } else {
        debug.println(F("UDP listen failed"));
        bridgeLogSetLastError("UDP listen failed");
    }
#endif
}

/* ========== Отправка UART -> сеть ========== */
void bridgeSendUartToNetwork(const uint8_t* data, uint16_t len) {
    if (!data || len == 0) return;
    uartBytesRx.fetch_add((uint64_t)len, std::memory_order_relaxed);

#if defined(PROTOCOL_TCP)
    for (int i = 0; i < MAX_NMEA_CLIENTS; i++) {
        TcpSlot* s = &g_slots[i];
        if (!s->client || !s->txBuf) continue;
        /* Пытаемся положить в стрим-буфер без блокировки. */
        size_t sent = xStreamBufferSend(s->txBuf, data, len, 0);
        if (sent < len) {
            uint32_t dropped = (uint32_t)(len - sent);
            s->dropBytes.fetch_add(dropped, std::memory_order_relaxed);
            /* Не убиваем клиента — возможно, временно медленный; считаем как drop. */
        }
        /* Не flushSlot() — это делает AsyncTCP-task в onAck/onPoll.
         * Но можно триггернуть уведомление send() через _send из сетевого таска — это запланировано отдельно. */
    }
#endif

#if defined(PROTOCOL_UDP)
    if (g_udpKnown.load()) {
        IPAddress ip;
        uint16_t port;
        portENTER_CRITICAL(&g_udpMux);
        ip = g_udpPeer;
        port = g_udpPeerPort;
        portEXIT_CRITICAL(&g_udpMux);
        size_t n = g_udp.writeTo(data, len, ip, port);
        g_udpBytesOut.fetch_add((uint64_t)n, std::memory_order_relaxed);
        netBytesToGcs.fetch_add((uint64_t)n, std::memory_order_relaxed);
    }
    /* ВАЖНО: без broadcast когда клиент неизвестен — иначе шумим в сеть впустую. */
#endif
}

/* ========== Поллинг таймаутов ========== */
void bridgePollDisconnects(void) {
#if defined(PROTOCOL_UDP)
    if (g_udpKnown.load()) {
        uint32_t last = g_udpLastMs.load();
        if (last != 0 && (millis() - last) > UDP_CLIENT_TIMEOUT_MS) {
            IPAddress ip;
            uint16_t port;
            portENTER_CRITICAL(&g_udpMux);
            ip = g_udpPeer;
            port = g_udpPeerPort;
            portEXIT_CRITICAL(&g_udpMux);
            espLogPrintf("[bridge] UDP client timeout %s:%u",
                         ip.toString().c_str(), (unsigned)port);
            bridgeClearUdpClient();
        }
    }
#endif
}

/* ========== Геттеры ========== */
uint8_t bridgeGetTcpConnectedCount(void) {
#if defined(PROTOCOL_TCP)
    return g_tcpActiveCount.load();
#else
    return 0;
#endif
}

uint8_t bridgeGetUdpClientCount(void) {
#if defined(PROTOCOL_UDP)
    return g_udpKnown.load() ? 1 : 0;
#else
    return 0;
#endif
}

bool bridgeGetTcpSlotInfo(uint8_t idx, bridge_tcp_slot_info_t* out) {
    if (!out || idx >= MAX_NMEA_CLIENTS) return false;
    out->active = false;
    out->peer[0] = '\0';
    out->connectedMs = 0;
    out->bytesIn = 0; out->bytesOut = 0; out->dropBytes = 0;
    out->queueUsed = 0; out->queueSize = 0;
#if defined(PROTOCOL_TCP)
    TcpSlot* s = &g_slots[idx];
    if (!s->client) return false;
    out->active = true;
    portENTER_CRITICAL(&g_slotMux);
    strncpy(out->peer, s->peer, sizeof(out->peer) - 1);
    out->peer[sizeof(out->peer) - 1] = '\0';
    out->connectedMs = s->connectedAtMs ? (millis() - s->connectedAtMs) : 0;
    portEXIT_CRITICAL(&g_slotMux);
    out->bytesIn = s->bytesIn.load();
    out->bytesOut = s->bytesOut.load();
    out->dropBytes = s->dropBytes.load();
    out->queueUsed = s->txBuf ? xStreamBufferBytesAvailable(s->txBuf) : 0;
    out->queueSize = TCP_SLOT_TX_BUFFER;
    return true;
#else
    (void)idx;
    return false;
#endif
}

void bridgeGetUdpInfo(bridge_udp_info_t* out) {
    if (!out) return;
    out->known = false;
    out->peer[0] = '\0';
    out->lastPacketMs = 0;
    out->bytesIn = 0;
    out->bytesOut = 0;
#if defined(PROTOCOL_UDP)
    if (!g_udpKnown.load()) return;
    out->known = true;
    IPAddress ip;
    uint16_t port;
    portENTER_CRITICAL(&g_udpMux);
    ip = g_udpPeer;
    port = g_udpPeerPort;
    portEXIT_CRITICAL(&g_udpMux);
    snprintf(out->peer, sizeof(out->peer), "%s:%u", ip.toString().c_str(), (unsigned)port);
    uint32_t last = g_udpLastMs.load();
    out->lastPacketMs = last ? (millis() - last) : 0;
    out->bytesIn = g_udpBytesIn.load();
    out->bytesOut = g_udpBytesOut.load();
#endif
}

bool bridgeGetUdpClientInfo(char* buf, size_t bufSize) {
#if defined(PROTOCOL_UDP)
    if (!g_udpKnown.load() || !buf || bufSize < 2) return false;
    IPAddress ip;
    uint16_t port;
    portENTER_CRITICAL(&g_udpMux);
    ip = g_udpPeer;
    port = g_udpPeerPort;
    portEXIT_CRITICAL(&g_udpMux);
    snprintf(buf, bufSize, "%s:%u", ip.toString().c_str(), (unsigned)port);
    return true;
#else
    (void)buf; (void)bufSize; return false;
#endif
}

#if defined(PROTOCOL_UDP)
void bridgeClearUdpClient(void) {
    g_udpKnown.store(false);
    g_udpLastMs.store(0);
    portENTER_CRITICAL(&g_udpMux);
    g_udpPeer = IPAddress((uint32_t)0);
    g_udpPeerPort = 0;
    portEXIT_CRITICAL(&g_udpMux);
}

void bridgeSetUdpClient(IPAddress ip, uint16_t port) {
    portENTER_CRITICAL(&g_udpMux);
    g_udpPeer = ip;
    g_udpPeerPort = port ? port : SERIAL_UDP_PORT;
    portEXIT_CRITICAL(&g_udpMux);
    g_udpKnown.store(true);
    g_udpLastMs.store(millis());
    char s[32];
    snprintf(s, sizeof(s), "%s:%u", ip.toString().c_str(), (unsigned)port);
    espLogPrintf("[bridge] UDP client set %s (manual)", s);
}
#else
void bridgeClearUdpClient(void) {}
void bridgeSetUdpClient(IPAddress, uint16_t) {}
#endif

/* bridgeStartUartTask() определён в main.cpp (создаёт task и начинает чтение UART). */
