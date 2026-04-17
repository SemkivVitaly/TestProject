/**
 * web_handlers.cpp — HTTP-обработчики на AsyncWebServer (неблокирующие).
 *
 * МАРШРУТЫ:
 *   — /                        → handleRoot   — главная с ссылками.
 *   — /params                  → handleParamsPage — страница SERVO-параметров.
 *   — /bridge                  → handleBridgePage — Bridge UI (PROGMEM или LittleFS).
 *   — /api/status              → JSON статуса (uptime, heap, chip_temp, connected, MAVLink счётчики, байт-счётчики).
 *   — /api/link                → JSON канала (пакеты/потери/HB).
 *   — /api/clients             → JSON по TCP/UDP клиентам (слоты, очереди).
 *   — /api/system/info         → метаданные (совместимость с существующим UI).
 *   — /api/system/stats        → RSSI, байты, клиенты, chip_temp.
 *   — /api/params GET/POST     → чтение / установка SERVO (POST — x-www-form-urlencoded).
 *   — /api/param_request       → отправить PARAM_REQUEST_READ.
 *   — /api/settings GET/POST   → конфиг Wi‑Fi/UART; POST с JSON-body, валидация, ребут.
 *   — /api/settings/clients/udp POST           → задать UDP-клиента JSON {"ip":"..","port":...}.
 *   — /api/settings/clients/clear_udp DELETE   → сбросить UDP-клиента.
 *   — /api/log /api/log/file /api/log/esp32    → логи.
 *   — /<имя>.png               → иконки с LittleFS.
 *
 * JSON собирается в char[] через snprintf — никакого String::operator+=, не фрагментируется heap.
 */
#include "config.h"
#ifdef WEB_SERVER

#include <Arduino.h>
#include <cmath>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_heap_caps.h>
#include "bridge.h"
#if __has_include("bridge_ui_embed.h")
#include "bridge_ui_embed.h"
#define BRIDGE_UI_EMBED_H
#endif
#include "bridge_nvs.h"
#include "mavlink_state.h"
#include "bridge_log.h"
#include "esp_log.h"
#include "web_handlers.h"

/** Экземпляр сервера (объявлен в этом модуле; не экспортируем в заголовке). */
static AsyncWebServer* s_server = nullptr;

/** Вспомогательные extern-функции, определённые в main.cpp. */
extern "C" void bridgeRequestRestart(uint32_t reason);
void bridgeGetRssiStats(int8_t* now, int8_t* mn, int8_t* mx, int8_t* avg);

static const char* const kBridgePngPaths[] = {
    "/add_16dp_icon.png",
    "/remove_16dp_icon.png",
    "/DroneBridgeLogo.png",
    "/favicon-32x32.png",
    "/favicon-16x16.png",
    "/apple-touch-icon.png",
};
static const size_t kBridgePngCount = sizeof(kBridgePngPaths) / sizeof(kBridgePngPaths[0]);

/** Добавить Cache-Control: no-store к ответу (для /api/*). */
static void addNoStore(AsyncWebServerResponse* r) {
    if (r) r->addHeader("Cache-Control", "no-store");
}

/* ========== Главные HTML страницы ========== */

static const char PROGMEM kRootHtml[] =
    "<!DOCTYPE html><html><head><meta charset='utf-8'><title>ESP32-S3-Box Bridge</title>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<style>"
    "body{font-family:'Segoe UI',Roboto,Helvetica,Arial,sans-serif;margin:0;padding:0;background:linear-gradient(135deg,#001f3f 0%,#0074d9 100%);color:#fff;display:flex;justify-content:center;align-items:center;min-height:100vh;}"
    ".container{background:rgba(0,0,0,0.4);padding:2.5rem;border-radius:16px;box-shadow:0 10px 30px rgba(0,0,0,0.5);text-align:center;max-width:400px;width:90%;backdrop-filter:blur(10px);}"
    "h1{margin-top:0;font-size:2.2rem;font-weight:300;margin-bottom:2rem;letter-spacing:1px;}"
    "a{display:block;background:#33c3f0;color:#fff;text-decoration:none;padding:14px 20px;margin:12px 0;border-radius:8px;font-weight:600;font-size:1.1rem;transition:all 0.3s ease;box-shadow:0 4px 6px rgba(0,0,0,0.2);}"
    "a:hover{background:#1eaedb;transform:translateY(-2px);box-shadow:0 6px 12px rgba(0,0,0,0.3);}"
    ".log-links{display:flex;gap:10px;margin-top:12px;} .log-links a{flex:1;margin:0;font-size:0.95rem;background:rgba(255,255,255,0.1);border:1px solid rgba(255,255,255,0.2);}"
    ".log-links a:hover{background:rgba(255,255,255,0.2);}"
    "</style></head><body>"
    "<div class='container'>"
    "<h1>ESP32-S3 Bridge</h1>"
    "<a href='/bridge' style='background:#ff9734;'>Настройки Bridge UI</a>"
    "<a href='/params'>Управление SERVO</a>"
    "<div class='log-links'>"
    "<a href='/api/log/file'>Единый лог</a>"
    "<a href='/api/log'>JSON Пакеты</a>"
    "<a href='/api/log/esp32'>Лог ESP32</a>"
    "</div>"
    "</div></body></html>";

static void handleRoot(AsyncWebServerRequest* req) {
    AsyncWebServerResponse* r = req->beginResponse_P(200, "text/html; charset=utf-8", kRootHtml);
    req->send(r);
}

/* Страница SERVO-параметров: обновляется раз в WEB_POLL_MIN_MS мс, новые подписи потерь. */
static const char PROGMEM kParamsHtml[] =
    "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Параметры SERVO</title>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<style>"
    "body{font-family:'Segoe UI',Roboto,sans-serif;margin:0;padding:20px;background:linear-gradient(135deg,#001f3f 0%,#0074d9 100%);color:#fff;display:flex;justify-content:center;align-items:center;min-height:100vh;box-sizing:border-box;}"
    ".container{background:rgba(0,0,0,0.4);padding:2.5rem;border-radius:16px;box-shadow:0 10px 30px rgba(0,0,0,0.5);max-width:560px;width:100%;backdrop-filter:blur(10px);}"
    "h1{margin-top:0;font-size:1.8rem;font-weight:300;text-align:center;margin-bottom:1.5rem;}"
    "#conn{text-align:center;margin-bottom:1.5rem;background:rgba(0,0,0,0.3);padding:12px;border-radius:8px;font-size:0.95rem;border:1px solid rgba(255,255,255,0.1);}"
    "table{width:100%;border-collapse:collapse;margin-bottom:1.5rem;background:rgba(255,255,255,0.05);border-radius:8px;overflow:hidden;}"
    "th,td{padding:12px 15px;text-align:left;border-bottom:1px solid rgba(255,255,255,0.1);}"
    "th{background:rgba(0,0,0,0.3);font-weight:600;font-size:0.9rem;text-transform:uppercase;letter-spacing:1px;}"
    "input[type=number]{width:100%;padding:10px;border:1px solid rgba(255,255,255,0.2);border-radius:6px;background:rgba(255,255,255,0.9);color:#000;box-sizing:border-box;font-family:inherit;font-size:1rem;transition:border 0.3s;}"
    "input[type=number]:focus{outline:none;border-color:#ff9734;}"
    "button{background:#ff9734;color:#fff;border:none;padding:14px 20px;border-radius:8px;cursor:pointer;font-weight:600;font-size:1rem;width:100%;transition:all 0.3s ease;box-shadow:0 4px 6px rgba(0,0,0,0.2);margin-bottom:12px;}"
    "button:hover{background:#e6862b;transform:translateY(-2px);box-shadow:0 6px 12px rgba(0,0,0,0.3);}"
    "button.secondary{background:transparent;border:2px solid #ff9734;color:#ff9734;box-shadow:none;}"
    "button.secondary:hover{background:rgba(255,151,52,0.1);}"
    ".ok{color:#68b838;font-weight:bold;} .no{color:#f63e3e;font-weight:bold;}"
    ".nav{display:flex;justify-content:center;gap:15px;margin-top:1rem;}"
    ".nav a{color:#33c3f0;text-decoration:none;font-size:0.9rem;font-weight:500;}"
    "</style></head><body>"
    "<div class='container'>"
    "<h1>Параметры MAVLink</h1>"
    "<div id='conn'>Ожидание данных...</div>"
    "<button type='button' class='secondary' onclick=\"window.paramsFrozen=false; fetch('/api/param_request');setTimeout(load,500);\">Запросить с автопилота</button>"
    "<form method='post' action='/api/params' onsubmit='setTimeout(function(){window.location.reload();},500);'>"
    "<table><tr><th>Параметр</th><th>Значение</th><th style='text-align:center;'>Получен</th></tr>"
    "<tr><td>SERVO1_REVERSED</td><td><input name='SERVO1_REVERSED' id='v1' type='number' step='0.01'></td><td id='k1' style='text-align:center;'>—</td></tr>"
    "<tr><td>SERVO3_TRIM</td><td><input name='SERVO3_TRIM' id='v2' type='number' step='0.01'></td><td id='k2' style='text-align:center;'>—</td></tr>"
    "<tr><td>SERVO4_TRIM</td><td><input name='SERVO4_TRIM' id='v3' type='number' step='0.01'></td><td id='k3' style='text-align:center;'>—</td></tr>"
    "</table><button type='submit'>Установить значения</button></form>"
    "<div class='nav'><a href='/'>&larr; На главную</a><a href='/api/status'>JSON Статус</a><a href='/api/log/file'>Текст лог</a></div>"
    "</div>"
    "<script>"
    "function load(){"
    "var c=new AbortController();var to=setTimeout(function(){c.abort();},1800);"
    "fetch('/api/status',{signal:c.signal}).then(function(r){clearTimeout(to);return r.json();}).then(function(j){"
    "var lossTxt=(j.mavlink_rx_lost||0)+' (' + ((j.mavlink_loss_pct||0).toFixed?j.mavlink_loss_pct.toFixed(2):j.mavlink_loss_pct) + '%)';"
    "document.getElementById('conn').innerHTML='Связь: '+(j.connected?'<span class=ok>Активна</span>':'<span class=no>Нет</span>')+"
    "'<br><span style=\"font-size:0.85rem;color:#ccc;display:inline-block;margin-top:6px;\">'+"
    "'MAVLink RX: '+(j.mavlink_rx_pkts||0)+' пкт | Потери (seq): '+lossTxt+' | Parse err: '+(j.mavlink_parse_err||0)+"
    "'<br>Период HB: '+(j.heartbeat_interval_ms||'—')+' мс | С последн. HEARTBEAT: '+(j.connected?j.heartbeat_age_ms+' мс':'—')+"
    "' | Bridge TX: '+(j.mavlink_bridge_tx_pkts||0)+' пкт</span>';"
    "if(!window.paramsFrozen){document.getElementById('v1').value=j.SERVO1_REVERSED; document.getElementById('v2').value=j.SERVO3_TRIM; document.getElementById('v3').value=j.SERVO4_TRIM;}"
    "document.getElementById('k1').innerHTML=j.SERVO1_REVERSED_known?'<span class=ok>OK</span>':'—';"
    "document.getElementById('k2').innerHTML=j.SERVO3_TRIM_known?'<span class=ok>OK</span>':'—';"
    "document.getElementById('k3').innerHTML=j.SERVO4_TRIM_known?'<span class=ok>OK</span>':'—';"
    "if(j.SERVO1_REVERSED_known&&j.SERVO3_TRIM_known&&j.SERVO4_TRIM_known)window.paramsFrozen=true;"
    "}).catch(function(e){clearTimeout(to);});"
    "}"
    "load(); setInterval(function(){if(!document.hidden)load();},2000);"
    "</script></body></html>";

static void handleParamsPage(AsyncWebServerRequest* req) {
    AsyncWebServerResponse* r = req->beginResponse_P(200, "text/html; charset=utf-8", kParamsHtml);
    req->send(r);
}

/* ========== /api/status: основной JSON со всеми счётчиками (новая модель) ========== */
static void handleApiStatus(AsyncWebServerRequest* req) {
    /* Соберём в статическом буфере. Размер прикинут с запасом под MAVLINK_LOG. */
    const size_t BUF_SZ = 3072;
    char* buf = (char*)malloc(BUF_SZ);
    if (!buf) {
        req->send(503, "text/plain", "oom");
        return;
    }
    size_t p = 0;

    /* MAVLink реальные потери (по seq-gap) и parse_error отдельно. */
    uint64_t rx   = mavlinkRxPkts.load();
    uint64_t lost = mavlinkRxLost.load();
    uint64_t perr = mavlinkParseErr.load();
    uint64_t btx  = mavlinkBridgeTxPkts.load();
    float lossPct = (rx + lost > 0) ? 100.0f * (float)lost / (float)(rx + lost) : 0.0f;

    uint32_t hbAge = 0;
    if (mavlinkConnected && lastHeartbeatMs != 0) {
        uint32_t now = millis();
        hbAge = (now >= lastHeartbeatMs) ? (now - lastHeartbeatMs) : 0;
    }

    char lastErr[64];
    bridgeLogGetLastError(lastErr, sizeof(lastErr));

    float tc = temperatureRead();
    int8_t rssiNow, rssiMin, rssiMax, rssiAvg;
    bridgeGetRssiStats(&rssiNow, &rssiMin, &rssiMax, &rssiAvg);

    p += snprintf(buf + p, BUF_SZ - p,
        "{\"uptime\":%lu,"
        "\"free_heap\":%u,"
        "\"min_free_heap\":%u,"
        "\"largest_free_block\":%u,",
        (unsigned long)(millis() / 1000),
        (unsigned)ESP.getFreeHeap(),
        (unsigned)ESP.getMinFreeHeap(),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    if (std::isfinite(static_cast<double>(tc)))
        p += snprintf(buf + p, BUF_SZ - p, "\"chip_temp_c\":%.1f,", (double)tc);
    else
        p += snprintf(buf + p, BUF_SZ - p, "\"chip_temp_c\":null,");

    p += snprintf(buf + p, BUF_SZ - p,
        "\"connected\":%s,"
        "\"tcp_connected\":%u,"
        "\"udp_known\":%s,"
        "\"last_error\":\"%s\","
        "\"last_heartbeat_ms\":%lu,"
        "\"heartbeat_age_ms\":%lu,"
        "\"heartbeat_interval_ms\":%lu,"
        "\"latency_ms\":%lu,"
        "\"mavlink_rx_pkts\":%llu,"
        "\"mavlink_rx_lost\":%llu,"
        "\"mavlink_parse_err\":%llu,"
        "\"mavlink_bridge_tx_pkts\":%llu,"
        "\"mavlink_loss_pct\":%.2f,"
        /* Совместимость со старым UI (чтобы не ломать скрипты): */
        "\"packets_rx\":%llu,"
        "\"packets_tx\":%llu,"
        "\"packet_drops\":%llu,"
        "\"packet_loss_pct\":%.2f,"
        "\"packets_processed\":%llu,"
        "\"uart_bytes_rx\":%llu,"
        "\"uart_bytes_tx\":%llu,"
        "\"uart_overruns\":%u,"
        "\"net_bytes_to_gcs\":%llu,"
        "\"net_bytes_from_gcs\":%llu,"
        "\"tcp_connects_total\":%u,"
        "\"tcp_disconnects_total\":%u,"
        "\"sta_reconnects_total\":%u,"
        "\"ap_assoc_total\":%u,"
        "\"ap_disassoc_total\":%u,"
        "\"rssi_now\":%d,"
        "\"rssi_min\":%d,"
        "\"rssi_max\":%d,"
        "\"rssi_avg\":%d,"
        /* для совместимости с api/status старого UI: */
        "\"bytes_network_tx\":%llu,"
        "\"bytes_network_rx\":%llu,"
        "\"SERVO1_REVERSED\":%.6f,\"SERVO1_REVERSED_known\":%s,"
        "\"SERVO3_TRIM\":%.6f,\"SERVO3_TRIM_known\":%s,"
        "\"SERVO4_TRIM\":%.6f,\"SERVO4_TRIM_known\":%s",
        mavlinkConnected ? "true" : "false",
        (unsigned)bridgeGetTcpConnectedCount(),
        (bridgeGetUdpClientCount() > 0) ? "true" : "false",
        lastErr,
        (unsigned long)lastHeartbeatMs,
        (unsigned long)hbAge,
        (unsigned long)mavlinkHeartbeatIntervalMs,
        (unsigned long)hbAge,
        (unsigned long long)rx,
        (unsigned long long)lost,
        (unsigned long long)perr,
        (unsigned long long)btx,
        (double)lossPct,
        (unsigned long long)rx,
        (unsigned long long)btx,
        (unsigned long long)lost,
        (double)lossPct,
        (unsigned long long)rx,
        (unsigned long long)uartBytesRx.load(),
        (unsigned long long)uartBytesTx.load(),
        (unsigned)uartOverruns.load(),
        (unsigned long long)netBytesToGcs.load(),
        (unsigned long long)netBytesFromGcs.load(),
        (unsigned)tcpConnectsTotal.load(),
        (unsigned)tcpDisconnectsTotal.load(),
        (unsigned)staReconnectsTotal.load(),
        (unsigned)apAssocTotal.load(),
        (unsigned)apDisassocTotal.load(),
        (int)rssiNow, (int)rssiMin, (int)rssiMax, (int)rssiAvg,
        (unsigned long long)netBytesToGcs.load(),
        (unsigned long long)netBytesFromGcs.load(),
        (double)paramServo1Revers, paramServo1ReversKnown ? "true" : "false",
        (double)paramServo3Trim,   paramServo3TrimKnown   ? "true" : "false",
        (double)paramServo4Trim,   paramServo4TrimKnown   ? "true" : "false");

    /* MAVLink log tail. */
    if (p + 10 < BUF_SZ) {
        p += snprintf(buf + p, BUF_SZ - p, ",\"log\":[");
        bool first = true;
        for (uint8_t n = 0; n < MAVLINK_LOG_SIZE; n++) {
            uint8_t idx = (mavlinkLogHead + n) % MAVLINK_LOG_SIZE;
            if (mavlinkLog[idx][0] == '\0') continue;
            if (p + MAVLINK_LOG_ENTRY_LEN + 4 >= BUF_SZ) break;
            if (!first) buf[p++] = ',';
            first = false;
            buf[p++] = '"';
            for (size_t k = 0; k < MAVLINK_LOG_ENTRY_LEN && p + 2 < BUF_SZ; k++) {
                char ch = mavlinkLog[idx][k];
                if (ch == '\0') break;
                if (ch == '"' || ch == '\\') buf[p++] = '\\';
                buf[p++] = ch;
            }
            buf[p++] = '"';
        }
        if (p + 2 < BUF_SZ) buf[p++] = ']';
    }
    if (p + 1 < BUF_SZ) buf[p++] = '}';
    buf[p] = '\0';

    AsyncWebServerResponse* r = req->beginResponse(200, "application/json", buf);
    addNoStore(r);
    req->send(r);
    free(buf);
}

/* ========== /api/link ========== */
static void handleApiLink(AsyncWebServerRequest* req) {
    uint64_t rx   = mavlinkRxPkts.load();
    uint64_t lost = mavlinkRxLost.load();
    uint64_t perr = mavlinkParseErr.load();
    uint64_t btx  = mavlinkBridgeTxPkts.load();
    float lossPct = (rx + lost > 0) ? 100.0f * (float)lost / (float)(rx + lost) : 0.0f;
    uint32_t hbAge = (mavlinkConnected && lastHeartbeatMs) ? (millis() - lastHeartbeatMs) : 0;

    char buf[768];
    int p = snprintf(buf, sizeof(buf),
        "{\"connected\":%s,"
        "\"packets_sent\":%llu,"
        "\"packets_received\":%llu,"
        "\"packets_processed\":%llu,"
        "\"packet_drops\":%llu,"
        "\"packet_loss_pct\":%.2f,"
        "\"mavlink_parse_err\":%llu,"
        "\"heartbeat_age_ms\":%lu,"
        "\"heartbeat_interval_ms\":%lu,"
        "\"latency_ms\":%lu,"
        "\"bytes_network_tx\":%llu,"
        "\"bytes_network_rx\":%llu}",
        mavlinkConnected ? "true" : "false",
        (unsigned long long)btx,
        (unsigned long long)rx,
        (unsigned long long)rx,
        (unsigned long long)lost,
        (double)lossPct,
        (unsigned long long)perr,
        (unsigned long)hbAge,
        (unsigned long)mavlinkHeartbeatIntervalMs,
        (unsigned long)hbAge,
        (unsigned long long)netBytesToGcs.load(),
        (unsigned long long)netBytesFromGcs.load());
    (void)p;
    AsyncWebServerResponse* r = req->beginResponse(200, "application/json", buf);
    addNoStore(r);
    req->send(r);
}

/* ========== /api/clients: TCP slots + UDP ========== */
static void handleApiClients(AsyncWebServerRequest* req) {
    const size_t BUF_SZ = 1536;
    char* buf = (char*)malloc(BUF_SZ);
    if (!buf) { req->send(503, "text/plain", "oom"); return; }
    size_t p = 0;
    p += snprintf(buf + p, BUF_SZ - p, "{\"tcp\":[");
    bool first = true;
    for (uint8_t i = 0; i < MAX_NMEA_CLIENTS; i++) {
        bridge_tcp_slot_info_t si;
        if (!bridgeGetTcpSlotInfo(i, &si) || !si.active) continue;
        if (!first) { if (p + 2 < BUF_SZ) buf[p++] = ','; }
        first = false;
        p += snprintf(buf + p, BUF_SZ - p,
            "{\"slot\":%u,\"peer\":\"%s\",\"connected_ms\":%u,"
            "\"bytes_in\":%llu,\"bytes_out\":%llu,\"drop_bytes\":%u,"
            "\"queue_used\":%u,\"queue_size\":%u}",
            (unsigned)i, si.peer, (unsigned)si.connectedMs,
            (unsigned long long)si.bytesIn, (unsigned long long)si.bytesOut,
            (unsigned)si.dropBytes, (unsigned)si.queueUsed, (unsigned)si.queueSize);
    }
    p += snprintf(buf + p, BUF_SZ - p, "],\"udp\":");
    bridge_udp_info_t ui;
    bridgeGetUdpInfo(&ui);
    if (ui.known) {
        p += snprintf(buf + p, BUF_SZ - p,
            "{\"peer\":\"%s\",\"last_packet_ms\":%u,\"bytes_in\":%llu,\"bytes_out\":%llu}",
            ui.peer, (unsigned)ui.lastPacketMs,
            (unsigned long long)ui.bytesIn, (unsigned long long)ui.bytesOut);
    } else {
        p += snprintf(buf + p, BUF_SZ - p, "null");
    }
    if (p + 1 < BUF_SZ) buf[p++] = '}';
    buf[p] = '\0';
    AsyncWebServerResponse* r = req->beginResponse(200, "application/json", buf);
    addNoStore(r);
    req->send(r);
    free(buf);
}

/* ========== /api/params ========== */
static void handleParamsGet(AsyncWebServerRequest* req) {
    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"SERVO1_REVERSED\":%.6f,\"SERVO1_REVERSED_known\":%s,"
        "\"SERVO3_TRIM\":%.6f,\"SERVO3_TRIM_known\":%s,"
        "\"SERVO4_TRIM\":%.6f,\"SERVO4_TRIM_known\":%s}",
        (double)paramServo1Revers, paramServo1ReversKnown ? "true" : "false",
        (double)paramServo3Trim,   paramServo3TrimKnown   ? "true" : "false",
        (double)paramServo4Trim,   paramServo4TrimKnown   ? "true" : "false");
    AsyncWebServerResponse* r = req->beginResponse(200, "application/json", buf);
    addNoStore(r);
    req->send(r);
}

static void handleParamsSet(AsyncWebServerRequest* req) {
    bool sent = false;
    const AsyncWebParameter* p;
    if ((p = req->getParam("SERVO1_REVERSED", true))) {
        mavlinkSendParamSet("SERVO1_REVERSED", p->value().toFloat());
        sent = true;
    }
    if ((p = req->getParam("SERVO3_TRIM", true))) {
        mavlinkSendParamSet("SERVO3_TRIM", p->value().toFloat());
        sent = true;
    }
    if ((p = req->getParam("SERVO4_TRIM", true))) {
        mavlinkSendParamSet("SERVO4_TRIM", p->value().toFloat());
        sent = true;
    }
    if (sent) req->send(200, "application/json", "{\"ok\":true}");
    else      req->send(400, "application/json",
                        "{\"ok\":false,\"error\":\"need SERVO1_REVERSED, SERVO3_TRIM or SERVO4_TRIM\"}");
}

static void handleParamRequest(AsyncWebServerRequest* req) {
    mavlinkRequestServoParams();
    req->send(200, "application/json", "{\"ok\":true}");
}

/* ========== /bridge page ========== */
static void handleBridgePage(AsyncWebServerRequest* req) {
#ifdef BRIDGE_UI_EMBED_H
    AsyncWebServerResponse* r = req->beginResponse_P(200, "text/html; charset=utf-8", (const uint8_t*)BRIDGE_UI_HTML, strlen_P((const char*)BRIDGE_UI_HTML));
    req->send(r);
#else
    if (!LittleFS.exists("/index.html")) {
        req->send(404, "text/plain", "Интерфейс Bridge не найден. Загрузите ФС: pio run -t uploadfs.");
        return;
    }
    req->send(LittleFS, "/index.html", "text/html; charset=utf-8");
#endif
}

/* ========== /api/system/info, /api/system/stats (совместимость с UI) ========== */
static void handleApiSystemInfo(AsyncWebServerRequest* req) {
    char uid[64];
    bridgeLogGetUniqueId(uid, sizeof(uid));
    char buf[384];
    snprintf(buf, sizeof(buf),
        "{\"major_version\":2,\"minor_version\":1,\"patch_version\":0,"
        "\"maturity_version\":\"Bridge\",\"idf_version\":\"arduino\","
        "\"esp_chip_model\":9,"
        "\"esp_mac\":\"%s\","          /* MAC текущего интерфейса (может быть переписан). */
        "\"chip_uid\":\"%s\","         /* Истинный заводской ID чипа (eFuse). */
        "\"serial_via_JTAG\":0,\"has_rf_switch\":0}",
        WiFi.macAddress().c_str(), uid);
    AsyncWebServerResponse* r = req->beginResponse(200, "application/json", buf);
    addNoStore(r);
    req->send(r);
}

static void handleApiSystemStats(AsyncWebServerRequest* req) {
    int8_t rssiNow, rssiMin, rssiMax, rssiAvg;
    bridgeGetRssiStats(&rssiNow, &rssiMin, &rssiMax, &rssiAvg);
    bridgeLogUpdateStats(
        (uint32_t)(mavlinkBridgeTxPkts.load() & 0xFFFFFFFFU),
        (uint32_t)(mavlinkRxPkts.load() & 0xFFFFFFFFU),
        (uint32_t)(mavlinkRxLost.load() & 0xFFFFFFFFU),
        (uint32_t)((mavlinkRxPkts.load() + mavlinkRxLost.load()) & 0xFFFFFFFFU));
    bridgeLogUpdateRssi(rssiNow);

    char udpInfo[32];
    bool hasUdp = bridgeGetUdpClientInfo(udpInfo, sizeof(udpInfo));

    char buf[768];
    int p = 0;
    p += snprintf(buf + p, sizeof(buf) - p,
        "{\"read_bytes\":%llu,"
        "\"serial_dec_mav_msgs\":%llu,"
        "\"tcp_connected\":%u,"
        "\"udp_connected\":%u,"
        "\"udp_clients\":",
        (unsigned long long)uartBytesRx.load(),
        (unsigned long long)mavlinkRxPkts.load(),
        (unsigned)bridgeGetTcpConnectedCount(),
        (unsigned)bridgeGetUdpClientCount());
    if (hasUdp)
        p += snprintf(buf + p, sizeof(buf) - p, "[\"%s\"]", udpInfo);
    else
        p += snprintf(buf + p, sizeof(buf) - p, "[]");
    p += snprintf(buf + p, sizeof(buf) - p,
        ",\"current_client_ip\":\"%s\",\"esp_rssi\":%d,\"rssi_min\":%d,\"rssi_max\":%d,\"rssi_avg\":%d",
        hasUdp ? udpInfo : "", (int)rssiNow, (int)rssiMin, (int)rssiMax, (int)rssiAvg);
    float tc = temperatureRead();
    if (std::isfinite(static_cast<double>(tc)))
        p += snprintf(buf + p, sizeof(buf) - p, ",\"chip_temp_c\":%.1f}", (double)tc);
    else
        p += snprintf(buf + p, sizeof(buf) - p, ",\"chip_temp_c\":null}");
    (void)p;
    AsyncWebServerResponse* r = req->beginResponse(200, "application/json", buf);
    addNoStore(r);
    req->send(r);
}

/* ========== /api/settings GET ========== */
static void handleApiSettingsGet(AsyncWebServerRequest* req) {
    const bridge_nvs_config_t& c = bridge_nvs_config;
    char buf[768];
    snprintf(buf, sizeof(buf),
        "{\"esp32_mode\":%u,\"ssid\":\"%s\",\"wifi_pass\":\"%s\","
        "\"wifi_chan\":%u,\"ap_ip\":\"%s\","
        "\"wifi_en_gn\":1,\"radio_dis_onarm\":0,\"ant_use_ext\":0,"
        "\"ip_sta\":\"\",\"ip_sta_gw\":\"\",\"ip_sta_netmsk\":\"\","
        "\"wifi_hostname\":\"%s\","
        "\"gpio_tx\":%d,\"gpio_rx\":%d,\"gpio_rts\":0,\"gpio_cts\":0,\"rts_thresh\":64,"
        "\"proto\":%u,\"baud\":%u,"
        "\"ltm_per_packet\":1,\"trans_pack_size\":128,\"serial_timeout\":50,\"rep_rssi_dbm\":0}",
        (unsigned)c.wifi_mode, c.ssid, c.wifi_pass,
        (unsigned)c.wifi_chan, c.ap_ip, c.hostname,
        (int)c.gpio_tx, (int)c.gpio_rx,
        (unsigned)c.proto, (unsigned)c.baud);
    AsyncWebServerResponse* r = req->beginResponse(200, "application/json", buf);
    addNoStore(r);
    req->send(r);
}

/* Состояние JSON-body для POST /api/settings и /api/settings/clients/udp (собираем по чанкам). */
struct PostJsonCtx {
    char* buf;
    size_t cap;
    size_t len;
    bool overflow;
};

static void postJsonInit(AsyncWebServerRequest* req, size_t cap) {
    PostJsonCtx* ctx = new PostJsonCtx{};
    ctx->buf = (char*)malloc(cap);
    ctx->cap = cap;
    ctx->len = 0;
    ctx->overflow = false;
    req->_tempObject = ctx;
}
static void postJsonAppend(AsyncWebServerRequest* req, uint8_t* data, size_t len) {
    PostJsonCtx* ctx = (PostJsonCtx*)req->_tempObject;
    if (!ctx) return;
    if (ctx->len + len + 1 > ctx->cap) { ctx->overflow = true; return; }
    memcpy(ctx->buf + ctx->len, data, len);
    ctx->len += len;
    ctx->buf[ctx->len] = '\0';
}
static void postJsonFree(AsyncWebServerRequest* req) {
    PostJsonCtx* ctx = (PostJsonCtx*)req->_tempObject;
    if (!ctx) return;
    if (ctx->buf) free(ctx->buf);
    delete ctx;
    req->_tempObject = nullptr;
}

/* ========== /api/settings POST: тело — JSON с настройками. ========== */
static void handleApiSettingsPostDone(AsyncWebServerRequest* req) {
    PostJsonCtx* ctx = (PostJsonCtx*)req->_tempObject;
    if (!ctx || ctx->overflow || ctx->len == 0) {
        postJsonFree(req);
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"body required\"}");
        return;
    }
    bool ok = setBridgeConfigFromJson(ctx->buf);
    postJsonFree(req);
    if (!ok) {
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid json or values\"}");
        return;
    }
    req->send(200, "application/json", "{\"msg\":\"Настройки сохранены. Перезагрузка...\"}");
    /* restart после небольшой задержки, чтобы ответ успел уйти. */
    static uint32_t s_restartAtMs = 0;
    s_restartAtMs = millis() + 400;
    /* планируем бегом в async_tcp task — но проще задача-сторож. */
    xTaskCreate([](void*) {
        uint32_t target = s_restartAtMs;
        while ((int32_t)(target - millis()) > 0) vTaskDelay(20 / portTICK_PERIOD_MS);
        bridgeRequestRestart(3 /* RESTART_REASON_USER_SETTINGS */);
        vTaskDelete(nullptr);
    }, "restartdelay", 2048, nullptr, 1, nullptr);
}

/* ========== /api/settings/clients/udp POST ========== */
static void handleApiUdpSetDone(AsyncWebServerRequest* req) {
    PostJsonCtx* ctx = (PostJsonCtx*)req->_tempObject;
    if (!ctx || ctx->overflow || ctx->len == 0) {
        postJsonFree(req);
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"body required\"}");
        return;
    }
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, ctx->buf, ctx->len);
    postJsonFree(req);
    if (err) {
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid json\"}");
        return;
    }
    const char* ipStr = doc["udp_client_ip"] | doc["ip"] | nullptr;
    uint16_t port = (uint16_t)(doc["udp_client_port"] | doc["port"] | SERIAL_UDP_PORT);
    if (!ipStr || !ipStr[0]) {
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"ip required\"}");
        return;
    }
    IPAddress ip;
    if (!ip.fromString(ipStr)) {
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"bad ip\"}");
        return;
    }
    bridgeSetUdpClient(ip, port);
    req->send(200, "application/json", "{\"msg\":\"UDP-клиент задан.\"}");
}

/* ========== /api/settings/clients/clear_udp DELETE ========== */
static void handleApiUdpClear(AsyncWebServerRequest* req) {
    bridgeClearUdpClient();
    req->send(200, "application/json", "{\"msg\":\"UDP-клиент сброшен.\"}");
}

/* ========== Логи ========== */
static void handleApiLog(AsyncWebServerRequest* req) {
    const size_t BUF_SZ = 2048;
    char* buf = (char*)malloc(BUF_SZ);
    if (!buf) { req->send(503, "text/plain", "oom"); return; }
    size_t p = 0;
    buf[p++] = '[';
    bool first = true;
    for (uint8_t n = 0; n < MAVLINK_LOG_SIZE; n++) {
        uint8_t idx = (mavlinkLogHead + n) % MAVLINK_LOG_SIZE;
        if (mavlinkLog[idx][0] == '\0') continue;
        if (p + MAVLINK_LOG_ENTRY_LEN + 4 >= BUF_SZ) break;
        if (!first) buf[p++] = ',';
        first = false;
        buf[p++] = '"';
        for (size_t k = 0; k < MAVLINK_LOG_ENTRY_LEN && p + 2 < BUF_SZ; k++) {
            char ch = mavlinkLog[idx][k];
            if (ch == '\0') break;
            if (ch == '"' || ch == '\\') buf[p++] = '\\';
            buf[p++] = ch;
        }
        buf[p++] = '"';
    }
    if (p + 1 < BUF_SZ) buf[p++] = ']';
    buf[p] = '\0';
    AsyncWebServerResponse* r = req->beginResponse(200, "application/json", buf);
    addNoStore(r);
    req->send(r);
    free(buf);
}

static void handleApiLogFile(AsyncWebServerRequest* req) {
    constexpr size_t kBufSize = 3072;
    char* buf = (char*)malloc(kBufSize);
    if (!buf) { req->send(503, "text/plain", "oom"); return; }
    bridgeLogGetText(buf, kBufSize);
    AsyncWebServerResponse* r = req->beginResponse(200, "text/plain; charset=utf-8", buf);
    addNoStore(r);
    req->send(r);
    free(buf);
}

static void handleApiLogEsp32(AsyncWebServerRequest* req) {
    constexpr size_t kBufSize = ESP_LOG_SIZE + 64;
    char* buf = (char*)malloc(kBufSize);
    if (!buf) { req->send(503, "text/plain", "oom"); return; }
    espLogGetText(buf, kBufSize);
    AsyncWebServerResponse* r = req->beginResponse(200, "text/plain; charset=utf-8", buf);
    addNoStore(r);
    req->send(r);
    free(buf);
}

/* ========== Статические PNG из LittleFS ========== */
static void handleBridgePng(AsyncWebServerRequest* req) {
    String uri = req->url();
    bool allowed = false;
    for (size_t i = 0; i < kBridgePngCount; i++) {
        if (uri == kBridgePngPaths[i]) { allowed = true; break; }
    }
    if (!allowed) { req->send(404, "text/plain", "Not found"); return; }
    if (!LittleFS.exists(uri)) {
        req->send(404, "text/plain", "Not on filesystem; run pio run -t uploadfs");
        return;
    }
    req->send(LittleFS, uri, "image/png");
}

/* ========== Настройка маршрутов ========== */
void webSetup(void) {
    if (s_server) return;
    s_server = new AsyncWebServer(WEB_SERVER_PORT);

    s_server->on("/",                   HTTP_GET, handleRoot);
    s_server->on("/params",             HTTP_GET, handleParamsPage);
    s_server->on("/bridge",             HTTP_GET, handleBridgePage);
    s_server->on("/smd",                HTTP_GET, [](AsyncWebServerRequest* req) {
        AsyncWebServerResponse* r = req->beginResponse(302, "text/plain", "");
        r->addHeader("Location", "/bridge");
        req->send(r);
    });

    s_server->on("/api/status",         HTTP_GET, handleApiStatus);
    s_server->on("/api/link",           HTTP_GET, handleApiLink);
    s_server->on("/api/clients",        HTTP_GET, handleApiClients);
    s_server->on("/api/system/info",    HTTP_GET, handleApiSystemInfo);
    s_server->on("/api/system/stats",   HTTP_GET, handleApiSystemStats);

    s_server->on("/api/params",         HTTP_GET, handleParamsGet);
    s_server->on("/api/params",         HTTP_POST, handleParamsSet);
    s_server->on("/api/param_request",  HTTP_GET, handleParamRequest);
    s_server->on("/api/param_request",  HTTP_POST, handleParamRequest);

    s_server->on("/api/log",            HTTP_GET, handleApiLog);
    s_server->on("/api/log/file",       HTTP_GET, handleApiLogFile);
    s_server->on("/api/log/esp32",      HTTP_GET, handleApiLogEsp32);

    s_server->on("/api/settings",       HTTP_GET, handleApiSettingsGet);
    s_server->on("/api/settings",       HTTP_POST,
        [](AsyncWebServerRequest* req) { handleApiSettingsPostDone(req); },
        nullptr, /* onUpload */
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index == 0) postJsonInit(req, total > 0 && total < 4096 ? total + 16 : 2048);
            postJsonAppend(req, data, len);
        });

    s_server->on("/api/settings/clients/udp", HTTP_POST,
        [](AsyncWebServerRequest* req) { handleApiUdpSetDone(req); },
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index == 0) postJsonInit(req, total > 0 && total < 512 ? total + 16 : 256);
            postJsonAppend(req, data, len);
        });

    s_server->on("/api/settings/clients/clear_udp", HTTP_DELETE, handleApiUdpClear);

    for (size_t i = 0; i < kBridgePngCount; i++)
        s_server->on(kBridgePngPaths[i], HTTP_GET, handleBridgePng);

    s_server->onNotFound([](AsyncWebServerRequest* req) {
        req->send(404, "text/plain", "Not found");
    });

    s_server->begin();
}

#endif /* WEB_SERVER */
