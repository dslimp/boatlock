#!/usr/bin/env python3
from __future__ import annotations

import unittest

from check_firmware_version import (
    extract_firmware_version,
    is_semver,
    normalize_tag,
)


class FirmwareVersionTests(unittest.TestCase):
    def test_extract_firmware_version(self) -> None:
        text = 'constexpr char kFirmwareVersion[] = "1.2.3";'
        self.assertEqual(extract_firmware_version(text), "1.2.3")

    def test_is_semver(self) -> None:
        self.assertTrue(is_semver("0.1.0"))
        self.assertTrue(is_semver("10.20.30"))
        self.assertFalse(is_semver("1.0"))
        self.assertFalse(is_semver("v1.0.0"))
        self.assertFalse(is_semver("1.0.0-beta"))

    def test_normalize_tag(self) -> None:
        self.assertEqual(normalize_tag("v0.1.0"), "0.1.0")
        self.assertEqual(normalize_tag("refs/tags/v0.1.0"), "0.1.0")
        self.assertEqual(normalize_tag("0.1.0"), "0.1.0")


if __name__ == "__main__":
    unittest.main()
