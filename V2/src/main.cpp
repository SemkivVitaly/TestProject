/**
 * main.cpp — точка входа. WiFi-мост Mission Planner (TCP/UDP) ↔ UART ↔ автопилот.
 * Сборка: config.h, bridge, mavlink_state, web_handlers.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "config.h"
#include "mavlink_state.h"
#include "bridge.h"

#ifdef OTA_HANDLER
#include <ArduinoOTA.h>
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

#ifdef MODE_STA
static void onWiFiDisconnected(WiFiEvent_t, WiFiEventInfo_t) {
    debug.println(F("WiFi disconnected, reconnecting..."));
    WiFi.begin(SSID, PASSWD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        debug.print(F("."));
    }
    debug.print(F("\nConnected, IP: "));
    debug.println(WiFi.localIP());
}
#endif

void setup() {
    delay(500);
    Serial.begin(115200);
    debug.print(F("\nWiFi bridge "));
    debug.println(VERSION);

    SerialUART.begin(UART_BAUD, SERIAL_PARAM, SERIAL_RXPIN, SERIAL_TXPIN);

#ifdef WEB_SERVER
    pinMode(LED_PIN, OUTPUT);
    pinMode(BTN_PIN, INPUT_PULLUP);
    mavlinkInitLog();
#endif

#ifdef MODE_AP
    WiFi.mode(WIFI_AP);
    WiFi.softAP(SSID, PASSWD);
    delay(2000);
    WiFi.softAPConfig(STATIC_IP, STATIC_IP, NETMASK);
    debug.println(F("AP IP: "));
    debug.println(WiFi.softAPIP());
#endif

#ifdef MODE_STA
    WiFi.mode(WIFI_STA);
    WiFi.onEvent(onWiFiDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    WiFi.begin(SSID, PASSWD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        debug.print(F("."));
    }
    debug.println();
    debug.println(WiFi.localIP());
    if (MDNS.begin(HOSTNAME)) {
        MDNS.addService("_telnet", "_tcp", SERIAL0_TCP_PORT);
        debug.println(F("mDNS: " HOSTNAME));
    }
#endif

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
    debug.println(F("Web: http://192.168.2.1 (AP)"));
#endif

#ifdef BATTERY_SAVER
    esp_wifi_set_max_tx_power(50);
#endif
}

void loop() {
#ifdef OTA_HANDLER
    ArduinoOTA.handle();
#endif

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

    bridgeAcceptClient();
    bridgePollNetworkToUart();

    /* Телеметрия с UART: читаем всё доступное порциями и сразу пересылаем в сеть. */
    while (SerialUART.available()) {
        lenFromUART = 0;
        while (SerialUART.available() && lenFromUART < BUFFERSIZE - 1)
            bufFromUART[lenFromUART++] = SerialUART.read();

        if (lenFromUART == 0) break;
        mavlinkProcessBytes(bufFromUART, lenFromUART);
        bridgeSendUartToNetwork(bufFromUART, lenFromUART);
    }

    delay(1);
}
