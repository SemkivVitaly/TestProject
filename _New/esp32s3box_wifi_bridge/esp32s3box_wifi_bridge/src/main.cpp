/**
 * main.cpp — точка входа прошивки.
 *
 * АРХИТЕКТУРА (core 0 / core 1):
 *   core 0: WiFi, LWIP, AsyncTCP (CONFIG_ASYNC_TCP_RUNNING_CORE=0), AsyncUDP — обслуживают сеть.
 *   core 1: loopTask (supervision, STA FSM, thermal/heap/rssi), uart-task (чтение UART → MAVLink → сеть).
 *
 * ТАСКИ:
 *   — setup() готовит WiFi/UART/bridge/web, запускает uart-task на core 1.
 *   — loop() — только управление (STA reconnect FSM, thermal, heap, rssi history, mavlink timeout).
 *     Всё блокирующее (SerialUART.read, mavlinkProcessBytes) вынесено в uart-task.
 *
 * ОТКАЗОУСТОЙЧИВОСТЬ:
 *   — WatchDog Timer на loop() и uart-task (10 с).
 *   — RTC-NOINIT слот хранит причину предыдущего ребута; читаем в setup().
 *   — После 3 мин неуспешных STA reconnect — ESP.restart() (сохранив причину).
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiGeneric.h>
#include <cmath>
#include <atomic>
#include <ESPmDNS.h>
#include <esp_wifi.h>
#include <esp_task_wdt.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <soc/rtc_cntl_reg.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "config.h"
#include "bridge_nvs.h"
#include "mavlink_state.h"
#include "bridge.h"
#include "bridge_log.h"
#include "esp_log.h"

#ifdef OTA_HANDLER
#include <ArduinoOTA.h>
#endif
#ifdef WEB_SERVER
#include <ESP32Servo.h>
#include <LittleFS.h>
#include "web_handlers.h"
#endif

HardwareSerial SerialUART(1);

#ifdef WEB_SERVER
Servo servo;
bool servoAttached = false;
#endif

/* ========== RTC-сохраняемые данные между ребутами ========== */
/* Значения, устойчивые к ребутам: RTC_NOINIT_ATTR не очищается software reset'ами
 * (в отличие от RTC_DATA_ATTR, который инициализируется при загрузке). */
RTC_NOINIT_ATTR static uint32_t s_rtcMagic;
RTC_NOINIT_ATTR static uint32_t s_rtcLastUptime;
RTC_NOINIT_ATTR static uint32_t s_rtcRestartReason;  /* custom: 1=STA timeout, 2=panic, 3=manual */

#define RTC_MAGIC_VALUE 0xB0A1C0DE

/* Причины, которые мы сами выставляем перед restart. */
enum : uint32_t {
    RESTART_REASON_NONE = 0,
    RESTART_REASON_STA_GIVE_UP = 1,
    RESTART_REASON_PANIC = 2,
    RESTART_REASON_USER_SETTINGS = 3,
};

/* ========== Периодические таймеры (из loop) ========== */
static uint32_t s_lastThermalMs = 0;
static uint32_t s_lastHeapMs = 0;
static uint32_t s_lastRssiMs = 0;
static uint32_t s_lastSupervisorMs = 0;

/* ========== STA reconnect FSM ========== */
enum class StaState : uint8_t {
    IDLE,             /* AP-режим или ещё не настроен */
    CONNECTED,        /* имеем IP */
    WAIT_RECONNECT,   /* ждём STA_RECONNECT_PERIOD_MS до begin() */
    CONNECTING,       /* begin() выполнен, ждём GOT_IP или timeout */
    HARD_RESET,       /* WIFI_OFF→STA после N подряд неудач */
    GIVE_UP_RESTART,  /* ESP.restart() через 180 с без успеха */
};
static std::atomic<StaState> g_staState{StaState::IDLE};
static uint32_t g_staSinceMs = 0;        /* когда вошли в текущее состояние */
static uint32_t g_staFirstFailMs = 0;    /* когда впервые потеряли connection */
static uint8_t g_staFails = 0;

/* ========== Термо-троттлинг ========== */
static bool s_thermalThrottled = false;

/* ========== RSSI history (кольцевой буфер отсчётов за окно RSSI_HISTORY_LEN секунд) ========== */
static int8_t s_rssiRing[RSSI_HISTORY_LEN];
static uint16_t s_rssiRingCount = 0;
static uint16_t s_rssiRingHead = 0;

/* ========== UART-task handle ========== */
static TaskHandle_t s_uartTaskHandle = nullptr;

/* ========== UART-task: читает SerialUART, вызывает MAVLink и мост ========== */
static void uartTask(void*) {
    uint8_t buf[BUFFERSIZE];
    /* Эта задача под WDT: сбрасываем таймер в каждой итерации. */
    esp_task_wdt_add(nullptr);
    /* Порог близости к переполнению RX-буфера UART. */
    const size_t overrunWatermark = (UART_RX_BUF_SIZE > 512) ? (UART_RX_BUF_SIZE - 512) : (UART_RX_BUF_SIZE / 2);
    uint32_t lastOverrunLogMs = 0;
    for (;;) {
        esp_task_wdt_reset();

        /* Детект оверранов: RX-буфер почти полон — значит мы не успеваем вычитывать. */
        size_t avail = SerialUART.available();
        if (avail >= overrunWatermark) {
            uartOverruns.fetch_add(1, std::memory_order_relaxed);
            uint32_t now = millis();
            if (now - lastOverrunLogMs > 2000U) {
                lastOverrunLogMs = now;
                espLogPrintf("[uart] RX near-overrun: avail=%u/%u", (unsigned)avail, (unsigned)UART_RX_BUF_SIZE);
            }
        }

        /* 1) Чтение UART (ограничиваем по времени в миллисекундах, чтобы не застрять). */
        size_t n = 0;
        uint32_t start = millis();
        while (SerialUART.available() && n < sizeof(buf) && (millis() - start) < 3U) {
            int b = SerialUART.read();
            if (b < 0) break;
            buf[n++] = (uint8_t)b;
        }

        if (n > 0) {
            bridgeLogSetLastRx(buf, (uint16_t)n);
            mavlinkProcessBytes(buf, (uint16_t)n);
            bridgeSendUartToNetwork(buf, (uint16_t)n);
        } else {
            /* Нет данных — уступаем. 1 тик = ~1 мс у нас. */
            vTaskDelay(1);
        }
    }
}

/** Получить текущий RSSI в dBm (STA: до AP; AP: среднее по станциям). Возвращает 0 если нет клиентов. */
static int8_t bridgeGetRssiDbm(void) {
    wifi_mode_t mode = WiFi.getMode();
    if ((mode == WIFI_STA || mode == WIFI_AP_STA) && WiFi.status() == WL_CONNECTED)
        return (int8_t)WiFi.RSSI();
    if (mode == WIFI_AP || mode == WIFI_AP_STA) {
        if (WiFi.softAPgetStationNum() > 0) {
            wifi_sta_list_t list;
            if (esp_wifi_ap_get_sta_list(&list) == ESP_OK && list.num > 0) {
                int sum = 0;
                for (int i = 0; i < list.num && i < ESP_WIFI_MAX_CONN_NUM; i++)
                    sum += list.sta[i].rssi;
                return (int8_t)(sum / list.num);
            }
        }
    }
    return 0;
}

/** Добавить значение в кольцевой буфер RSSI. */
static void rssiHistoryPush(int8_t v) {
    if (v == 0) return; /* 0 ≈ "нет данных" — не засоряем гистограмму */
    s_rssiRing[s_rssiRingHead] = v;
    s_rssiRingHead = (s_rssiRingHead + 1) % RSSI_HISTORY_LEN;
    if (s_rssiRingCount < RSSI_HISTORY_LEN) s_rssiRingCount++;
}

void bridgeGetRssiStats(int8_t* now, int8_t* mn, int8_t* mx, int8_t* avg) {
    int8_t n = bridgeGetRssiDbm();
    if (now) *now = n;
    if (s_rssiRingCount == 0) {
        if (mn) *mn = 0;
        if (mx) *mx = 0;
        if (avg) *avg = 0;
        return;
    }
    int32_t sum = 0;
    int8_t vmin = 127, vmax = -128;
    for (uint16_t i = 0; i < s_rssiRingCount; i++) {
        int8_t v = s_rssiRing[i];
        sum += v;
        if (v < vmin) vmin = v;
        if (v > vmax) vmax = v;
    }
    if (mn) *mn = vmin;
    if (mx) *mx = vmax;
    if (avg) *avg = (int8_t)(sum / (int32_t)s_rssiRingCount);
}

/** Отключение Wi‑Fi power save; TX и beacon из config.h. */
static void applyWifiLowLatencyAndPower(void) {
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
    if (!s_thermalThrottled)
        esp_wifi_set_max_tx_power(WIFI_MAX_TX_POWER);

    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) != ESP_OK) return;
    if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) {
        wifi_config_t cfg = {};
        if (esp_wifi_get_config(WIFI_IF_AP, &cfg) == ESP_OK) {
            cfg.ap.beacon_interval = WIFI_AP_BEACON_INTERVAL_TU;
            esp_wifi_set_config(WIFI_IF_AP, &cfg);
        }
    }
}

/* ========== WiFi event handlers (НЕ делаем WiFi.begin() прямо здесь — только флаги) ========== */
static void onWiFiStaDisconnected(WiFiEvent_t, WiFiEventInfo_t info) {
    (void)info;
    StaState prev = g_staState.load();
    if (prev == StaState::CONNECTED || prev == StaState::IDLE) {
        /* Первое падение — фиксируем время и переключаемся в WAIT. */
        g_staFirstFailMs = millis();
    }
    g_staState.store(StaState::WAIT_RECONNECT);
    g_staSinceMs = millis();
    espLogPrintf("[WiFi] STA disconnected, will reconnect (fails=%u)", (unsigned)g_staFails);
}

static void onWiFiStaGotIP(WiFiEvent_t, WiFiEventInfo_t) {
    g_staState.store(StaState::CONNECTED);
    g_staFails = 0;
    g_staFirstFailMs = 0;
    espLogPrintf("[WiFi] STA connected IP %s", WiFi.localIP().toString().c_str());
    applyWifiLowLatencyAndPower();
}

static void onWiFiApStaConnected(WiFiEvent_t, WiFiEventInfo_t) {
    apAssocTotal.fetch_add(1);
    espLogPrintf("[WiFi] AP assoc (stations=%u)", (unsigned)WiFi.softAPgetStationNum());
}

static void onWiFiApStaDisconnected(WiFiEvent_t, WiFiEventInfo_t) {
    apDisassocTotal.fetch_add(1);
    unsigned left = WiFi.softAPgetStationNum();
    espLogPrintf("[WiFi] AP disassoc (stations=%u)", left);
    if (left == 0) {
        /* Все ушли — сбросим UDP-клиента, чтобы не слать в никуда. */
        bridgeClearUdpClient();
    }
}

/* ========== STA supervisor: выполняет переходы FSM раз в ~100 мс ========== */
static void supervisorTick(void) {
    if (bridge_nvs_config.wifi_mode != 2) return; /* только в STA-режиме */
    uint32_t now = millis();
    StaState st = g_staState.load();

    switch (st) {
        case StaState::CONNECTED:
            if (WiFi.status() != WL_CONNECTED) {
                /* События могли прийти до этого тика, но страхуемся. */
                g_staState.store(StaState::WAIT_RECONNECT);
                g_staSinceMs = now;
                g_staFirstFailMs = g_staFirstFailMs ? g_staFirstFailMs : now;
            }
            break;
        case StaState::IDLE:
            /* Инициируем первое подключение. */
            g_staState.store(StaState::CONNECTING);
            g_staSinceMs = now;
            WiFi.begin(bridge_nvs_config.ssid, bridge_nvs_config.wifi_pass);
            break;
        case StaState::WAIT_RECONNECT:
            if ((now - g_staSinceMs) >= STA_RECONNECT_PERIOD_MS) {
                g_staState.store(StaState::CONNECTING);
                g_staSinceMs = now;
                WiFi.disconnect(false, false);
                delay(50);
                WiFi.begin(bridge_nvs_config.ssid, bridge_nvs_config.wifi_pass);
                g_staFails++;
                staReconnectsTotal.fetch_add(1);
                espLogPrintf("[WiFi] STA begin (attempt %u)", (unsigned)g_staFails);
            }
            break;
        case StaState::CONNECTING:
            if (WiFi.status() == WL_CONNECTED) {
                /* Обычно придёт GOT_IP, но страхуемся. */
                g_staState.store(StaState::CONNECTED);
                g_staFails = 0;
                g_staFirstFailMs = 0;
            } else if ((now - g_staSinceMs) > 15000U) {
                /* Таймаут этой попытки — снова WAIT. */
                if (g_staFails >= STA_RECONNECT_MAX_FAILS) {
                    g_staState.store(StaState::HARD_RESET);
                } else {
                    g_staState.store(StaState::WAIT_RECONNECT);
                }
                g_staSinceMs = now;
            }
            break;
        case StaState::HARD_RESET:
            espLogPrintf("[WiFi] STA hard reset (radio off/on)");
            WiFi.disconnect(true, true);
            WiFi.mode(WIFI_OFF);
            delay(200);
            WiFi.mode(WIFI_STA);
            delay(100);
            g_staFails = 0;
            g_staState.store(StaState::WAIT_RECONNECT);
            g_staSinceMs = now;
            break;
        case StaState::GIVE_UP_RESTART:
            s_rtcMagic = RTC_MAGIC_VALUE;
            s_rtcLastUptime = millis() / 1000;
            s_rtcRestartReason = RESTART_REASON_STA_GIVE_UP;
            espLogPrintf("[WiFi] STA give up, restarting ESP32");
            Serial.flush();
            delay(100);
            ESP.restart();
            break;
    }

    /* Если мы давно не в CONNECTED — пора ребутить. */
    if (st != StaState::CONNECTED && g_staFirstFailMs != 0 &&
        (now - g_staFirstFailMs) > STA_RECONNECT_HARD_RESET_MS) {
        g_staState.store(StaState::GIVE_UP_RESTART);
    }
}

/* ========== Thermal guard ========== */
static void thermalGuardTick(void) {
    float t = temperatureRead();
    if (!std::isfinite(static_cast<double>(t))) return;

    if (!s_thermalThrottled && t >= THERMAL_THROTTLE_HIGH_C) {
        esp_wifi_set_max_tx_power(THERMAL_THROTTLE_TX_POWER);
        s_thermalThrottled = true;
        espLogPrintf("[thermal] throttle ON %.1fC (TX=%d)", (double)t, (int)THERMAL_THROTTLE_TX_POWER);
    } else if (s_thermalThrottled && t <= THERMAL_THROTTLE_LOW_C) {
        esp_wifi_set_max_tx_power(WIFI_MAX_TX_POWER);
        s_thermalThrottled = false;
        espLogPrintf("[thermal] throttle OFF %.1fC (TX=%d)", (double)t, (int)WIFI_MAX_TX_POWER);
    }

    /* Периодический лог температуры. */
    espLogPrintf("[chip_temp] %.1f C%s", (double)t, s_thermalThrottled ? " (throttled)" : "");
}

/* ========== Heap monitor ========== */
static void heapGuardTick(void) {
    uint32_t freeH = ESP.getFreeHeap();
    uint32_t minH = ESP.getMinFreeHeap();
    size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    espLogPrintf("[heap] free=%u min=%u largest=%u", (unsigned)freeH, (unsigned)minH, (unsigned)largest);
    if (largest < HEAP_FRAG_WARN_BYTES) {
        espLogPrintf("[heap] WARN fragmentation: largest=%u", (unsigned)largest);
    }
}

/* ========== RSSI tick: раз в секунду кладём значение в кольцевой буфер. ========== */
static void rssiTick(void) {
    int8_t v = bridgeGetRssiDbm();
    rssiHistoryPush(v);
}

/* ========== Запуск uart-task ========== */
extern "C" void bridgeStartUartTask(void) {
    if (s_uartTaskHandle) return;
    BaseType_t r = xTaskCreatePinnedToCore(uartTask, "uart",
                                           UART_TASK_STACK, nullptr,
                                           UART_TASK_PRIORITY, &s_uartTaskHandle,
                                           UART_TASK_CORE);
    if (r != pdPASS) {
        espLogPrintf("[boot] FATAL: uart-task create failed");
    } else {
        espLogPrintf("[boot] uart-task started (core %d, prio %d)",
                     (int)UART_TASK_CORE, (int)UART_TASK_PRIORITY);
    }
}

/* ========== Чтение RTC-данных при загрузке: публикуем причину предыдущего ребута в лог ========== */
static void logPreviousRestart(void) {
    esp_reset_reason_t r = esp_reset_reason();
    const char* reasonName;
    switch (r) {
        case ESP_RST_POWERON:   reasonName = "poweron"; break;
        case ESP_RST_EXT:       reasonName = "external"; break;
        case ESP_RST_SW:        reasonName = "software"; break;
        case ESP_RST_PANIC:     reasonName = "PANIC"; break;
        case ESP_RST_INT_WDT:   reasonName = "int_wdt"; break;
        case ESP_RST_TASK_WDT:  reasonName = "task_wdt"; break;
        case ESP_RST_WDT:       reasonName = "wdt"; break;
        case ESP_RST_DEEPSLEEP: reasonName = "deepsleep"; break;
        case ESP_RST_BROWNOUT:  reasonName = "brownout"; break;
        case ESP_RST_SDIO:      reasonName = "sdio"; break;
        default:                reasonName = "unknown"; break;
    }
    if (s_rtcMagic == RTC_MAGIC_VALUE) {
        const char* customReason = "none";
        switch (s_rtcRestartReason) {
            case RESTART_REASON_STA_GIVE_UP:    customReason = "sta_give_up"; break;
            case RESTART_REASON_PANIC:          customReason = "panic"; break;
            case RESTART_REASON_USER_SETTINGS:  customReason = "user_settings"; break;
        }
        espLogPrintf("[boot] prev uptime=%us, reset=%s, custom=%s",
                     (unsigned)s_rtcLastUptime, reasonName, customReason);
    } else {
        espLogPrintf("[boot] cold start, reset=%s", reasonName);
    }
    /* Сбрасываем для следующего цикла. */
    s_rtcMagic = RTC_MAGIC_VALUE;
    s_rtcLastUptime = 0;
    s_rtcRestartReason = RESTART_REASON_NONE;
}

void setup() {
    delay(500);
    Serial.begin(115200);
    debug.print(F("\nWiFi bridge "));
    debug.println(VERSION);
    espLogPrintf("[boot] WiFi bridge %s", VERSION);

    /* Раньше, чем все — чтобы залогировать причину прошлого ребута. */
    logPreviousRestart();

    /* Watchdog timer: 10 с, включаем panic на таймауте (ребут с причиной PANIC). */
    esp_task_wdt_init(WDT_TIMEOUT_S, true);
    esp_task_wdt_add(nullptr); /* loopTask под WDT */

    loadBridgeConfig();

    uint32_t baud = (bridge_nvs_config.baud > 0) ? bridge_nvs_config.baud : (uint32_t)UART_BAUD;
    int8_t rxPin = (bridge_nvs_config.gpio_rx >= 0) ? bridge_nvs_config.gpio_rx : (int8_t)SERIAL_RXPIN;
    int8_t txPin = (bridge_nvs_config.gpio_tx >= 0) ? bridge_nvs_config.gpio_tx : (int8_t)SERIAL_TXPIN;
    SerialUART.setRxBufferSize(UART_RX_BUF_SIZE);
    SerialUART.setTxBufferSize(UART_TX_BUF_SIZE);
    SerialUART.begin(baud, SERIAL_PARAM, (int)rxPin, (int)txPin);

#ifdef WEB_SERVER
    pinMode(LED_PIN, OUTPUT);
    pinMode(BTN_PIN, INPUT_PULLUP);
    mavlinkInitLog();
#endif

    /* Регистрируем WiFi events ДО включения режимов. */
    WiFi.onEvent(onWiFiStaDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    WiFi.onEvent(onWiFiStaGotIP,         ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent(onWiFiApStaConnected,    ARDUINO_EVENT_WIFI_AP_STACONNECTED);
    WiFi.onEvent(onWiFiApStaDisconnected, ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);

    if (bridge_nvs_config.wifi_mode == 2) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(bridge_nvs_config.ssid, bridge_nvs_config.wifi_pass);
        g_staState.store(StaState::CONNECTING);
        g_staSinceMs = millis();
        {
            uint32_t t0 = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
                esp_task_wdt_reset();
                delay(250);
                debug.print(F("."));
            }
        }
        if (WiFi.status() == WL_CONNECTED) {
            debug.println();
            debug.println(WiFi.localIP());
            g_staState.store(StaState::CONNECTED);
            espLogPrintf("[WiFi] STA connected IP %s", WiFi.localIP().toString().c_str());
        } else {
            debug.println(F("\nSTA: initial connect timeout, will retry in background"));
            espLogPrintf("[WiFi] STA initial connect timeout");
            g_staState.store(StaState::WAIT_RECONNECT);
            g_staSinceMs = millis();
            g_staFirstFailMs = millis();
        }
        applyWifiLowLatencyAndPower();
        if (MDNS.begin(bridge_nvs_config.hostname)) {
            MDNS.addService("_telnet", "_tcp", SERIAL0_TCP_PORT);
            debug.print(F("mDNS: ")); debug.println(bridge_nvs_config.hostname);
        }
    } else {
        uint8_t apCh = bridge_nvs_config.wifi_chan;
        if (apCh < 1 || apCh > 13) apCh = 6;
        WiFi.mode(WIFI_AP);
        WiFi.softAP(bridge_nvs_config.ssid, bridge_nvs_config.wifi_pass, apCh, 0, 4);
        delay(400);
        WiFi.softAPConfig(STATIC_IP, STATIC_IP, NETMASK);
        applyWifiLowLatencyAndPower();
        debug.println(F("AP IP: "));
        debug.println(WiFi.softAPIP());
        espLogPrintf("[WiFi] AP started ch=%u IP %s", (unsigned)apCh, WiFi.softAPIP().toString().c_str());
        g_staState.store(StaState::IDLE);
    }

#ifdef OTA_HANDLER
    ArduinoOTA.begin();
#endif

    bridgeSetup();

#ifdef WEB_SERVER
    LittleFS.begin(true);
    webSetup();
    debug.println(F("Web (local): http://192.168.2.1"));
#endif

#ifdef BATTERY_SAVER
    esp_wifi_set_max_tx_power(50);
#endif

    /* Стартуем UART-task — с этого момента UART обрабатывается в своём потоке. */
    bridgeStartUartTask();

    {
        float tc = temperatureRead();
        if (std::isfinite(static_cast<double>(tc)))
            espLogPrintf("[chip_temp] boot %.1f C", (double)tc);
    }

    /* Инициализируем таймеры первыми вызовами. */
    uint32_t now = millis();
    s_lastThermalMs = now;
    s_lastHeapMs = now;
    s_lastRssiMs = now;
    s_lastSupervisorMs = now;
}

void loop() {
    esp_task_wdt_reset();

#ifdef OTA_HANDLER
    ArduinoOTA.handle();
#endif

    uint32_t now = millis();

    /* Supervisor (STA FSM) — каждые 100 мс. */
    if (now - s_lastSupervisorMs >= 100) {
        s_lastSupervisorMs = now;
        supervisorTick();
        bridgePollDisconnects();
    }

    /* RSSI — раз в секунду. */
    if (now - s_lastRssiMs >= RSSI_TICK_MS) {
        s_lastRssiMs = now;
        rssiTick();
    }

    /* Thermal — раз в 5 с. */
    if (now - s_lastThermalMs >= THERMAL_TICK_MS) {
        s_lastThermalMs = now;
        thermalGuardTick();
    }

    /* Heap — раз в минуту. */
    if (now - s_lastHeapMs >= HEAP_LOG_INTERVAL_MS) {
        s_lastHeapMs = now;
        heapGuardTick();
    }

#ifdef WEB_SERVER
    mavlinkCheckDisconnect();
    if (digitalRead(BTN_PIN) == LOW) {
        static uint32_t lastBtn = 0;
        if (millis() - lastBtn > 300) {
            lastBtn = millis();
            debug.println(F("Btn"));
        }
    }
#endif

    vTaskDelay(5 / portTICK_PERIOD_MS);
}

/* ========== Утилита для web_handlers: запросить restart с указанием причины ========== */
extern "C" void bridgeRequestRestart(uint32_t reason) {
    s_rtcMagic = RTC_MAGIC_VALUE;
    s_rtcLastUptime = millis() / 1000;
    s_rtcRestartReason = reason ? reason : RESTART_REASON_USER_SETTINGS;
    Serial.flush();
    SerialUART.flush();
    delay(100);
    ESP.restart();
}
