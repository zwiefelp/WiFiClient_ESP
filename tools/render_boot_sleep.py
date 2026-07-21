#!/usr/bin/env python3
"""Vorschau des neuen Boot-/Splash- und Sleep-/Dashboard-Screens
(bildet die GFX-Aufrufe aus src/WiFiClient_ESP.ino nach)."""
from PIL import Image, ImageDraw, ImageFont

W, H = 240, 320
SS = 3
w, h = W * SS, H * SS

def rgb565(v):
    r = (v >> 11) & 0x1F; g = (v >> 5) & 0x3F; b = v & 0x1F
    return (r * 255 // 31, g * 255 // 63, b * 255 // 31)

BG      = (0, 0, 0)
CARD    = rgb565(0x0841)
CARD_HI = rgb565(0x4208)
STROKE  = rgb565(0x52AA)
TEXT    = (255, 255, 255)
MUTED   = rgb565(0xC618)
ACCENT  = rgb565(0x07FF)
ON      = rgb565(0x07E0)
OFF     = rgb565(0xF800)
TRACK   = rgb565(0x2945)
AMBER   = rgb565(0xFD20)
VIOLET  = rgb565(0xF81F)

FB = "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans-Bold.ttf"
MONO = "/usr/share/fonts/google-noto-vf/NotoSansMono[wght].ttf"
def fb(px): return ImageFont.truetype(FB, px * SS)
def mono(px): return ImageFont.truetype(MONO, px * SS)
F18 = fb(25); F9 = fb(13); GLCD = mono(10)

def S(v): return int(round(v * SS))

# --- netatmo Bitmaps (aus netatmo_icons.h) ---
ICONS = {
 "hum": (11, [0x04,0,0x0e,0,0x09,0,0x11,0,0x24,0x80,0x28,0x40,0x58,0x40,0x50,0x20,
   0xd0,0x20,0xa0,0x20,0xa0,0x20,0xa0,0x20,0x50,0x20,0x4c,0x40,0x30,0x80,0x1f,0]),
 "co2": (16, [0x03,0x80,0x0c,0x70,0x10,0x08,0x20,0x04,0x40,0x04,0x44,0x62,0x5a,0xd2,
   0x90,0x8a,0x90,0x8a,0x18,0x99,0x4e,0x64,0x40,0x06,0x20,0x04,0x10,0x08,0x0c,0x30,0x03,0xc0]),
 "noise": (15, [0,0x80,0x01,0xc0,0x02,0x40,0x04,0x40,0x08,0x48,0xf0,0x44,0x90,0x72,0x90,0x52,
   0x90,0x52,0x90,0x72,0xf0,0x44,0x08,0x4c,0x04,0x40,0x02,0x40,0x01,0x80,0,0x80]),
 "press": (14, [0x07,0x80,0x18,0x60,0x20,0x10,0x20,0x50,0x40,0x88,0x43,0x08,0x43,0x08,0x40,0x08,
   0x20,0x10,0x20,0x10,0x18,0x60,0x07,0x80,0x03,0,0xff,0xfc,0,0,0xff,0xfc]),
}

def draw_bitmap(d, x, y, name, color):
    width, data = ICONS[name]
    bpr = (width + 7) // 8
    for row in range(16):
        for col in range(width):
            if (data[row * bpr + (col >> 3)] >> (7 - (col & 7))) & 1:
                px = S(x + col); py = S(y + row)
                d.rectangle([px, py, px + SS - 1, py + SS - 1], fill=color)

def arc(d, cx, cy, r, a0, a1, thick, color):
    d.arc([S(cx - r), S(cy - r), S(cx + r), S(cy + r)], a0, a1,
          fill=color, width=S(thick))

def ring(d, cx, cy, r, frac, color):
    frac = max(0.0, min(1.0, frac))
    arc(d, cx, cy, r, 0, 360, 3, TRACK)
    if frac > 0:
        arc(d, cx, cy, r, -90, -90 + 360 * frac, 3, color)

def rrect(d, x, y, ww, hh, rad, fill=None, outline=None):
    d.rounded_rectangle([S(x), S(y), S(x + ww), S(y + hh)], radius=S(rad),
                        fill=fill, outline=outline, width=SS)

def ctext(d, cx, y, s, font, fill):
    d.text((S(cx), S(y)), s, font=font, fill=fill, anchor="ms")

def wifi(d, cx, cy, size, color):
    for rr in (size * 0.30, size * 0.58, size * 0.86):
        arc(d, cx, cy, rr, 210, 330, 3, color)
    d.ellipse([S(cx - 3), S(cy - 3), S(cx + 3), S(cy + 3)], fill=color)


def screen_boot():
    img = Image.new("RGB", (w, h), BG); d = ImageDraw.Draw(img)
    d.ellipse([S(120 - 46), S(92 - 46), S(120 + 46), S(92 + 46)], fill=CARD)
    d.ellipse([S(120 - 34), S(92 - 34), S(120 + 34), S(92 + 34)], fill=CARD_HI)
    wifi(d, 120, 104, 30, ACCENT)
    ctext(d, 120, 168, "Verbinde", F18, TEXT)
    ctext(d, 120, 202, "MyWLAN", F9, MUTED)
    rrect(d, 40, 228, 160, 8, 4, fill=TRACK)
    rrect(d, 40, 228, 160 * 0.6, 8, 4, fill=ACCENT)
    return img.resize((W, H), Image.LANCZOS)


def temp_card(d, x, y, ww, hh, label, temp, hum, col):
    rrect(d, x, y, ww, hh, 10, fill=CARD, outline=STROKE)
    d.text((S(x + 12), S(y + 12)), label, font=GLCD, fill=MUTED, anchor="la")
    d.text((S(x + 10), S(y + 50)), temp, font=F18, fill=col, anchor="ls")
    twm = d.textlength(temp, font=F18)
    d.text((S(x + 10) + twm, S(y + 50)), " C", font=F9, fill=col, anchor="ls")
    draw_bitmap(d, x + 10, y + hh - 22, "hum", ACCENT)
    d.text((S(x + 26), S(y + hh - 9)), hum + "%", font=F9, fill=TEXT, anchor="ls")


def sensor_tile(d, x, y, ww, hh, icon, iw, col, value, unit, frac):
    rrect(d, x, y, ww, hh, 10, fill=CARD, outline=STROKE)
    cx, cy = x + 26, y + hh / 2
    ring(d, cx, cy, 15, frac, col)
    draw_bitmap(d, cx - iw / 2, cy - 8, icon, col)
    d.text((S(x + 48), S(cy - 1)), value, font=F9, fill=TEXT, anchor="ls")
    d.text((S(x + 48), S(cy + 5)), unit, font=GLCD, fill=MUTED, anchor="la")


def screen_sleep():
    img = Image.new("RGB", (w, h), BG); d = ImageDraw.Draw(img)
    ctext(d, 120, 46, "14:30", F18, TEXT)
    ctext(d, 120, 74, "Mo, 21.07.2026", F9, MUTED)
    temp_card(d, 8, 88, 110, 86, "INNEN", "22.5", "45", ON)
    temp_card(d, 122, 88, 110, 86, "AUSSEN", "18.2", "60", OFF)
    sensor_tile(d, 8, 180, 110, 66, "co2", 16, AMBER, "620", "ppm CO2", (620 - 400) / 1600)
    sensor_tile(d, 122, 180, 110, 66, "noise", 15, VIOLET, "38", "dB Laerm", (38 - 30) / 60)
    sensor_tile(d, 8, 250, 110, 66, "press", 14, ACCENT, "1013", "mbar", (1013 - 960) / 80)
    sensor_tile(d, 122, 250, 110, 66, "hum", 11, ON, "78", "% Avocado", 78 / 100)
    return img.resize((W, H), Image.LANCZOS)


def main():
    panels = [("Boot / Splash", "setup()", screen_boot()),
              ("Sleep / Dashboard", "paintSleep()", screen_sleep())]
    pad, title_h, label_h, cap_h = 30, 56, 30, 26
    n = len(panels)
    cw = n * W + (n + 1) * pad
    ch = title_h + label_h + H + cap_h + pad
    canvas = Image.new("RGB", (cw, ch), (243, 244, 247))
    d = ImageDraw.Draw(canvas)
    tf = ImageFont.truetype(FB, 26)
    t = "Boot- & Sleep-Screen im neuen Design"
    d.text(((cw - d.textlength(t, font=tf)) / 2, 16), t, font=tf, fill=(22, 24, 34))
    lf = ImageFont.truetype(FB, 18); cf = ImageFont.truetype(MONO, 13)
    for i, (lbl, fn, im) in enumerate(panels):
        x = pad + i * (W + pad); y = title_h + label_h
        d.text((x + (W - d.textlength(lbl, font=lf)) / 2, title_h + 4), lbl, font=lf, fill=(22, 24, 34))
        d.rectangle([x - 2, y - 2, x + W + 1, y + H + 1], outline=(70, 78, 96), width=2)
        canvas.paste(im, (x, y))
        d.text((x + (W - d.textlength(fn, font=cf)) / 2, y + H + 6), fn, font=cf, fill=(120, 126, 140))
    import os
    os.makedirs("docs", exist_ok=True)
    canvas.save("docs/boot_sleep_preview.png")
    print("geschrieben: docs/boot_sleep_preview.png", canvas.size)


if __name__ == "__main__":
    main()
