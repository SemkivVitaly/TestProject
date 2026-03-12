# Bridge DroneBridge (Project B, V2)

Отдельный проект на **PlatformIO** в каталоге `C:\Users\vital\OneDrive\Desktop\esp32\V2`: прошивка в стиле DroneBridge на C++, веб-интерфейс Bridge на русском языке, метрики канала (пакеты, задержка, потери).

## Сборка и загрузка

- Сборка: `pio run` (из каталога V2).
- Прошивка: `pio run -t upload`.
- Загрузка файловой системы (Bridge UI): `pio run -t uploadfs`.

## Функционал

- Мост Wi‑Fi (TCP/UDP) ↔ UART для Mission Planner и MAVLink.
- Веб-интерфейс Bridge (страница `/bridge`) с метриками и настройками, полностью на русском.
- API: `/api/status`, `/api/link`, `/api/system/info`, `/api/system/stats`, `/api/settings`.

## Структура

```
V2/
├── platformio.ini       # Сборка, плата esp32s3box
├── README.md
├── data/                # LittleFS: index.html и ресурсы Bridge UI (иконки, логотип)
└── src/
    ├── main.cpp         # Точка входа: WiFi, bridge, mavlink, web
    ├── config.h         # Настройки: Wi‑Fi, пины, порты
    ├── bridge.h/cpp     # Мост сеть ↔ UART (TCP/UDP)
    ├── mavlink_state.h/cpp  # Парсинг HEARTBEAT/PARAM_VALUE, лог (использует <MAVLink.h>)
    ├── web_handlers.h/cpp   # HTTP: /, /bridge, /api/*
    ├── db_crc.h/cpp     # CRC (DroneBridge)
```

- **MAVLink** и **ESP32Servo** — из `lib_deps` (`#include <MAVLink.h>`, `#include <ESP32Servo.h>`).
- `data/` — единственное место веб-интерфейса: `index.html` + иконки (favicon, логотип, кнопки). Загружается на плату через `pio run -t uploadfs`.

## Связь с проектом A

Проект A — основной репозиторий с интеграцией Bridge UI, сохранением настроек в NVS и настройками Wi‑Fi из веба. Проект B (этот каталог V2) — автономная прошивка с той же логикой моста и заделом под полный порт DroneBridge на C++.
