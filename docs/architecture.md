# ESP32 EVCC Switch - Architektur

## Ziel

Ein ESP32 Cheap Yellow Display (CYD / ESP32-2432S028R) sitzt neben der Garagen-Wallbox und steuert genau diesen EVCC-Ladepunkt. Das Display zeigt den aktuellen Ladezustand und bietet eine direkte Umschaltung der EVCC-Lademodi.

## EVCC-Anbindung

Der erste Stand nutzt die EVCC REST API:

- Status lesen: `GET http://<evcc-host>:7070/api/state`
- Modus setzen: `POST http://<evcc-host>:7070/api/loadpoints/<id>/mode/<mode>`

Die EVCC-Dokumentation nennt die Modi:

- `off`: Laden aus
- `pv`: nur PV-Ueberschuss
- `minpv`: sofort mit Mindestleistung, bei PV-Ueberschuss schneller
- `now`: sofort mit maximal moeglicher Leistung

Der Code verarbeitet sowohl das alte EVCC-JSON mit aeusserem `result`-Objekt als auch das neuere Format ohne diese Ebene.

MQTT bleibt eine gute spaetere Option, falls ohnehin ein Broker laeuft. Fuer den Start ist REST einfacher, weil der ESP32 nur EVCC erreichen muss.

## Erstkonfiguration

Die veroeffentlichte Firmware enthaelt keine privaten Netzwerkdaten. Beim ersten Start nutzt sie WiFiManager:

- Access Point: `EVCC-Switch`
- Passwort: `evccswitch`
- Eingaben: WLAN, EVCC Host/IP, Port, Loadpoint-ID, Display-Label

Die Werte werden im ESP32-NVS gespeichert. Zum Zuruecksetzen beim Einschalten die BOOT-Taste gedrueckt halten.

## Bedienkonzept

Startbildschirm:

- Titel des Ladepunkts, z. B. "Garage"
- WLAN-/EVCC-Verbindungsstatus
- aktueller EVCC-Modus
- Fahrzeug verbunden / laedt
- Ladeleistung, Netzbezug/Einspeisung, PV-Leistung
- drei grosse Touch-Flaechen: Aus, Standard, Schnell

Das CYD wird hochkant mit USB-Anschluss unten montiert. Die Firmware nutzt dafuer standardmaessig `DISPLAY_ROTATION=0` und `TOUCH_ROTATION=0`. Falls eine CYD-Revision anders ausgerichtet ist, koennen diese Defines in `platformio.ini` oder einer lokalen `Config.h` angepasst werden.

`Standard` ist ein konfigurierbarer EVCC-Modus und kann `minpv` oder `pv` sein. Nach dem Abstecken setzt die Firmware auf diesen Standardmodus zurueck, wenn die Session vorher auf `off` oder `now` umgestellt wurde.

## Technische Basis

- PlatformIO
- Arduino Framework fuer ESP32
- TFT_eSPI fuer das ILI9341 Display
- XPT2046_Touchscreen fuer Touch
- ArduinoJson fuer `/api/state`
- WiFiManager fuer das Setup-Portal

## CYD-Pins

Display:

- MISO GPIO 12
- MOSI GPIO 13
- SCLK GPIO 14
- CS GPIO 15
- DC GPIO 2
- RST -1
- Backlight GPIO 21

Touch:

- IRQ GPIO 36
- MOSI GPIO 32
- MISO GPIO 39
- CLK GPIO 25
- CS GPIO 33

## Naechste Ausbaustufen

- Loadpoint automatisch anhand des Titels "Garage" finden
- WiFi-/EVCC-Konfiguration ueber Captive Portal statt Header-Datei
- Display nachts dimmen oder per LDR automatisch regeln
- MQTT-Variante fuer schnellere Live-Updates
- optional: PIN/Long-Press fuer "Schnell", damit man nicht versehentlich Netzladen startet
