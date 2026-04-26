#!/usr/bin/env python3
"""Smoke-test the FreeCAD MCP addon RPC endpoint used by the MCP server."""

from __future__ import annotations

import argparse
import sys
from xmlrpc.client import ServerProxy


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9875)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    url = f"http://{args.host}:{args.port}"
    server = ServerProxy(url, allow_none=True)

    if server.ping() is not True:
        print(f"FreeCAD RPC did not return ping=True at {url}", file=sys.stderr)
        return 1

    result = server.execute_code(
        "import FreeCAD\n"
        "print('FreeCAD version:', '.'.join(FreeCAD.Version()[:3]))\n"
        "print('Active document:', FreeCAD.ActiveDocument.Name if FreeCAD.ActiveDocument else '<none>')\n"
    )
    if not result.get("success"):
        print(result, file=sys.stderr)
        return 1

    print(f"FreeCAD RPC OK at {url}")
    print(result.get("message", "").strip())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
