/**
 * bridge_nvs.h — сохранение и загрузка настроек моста в NVS (энергонезависимая память ESP32).
 *
 * ЧТО ЭТО ДАЁТ:
 *   Настройки (WiFi, UART, hostname) сохраняются между перезагрузками. При первом запуске
 *   подставляются значения по умолчанию из config.h; после сохранения через веб (/api/settings)
 *   они записываются в NVS и при следующей загрузке читаются в bridge_nvs_config.
 *
 * КТО ВЫЗЫВАЕТ:
 *   — main.cpp в setup() вызывает loadBridgeConfig().
 *   — web_handlers при POST /api/settings вызывает setBridgeConfigFromJson() и затем saveBridgeConfig() (внутри setBridgeConfigFromJson).
 */
#ifndef BRIDGE_NVS_H
#define BRIDGE_NVS_H

#include "config.h"
#include <stdint.h>
#include <stdbool.h>

#define BRIDGE_NVS_SSID_LEN     32   /* Макс. длина SSID. */
#define BRIDGE_NVS_PASS_LEN     64   /* Макс. длина пароля Wi‑Fi. */
#define BRIDGE_NVS_HOSTNAME_LEN 32   /* Макс. длина имени для mDNS (esps3.local). */
#define BRIDGE_NVS_APIP_LEN     16   /* Макс. длина строки IP в режиме AP. */

/** Единая структура настроек: хранится в NVS и заполняется при loadBridgeConfig(). */
typedef struct {
    uint8_t  wifi_mode;   /* 1 = AP (точка доступа), 2 = STA (клиент). */
    char     ssid[BRIDGE_NVS_SSID_LEN];
    char     wifi_pass[BRIDGE_NVS_PASS_LEN];
    char     hostname[BRIDGE_NVS_HOSTNAME_LEN];
    uint8_t  wifi_chan;
    char     ap_ip[BRIDGE_NVS_APIP_LEN];
    uint32_t baud;        /* Скорость UART к автопилоту. */
    int8_t   gpio_tx;     /* GPIO для TX к автопилоту; -1 = не задано, использовать из config.h. */
    int8_t   gpio_rx;     /* GPIO для RX. */
    uint8_t  proto;       /* Флаги протоколов (TCP/UDP и т.д.). */
} bridge_nvs_config_t;

/** Глобальный экземпляр: после loadBridgeConfig() здесь лежат настройки из NVS или defaults. */
extern bridge_nvs_config_t bridge_nvs_config;

/** Загрузить настройки из NVS в bridge_nvs_config; если NVS пустой или формат другой — подставить значения по умолчанию. */
void loadBridgeConfig(void);
/** Сохранить bridge_nvs_config в NVS. Возвращает true при успехе. */
bool saveBridgeConfig(void);
/** Разобрать JSON-строку (тело POST /api/settings), обновить bridge_nvs_config и сохранить в NVS. Возвращает true при успехе. */
bool setBridgeConfigFromJson(const char* json);

#endif
