#!/usr/bin/env python3
import json
import hashlib
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class FirmwareUpdateManifestTests(unittest.TestCase):
    def test_generates_release_manifest(self) -> None:
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
                    "https://github.com/dslimp/boatlock/releases/download/v1.2.3",
                    "--binary-filename",
                    "firmware-esp32s3.bin",
                    "--channel",
                    "release",
                    "--branch",
                    "release/v1.2.x",
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
            self.assertEqual(manifest["channel"], "release")
            self.assertEqual(manifest["repo"], "dslimp/boatlock")
            self.assertEqual(manifest["branch"], "release/v1.2.x")
            self.assertEqual(manifest["gitSha"], "abc1234def5678")
            self.assertEqual(manifest["workflowRunId"], 42)
            self.assertEqual(manifest["firmwareVersion"], "1.2.3")
            self.assertEqual(manifest["platformioEnv"], "esp32s3")
            self.assertEqual(manifest["commandProfile"], "release")
            self.assertEqual(manifest["artifactName"], "firmware-esp32s3")
            self.assertEqual(
                manifest["binaryUrl"],
                "https://github.com/dslimp/boatlock/releases/download/v1.2.3/firmware-esp32s3.bin",
            )
            self.assertEqual(manifest["size"], len(b"boatlock-firmware"))
            self.assertRegex(manifest["sha256"], r"^[0-9a-f]{64}$")
            self.assertEqual(manifest["builtAt"], "2026-04-26T12:00:00Z")

    def test_release_ref_prints_expected_branch(self) -> None:
        result = subprocess.run(
            [
                "python3",
                "tools/ci/check_release_ref.py",
                "--tag",
                "v1.2.3",
                "--print-branch",
            ],
            cwd=ROOT,
            check=True,
            stdout=subprocess.PIPE,
            text=True,
        )
        self.assertEqual(result.stdout.strip(), "release/v1.2.x")

    def test_verifies_published_channel_files(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            tmp = Path(td)
            firmware = tmp / "firmware.bin"
            manifest_path = tmp / "manifest.json"
            build_info = tmp / "BUILD_INFO.txt"
            checksums = tmp / "SHA256SUMS"
            firmware.write_bytes(b"boatlock-firmware")
            firmware_sha = hashlib.sha256(b"boatlock-firmware").hexdigest()

            subprocess.run(
                [
                    "python3",
                    "tools/ci/generate_firmware_update_manifest.py",
                    "--firmware",
                    str(firmware),
                    "--out",
                    str(manifest_path),
                    "--base-url",
                    tmp.as_uri(),
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
            build_info.write_text(
                "\n".join(
                    [
                        "firmware_sha256=" + firmware_sha,
                        "artifact_name=firmware-esp32s3",
                        "platformio_env=esp32s3",
                        "command_profile=release",
                    ]
                )
                + "\n"
            )
            checksums.write_text(f"{firmware_sha}  firmware.bin\n")

            subprocess.run(
                [
                    "python3",
                    "tools/ci/verify_firmware_update_channel.py",
                    "--manifest-url",
                    manifest_path.as_uri(),
                    "--expected-git-sha",
                    "abc1234def5678",
                    "--expected-workflow-run-id",
                    "42",
                ],
                cwd=ROOT,
                check=True,
            )


if __name__ == "__main__":
    unittest.main()
