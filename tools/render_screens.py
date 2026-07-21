#!/usr/bin/env python3
"""Rendert eine Uebersicht der auf dem ILI9341-TFT (240x320) dargestellten
Screens des WiFiClient_ESP-Sketches als PNG.

Die Screens entsprechen den Funktionen in src/WiFiClient_ESP.ino:
  1. Boot/WLAN        -> setup()
  2. MQTT-Verbindung  -> reconnect()
  3. Konfiguration    -> getConfiguration()
  4. Steuerung        -> paintScreen()
  5. Sleep/Info       -> paintSleep()
"""

from PIL import Image, ImageDraw, ImageFont

# ---- ILI9341 Farben (RGB565 -> RGB888 Naeherung) ----
BLACK  = (0, 0, 0)
WHITE  = (255, 255, 255)
GREEN  = (0, 255, 0)
RED    = (255, 0, 0)
YELLOW = (255, 255, 0)

W, H = 240, 320  # Displayaufloesung (Hochformat, setRotation(2))

FONT = "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf"
MONO = "/usr/share/fonts/google-noto-vf/NotoSansMono[wght].ttf"

def font(px):
    return ImageFont.truetype(FONT, px)

# GLCD-Standardfont (tft.setFont() ohne Argument): ~6x8 px, monospace
def glcd():
    return ImageFont.truetype(MONO, 10)

# Adafruit-GFX FreeSans-Naeherungen
F9  = font(13)   # FreeSans9pt7b
F12 = font(17)   # FreeSans12pt7b
F18 = font(25)   # FreeSans18pt7b


def center_x(draw, text, fnt):
    w = draw.textlength(text, font=fnt)
    return (W - w) / 2


def new_screen():
    img = Image.new("RGB", (W, H), BLACK)
    return img, ImageDraw.Draw(img)


# ---------------------------------------------------------------------------
# 1. Boot / WLAN-Verbindung  (setup)
# ---------------------------------------------------------------------------
def screen_boot():
    img, d = new_screen()
    f = glcd()
    y = 4
    lh = 12
    d.text((2, y), "Connecting to MyWLAN.....", font=f, fill=WHITE); y += lh
    d.text((2, y), "WiFi connected", font=f, fill=WHITE); y += lh
    d.text((2, y), "IP address: 192.168.1.42", font=f, fill=WHITE)
    return img


# ---------------------------------------------------------------------------
# 2. MQTT-Verbindung  (reconnect)
# ---------------------------------------------------------------------------
def screen_mqtt():
    img, d = new_screen()
    f = glcd()
    y = 4
    lh = 12
    d.text((2, y), "Attempting MQTT connection...", font=f, fill=WHITE); y += lh
    d.text((2, y), "connected", font=f, fill=WHITE)
    return img


# ---------------------------------------------------------------------------
# 3. Konfiguration  (getConfiguration)
# ---------------------------------------------------------------------------
def screen_config():
    img, d = new_screen()
    f = glcd()
    y = 4
    lh = 12
    lines = [
        "initialize",
        "Getting Configuration...",
        "Screens:3",
        "StartScreen:Wohnzimmer",
        "Member:1:Licht:Decke:...",
        "Member:2:Steckdose:TV:...",
        "EndScreen",
        "StartScreen:Kueche",
        "...",
        "EndConfig",
    ]
    for ln in lines:
        d.text((2, y), ln, font=f, fill=WHITE); y += lh
    return img


# ---------------------------------------------------------------------------
# 4. Steuerungs-Screen  (paintScreen)
# ---------------------------------------------------------------------------
def color_for(text):
    if text == "Ein":
        return GREEN
    if text == "Aus":
        return RED
    return YELLOW


def rounded_button(d, cx_right, ypos, text, fnt, active):
    """Button rechtsbuendig (wie txtr in paintScreen)."""
    color = color_for(text)
    w = d.textlength(text, font=fnt)
    h = 14
    xpos = W - w - 6
    box = [xpos - 4, ypos - 16, xpos - 4 + w + 8, ypos - 16 + h + 8]
    if active:
        d.rounded_rectangle(box, radius=2, fill=color)
        d.text((xpos, ypos - 14), text, font=fnt, fill=WHITE)
    else:
        d.rounded_rectangle(box, radius=2, outline=color)
        d.text((xpos, ypos - 14), text, font=fnt, fill=color)


def rounded_button_left(d, ypos, text, fnt, active):
    """Button linksbuendig (wie txtl in paintScreen)."""
    color = color_for(text)
    w = d.textlength(text, font=fnt)
    h = 14
    box = [6 - 4, ypos - 16, 6 - 4 + w + 8, ypos - 16 + h + 8]
    if active:
        d.rounded_rectangle(box, radius=2, fill=color)
        d.text((6, ypos - 14), text, font=fnt, fill=WHITE)
    else:
        d.rounded_rectangle(box, radius=2, outline=color)
        d.text((6, ypos - 14), text, font=fnt, fill=color)


def screen_control():
    img, d = new_screen()
    # Titelzeile
    f = glcd()
    title = "Wohnzimmer (1/3)"
    d.text((center_x(d, title, f), 2), title, font=f, fill=WHITE)
    # Navigations-Dreiecke
    d.polygon([(231, 14), (231, 4), (238, 9)], fill=WHITE)  # rechts
    d.polygon([(9, 14), (9, 4), (2, 9)], fill=WHITE)         # links

    members = [
        ("Licht", "Decke",   "Ein", "Aus", True,  False),
        ("Steckdose", "TV",  "Ein", "Aus", False, True),
        ("Rollo", "Fenster", "Auf", "Ab",  False, False),
        ("Heizung", "WZ",    "Ein", "Aus", True,  False),
    ]
    for i, (n1, n2, txtl, txtr, actl, actr) in enumerate(members, start=1):
        ypos = (i - 1) * 83 + 15 + 35
        rounded_button_left(d, ypos, txtl, F9, actl)
        # Namen zentriert
        d.text((center_x(d, n1, F9), ypos - 22), n1, font=F9, fill=WHITE)
        d.text((center_x(d, n2, F9), ypos - 4), n2, font=F9, fill=WHITE)
        rounded_button(d, None, ypos, txtr, F9, actr)
    return img


# ---------------------------------------------------------------------------
# 5. Sleep / Info-Screen  (paintSleep)
# ---------------------------------------------------------------------------
def right_text(d, text, fnt, right_x, y, fill):
    w = d.textlength(text, font=fnt)
    d.text((right_x - w, y), text, font=fnt, fill=fill)


# Monochrome Adafruit-GFX-Bitmaps aus src/netatmo_icons.h (MSB-first, 16px hoch)
ICONS = {
    "hum": (11, [
        0x04,0x00, 0x0e,0x00, 0x09,0x00, 0x11,0x00, 0x24,0x80, 0x28,0x40,
        0x58,0x40, 0x50,0x20, 0xd0,0x20, 0xa0,0x20, 0xa0,0x20, 0xa0,0x20,
        0x50,0x20, 0x4c,0x40, 0x30,0x80, 0x1f,0x00,
    ]),
    "co2": (16, [
        0x03,0x80, 0x0c,0x70, 0x10,0x08, 0x20,0x04, 0x40,0x04, 0x44,0x62,
        0x5a,0xd2, 0x90,0x8a, 0x90,0x8a, 0x18,0x99, 0x4e,0x64, 0x40,0x06,
        0x20,0x04, 0x10,0x08, 0x0c,0x30, 0x03,0xc0,
    ]),
    "noise": (15, [
        0x00,0x80, 0x01,0xc0, 0x02,0x40, 0x04,0x40, 0x08,0x48, 0xf0,0x44,
        0x90,0x72, 0x90,0x52, 0x90,0x52, 0x90,0x72, 0xf0,0x44, 0x08,0x4c,
        0x04,0x40, 0x02,0x40, 0x01,0x80, 0x00,0x80,
    ]),
    "press": (14, [
        0x07,0x80, 0x18,0x60, 0x20,0x10, 0x20,0x50, 0x40,0x88, 0x43,0x08,
        0x43,0x08, 0x40,0x08, 0x20,0x10, 0x20,0x10, 0x18,0x60, 0x07,0x80,
        0x03,0x00, 0xff,0xfc, 0x00,0x00, 0xff,0xfc,
    ]),
}


def icon(d, x, y, name):
    """Zeichnet ein monochromes GFX-Bitmap (wie tft.drawBitmap)."""
    width, data = ICONS[name]
    bytes_per_row = (width + 7) // 8
    for row in range(16):
        for col in range(width):
            byte = data[row * bytes_per_row + (col >> 3)]
            if (byte >> (7 - (col & 7))) & 1:
                d.point((x + col, y + row), fill=WHITE)


def screen_sleep():
    img, d = new_screen()
    # Uhrzeit gross zentriert
    t = "14:30"
    d.text((center_x(d, t, F18), 14), t, font=F18, fill=WHITE)
    # Datum
    dt = "Mon,21.07.2026"
    d.text((center_x(d, dt, F9), 48), dt, font=F9, fill=WHITE)

    f = glcd()
    # --- Netatmo Innen ---
    d.text((10, 102), "Netatmo Innen", font=f, fill=WHITE)
    right_text(d, "22.5 C", F18, 230, 116, GREEN)
    icon(d, 10, 156, "hum");   right_text(d, "45 %", F9, 100, 156, WHITE)
    icon(d, 130, 156, "press"); right_text(d, "1013 mb", F9, 230, 156, WHITE)
    icon(d, 8, 176, "co2");    right_text(d, "620 ppm", F9, 100, 176, WHITE)
    icon(d, 130, 176, "noise"); right_text(d, "38 db", F9, 230, 176, WHITE)

    # --- Avocado WZ ---
    d.text((10, 192), "Avocado WZ", font=f, fill=WHITE)
    icon(d, 130, 196, "hum"); right_text(d, "78 %", F9, 230, 196, WHITE)

    # --- Netatmo Aussen ---
    d.text((10, 227), "Netatmo Aussen", font=f, fill=WHITE)
    right_text(d, "18.2 C", F18, 230, 241, RED)
    icon(d, 10, 281, "hum");   right_text(d, "60 %", F9, 100, 281, WHITE)
    icon(d, 130, 281, "press"); right_text(d, "1013 mb", F9, 230, 281, WHITE)
    return img


# ---------------------------------------------------------------------------
# Uebersicht zusammensetzen
# ---------------------------------------------------------------------------
def main():
    screens = [
        ("1. Boot / WLAN", "setup()", screen_boot()),
        ("2. MQTT-Verbindung", "reconnect()", screen_mqtt()),
        ("3. Konfiguration", "getConfiguration()", screen_config()),
        ("4. Steuerung", "paintScreen()", screen_control()),
        ("5. Sleep / Info", "paintSleep()", screen_sleep()),
    ]

    pad = 30
    label_h = 34
    caption_h = 28
    title_h = 60
    n = len(screens)
    panel_w = W
    cw = n * panel_w + (n + 1) * pad
    ch = title_h + label_h + H + caption_h + pad

    canvas = Image.new("RGB", (cw, ch), (245, 245, 247))
    d = ImageDraw.Draw(canvas)

    tfont = font(30)
    title = "WiFiClient_ESP  -  Uebersicht der TFT-Screens (ILI9341, 240x320)"
    d.text(((cw - d.textlength(title, font=tfont)) / 2, 16), title,
           font=tfont, fill=(20, 20, 30))

    lfont = font(19)
    cfont = font(15)
    for i, (label, fn, img) in enumerate(screens):
        x = pad + i * (panel_w + pad)
        y = title_h + label_h
        # Label
        lw = d.textlength(label, font=lfont)
        d.text((x + (panel_w - lw) / 2, title_h + 4), label, font=lfont,
               fill=(20, 20, 30))
        # Rahmen + Screen
        d.rectangle([x - 2, y - 2, x + panel_w + 1, y + H + 1],
                    outline=(60, 60, 70), width=2)
        canvas.paste(img, (x, y))
        # Caption (Codefunktion)
        cwid = d.textlength(fn, font=cfont)
        d.text((x + (panel_w - cwid) / 2, y + H + 6), fn, font=cfont,
               fill=(90, 90, 100))

    out = "docs/screens_overview.png"
    import os
    os.makedirs("docs", exist_ok=True)
    canvas.save(out)
    print("geschrieben:", out, canvas.size)


if __name__ == "__main__":
    main()
