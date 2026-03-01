#!/usr/bin/env python3
from __future__ import annotations

import unittest

from check_config_schema_version import extract_doc_version, extract_settings_version


class ConfigSchemaVersionTests(unittest.TestCase):
    def test_extract_settings_version(self) -> None:
        text = "class Settings { public: static constexpr uint8_t VERSION = 0x15; };"
        self.assertEqual(extract_settings_version(text), "0x15")

    def test_extract_doc_version(self) -> None:
        text = "# Config Schema\nSchema version: `0x15` (`Settings::VERSION`).\n"
        self.assertEqual(extract_doc_version(text), "0x15")

    def test_mismatch_can_be_detected_by_caller(self) -> None:
        s = extract_settings_version("VERSION = 0x16")
        d = extract_doc_version("Schema version: `0x15`")
        self.assertNotEqual(s, d)


if __name__ == "__main__":
    unittest.main()
