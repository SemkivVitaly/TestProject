/**
 * bridge_nvs.h — сохранение и загрузка настроек моста в NVS.
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

typedef struct {
    uint8_t  wifi_mode;
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

extern bridge_nvs_config_t bridge_nvs_config;

void loadBridgeConfig(void);
bool saveBridgeConfig(void);
bool setBridgeConfigFromJson(const char* json);

#endif
