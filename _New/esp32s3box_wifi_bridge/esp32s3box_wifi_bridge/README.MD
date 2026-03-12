# WiFi-мост ESP32-S3-Box для Mission Planner (MAVLink)

Мост между наземной станцией (Mission Planner, QGC) и автопилотом: **Wi‑Fi (TCP/UDP)** ↔ **UART** ↔ **автопилот**. Поддержка веб-интерфейса, параметров SERVO и лога MAVLink.

- **Сеть Wi‑Fi:** имя и пароль в `src/config.h` (по умолчанию AP: `ESP32S3_NEW` / `12345678`, IP `192.168.2.1`).
- **Подключение:** TCP порт **8880**, UDP порт **14550** (см. [docs/Инструкция_MAVLink_и_Mission_Planner.md](docs/Инструкция_MAVLink_и_Mission_Planner.md)).
- **Веб-интерфейс только локально:** подключитесь к Wi‑Fi платы → в браузере откройте **http://192.168.2.1**. Интернет не нужен. Подробнее и список функционала — [docs/Веб_интерфейс_и_функционал.md](docs/Веб_интерфейс_и_функционал.md).

---

## Структура проекта

```
esp32s3box_wifi_bridge/
├── platformio.ini          # Сборка, плата esp32s3box, зависимости
├── README.MD               # Этот файл
├── data/                   # LittleFS: index.html и ресурсы для Bridge UI (иконки, логотип)
├── src/
│   ├── main.cpp            # Точка входа: WiFi, setup/loop, вызовы bridge/mavlink/web
│   ├── config.h            # Настройки: Wi‑Fi, пины, порты, флаги (MODE_AP/STA, WEB_SERVER и т.д.)
│   ├── bridge_nvs.h/cpp    # Сохранение настроек в NVS (Wi‑Fi, режим, hostname и т.д.)
│   ├── mavlink_state.h     # Состояние MAVLink и API (парсинг, отправка параметров)
│   ├── mavlink_state.cpp   # Реализация: HEARTBEAT/PARAM_VALUE, PARAM_REQUEST_READ/PARAM_SET, лог
│   ├── bridge.h/cpp        # API моста: сеть ↔ UART (TCP/UDP/BT)
│   ├── web_handlers.h      # Регистрация маршрутов веб-сервера
│   └── web_handlers.cpp    # Обработчики HTTP: /, /params, /bridge, /api/*
└── docs/
    ├── Инструкция_MAVLink_и_Mission_Planner.md  # Подключение, примеры кода, подсказки
    ├── Mission_Planner_подключение.md
    ├── Курс_PlatformIO_ESP32-S3-Box.md
    └── ...
```

- **config.h** — единственное место для смены SSID, пароля, пинов, портов, включения TCP/UDP/WEB_SERVER/OTA/BLUETOOTH.
- **MAVLink** и **ESP32Servo** подключаются напрямую из зависимостей (`lib_deps` в platformio.ini): `#include <MAVLink.h>`, `#include <ESP32Servo.h>`.
- **mavlink_state** — разбор входящего MAVLink (подключение, параметры SERVO), отправка запросов и установка параметров, лог.
- **bridge** — приём TCP-клиентов, чтение из TCP/BT → UART, отправка UART → TCP/UDP/BT.
- **web_handlers** — веб-страницы и API для статуса, параметров SERVO и лога.

Сборка: **PlatformIO** (Build / Upload). Документация по подключению и работе с MAVLink — в `docs/Инструкция_MAVLink_и_Mission_Planner.md`.

---

## Два проекта (план DroneBridge → Bridge)

- **Проект A (этот репозиторий):** веб-интерфейс **Bridge** (стиль DroneBridge), метрики канала (пакеты, задержка, потери), **сохранение настроек в NVS** и **настройки Wi‑Fi/режим из веба**. Главная страница `/`; полный Bridge UI — **`/bridge`** (старый адрес `/smd` перенаправляется на `/bridge`). Для работы `/bridge` нужна загрузка файловой системы: **`pio run -t uploadfs`**.
- **Проект B (V2)** — отдельная прошивка в каталоге **`C:\Users\vital\OneDrive\Desktop\esp32\V2`**: та же логика моста и Bridge UI на русском языке, задел под полный порт DroneBridge на C++. Сборка: открыть папку `V2` в PlatformIO и выполнить `pio run` / `pio run -t upload` / `pio run -t uploadfs`.
