/**
 * bridge_nvs.h — сохранение и загрузка настроек моста в NVS.
 *
 * Настройки Wi‑Fi (режим AP/STA, SSID, пароль, hostname и т.д.) можно менять
 * из веб-интерфейса Bridge; они сохраняются в NVS и применяются при следующей загрузке.
 */
#ifndef BRIDGE_NVS_H
#define BRIDGE_NVS_H

#include "config.h"
#include <stdint.h>
#include <stdbool.h>

#define BRIDGE_NVS_SSID_LEN     32
#define BRIDGE_NVS_PASS_LEN     64
#define BRIDGE_NVS_HOSTNAME_LEN 32
#define BRIDGE_NVS_APIP_LEN     16

/** Текущая конфигурация (загружена из NVS или defaults из config.h). */
typedef struct {
    uint8_t  wifi_mode;   /* 1 = AP, 2 = STA */
    char     ssid[BRIDGE_NVS_SSID_LEN];
    char     wifi_pass[BRIDGE_NVS_PASS_LEN];
    char     hostname[BRIDGE_NVS_HOSTNAME_LEN];
    uint8_t  wifi_chan;
    char     ap_ip[BRIDGE_NVS_APIP_LEN];
    uint32_t baud;
    int8_t   gpio_tx;
    int8_t   gpio_rx;
    uint8_t  proto;
} bridge_nvs_config_t;

/** Глобальная конфигурация (заполняется в loadBridgeConfig). */
extern bridge_nvs_config_t bridge_nvs_config;

/** Загрузить конфигурацию из NVS. Если в NVS пусто — подставить значения по умолчанию из config.h. Вызывать в setup() до инициализации Wi‑Fi. */
void loadBridgeConfig(void);

/** Сохранить текущую bridge_nvs_config в NVS. */
bool saveBridgeConfig(void);

/**
 * Распарсить JSON-тело POST /api/settings, обновить bridge_nvs_config и сохранить в NVS.
 * Возвращает true при успехе. После вызова обычно выполняется ESP.restart().
 */
bool setBridgeConfigFromJson(const char* json);

#endif /* BRIDGE_NVS_H */
