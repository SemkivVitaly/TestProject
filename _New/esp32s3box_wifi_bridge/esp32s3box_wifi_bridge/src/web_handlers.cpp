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
    "html{-webkit-text-size-adjust:100%;text-size-adjust:100%;font-size:clamp(14px,0.35rem+2.1vw,18px);}"
    "body{font-family:'Segoe UI',Roboto,Helvetica,Arial,sans-serif;margin:0;padding:clamp(12px,4vw,28px);background:linear-gradient(135deg,#001f3f 0%,#0074d9 100%);color:#fff;display:flex;justify-content:center;align-items:center;min-height:100vh;box-sizing:border-box;}"
    ".container{background:rgba(0,0,0,0.4);padding:clamp(1.1rem,4vw,2.5rem);border-radius:16px;box-shadow:0 10px 30px rgba(0,0,0,0.5);text-align:center;max-width:min(440px,100%);width:100%;backdrop-filter:blur(10px);box-sizing:border-box;}"
    "h1{margin-top:0;font-size:clamp(1.35rem,0.9rem+1.8vw,2.2rem);font-weight:300;margin-bottom:clamp(1rem,3vw,2rem);letter-spacing:1px;}"
    "a{display:block;background:#33c3f0;color:#fff;text-decoration:none;padding:clamp(10px,2.5vw,14px) clamp(14px,4vw,22px);margin:clamp(8px,2vw,12px) 0;border-radius:8px;font-weight:600;font-size:clamp(0.92rem,0.75rem+0.7vw,1.1rem);transition:all 0.3s ease;box-shadow:0 4px 6px rgba(0,0,0,0.2);min-height:clamp(44px,10vw,52px);line-height:1.3;touch-action:manipulation;}"
    "a:hover{background:#1eaedb;transform:translateY(-2px);box-shadow:0 6px 12px rgba(0,0,0,0.3);}"
    ".log-links{display:flex;flex-wrap:wrap;gap:clamp(6px,2vw,10px);margin-top:12px;} .log-links a{flex:1;min-width:min(140px,100%);margin:0;font-size:clamp(0.8rem,0.65rem+0.5vw,0.92rem);background:rgba(255,255,255,0.1);border:1px solid rgba(255,255,255,0.2);}"
    ".log-links a:hover{background:rgba(255,255,255,0.2);}"
    ".raw{font-size:clamp(0.72rem,0.6rem+0.4vw,0.78rem);color:rgba(255,255,255,0.55);margin-top:1.2rem;}"
    ".raw a{display:inline;background:none;padding:0;margin:0 8px;color:#7fd1ff;font-weight:400;box-shadow:none;text-decoration:underline;}"
    "</style></head><body>"
    "<div class='container'>"
    "<h1>ESP32-S3 Bridge</h1>"
    "<a href='/bridge' style='background:#ff9734;'>Настройки Bridge UI</a>"
    "<a href='/params'>Управление SERVO</a>"
    "<div class='log-links'>"
    "<a href='/log'>Единый лог</a>"
    "<a href='/esp32log'>Лог ESP32</a>"
    "</div>"
    "<div class='raw'>RAW API: <a href='/api/log/file'>log/file</a><a href='/api/log/esp32'>log/esp32</a><a href='/api/log'>log</a></div>"
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
    "html{-webkit-text-size-adjust:100%;text-size-adjust:100%;font-size:clamp(14px,0.35rem+2.1vw,18px);}"
    "body{font-family:'Segoe UI',Roboto,sans-serif;margin:0;padding:clamp(12px,4vw,24px);background:linear-gradient(135deg,#001f3f 0%,#0074d9 100%);color:#fff;display:flex;justify-content:center;align-items:center;min-height:100vh;box-sizing:border-box;}"
    ".container{background:rgba(0,0,0,0.4);padding:clamp(1.1rem,3.5vw,2.5rem);border-radius:16px;box-shadow:0 10px 30px rgba(0,0,0,0.5);max-width:min(560px,100%);width:100%;backdrop-filter:blur(10px);box-sizing:border-box;}"
    "h1{margin-top:0;font-size:clamp(1.25rem,0.85rem+1.4vw,1.8rem);font-weight:300;text-align:center;margin-bottom:1.5rem;}"
    "#conn{text-align:center;margin-bottom:1.5rem;background:rgba(0,0,0,0.3);padding:clamp(10px,2.5vw,14px);border-radius:8px;font-size:clamp(0.85rem,0.7rem+0.5vw,0.95rem);border:1px solid rgba(255,255,255,0.1);}"
    "table{width:100%;border-collapse:collapse;margin-bottom:1.5rem;background:rgba(255,255,255,0.05);border-radius:8px;overflow:hidden;}"
    "th,td{padding:clamp(8px,2vw,14px) clamp(8px,2.5vw,16px);text-align:left;border-bottom:1px solid rgba(255,255,255,0.1);word-wrap:break-word;}"
    "th{background:rgba(0,0,0,0.3);font-weight:600;font-size:clamp(0.75rem,0.6rem+0.5vw,0.9rem);text-transform:uppercase;letter-spacing:1px;}"
    "input[type=number]{width:100%;padding:clamp(8px,2vw,12px);min-height:clamp(2.5rem,7vw,2.85rem);border:1px solid rgba(255,255,255,0.2);border-radius:6px;background:rgba(255,255,255,0.9);color:#000;box-sizing:border-box;font-family:inherit;font-size:clamp(0.9rem,0.75rem+0.5vw,1rem);transition:border 0.3s;}"
    "input[type=number]:focus{outline:none;border-color:#ff9734;}"
    "button{background:#ff9734;color:#fff;border:none;padding:clamp(10px,2.5vw,16px) clamp(14px,4vw,22px);border-radius:8px;cursor:pointer;font-weight:600;font-size:clamp(0.9rem,0.75rem+0.5vw,1rem);width:100%;min-height:clamp(44px,9vw,52px);transition:all 0.3s ease;box-shadow:0 4px 6px rgba(0,0,0,0.2);margin-bottom:12px;touch-action:manipulation;}"
    "button:hover{background:#e6862b;transform:translateY(-2px);box-shadow:0 6px 12px rgba(0,0,0,0.3);}"
    "button.secondary{background:transparent;border:2px solid #ff9734;color:#ff9734;box-shadow:none;}"
    "button.secondary:hover{background:rgba(255,151,52,0.1);}"
    ".ok{color:#68b838;font-weight:bold;} .no{color:#f63e3e;font-weight:bold;}"
    ".nav{display:flex;flex-wrap:wrap;justify-content:center;gap:clamp(8px,2vw,15px);margin-top:1rem;}"
    ".nav a{color:#33c3f0;text-decoration:none;font-size:clamp(0.8rem,0.7rem+0.4vw,0.9rem);font-weight:500;}"
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

/* /api/log/samples — последние RX/TX пакеты в hex (для просмотра в UI). */
static void handleApiLogSamples(AsyncWebServerRequest* req) {
    uint8_t raw[64];
    uint16_t rxN = bridgeLogGetLastRxSample(raw, sizeof(raw));
    char rxHex[3 * 64 + 1];
    size_t p = 0;
    for (uint16_t i = 0; i < rxN; i++) p += snprintf(rxHex + p, sizeof(rxHex) - p, "%02X%s", raw[i], (i + 1 < rxN) ? " " : "");
    rxHex[p] = '\0';

    uint16_t txN = bridgeLogGetLastTxSample(raw, sizeof(raw));
    char txHex[3 * 64 + 1];
    p = 0;
    for (uint16_t i = 0; i < txN; i++) p += snprintf(txHex + p, sizeof(txHex) - p, "%02X%s", raw[i], (i + 1 < txN) ? " " : "");
    txHex[p] = '\0';

    char buf[768];
    snprintf(buf, sizeof(buf),
        "{\"rx_len\":%u,\"rx_hex\":\"%s\",\"tx_len\":%u,\"tx_hex\":\"%s\"}",
        (unsigned)rxN, rxHex, (unsigned)txN, txHex);
    AsyncWebServerResponse* r = req->beginResponse(200, "application/json", buf);
    addNoStore(r);
    req->send(r);
}

/* ===================================================================
 * Страница /log  — "Единый лог" (HTML-dashboard, авто-обновление)
 * =================================================================== */
static const char PROGMEM kLogPageHtml[] =
    "<!DOCTYPE html><html lang='ru'><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Единый лог — ESP32 Bridge</title>"
    "<style>"
    "*{box-sizing:border-box}"
    "html{-webkit-text-size-adjust:100%;text-size-adjust:100%;font-size:clamp(14px,0.35rem+2.1vw,18px);}"
    "body{margin:0;padding:clamp(10px,3.5vw,22px);font-family:'Segoe UI',Roboto,sans-serif;color:#fff;"
    "background:linear-gradient(135deg,#001f3f 0%,#0074d9 100%);background-attachment:fixed;min-height:100vh;}"
    ".wrap{max-width:min(1100px,100%);margin:0 auto;}"
    "header{display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;gap:clamp(8px,2vw,14px);margin-bottom:clamp(12px,3vw,18px);}"
    "h1{font-weight:300;font-size:clamp(1.2rem,0.9rem+1.4vw,1.9rem);margin:0;letter-spacing:.5px;}"
    ".nav{display:flex;gap:clamp(6px,1.8vw,10px);flex-wrap:wrap;align-items:center;}"
    ".nav a,.nav button{background:rgba(255,255,255,.08);color:#fff;border:1px solid rgba(255,255,255,.18);"
    "padding:clamp(6px,1.8vw,10px) clamp(8px,2.5vw,16px);border-radius:8px;text-decoration:none;font-size:clamp(0.78rem,0.65rem+0.45vw,0.9rem);"
    "cursor:pointer;font-family:inherit;transition:.2s;min-height:clamp(38px,9vw,46px);touch-action:manipulation;line-height:1.2;}"
    ".nav a:hover,.nav button:hover{background:rgba(255,151,52,.25);border-color:#ff9734;}"
    ".nav a.primary{background:#ff9734;border-color:#ff9734;color:#fff;font-weight:600;}"
    ".nav a.primary:hover{background:#e6862b;}"
    ".nav button.primary{background:#ff9734;border-color:#ff9734;color:#fff;font-weight:600;}"
    ".nav button.primary:hover{background:#e6862b;border-color:#e6862b;}"
    ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(min(100%,280px),1fr));gap:clamp(10px,2.5vw,16px);margin-bottom:clamp(10px,2.5vw,16px);}"
    ".card{background:rgba(0,0,0,.35);border:1px solid rgba(255,255,255,.08);border-radius:12px;"
    "padding:clamp(12px,3vw,20px) clamp(12px,3.2vw,22px);box-shadow:0 6px 20px rgba(0,0,0,.25);backdrop-filter:blur(8px);}"
    ".card h2{margin:0 0 10px 0;font-size:clamp(0.82rem,0.7rem+0.4vw,0.95rem);font-weight:600;text-transform:uppercase;letter-spacing:1px;"
    "color:rgba(255,255,255,.72);border-bottom:1px solid rgba(255,255,255,.08);padding-bottom:8px;}"
    ".kv{display:grid;grid-template-columns:minmax(100px,auto) 1fr;gap:6px 14px;font-size:clamp(0.86rem,0.75rem+0.35vw,0.95rem);}"
    ".kv .k{color:rgba(255,255,255,.65);}"
    ".kv .v{font-weight:500;word-break:break-word;}"
    ".dot{display:inline-block;width:clamp(8px,2vw,10px);height:clamp(8px,2vw,10px);border-radius:50%;margin-right:7px;vertical-align:middle;box-shadow:0 0 6px rgba(0,0,0,.5);}"
    ".d-ok{background:#68b838;}.d-no{background:#f63e3e;}.d-warn{background:#ffc107;}"
    "#tbl_clients{min-width:520px;width:100%;border-collapse:collapse;font-size:clamp(0.78rem,0.65rem+0.35vw,0.88rem);}"
    "th{text-align:left;font-weight:600;padding:6px 8px;border-bottom:1px solid rgba(255,255,255,.15);"
    "color:rgba(255,255,255,.6);text-transform:uppercase;font-size:clamp(0.68rem,0.6rem+0.3vw,0.78rem);letter-spacing:.5px;white-space:nowrap;}"
    "td{padding:8px;border-bottom:1px solid rgba(255,255,255,.06);vertical-align:top;}"
    ".proto-tcp{color:#33c3f0;font-weight:600;} .proto-udp{color:#ff9734;font-weight:600;}"
    ".hex{font-family:Consolas,'Cascadia Mono','Liberation Mono',monospace;font-size:clamp(0.74rem,0.62rem+0.3vw,0.84rem);"
    "background:rgba(0,0,0,.35);padding:8px 10px;border-radius:6px;word-break:break-all;line-height:1.5;"
    "border:1px solid rgba(255,255,255,.06);max-height:clamp(72px,22vw,90px);overflow-y:auto;}"
    ".ev{font-family:Consolas,'Cascadia Mono',monospace;font-size:clamp(0.74rem,0.62rem+0.3vw,0.84rem);"
    "background:rgba(0,0,0,.35);border-radius:6px;padding:8px 10px;max-height:clamp(160px,38vh,220px);overflow-y:auto;white-space:pre-wrap;}"
    ".pill{display:inline-block;padding:2px 9px;border-radius:999px;font-size:clamp(0.7rem,0.6rem+0.25vw,0.8rem);font-weight:600;}"
    ".pill-ok{background:rgba(104,184,56,.18);color:#8fd65b;border:1px solid rgba(104,184,56,.35);}"
    ".pill-no{background:rgba(246,62,62,.15);color:#ff8787;border:1px solid rgba(246,62,62,.35);}"
    ".muted{color:rgba(255,255,255,.55);}"
    ".row2{grid-column:span 2;overflow-x:auto;-webkit-overflow-scrolling:touch;}"
    "@media(max-width:700px){.row2{grid-column:auto;}}"
    "@media(max-width:420px){.kv{grid-template-columns:1fr;}}"
    ".ts{font-size:clamp(0.7rem,0.6rem+0.25vw,0.8rem);color:rgba(255,255,255,.5);margin-left:6px;}"
    "</style></head><body><div class='wrap'>"
    "<header>"
    "<h1>Единый лог <span class='ts' id='ts'>—</span></h1>"
    "<nav class='nav'>"
    "<a href='/'>&larr; Главная</a>"
    "<a href='/bridge'>Настройки</a>"
    "<a href='/esp32log'>Лог ESP32</a>"
    "<button class='primary' type='button' onclick='downloadUnifiedTxt()'>Сохранить .txt</button>"
    "<button onclick='loadAll(true)'>Обновить</button>"
    "</nav></header>"

    "<div class='grid'>"

    "<div class='card'><h2>Соединение MAVLink / MissionPlanner</h2>"
    "<div class='kv'>"
    "<div class='k'>Статус</div><div class='v' id='mav_status'>—</div>"
    "<div class='k'>Последний HEARTBEAT</div><div class='v' id='hb_age'>—</div>"
    "<div class='k'>Интервал HB</div><div class='v' id='hb_int'>—</div>"
    "<div class='k'>Ошибок парсинга</div><div class='v' id='parse_err'>—</div>"
    "<div class='k'>Последняя ошибка</div><div class='v' id='last_err'>—</div>"
    "</div></div>"

    "<div class='card'><h2>Устройство</h2>"
    "<div class='kv'>"
    "<div class='k'>Уникальный ID чипа</div><div class='v' id='chip_uid' style='font-family:Consolas,monospace;font-size:.85rem'>—</div>"
    "<div class='k'>MAC</div><div class='v' id='chip_mac' style='font-family:Consolas,monospace;font-size:.85rem'>—</div>"
    "<div class='k'>Uptime</div><div class='v' id='uptime'>—</div>"
    "<div class='k'>Свободный heap</div><div class='v' id='heap'>—</div>"
    "<div class='k'>Температура чипа</div><div class='v' id='chip_temp'>—</div>"
    "</div></div>"

    "<div class='card'><h2>Wi-Fi / Сигнал</h2>"
    "<div class='kv'>"
    "<div class='k'>RSSI (сейчас)</div><div class='v' id='rssi_now'>—</div>"
    "<div class='k'>Мин / Макс / Средн.</div><div class='v' id='rssi_mma'>—</div>"
    "<div class='k'>AP assoc / disassoc</div><div class='v' id='ap_ev'>—</div>"
    "<div class='k'>STA реконнектов</div><div class='v' id='sta_rc'>—</div>"
    "</div></div>"

    "<div class='card row2'><h2>Подключённые клиенты</h2>"
    "<table id='tbl_clients'><thead><tr>"
    "<th>Протокол</th><th>IP : порт</th><th>Соединение</th><th>Байт в / из ESP</th><th>Очередь</th>"
    "</tr></thead><tbody id='clients_tb'><tr><td class='muted' colspan='5'>Ожидание данных…</td></tr></tbody></table></div>"

    "<div class='card'><h2>Пакеты MAVLink</h2>"
    "<div class='kv'>"
    "<div class='k'>Получено RX</div><div class='v' id='p_rx'>—</div>"
    "<div class='k'>Отправлено в GCS (TX)</div><div class='v' id='p_tx'>—</div>"
    "<div class='k'>Потеряно (seq-gap)</div><div class='v' id='p_lost'>—</div>"
    "<div class='k'>Всего обработано</div><div class='v' id='p_total'>—</div>"
    "<div class='k'>Доля потерь</div><div class='v' id='p_lossp'>—</div>"
    "</div></div>"

    "<div class='card'><h2>UART (автопилот)</h2>"
    "<div class='kv'>"
    "<div class='k'>Байт RX / TX</div><div class='v' id='uart_bytes'>—</div>"
    "<div class='k'>Overruns</div><div class='v' id='uart_or'>—</div>"
    "<div class='k'>Байт в сеть / из сети</div><div class='v' id='net_bytes'>—</div>"
    "<div class='k'>TCP connect / disconnect</div><div class='v' id='tcp_cd'>—</div>"
    "</div></div>"

    "<div class='card'><h2>RX (UART): автопилот → ESP, hex</h2>"
    "<p class='muted' style='font-size:clamp(0.72rem,0.6rem+0.25vw,0.82rem);margin:0 0 8px 0'>Пока нет байт с FC по UART — блок пуст (проверьте TX/RX и baud).</p>"
    "<div class='hex' id='rx_sample'>—</div>"
    "<div class='muted' style='font-size:.78rem;margin-top:6px'>Длина: <span id='rx_len'>0</span> байт</div></div>"

    "<div class='card'><h2>TX (UART): GCS → автопилот, hex</h2>"
    "<p class='muted' style='font-size:clamp(0.72rem,0.6rem+0.25vw,0.82rem);margin:0 0 8px 0'>Только команды от Mission Planner/QGC (TCP/UDP→UART). Телеметрия в обратную сторону здесь не показывается.</p>"
    "<div class='hex' id='tx_sample'>—</div>"
    "<div class='muted' style='font-size:.78rem;margin-top:6px'>Длина: <span id='tx_len'>0</span> байт</div></div>"

    "<div class='card row2'><h2>Последние события MAVLink</h2>"
    "<div class='ev' id='mav_events'>—</div></div>"

    "</div></div>"

    "<script>"
    "function fmt(n){if(n==null||isNaN(n))return '—';n=Number(n);"
    "if(n>=1e6)return(n/1e6).toFixed(2)+' M';if(n>=1e3)return(n/1e3).toFixed(1)+' k';return String(n);}"
    "function fmtBytes(b){if(b==null||isNaN(b))return '—';b=Number(b);"
    "if(b>=1048576)return(b/1048576).toFixed(2)+' MB';if(b>=1024)return(b/1024).toFixed(1)+' kB';return b+' B';}"
    "function fmtMs(m){if(m==null||isNaN(m))return '—';m=Number(m);"
    "if(m>=60000)return(m/60000).toFixed(1)+' мин';if(m>=1000)return(m/1000).toFixed(1)+' с';return m+' мс';}"
    "function fmtUp(s){s=Number(s)||0;var d=Math.floor(s/86400);s-=d*86400;var h=Math.floor(s/3600);s-=h*3600;var m=Math.floor(s/60);s-=m*60;"
    "return(d?d+'д ':'')+h+'ч '+m+'м '+Math.floor(s)+'с';}"
    "function pill(ok,t){return '<span class=\"pill '+(ok?'pill-ok':'pill-no')+'\">'+t+'</span>';}"
    "function dot(ok){return '<span class=\"dot '+(ok?'d-ok':'d-no')+'\"></span>';}"
    "async function jget(u){var c=new AbortController();var to=setTimeout(function(){c.abort();},2200);"
    "try{var r=await fetch(u,{signal:c.signal});clearTimeout(to);if(!r.ok)return null;return await r.json();}"
    "catch(e){clearTimeout(to);return null;}}"
    "async function jtext(u){var c=new AbortController();var to=setTimeout(function(){c.abort();},120000);"
    "try{var r=await fetch(u,{signal:c.signal});clearTimeout(to);if(!r.ok)return'(HTTP '+r.status+')';return await r.text();}"
    "catch(e){clearTimeout(to);return'(ошибка загрузки)';}}"
    "function nextUnifiedFilename(){var k='unifiedLogDlCnt';var n=parseInt(localStorage.getItem(k)||'0',10)||0;n++;localStorage.setItem(k,String(n));"
    "return(n===1?'\xD0\x95\xD0\xB4\xD0\xB8\xD0\xBD\xD1\x8B\xD0\xB9 \xD0\xBB\xD0\xBE\xD0\xB3.txt':'\xD0\x95\xD0\xB4\xD0\xB8\xD0\xBD\xD1\x8B\xD0\xB9 \xD0\xBB\xD0\xBE\xD0\xB3 '+n+'.txt');}"
    "function buildUnifiedReportText(st,inf,cli,smp,fileTxt,espTxt){var L=[],ln=function(x){L.push(x);},br=String.fromCharCode(10);"
    "ln('\\xD0\\x95\\xD0\\xB4\\xD0\\xB8\\xD0\\xBD\\xD1\\x8B\\xD0\\xB9 \\xD0\\xBB\\xD0\\xBE\\xD0\\xB3 \\xE2\\x80\\x94 \\xD1\\x8D\\xD0\\xBA\\xD1\\x81\\xD0\\xBF\\xD0\\xBE\\xD1\\x80\\xD1\\x82');"
    "ln('\\xD0\\xA1\\xD1\\x84\\xD0\\xBE\\xD1\\x80\\xD0\\xBC\\xD0\\xB8\\xD1\\x80\\xD0\\xBE\\xD0\\xB2\\xD0\\xB0\\xD0\\xBD\\xD0\\xBE: '+new Date().toLocaleString());ln('');"
    "if(st){ln('=== \\xD0\\xA1\\xD0\\xBE\\xD0\\xB5\\xD0\\xB4\\xD0\\xB8\\xD0\\xBD\\xD0\\xB5\\xD0\\xBD\\xD0\\xB8\\xD0\\xB5 MAVLink / MissionPlanner ===');var c=!!st.connected;"
    "ln('\\xD0\\xA1\\xD1\\x82\\xD0\\xB0\\xD1\\x82\\xD1\\x83\\xD1\\x81: '+(c?'\\xD0\\x9F\\xD0\\xBE\\xD0\\xB4\\xD0\\xBA\\xD0\\xBB\\xD1\\x8E\\xD1\\x87\\xD0\\xB5\\xD0\\xBD\\xD0\\xBE \\xD0\\xBA MissionPlanner/MAVLink':'\\xD0\\x9D\\xD0\\xB5\\xD1\\x82 HEARTBEAT'));"
    "ln('\\xD0\\x9F\\xD0\\xBE\\xD1\\x81\\xD0\\xBB\\xD0\\xB5\\xD0\\xB4\\xD0\\xBD\\xD0\\xB8\\xD0\\xB9 HEARTBEAT: '+(c?fmtMs(st.heartbeat_age_ms):'\\xE2\\x80\\x94'));"
    "ln('\\xD0\\x98\\xD0\\xBD\\xD1\\x82\\xD0\\xB5\\xD1\\x80\\xD0\\xB2\\xD0\\xB0\\xD0\\xBB HB: '+(st.heartbeat_interval_ms?fmtMs(st.heartbeat_interval_ms):'\\xE2\\x80\\x94'));"
    "ln('\\xD0\\x9E\\xD1\\x88\\xD0\\xB8\\xD0\\xB1\\xD0\\xBE\\xD0\\xBA \\xD0\\xBF\\xD0\\xB0\\xD1\\x80\\xD1\\x81\\xD0\\xB8\\xD0\\xBD\\xD0\\xB3\\xD0\\xB0: '+fmt(st.mavlink_parse_err));"
    "ln('\\xD0\\x9F\\xD0\\xBE\\xD1\\x81\\xD0\\xBB\\xD0\\xB5\\xD0\\xB4\\xD0\\xBD\\xD1\\x8F\\xD1\\x8F \\xD0\\xBE\\xD1\\x88\\xD0\\xB8\\xD0\\xB1\\xD0\\xBA\\xD0\\xB0: '+(st.last_error||'none'));ln('');}else{"
    "ln('(/api/status \\xD0\\xBD\\xD0\\xB5\\xD0\\xB4\\xD0\\xBE\\xD1\\x81\\xD1\\x82\\xD1\\x83\\xD0\\xBF\\xD0\\xB5\\xD0\\xBD)');ln('');}"
    "if(inf||st){ln('=== \\xD0\\xA3\\xD1\\x81\\xD1\\x82\\xD1\\x80\\xD0\\xBE\\xD0\\xB9\\xD1\\x81\\xD1\\x82\\xD0\\xB2\\xD0\\xBE ===');if(inf){"
    "ln('\\xD0\\xA3\\xD0\\xBD\\xD0\\xB8\\xD0\\xBA\\xD0\\xB0\\xD0\\xBB\\xD1\\x8C\\xD0\\xBD\\xD1\\x8B\\xD0\\xB9 ID \\xD1\\x87\\xD0\\xB8\\xD0\\xBF\\xD0\\xB0: '+(inf.chip_uid||'\\xE2\\x80\\x94'));"
    "ln('MAC: '+(inf.esp_mac||'\\xE2\\x80\\x94'));}if(st){ln('Uptime: '+fmtUp(st.uptime));"
    "ln('\\xD0\\xA1\\xD0\\xB2\\xD0\\xBE\\xD0\\xB1\\xD0\\xBE\\xD0\\xB4\\xD0\\xBD\\xD1\\x8B\\xD0\\xB9 heap: '+fmtBytes(st.free_heap)+' (min '+fmtBytes(st.min_free_heap)+', max-\\xD0\\xB1\\xD0\\xBB\\xD0\\xBE\\xD0\\xBA '+fmtBytes(st.largest_free_block)+')');"
    "ln('\\xD0\\xA2\\xD0\\xB5\\xD0\\xBC\\xD0\\xBF\\xD0\\xB5\\xD1\\x80\\xD0\\xB0\\xD1\\x82\\xD1\\x83\\xD1\\x80\\xD0\\xB0 \\xD1\\x87\\xD0\\xB8\\xD0\\xBF\\xD0\\xB0: '+(st.chip_temp_c!=null?st.chip_temp_c.toFixed(1)+' \\xC2\\xB0C':'\\xE2\\x80\\x94'));}ln('');}"
    "if(st){ln('=== Wi-Fi / \\xD0\\xA1\\xD0\\xB8\\xD0\\xB3\\xD0\\xBD\\xD0\\xB0\\xD0\\xBB ===');"
    "ln('RSSI (\\xD1\\x81\\xD0\\xB5\\xD0\\xB9\\xD1\\x87\\xD0\\xB0\\xD1\\x81): '+(st.rssi_now!=null?st.rssi_now+' dBm':'\\xE2\\x80\\x94'));"
    "ln('\\xD0\\x9C\\xD0\\xB8\\xD0\\xBD / \\xD0\\x9C\\xD0\\xB0\\xD0\\xBA\\xD1\\x81 / \\xD0\\xA1\\xD1\\x80\\xD0\\xB5\\xD0\\xB4\\xD0\\xBD.: '+st.rssi_min+' / '+st.rssi_max+' / '+st.rssi_avg+' dBm');"
    "ln('AP assoc / disassoc: '+(st.ap_assoc_total||0)+' / '+(st.ap_disassoc_total||0));ln('STA \\xD1\\x80\\xD0\\xB5\\xD0\\xBA\\xD0\\xBE\\xD0\\xBD\\xD0\\xBD\\xD0\\xB5\\xD0\\xBA\\xD1\\x82\\xD0\\xBE\\xD0\\xB2: '+fmt(st.sta_reconnects_total));ln('');}"
    "if(cli){ln('=== \\xD0\\x9F\\xD0\\xBE\\xD0\\xB4\\xD0\\xBA\\xD0\\xBB\\xD1\\x8E\\xD1\\x87\\xD1\\x91\\xD0\\xBD\\xD0\\xBD\\xD1\\x8B\\xD0\\xB5 \\xD0\\xBA\\xD0\\xBB\\xD0\\xB8\\xD0\\xB5\\xD0\\xBD\\xD1\\x82\\xD1\\x8B ===');var n=0;if(cli.tcp){cli.tcp.forEach(function(t){"
    "ln('TCP | '+t.peer+' | \\xD1\\x81\\xD0\\xBE\\xD0\\xB5\\xD0\\xB4\\xD0\\xB8\\xD0\\xBD\\xD0\\xB5\\xD0\\xBD\\xD0\\xB8\\xD0\\xB5 '+fmtMs(t.connected_ms)+' | \\xD0\\xB1\\xD0\\xB0\\xD0\\xB9\\xD1\\x82 \\xD0\\xB2/\\xD0\\xB8\\xD0\\xB7 ESP '+fmtBytes(t.bytes_in)+' / '+fmtBytes(t.bytes_out)+' | \\xD0\\xBE\\xD1\\x87\\xD0\\xB5\\xD1\\x80\\xD0\\xB5\\xD0\\xB4\\xD1\\x8C '+t.queue_used+'/'+t.queue_size+(t.drop_bytes?' drop '+t.drop_bytes:''));n++;});}"
    "if(cli.udp){ln('UDP | '+cli.udp.peer+' | \\xD0\\xBF\\xD0\\xBE\\xD1\\x81\\xD0\\xBB. \\xD0\\xBF\\xD0\\xB0\\xD0\\xBA\\xD0\\xB5\\xD1\\x82 '+fmtMs(cli.udp.last_packet_ms)+' \\xD0\\xBD\\xD0\\xB0\\xD0\\xB7\\xD0\\xB0\\xD0\\xB4 | \\xD0\\xB1\\xD0\\xB0\\xD0\\xB9\\xD1\\x82 '+fmtBytes(cli.udp.bytes_in)+' / '+fmtBytes(cli.udp.bytes_out)+' | q \\xE2\\x80\\x94');n++;}"
    "if(!n)ln('\\xD0\\x9A\\xD0\\xBB\\xD0\\xB8\\xD0\\xB5\\xD0\\xBD\\xD1\\x82\\xD1\\x8B \\xD0\\xBD\\xD0\\xB5 \\xD0\\xBF\\xD0\\xBE\\xD0\\xB4\\xD0\\xBA\\xD0\\xBB\\xD1\\x8E\\xD1\\x87\\xD0\\xB5\\xD0\\xBD\\xD1\\x8B');ln('');}else{"
    "ln('(/api/clients недоступен)');ln('');}"
    "if(st){ln('=== \\xD0\\x9F\\xD0\\xB0\\xD0\\xBA\\xD0\\xB5\\xD1\\x82\\xD1\\x8B MAVLink ===');ln('\\xD0\\x9F\\xD0\\xBE\\xD0\\xBB\\xD1\\x83\\xD1\\x87\\xD0\\xB5\\xD0\\xBD\\xD0\\xBE RX: '+fmt(st.mavlink_rx_pkts));"
    "ln('\\xD0\\x9E\\xD1\\x82\\xD0\\xBF\\xD1\\x80\\xD0\\xB0\\xD0\\xB2\\xD0\\xBB\\xD0\\xB5\\xD0\\xBD\\xD0\\xBE \\xD0\\xB2 GCS (TX): '+fmt(st.mavlink_bridge_tx_pkts));"
    "ln('\\xD0\\x9F\\xD0\\xBE\\xD1\\x82\\xD0\\xB5\\xD1\\x80\\xD1\\x8F\\xD0\\xBD\\xD0\\xBE (seq-gap): '+fmt(st.mavlink_rx_lost));"
    "ln('\\xD0\\x92\\xD1\\x81\\xD0\\xB5\\xD0\\xB3\\xD0\\xBE \\xD0\\xBE\\xD0\\xB1\\xD1\\x80\\xD0\\xB0\\xD0\\xB1\\xD0\\xBE\\xD1\\x82\\xD0\\xB0\\xD0\\xBD\\xD0\\xBE: '+fmt((Number(st.mavlink_rx_pkts||0)+Number(st.mavlink_rx_lost||0))));"
    "var lp=st.mavlink_loss_pct!=null?Number(st.mavlink_loss_pct):0;ln('\\xD0\\x94\\xD0\\xBE\\xD0\\xBB\\xD1\\x8F \\xD0\\xBF\\xD0\\xBE\\xD1\\x82\\xD0\\xB5\\xD1\\x80\\xD1\\x8C: '+lp.toFixed(2)+'% '+(lp<5?'OK':'HIGH'));ln('');"
    "ln('=== UART (\\xD0\\xB0\\xD0\\xB2\\xD1\\x82\\xD0\\xBE\\xD0\\xBF\\xD0\\xB8\\xD0\\xBB\\xD0\\xBE\\xD1\\x82) ===');ln('\\xD0\\x91\\xD0\\xB0\\xD0\\xB9\\xD1\\x82 RX / TX: '+fmtBytes(st.uart_bytes_rx)+' / '+fmtBytes(st.uart_bytes_tx));"
    "ln('Overruns: '+st.uart_overruns+' '+(st.uart_overruns?'WARN':'OK'));ln('\\xD0\\x91\\xD0\\xB0\\xD0\\xB9\\xD1\\x82 \\xD0\\xB2 \\xD1\\x81\\xD0\\xB5\\xD1\\x82\\xD1\\x8C / \\xD0\\xB8\\xD0\\xB7 \\xD1\\x81\\xD0\\xB5\\xD1\\x82\\xD0\\xB8: '+fmtBytes(st.net_bytes_to_gcs)+' / '+fmtBytes(st.net_bytes_from_gcs));"
    "ln('TCP connect / disconnect: '+(st.tcp_connects_total||0)+' / '+(st.tcp_disconnects_total||0));ln('');}"
    "if(smp){ln('=== RX (UART): \\xD0\\xB0\\xD0\\xB2\\xD1\\x82\\xD0\\xBE\\xD0\\xBF\\xD0\\xB8\\xD0\\xBB\\xD0\\xBE\\xD1\\x82 \\xE2\\x86\\x92 ESP, hex ===');"
    "ln(smp.rx_len?smp.rx_hex:'(\\xD0\\xBD\\xD0\\xB5\\xD1\\x82 \\xD1\\x82\\xD1\\x80\\xD0\\xB0\\xD1\\x84\\xD0\\xB8\\xD0\\xBA\\xD0\\xB0 \\xD1\\x81 UART \\xD0\\xB0\\xD0\\xB2\\xD1\\x82\\xD0\\xBE\\xD0\\xBF\\xD0\\xB8\\xD0\\xBB\\xD0\\xBE\\xD1\\x82\\xD0\\xB0)');ln('\\xD0\\x94\\xD0\\xBB\\xD0\\xB8\\xD0\\xBD\\xD0\\xB0: '+(smp.rx_len||0)+' \\xD0\\xB1\\xD0\\xB0\\xD0\\xB9\\xD1\\x82');ln('');"
    "ln('=== TX (UART): GCS \\xE2\\x86\\x92 \\xD0\\xB0\\xD0\\xB2\\xD1\\x82\\xD0\\xBE\\xD0\\xBF\\xD0\\xB8\\xD0\\xBB\\xD0\\xBE\\xD1\\x82, hex ===');"
    "ln(smp.tx_len?smp.tx_hex:'(\\xD0\\xBD\\xD0\\xB5\\xD1\\x82 \\xD0\\xBA\\xD0\\xBE\\xD0\\xBC\\xD0\\xB0\\xD0\\xBD\\xD0\\xB4 \\xD0\\xBE\\xD1\\x82 GCS)');ln('\\xD0\\x94\\xD0\\xBB\\xD0\\xB8\\xD0\\xBD\\xD0\\xB0: '+(smp.tx_len||0)+' \\xD0\\xB1\\xD0\\xB0\\xD0\\xB9\\xD1\\x82');ln('');}else{"
    "ln('(/api/log/samples \\xD0\\xBD\\xD0\\xB5\\xD0\\xB4\\xD0\\xBE\\xD1\\x81\\xD1\\x82\\xD1\\x83\\xD0\\xBF\\xD0\\xB5\\xD0\\xBD)');ln('');}"
    "if(st&&st.log&&st.log.length){ln('=== \\xD0\\x9F\\xD0\\xBE\\xD1\\x81\\xD0\\xBB\\xD0\\xB5\\xD0\\xB4\\xD0\\xBD\\xD0\\xB8\\xD0\\xB5 \\xD1\\x81\\xD0\\xBE\\xD0\\xB1\\xD1\\x8B\\xD1\\x82\\xD0\\xB8\\xD1\\x8F MAVLink ===');st.log.slice().reverse().forEach(function(e){ln(e);});ln('');}"
    "ln('=== \\xD0\\x9F\\xD0\\xA0\\xD0\\x98\\xD0\\x9B\\xD0\\x9E\\xD0\\x96\\xD0\\x95\\xD0\\x9D\\xD0\\x98\\xD0\\x95 A: /api/log/file ===');ln(fileTxt||'(\\xD0\\xBF\\xD1\\x83\\xD1\\x81\\xD1\\x82\\xD0\\xBE)');ln('');"
    "ln('=== \\xD0\\x9F\\xD0\\xA0\\xD0\\x98\\xD0\\x9B\\xD0\\x9E\\xD0\\x96\\xD0\\x95\\xD0\\x9D\\xD0\\x98\\xD0\\x95 B: /api/log/esp32 ===');ln(espTxt||'(\\xD0\\xBF\\xD1\\x83\\xD1\\x81\\xD1\\x82\\xD0\\xBE)');return L.join(br);}"
    "async function downloadUnifiedTxt(){var st,inf,cli,smp,fileTxt,espTxt;"
    "try{st=await jget('/api/status');inf=await jget('/api/system/info');cli=await jget('/api/clients');smp=await jget('/api/log/samples');"
    "fileTxt=await jtext('/api/log/file');espTxt=await jtext('/api/log/esp32');}catch(e){alert('Ошибка загрузки');return;}"
    "var body=buildUnifiedReportText(st,inf,cli,smp,fileTxt,espTxt),fn=nextUnifiedFilename(),blob=new Blob([body],{type:'text/plain;charset=utf-8'}),a=document.createElement('a');"
    "a.href=URL.createObjectURL(blob);a.download=fn;a.click();setTimeout(function(){URL.revokeObjectURL(a.href);},2500);}"
    "async function loadAll(force){"
    "var st=await jget('/api/status');"
    "var inf=await jget('/api/system/info');"
    "var cli=await jget('/api/clients');"
    "var smp=await jget('/api/log/samples');"
    "document.getElementById('ts').textContent='(обновлено '+new Date().toLocaleTimeString()+')';"
    "if(st){"
    "var conn=!!st.connected;"
    "document.getElementById('mav_status').innerHTML=dot(conn)+(conn?'Подключено к MissionPlanner/MAVLink':'Нет HEARTBEAT');"
    "document.getElementById('hb_age').textContent=conn?fmtMs(st.heartbeat_age_ms):'—';"
    "document.getElementById('hb_int').textContent=st.heartbeat_interval_ms?fmtMs(st.heartbeat_interval_ms):'—';"
    "document.getElementById('parse_err').textContent=fmt(st.mavlink_parse_err);"
    "document.getElementById('last_err').textContent=st.last_error||'none';"
    "document.getElementById('uptime').textContent=fmtUp(st.uptime);"
    "document.getElementById('heap').textContent=fmtBytes(st.free_heap)+' (min '+fmtBytes(st.min_free_heap)+', max-блок '+fmtBytes(st.largest_free_block)+')';"
    "document.getElementById('chip_temp').textContent=(st.chip_temp_c!=null?st.chip_temp_c.toFixed(1)+' °C':'—');"
    "document.getElementById('rssi_now').textContent=(st.rssi_now!=null?st.rssi_now+' dBm':'—');"
    "document.getElementById('rssi_mma').textContent=st.rssi_min+' / '+st.rssi_max+' / '+st.rssi_avg+' dBm';"
    "document.getElementById('ap_ev').textContent=(st.ap_assoc_total||0)+' / '+(st.ap_disassoc_total||0);"
    "document.getElementById('sta_rc').textContent=fmt(st.sta_reconnects_total);"
    "document.getElementById('p_rx').textContent=fmt(st.mavlink_rx_pkts);"
    "document.getElementById('p_tx').textContent=fmt(st.mavlink_bridge_tx_pkts);"
    "document.getElementById('p_lost').textContent=fmt(st.mavlink_rx_lost);"
    "document.getElementById('p_total').textContent=fmt((Number(st.mavlink_rx_pkts||0)+Number(st.mavlink_rx_lost||0)));"
    "var lp=st.mavlink_loss_pct!=null?Number(st.mavlink_loss_pct):0;"
    "document.getElementById('p_lossp').innerHTML=lp.toFixed(2)+'% '+pill(lp<5,lp<5?'OK':'HIGH');"
    "document.getElementById('uart_bytes').textContent=fmtBytes(st.uart_bytes_rx)+' / '+fmtBytes(st.uart_bytes_tx);"
    "document.getElementById('uart_or').innerHTML=st.uart_overruns+' '+pill(!st.uart_overruns,st.uart_overruns?'WARN':'OK');"
    "document.getElementById('net_bytes').textContent=fmtBytes(st.net_bytes_to_gcs)+' / '+fmtBytes(st.net_bytes_from_gcs);"
    "document.getElementById('tcp_cd').textContent=(st.tcp_connects_total||0)+' / '+(st.tcp_disconnects_total||0);"
    "if(st.log&&st.log.length){document.getElementById('mav_events').textContent=st.log.slice().reverse().join('\\n');}"
    "}"
    "if(inf){"
    "document.getElementById('chip_uid').textContent=inf.chip_uid||'—';"
    "document.getElementById('chip_mac').textContent=inf.esp_mac||'—';"
    "}"
    "if(cli){"
    "var tb=document.getElementById('clients_tb');tb.innerHTML='';"
    "var rows=0;"
    "if(cli.tcp){cli.tcp.forEach(function(t){"
    "var tr=document.createElement('tr');"
    "tr.innerHTML='<td><span class=\"proto-tcp\">TCP</span></td>'+"
    "'<td>'+t.peer+'</td>'+"
    "'<td>'+fmtMs(t.connected_ms)+'</td>'+"
    "'<td>'+fmtBytes(t.bytes_in)+' / '+fmtBytes(t.bytes_out)+'</td>'+"
    "'<td>'+t.queue_used+'/'+t.queue_size+(t.drop_bytes?' <span class=\"muted\">(drop '+t.drop_bytes+')</span>':'')+'</td>';"
    "tb.appendChild(tr);rows++;});}"
    "if(cli.udp){"
    "var tr=document.createElement('tr');"
    "tr.innerHTML='<td><span class=\"proto-udp\">UDP</span></td>'+"
    "'<td>'+cli.udp.peer+'</td>'+"
    "'<td>посл. '+fmtMs(cli.udp.last_packet_ms)+' назад</td>'+"
    "'<td>'+fmtBytes(cli.udp.bytes_in)+' / '+fmtBytes(cli.udp.bytes_out)+'</td>'+"
    "'<td class=\"muted\">—</td>';"
    "tb.appendChild(tr);rows++;"
    "}"
    "if(rows===0)tb.innerHTML='<tr><td class=\"muted\" colspan=\"5\">Клиенты не подключены</td></tr>';"
    "}"
    "if(smp){"
    "document.getElementById('rx_sample').textContent=smp.rx_len?smp.rx_hex:'(нет трафика с UART автопилота — нет данных FC→ESP)';"
    "document.getElementById('rx_len').textContent=smp.rx_len||0;"
    "document.getElementById('tx_sample').textContent=smp.tx_len?smp.tx_hex:'(нет команд от GCS к автопилоту — откройте Mission Planner и подключитесь по TCP/UDP)';"
    "document.getElementById('tx_len').textContent=smp.tx_len||0;"
    "}"
    "}"
    "loadAll();setInterval(function(){if(!document.hidden)loadAll();},2000);"
    "</script></body></html>";

static void handleLogPage(AsyncWebServerRequest* req) {
    AsyncWebServerResponse* r = req->beginResponse_P(200, "text/html; charset=utf-8", kLogPageHtml);
    req->send(r);
}

/* ===================================================================
 * Страница /esp32log — просмотр лога событий ESP32
 * =================================================================== */
static const char PROGMEM kEspLogPageHtml[] =
    "<!DOCTYPE html><html lang='ru'><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Лог ESP32 — Bridge</title>"
    "<style>"
    "*{box-sizing:border-box}"
    "html{-webkit-text-size-adjust:100%;text-size-adjust:100%;font-size:clamp(14px,0.35rem+2.1vw,18px);}"
    "body{margin:0;padding:clamp(10px,3.5vw,22px);font-family:'Segoe UI',Roboto,sans-serif;color:#fff;"
    "background:linear-gradient(135deg,#001f3f 0%,#0074d9 100%);background-attachment:fixed;min-height:100vh;}"
    ".wrap{max-width:min(1100px,100%);margin:0 auto;}"
    "header{display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;gap:clamp(8px,2vw,14px);margin-bottom:clamp(12px,3vw,18px);}"
    "h1{font-weight:300;font-size:clamp(1.2rem,0.9rem+1.4vw,1.9rem);margin:0;letter-spacing:.5px;}"
    ".nav{display:flex;gap:clamp(6px,1.8vw,10px);flex-wrap:wrap;align-items:center;}"
    ".nav a,.nav button,.nav label{background:rgba(255,255,255,.08);color:#fff;border:1px solid rgba(255,255,255,.18);"
    "padding:clamp(6px,1.8vw,10px) clamp(8px,2.5vw,16px);border-radius:8px;text-decoration:none;font-size:clamp(0.78rem,0.65rem+0.45vw,0.9rem);"
    "cursor:pointer;font-family:inherit;transition:.2s;min-height:clamp(38px,9vw,46px);touch-action:manipulation;line-height:1.2;}"
    ".nav a:hover,.nav button:hover,.nav label:hover{background:rgba(255,151,52,.25);border-color:#ff9734;}"
    ".nav a.primary{background:#ff9734;border-color:#ff9734;color:#fff;font-weight:600;}"
    ".nav a.primary:hover{background:#e6862b;}"
    ".nav input[type=checkbox]{margin-right:6px;vertical-align:middle;}"
    ".card{background:rgba(0,0,0,.35);border:1px solid rgba(255,255,255,.08);border-radius:12px;"
    "padding:clamp(10px,2.8vw,18px) clamp(10px,3vw,18px);box-shadow:0 6px 20px rgba(0,0,0,.25);backdrop-filter:blur(8px);}"
    ".meta{display:flex;flex-wrap:wrap;gap:clamp(8px,2.2vw,16px);font-size:clamp(0.8rem,0.7rem+0.35vw,0.9rem);margin-bottom:12px;color:rgba(255,255,255,.75);}"
    ".meta span b{color:#fff;font-weight:600;}"
    "#logview{font-family:Consolas,'Cascadia Mono','Liberation Mono',monospace;font-size:clamp(0.78rem,0.65rem+0.3vw,0.9rem);line-height:1.55;"
    "background:rgba(0,0,0,.45);border:1px solid rgba(255,255,255,.08);border-radius:8px;"
    "padding:clamp(10px,2.5vw,16px);"
    "min-height:clamp(200px,38vh,360px);height:min(calc(100vh - 200px),85vh);max-height:min(90vh,720px);"
    "overflow-y:auto;white-space:pre-wrap;word-break:break-word;-webkit-overflow-scrolling:touch;}"
    ".ln-err{color:#ff8f8f;}.ln-warn{color:#ffd07a;}.ln-ok{color:#8fd65b;}.ln-info{color:#9bd4ff;}"
    ".ts{font-size:clamp(0.7rem,0.6rem+0.25vw,0.8rem);color:rgba(255,255,255,.5);margin-left:6px;}"
    ".muted{color:rgba(255,255,255,.55);}"
    "</style></head><body><div class='wrap'>"
    "<header>"
    "<h1>Лог ESP32 <span class='ts' id='ts'>—</span></h1>"
    "<nav class='nav'>"
    "<a href='/'>&larr; Главная</a>"
    "<a href='/log'>Единый лог</a>"
    "<label><input type='checkbox' id='auto' checked>авто 3с</label>"
    "<label><input type='checkbox' id='follow' checked>следить</label>"
    "<button onclick='loadLog()'>Обновить</button>"
    "<a class='primary' href='/api/log/esp32' download='esp32.log'>Скачать .log</a>"
    "</nav></header>"

    "<div class='card'>"
    "<div class='meta'>"
    "<span>Строк: <b id='m_lines'>—</b></span>"
    "<span>Размер: <b id='m_size'>—</b></span>"
    "<span>Uptime: <b id='m_up'>—</b></span>"
    "<span>Heap: <b id='m_heap'>—</b></span>"
    "<span>Темп: <b id='m_temp'>—</b></span>"
    "<span>RSSI: <b id='m_rssi'>—</b></span>"
    "</div>"
    "<div id='logview'><span class='muted'>Загрузка лога…</span></div>"
    "</div>"
    "</div>"

    "<script>"
    "function classify(l){var u=l.toUpperCase();"
    "if(u.indexOf('[E]')>=0||u.indexOf('ERROR')>=0||u.indexOf('FAIL')>=0||u.indexOf('PANIC')>=0)return 'ln-err';"
    "if(u.indexOf('[W]')>=0||u.indexOf('WARN')>=0)return 'ln-warn';"
    "if(u.indexOf('[I]')>=0||u.indexOf('CONNECTED')>=0||u.indexOf('SUCCESS')>=0)return 'ln-ok';"
    "return 'ln-info';}"
    "function fmtBytes(b){if(b==null||isNaN(b))return '—';b=Number(b);"
    "if(b>=1048576)return(b/1048576).toFixed(2)+' MB';if(b>=1024)return(b/1024).toFixed(1)+' kB';return b+' B';}"
    "function fmtUp(s){s=Number(s)||0;var d=Math.floor(s/86400);s-=d*86400;var h=Math.floor(s/3600);s-=h*3600;var m=Math.floor(s/60);s-=m*60;"
    "return(d?d+'д ':'')+h+'ч '+m+'м '+Math.floor(s)+'с';}"
    "async function loadLog(){"
    "var c=new AbortController();var to=setTimeout(function(){c.abort();},2500);"
    "try{"
    "var r=await fetch('/api/log/esp32',{signal:c.signal});"
    "clearTimeout(to);"
    "var txt=await r.text();"
    "var lines=txt.split(/\\r?\\n/);"
    "document.getElementById('m_lines').textContent=lines.filter(function(x){return x.length;}).length;"
    "document.getElementById('m_size').textContent=fmtBytes(txt.length);"
    "var v=document.getElementById('logview');"
    "var follow=document.getElementById('follow').checked;"
    "var html='';for(var i=0;i<lines.length;i++){var ln=lines[i];if(!ln)continue;"
    "var cls=classify(ln);html+='<span class=\"'+cls+'\">'+ln.replace(/[<>&]/g,function(c){return({'<':'&lt;','>':'&gt;','&':'&amp;'})[c];})+'</span>\\n';}"
    "v.innerHTML=html||'<span class=\"muted\">(пусто)</span>';"
    "if(follow)v.scrollTop=v.scrollHeight;"
    "document.getElementById('ts').textContent='(обновлено '+new Date().toLocaleTimeString()+')';"
    "}catch(e){clearTimeout(to);document.getElementById('logview').innerHTML='<span class=\"ln-err\">Ошибка загрузки: '+e+'</span>';}"
    "try{var s=await (await fetch('/api/status',{cache:'no-store'})).json();"
    "document.getElementById('m_up').textContent=fmtUp(s.uptime);"
    "document.getElementById('m_heap').textContent=fmtBytes(s.free_heap);"
    "document.getElementById('m_temp').textContent=(s.chip_temp_c!=null?s.chip_temp_c.toFixed(1)+' °C':'—');"
    "document.getElementById('m_rssi').textContent=(s.rssi_now!=null?s.rssi_now+' dBm':'—');"
    "}catch(e){}}"
    "loadLog();"
    "setInterval(function(){if(!document.hidden&&document.getElementById('auto').checked)loadLog();},3000);"
    "</script></body></html>";

static void handleEsp32LogPage(AsyncWebServerRequest* req) {
    AsyncWebServerResponse* r = req->beginResponse_P(200, "text/html; charset=utf-8", kEspLogPageHtml);
    req->send(r);
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
    s_server->on("/log",                HTTP_GET, handleLogPage);
    s_server->on("/esp32log",           HTTP_GET, handleEsp32LogPage);
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
    s_server->on("/api/log/samples",    HTTP_GET, handleApiLogSamples);

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
