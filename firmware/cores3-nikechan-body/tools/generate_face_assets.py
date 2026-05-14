#!/usr/bin/env python3
"""Generate the CoreS3 face image header from 320x240 JPEG assets."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ASSET_DIR = ROOT / "assets" / "face"
OUT = ROOT / "src" / "assets" / "face_images.h"

ASSETS = [
    ("kFaceEyeOnMouthOn", "eye_on_mouth_on.jpg"),
    ("kFaceEyeOnMouthOff", "eye_on_mouth_off.jpg"),
    ("kFaceEyeOffMouthOff", "eye_off_mouth_off.jpg"),
    ("kFaceEyeOffMouthOn", "eye_off_mouth_on.jpg"),
]


def main() -> None:
    lines = [
        "#pragma once",
        "",
        "#include <Arduino.h>",
        "#include <pgmspace.h>",
        "",
        "namespace face_assets {",
        "",
    ]

    for symbol, filename in ASSETS:
        data = (ASSET_DIR / filename).read_bytes()
        lines.append(f"constexpr size_t {symbol}Size = {len(data)};")
        lines.append(f"const uint8_t {symbol}[] PROGMEM = {{")
        for i in range(0, len(data), 12):
            chunk = data[i:i + 12]
            lines.append("  " + ", ".join(f"0x{byte:02X}" for byte in chunk) + ",")
        lines.append("};")
        lines.append("")

    lines.append("}  // namespace face_assets")
    lines.append("")
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text("\n".join(lines), encoding="ascii")
    print(OUT)


if __name__ == "__main__":
    main()
