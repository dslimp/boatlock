#!/usr/bin/env python3
import argparse
import hashlib
import json
import urllib.parse
import urllib.request


def fetch_bytes(url: str) -> bytes:
    with urllib.request.urlopen(url, timeout=30) as response:
        status = getattr(response, "status", 200)
        if status is not None and status != 200:
            raise SystemExit(f"unexpected HTTP status {status} for {url}")
        return response.read()


def fetch_text(url: str) -> str:
    return fetch_bytes(url).decode("utf-8")


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def require_equal(name: str, actual, expected) -> None:
    if actual != expected:
        raise SystemExit(f"{name} mismatch: expected {expected!r}, got {actual!r}")


def sibling_url(manifest_url: str, filename: str) -> str:
    return urllib.parse.urljoin(manifest_url, filename)


def verify(args: argparse.Namespace) -> None:
    manifest = json.loads(fetch_text(args.manifest_url))
    require_equal("schema", manifest.get("schema"), 1)
    require_equal("channel", manifest.get("channel"), "main")
    require_equal("repo", manifest.get("repo"), args.expected_repo)
    require_equal("branch", manifest.get("branch"), "main")
    require_equal("platformioEnv", manifest.get("platformioEnv"), "esp32s3")
    require_equal("commandProfile", manifest.get("commandProfile"), "release")
    require_equal("artifactName", manifest.get("artifactName"), "firmware-esp32s3")
    if args.expected_git_sha:
        require_equal("gitSha", manifest.get("gitSha"), args.expected_git_sha)
    if args.expected_workflow_run_id is not None:
        require_equal(
            "workflowRunId",
            manifest.get("workflowRunId"),
            args.expected_workflow_run_id,
        )

    binary_url = manifest.get("binaryUrl")
    if not isinstance(binary_url, str) or not binary_url:
        raise SystemExit("manifest binaryUrl missing")
    firmware = fetch_bytes(binary_url)
    require_equal("size", len(firmware), manifest.get("size"))
    firmware_sha = sha256_hex(firmware)
    require_equal("sha256", firmware_sha, manifest.get("sha256"))

    checksums = fetch_text(sibling_url(args.manifest_url, "SHA256SUMS"))
    checksum_line = f"{firmware_sha}  firmware.bin"
    if checksum_line not in checksums:
        raise SystemExit("SHA256SUMS does not contain matching firmware.bin hash")

    build_info = fetch_text(sibling_url(args.manifest_url, "BUILD_INFO.txt"))
    for line in (
        f"firmware_sha256={firmware_sha}",
        "artifact_name=firmware-esp32s3",
        "platformio_env=esp32s3",
        "command_profile=release",
    ):
        if line not in build_info:
            raise SystemExit(f"BUILD_INFO.txt missing {line}")


def main() -> None:
    ap = argparse.ArgumentParser(description="Verify BoatLock firmware update channel")
    ap.add_argument("--manifest-url", required=True)
    ap.add_argument("--expected-repo", default="dslimp/boatlock")
    ap.add_argument("--expected-git-sha", default=None)
    ap.add_argument("--expected-workflow-run-id", type=int, default=None)
    args = ap.parse_args()
    verify(args)


if __name__ == "__main__":
    main()
