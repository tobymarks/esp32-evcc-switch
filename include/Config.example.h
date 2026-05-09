#pragma once

// Optional developer defaults:
// Copy this file to include/Config.h if you want local defaults during development.
// End users configure Wi-Fi and EVCC details in the setup portal after flashing.

#define WIFI_SSID "Dein WLAN"
#define WIFI_PASSWORD "Dein WLAN Passwort"

// EVCC base URL parts. Use the plain host or IP without http://.
#define EVCC_HOST "192.168.178.50"
#define EVCC_PORT 7070

// EVCC loadpoint IDs start at 1. Set this to the garage loadpoint.
#define EVCC_LOADPOINT_ID 1

// Optional label shown on the display.
#define WALLBOX_LABEL "Garage"
