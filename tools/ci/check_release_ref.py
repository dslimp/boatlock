#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import subprocess
import sys


TAG_RE = re.compile(r"^v(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$")


def expected_branch(tag: str) -> str:
    match = TAG_RE.fullmatch(tag.strip())
    if match is None:
        raise ValueError(f"release tag must look like vX.Y.Z: {tag!r}")
    major, minor, _patch = match.groups()
    return f"release/v{major}.{minor}.x"


def run_git(args: list[str], *, check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", *args],
        check=check,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def require_head_on_release_branch(branch: str) -> None:
    remote_ref = f"origin/{branch}"
    fetch = run_git(
        ["fetch", "--no-tags", "origin", f"{branch}:refs/remotes/{remote_ref}"],
        check=False,
    )
    if fetch.returncode != 0:
        print(
            f"could not fetch required release branch {remote_ref}: {fetch.stderr.strip()}",
            file=sys.stderr,
        )
        raise SystemExit(1)

    contains = run_git(
        ["merge-base", "--is-ancestor", "HEAD", remote_ref],
        check=False,
    )
    if contains.returncode != 0:
        print(
            f"tag commit is not contained in required release branch {remote_ref}",
            file=sys.stderr,
        )
        raise SystemExit(1)


def main() -> int:
    ap = argparse.ArgumentParser(description="Validate BoatLock release branch refs")
    ap.add_argument("--tag", required=True, help="Release tag, for example v0.2.1")
    ap.add_argument(
        "--print-branch",
        action="store_true",
        help="Print the expected release branch and exit",
    )
    ap.add_argument(
        "--require-current-commit",
        action="store_true",
        help="Require HEAD to be contained in origin/release/vX.Y.x",
    )
    args = ap.parse_args()

    try:
        branch = expected_branch(args.tag)
    except ValueError as exc:
        print(str(exc), file=sys.stderr)
        return 1

    if args.print_branch:
        print(branch)

    if args.require_current_commit:
        require_head_on_release_branch(branch)

    if not args.print_branch:
        print(f"release branch OK: {branch}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
