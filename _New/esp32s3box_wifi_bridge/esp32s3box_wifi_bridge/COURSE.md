# Обучающий курс: ESP32-S3-Box WiFi-мост для MAVLink

Полный пошаговый курс для новичков: что это за проект, как он устроен, как собирать, прошивать и пользоваться. Каждая деталь объяснена так, чтобы можно было разобраться с нуля.

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

PlatformIO сам подтянет платформу ESP32 и библиотеки, указанные в `platformio.ini`.

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

---

## Часть 4. Модули проекта: кто за что отвечает

### 4.1. config.h

- Один общий заголовок с константами: SSID, пароль, порты TCP/UDP, скорость и пины UART, размер буферов, версия, флаги (WEB_SERVER, OTA_HANDLER и т.д.).
- Никто не «вызывает» config.h — его подключают через `#include` и используют значения `#define` и константы. При первой прошивке или если NVS пустой эти значения задают поведение по умолчанию.

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

- В PlatformIO: **Build** (галочка) или через панель PIO → Build. Перед компиляцией выполнится **embed_bridge_ui.py**, затем компиляция **src/** и линковка с библиотеками.
- Ошибки компиляции смотрите в выводе; часто это отсутствующий include или несовпадение типов — комментарии в коде подскажут, откуда какой тип берётся.

### 5.2. Прошивка

- Подключите плату по USB, выберите порт при необходимости (PIO → Upload).
- **Upload** — заливка прошивки через esptool. После успешной загрузки плата перезапустится.
- Опционально: **Upload Filesystem Image** — заливка содержимого **data/** в LittleFS (для раздачи Bridge UI из файловой системы, если не используете только embed).

### 5.3. Первый запуск

- По умолчанию (если NVS пустой) плата поднимет точку доступа с SSID и паролем из **config.h** (например SSID "ESP32S3_NEW", пароль "12345678").
- Подключитесь к этой Wi‑Fi сети с ноутбука/телефона, откройте в браузере **http://192.168.2.1**. Должна открыться главная страница со ссылками на лог, параметры и Bridge UI.
- В Mission Planner: Add Connection → TCP → IP `192.168.2.1`, порт `8880` (или UDP порт 14550 при настройке UDP). Автопилот должен быть подключён к UART платы (TX платы → RX автопилота, RX платы → TX автопилота, GND общий).

### 5.4. Настройки через веб

- **GET/POST /api/settings** — чтение и сохранение настроек (режим Wi‑Fi, SSID, пароль, hostname, baud, gpio_tx/rx). После POST обычно выполняется перезагрузка.
- Страница **/params** — запрос параметров SERVO с автопилота и отправка PARAM_SET (SERVO1_REVERSED, SERVO3_TRIM, SERVO4_TRIM).
- **/api/log/file** — скачать единый лог; **/api/log/esp32** — лог событий ESP32; **/api/status** — JSON со статусом, пакетами, параметрами.

---

## Часть 6. Схема взаимодействия компонентов (кратко)

- **main.cpp** вызывает при старте **loadBridgeConfig()**, **bridgeSetup()**, **webSetup()**; в цикле — **bridgeAcceptClient()**, **bridgePollDisconnects()**, **bridgePollNetworkToUart()**, затем чтение UART и вызовы **mavlinkProcessBytes()**, **bridgeSendUartToNetwork()**, при WEB_SERVER — **webServer.handleClient()**, **mavlinkCheckDisconnect()**.
- **bridge** пишет/читает в **SerialUART** (объявлен в main), обновляет счётчики байт; при ошибке UDP вызывает **bridgeLogSetLastError()**.
- **mavlink_state** при установке/потере связи вызывает **bridgeLogSetConnected()**; использует **SerialUART** для отправки PARAM_*.
- **web_handlers** читают **bridge_nvs_config**, **mavlinkConnected**, **paramServo***, счётчики из **bridge** и **mavlink_state**, вызывают **bridgeLogUpdateStats/Rssi** и **bridgeLogGetText()**, **espLogGetText()**, **setBridgeConfigFromJson()**, **bridgeSetUdpClient()** / **bridgeClearUdpClient()**.
- **bridge_log** и **esp_log** только накапливают данные и отдают их по запросу; их вызывают остальные модули, как указано выше.

---

## Часть 7. Частые вопросы

**Где поменять SSID и пароль по умолчанию?**  
В **config.h** — SSID, PASSWD. После первого сохранения настроек через веб они будут храниться в NVS и браться оттуда.

**Как переключить плату в режим клиента Wi‑Fi (STA)?**  
Через веб: настройки (например /api/settings или страница Bridge UI), задать режим STA, SSID и пароль роутера, сохранить. После перезагрузки плата подключится к роутеру; доступ по IP роутера или по mDNS (hostname), например http://esps3.local.

**Порт 8880 не подключается.**  
Проверьте, что вы подключены к Wi‑Fi платы (или к роутеру, к которому подключена плата в STA). Проверьте, что в config.h определён PROTOCOL_TCP и SERIAL_TCP_PORT 8880. В логе ESP32 и едином логе можно увидеть, запустился ли TCP-сервер.

**Нет связи с автопилотом (mavlinkConnected = false).**  
Проверьте проводку UART (TX↔RX, GND), скорость (baud) в настройках и в автопилоте (должны совпадать), правильность пинов в config.h или NVS. Убедитесь, что автопилот выдаёт MAVLink (например HEARTBEAT) на этот UART.

**Где описание API (JSON)?**  
Эндпоинты перечислены в комментариях в **web_handlers.cpp** в начале файла. Формат ответов можно посмотреть по коду обработчиков (handleApiStatus, handleApiLink и т.д.) или вызвав соответствующие URL в браузере (GET).

---

Этот курс можно использовать как пошаговое руководство и как справочник по модулям. Все важные вызовы и связи между файлами продублированы в комментариях в исходном коде.
