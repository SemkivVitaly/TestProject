/**
 * web_handlers.cpp — HTTP-обработчики и маршруты веб-сервера.
 *
 * Веб-интерфейс работает только локально по Wi‑Fi платы (http://192.168.2.1 в AP или IP платы в STA).
 *
 * МАРШРУТЫ И КТО ИХ ВЫЗЫВАЕТ:
 *   — Браузер пользователя запрашивает URL → WebServer вызывает зарегистрированный обработчик.
 *   — / → handleRoot() — главная с ссылками на лог, параметры, Bridge UI.
 *   — /params → handleParamsPage() — страница параметров SERVO (запрос с автопилота, форма установки).
 *   — /bridge → handleBridgePage() — раздача Bridge UI (из PROGMEM или LittleFS).
 *   — /api/status → handleApiStatus() — JSON: uptime, connected, packets, bytes, параметры SERVO, лог.
 *   — /api/link → handleApiLink() — JSON по каналу (packets, drops, latency, bytes).
 *   — /api/params GET/POST → handleParamsGet/handleParamsSet — чтение/установка параметров SERVO.
 *   — /api/param_request → handleParamRequest() — отправить запрос параметров на автопилот.
 *   — /api/settings GET/POST → настройки Wi‑Fi/UART; POST вызывает setBridgeConfigFromJson() и перезагрузку.
 *   — /api/settings/clients/udp (POST), clear_udp (DELETE) — задать/сбросить UDP-клиента.
 */
#include "config.h"
#ifdef WEB_SERVER

#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>
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

static WebServer* s_server = nullptr;  /* Указатель на сервер, переданный в webSetup(); используется в sendJson/sendHtml. */

static void sendJson(const String& s) {
    if (s_server) s_server->send(200, F("application/json"), s);
}

static void sendHtml(const String& html) {
    if (s_server) s_server->send(200, F("text/html; charset=utf-8"), html);
}

/** Главная страница: единый лог, лог пакетов, лог ESP32, управление SERVO, Bridge UI. */
static void handleRoot() {
    sendHtml(F(
        "<!DOCTYPE html><html><head><meta charset='utf-8'><title>ESP32-S3-Box</title>"
        "<style>body{font-family:sans-serif;margin:1.5rem;} a{color:#1eaedb;} a:hover{color:#0fa0ce;}</style></head><body>"
        "<h1>ESP32-S3-Box Bridge</h1>"
        "<p><a href='/api/log/file'>Единый лог</a></p>"
        "<p><a href='/api/log'>Лог пакетов</a></p>"
        "<p><a href='/api/log/esp32'>Лог ESP32</a></p>"
        "<p><a href='/params'>Управление SERVO</a></p>"
        "<p><a href='/bridge'>Bridge UI</a></p>"
        "</body></html>"
    ));
}

static void handleStatus() {
    String s;
    s += F("{\"heap\":"); s += ESP.getFreeHeap();
    s += F(",\"uptime\":"); s += (millis() / 1000);
    s += F(",\"mavlink_connected\":"); s += mavlinkConnected ? F("true") : F("false");
    s += F(",\"packets_rx\":"); s += mavlinkPacketsRx;
    s += F(",\"packets_tx\":"); s += mavlinkPacketsTx;
    s += F("}");
    sendJson(s);
}

static void handleApiStatus() {
    uint32_t latencyMs = mavlinkConnected ? (millis() - lastHeartbeatMs) : 0;
    uint32_t totalRx = mavlinkPacketsRx + mavlinkPacketDrops;
    float packetLossPct = (totalRx > 0) ? (100.0f * (float)mavlinkPacketDrops / (float)totalRx) : 0.0f;
    char lastErr[64];
    bridgeLogGetLastError(lastErr, sizeof(lastErr));
    String s;
    s += F("{\"uptime\":"); s += (millis() / 1000);
    s += F(",\"free_heap\":"); s += ESP.getFreeHeap();
    s += F(",\"connected\":"); s += mavlinkConnected ? F("true") : F("false");
    s += F(",\"tcp_connected\":"); s += bridgeGetTcpConnectedCount();
    s += F(",\"udp_known\":"); s += (bridgeGetUdpClientCount() > 0) ? F("true") : F("false");
    s += F(",\"last_error\":\""); s += lastErr; s += F("\"");
    s += F(",\"last_heartbeat_ms\":"); s += lastHeartbeatMs;
    s += F(",\"packets_rx\":"); s += mavlinkPacketsRx;
    s += F(",\"packets_tx\":"); s += mavlinkPacketsTx;
    s += F(",\"packets_processed\":"); s += mavlinkPacketsRx;
    s += F(",\"packet_drops\":"); s += mavlinkPacketDrops;
    s += F(",\"packet_loss_pct\":"); s += String(packetLossPct, 2);
    s += F(",\"latency_ms\":"); s += latencyMs;
    s += F(",\"bytes_network_tx\":"); s += bridgeBytesTxNetwork;
    s += F(",\"bytes_network_rx\":"); s += bridgeBytesRxNetwork;
    s += F(",\"SERVO1_REVERSED\":"); s += paramServo1Revers;
    s += F(",\"SERVO1_REVERSED_known\":"); s += paramServo1ReversKnown ? F("true") : F("false");
    s += F(",\"SERVO3_TRIM\":"); s += paramServo3Trim;
    s += F(",\"SERVO3_TRIM_known\":"); s += paramServo3TrimKnown ? F("true") : F("false");
    s += F(",\"SERVO4_TRIM\":"); s += paramServo4Trim;
    s += F(",\"SERVO4_TRIM_known\":"); s += paramServo4TrimKnown ? F("true") : F("false");
    s += F(",\"log\":[");
    for (uint8_t n = 0, i = 0; n < MAVLINK_LOG_SIZE; n++) {
        uint8_t idx = (mavlinkLogHead + n) % MAVLINK_LOG_SIZE;
        if (mavlinkLog[idx][0] != '\0') {
            if (i++) s += ',';
            s += '"'; s += mavlinkLog[idx]; s += '"';
        }
    }
    s += F("]}");
    sendJson(s);
}

static void handleParamsPage() {
    String html = F(
        "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Параметры SERVO</title>"
        "<style>body{font-family:sans-serif;margin:1rem;} table{border-collapse:collapse;} th,td{border:1px solid #ccc;padding:6px;} .ok{color:green;} .no{color:red;}</style>"
        "</head><body><h1>Параметры MAVLink (SERVO)</h1>"
        "<p id='conn'></p>"
        "<p><button type='button' onclick=\"window.paramsFrozen=false; var x=new XMLHttpRequest();x.open('GET','/api/param_request');x.send();setTimeout(load,500);\">Запросить с автопилота</button></p>"
        "<form method='post' action='/api/params'>"
        "<table><tr><th>Параметр</th><th>Значение</th><th>Получен</th></tr>"
        "<tr><td>SERVO1_REVERSED</td><td><input name='SERVO1_REVERSED' id='v1' type='number' step='0.01'></td><td id='k1'>—</td></tr>"
        "<tr><td>SERVO3_TRIM</td><td><input name='SERVO3_TRIM' id='v2' type='number' step='0.01'></td><td id='k2'>—</td></tr>"
        "<tr><td>SERVO4_TRIM</td><td><input name='SERVO4_TRIM' id='v3' type='number' step='0.01'></td><td id='k3'>—</td></tr>"
        "</table><button type='submit'>Установить</button></form>"
        "<p><a href='/'>Назад</a> | <a href='/api/status'>Статус JSON</a> | <a href='/api/log'>Лог</a></p>"
        "<script>"
        "function load(){ var x=new XMLHttpRequest(); x.open('GET','/api/status'); x.onload=function(){"
        "var j=JSON.parse(x.responseText);"
        "document.getElementById('conn').innerHTML='Подключение: '+(j.connected?'<span class=ok>Да</span>':'<span class=no>Нет</span>')+' | RX: '+j.packets_rx+' TX: '+j.packets_tx+' | Задержка: '+(j.connected?j.latency_ms:'—')+' ms | Потери: '+j.packet_drops+' ('+j.packet_loss_pct+'%)';"
        "if(!window.paramsFrozen){document.getElementById('v1').value=j.SERVO1_REVERSED!=undefined?j.SERVO1_REVERSED:j.SERVO1_REVERS; document.getElementById('v2').value=j.SERVO3_TRIM; document.getElementById('v3').value=j.SERVO4_TRIM;} "
        "document.getElementById('k1').textContent=(j.SERVO1_REVERSED_known||j.SERVO1_REVERS_known)?'да':'—'; document.getElementById('k2').textContent=j.SERVO3_TRIM_known?'да':'—'; document.getElementById('k3').textContent=j.SERVO4_TRIM_known?'да':'—'; "
        "if((j.SERVO1_REVERSED_known||j.SERVO1_REVERS_known)&&j.SERVO3_TRIM_known&&j.SERVO4_TRIM_known)window.paramsFrozen=true;"
        "}; x.send(); }"
        "load(); setInterval(load,5000);"
        "</script></body></html>"
    );
    sendHtml(html);
}

static void handleParamsGet() {
    String s;
    s += F("{\"SERVO1_REVERSED\":"); s += paramServo1Revers; s += F(",\"SERVO1_REVERSED_known\":"); s += paramServo1ReversKnown ? F("true") : F("false");
    s += F(",\"SERVO3_TRIM\":"); s += paramServo3Trim; s += F(",\"SERVO3_TRIM_known\":"); s += paramServo3TrimKnown ? F("true") : F("false");
    s += F(",\"SERVO4_TRIM\":"); s += paramServo4Trim; s += F(",\"SERVO4_TRIM_known\":"); s += paramServo4TrimKnown ? F("true") : F("false");
    s += F("}");
    sendJson(s);
}

static void handleParamsSet() {
    bool sent = false;
    if (s_server->hasArg(F("SERVO1_REVERSED"))) {
        mavlinkSendParamSet("SERVO1_REVERSED", s_server->arg(F("SERVO1_REVERSED")).toFloat());
        sent = true;
    }
    if (s_server->hasArg(F("SERVO3_TRIM"))) {
        mavlinkSendParamSet("SERVO3_TRIM", s_server->arg(F("SERVO3_TRIM")).toFloat());
        sent = true;
    }
    if (s_server->hasArg(F("SERVO4_TRIM"))) {
        mavlinkSendParamSet("SERVO4_TRIM", s_server->arg(F("SERVO4_TRIM")).toFloat());
        sent = true;
    }
    if (sent)
        sendJson(F("{\"ok\":true}"));
    else if (s_server)
        s_server->send(400, F("application/json"), F("{\"ok\":false,\"error\":\"need SERVO1_REVERSED, SERVO3_TRIM or SERVO4_TRIM\"}"));
}

static void handleParamRequest() {
    mavlinkRequestServoParams();
    sendJson(F("{\"ok\":true}"));
}

static void handleApiLink() {
    uint32_t latencyMs = mavlinkConnected ? (millis() - lastHeartbeatMs) : 0;
    uint32_t totalRx = mavlinkPacketsRx + mavlinkPacketDrops;
    float packetLossPct = (totalRx > 0) ? (100.0f * (float)mavlinkPacketDrops / (float)totalRx) : 0.0f;
    String s;
    s += F("{\"connected\":"); s += mavlinkConnected ? F("true") : F("false");
    s += F(",\"packets_sent\":"); s += mavlinkPacketsTx;
    s += F(",\"packets_received\":"); s += mavlinkPacketsRx;
    s += F(",\"packets_processed\":"); s += mavlinkPacketsRx;
    s += F(",\"packet_drops\":"); s += mavlinkPacketDrops;
    s += F(",\"packet_loss_pct\":"); s += String(packetLossPct, 2);
    s += F(",\"latency_ms\":"); s += latencyMs;
    s += F(",\"bytes_network_tx\":"); s += bridgeBytesTxNetwork;
    s += F(",\"bytes_network_rx\":"); s += bridgeBytesRxNetwork;
    s += F("}");
    sendJson(s);
}

/** Страница Bridge UI: из прошивки (PROGMEM) или из LittleFS. */
static void handleBridgePage() {
    if (!s_server) return;
#ifdef BRIDGE_UI_EMBED_H
    s_server->send_P(200, PSTR("text/html; charset=utf-8"), (const char*)BRIDGE_UI_HTML);
#else
    File f = LittleFS.open("/index.html", "r");
    if (!f) {
        s_server->send(404, F("text/plain"), F("Интерфейс Bridge не найден. Загрузите ФС: pio run -t uploadfs."));
        return;
    }
    s_server->streamFile(f, F("text/html; charset=utf-8"));
    f.close();
#endif
}

static void handleApiSystemInfo() {
    String s;
    s += F("{\"major_version\":2,\"minor_version\":0,\"patch_version\":0,\"maturity_version\":\"Bridge\",\"idf_version\":\"arduino\",\"esp_chip_model\":9,\"esp_mac\":\"");
    s += WiFi.macAddress();
    s += F("\",\"serial_via_JTAG\":0,\"has_rf_switch\":0}");
    sendJson(s);
}

static void handleApiSystemStats() {
    int8_t rssi = 0;
    if (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED)
        rssi = WiFi.RSSI();
    uint32_t totalRx = mavlinkPacketsRx + mavlinkPacketDrops;
    bridgeLogUpdateStats(mavlinkPacketsTx, mavlinkPacketsRx, mavlinkPacketDrops, totalRx);
    bridgeLogUpdateRssi(rssi);
    /* Периодически или при изменении RSSI записывать в лог (раз в 60 с или при падении > 5 dBm). */
    {
        static int8_t s_lastRssi = 0;
        static uint32_t s_lastRssiLogMs = 0;
        uint32_t now = millis();
        if ((now - s_lastRssiLogMs >= 60000) || (rssi != 0 && (rssi < s_lastRssi - 5 || rssi > s_lastRssi + 5))) {
            s_lastRssiLogMs = now;
            s_lastRssi = rssi;
            espLogPrintf("[WiFi] RSSI %d dBm", (int)rssi);
        }
    }
    char udpInfo[32];
    bool hasUdp = bridgeGetUdpClientInfo(udpInfo, sizeof(udpInfo));
    String s;
    s += F("{\"read_bytes\":"); s += bridgeBytesFromUart;
    s += F(",\"serial_dec_mav_msgs\":"); s += mavlinkPacketsRx;
    s += F(",\"tcp_connected\":"); s += bridgeGetTcpConnectedCount();
    s += F(",\"udp_connected\":"); s += bridgeGetUdpClientCount();
    s += F(",\"udp_clients\":");
    if (hasUdp) {
        s += F("[\""); s += udpInfo; s += F("\"]");
    } else {
        s += F("[]");
    }
    s += F(",\"current_client_ip\":\"");
    if (hasUdp) s += udpInfo;
    s += F("\",\"esp_rssi\":");
    s += (int)rssi;
    s += F("}");
    sendJson(s);
}

static void handleApiSettingsGet() {
    const bridge_nvs_config_t& c = bridge_nvs_config;
    String s;
    s += F("{\"esp32_mode\":"); s += c.wifi_mode;
    s += F(",\"ssid\":\""); s += c.ssid;
    s += F("\",\"wifi_pass\":\""); s += c.wifi_pass;
    s += F("\",\"wifi_chan\":"); s += c.wifi_chan;
    s += F(",\"ap_ip\":\""); s += c.ap_ip;
    s += F("\",\"wifi_en_gn\":1,\"radio_dis_onarm\":0,\"ant_use_ext\":0,\"ip_sta\":\"\",\"ip_sta_gw\":\"\",\"ip_sta_netmsk\":\"\",\"wifi_hostname\":\""); s += c.hostname;
    s += F("\",\"gpio_tx\":"); s += c.gpio_tx;
    s += F(",\"gpio_rx\":"); s += c.gpio_rx;
    s += F(",\"gpio_rts\":0,\"gpio_cts\":0,\"rts_thresh\":64,\"proto\":"); s += c.proto;
    s += F(",\"baud\":"); s += c.baud;
    s += F(",\"ltm_per_packet\":1,\"trans_pack_size\":256,\"serial_timeout\":50,\"rep_rssi_dbm\":0}");
    sendJson(s);
}

static void handleApiSettingsPost() {
    String body;
    if (s_server->hasArg(F("plain")))
        body = s_server->arg(F("plain"));
    if (body.length())
        setBridgeConfigFromJson(body.c_str());
    sendJson(F("{\"msg\":\"Настройки сохранены. Перезагрузка...\"}"));
    delay(1000);
    ESP.restart();
}

static void handleApiSettingsClientsUdp() {
    String body = s_server->hasArg(F("plain")) ? s_server->arg(F("plain")) : String();
    IPAddress ip(0U);
    uint16_t port = SERIAL_UDP_PORT;
    if (body.length() > 0) {
        int idx = body.indexOf(F("\"udp_client_ip\":\""));
        if (idx >= 0) {
            idx += 19; /* длина "\"udp_client_ip\":\"" */
            int end = body.indexOf('"', idx);
            if (end > idx)
                ip.fromString(body.substring(idx, end));
        }
        if (ip == IPAddress(0U)) {
            idx = body.indexOf(F("\"ip\":\""));
            if (idx >= 0) {
                idx += 6;
                int end = body.indexOf('"', idx);
                if (end > idx)
                    ip.fromString(body.substring(idx, end));
            }
        }
        idx = body.indexOf(F("\"udp_client_port\":"));
        if (idx >= 0) {
            port = (uint16_t)body.substring(idx + 19).toInt();  /* 19 = len("\"udp_client_port\":") */
            if (port == 0) port = SERIAL_UDP_PORT;
        } else {
            idx = body.indexOf(F("\"port\":"));
            if (idx >= 0) {
                port = (uint16_t)body.substring(idx + 7).toInt();
                if (port == 0) port = SERIAL_UDP_PORT;
            }
        }
    }
    if (ip != IPAddress(0U)) {
        bridgeSetUdpClient(ip, port);
        sendJson(F("{\"msg\":\"UDP-клиент задан по IP и порту.\"}"));
    } else {
        sendJson(F("{\"msg\":\"Укажите ip в теле JSON ({\\\"ip\\\":\\\"192.168.2.100\\\",\\\"port\\\":14550}).\"}"));
    }
}

static void handleApiSettingsClearUdp() {
    bridgeClearUdpClient();
    sendJson(F("{\"msg\":\"UDP-клиент сброшен.\"}"));
}

static void handleLog() {
    String s = F("[");
    for (uint8_t n = 0, i = 0; n < MAVLINK_LOG_SIZE; n++) {
        uint8_t idx = (mavlinkLogHead + n) % MAVLINK_LOG_SIZE;
        if (mavlinkLog[idx][0] != '\0') {
            if (i++) s += ',';
            s += '"'; s += mavlinkLog[idx]; s += '"';
        }
    }
    s += F("]");
    sendJson(s);
}

/** Единый лог: ID, подключение, статистика, WiFi dBm, счётчики по типам, образцы RX/TX, последние MAVLink события, события ESP32. */
static void handleApiLogFile() {
    char buf[3072];
    bridgeLogGetText(buf, sizeof(buf));
    if (s_server) s_server->send(200, F("text/plain; charset=utf-8"), buf);
}

/** Лог ESP32 (кольцевой буфер) — скачать. */
static void handleApiLogEsp32() {
    char buf[ESP_LOG_SIZE + 64];
    size_t n = espLogGetText(buf, sizeof(buf));
    if (s_server) s_server->send(200, F("text/plain; charset=utf-8"), buf);
}

/** Регистрирует все маршруты на server и вызывает server.begin(). Вызывается из main.cpp setup() один раз. */
void webSetup(WebServer& server) {
    s_server = &server;
    server.on(F("/"), handleRoot);
    server.on(F("/status"), handleStatus);
    server.on(F("/api/status"), handleApiStatus);
    server.on(F("/params"), handleParamsPage);
    server.on(F("/api/params"), HTTP_GET, handleParamsGet);
    server.on(F("/api/params"), HTTP_POST, handleParamsSet);
    server.on(F("/api/param_request"), handleParamRequest);
    server.on(F("/api/log"), handleLog);
    server.on(F("/api/log/file"), handleApiLogFile);
    server.on(F("/api/log/esp32"), handleApiLogEsp32);
    server.on(F("/api/link"), handleApiLink);
    server.on(F("/bridge"), handleBridgePage);
    server.on(F("/smd"), []() { if (s_server) s_server->sendHeader(F("Location"), F("/bridge"), true); if (s_server) s_server->send(302, F("text/plain"), F("")); });
    server.on(F("/api/system/info"), handleApiSystemInfo);
    server.on(F("/api/system/stats"), handleApiSystemStats);
    server.on(F("/api/settings"), HTTP_GET, handleApiSettingsGet);
    server.on(F("/api/settings"), HTTP_POST, handleApiSettingsPost);
    server.on(F("/api/settings/clients/udp"), HTTP_POST, handleApiSettingsClientsUdp);
    server.on(F("/api/settings/clients/clear_udp"), HTTP_DELETE, handleApiSettingsClearUdp);
    server.begin();
}

#endif /* WEB_SERVER */
