/**
 * config.h — центральный конфиг WiFi-моста и элементов платы.
 *
 * Веб-интерфейс работает только локально:
 *   1. Подключитесь к Wi‑Fi точки доступа ESP32 (SSID/PASSWD ниже).
 *   2. В браузере откройте http://192.168.2.1 (или http://esps3.local в STA).
 * Интернет не нужен — всё обслуживается самой платой.
 */
#ifndef CONFIG_H
#define CONFIG_H

#define DEBUG

#include <WiFi.h>

/* ========== Режим Wi‑Fi ========== */
#define MODE_AP
#define SSID     "ESP32S3_NEW"
#define PASSWD   "12345678"
#define HOSTNAME "esps3"
#define STATIC_IP IPAddress(192, 168, 2, 1)
#define NETMASK   IPAddress(255, 255, 255, 0)

/* ========== Мост: протоколы ========== */
#define PROTOCOL_TCP
#define PROTOCOL_UDP
#define MAX_NMEA_CLIENTS 4

/* ========== UART к автопилоту ========== */
#define UART_BAUD 57600
#define SERIAL_PARAM SERIAL_8N1
#define SERIAL_TXPIN 13
#define SERIAL_RXPIN 14

/* ========== Порты для Mission Planner (MAVLink) ========== */
#define SERIAL_TCP_PORT 8880
#define SERIAL0_TCP_PORT SERIAL_TCP_PORT
#define SERIAL_UDP_PORT 14550

/* ========== Буферы и версия ========== */
#define BUFFERSIZE 1024
#define VERSION "2.0-ESP32S3"

/* ========== Веб-интерфейс и пины ========== */
#define WEB_SERVER
#define WEB_SERVER_PORT 80
#define LED_PIN 2
#define SERVO_PIN 12
#define BTN_PIN 0

/* Опционально: раскомментируйте при необходимости. */
/* #define OTA_HANDLER     — OTA-обновление по Wi‑Fi в setup(). */
/* #define BATTERY_SAVER   — снижение мощности Wi‑Fi в конце setup(). */
/* #define BLUETOOTH       — включить мост по Bluetooth в bridge.cpp. */

/* ========== Лог MAVLink ========== */
#define MAVLINK_LOG_SIZE 50
#define MAVLINK_LOG_ENTRY_LEN 80
#define MAVLINK_HEARTBEAT_TIMEOUT_MS 5000

/* ========== Отладка ========== */
#ifdef DEBUG
    #define debug Serial
#else
    class Debug : public Print { size_t write(uint8_t) override { return 0; } };
    Debug debug;
#endif

#endif
