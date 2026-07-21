#!/usr/bin/env python3
"""Rendert ALLE Screens aus der echten Konfiguration (docs/espconfig.conf)
mit der neuen paintScreen()-Darstellung, als Overview-PNG.

Zustaende sind simuliert (keine Live-MQTT-Daten):
- statetopic endet auf 'none'  -> stateless -> beide Tasten inaktiv (Rahmen)
- sonst abwechselnd links/rechts aktiv, um beide Stile zu zeigen.
"""
import re
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
F12 = ImageFont.truetype(FB, 17)
F9  = ImageFont.truetype(FB, 13)
GLCD = ImageFont.truetype(MONO, 10)

CARD_X, CARD_W, CARD_H, BAND_H = 8, 224, 56, 64


def parse_config(path):
    screens = []
    cur = None
    for line in open(path):
        line = line.rstrip("\n")
        if line.startswith("StartScreen:"):
            cur = {"name": line.split(":", 1)[1], "members": []}
        elif line.startswith("EndScreen"):
            if cur:
                screens.append(cur); cur = None
        elif line.startswith("Member:") and cur is not None:
            p = line.split(":")
            # Member:idx:name1:name2:type:topic:txtl:txtr:cmdl:cmdr:statetopic
            cur["members"].append({
                "idx": int(p[1]), "name1": p[2], "name2": p[3], "type": p[4],
                "topic": p[5], "txtl": p[6], "txtr": p[7],
                "cmdl": p[8], "cmdr": p[9], "statetopic": p[10],
            })
    return screens


def key_color(t):
    if t in ("Ein", "An", "Start"): return ON
    if t in ("Aus", "Stop"): return OFF
    return NEUTRAL


def render_screen(screen, index, total):
    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)

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

    # Titel + Navigation + Punkte (Band 0)
    d.text((W / 2, 30), screen["name"], font=F12, fill=TEXT, anchor="ms")
    d.polygon([(10, 18), (10, 30), (4, 24)], fill=MUTED)
    d.polygon([(230, 18), (230, 30), (236, 24)], fill=MUTED)
    dx = (W - total * 10) // 2
    for k in range(total):
        cx = dx + k * 10 + 3
        d.ellipse([cx - 2, 44, cx + 2, 48], fill=ACCENT if k == index else STROKE)

    for m in screen["members"]:
        i = m["idx"]
        cy = i * BAND_H + (BAND_H - CARD_H) // 2
        d.rounded_rectangle([CARD_X, cy, CARD_X + CARD_W, cy + CARD_H], radius=8,
                            fill=CARD, outline=STROKE)
        # Zustand simulieren
        stateless = m["statetopic"].split("/")[-1] == "none"
        if stateless:
            left_on = right_on = False
        else:
            right_on = (i % 2 == 0)
            left_on = not right_on
        ky = cy + (CARD_H - 26) // 2
        lw = key_width(m["txtl"]); rw = key_width(m["txtr"])
        draw_key(CARD_X + 6, ky, m["txtl"], key_color(m["txtl"]), left_on)
        draw_key(CARD_X + CARD_W - 6 - rw, ky, m["txtr"], key_color(m["txtr"]), right_on)
        cx = ((CARD_X + 6 + lw) + (CARD_X + CARD_W - 6 - rw)) / 2
        d.text((cx, cy + 18), m["name1"], font=GLCD, fill=MUTED, anchor="ma")
        d.text((cx, cy + 42), m["name2"], font=F9, fill=TEXT, anchor="ms")

    return img


def main():
    screens = parse_config("docs/espconfig.conf")
    total = len(screens)
    panels = [render_screen(s, i, total) for i, s in enumerate(screens)]

    cols = 4
    rows = (total + cols - 1) // cols
    pad, label_h, title_h = 24, 26, 50
    cw = cols * W + (cols + 1) * pad
    ch = title_h + rows * (label_h + H + pad)
    canvas = Image.new("RGB", (cw, ch), (243, 244, 247))
    d = ImageDraw.Draw(canvas)
    tf = ImageFont.truetype(FB, 26)
    t = "Echte Konfiguration (espconfig.conf) - 7 Screens, Zustaende simuliert"
    d.text(((cw - d.textlength(t, font=tf)) / 2, 14), t, font=tf, fill=(22, 24, 34))

    lf = ImageFont.truetype(FB, 17)
    for idx, (s, im) in enumerate(zip(screens, panels)):
        r, c = idx // cols, idx % cols
        x = pad + c * (W + pad)
        y = title_h + r * (label_h + H + pad) + label_h
        lw = d.textlength(s["name"], font=lf)
        d.text((x + (W - lw) / 2, y - label_h + 2), s["name"], font=lf, fill=(22, 24, 34))
        d.rectangle([x - 2, y - 2, x + W + 1, y + H + 1], outline=(70, 78, 96), width=2)
        canvas.paste(im, (x, y))

    import os
    os.makedirs("docs", exist_ok=True)
    canvas.save("docs/config_screens.png")
    print("geschrieben: docs/config_screens.png", canvas.size)


if __name__ == "__main__":
    main()
