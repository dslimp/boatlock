#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

from check_firmware_version import extract_firmware_version, normalize_tag


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CHANGELOG = ROOT / "CHANGELOG.md"
DEFAULT_MAIN_CPP = ROOT / "boatlock" / "main.cpp"
DEFAULT_OUT = ROOT / "release-notes.md"


def extract_section(changelog_text: str, version: str) -> str:
    escaped = re.escape(version)
    pattern = re.compile(
        rf"^##\s+\[{escaped}\]\s*-\s*[^\n]+\n(.*?)(?=^##\s+\[|\Z)",
        re.MULTILINE | re.DOTALL,
    )
    m = pattern.search(changelog_text)
    if not m:
        raise ValueError(f"version section [{version}] not found in changelog")
    section = m.group(1).strip()
    if not section:
        raise ValueError(f"version section [{version}] is empty")
    return section


def resolve_version(tag: str, main_cpp_text: str) -> str:
    if tag:
        return normalize_tag(tag)
    return extract_firmware_version(main_cpp_text)


def main() -> int:
    ap = argparse.ArgumentParser(description="Generate release-notes.md from CHANGELOG.md")
    ap.add_argument("--tag", type=str, default="", help="Release tag (vX.Y.Z)")
    ap.add_argument("--changelog", type=str, default=str(DEFAULT_CHANGELOG), help="Path to CHANGELOG.md")
    ap.add_argument("--main-cpp", type=str, default=str(DEFAULT_MAIN_CPP), help="Path to firmware main.cpp")
    ap.add_argument("--out", type=str, default=str(DEFAULT_OUT), help="Output path for release notes")
    args = ap.parse_args()

    changelog_path = Path(args.changelog)
    main_cpp_path = Path(args.main_cpp)
    out_path = Path(args.out)

    changelog_text = changelog_path.read_text(encoding="utf-8")
    main_cpp_text = main_cpp_path.read_text(encoding="utf-8")
    version = resolve_version(args.tag, main_cpp_text)
    section = extract_section(changelog_text, version)

    body = f"# Release v{version}\n\n{section.strip()}\n"
    out_path.write_text(body, encoding="utf-8")
    print(f"generated release notes: {out_path}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ValueError as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
