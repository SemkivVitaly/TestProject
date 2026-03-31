/**
 * main.cpp — точка входа прошивки.
 *
 * Назначение:
 *   WiFi-мост между наземной станцией (Mission Planner, QGC) и автопилотом:
 *   данные по Wi‑Fi (TCP/UDP) ↔ ESP32 ↔ UART ↔ автопилот. MAVLink-пакеты
 *   парсятся для статуса и параметров SERVO; веб-интерфейс доступен только
 *   локально — по Wi‑Fi точки доступа ESP32 (подключились к SSID платы →
 *   в браузере открыть http://192.168.2.1). Интернет не требуется.
 *
 * Зависимости: config.h, bridge, mavlink_state, web_handlers (при WEB_SERVER).
 */
#include <Arduino.h>
#include <WiFi.h>
#include <cmath>
#include <ESPmDNS.h>
#include <esp_wifi.h>
#include "config.h"
#include "bridge_nvs.h"
#include "mavlink_state.h"
#include "bridge.h"
#include "bridge_log.h"
#include "esp_log.h"

#ifdef OTA_HANDLER
#include <ArduinoOTA.h>
#endif
#ifdef BATTERY_SAVER
/* При BATTERY_SAVER мощность Wi‑Fi снижается в конце setup() — иначе используется максимум. */
#endif
#ifdef WEB_SERVER
#include <ESP32Servo.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "web_handlers.h"
#endif

HardwareSerial SerialUART(1);

#ifdef WEB_SERVER
WebServer webServer(WEB_SERVER_PORT);
Servo servo;
bool servoAttached = false;
#endif

static uint8_t bufFromUART[BUFFERSIZE];
static uint16_t lenFromUART = 0;

static uint32_t s_lastChipTempLogMs = 0;
static const uint32_t kChipTempLogIntervalMs = 15000;

static volatile bool s_staReconnecting = false;

/** Отключение Wi‑Fi power save; TX и beacon из config.h (см. WIFI_TUNE_AGGRESSIVE). */
static void applyWifiLowLatencyAndPower(void) {
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
#ifndef BATTERY_SAVER
    esp_wifi_set_max_tx_power(WIFI_MAX_TX_POWER);
#endif

    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) != ESP_OK)
        return;
    if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) {
        wifi_config_t cfg = {};
        if (esp_wifi_get_config(WIFI_IF_AP, &cfg) == ESP_OK) {
            cfg.ap.beacon_interval = WIFI_AP_BEACON_INTERVAL_TU;
            esp_wifi_set_config(WIFI_IF_AP, &cfg);
        }
    }
}

static void onWiFiDisconnected(WiFiEvent_t, WiFiEventInfo_t) {
    if (s_staReconnecting) return;
    s_staReconnecting = true;
    espLogPrintf("[WiFi] STA disconnected, will reconnect...");
    debug.println(F("WiFi disconnected, reconnecting..."));
    WiFi.begin(bridge_nvs_config.ssid, bridge_nvs_config.wifi_pass);
}

static void onWiFiGotIP(WiFiEvent_t, WiFiEventInfo_t) {
    s_staReconnecting = false;
    debug.print(F("Connected, IP: "));
    debug.println(WiFi.localIP());
    espLogPrintf("[WiFi] STA reconnected IP %s", WiFi.localIP().toString().c_str());
    applyWifiLowLatencyAndPower();
}

void setup() {
    delay(500);
    Serial.begin(115200);
    debug.print(F("\nWiFi bridge "));
    debug.println(VERSION);
    espLogPrintf("[boot] WiFi bridge %s", VERSION);

    loadBridgeConfig();

    uint32_t baud = (bridge_nvs_config.baud > 0) ? bridge_nvs_config.baud : (uint32_t)UART_BAUD;
    int8_t rxPin = (bridge_nvs_config.gpio_rx >= 0) ? bridge_nvs_config.gpio_rx : (int8_t)SERIAL_RXPIN;
    int8_t txPin = (bridge_nvs_config.gpio_tx >= 0) ? bridge_nvs_config.gpio_tx : (int8_t)SERIAL_TXPIN;
    SerialUART.setRxBufferSize(8192);
    SerialUART.setTxBufferSize(4096);
    SerialUART.begin(baud, SERIAL_PARAM, (int)rxPin, (int)txPin);

#ifdef WEB_SERVER
    pinMode(LED_PIN, OUTPUT);
    pinMode(BTN_PIN, INPUT_PULLUP);
    mavlinkInitLog();
#endif

    if (bridge_nvs_config.wifi_mode == 2) {
        WiFi.mode(WIFI_STA);
        WiFi.onEvent(onWiFiDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
        WiFi.onEvent(onWiFiGotIP, ARDUINO_EVENT_WIFI_STA_GOT_IP);
        WiFi.begin(bridge_nvs_config.ssid, bridge_nvs_config.wifi_pass);
        {
            uint32_t t0 = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
                delay(250);
                debug.print(F("."));
            }
        }
        if (WiFi.status() == WL_CONNECTED) {
            debug.println();
            debug.println(WiFi.localIP());
            espLogPrintf("[WiFi] STA connected IP %s", WiFi.localIP().toString().c_str());
        } else {
            debug.println(F("\nSTA: initial connect timeout, will retry in background"));
            espLogPrintf("[WiFi] STA initial connect timeout");
        }
        applyWifiLowLatencyAndPower();
        if (MDNS.begin(bridge_nvs_config.hostname)) {
            MDNS.addService("_telnet", "_tcp", SERIAL0_TCP_PORT);
            debug.print(F("mDNS: ")); debug.println(bridge_nvs_config.hostname);
        }
    } else {
        uint8_t apCh = bridge_nvs_config.wifi_chan;
        if (apCh < 1 || apCh > 13)
            apCh = 6;
        WiFi.mode(WIFI_AP);
        WiFi.softAP(bridge_nvs_config.ssid, bridge_nvs_config.wifi_pass, apCh, 0, 4);
        delay(400);
        WiFi.softAPConfig(STATIC_IP, STATIC_IP, NETMASK);
        applyWifiLowLatencyAndPower();
        debug.println(F("AP IP: "));
        debug.println(WiFi.softAPIP());
        espLogPrintf("[WiFi] AP started ch=%u IP %s", (unsigned)apCh, WiFi.softAPIP().toString().c_str());
    }

#ifdef OTA_HANDLER
    ArduinoOTA.onStart([]() {
        Serial.println(ArduinoOTA.getCommand() == U_FLASH ? F("OTA sketch") : F("OTA fs"));
    });
    ArduinoOTA.onEnd([]() { Serial.println(F("OTA end")); });
    ArduinoOTA.onProgress([](unsigned int p, unsigned int t) {
        if (t) Serial.printf("%u%%\r", (p * 100) / t);
    });
    ArduinoOTA.onError([](ota_error_t e) { Serial.printf("OTA err %u\n", e); });
    ArduinoOTA.begin();
#endif

    bridgeSetup();

#ifdef WEB_SERVER
    LittleFS.begin(true);
    webSetup(webServer);
    debug.println(F("Web (local): http://192.168.2.1"));
#endif

#ifdef BATTERY_SAVER
    esp_wifi_set_max_tx_power(50);
#endif

    {
        float tc = temperatureRead();
        if (std::isfinite(static_cast<double>(tc)))
            espLogPrintf("[chip_temp] boot %.1f C", (double)tc);
    }
}

void loop() {
#ifdef OTA_HANDLER
    ArduinoOTA.handle();
#endif

    uint32_t nowMs = millis();
    if (nowMs - s_lastChipTempLogMs >= kChipTempLogIntervalMs) {
        s_lastChipTempLogMs = nowMs;
        float tc = temperatureRead();
        if (std::isfinite(static_cast<double>(tc)))
            espLogPrintf("[chip_temp] %.1f C", (double)tc);
    }

    /* Сначала мост и UART — меньше задержек для Mission Planner и PARAM_* */
    bridgeAcceptClient();
    bridgePollDisconnects();
    bridgePollNetworkToUart();

    while (SerialUART.available()) {
        lenFromUART = 0;
        while (SerialUART.available() && lenFromUART < BUFFERSIZE - 1)
            bufFromUART[lenFromUART++] = SerialUART.read();

        if (lenFromUART == 0) break;
        bridgeLogSetLastRx(bufFromUART, lenFromUART);
        mavlinkProcessBytes(bufFromUART, lenFromUART);
        bridgeSendUartToNetwork(bufFromUART, lenFromUART);
    }

#ifdef WEB_SERVER
    webServer.handleClient();
    mavlinkCheckDisconnect();
    if (digitalRead(BTN_PIN) == LOW) {
        static uint32_t lastBtn = 0;
        if (millis() - lastBtn > 300) {
            lastBtn = millis();
            debug.println(F("Btn"));
        }
    }
#endif

    yield();
}
