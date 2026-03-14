#!/usr/bin/env python3
"""
embed_bridge_ui.py — пре-сборка: встраивание веб-интерфейса Bridge в прошивку.

НАЗНАЧЕНИЕ:
   Читает data/index.html (страница Bridge UI) и генерирует include/bridge_ui_embed.h —
   C++ заголовок с константой BRIDGE_UI_HTML[] в PROGMEM (хранение во флеши, не в RAM).
   Веб-сервер в web_handlers.cpp при запросе /bridge отдаёт эту строку (или файл из LittleFS, если embed не подключён).

КОГДА ВЫЗЫВАЕТСЯ:
   PlatformIO перед каждой сборкой (см. platformio.ini: extra_scripts = pre:tools/embed_bridge_ui.py).
   Можно запустить вручную: python3 tools/embed_bridge_ui.py из корня проекта.

ВХОД:  data/index.html
ВЫХОД: include/bridge_ui_embed.h (определения BRIDGE_UI_EMBED_H, BRIDGE_UI_HTML[]).
"""
import os

# При запуске из PlatformIO/SCons __file__ может быть не определён — тогда берём текущую директорию как корень проекта.
try:
    _script_dir = os.path.dirname(os.path.abspath(__file__))
    PROJECT_DIR = os.path.dirname(_script_dir)
except NameError:
    PROJECT_DIR = os.getcwd()

DATA_HTML = os.path.join(PROJECT_DIR, "data", "index.html")
OUTPUT_H = os.path.join(PROJECT_DIR, "include", "bridge_ui_embed.h")
DELIM = "BRIDGE_UI_RAW"  # Разделитель raw string в C++; не должен встречаться внутри content.

def main():
    with open(DATA_HTML, "r", encoding="utf-8") as f:
        content = f.read()
    # В C++ raw string R"DELIM(...)DELIM" — если в HTML есть )DELIM", парсер сломается.
    if (")" + DELIM + '"') in content:
        raise SystemExit("Delimiter )%s\" found in HTML - use another DELIM" % DELIM)
    os.makedirs(os.path.dirname(OUTPUT_H), exist_ok=True)
    with open(OUTPUT_H, "w", encoding="utf-8") as f:
        f.write("/* Auto-generated from data/index.html - do not edit */\n")
        f.write("#ifndef BRIDGE_UI_EMBED_H\n#define BRIDGE_UI_EMBED_H\n\n")
        f.write("#ifndef PROGMEM\n#define PROGMEM\n#endif\n\n")
        f.write("static const char BRIDGE_UI_HTML[] PROGMEM = R\"" + DELIM + "(" + content + ")" + DELIM + "\";\n\n")
        f.write("#endif /* BRIDGE_UI_EMBED_H */\n")
    print("Generated", OUTPUT_H)

if __name__ == "__main__":
    main()
