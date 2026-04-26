#!/usr/bin/env python3
import argparse
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path


def sha256_hex(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def firmware_version() -> str:
    from check_firmware_version import extract_firmware_version

    return extract_firmware_version(Path("boatlock/main.cpp").read_text())


def build_manifest(args: argparse.Namespace) -> dict:
    firmware = args.firmware
    if not firmware.is_file():
        raise SystemExit(f"firmware binary not found: {firmware}")
    base_url = args.base_url.rstrip("/")
    built_at = args.built_at or datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")
    return {
        "schema": 1,
        "channel": args.channel,
        "repo": args.repo,
        "branch": args.branch,
        "gitSha": args.git_sha,
        "workflowRunId": args.workflow_run_id,
        "firmwareVersion": args.firmware_version or firmware_version(),
        "platformioEnv": args.platformio_env,
        "commandProfile": args.command_profile,
        "artifactName": args.artifact_name,
        "binaryUrl": f"{base_url}/{args.binary_filename}",
        "size": firmware.stat().st_size,
        "sha256": sha256_hex(firmware),
        "builtAt": built_at,
    }


def main() -> None:
    ap = argparse.ArgumentParser(description="Generate BoatLock firmware update manifest")
    ap.add_argument("--firmware", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--base-url", required=True)
    ap.add_argument("--binary-filename", default="firmware.bin")
    ap.add_argument("--repo", default="dslimp/boatlock")
    ap.add_argument("--branch", default="main")
    ap.add_argument("--channel", default="main")
    ap.add_argument("--git-sha", required=True)
    ap.add_argument("--workflow-run-id", type=int, default=None)
    ap.add_argument("--firmware-version", default=None)
    ap.add_argument("--platformio-env", default="esp32s3_service")
    ap.add_argument("--command-profile", default="service")
    ap.add_argument("--artifact-name", default="firmware-esp32s3-service")
    ap.add_argument("--built-at", default=None)
    args = ap.parse_args()

    manifest = build_manifest(args)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
