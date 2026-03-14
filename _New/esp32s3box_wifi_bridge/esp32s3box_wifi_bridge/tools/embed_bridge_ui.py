#!/usr/bin/env python3
"""
Generate include/bridge_ui_embed.h from data/index.html for embedding Bridge UI in firmware.
Run before build (e.g. extra_scripts in platformio.ini).
"""
import os

# When run by PlatformIO/SCons, __file__ may be undefined; use cwd (project dir).
try:
    _script_dir = os.path.dirname(os.path.abspath(__file__))
    PROJECT_DIR = os.path.dirname(_script_dir)
except NameError:
    PROJECT_DIR = os.getcwd()

DATA_HTML = os.path.join(PROJECT_DIR, "data", "index.html")
OUTPUT_H = os.path.join(PROJECT_DIR, "include", "bridge_ui_embed.h")
DELIM = "BRIDGE_UI_RAW"

def main():
    with open(DATA_HTML, "r", encoding="utf-8") as f:
        content = f.read()
    # Raw string delimiter must not appear in content
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
