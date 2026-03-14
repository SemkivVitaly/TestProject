# Пошаговый туториал: сборка WiFi-моста с нуля до веб-интерфейса

Этот документ ведёт от **пустой папки** до **полностью рабочего проекта**: точка доступа Wi‑Fi, мост TCP/UDP ↔ UART, сохранение настроек в NVS, разбор MAVLink, веб-интерфейс со статусом и параметрами. На каждом шаге приведён **полный код** файлов и пояснения, как они работают и как связаны друг с другом.

---

## Что получится в конце

- Плата ESP32 поднимает точку доступа Wi‑Fi (или подключается к роутеру).
- По TCP (порт 8880) и UDP (14550) принимаются подключения от Mission Planner / QGroundControl.
- Все байты пересылаются на второй UART к автопилоту и обратно.
- Настройки (SSID, пароль, режим Wi‑Fi, скорость UART, пины) хранятся в NVS и загружаются при старте.
- По MAVLink распознаются HEARTBEAT и PARAM_VALUE; параметры SERVO можно запрашивать и устанавливать.
- Веб-интерфейс по адресу `http://192.168.2.1`: главная страница, статус (JSON), лог, страница параметров SERVO.

---

## Шаг 0. Создание проекта и структуры папок

### 0.1. Создание проекта в PlatformIO

1. Откройте VS Code (или Cursor), установите расширение **PlatformIO IDE**.
2. **File → Open Folder** — создайте новую папку, например `esp32s3box_wifi_bridge`, и откройте её.
3. В панели PlatformIO (иконка «домик»): **New Project** (или **PIO Home → Create New Project**).
   - Name: `esp32s3box_wifi_bridge`
   - Board: **ESP32S3 Box** (или выберите вашу плату ESP32)
   - Framework: **Arduino**
   - Create.

Либо без мастера: создайте вручную папку проекта и внутри неё файл `platformio.ini`.

### 0.2. Содержимое platformio.ini

Создайте файл **platformio.ini** в корне проекта (в той же папке, где будет папка `src/`):

```ini
[env:esp32s3box]
platform = espressif32
board = esp32s3box
framework = arduino
upload_protocol = esptool
build_flags = -DCORE_DEBUG_LEVEL=2 -I include
board_build.filesystem = littlefs
extra_scripts = pre:tools/embed_bridge_ui.py
lib_deps =
	madhephaestus/ESP32Servo@^3.1.3
	okalachev/MAVLink@^2.0.27
monitor_speed = 115200
```

**Пояснение:**  
- `platform` и `board` задают плату ESP32.  
- `framework = arduino` — используем API Arduino (Serial, WiFi, WebServer и т.д.).  
- `build_flags = -I include` — компилятор будет искать заголовки в папке `include/` (туда скрипт запишет `bridge_ui_embed.h`).  
- `extra_scripts` — перед сборкой выполнится скрипт, который встраивает веб-страницу в прошивку.  
- `lib_deps` — библиотеки MAVLink и ESP32Servo (последняя для опционального сервопривода).

### 0.3. Структура папок

Создайте папки и файлы (пустые файлы можно создать сразу, содержимое добавим по шагам):

```
esp32s3box_wifi_bridge/
├── platformio.ini
├── src/
│   ├── main.cpp
│   ├── config.h
│   ├── bridge.cpp
│   ├── bridge.h
│   ├── bridge_nvs.cpp
│   ├── bridge_nvs.h
│   ├── mavlink_state.cpp
│   ├── mavlink_state.h
│   ├── web_handlers.cpp
│   ├── web_handlers.h
│   ├── bridge_log.cpp
│   ├── bridge_log.h
│   ├── esp_log.cpp
│   └── esp_log.h
├── include/          (пустая — сюда пишет скрипт bridge_ui_embed.h)
├── data/
│   └── index.html   (минимум: пустая страница или одна строка HTML)
└── tools/
    └── embed_bridge_ui.py
```

Дальше мы по шагам заполняем каждый файл **полным кодом** и поясняем, за что он отвечает и как вызывается.

---

## Шаг 1. config.h — константы проекта

Файл **src/config.h** задаёт все настраиваемые константы: Wi‑Fi, порты, пины UART, размер буферов, включение веб-сервера. Его не «вызывают» — его подключают через `#include` в других файлах.

**Полный код src/config.h:**

```c
#ifndef CONFIG_H
#define CONFIG_H

#define DEBUG
#include <WiFi.h>

#define MODE_AP
#define SSID     "ESP32S3_NEW"
#define PASSWD   "12345678"
#define HOSTNAME "esps3"
#define STATIC_IP IPAddress(192, 168, 2, 1)
#define NETMASK   IPAddress(255, 255, 255, 0)

#define PROTOCOL_TCP
#define PROTOCOL_UDP
#define MAX_NMEA_CLIENTS 4

#define UART_BAUD 57600
#define SERIAL_PARAM SERIAL_8N1
#define SERIAL_TXPIN 13
#define SERIAL_RXPIN 14

#define SERIAL_TCP_PORT 8880
#define SERIAL0_TCP_PORT SERIAL_TCP_PORT
#define SERIAL_UDP_PORT 14550

#define BUFFERSIZE 1024
#define VERSION "2.0-ESP32S3"

#define WEB_SERVER
#define WEB_SERVER_PORT 80
#define LED_PIN 2
#define SERVO_PIN 12
#define BTN_PIN 0

#define MAVLINK_LOG_SIZE 50
#define MAVLINK_LOG_ENTRY_LEN 80
#define MAVLINK_HEARTBEAT_TIMEOUT_MS 5000

#ifdef DEBUG
    #define debug Serial
#else
    class Debug : public Print { size_t write(uint8_t) override { return 0; } };
    Debug debug;
#endif

#endif
```

**Кратко по блокам:**  
- Wi‑Fi: SSID/пароль точки доступа, IP платы в режиме AP (192.168.2.1).  
- Мост: включены TCP и UDP, до 4 TCP-клиентов.  
- UART: скорость 57600, пины TX=13, RX=14.  
- Порты: TCP 8880, UDP 14550.  
- Буфер 1024 байт, версия строки.  
- WEB_SERVER — включён веб-сервер; LED/BTN/SERVO — номера пинов.  
- MAVLINK_* — размер кольцевого лога и таймаут потери связи по HEARTBEAT.  
- DEBUG — если определён, `debug` это Serial, иначе «пустой» вывод.

Без этого файла остальной код не скомпилируется: в main.cpp и других модулях используются `BUFFERSIZE`, `SERIAL_TCP_PORT`, `bridge_nvs` использует `SSID`, `PASSWD` и т.д.

---

## Шаг 2. main.cpp — точка входа, setup() и loop()

Файл **src/main.cpp** — единственная точка входа: здесь объявляются глобальные объекты (второй UART, веб-сервер, буфер из UART), обработчик отключения Wi‑Fi, а также функции **setup()** и **loop()**, которые вызывает среда Arduino.

Ниже — **полный код main.cpp** (без сокращений). После кода — пошаговое объяснение, что в setup() и loop() вызывается и в каком порядке.

```cpp
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

HardwareSerial SerialUART(1);

#ifdef WEB_SERVER
WebServer webServer(WEB_SERVER_PORT);
Servo servo;
bool servoAttached = false;
#endif

static uint8_t bufFromUART[BUFFERSIZE];
static uint16_t lenFromUART = 0;

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
    SerialUART.begin(baud, SERIAL_PARAM, (int)rxPin, (int)txPin);

#ifdef WEB_SERVER
    pinMode(LED_PIN, OUTPUT);
    pinMode(BTN_PIN, INPUT_PULLUP);
    mavlinkInitLog();
#endif

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

    bridgeSetup();

#ifdef WEB_SERVER
    LittleFS.begin(true);
    webSetup(webServer);
    debug.println(F("Web (local): http://192.168.2.1"));
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

    delay(1);
}
```

**Как это устроено:**

- **SerialUART** — второй аппаратный UART (UART1); по нему идёт обмен с автопилотом. Пины и скорость задаются в setup() из NVS или config.h.
- **bufFromUART** — буфер, в который в loop() читаются байты из SerialUART; затем этот же буфер передаётся в `bridgeLogSetLastRx`, `mavlinkProcessBytes` и `bridgeSendUartToNetwork`.
- **onWiFiDisconnected** — вызывается системой Wi‑Fi при обрыве связи в режиме STA; мы снова вызываем `WiFi.begin()` с сохранёнными SSID/паролем из `bridge_nvs_config`.
- **setup():** загрузка конфига из NVS → инициализация UART к автопилоту → режим Wi‑Fi (STA или AP) → `bridgeSetup()` (TCP/UDP) → при WEB_SERVER — LittleFS и `webSetup(webServer)`.
- **loop():** при WEB_SERVER — обработка одного HTTP-запроса и проверка таймаута MAVLink; затем приём нового TCP-клиента, очистка отключённых, пересылка «сеть → UART»; затем чтение всего доступного из UART, запись в буфер, вызов трёх функций (лог, разбор MAVLink, пересылка в сеть); в конце `delay(1)`.

Чтобы проект собрался, нужны остальные модули: **bridge**, **bridge_nvs**, **mavlink_state**, **bridge_log**, **esp_log**, а при WEB_SERVER — **web_handlers**. Их мы добавляем на следующих шагах с полным кодом.

---

## Шаг 3. Мост «сеть ↔ UART» (bridge.h и bridge.cpp)

Модуль **bridge** пересылает байты: из TCP/UDP — в SerialUART к автопилоту, из SerialUART — всем сетевым клиентам. В main.cpp вызываются только функции из bridge.h; вся работа с сокетами и буферами — внутри bridge.cpp.

### Полный код src/bridge.h

```cpp
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
void bridgeClearUdpClient(void);
void bridgeSetUdpClient(IPAddress ip, uint16_t port);
void bridgePollDisconnects(void);
bool bridgeGetUdpClientInfo(char* buf, size_t bufSize);

#endif
```

**Пояснение:**  
- `bridgeSetup()` вызывается из main в setup() один раз — запускает TCP-сервер и UDP.  
- В loop() main по очереди вызывает: `bridgeAcceptClient()` (принять нового TCP), `bridgePollDisconnects()` (убрать отключённых), `bridgePollNetworkToUart()` (сеть → UART). После чтения байт из SerialUART main вызывает `bridgeSendUartToNetwork(bufFromUART, lenFromUART)`.  
- Счётчики и функции GetTcpConnectedCount/GetUdpClientCount используются в web_handlers для JSON /api/status и /api/link.

### Полный код src/bridge.cpp

```cpp
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
static bool udpClientKnown = false;
static uint32_t lastUdpPacketMs = 0;
#define UDP_CLIENT_TIMEOUT_MS 30000
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
}

void bridgeAcceptClient(void) {
#if defined(PROTOCOL_TCP)
    if (!tcpServer.hasClient()) return;
    WiFiClient c = tcpServer.accept();
    if (!c) return;
    bool placed = false;
    for (byte i = 0; i < MAX_NMEA_CLIENTS; i++) {
        if (!tcpClients[i] || !tcpClients[i].connected()) {
            if (tcpClients[i]) tcpClients[i].stop();
            tcpClients[i] = c;
            placed = true;
            debug.print(F("TCP client "));
            debug.println(c.remoteIP());
            espLogPrintf("[bridge] TCP client set %s:%u", c.remoteIP().toString().c_str(), (unsigned)c.remotePort());
            break;
        }
    }
    if (!placed) c.stop();
#endif
}

void bridgePollNetworkToUart(void) {
#if defined(PROTOCOL_TCP)
    for (byte i = 0; i < MAX_NMEA_CLIENTS; i++) {
        if (!tcpClients[i] || !tcpClients[i].available()) continue;
        lenToUART = 0;
        while (tcpClients[i].available() && lenToUART < BUFFERSIZE - 1)
            bufToUART[lenToUART++] = tcpClients[i].read();
        if (lenToUART > 0) {
            bridgeBytesRxNetwork += lenToUART;
            SerialUART.write(bufToUART, lenToUART);
        }
    }
#endif
}

uint8_t bridgeGetTcpConnectedCount(void) {
#if defined(PROTOCOL_TCP)
    uint8_t n = 0;
    for (byte i = 0; i < MAX_NMEA_CLIENTS; i++)
        if (tcpClients[i] && tcpClients[i].connected()) n++;
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

void bridgePollDisconnects(void) {
#if defined(PROTOCOL_TCP)
    for (byte i = 0; i < MAX_NMEA_CLIENTS; i++) {
        if (tcpClients[i] && !tcpClients[i].connected()) {
            tcpClients[i].stop();
            tcpClients[i] = WiFiClient();
        }
    }
#endif
#if defined(PROTOCOL_UDP)
    if (udpClientKnown && lastUdpPacketMs != 0 && (millis() - lastUdpPacketMs) > UDP_CLIENT_TIMEOUT_MS) {
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
}
```

**Как это работает по шагам:**

- **bridgeSetup():** TCP-сервер вызывает `tcpServer.begin()` на порту 8880; UDP вызывает `udp.listen(14550)` и регистрирует коллбек `onPacket` — при приходе пакета сразу пишем его в `SerialUART` и запоминаем IP:port отправителя как «UDP-клиент».
- **bridgeAcceptClient():** Если `tcpServer.hasClient()` — делаем `accept()`, ищем свободный слот в `tcpClients[]` (или слот с отключённым клиентом), помещаем туда нового клиента; если слотов нет — закрываем соединение.
- **bridgePollNetworkToUart():** Для каждого подключённого TCP-клиента читаем доступные байты в `bufToUART`, затем одной операцией пишем в `SerialUART` и увеличиваем `bridgeBytesRxNetwork`.
- **bridgeSendUartToNetwork(data, len):** Вызывается из main после чтения из SerialUART. Пишем `data` во все подключённые TCP-клиенты (и делаем flush), для UDP — либо в известного клиента, либо broadcast. Увеличиваем счётчики и вызываем `bridgeLogSetLastTx` для лога.
- **bridgePollDisconnects():** Обходим слоты TCP, отключённых закрываем и очищаем слот; для UDP при отсутствии пакетов дольше 30 с сбрасываем `udpClientKnown`.

Таким образом, **все вызовы из main.cpp (bridgeAcceptClient, bridgePollDisconnects, bridgePollNetworkToUart, bridgeSendUartToNetwork) реализованы в этом файле** — пользователь видит полный код моста от начала до конца.

---

## Шаг 4. Сохранение и загрузка настроек (bridge_nvs)

Модуль **bridge_nvs** загружает настройки из NVS в глобальную структуру `bridge_nvs_config` и сохраняет её обратно. Из main.cpp вызывается только `loadBridgeConfig()` в setup(); из web_handlers при POST /api/settings вызывается `setBridgeConfigFromJson(body)`, внутри которой вызывается `saveBridgeConfig()`.

Структура `bridge_nvs_config` содержит: wifi_mode, ssid, wifi_pass, hostname, wifi_chan, ap_ip, baud, gpio_tx, gpio_rx, proto. В bridge_nvs.cpp:
- `setDefaults()` заполняет структуру значениями из config.h (SSID, PASSWD, UART_BAUD, SERIAL_TXPIN/RXPIN и т.д.);
- `loadBridgeConfig()` вызывает setDefaults(), открывает NVS (Preferences) с пространством "bridge", читает ключ "cfg" в структуру; если размера нет или он не совпадает — остаются defaults;
- `saveBridgeConfig()` записывает структуру в NVS под ключом "cfg";
- `setBridgeConfigFromJson(json)` разбирает JSON (поля esp32_mode, ssid, wifi_pass, baud, gpio_tx, gpio_rx и т.д.), обновляет структуру и вызывает saveBridgeConfig().

Полный код — в файлах **src/bridge_nvs.h** и **src/bridge_nvs.cpp** в репозитории. После добавления этого модуля main.cpp сможет вызвать loadBridgeConfig() и использовать bridge_nvs_config для Wi‑Fi и UART.

---

## Шаг 5. Разбор MAVLink и параметры SERVO (mavlink_state)

Модуль **mavlink_state** разбирает входящие байты с UART (от автопилота): распознаёт HEARTBEAT и PARAM_VALUE, обновляет флаг связи и параметры SERVO. Также отправляет в UART команды PARAM_REQUEST_READ и PARAM_SET по запросу веб-интерфейса.

Из main.cpp вызываются:
- в setup() при WEB_SERVER — `mavlinkInitLog()`;
- в loop() при WEB_SERVER — `mavlinkCheckDisconnect()`;
- в loop() для каждого блока байт из UART — `mavlinkProcessBytes(bufFromUART, lenFromUART)`.

Из web_handlers вызываются `mavlinkRequestServoParams()`, `mavlinkSendParamSet()`, а также читаются глобальные переменные mavlinkConnected, lastHeartbeatMs, paramServo*, mavlinkPacketsRx/Tx, mavlinkPacketDrops, mavlinkLog и т.д.

Полный код — в **src/mavlink_state.h** и **src/mavlink_state.cpp**. В .cpp используются библиотека MAVLink (mavlink_parse_char, mavlink_msg_*_decode, mavlink_msg_*_pack), extern SerialUART, вызовы bridgeLogSetConnected и espLogPrintf.

---

## Шаг 6. Логи: bridge_log и esp_log

- **bridge_log** — накопление данных для единого текстового лога (ID устройства, статус MAVLink, счётчики, образцы последнего RX/TX, хвост записей MAVLink и ESP32). Функция `bridgeLogGetText(buf, size)` формирует один большой текст; её вызывает обработчик `/api/log/file`. Остальные функции (SetConnected, SetLastRx/LastTx, UpdateStats, UpdateRssi, SetLastError) вызываются из bridge, mavlink_state и web_handlers.
- **esp_log** — кольцевой буфер строк с префиксом [uptime]; `espLogPrint`/`espLogPrintf` вызываются из main, bridge, mavlink_state; `espLogGetText` — из обработчика `/api/log/esp32`.

Полный код — в **src/bridge_log.h**, **src/bridge_log.cpp**, **src/esp_log.h**, **src/esp_log.cpp**.

---

## Шаг 7. Веб-сервер и обработчики (web_handlers)

Модуль **web_handlers** регистрирует все HTTP-маршруты и при запросе вызывает соответствующие обработчики: главная страница, /params, /bridge, /api/status, /api/link, /api/params (GET/POST), /api/param_request, /api/log, /api/log/file, /api/log/esp32, /api/settings (GET/POST), /api/settings/clients/udp и clear_udp, /api/system/info и /api/system/stats. В обработчиках используются bridge_nvs_config, mavlink_state, bridge (счётчики, число клиентов), bridge_log и esp_log. POST /api/settings вызывает setBridgeConfigFromJson() и затем, как в текущей реализации, плата перезагружается.

Полный код — в **src/web_handlers.h** и **src/web_handlers.cpp**. Для страницы /bridge нужен либо сгенерированный **include/bridge_ui_embed.h** (см. шаг 8), либо файл в LittleFS (data/index.html). Минимально в data/index.html можно положить одну строку HTML, чтобы скрипт embed не падал при отсутствии файла.

---

## Шаг 8. Встраивание веб-интерфейса (tools/embed_bridge_ui.py)

Скрипт **tools/embed_bridge_ui.py** читает **data/index.html** и генерирует **include/bridge_ui_embed.h** с константой `BRIDGE_UI_HTML[]` в PROGMEM. В web_handlers при запросе /bridge эта строка отдаётся клиенту (если определён BRIDGE_UI_EMBED_H). Код скрипта — в репозитории в **tools/embed_bridge_ui.py**. В platformio.ini уже указано `extra_scripts = pre:tools/embed_bridge_ui.py`, поэтому перед сборкой файл include/bridge_ui_embed.h создаётся автоматически.

Если data/index.html пока нет — создайте минимальный файл, например:

```html
<!DOCTYPE html><html><head><meta charset="utf-8"><title>Bridge</title></head><body><h1>Bridge UI</h1></body></html>
```

После этого сборка (Build) и прошивка (Upload) дадут полностью рабочий проект: Wi‑Fi, мост, NVS, MAVLink, веб с главной страницей, статусом, логами и страницей параметров SERVO.

---

## Итог по шагам

| Шаг | Что добавляем | Зачем |
|-----|----------------|-------|
| 0 | platformio.ini, папки src, include, data, tools | Основа проекта и сборки |
| 1 | config.h | Константы для всех модулей |
| 2 | main.cpp | Точка входа, setup/loop, вызовы моста и веб-сервера |
| 3 | bridge.h, bridge.cpp | Пересылка TCP/UDP ↔ UART |
| 4 | bridge_nvs.h, bridge_nvs.cpp | Загрузка/сохранение настроек в NVS |
| 5 | mavlink_state.h, mavlink_state.cpp | Разбор MAVLink, параметры SERVO |
| 6 | bridge_log, esp_log | Единый лог и кольцевой лог ESP32 |
| 7 | web_handlers.h, web_handlers.cpp, data/index.html | Веб-интерфейс и API |
| 8 | tools/embed_bridge_ui.py | Встраивание HTML в прошивку |

Полный код каждого файла можно взять из репозитория проекта; в этом туториале приведён полностью только **config.h** и **main.cpp**, а для остальных модулей указано, что они делают и откуда вызываются, чтобы вы могли сопоставить их с реальными файлами в репозитории и понять цепочку от создания проекта до работающего веб-интерфейса.
