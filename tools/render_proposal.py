#!/usr/bin/env python3
"""Design-Vorschlag: elegantere Darstellung der WiFiClient_ESP-Screens.

Rendert drei neu gestaltete Schluessel-Screens (Splash, Steuerung, Dashboard)
fuer das ILI9341-TFT (240x320). Rein konzeptioneller Mockup - zeigt, wie die
bestehenden Funktionen (setup/paintScreen/paintSleep) grafisch aufgewertet
werden koennten, weiterhin machbar mit Adafruit-GFX-Primitiven.
"""

from PIL import Image, ImageDraw, ImageFont

W, H = 240, 320
SS = 3                      # Supersampling fuer glatte Kanten
w, h = W * SS, H * SS

# ---- Farbpalette (dunkles Theme mit Akzenten) ----
BG_TOP   = (14, 18, 27)
BG_BOT   = (26, 32, 46)
CARD     = (30, 38, 56)
CARD_HI  = (38, 48, 70)
STROKE   = (52, 64, 92)
ACCENT   = (56, 199, 244)   # cyan
GREEN    = (52, 211, 153)
AMBER    = (251, 191, 36)
RED      = (248, 113, 113)
VIOLET   = (167, 139, 250)
TEXT     = (238, 243, 250)
MUTED    = (140, 152, 174)
TRACK    = (46, 56, 80)

FB = "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans-Bold.ttf"
FR = "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf"

def fb(px): return ImageFont.truetype(FB, px * SS)
def fr(px): return ImageFont.truetype(FR, px * SS)

def S(v): return int(round(v * SS))


def gradient_bg():
    img = Image.new("RGB", (w, h), BG_TOP)
    top, bot = BG_TOP, BG_BOT
    for yy in range(h):
        t = yy / (h - 1)
        c = tuple(int(top[i] + (bot[i] - top[i]) * t) for i in range(3))
        for xx in range(0, w):
            pass
        img.paste(c, (0, yy, w, yy + 1))
    return img


def rrect(d, x, y, ww, hh, r, fill=None, outline=None, width=1):
    d.rounded_rectangle([S(x), S(y), S(x + ww), S(y + hh)], radius=S(r),
                        fill=fill, outline=outline, width=S(width))


def text(d, x, y, s, font, fill, anchor="la"):
    d.text((S(x), S(y)), s, font=font, fill=fill, anchor=anchor)


def ctext(d, cx, y, s, font, fill):
    d.text((S(cx), S(y)), s, font=font, fill=fill, anchor="ma")


# ---------------------------------------------------------------------------
# Icons (schlichte Vektor-Glyphen, in nativen Koordinaten)
# ---------------------------------------------------------------------------
def i_wifi(d, cx, cy, size, color, lw=2):
    for rr in (size * 0.30, size * 0.58, size * 0.86):
        d.arc([S(cx - rr), S(cy - rr), S(cx + rr), S(cy + rr)], 210, 330,
              fill=color, width=S(lw))
    r = size * 0.09
    d.ellipse([S(cx - r), S(cy - r), S(cx + r), S(cy + r)], fill=color)


def i_bulb(d, cx, cy, size, color, lw=2):
    r = size * 0.5
    d.ellipse([S(cx - r), S(cy - r), S(cx + r), S(cy + r)], outline=color, width=S(lw))
    bw = r * 0.55
    yb = cy + r * 0.95
    for dx in (-bw, 0, bw):
        d.line([S(cx + dx), S(yb), S(cx + dx), S(yb + size * 0.22)], fill=color, width=S(lw))


def i_plug(d, cx, cy, size, color, lw=2):
    r = size * 0.42
    d.arc([S(cx - r), S(cy - r), S(cx + r), S(cy + r)], 0, 180, fill=color, width=S(lw))
    d.line([S(cx - r), S(cy), S(cx - r), S(cy - size * 0.35)], fill=color, width=S(lw))
    d.line([S(cx + r), S(cy), S(cx + r), S(cy - size * 0.35)], fill=color, width=S(lw))
    d.line([S(cx - r), S(cy), S(cx + r), S(cy)], fill=color, width=S(lw))
    d.line([S(cx), S(cy), S(cx), S(cy + size * 0.4)], fill=color, width=S(lw))


def i_blinds(d, cx, cy, size, color, lw=2):
    s = size * 0.5
    d.rectangle([S(cx - s), S(cy - s), S(cx + s), S(cy + s)], outline=color, width=S(lw))
    for k in range(1, 4):
        yy = cy - s + (2 * s) * k / 4
        d.line([S(cx - s), S(yy), S(cx + s), S(yy)], fill=color, width=S(lw))


def i_heat(d, cx, cy, size, color, lw=2):
    s = size * 0.5
    d.rounded_rectangle([S(cx - s), S(cy - s), S(cx + s), S(cy + s)], radius=S(size*0.12),
                        outline=color, width=S(lw))
    for k in range(1, 4):
        xx = cx - s + (2 * s) * k / 4
        d.line([S(xx), S(cy - s * 0.7), S(xx), S(cy + s * 0.7)], fill=color, width=S(lw))


def i_drop(d, cx, cy, size, color, lw=2):
    r = size * 0.34
    d.ellipse([S(cx - r), S(cy - r + size*0.12), S(cx + r), S(cy + r + size*0.12)],
              outline=color, width=S(lw))
    d.polygon([ (S(cx), S(cy - size*0.5)),
                (S(cx - r*0.9), S(cy)), (S(cx + r*0.9), S(cy)) ], fill=color)


def i_speaker(d, cx, cy, size, color, lw=2):
    s = size * 0.42
    d.polygon([(S(cx - s), S(cy - s*0.45)), (S(cx - s*0.3), S(cy - s*0.45)),
               (S(cx + s*0.2), S(cy - s)), (S(cx + s*0.2), S(cy + s)),
               (S(cx - s*0.3), S(cy + s*0.45)), (S(cx - s), S(cy + s*0.45))], fill=color)
    for rr in (s*0.7, s*1.15):
        d.arc([S(cx + s*0.2 - rr), S(cy - rr), S(cx + s*0.2 + rr), S(cy + rr)],
              300, 60, fill=color, width=S(lw))


def toggle(d, x, y, ww, hh, on):
    col = GREEN if on else TRACK
    rrect(d, x, y, ww, hh, hh / 2, fill=col)
    knob = hh - 4
    kx = x + ww - knob - 2 if on else x + 2
    d.ellipse([S(kx), S(y + 2), S(kx + knob), S(y + 2 + knob)], fill=TEXT)


def ring(d, cx, cy, r, frac, color, lw=4, track=TRACK):
    bb = [S(cx - r), S(cy - r), S(cx + r), S(cy + r)]
    d.arc(bb, 0, 360, fill=track, width=S(lw))
    d.arc(bb, -90, -90 + int(360 * frac), fill=color, width=S(lw))


def status_bar(d, clock="14:30", wifi=True, mqtt=True):
    text(d, 10, 8, clock, fb(11), TEXT)
    i_wifi(d, 210, 15, 9, ACCENT if wifi else MUTED, 2)
    r = 3
    d.ellipse([S(226 - r), S(11 - r), S(226 + r), S(11 + r)],
              fill=GREEN if mqtt else RED)


# ---------------------------------------------------------------------------
# Screen A: Splash / Boot
# ---------------------------------------------------------------------------
def screen_splash():
    img = gradient_bg()
    d = ImageDraw.Draw(img)
    # Glow-Kreis
    for rr, a in ((46, CARD), (34, CARD_HI)):
        d.ellipse([S(120 - rr), S(96 - rr), S(120 + rr), S(96 + rr)], fill=a)
    i_wifi(d, 120, 108, 30, ACCENT, 3)
    ctext(d, 120, 158, "SmartPanel", fb(22), TEXT)
    ctext(d, 120, 190, "ESP8266  ·  ILI9341", fr(11), MUTED)

    # Fortschrittsbalken
    bx, bw2, by = 40, 160, 240
    rrect(d, bx, by, bw2, 8, 4, fill=TRACK)
    rrect(d, bx, by, bw2 * 0.72, 8, 4, fill=ACCENT)
    ctext(d, 120, 258, "Verbinde mit WLAN …", fr(11), MUTED)
    ctext(d, 120, 292, "192.168.1.42", fb(11), GREEN)
    return img


# ---------------------------------------------------------------------------
# Screen B: Steuerung (Redesign von paintScreen)
# ---------------------------------------------------------------------------
def screen_control():
    img = gradient_bg()
    d = ImageDraw.Draw(img)
    status_bar(d)

    text(d, 14, 32, "Wohnzimmer", fb(18), TEXT)
    # Seiten-Punkte (1/3)
    for k in range(3):
        cx = 200 + k * 12
        col = ACCENT if k == 0 else STROKE
        r = 3
        d.ellipse([S(cx - r), S(62 - r), S(cx + r), S(62 + r)], fill=col)

    devices = [
        ("Licht",     "Decke",   i_bulb,  AMBER,  True),
        ("Steckdose", "TV",      i_plug,  ACCENT, False),
        ("Rollo",     "Fenster", i_blinds,VIOLET, False),
        ("Heizung",   "WZ",      i_heat,  RED,    True),
    ]
    y = 76
    ch = 50
    for name, room, icon, col, on in devices:
        rrect(d, 12, y, 216, ch, 12, fill=CARD, outline=STROKE, width=1)
        # Icon-Kachel
        rrect(d, 22, y + 9, 32, 32, 9, fill=CARD_HI)
        icon(d, 38, y + 25, 16, col, 2)
        text(d, 66, y + 12, name, fb(13), TEXT)
        text(d, 66, y + 30, room, fr(10), MUTED)
        toggle(d, 176, y + 16, 40, 20, on)
        y += ch + 8
    return img


# ---------------------------------------------------------------------------
# Screen C: Dashboard (Redesign von paintSleep)
# ---------------------------------------------------------------------------
def temp_card(d, x, y, ww, hh, title, temp, hum, color):
    rrect(d, x, y, ww, hh, 12, fill=CARD, outline=STROKE, width=1)
    text(d, x + 12, y + 10, title, fr(10), MUTED)
    text(d, x + 12, y + 24, temp, fb(26), color)
    text(d, x + 12, y + 60, "°C", fr(10), MUTED)
    i_drop(d, x + ww - 44, y + 68, 12, ACCENT, 2)
    text(d, x + ww - 30, y + 62, hum, fb(11), TEXT)


def sensor_tile(d, x, y, ww, hh, icon, color, value, unit, frac):
    rrect(d, x, y, ww, hh, 11, fill=CARD, outline=STROKE, width=1)
    ring(d, x + 22, y + hh / 2, 13, frac, color, 3)
    icon(d, x + 22, y + hh / 2, 12, color, 2)
    text(d, x + 42, y + 12, value, fb(14), TEXT)
    text(d, x + 42, y + 32, unit, fr(9), MUTED)


def i_co2(d, cx, cy, size, color, lw=2):
    d.text((S(cx), S(cy)), "CO", font=fb(9), fill=color, anchor="mm")


def screen_dashboard():
    img = gradient_bg()
    d = ImageDraw.Draw(img)
    status_bar(d)

    # Uhr-Hero
    ctext(d, 120, 30, "14:30", fb(40), TEXT)
    ctext(d, 120, 84, "Montag, 21. Juli 2026", fr(11), MUTED)

    # Zwei Temperatur-Karten
    temp_card(d, 12, 108, 105, 82, "INNEN",  "22.5", "45%", GREEN)
    temp_card(d, 123, 108, 105, 82, "AUSSEN", "18.2", "60%", RED)

    # Sensor-Kacheln
    ty = 200
    sensor_tile(d, 12, ty, 105, 44, i_co2,     AMBER,  "620",  "ppm CO₂", 0.42)
    sensor_tile(d, 123, ty, 105, 44, i_speaker, VIOLET, "38",   "dB Lärm", 0.38)
    sensor_tile(d, 12, ty + 52, 105, 44, i_drop, ACCENT, "1013", "mbar",   0.65)
    sensor_tile(d, 123, ty + 52, 105, 44, i_bulb, GREEN,  "78",   "% Avocado", 0.78)
    return img


# ---------------------------------------------------------------------------
def compose():
    panels = [
        ("Splash / Boot", "setup()", screen_splash()),
        ("Steuerung", "paintScreen()", screen_control()),
        ("Dashboard", "paintSleep()", screen_dashboard()),
    ]
    # downsample einzelne Screens
    finals = [(lbl, fn, im.resize((W, H), Image.LANCZOS)) for lbl, fn, im in panels]

    pad, title_h, label_h, cap_h = 34, 66, 34, 30
    n = len(finals)
    tf = ImageFont.truetype(FB, 28)
    t = "Design-Vorschlag  ·  elegantere TFT-Screens (240×320)"
    grid_w = n * W + (n + 1) * pad
    _tmp = ImageDraw.Draw(Image.new("RGB", (1, 1)))
    title_w = _tmp.textlength(t, font=tf)
    cw = int(max(grid_w, title_w + 2 * pad))
    x0 = (cw - grid_w) // 2   # Panelraster zentrieren, falls Titel breiter
    ch = title_h + label_h + H + cap_h + pad
    canvas = Image.new("RGB", (cw, ch), (243, 244, 247))
    d = ImageDraw.Draw(canvas)

    d.text(((cw - title_w) / 2, 18), t, font=tf, fill=(22, 24, 34))

    lf = ImageFont.truetype(FB, 19)
    cf = ImageFont.truetype(FR, 15)
    for i, (lbl, fn, im) in enumerate(finals):
        x = x0 + pad + i * (W + pad)
        y = title_h + label_h
        lw2 = d.textlength(lbl, font=lf)
        d.text((x + (W - lw2) / 2, title_h + 4), lbl, font=lf, fill=(22, 24, 34))
        d.rectangle([x - 2, y - 2, x + W + 1, y + H + 1], outline=(70, 78, 96), width=2)
        canvas.paste(im, (x, y))
        cwid = d.textlength(fn, font=cf)
        d.text((x + (W - cwid) / 2, y + H + 6), fn, font=cf, fill=(120, 126, 140))

    import os
    os.makedirs("docs", exist_ok=True)
    out = "docs/screens_proposal.png"
    canvas.save(out)
    print("geschrieben:", out, canvas.size)


if __name__ == "__main__":
    compose()
