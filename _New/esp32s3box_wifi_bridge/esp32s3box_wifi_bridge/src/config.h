/**
 * config.h — центральный конфиг WiFi-моста и элементов платы.
 *
 * ЧТО ЭТОТ ФАЙЛ ДЕЛАЕТ:
 *   Здесь собраны ВСЕ настраиваемые константы проекта. Меняя значения ниже,
 *   вы меняете поведение прошивки при первой загрузке (до сохранения в NVS).
 *   После сохранения настроек через веб-интерфейс приоритет имеют данные из NVS.
 *
 * ВЗАИМОДЕЙСТВИЕ:
 *   — Подключается в main.cpp, bridge.cpp, bridge_nvs.cpp, web_handlers.cpp и др.
 *   — Никто не передаёт сюда параметры: это только чтение #define-констант.
 *
 * Веб-интерфейс работает только локально:
 *   1. Подключитесь к Wi‑Fi точки доступа ESP32 (SSID/PASSWD ниже).
 *   2. В браузере откройте http://192.168.2.1 (или http://esps3.local в STA).
 * Интернет не нужен — всё обслуживается самой платой.
 */
#ifndef CONFIG_H
#define CONFIG_H

/* Включение отладочного вывода в Serial: сообщения debug.print() идут в Serial. */
#define DEBUG

#include <WiFi.h>

/* ========== Режим Wi‑Fi ========== */
/* MODE_AP — при сборке включён режим точки доступа по умолчанию (если в NVS не сохранён STA). */
#define MODE_AP
/* Имя сети Wi‑Fi, которое видит пользователь при подключении к плате (режим AP). */
#define SSID     "ESP32S3_NEW"
/* Пароль от точки доступа (минимум 8 символов для WPA2). */
#define PASSWD   "12345678"
/* Имя устройства для mDNS в режиме STA: доступ по http://esps3.local */
#define HOSTNAME "esps3"
/* IP-адрес платы в режиме AP: к нему подключаются клиенты (GCS, браузер). */
#define STATIC_IP IPAddress(192, 168, 2, 1)
/* Маска подсети: 255.255.255.0 — одна подсеть до 254 устройств. */
#define NETMASK   IPAddress(255, 255, 255, 0)

/* Опционально: раскомментируйте для прежних агрессивных настроек Wi‑Fi (max TX ~20 dBm, beacon 50 TU). */
/* #define WIFI_TUNE_AGGRESSIVE */

#ifndef WIFI_TUNE_AGGRESSIVE
/* esp_wifi_set_max_tx_power(): шаг 0.25 dBm; умеренное значение снижает ошибки PHY на части антенн/плат. */
#define WIFI_MAX_TX_POWER 60
#define WIFI_AP_BEACON_INTERVAL_TU 100
#else
#define WIFI_MAX_TX_POWER 80
#define WIFI_AP_BEACON_INTERVAL_TU 50
#endif

/* ========== Мост: протоколы ========== */
/* Включить TCP-сервер: Mission Planner подключается по TCP к порту SERIAL_TCP_PORT. */
#define PROTOCOL_TCP
/* Включить приём UDP: QGroundControl и др. шлют MAVLink на порт SERIAL_UDP_PORT. */
#define PROTOCOL_UDP
/* Максимум одновременных TCP-клиентов (слотов для подключений). */
#define MAX_NMEA_CLIENTS 4

/* ========== UART к автопилоту ========== */
/* Скорость по умолчанию до первой записи в NVS; на автопилоте SERIALx_BAUD должен совпадать (часто 115). */
#define UART_BAUD 115200
/* 8 бит данных, без чётности, 1 стоп-бит — стандарт для MAVLink. */
#define SERIAL_PARAM SERIAL_8N1
/* GPIO для передачи (TX): пин ESP32, который идёт на RX автопилота. */
#define SERIAL_TXPIN 13
/* GPIO для приёма (RX): пин ESP32, который идёт на TX автопилота. */
#define SERIAL_RXPIN 14

/* ========== Порты для Mission Planner (MAVLink) ========== */
/* TCP-порт, на котором мост принимает подключения (Mission Planner → Add Connection → TCP, этот порт). */
#define SERIAL_TCP_PORT 8880
#define SERIAL0_TCP_PORT SERIAL_TCP_PORT
/* UDP-порт для приёма/отправки MAVLink (стандарт 14550 для GCS). */
#define SERIAL_UDP_PORT 14550

/* ========== Буферы и версия ========== */
/* Размер буфера для чтения/записи порциями из UART и сети (байт). */
#define BUFFERSIZE 2048
/* Строка версии: показывается при загрузке и в веб-интерфейсе. */
#define VERSION "2.1-ESP32S3-async"

/* ========== Веб-интерфейс и пины ========== */
/* Включить веб-сервер: страницы /, /params, /bridge и API /api/*. */
#define WEB_SERVER
/* Порт HTTP: открыть в браузере http://192.168.2.1:80 (80 по умолчанию можно не писать). */
#define WEB_SERVER_PORT 80
/* GPIO светодиода на плате (можно мигать при событиях). */
#define LED_PIN 2
/* GPIO для сервопривода (если используется). */
#define SERVO_PIN 12
/* GPIO кнопки (например BOOT на ESP32): обработка в loop() в main.cpp. */
#define BTN_PIN 0

/* Опционально: раскомментируйте при необходимости. */
/* #define OTA_HANDLER     — OTA-обновление по Wi‑Fi в setup(). */
/* #define BATTERY_SAVER   — снижение мощности Wi‑Fi в конце setup(). */
/* #define BLUETOOTH       — включить мост по Bluetooth в bridge.cpp. */

/* ========== Лог MAVLink ========== */
/* Количество записей в кольцевом логе событий MAVLink (HEARTBEAT, PARAM_VALUE и т.д.). */
#define MAVLINK_LOG_SIZE 50
/* Максимальная длина одной записи в этом логе (символов). */
#define MAVLINK_LOG_ENTRY_LEN 80
/* Если столько мс не было HEARTBEAT от автопилота — считаем связь потерянной. */
#define MAVLINK_HEARTBEAT_TIMEOUT_MS 5000

/* ========== UART-task и асинхронный мост ========== */
/* Размер аппаратного RX-буфера UART: при ATTITUDE@50Гц + GPS@5Гц нужен запас на всплески. */
#define UART_RX_BUF_SIZE          16384
/* Размер TX-буфера UART: данные от нескольких TCP-клиентов могут прийти одновременно. */
#define UART_TX_BUF_SIZE          8192
/* Размер стека FreeRTOS-task для UART: парсинг MAVLink + отправка в stream-буферы. */
#define UART_TASK_STACK           8192
/* Приоритет UART-task: выше loopTask(=1), ниже wifi(=23) — чтобы не пропускать HEARTBEAT. */
#define UART_TASK_PRIORITY        5
/* Ядро для UART-task: 1 (user), core 0 занят wifi/lwip и AsyncTCP (CONFIG_ASYNC_TCP_RUNNING_CORE=0). */
#define UART_TASK_CORE            1
/* Стрим-буфер на каждого TCP-клиента для данных, идущих в сеть (8 КБ = ~0.5 c при 128 кбит/с). */
#define TCP_SLOT_TX_BUFFER        8192
/* Минимум свободного окна TCP перед попыткой записи (если меньше — копим дальше). */
#define TCP_CLIENT_SPACE_MIN      64
/* Таймаут UDP-клиента (мс): если давно не было пакетов — забываем, чтобы не слать в никуда. */
#define UDP_CLIENT_TIMEOUT_MS     120000
/* Период flush-а стрим-буферов клиентам (мс) — на случай, если onAck/onPoll редки. */
#define TCP_FLUSH_INTERVAL_MS     20

/* ========== STA reconnect FSM ========== */
/* Пауза перед попыткой переподключения (мс). */
#define STA_RECONNECT_PERIOD_MS     3000
/* Сколько подряд неудач → hard radio reset (WIFI_OFF/ON). */
#define STA_RECONNECT_MAX_FAILS     10
/* После стольких мс без успеха — ESP.restart() (записав причину в RTC). */
#define STA_RECONNECT_HARD_RESET_MS 180000

/* ========== Термо-контроль ========== */
/* Порог включения троттлинга TX power (°C). */
#define THERMAL_THROTTLE_HIGH_C   80.0f
/* Порог выключения троттлинга (гистерезис). */
#define THERMAL_THROTTLE_LOW_C    72.0f
/* Пониженная мощность в троттле (шаг 0.25 dBm; 52 ≈ 13 dBm). */
#define THERMAL_THROTTLE_TX_POWER 52
/* Период опроса температуры (мс). */
#define THERMAL_TICK_MS           5000

/* ========== Heap / диагностика ========== */
/* Период вывода лога о heap (мс). */
#define HEAP_LOG_INTERVAL_MS      60000
/* Если largest_free_block < этого значения — пишем WARN (фрагментация). */
#define HEAP_FRAG_WARN_BYTES      20000

/* ========== RSSI history ========== */
/* Сколько последних отсчётов RSSI храним (1 отсчёт/с → окно 60 с). */
#define RSSI_HISTORY_LEN          60
/* Период опроса RSSI (мс). */
#define RSSI_TICK_MS              1000

/* ========== Watchdog ========== */
/* Таймаут WDT для loop()/uart-task (с). */
#define WDT_TIMEOUT_S             10

/* ========== Веб-интерфейс ========== */
/* Минимальный период обновления веб-UI (мс): снижаем частоту опроса, чтобы не фрагментировать heap. */
#define WEB_POLL_MIN_MS           2000

/* ========== Отладка ========== */
/* DEBUG включён: debug — это Serial, все debug.print() выводятся в порт. */
#ifdef DEBUG
    #define debug Serial
#else
    class DebugNull : public Print { public: size_t write(uint8_t) override { return 0; } };
    static DebugNull debug;
#endif

#endif
