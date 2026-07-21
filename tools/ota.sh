#!/usr/bin/env bash
#
# OTA-Flash-Helfer fuer WiFiClient_ESP.
#
# Das Passwort steht NICHT in diesem Skript. Quelle (in dieser Reihenfolge):
#   1. Umgebungsvariable OTA_PASSWORD
#   2. SECRET_OTA_PASSWORD aus src/secrets.h (gitignoriert)
# Der Wert wird nicht ausgegeben.
#
# Aufruf:
#   ./tools/ota.sh                 # Ziel: esp-wificlient.local
#   ./tools/ota.sh 192.168.20.242  # optional: Host/IP ueberschreiben
#
set -euo pipefail

cd "$(dirname "$0")/.."

# Passwort ermitteln (ohne es anzuzeigen)
PW="${OTA_PASSWORD:-}"
if [ -z "$PW" ] && [ -f src/secrets.h ]; then
  PW="$(sed -n 's/.*SECRET_OTA_PASSWORD[[:space:]]*"\([^"]*\)".*/\1/p' src/secrets.h)"
fi
[ -n "$PW" ] || { echo "FEHLER: Kein OTA_PASSWORD gesetzt und keins in src/secrets.h gefunden"; exit 1; }

HOST="${1:-esp-wificlient.local}"
ENV="nodemcuv2_ota"
BIN=".pio/build/${ENV}/firmware.bin"
PIO="${PIO:-$HOME/.platformio/penv/bin/pio}"

echo "==> Baue ${ENV} ..."
"$PIO" run -e "$ENV"

ESPOTA="$(find "$HOME/.platformio/packages" -name espota.py 2>/dev/null | head -1)"
[ -n "$ESPOTA" ] || { echo "FEHLER: espota.py nicht gefunden"; exit 1; }

echo "==> OTA-Upload an ${HOST}:8266 ..."
python3 "$ESPOTA" -i "$HOST" -p 8266 -a "$PW" -f "$BIN" -r

echo "==> fertig."
