#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SETTINGS_PATH = ROOT / "boatlock" / "Settings.h"
SCHEMA_DOC_PATH = ROOT / "docs" / "CONFIG_SCHEMA.md"


def extract_settings_version(text: str) -> str:
    m = re.search(r"VERSION\s*=\s*(0x[0-9A-Fa-f]+)", text)
    if not m:
        raise ValueError("Settings::VERSION not found")
    return m.group(1).lower()


def extract_doc_version(text: str) -> str:
    m = re.search(r"Schema version:\s*`(0x[0-9A-Fa-f]+)`", text)
    if not m:
        raise ValueError("Schema version not found in docs/CONFIG_SCHEMA.md")
    return m.group(1).lower()


def main() -> int:
    settings_text = SETTINGS_PATH.read_text(encoding="utf-8")
    doc_text = SCHEMA_DOC_PATH.read_text(encoding="utf-8")

    settings_version = extract_settings_version(settings_text)
    doc_version = extract_doc_version(doc_text)

    if settings_version != doc_version:
        print(
            f"config schema version mismatch: Settings.h={settings_version} docs={doc_version}",
            file=sys.stderr,
        )
        return 1

    print(f"config schema version OK: {settings_version}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
