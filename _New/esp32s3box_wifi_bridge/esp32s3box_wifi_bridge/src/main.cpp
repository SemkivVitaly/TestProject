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
#include <ESPmDNS.h>
#include "config.h"
#include "bridge_nvs.h"
#include "mavlink_state.h"
#include "bridge.h"
#include "bridge_log.h"
#include "esp_log.h"

#ifdef OTA_HANDLER
#include <ArduinoOTA.h>
#endif
#ifdef WEB_SERVER
#include <ESP32Servo.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "web_handlers.h"
#endif

/**
 * Второй последовательный порт (UART1). Serial — это UART0 (USB/консоль).
 * Пины задаются в setup() из NVS или config.h. По этому порту идёт обмен с автопилотом (MAVLink).
 */
HardwareSerial SerialUART(1);

#ifdef WEB_SERVER
WebServer webServer(WEB_SERVER_PORT);  /* HTTP-сервер на порту из config.h. */
Servo servo;
bool servoAttached = false;
#endif

/** Буфер для данных с UART от автопилота. Заполняется в loop(), передаётся в mavlinkProcessBytes() и bridgeSendUartToNetwork(). */
static uint8_t bufFromUART[BUFFERSIZE];
static uint16_t lenFromUART = 0;

/** Обработчик «Wi‑Fi отключился» в режиме STA: переподключаемся через WiFi.begin() с SSID/паролем из bridge_nvs_config. */
static void onWiFiDisconnected(WiFiEvent_t, WiFiEventInfo_t) {
    espLogPrintf("[WiFi] STA disconnected, reconnecting...");
    debug.println(F("WiFi disconnected, reconnecting..."));
    WiFi.begin(bridge_nvs_config.ssid, bridge_nvs_config.wifi_pass);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        debug.print(F("."));
    }
    debug.print(F("\nConnected, IP: "));
    debug.println(WiFi.localIP());
    espLogPrintf("[WiFi] STA reconnected IP %s", WiFi.localIP().toString().c_str());
}

/**
 * setup() — выполняется один раз при включении платы.
 * Порядок: Serial, загрузка конфига из NVS, инициализация UART к автопилоту,
 * выбор режима Wi‑Fi (AP или STA), запуск моста, при WEB_SERVER — LittleFS и веб-сервер.
 */
void setup() {
    delay(500);
    Serial.begin(115200);  /* Скорость консоли для отладки (Monitor в PlatformIO). */
    debug.print(F("\nWiFi bridge "));
    debug.println(VERSION);
    espLogPrintf("[boot] WiFi bridge %s", VERSION);

    /* Загрузить настройки из NVS в глобальную структуру bridge_nvs_config (WiFi, UART, hostname и т.д.). Если NVS пустой — используются значения по умолчанию из bridge_nvs (из config.h). */
    loadBridgeConfig();

    /* UART к автопилоту: скорость и пины берём из NVS (если там заданы), иначе из config.h. */
    uint32_t baud = (bridge_nvs_config.baud > 0) ? bridge_nvs_config.baud : (uint32_t)UART_BAUD;
    int8_t rxPin = (bridge_nvs_config.gpio_rx >= 0) ? bridge_nvs_config.gpio_rx : (int8_t)SERIAL_RXPIN;
    int8_t txPin = (bridge_nvs_config.gpio_tx >= 0) ? bridge_nvs_config.gpio_tx : (int8_t)SERIAL_TXPIN;
    SerialUART.begin(baud, SERIAL_PARAM, (int)rxPin, (int)txPin);

#ifdef WEB_SERVER
    pinMode(LED_PIN, OUTPUT);
    pinMode(BTN_PIN, INPUT_PULLUP);
    mavlinkInitLog();  /* Обнулить кольцевой лог событий MAVLink. */
#endif

    /* Режим Wi‑Fi из сохранённой конфигурации: 1 = точка доступа (AP), 2 = клиент (STA). */
    if (bridge_nvs_config.wifi_mode == 2) {
        WiFi.mode(WIFI_STA);
        WiFi.onEvent(onWiFiDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
        WiFi.begin(bridge_nvs_config.ssid, bridge_nvs_config.wifi_pass);
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            debug.print(F("."));
        }
        debug.println();
        debug.println(WiFi.localIP());
        espLogPrintf("[WiFi] STA connected IP %s", WiFi.localIP().toString().c_str());
        if (MDNS.begin(bridge_nvs_config.hostname)) {
            MDNS.addService("_telnet", "_tcp", SERIAL0_TCP_PORT);
            debug.print(F("mDNS: ")); debug.println(bridge_nvs_config.hostname);
        }
    } else {
        /* Режим AP: плата раздаёт Wi‑Fi. Подключиться к SSID → веб по 192.168.2.1. */
        WiFi.mode(WIFI_AP);
        WiFi.softAP(bridge_nvs_config.ssid, bridge_nvs_config.wifi_pass);
        delay(2000);
        WiFi.softAPConfig(STATIC_IP, STATIC_IP, NETMASK);
        debug.println(F("AP IP: "));
        debug.println(WiFi.softAPIP());
        espLogPrintf("[WiFi] AP started IP %s", WiFi.softAPIP().toString().c_str());
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

    /* Запуск TCP-сервера (порт 8880), UDP (14550) и при BLUETOOTH — BT. Клиенты принимаются в loop() через bridgeAcceptClient(). */
    bridgeSetup();

#ifdef WEB_SERVER
    /* LittleFS нужна для раздачи страницы /bridge из файла, если не встроена в прошивку (embed). Веб доступен только по Wi‑Fi платы. */
    LittleFS.begin(true);
    webSetup(webServer);  /* Регистрирует все маршруты: /, /params, /api/* и т.д. */
    debug.println(F("Web (local): http://192.168.2.1"));
#endif

#ifdef BATTERY_SAVER
    esp_wifi_set_max_tx_power(50);
#endif
}

/**
 * loop() — вызывается бесконечно после setup().
 * Порядок важен: сначала обрабатываем веб-клиентов и OTA, затем мост (принять клиента, отключения, сеть→UART), затем чтение UART и пересылка в сеть.
 */
void loop() {
#ifdef OTA_HANDLER
    ArduinoOTA.handle();  /* Проверка входящего OTA-обновления по Wi‑Fi. */
#endif

#ifdef WEB_SERVER
    webServer.handleClient();   /* Обработать один HTTP-запрос (главная, /api/status и т.д.). */
    mavlinkCheckDisconnect();   /* Если давно не было HEARTBEAT — сбросить mavlinkConnected. */
    if (digitalRead(BTN_PIN) == LOW) {
        static uint32_t lastBtn = 0;
        if (millis() - lastBtn > 300) {
            lastBtn = millis();
            debug.println(F("Btn"));
        }
    }
#endif

    /* Принять нового TCP-клиента (Mission Planner и т.п.), если есть свободный слот. */
    bridgeAcceptClient();
    /* Удалить отключённых TCP-клиентов и сбросить UDP-клиента по таймауту. */
    bridgePollDisconnects();
    /* Прочитать данные из TCP (и BT) и записать их в SerialUART к автопилоту. */
    bridgePollNetworkToUart();

    /* Читаем всё, что пришло с автопилота по UART. Каждый блок: сохраняем в лог, парсим MAVLink (статус, параметры), пересылаем в сеть (TCP/UDP). */
    while (SerialUART.available()) {
        lenFromUART = 0;
        while (SerialUART.available() && lenFromUART < BUFFERSIZE - 1)
            bufFromUART[lenFromUART++] = SerialUART.read();

        if (lenFromUART == 0) break;
        bridgeLogSetLastRx(bufFromUART, lenFromUART);       /* Для единого лога (образец RX). */
        mavlinkProcessBytes(bufFromUART, lenFromUART);      /* Разбор HEARTBEAT, PARAM_VALUE и т.д. */
        bridgeSendUartToNetwork(bufFromUART, lenFromUART);  /* Отправить те же байты всем TCP/UDP клиентам. */
    }

    delay(1);  /* Небольшая пауза, чтобы не нагружать CPU на 100%. */
}
