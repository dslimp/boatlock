#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
FIRMWARE_MAIN = ROOT / "boatlock" / "main.cpp"

SEMVER_RE = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$")


def extract_firmware_version(text: str) -> str:
    m = re.search(r'kFirmwareVersion\[\]\s*=\s*"([^"]+)"', text)
    if not m:
        raise ValueError("kFirmwareVersion not found in boatlock/main.cpp")
    return m.group(1).strip()


def is_semver(version: str) -> bool:
    return SEMVER_RE.fullmatch(version) is not None


def normalize_tag(tag: str) -> str:
    t = tag.strip()
    if t.startswith("refs/tags/"):
        t = t[len("refs/tags/") :]
    if t.startswith("v"):
        t = t[1:]
    return t


def main() -> int:
    ap = argparse.ArgumentParser(description="Validate firmware semantic version")
    ap.add_argument("--tag", type=str, default="", help="Optional release tag (vX.Y.Z)")
    ap.add_argument(
        "--print-only",
        action="store_true",
        help="Print version and exit without validation against tag",
    )
    args = ap.parse_args()

    text = FIRMWARE_MAIN.read_text(encoding="utf-8")
    version = extract_firmware_version(text)
    if not is_semver(version):
        print(f"firmware version is not semver: '{version}'", file=sys.stderr)
        return 1

    if args.print_only:
        print(version)
        return 0

    if args.tag:
        tag_version = normalize_tag(args.tag)
        if not is_semver(tag_version):
            print(f"tag is not semver-compatible: '{args.tag}'", file=sys.stderr)
            return 1
        if tag_version != version:
            print(
                f"firmware/tag mismatch: firmware={version} tag={tag_version}",
                file=sys.stderr,
            )
            return 1

    print(f"firmware version OK: {version}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
