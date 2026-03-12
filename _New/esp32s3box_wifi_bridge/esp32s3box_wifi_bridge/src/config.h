/**
 * config.h — центральный конфиг WiFi-моста и элементов платы.
 *
 * Веб-интерфейс работает только локально:
 *   1. Подключитесь к Wi‑Fi точки доступа ESP32 (SSID/PASSWD ниже).
 *   2. В браузере откройте http://192.168.2.1 (или http://esps3.local в STA).
 * Интернет не нужен — всё обслуживается самой платой.
 *
 * Меняйте только здесь: режим Wi‑Fi, порты, пины, флаги функций.
 */
#ifndef CONFIG_H
#define CONFIG_H

#define DEBUG  /* Закомментируйте, чтобы отключить отладочный вывод в Serial. */

#include <WiFi.h>

/* ========== Режим Wi‑Fi ========== */
#define MODE_AP       /* Точка доступа: плата раздаёт Wi‑Fi. Либо MODE_STA (подключение к роутеру). */
#define SSID     "ESP32S3_NEW"   /* Имя сети (в AP — к нему подключаются для доступа к веб-интерфейсу). */
#define PASSWD   "12345678"      /* Пароль Wi‑Fi (минимум 8 символов для AP). */
#define HOSTNAME "esps3"         /* Имя для mDNS в режиме STA (esps3.local). */
#define STATIC_IP IPAddress(192, 168, 2, 1)
#define NETMASK   IPAddress(255, 255, 255, 0)

/* ========== Мост: протоколы ========== */
#define PROTOCOL_TCP        /* TCP-сервер: Mission Planner — Connection Type TCP, IP 192.168.2.1, Port 8880. */
#define PROTOCOL_UDP        /* UDP: Mission Planner — Connection Type UDP, Port 14550. */
#define MAX_NMEA_CLIENTS 4  /* Максимум одновременных TCP-клиентов (GCS и т.д.). */
/* #define BLUETOOTH     */ /* Раскомментируйте для моста по Bluetooth. */
/* #define OTA_HANDLER   */ /* Раскомментируйте для прошивки по Wi‑Fi (OTA). */
/* #define BATTERY_SAVER */ /* Раскомментируйте для снижения мощности Wi‑Fi. */

/* ========== UART к автопилоту ========== */
/* Подключение: TX автопилота → SERIAL_RXPIN (14), RX автопилота → SERIAL_TXPIN (13), GND → GND. */
#define UART_BAUD 57600
#define SERIAL_PARAM SERIAL_8N1
#define SERIAL_TXPIN 13
#define SERIAL_RXPIN 14

/* ========== Порты для Mission Planner (MAVLink) ========== */
/* TCP: Connection Type = TCP, IP = 192.168.2.1 (в режиме AP), Port = 8880. */
/* UDP: Connection Type = UDP, Port = 14550; подключиться к Wi‑Fi платы и нажать Connect. */
#define SERIAL_TCP_PORT 8880
#define SERIAL0_TCP_PORT SERIAL_TCP_PORT
#define SERIAL_UDP_PORT 14550

/* ========== Буферы и версия ========== */
#define BUFFERSIZE 1024
#define VERSION "2.0-ESP32S3"

/* ========== Веб-интерфейс (локальный, по Wi‑Fi платы) и пины ========== */
#define WEB_SERVER
#define WEB_SERVER_PORT 80
#define LED_PIN 2
#define SERVO_PIN 12
#define BTN_PIN 0

/* ========== Лог MAVLink (кольцевой буфер для веб-страницы) ========== */
#define MAVLINK_LOG_SIZE 50
#define MAVLINK_LOG_ENTRY_LEN 80

/* Таймаут MAVLink: если столько мс нет HEARTBEAT — считаем автопилот отключённым. */
#define MAVLINK_HEARTBEAT_TIMEOUT_MS 5000

/* ========== Отладка ========== */
#ifdef DEBUG
    #define debug Serial
#else
    class Debug : public Print { size_t write(uint8_t) override { return 0; } };
    Debug debug;
#endif

#endif
