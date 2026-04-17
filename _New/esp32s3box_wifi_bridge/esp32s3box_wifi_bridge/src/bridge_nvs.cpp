/**
 * bridge_nvs.cpp — загрузка и сохранение настроек моста в NVS (Non-Volatile Storage).
 *
 * NVS — это встроенная флеш-память ESP32 для ключ-значение; данные не пропадают при выключении.
 * Используется класс Preferences (обёртка над NVS) с пространством имён "bridge" и ключом "cfg".
 *
 * ВЗАИМОДЕЙСТВИЕ:
 *   — loadBridgeConfig() вызывается из main.cpp в setup(); читает в bridge_nvs_config.
 *   — setBridgeConfigFromJson() вызывается из web_handlers при POST /api/settings; парсит JSON и вызывает saveBridgeConfig().
 */
#include "config.h"
#include "bridge_nvs.h"
#include <Preferences.h>
#include <Arduino.h>
#include <string.h>

#define NVS_NAMESPACE "bridge"  /* Имя раздела в NVS для наших настроек. */

bridge_nvs_config_t bridge_nvs_config;  /* Глобальная структура: сюда загружаем и отсюда сохраняем. */

/** Заполняет bridge_nvs_config значениями по умолчанию из config.h (SSID, PASSWD, UART_BAUD и т.д.). */
static void setDefaults(void) {
    bridge_nvs_config.wifi_mode = 1;
    strncpy(bridge_nvs_config.ssid, SSID, BRIDGE_NVS_SSID_LEN - 1);
    bridge_nvs_config.ssid[BRIDGE_NVS_SSID_LEN - 1] = '\0';
    strncpy(bridge_nvs_config.wifi_pass, PASSWD, BRIDGE_NVS_PASS_LEN - 1);
    bridge_nvs_config.wifi_pass[BRIDGE_NVS_PASS_LEN - 1] = '\0';
    strncpy(bridge_nvs_config.hostname, HOSTNAME, BRIDGE_NVS_HOSTNAME_LEN - 1);
    bridge_nvs_config.hostname[BRIDGE_NVS_HOSTNAME_LEN - 1] = '\0';
    bridge_nvs_config.wifi_chan = 6;
    strncpy(bridge_nvs_config.ap_ip, "192.168.2.1", BRIDGE_NVS_APIP_LEN - 1);
    bridge_nvs_config.ap_ip[BRIDGE_NVS_APIP_LEN - 1] = '\0';
    bridge_nvs_config.baud = UART_BAUD;
    bridge_nvs_config.gpio_tx = SERIAL_TXPIN;
    bridge_nvs_config.gpio_rx = SERIAL_RXPIN;
    bridge_nvs_config.proto = 4;
}

/** Открывает NVS в режиме чтения, читает ключ "cfg" в bridge_nvs_config. Если размера не совпадают или открыть не удалось — остаются defaults из setDefaults(). */
void loadBridgeConfig(void) {
    setDefaults();
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true))  /* true = только чтение. */
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

/** Записывает bridge_nvs_config в NVS под ключом "cfg". Вызывается после изменения настроек (в т.ч. из setBridgeConfigFromJson). */
bool saveBridgeConfig(void) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false))
        return false;
    prefs.putBytes("cfg", &bridge_nvs_config, sizeof(bridge_nvs_config_t));
    prefs.end();
    return true;
}

/** Вспомогательная функция: из строки JSON извлечь значение строкового ключа key и записать в out (макс. outLen-1 символов). */
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

/** Вспомогательная функция: из JSON извлечь целочисленное значение по ключу key. */
static bool jsonGetInt(const char* json, const char* key, int* out) {
    char search[48];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char* p = strstr(json, search);
    if (!p) return false;
    p += strlen(search);
    *out = atoi(p);
    return true;
}

/** Список допустимых baud-значений (совпадает со списком в UI). */
static bool isValidBaud(int v) {
    static const int kBauds[] = {1200,2400,4800,9600,19200,38400,57600,76800,115200,
                                 230400,460800,500800,576000,921600,1000000,1500000,
                                 2000000,3000000,5000000};
    for (size_t i = 0; i < sizeof(kBauds)/sizeof(kBauds[0]); i++)
        if (kBauds[i] == v) return true;
    return false;
}

/**
 * Разбирает JSON (тело POST /api/settings) с ПОЛНОЙ валидацией: SSID 1..31 символ,
 * пароль 0 (открытая сеть) или 8..63 символов (WPA2), канал 1..13, baud из списка,
 * GPIO 0..48. При любой ошибке возвращает false, не перезаписывая NVS.
 */
bool setBridgeConfigFromJson(const char* json) {
    if (!json) return false;
    /* Работаем с копией конфига, перекладываем только после полной проверки. */
    bridge_nvs_config_t tmp = bridge_nvs_config;
    int v;
    if (jsonGetInt(json, "esp32_mode", &v)) {
        if (v != 1 && v != 2) return false;
        tmp.wifi_mode = (uint8_t)v;
    }
    char ssidBuf[BRIDGE_NVS_SSID_LEN];
    if (jsonGetString(json, "ssid", ssidBuf, sizeof(ssidBuf))) {
        size_t len = strlen(ssidBuf);
        if (len < 1 || len > BRIDGE_NVS_SSID_LEN - 1) return false;
        strncpy(tmp.ssid, ssidBuf, BRIDGE_NVS_SSID_LEN - 1);
        tmp.ssid[BRIDGE_NVS_SSID_LEN - 1] = '\0';
    }
    char passBuf[BRIDGE_NVS_PASS_LEN];
    if (jsonGetString(json, "wifi_pass", passBuf, sizeof(passBuf))) {
        size_t len = strlen(passBuf);
        if (!(len == 0 || (len >= 8 && len <= BRIDGE_NVS_PASS_LEN - 1))) return false;
        strncpy(tmp.wifi_pass, passBuf, BRIDGE_NVS_PASS_LEN - 1);
        tmp.wifi_pass[BRIDGE_NVS_PASS_LEN - 1] = '\0';
    }
    char hnBuf[BRIDGE_NVS_HOSTNAME_LEN];
    if (jsonGetString(json, "wifi_hostname", hnBuf, sizeof(hnBuf))) {
        size_t len = strlen(hnBuf);
        if (len > BRIDGE_NVS_HOSTNAME_LEN - 1) return false;
        if (len > 0) { /* пустой hostname оставляем как есть */
            strncpy(tmp.hostname, hnBuf, BRIDGE_NVS_HOSTNAME_LEN - 1);
            tmp.hostname[BRIDGE_NVS_HOSTNAME_LEN - 1] = '\0';
        }
    }
    if (jsonGetInt(json, "wifi_chan", &v)) {
        if (v < 1 || v > 13) return false;
        tmp.wifi_chan = (uint8_t)v;
    }
    char apBuf[BRIDGE_NVS_APIP_LEN];
    if (jsonGetString(json, "ap_ip", apBuf, sizeof(apBuf))) {
        if (strlen(apBuf) > 0) {
            strncpy(tmp.ap_ip, apBuf, BRIDGE_NVS_APIP_LEN - 1);
            tmp.ap_ip[BRIDGE_NVS_APIP_LEN - 1] = '\0';
        }
    }
    if (jsonGetInt(json, "baud", &v)) {
        if (!isValidBaud(v)) return false;
        tmp.baud = (uint32_t)v;
    }
    if (jsonGetInt(json, "gpio_tx", &v)) {
        if (v < -1 || v > 48) return false;
        tmp.gpio_tx = (int8_t)v;
    }
    if (jsonGetInt(json, "gpio_rx", &v)) {
        if (v < -1 || v > 48) return false;
        tmp.gpio_rx = (int8_t)v;
    }
    if (jsonGetInt(json, "proto", &v)) {
        if (v < 0 || v > 10) return false;
        tmp.proto = (uint8_t)v;
    }
    /* Всё ок — коммитим. */
    bridge_nvs_config = tmp;
    return saveBridgeConfig();
}
