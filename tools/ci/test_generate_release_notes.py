#!/usr/bin/env python3
from __future__ import annotations

import unittest

from generate_release_notes import extract_section, resolve_version


class GenerateReleaseNotesTests(unittest.TestCase):
    def test_extract_section(self) -> None:
        text = (
            "# Changelog\n\n"
            "## [0.1.0] - 2026-03-01\n"
            "### Added\n"
            "- One\n\n"
            "## [0.0.9] - 2026-02-01\n"
            "### Fixed\n"
            "- Two\n"
        )
        sec = extract_section(text, "0.1.0")
        self.assertIn("### Added", sec)
        self.assertIn("- One", sec)
        self.assertNotIn("0.0.9", sec)

    def test_extract_section_missing_raises(self) -> None:
        with self.assertRaises(ValueError):
            extract_section("## [0.0.1] - 2026-01-01\n- x\n", "0.1.0")

    def test_resolve_version_from_tag_or_firmware(self) -> None:
        main = 'constexpr char kFirmwareVersion[] = "1.2.3";'
        self.assertEqual(resolve_version("v2.0.0", main), "2.0.0")
        self.assertEqual(resolve_version("", main), "1.2.3")


if __name__ == "__main__":
    unittest.main()
