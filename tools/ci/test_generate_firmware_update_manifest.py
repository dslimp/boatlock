#!/usr/bin/env python3
import json
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class FirmwareUpdateManifestTests(unittest.TestCase):
    def test_generates_latest_main_service_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            tmp = Path(td)
            firmware = tmp / "firmware.bin"
            out = tmp / "manifest.json"
            firmware.write_bytes(b"boatlock-firmware")

            subprocess.run(
                [
                    "python3",
                    "tools/ci/generate_firmware_update_manifest.py",
                    "--firmware",
                    str(firmware),
                    "--out",
                    str(out),
                    "--base-url",
                    "https://dslimp.github.io/boatlock/firmware/main",
                    "--git-sha",
                    "abc1234def5678",
                    "--workflow-run-id",
                    "42",
                    "--firmware-version",
                    "1.2.3",
                    "--built-at",
                    "2026-04-26T12:00:00Z",
                ],
                cwd=ROOT,
                check=True,
            )

            manifest = json.loads(out.read_text())
            self.assertEqual(manifest["schema"], 1)
            self.assertEqual(manifest["channel"], "main")
            self.assertEqual(manifest["repo"], "dslimp/boatlock")
            self.assertEqual(manifest["branch"], "main")
            self.assertEqual(manifest["gitSha"], "abc1234def5678")
            self.assertEqual(manifest["workflowRunId"], 42)
            self.assertEqual(manifest["firmwareVersion"], "1.2.3")
            self.assertEqual(manifest["platformioEnv"], "esp32s3_service")
            self.assertEqual(manifest["commandProfile"], "service")
            self.assertEqual(manifest["artifactName"], "firmware-esp32s3-service")
            self.assertEqual(
                manifest["binaryUrl"],
                "https://dslimp.github.io/boatlock/firmware/main/firmware.bin",
            )
            self.assertEqual(manifest["size"], len(b"boatlock-firmware"))
            self.assertRegex(manifest["sha256"], r"^[0-9a-f]{64}$")
            self.assertEqual(manifest["builtAt"], "2026-04-26T12:00:00Z")


if __name__ == "__main__":
    unittest.main()
