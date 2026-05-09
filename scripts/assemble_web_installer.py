#!/usr/bin/env python3
import argparse
import json
import shutil
from pathlib import Path


def find_boot_app0() -> Path:
    candidates = list(
        Path.home().glob(".platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin")
    )
    if not candidates:
        raise FileNotFoundError("Could not find boot_app0.bin in the PlatformIO Arduino ESP32 package")
    return candidates[0]


def copy_required(src: Path, dst: Path) -> None:
    if not src.exists():
        raise FileNotFoundError(f"Missing build artifact: {src}")
    shutil.copy2(src, dst)


def main() -> None:
    parser = argparse.ArgumentParser(description="Assemble ESP Web Tools firmware files.")
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--site-dir", required=True, type=Path)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()

    firmware_dir = args.site_dir / "firmware"
    firmware_dir.mkdir(parents=True, exist_ok=True)

    copy_required(args.build_dir / "bootloader.bin", firmware_dir / "bootloader.bin")
    copy_required(args.build_dir / "partitions.bin", firmware_dir / "partitions.bin")
    copy_required(args.build_dir / "firmware.bin", firmware_dir / "firmware.bin")
    copy_required(find_boot_app0(), firmware_dir / "boot_app0.bin")

    manifest = {
        "name": "ESP32 EVCC Switch CYD",
        "version": args.version,
        "builds": [
            {
                "chipFamily": "ESP32",
                "parts": [
                    {"path": "bootloader.bin", "offset": 0x1000},
                    {"path": "partitions.bin", "offset": 0x8000},
                    {"path": "boot_app0.bin", "offset": 0xE000},
                    {"path": "firmware.bin", "offset": 0x10000},
                ],
            }
        ],
    }

    (firmware_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
