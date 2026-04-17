# ESP32-S3 Wi-Fi Bridge для MAVLink

Неблокирующий мост UART ↔ Wi-Fi (TCP/UDP) для ESP32-S3 на базе Arduino-framework.
Предназначен для подключения GCS (Mission Planner, QGroundControl, Betaflight Configurator)
к автопилоту (ArduPilot / PX4) по Wi-Fi.

## Архитектура

- **Core 0**: Wi-Fi / LWIP / AsyncTCP / AsyncUDP — все сетевые колбеки.
- **Core 1**:
  - `loopTask` — супервизор (STA reconnect FSM, thermal guard, heap monitor, RSSI history).
  - `uart-task` — чтение `SerialUART`, разбор MAVLink (seq-gap detection), отправка в сеть.
- **AsyncTCP** обслуживает N клиентов, у каждого — `StreamBuffer` под TX (UART→GCS).
- **AsyncUDP** — один известный клиент (первый, кто прислал пакет, либо заданный через UI).
- Все счётчики — `std::atomic<uint64_t>` (thread-safe).
- Web-интерфейс — на `ESPAsyncWebServer`, JSON собирается через `snprintf`/`StaticJsonDocument`.
- Таймаут STA reconnect → hard radio reset → `ESP.restart()` (через 3 минуты), с сохранением причины в RTC.

## Метрики

- `mavlink_rx_pkts` — число разобранных MAVLink-пакетов с UART.
- `mavlink_rx_lost` — число пакетов, потерянных по sequence-gap.
- `mavlink_parse_err` — ошибки парсинга (CRC, длина и т.п.).
- `mavlink_bridge_tx_pkts` — пакеты, сгенерированные самим мостом (PARAM_SET/REQUEST).
- `uart_bytes_rx/tx`, `uart_overruns` — счётчики UART.
- `net_bytes_to_gcs`, `net_bytes_from_gcs` — сетевой трафик.
- `tcp_connects_total`, `tcp_disconnects_total`, `sta_reconnects_total`, `ap_assoc_total`, `ap_disassoc_total`.
- `rssi_now/min/max/avg` — окно RSSI_HISTORY_LEN секунд.
- `chip_temp_c`, `free_heap`, `min_free_heap`, `largest_free_block`.

## Сборка и прошивка

```bash
pio run                 # собрать
pio run -t upload       # прошить
pio run -t uploadfs     # залить LittleFS (иконки)
pio device monitor      # логи
```

Скрипт `tools/embed_bridge_ui.py` запускается автоматически перед сборкой и встраивает
`data/index.html` в `include/bridge_ui_embed.h`.

## Процедура приёмки (24 ч)

Перед релизом прошивки необходимо провести полный цикл тестирования. Все проверки
должны быть выполнены **без перезагрузки** в течение заявленного времени.

### 1. Функциональный дым (30 мин)

- [ ] Подключение Mission Planner по TCP (`<IP>:5760`) — активно, поток MAVLink идёт.
- [ ] Подключение QGC по UDP (`:14550`) — UI показывает `udp_connected: 1`, трафик идёт.
- [ ] `/api/status` возвращает корректные метрики, `mavlink_loss_pct < 1%`.
- [ ] SERVO-параметры (`/params`) читаются и записываются, статус "OK".
- [ ] Смена настроек (`/api/settings`) с валидными данными → ответ 200, ребут, причина `user_settings`.
- [ ] Смена настроек с невалидными данными (SSID пуст, пароль короткий) → ответ 400, ребута НЕТ.

### 2. Нагрузочный тест (2 ч)

- [ ] Телеметрия с автопилота 115200/921600 бод, 30-100 пакетов/с.
- [ ] Одновременно 2-3 TCP клиента + 1 UDP.
- [ ] Результат: `uart_overruns == 0`, `mavlink_loss_pct < 0.5%`, без разрывов TCP.

### 3. Отказоустойчивость (2 ч)

- [ ] В AP-режиме: отключать/подключать станции 10 раз — `ap_assoc_total/ap_disassoc_total`
      растут, UDP-клиент сбрасывается когда станций 0, UI продолжает работать.
- [ ] В STA-режиме: выключить роутер на 5 мин, затем включить — после `STA_RECONNECT_PERIOD_MS`
      мост сам переподключается, `sta_reconnects_total` растёт, после восстановления UI работает.
- [ ] В STA-режиме: выключить роутер на 4 мин (> `STA_RECONNECT_HARD_RESET_MS` = 3 мин) —
      ESP.restart() с причиной `sta_give_up` в логе.

### 4. Термо-тест (1 ч)

- [ ] Непрерывная трансляция на полной мощности → температура > 80°C → в логе
      `[thermal] throttle ON`, `rssi_max` падает; после охлаждения < 72°C → `throttle OFF`.

### 5. Память (24 ч)

- [ ] `free_heap` и `largest_free_block` не деградируют более чем на 10% за 24 ч
      (проверять раз в час, логи `[heap] free=... min=... largest=...`).
- [ ] `/api/status` и `/api/clients` отвечают без OOM за весь период.
- [ ] Watchdog таймеры не срабатывают (в логе нет `ESP_RST_TASK_WDT`).

### 6. Web UI

- [ ] Страница `/bridge` грузится за < 1 сек.
- [ ] Обновление метрик раз в 2 с, без "дерганий" при временной потере соединения
      (AbortController отменяет зависшие запросы).
- [ ] Tooltip UDP-клиентов показывает правильный IP:port.
- [ ] Метрики канала (MAVLink) корректны: `RX` растёт, `Потери (seq-gap)` почти 0,
      `Parse err` почти 0, `Bridge TX` растёт только при реальной отправке с UI.

### 7. Персистентность

- [ ] Смена SSID/пароля через UI — после ребута применена.
- [ ] Ручное задание UDP-клиента через UI (с `save:true`) — после ребута он восстановлен.

## Safety / лицензии

На основе `DroneBridge for ESP32` (Wolfgang Christl, Apache 2.0).
Код в этой директории — под той же лицензией.
