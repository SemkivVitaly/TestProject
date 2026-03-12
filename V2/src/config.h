/**
 * config.h — настройки WiFi-моста для Mission Planner и элементы платы
 * Меняйте только здесь: режим Wi‑Fi, порты, пины, включение функций.
 */

#ifndef CONFIG_H
#define CONFIG_H

#define DEBUG  // закомментируйте, чтобы отключить отладочный вывод в Serial

#include <WiFi.h>

// --- Режим Wi‑Fi ---
#define MODE_AP       // точка доступа (плата раздаёт Wi‑Fi). Либо MODE_STA (плата подключается к роутеру)
#define SSID     "ESP32S3_NEW"
#define PASSWD   "12345678"
#define HOSTNAME "esps3"           // имя для mDNS в режиме STA
#define STATIC_IP IPAddress(192, 168, 2, 1)
#define NETMASK   IPAddress(255, 255, 255, 0)

// --- Мост: какие протоколы включить ---
#define PROTOCOL_TCP        // TCP-сервер (Mission Planner: тип TCP, порт 8880)
#define PROTOCOL_UDP        // UDP (Mission Planner: тип UDP, порт 14550)
#define MAX_NMEA_CLIENTS 4  // сколько TCP-клиентов принимать
// #define BLUETOOTH        // раскомментируйте для моста по Bluetooth
// #define OTA_HANDLER      // раскомментируйте для прошивки по Wi‑Fi (OTA)
// #define BATTERY_SAVER    // раскомментируйте для снижения мощности Wi‑Fi

// --- UART к автопилоту (Pixhawk/телеметрия: часто 57600 или 115200) ---
// Подключение: TX автопилота → SERIAL_RXPIN (14), RX автопилота → SERIAL_TXPIN (13), GND → GND
#define UART_BAUD 57600
#define SERIAL_PARAM SERIAL_8N1
#define SERIAL_TXPIN 13
#define SERIAL_RXPIN 14

// --- Порты для Mission Planner (MAVLink) ---
#define SERIAL_TCP_PORT 8880
#define SERIAL0_TCP_PORT SERIAL_TCP_PORT
#define SERIAL_UDP_PORT 14550

// --- Буфер обмена ---
#define BUFFERSIZE 1024
#define VERSION "2.0-Bridge-DroneBridge"

// --- Веб-интерфейс и пины платы ---
#define WEB_SERVER
#define WEB_SERVER_PORT 80
#define LED_PIN 2
#define SERVO_PIN 12
#define BTN_PIN 0

// --- Лог MAVLink для веб-интерфейса ---
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
