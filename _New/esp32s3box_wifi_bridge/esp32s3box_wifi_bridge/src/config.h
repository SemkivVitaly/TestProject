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

/* ========== Мост: протоколы ========== */
/* Включить TCP-сервер: Mission Planner подключается по TCP к порту SERIAL_TCP_PORT. */
#define PROTOCOL_TCP
/* Включить приём UDP: QGroundControl и др. шлют MAVLink на порт SERIAL_UDP_PORT. */
#define PROTOCOL_UDP
/* Максимум одновременных TCP-клиентов (слотов для подключений). */
#define MAX_NMEA_CLIENTS 4

/* ========== UART к автопилоту ========== */
/* Скорость обмена с автопилотом (бит/с). Типично 57600 или 115200 для MAVLink. */
#define UART_BAUD 57600
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
#define BUFFERSIZE 1024
/* Строка версии: показывается при загрузке и в веб-интерфейсе. */
#define VERSION "2.0-ESP32S3"

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

/* ========== Отладка ========== */
/* DEBUG включён: debug — это Serial, все debug.print() выводятся в порт. */
#ifdef DEBUG
    #define debug Serial
#else
    /* Иначе debug — пустой объект: вывод отключён, код не ломается. */
    class Debug : public Print { size_t write(uint8_t) override { return 0; } };
    Debug debug;
#endif

#endif
