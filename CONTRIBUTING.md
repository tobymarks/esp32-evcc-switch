# Contributing

Thanks for helping improve ESP32 EVCC Switch.

## Development Setup

1. Install PlatformIO.
2. Clone the repository.
3. Build the firmware:

```bash
pio run -e cyd
```

Optional local defaults can be placed in `include/Config.h`. That file is ignored by git.

## Pull Requests

- Keep changes focused.
- Run `pio run -e cyd` before opening a pull request.
- Update `README.md` or `docs/` when behavior changes.
- Do not commit Wi-Fi credentials, IPs from private networks, generated `.pio` files or local editor metadata.

## Versioning

Release tags use `vMAJOR.MINOR.PATCH`, for example `v0.1.0`.
