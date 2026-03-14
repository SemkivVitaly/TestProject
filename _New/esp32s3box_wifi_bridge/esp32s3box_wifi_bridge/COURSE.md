# Обучающий курс: ESP32-S3-Box WiFi-мост для MAVLink

Полный пошаговый курс для новичков: что это за проект, как он устроен, как собирать, прошивать и пользоваться. Каждая деталь объяснена так, чтобы можно было разобраться с нуля. В курс включены **конкретные примеры**: фрагменты кода, примеры запросов и ответов API, пошаговые сценарии и практические задания.

---

## Часть 1. О чём этот проект

### 1.1. Что такое «WiFi-мост» и зачем он нужен

В полётных системах дронов (квадрокоптеры, самолёты на автопилоте) земля и борт общаются по протоколу **MAVLink**: команды, телеметрия, параметры. Обычно связь идёт по **проводу** (USB или UART) от автопилота к компьютеру или планшету, где запущена наземная станция (Mission Planner, QGroundControl и т.п.).

**WiFi-мост** — это устройство (здесь — плата ESP32), которое:

1. **По Wi‑Fi** принимает подключения от наземной станции (ноутбук, телефон).
2. **По проводу (UART)** подключено к автопилоту.
3. **Пересылает байты** туда-обратно без изменений: то, что пришло по Wi‑Fi, уходит в UART, и наоборот.

В итоге вы подключаетесь к Wi‑Fi точки доступа ESP32 и в Mission Planner выбираете, например, TCP с адресом `192.168.2.1` и портом `8880` — и работаете с дроном как будто он подключён по кабелю.

**Важно:** интернет не нужен. Вся связь локальная: GCS ↔ ESP32 ↔ автопилот.

**Пример цепочки данных:**  
Пользователь в Mission Planner нажимает «Arm» → Mission Planner отправляет MAVLink-пакет по TCP на `192.168.2.1:8880` → ESP32 получает байты в `bridgePollNetworkToUart()` и пишет их в `SerialUART` → автопилот получает пакет по UART и выполняет команду. Ответ автопилота идёт в обратную сторону: UART → ESP32 → `bridgeSendUartToNetwork()` → TCP клиенту (Mission Planner).

**Сценарий «от коробки до связи» (пошагово):**

1. Собрать проект в PlatformIO (Build), подключить ESP32 по USB, залить прошивку (Upload).
2. Подключить автопилот к UART платы: TX платы (GPIO 13) → RX автопилота, RX платы (GPIO 14) → TX автопилота, GND общий. Убедиться, что в автопилоте выбран тот же бодрейт (например 57600).
3. На ноутбуке/телефоне найти Wi‑Fi сеть с именем из config.h (например «ESP32S3_NEW»), подключиться (пароль из config.h, например «12345678»).
4. В браузере открыть `http://192.168.2.1`. Должна открыться главная страница моста.
5. В Mission Planner: Connections → Add Connection → выберите «TCP» (или UDP). Для TCP: IP `192.168.2.1`, порт `8880`. Подключиться.
6. Если автопилот отдаёт MAVLink на этом UART, в Mission Planner появится телеметрия (полоса «Connected»), а на странице `http://192.168.2.1/api/status` поле `connected` станет `true` и начнут расти `packets_rx`.

### 1.2. Что ещё умеет эта прошивка

- **Веб-интерфейс** по адресу `http://192.168.2.1` (в режиме точки доступа): главная страница, лог, статус, страница параметров SERVO и страница Bridge UI.
- **Сохранение настроек** в энергонезависимую память (NVS): режим Wi‑Fi (AP/STA), SSID, пароль, скорость UART, пины — после перезагрузки настройки не теряются.
- **Разбор MAVLink** «по пути»: прошивка распознаёт пакеты HEARTBEAT и PARAM_VALUE, чтобы показывать «подключён / не подключён» и читать/записывать параметры SERVO через веб.

---

## Часть 2. Что нужно установить и как открыть проект

### 2.1. Установка PlatformIO

Проект собирается через **PlatformIO** (не через классическую Arduino IDE, хотя используется фреймворк Arduino для ESP32).

1. Установите **VS Code** (или Cursor), если ещё не установлен.
2. В расширениях найдите **PlatformIO IDE** и установите.
3. После установки в боковой панели появится иконка PlatformIO (домик). Откройте проект: **File → Open Folder** и выберите папку `esp32s3box_wifi_bridge` (ту, где лежит `platformio.ini`).

**Пример полного пути к папке проекта (Windows):**  
`C:\Users\vital\OneDrive\Desktop\esp32\_New\esp32s3box_wifi_bridge\esp32s3box_wifi_bridge`  
Открывать нужно именно папку, внутри которой лежит файл `platformio.ini`.

PlatformIO сам подтянет платформу ESP32 и библиотеки, указанные в `platformio.ini`. При первом открытии внизу может появиться прогресс «Installing platform», «Installing dependencies» — дождитесь окончания.

### 2.2. Структура папок проекта

После открытия вы увидите примерно такую структуру:

```
esp32s3box_wifi_bridge/
├── platformio.ini    ← конфигурация сборки (плата, библиотеки, скрипты)
├── src/              ← исходный код прошивки
│   ├── main.cpp      ← точка входа (setup, loop)
│   ├── config.h      ← константы: SSID, порты, пины, буферы
│   ├── bridge.cpp/h  ← мост TCP/UDP ↔ UART
│   ├── bridge_nvs.cpp/h  ← загрузка/сохранение настроек в NVS
│   ├── mavlink_state.cpp/h  ← разбор MAVLink, параметры SERVO
│   ├── web_handlers.cpp/h   ← HTTP-маршруты и ответы
│   ├── bridge_log.cpp/h    ← единый лог для скачивания
│   └── esp_log.cpp/h       ← кольцевой лог событий ESP32
├── include/          ← заголовки; сюда скрипт пишет bridge_ui_embed.h
├── data/             ← веб-страницы и ресурсы (index.html — Bridge UI)
└── tools/
    └── embed_bridge_ui.py   ← скрипт: data/index.html → include/bridge_ui_embed.h
```

- **platformio.ini** — «сердце» конфигурации: какая плата, какие библиотеки, что запускать до сборки.
- **src/** — весь код на C++. Каждый модуль (bridge, mavlink_state, web_handlers и т.д.) описан в комментариях в начале файлов и у важных функций.
- **data/** — при «Upload Filesystem Image» эта папка заливается в LittleFS на плате; веб-сервер может отдавать страницы оттуда. Дополнительно скрипт встраивает `index.html` в прошивку как строку в PROGMEM.

**Пример: где искать точку входа программы.**  
Главный файл — `src/main.cpp`. В нём определены функции `setup()` и `loop()`. При сборке PlatformIO компилирует все `.cpp` из `src/` и линкует их вместе; `main.cpp` не имеет особого имени с точки зрения линкера, но по соглашению в нём находятся `setup()` и `loop()`, которые вызываются средой Arduino.

---

## Часть 3. Как работает программа: от включения до цикла

### 3.1. Точка входа: setup() и loop()

На ESP32 (как и в Arduino) после включения выполняется один раз **setup()**, затем бесконечно повторяется **loop()**.

В **setup()** в этом проекте по порядку происходит:

1. **Serial.begin(115200)** — скорость порта для отладки (монитор в PlatformIO должен быть 115200).
2. **loadBridgeConfig()** — из NVS читаются настройки в глобальную структуру `bridge_nvs_config` (режим Wi‑Fi, SSID, пароль, скорость UART, пины). Если NVS пустой или формат другой — подставляются значения по умолчанию из `bridge_nvs.cpp` (они берутся из `config.h`).
3. **Инициализация UART к автопилоту** — `SerialUART.begin(baud, ...)` на пинах из NVS или из `config.h`. Это второй последовательный порт (UART1); первый (Serial) используется для отладки.
4. **Режим Wi‑Fi:**
   - Если в настройках режим **STA (2)** — плата подключается к вашему роутеру (SSID/пароль из NVS), включается mDNS (имя из hostname, например `esps3.local`), при обрыве связи вызывается обработчик и переподключение.
   - Иначе — режим **AP (1)**: плата создаёт точку доступа с заданным SSID/паролем, клиенты получают IP из подсети 192.168.2.x, сама плата — 192.168.2.1.
5. **bridgeSetup()** — запуск TCP-сервера на порту 8880, привязка UDP на 14550, при необходимости Bluetooth.
6. При включённом **WEB_SERVER**: **LittleFS.begin()**, затем **webSetup(webServer)** — регистрация всех маршрутов (/, /params, /api/status и т.д.) и вызов `server.begin()`.

В **loop()** каждый «тик» выполняется примерно так:

1. При **OTA_HANDLER** — проверка входящего OTA-обновления.
2. При **WEB_SERVER** — **webServer.handleClient()** (обработка одного HTTP-запроса, если есть), **mavlinkCheckDisconnect()** (сброс флага связи при отсутствии HEARTBEAT дольше таймаута).
3. **bridgeAcceptClient()** — если к TCP-серверу подключился новый клиент и есть свободный слот — принять его.
4. **bridgePollDisconnects()** — удалить отключённых TCP-клиентов, сбросить UDP-клиента по таймауту неактивности.
5. **bridgePollNetworkToUart()** — прочитать данные из всех TCP-клиентов (и BT, если включён) и записать их в `SerialUART` к автопилоту.
6. **Чтение из SerialUART:** пока есть данные, читаем порциями в буфер `bufFromUART`, затем:
   - **bridgeLogSetLastRx(...)** — сохранить образец для единого лога;
   - **mavlinkProcessBytes(...)** — разобрать MAVLink (HEARTBEAT, PARAM_VALUE и т.д.), обновить состояние и счётчики;
   - **bridgeSendUartToNetwork(...)** — отправить те же байты всем TCP/UDP (и BT) клиентам.

Таким образом, **мост** только пересылает байты; **mavlink_state** параллельно парсит их для статуса и параметров, не мешая пересылке.

**Пример фрагмента loop() — порядок вызовов (упрощённо):**

```cpp
void loop() {
    webServer.handleClient();           // 1. Обработать HTTP, если пришёл запрос
    mavlinkCheckDisconnect();          // 2. Проверить таймаут HEARTBEAT
    bridgeAcceptClient();              // 3. Принять нового TCP-клиента
    bridgePollDisconnects();           // 4. Убрать отключённых
    bridgePollNetworkToUart();         // 5. Сеть → UART (команды к дрону)
    while (SerialUART.available()) {
        // 6. Читаем с UART, парсим MAVLink, шлём в сеть (телеметрия к GCS)
        mavlinkProcessBytes(...);
        bridgeSendUartToNetwork(...);
    }
    delay(1);
}
```

Если поменять порядок (например, сначала читать UART, потом принимать клиентов), возможны задержки телеметрии или запоздалое принятие нового подключения.

---

## Часть 4. Модули проекта: кто за что отвечает

### 4.1. config.h

- Один общий заголовок с константами: SSID, пароль, порты TCP/UDP, скорость и пины UART, размер буферов, версия, флаги (WEB_SERVER, OTA_HANDLER и т.д.).
- Никто не «вызывает» config.h — его подключают через `#include` и используют значения `#define` и константы. При первой прошивке или если NVS пустой эти значения задают поведение по умолчанию.
- **Пример использования в коде:** в `main.cpp` при инициализации UART берутся пины и бодрейт: если в NVS заданы `gpio_tx`/`gpio_rx`/`baud`, используются они; иначе — константы из config.h: `SERIAL_TXPIN`, `SERIAL_RXPIN`, `UART_BAUD`. То же для SSID/PASSWD в `bridge_nvs.cpp` в функции `setDefaults()` — оттуда копируются `SSID`, `PASSWD`, `HOSTNAME` и т.д. в структуру `bridge_nvs_config`.

### 4.2. main.cpp

- Объявляет **SerialUART** (второй UART), буфер **bufFromUART**, при WEB_SERVER — **WebServer** и т.д.
- Реализует **setup()** и **loop()** и обработчик **onWiFiDisconnected** для режима STA.
- Вызывает функции из других модулей в порядке, описанном выше. Исходный код и комментарии в файле объясняют каждый шаг.

### 4.3. bridge (bridge.h / bridge.cpp)

- **Назначение:** пересылка байтов между сетью (TCP/UDP и опционально Bluetooth) и UART к автопилоту.
- **bridgeSetup()** — старт TCP-сервера, привязка UDP, коллбек на приём пакетов; при приходе UDP-пакета данные сразу пишутся в SerialUART.
- **bridgeAcceptClient()** — принять нового TCP-клиента в свободный слот.
- **bridgePollNetworkToUart()** — читать из TCP-клиентов (и BT) и писать в SerialUART.
- **bridgeSendUartToNetwork()** — отправить переданный буфер всем TCP-клиентам и известному UDP-клиенту (или broadcast).
- Счётчики **bridgeBytesTxNetwork**, **bridgeBytesRxNetwork**, **bridgeBytesFromUart** увеличиваются при пересылке и читаются веб-обработчиками для метрик.
- Управление UDP-клиентом: **bridgeSetUdpClient(ip, port)**, **bridgeClearUdpClient()**, **bridgeGetUdpClientInfo()** — вызываются из веб-API настроек.

**Пример из кода (bridge.cpp):** при получении данных от TCP-клиента они накапливаются в буфер `bufToUART`, затем одной операцией отправляются в UART: `SerialUART.write(bufToUART, lenToUART)`. Счётчик `bridgeBytesRxNetwork` увеличивается на `lenToUART`. Так веб-интерфейс может показывать «сколько байт прошло из сети в автопилот».

### 4.4. bridge_nvs (bridge_nvs.h / bridge_nvs.cpp)

- **Назначение:** загрузка и сохранение настроек в NVS (ключ "cfg" в пространстве "bridge").
- **loadBridgeConfig()** — вызывается в setup(); заполняет глобальную структуру **bridge_nvs_config** (режим Wi‑Fi, SSID, пароль, hostname, baud, gpio_tx/rx и т.д.). Если в NVS ничего нет или размер не совпадает — остаются значения по умолчанию из **setDefaults()**.
- **saveBridgeConfig()** — записывает **bridge_nvs_config** в NVS.
- **setBridgeConfigFromJson()** — разбирает JSON (тело POST /api/settings), обновляет поля структуры и вызывает **saveBridgeConfig()**. Вызывается из веб-обработчика; после сохранения обычно идёт перезагрузка.

### 4.5. mavlink_state (mavlink_state.h / mavlink_state.cpp)

- **Назначение:** разбор входящих MAVLink-пакетов с UART и отправка команд по параметрам в UART.
- Входящий поток байтов обрабатывается в **mavlinkProcessBytes()**: по одному байту передаётся в парсер; при полном пакете обрабатываются HEARTBEAT (установка/обновление связи, lastHeartbeatMs) и PARAM_VALUE (извлечение SERVO1_REVERSED, SERVO3_TRIM, SERVO4_TRIM). Обновляются счётчики пакетов и потерь.
- **mavlinkCheckDisconnect()** — если прошло больше MAVLINK_HEARTBEAT_TIMEOUT_MS с последнего HEARTBEAT, связь сбрасывается (mavlinkConnected = false).
- **mavlinkSendParamRequest()**, **mavlinkRequestServoParams()**, **mavlinkSendParamSet()** — формирование и отправка в SerialUART пакетов PARAM_REQUEST_READ и PARAM_SET. Вызываются со страницы параметров и из API.
- Глобальные переменные (mavlinkConnected, paramServo*, mavlinkPacketsRx/Tx и т.д.) читаются в **web_handlers** для формирования JSON и страниц.

**Пример из кода (mavlink_state.cpp):** при разборе HEARTBEAT обновляются глобальные переменные и лог:
  - `mavlinkConnected = true`, `lastHeartbeatMs = millis()`, `autopilotSysId`/`autopilotCompId` из заголовка пакета;
  - вызывается `bridgeLogSetConnected(true)` и `espLogPrintf("[MAVLink] connected (HEARTBEAT)")`;
  - в кольцевой лог MAVLink добавляется запись «RX HEARTBEAT» (не чаще раза в 10 секунд). Так веб и единый лог всегда отражают актуальное состояние связи.

### 4.6. web_handlers (web_handlers.h / web_handlers.cpp)

- **Назначение:** HTTP-маршруты и ответы веб-сервера.
- **webSetup(server)** — сохраняет указатель на сервер и регистрирует все маршруты: `/`, `/params`, `/bridge`, `/api/status`, `/api/link`, `/api/params`, `/api/param_request`, `/api/log`, `/api/log/file`, `/api/log/esp32`, `/api/system/info`, `/api/system/stats`, `/api/settings` (GET/POST), `/api/settings/clients/udp` (POST), `/api/settings/clients/clear_udp` (DELETE) и т.д.
- Обработчики читают данные из **bridge**, **mavlink_state**, **bridge_nvs**, **bridge_log**, **esp_log** и отправляют HTML или JSON. POST /api/settings вызывает **setBridgeConfigFromJson()** и перезагрузку.

### 4.7. bridge_log (bridge_log.h / bridge_log.cpp)

- **Назначение:** накопление данных для единого текстового лога (ID устройства, статус MAVLink, счётчики, RSSI, образцы RX/TX, хвост логов MAVLink и ESP32).
- **bridgeLogSetConnected()**, **bridgeLogSetLastRx/LastTx()**, **bridgeLogUpdateStats()**, **bridgeLogUpdateRssi()**, **bridgeLogSetLastError()** вызываются из bridge, mavlink_state и web_handlers.
- **bridgeLogGetText()** формирует один большой текст и вызывается обработчиком **/api/log/file**.

### 4.8. esp_log (esp_log.h / esp_log.cpp)

- **Назначение:** кольцевой буфер строк с префиксом [uptime] для событий прошивки (WiFi, мост, MAVLink). При переполнении старые записи затираются.
- **espLogPrint()** / **espLogPrintf()** вызываются из main, bridge, mavlink_state. **espLogGetText()** вызывается из веб-обработчика **/api/log/esp32**.

### 4.9. platformio.ini и tools/embed_bridge_ui.py

- **platformio.ini** задаёт платформу, плату, фреймворк, флаги сборки, LittleFS, **extra_scripts = pre:tools/embed_bridge_ui.py** и библиотеки (ESP32Servo, MAVLink). Комментарии в файле поясняют каждую опцию.
- **embed_bridge_ui.py** перед сборкой читает **data/index.html** и генерирует **include/bridge_ui_embed.h** с константой **BRIDGE_UI_HTML[]** в PROGMEM. Веб-сервер при запросе /bridge может отдавать эту строку (или файл из LittleFS).

---

## Часть 5. Сборка, прошивка и запуск

### 5.1. Сборка

- В PlatformIO: **Build** (галочка на панели внизу) или через панель PIO → Build. Перед компиляцией выполнится **embed_bridge_ui.py**, затем компиляция **src/** и линковка с библиотеками.
- В терминале вы увидите что-то вроде: `Generating bridge_ui_embed.h` (скрипт), затем `Compiling .pioenvs\...\main.cpp`, `Linking .pioenvs\...\firmware.elf`, `Building .pioenvs\...\firmware.bin`. Успешная сборка заканчивается строкой «SUCCESS».
- Ошибки компиляции смотрите в выводе; часто это отсутствующий include или несовпадение типов — комментарии в коде подскажут, откуда какой тип берётся.

### 5.2. Прошивка

- Подключите плату по USB, выберите порт при необходимости (PIO → Upload).
- **Upload** — заливка прошивки через esptool. После успешной загрузки плата перезапустится.
- Опционально: **Upload Filesystem Image** — заливка содержимого **data/** в LittleFS (для раздачи Bridge UI из файловой системы, если не используете только embed).

### 5.3. Первый запуск

- По умолчанию (если NVS пустой) плата поднимет точку доступа с SSID и паролем из **config.h** (например SSID "ESP32S3_NEW", пароль "12345678").
- Подключитесь к этой Wi‑Fi сети с ноутбука/телефона, откройте в браузере **http://192.168.2.1**. Должна открыться главная страница со ссылками на лог, параметры и Bridge UI.
- В Mission Planner: Add Connection → TCP → IP `192.168.2.1`, порт `8880` (или UDP порт 14550 при настройке UDP). Автопилот должен быть подключён к UART платы (TX платы → RX автопилота, RX платы → TX автопилота, GND общий).

**Пример вывода в Serial Monitor после загрузки (режим AP):**

```
WiFi bridge 2.0-ESP32S3
[boot] WiFi bridge 2.0-ESP32S3
TCP port 8880
UDP port 14550
AP IP:
192.168.2.1
Web (local): http://192.168.2.1
```

Скорость монитора должна быть **115200** (настройка в PlatformIO: Monitor → Configure or platformio.ini → monitor_speed = 115200).

**Подключение UART к автопилоту (по умолчанию из config.h):**

| Сигнал   | ESP32 (пин) | Автопилот      |
|----------|-------------|----------------|
| TX       | GPIO 13     | RX (приём)     |
| RX       | GPIO 14     | TX (передача)  |
| GND      | GND         | GND            |

Скорость UART по умолчанию — **57600** (UART_BAUD в config.h). Она должна совпадать с настройкой порта телеметрии в автопилоте (например в ArduPilot: SERIAL1_PROTOCOL = 1, SERIAL1_BAUD = 57).

### 5.4. Настройки через веб

- **GET/POST /api/settings** — чтение и сохранение настроек (режим Wi‑Fi, SSID, пароль, hostname, baud, gpio_tx/rx). После POST обычно выполняется перезагрузка.
- Страница **/params** — запрос параметров SERVO с автопилота и отправка PARAM_SET (SERVO1_REVERSED, SERVO3_TRIM, SERVO4_TRIM).
- **/api/log/file** — скачать единый лог; **/api/log/esp32** — лог событий ESP32; **/api/status** — JSON со статусом, пакетами, параметрами.

---

## Часть 5.5. Примеры запросов и ответов API

Все запросы выполняются к базовому адресу платы: в режиме AP это `http://192.168.2.1`, в режиме STA — `http://<IP_платы>` или `http://esps3.local` (если mDNS работает).

### Пример 1: GET /api/status

**Запрос (в браузере или curl):**  
`http://192.168.2.1/api/status`

**Пример ответа (JSON):**

```json
{
  "uptime": 120,
  "free_heap": 180000,
  "connected": true,
  "tcp_connected": 1,
  "udp_known": false,
  "last_error": "none",
  "last_heartbeat_ms": 45000,
  "packets_rx": 1523,
  "packets_tx": 5,
  "packets_processed": 1523,
  "packet_drops": 0,
  "packet_loss_pct": "0.00",
  "latency_ms": 120,
  "bytes_network_tx": 45000,
  "bytes_network_rx": 3200,
  "SERVO1_REVERSED": 0,
  "SERVO1_REVERSED_known": true,
  "SERVO3_TRIM": 1500,
  "SERVO3_TRIM_known": true,
  "SERVO4_TRIM": 1500,
  "SERVO4_TRIM_known": true,
  "log": ["10 RX HEARTBEAT", "15 RX PARAM_VALUE SERVO3_TRIM", "20 TX PARAM_SET SERVO3_TRIM"]
}
```

- **connected** — есть ли связь с автопилотом по MAVLink (получен HEARTBEAT).
- **tcp_connected** — число подключённых TCP-клиентов (0..4).
- **packets_rx** / **packets_tx** — принятые и отправленные MAVLink-пакеты.
- **packet_loss_pct** — процент потерянных пакетов (по CRC).
- **latency_ms** — время с последнего HEARTBEAT (примерная задержка).

### Пример 2: GET /api/settings (текущие настройки)

**Запрос:**  
`http://192.168.2.1/api/settings`

**Пример ответа (сокращённо):**

```json
{
  "esp32_mode": 1,
  "ssid": "ESP32S3_NEW",
  "wifi_pass": "12345678",
  "wifi_hostname": "esps3",
  "ap_ip": "192.168.2.1",
  "gpio_tx": 13,
  "gpio_rx": 14,
  "baud": 57600,
  "proto": 4
}
```

**esp32_mode:** 1 = точка доступа (AP), 2 = клиент (STA).

### Пример 3: POST /api/settings (сохранить настройки и перезагрузиться)

Переключить плату в режим STA и подключить к роутеру:

**Запрос (curl):**

```bash
curl -X POST http://192.168.2.1/api/settings \
  -H "Content-Type: application/json" \
  -d "{\"esp32_mode\":2,\"ssid\":\"MyRouter\",\"wifi_pass\":\"MyPassword\",\"wifi_hostname\":\"esps3\"}"
```

**Тело (JSON) по полям:**

- `esp32_mode`: 2 — режим STA.
- `ssid`: имя Wi‑Fi сети роутера.
- `wifi_pass`: пароль от роутера.
- `wifi_hostname`: имя для mDNS (например `esps3` → доступ по `http://esps3.local`).

После успешного ответа плата перезагрузится и подключится к роутеру. Дальше веб и мост доступны по IP, который выдал роутер, или по `http://esps3.local`.

### Пример 4: POST /api/params (установить параметр SERVO)

**Запрос (curl):**

```bash
curl -X POST "http://192.168.2.1/api/params" \
  -d "SERVO3_TRIM=1520&SERVO4_TRIM=1480"
```

Или через форму на странице **/params**: вводите значения в поля и нажимаете «Установить». Внутри отправляется POST на `/api/params` с полями `SERVO1_REVERSED`, `SERVO3_TRIM`, `SERVO4_TRIM`. Прошивка формирует MAVLink PARAM_SET и отправляет в UART автопилоту.

### Пример 5: GET /api/log/file

**Запрос:**  
`http://192.168.2.1/api/log/file`

**Ответ:** текстовый файл (plain text) с единым логом: ID устройства, статус MAVLink, счётчики пакетов, RSSI, счётчики по типам MAVLink, образцы последнего RX/TX (hex), последние события MAVLink и ESP32. Удобно сохранить в файл для разбора: в браузере «Сохранить как» или `curl -o log.txt http://192.168.2.1/api/log/file`.

### Пример 6: POST /api/settings/clients/udp (задать UDP-клиента вручную)

Если GCS по UDP не «светится» автоматически (мост не знает, куда слать ответы), можно задать IP и порт клиента:

```bash
curl -X POST http://192.168.2.1/api/settings/clients/udp \
  -H "Content-Type: application/json" \
  -d "{\"ip\":\"192.168.2.100\",\"port\":14550}"
```

Здесь `192.168.2.100` — IP компьютера/планшета с QGroundControl (или другим GCS по UDP). Порт 14550 — стандартный для MAVLink UDP.

**Сводная таблица основных HTTP-маршрутов**

| Метод | URL | Назначение |
|-------|-----|------------|
| GET | `/` | Главная страница со ссылками (лог, параметры, Bridge UI) |
| GET | `/params` | Страница параметров SERVO (форма запроса и установки) |
| GET | `/bridge` | Страница Bridge UI (из embed или LittleFS) |
| GET | `/api/status` | JSON: uptime, connected, packets, bytes, параметры SERVO, лог |
| GET | `/api/link` | JSON: связь, пакеты, потери, latency, bytes |
| GET | `/api/params` | JSON: текущие значения SERVO1_REVERSED, SERVO3_TRIM, SERVO4_TRIM |
| POST | `/api/params` | Установить параметры (тело: SERVO1_REVERSED=…&SERVO3_TRIM=…&SERVO4_TRIM=…) |
| GET | `/api/param_request` | Запрос параметров SERVO с автопилота (отправка PARAM_REQUEST_READ). Вызывается кнопкой «Запросить с автопилота» на странице /params. |
| GET | `/api/log` | JSON-массив последних записей кольцевого лога MAVLink |
| GET | `/api/log/file` | Текстовый файл — единый лог (ID, статистика, образцы RX/TX, события) |
| GET | `/api/log/esp32` | Текстовый файл — кольцевой лог событий ESP32 |
| GET | `/api/settings` | JSON: текущие настройки (режим, SSID, baud, пины и т.д.) |
| POST | `/api/settings` | Сохранить настройки (тело JSON); после ответа плата перезагружается |
| POST | `/api/settings/clients/udp` | Задать UDP-клиента (JSON: ip, port) |
| DELETE | `/api/settings/clients/clear_udp` | Сбросить UDP-клиента |
| GET | `/api/system/info` | JSON: версия, MAC, модель чипа (для Bridge UI) |
| GET | `/api/system/stats` | JSON: read_bytes, tcp_connected, udp_connected, esp_rssi (для Bridge UI) |

---

## Часть 6. Примеры изменения config.h

Перед первой прошивкой или для сброса настроек к «заводским» можно изменить значения по умолчанию в **src/config.h**.

**Пример 1: свой SSID и пароль точки доступа**

```c
#define SSID     "MyDroneBridge"
#define PASSWD   "MySecretPass123"
```

После прошивки плата создаст сеть с таким именем и паролем (если в NVS ещё ничего не сохранено).

**Пример 2: другая скорость UART к автопилоту**

Если в автопилоте порт телеметрии настроен на 115200:

```c
#define UART_BAUD 115200
```

Или оставьте 57600 в config.h и после первого входа в веб сохраните baud через **/api/settings** (POST с `"baud": 115200`) — тогда значение попадёт в NVS и будет использоваться после перезагрузки.

**Пример 3: другие пины UART (например, для другой платы)**

```c
#define SERIAL_TXPIN 17
#define SERIAL_RXPIN 16
```

TX платы подключается к RX автопилота, RX платы — к TX автопилота.

**Пример 4: отключить веб-сервер (экономия памяти)**

Закомментируйте строку:

```c
// #define WEB_SERVER
```

Тогда не компилируются WebServer, LittleFS, страницы и часть API; мост TCP/UDP ↔ UART и разбор MAVLink продолжают работать.

---

## Часть 7. Схема взаимодействия компонентов (кратко)

- **main.cpp** вызывает при старте **loadBridgeConfig()**, **bridgeSetup()**, **webSetup()**; в цикле — **bridgeAcceptClient()**, **bridgePollDisconnects()**, **bridgePollNetworkToUart()**, затем чтение UART и вызовы **mavlinkProcessBytes()**, **bridgeSendUartToNetwork()**, при WEB_SERVER — **webServer.handleClient()**, **mavlinkCheckDisconnect()**.
- **bridge** пишет/читает в **SerialUART** (объявлен в main), обновляет счётчики байт; при ошибке UDP вызывает **bridgeLogSetLastError()**.
- **mavlink_state** при установке/потере связи вызывает **bridgeLogSetConnected()**; использует **SerialUART** для отправки PARAM_*.
- **web_handlers** читают **bridge_nvs_config**, **mavlinkConnected**, **paramServo***, счётчики из **bridge** и **mavlink_state**, вызывают **bridgeLogUpdateStats/Rssi** и **bridgeLogGetText()**, **espLogGetText()**, **setBridgeConfigFromJson()**, **bridgeSetUdpClient()** / **bridgeClearUdpClient()**.
- **bridge_log** и **esp_log** только накапливают данные и отдают их по запросу; их вызывают остальные модули, как указано выше.

---

## Часть 8. Практические задания (сделай сам)

Выполняя их по порядку, вы закрепите понимание проекта.

1. **Сборка и монитор**  
   Соберите проект (Build), подключите плату по USB, откройте Serial Monitor (115200). Залейте прошивку (Upload) и посмотрите вывод при старте. Найдите в выводе строки «TCP port 8880», «UDP port 14550», «AP IP:» и IP-адрес.

2. **Вход в веб и статус**  
   Подключитесь к Wi‑Fi платы (SSID/пароль из config.h), откройте в браузере `http://192.168.2.1`. Откройте ссылку на «Статус» или перейдите на `http://192.168.2.1/api/status`. Опишите, что означают поля `connected`, `tcp_connected`, `packets_rx`.

3. **Единый лог**  
   Откройте `http://192.168.2.1/api/log/file`. Сохраните страницу как текстовый файл. Найдите в файле блок «Статистика пакетов» и «Счётчики по типам MAVLink».

4. **Настройки по API**  
   Вызовите GET `http://192.168.2.1/api/settings` и посмотрите текущие значения. Если хотите переключить плату в режим STA, подготовьте JSON с `esp32_mode`, `ssid`, `wifi_pass` и отправьте POST на `/api/settings` (через curl или расширение для браузера для REST).

5. **Параметры SERVO (при подключённом автопилоте)**  
   Откройте страницу `/params`. Нажмите «Запросить с автопилота». Дождитесь появления значений в полях и отметок «получен». Измените одно значение и нажмите «Установить». В едином логе или на странице статуса проверьте, что появилась запись вида «TX PARAM_SET …».

6. **Порядок в loop()**  
   В **src/main.cpp** в функции `loop()` найдите вызовы `bridgePollNetworkToUart()` и `bridgeSendUartToNetwork()`. Объясните своими словами: почему данные с UART сначала проходят через `mavlinkProcessBytes()`, а потом отправляются в сеть.

---

## Часть 9. Частые вопросы

**Где поменять SSID и пароль по умолчанию?**  
В **config.h** — SSID, PASSWD. После первого сохранения настроек через веб они будут храниться в NVS и браться оттуда.

**Как переключить плату в режим клиента Wi‑Fi (STA)?**  
Через веб: настройки (например /api/settings или страница Bridge UI), задать режим STA, SSID и пароль роутера, сохранить. После перезагрузки плата подключится к роутеру; доступ по IP роутера или по mDNS (hostname), например http://esps3.local.

**Порт 8880 не подключается.**  
Проверьте, что вы подключены к Wi‑Fi платы (или к роутеру, к которому подключена плата в STA). Проверьте, что в config.h определён PROTOCOL_TCP и SERIAL_TCP_PORT 8880. В логе ESP32 и едином логе можно увидеть, запустился ли TCP-сервер.

**Нет связи с автопилотом (mavlinkConnected = false).**  
Проверьте проводку UART (TX↔RX, GND), скорость (baud) в настройках и в автопилоте (должны совпадать), правильность пинов в config.h или NVS. Убедитесь, что автопилот выдаёт MAVLink (например HEARTBEAT) на этот UART.

**Где описание API (JSON)?**  
Эндпоинты перечислены в комментариях в **web_handlers.cpp** в начале файла. Формат ответов можно посмотреть по коду обработчиков (handleApiStatus, handleApiLink и т.д.) или вызвав соответствующие URL в браузере (GET). В этом курсе в **Части 5.5** приведены примеры запросов и ответов для основных эндпоинтов.

**Как сбросить настройки NVS к умолчанию?**  
Через код: в `bridge_nvs.cpp` можно временно вызвать только `setDefaults()` и `saveBridgeConfig()` при старте (или добавить отдельную «кнопку сброса» в веб). Либо через веб заново отправить POST /api/settings с желаемыми значениями (режим AP, SSID/пароль из config.h и т.д.) — они перезапишут NVS.

**Почему после POST /api/settings плата перезагружается?**  
Чтобы применить новый режим Wi‑Fi (AP/STA), SSID и пароль: они читаются только в `setup()` при загрузке из `bridge_nvs_config`. Перезагрузка гарантирует, что WiFi.mode(), WiFi.softAP() или WiFi.begin() выполнятся уже с новыми данными.

**Можно ли использовать другой порт для TCP (не 8880)?**  
Да. В **config.h** измените `SERIAL_TCP_PORT` (и при необходимости `SERIAL0_TCP_PORT`) и пересоберите прошивку. В Mission Planner укажите этот порт при добавлении TCP-подключения. Через NVS порт не меняется в текущей версии — только через config.h.

**Что такое Bridge UI и где он лежит?**  
Bridge UI — это веб-страница с расширенным интерфейсом (графики, настройки в одном месте). Её HTML лежит в **data/index.html**. Перед сборкой скрипт **embed_bridge_ui.py** встраивает содержимое этого файла в **include/bridge_ui_embed.h** как строку в PROGMEM. При запросе `/bridge` веб-сервер отдаёт эту строку (или файл из LittleFS, если был залит Upload Filesystem Image). Таким образом интерфейс доступен даже без отдельной файловой системы.

---

Этот курс можно использовать как пошаговое руководство и как справочник по модулям. Все важные вызовы и связи между файлами продублированы в комментариях в исходном коде. Раздел с примерами API (Часть 5.5) и практические задания (Часть 8) помогут закрепить материал на практике.
