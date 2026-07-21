#!/usr/bin/env python3
"""Originalgetreue Vorschau der neu implementierten paintScreen()-Funktion.

Bildet 1:1 die GFX-Aufrufe aus src/WiFiClient_ESP.ino nach:
- Karte pro Member, Name mittig, kein Icon
- links/rechts je eine Taste (mappt auf die physischen Taster neben dem Display)
- aktive Taste gefuellt + schwarzer Text (hoher Kontrast), inaktiv nur Rahmen
"""
from PIL import Image, ImageDraw, ImageFont

W, H = 240, 320

def rgb565(v):
    r = (v >> 11) & 0x1F; g = (v >> 5) & 0x3F; b = v & 0x1F
    return (r * 255 // 31, g * 255 // 63, b * 255 // 31)

BG      = (0, 0, 0)
CARD    = rgb565(0x0841)
STROKE  = rgb565(0x52AA)
TEXT    = (255, 255, 255)
MUTED   = rgb565(0xC618)
ACCENT  = rgb565(0x07FF)
ON      = rgb565(0x07E0)
OFF     = rgb565(0xF800)
NEUTRAL = rgb565(0xC618)

FB = "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans-Bold.ttf"
MONO = "/usr/share/fonts/google-noto-vf/NotoSansMono[wght].ttf"
F12 = ImageFont.truetype(FB, 17)   # FreeSans12pt7b
F9  = ImageFont.truetype(FB, 13)   # FreeSans9pt7b
GLCD = ImageFont.truetype(MONO, 10)

# Kartenmasse (== CARD_* im Sketch)
CARD_X, CARD_W, CARD_H, BAND_H = 8, 224, 56, 64

img = Image.new("RGB", (W, H), BG)
d = ImageDraw.Draw(img)

scrname = "Kueche"
num, nScreens = 0, 5
# (name1, name2, txtl, txtr, leftOn, rightOn)  -- wie in der realen Config
members = [
    ("Licht", "Esstisch", "Aus", "Ein", False, True),
    ("Licht", "EZ Decke", "Aus", "Ein", True, False),
    ("Rollo", "Fenster", "Auf", "Ab", True, False),
    ("Licht", "Bar", "Aus", "Ein", False, True),
]

def key_color(t):
    if t in ("Ein", "An", "Start"): return ON
    if t in ("Aus", "Stop"): return OFF
    return NEUTRAL   # neutral, z.B. Rollo Auf/Ab/Zu

def key_width(label):
    return d.textlength(label, font=F9) + 16

def draw_key(x, y, label, color, active):
    h = 26
    w = d.textlength(label, font=F9) + 16
    if active:
        d.rounded_rectangle([x, y, x + w, y + h], radius=5, fill=color)
        d.text((x + 8, y + h / 2), label, font=F9, fill=BG, anchor="lm")
    else:
        d.rounded_rectangle([x, y, x + w, y + h], radius=5, outline=color)
        d.text((x + 8, y + h / 2), label, font=F9, fill=color, anchor="lm")

# ---- Titelzeile ----
tw = d.textlength(scrname, font=F12)
d.text(((W - tw) / 2, 30), scrname, font=F12, fill=TEXT, anchor="ls")
d.polygon([(10, 18), (10, 30), (4, 24)], fill=MUTED)
d.polygon([(230, 18), (230, 30), (236, 24)], fill=MUTED)
dots = nScreens + 1
dx = (W - dots * 10) // 2
for k in range(dots):
    cx = dx + k * 10 + 3
    d.ellipse([cx - 2, 44, cx + 2, 48], fill=ACCENT if k == num else STROKE)

# ---- Karten ----
for i, (n1, n2, lt, rt, left_on, right_on) in enumerate(members, start=1):
    cy = i * BAND_H + (BAND_H - CARD_H) // 2
    d.rounded_rectangle([CARD_X, cy, CARD_X + CARD_W, cy + CARD_H], radius=8,
                        fill=CARD, outline=STROKE)
    ky = cy + (CARD_H - 26) // 2
    lw = key_width(lt)
    rw = key_width(rt)
    draw_key(CARD_X + 6, ky, lt, key_color(lt), left_on)
    draw_key(CARD_X + CARD_W - 6 - rw, ky, rt, key_color(rt), right_on)
    cx = ((CARD_X + 6 + lw) + (CARD_X + CARD_W - 6 - rw)) / 2
    d.text((cx, cy + 18), n1, font=GLCD, fill=MUTED, anchor="ma")   # Typ dezent
    d.text((cx, cy + 42), n2, font=F9, fill=TEXT, anchor="ms")      # Name betont

out = img.resize((W * 3, H * 3), Image.NEAREST)
import os
os.makedirs("docs", exist_ok=True)
out.save("docs/paintscreen_preview.png")
print("geschrieben: docs/paintscreen_preview.png", out.size)
