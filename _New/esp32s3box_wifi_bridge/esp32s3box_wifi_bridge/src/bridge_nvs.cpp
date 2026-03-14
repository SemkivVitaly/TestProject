/**
 * bridge_nvs.cpp — загрузка/сохранение настроек моста в NVS.
 */
#include "config.h"
#include "bridge_nvs.h"
#include <Preferences.h>
#include <Arduino.h>
#include <string.h>

#define NVS_NAMESPACE "bridge"

bridge_nvs_config_t bridge_nvs_config;

static void setDefaults(void) {
    bridge_nvs_config.wifi_mode = 1;
    strncpy(bridge_nvs_config.ssid, SSID, BRIDGE_NVS_SSID_LEN - 1);
    bridge_nvs_config.ssid[BRIDGE_NVS_SSID_LEN - 1] = '\0';
    strncpy(bridge_nvs_config.wifi_pass, PASSWD, BRIDGE_NVS_PASS_LEN - 1);
    bridge_nvs_config.wifi_pass[BRIDGE_NVS_PASS_LEN - 1] = '\0';
    strncpy(bridge_nvs_config.hostname, HOSTNAME, BRIDGE_NVS_HOSTNAME_LEN - 1);
    bridge_nvs_config.hostname[BRIDGE_NVS_HOSTNAME_LEN - 1] = '\0';
    bridge_nvs_config.wifi_chan = 1;
    strncpy(bridge_nvs_config.ap_ip, "192.168.2.1", BRIDGE_NVS_APIP_LEN - 1);
    bridge_nvs_config.ap_ip[BRIDGE_NVS_APIP_LEN - 1] = '\0';
    bridge_nvs_config.baud = UART_BAUD;
    bridge_nvs_config.gpio_tx = SERIAL_TXPIN;
    bridge_nvs_config.gpio_rx = SERIAL_RXPIN;
    bridge_nvs_config.proto = 4;
}

void loadBridgeConfig(void) {
    setDefaults();
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true))
        return;
    if (prefs.getBytesLength("cfg") != sizeof(bridge_nvs_config_t)) {
        prefs.end();
        return;
    }
    prefs.getBytes("cfg", &bridge_nvs_config, sizeof(bridge_nvs_config_t));
    prefs.end();
    if (bridge_nvs_config.ssid[0] == '\0')
        strncpy(bridge_nvs_config.ssid, SSID, BRIDGE_NVS_SSID_LEN - 1);
    if (bridge_nvs_config.wifi_mode != 1 && bridge_nvs_config.wifi_mode != 2)
        bridge_nvs_config.wifi_mode = 1;
}

bool saveBridgeConfig(void) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false))
        return false;
    prefs.putBytes("cfg", &bridge_nvs_config, sizeof(bridge_nvs_config_t));
    prefs.end();
    return true;
}

static bool jsonGetString(const char* json, const char* key, char* out, size_t outLen) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":\"", key);
    const char* p = strstr(json, search);
    if (!p) return false;
    p += strlen(search);
    size_t i = 0;
    while (i < outLen - 1 && *p && *p != '"') {
        if (*p == '\\' && *(p + 1) == '"') p++;
        out[i++] = *p++;
    }
    out[i] = '\0';
    return true;
}

static bool jsonGetInt(const char* json, const char* key, int* out) {
    char search[48];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char* p = strstr(json, search);
    if (!p) return false;
    p += strlen(search);
    *out = atoi(p);
    return true;
}

bool setBridgeConfigFromJson(const char* json) {
    if (!json) return false;
    int v;
    if (jsonGetInt(json, "esp32_mode", &v) && (v == 1 || v == 2))
        bridge_nvs_config.wifi_mode = (uint8_t)v;
    jsonGetString(json, "ssid", bridge_nvs_config.ssid, BRIDGE_NVS_SSID_LEN);
    jsonGetString(json, "wifi_pass", bridge_nvs_config.wifi_pass, BRIDGE_NVS_PASS_LEN);
    jsonGetString(json, "wifi_hostname", bridge_nvs_config.hostname, BRIDGE_NVS_HOSTNAME_LEN);
    if (jsonGetInt(json, "wifi_chan", &v) && v >= 1 && v <= 13)
        bridge_nvs_config.wifi_chan = (uint8_t)v;
    jsonGetString(json, "ap_ip", bridge_nvs_config.ap_ip, BRIDGE_NVS_APIP_LEN);
    if (jsonGetInt(json, "baud", &v) && v >= 9600 && v <= 921600)
        bridge_nvs_config.baud = (uint32_t)v;
    if (jsonGetInt(json, "gpio_tx", &v) && v >= -1 && v <= 48)
        bridge_nvs_config.gpio_tx = (int8_t)v;
    if (jsonGetInt(json, "gpio_rx", &v) && v >= -1 && v <= 48)
        bridge_nvs_config.gpio_rx = (int8_t)v;
    if (jsonGetInt(json, "proto", &v))
        bridge_nvs_config.proto = (uint8_t)v;
    return saveBridgeConfig();
}
