# EVC32 EVCC Switch

Touch-Bedienteil fuer eine EVCC-gesteuerte Wallbox auf einem ESP32 Cheap Yellow Display.

## Was der erste Stand kann

- verbindet sich per WLAN mit dem Heimnetz
- wird nach dem Flashen ueber ein Setup-Portal konfiguriert
- liest EVCC per lokaler REST API
- nutzt genau einen konfigurierten EVCC-Loadpoint, z. B. die Garage
- zeigt Ladezustand, Modus, Ladeleistung, PV-Leistung und Netzleistung
- schaltet die EVCC-Modi `off`, `pv`, `minpv` und `now`

## Web-Installer

Dieses Projekt ist fuer GitHub Pages mit ESP Web Tools vorbereitet. Sobald die GitHub Actions auf `main` laufen, entsteht aus `site/` eine kleine Installer-Seite. Nutzer koennen dort in Chrome oder Edge den ESP32 direkt ueber USB flashen.

Nach dem ersten Start oeffnet das Geraet ein WLAN `EVCC-Switch` mit dem Passwort `evccswitch`. Dort werden WLAN, EVCC-IP, Port und Loadpoint-ID eingetragen.

Repository: <https://github.com/tobymarks/evc32-evcc-switch>

## Setup

Lokale Entwicklung:

1. Optional `include/Config.example.h` nach `include/Config.h` kopieren und lokale Defaults eintragen.
2. Firmware mit PlatformIO bauen und auf den CYD flashen:

```bash
pio run -e cyd -t upload
```

3. Beim ersten Start das Setup-Portal verwenden.

EVCC zaehlt Ladepunkte ab `1`; wenn die Garage in EVCC der zweite Ladepunkt ist, im Portal `2` eintragen.

Zum Zuruecksetzen beim Einschalten die BOOT-Taste gedrueckt halten.

## EVCC-Endpunkte

- Status: `GET /api/state`
- Modus setzen: `POST /api/loadpoints/<id>/mode/<mode>`

Der Code akzeptiert das alte EVCC-Antwortformat mit `result`-Wrapper und das neue Format ab EVCC v0.207 ohne diese Ebene.

## Projektplan

Die Details stehen in [docs/architecture.md](docs/architecture.md).

## Lizenz

MIT, siehe [LICENSE](LICENSE).
