#!/usr/bin/env python3
"""校验发布 tag 与 keyboard.json 的产品版本一致。

接受 `v<version>` 或 `v<version>-<prerelease>`，例如 `v1.0.2`、
`v1.0.2-test.1`。tag 与版本不匹配时退出非零，避免发错版本。
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
KEYBOARD_JSON = REPO_ROOT / "keyboards/royalkludge/rk98pro_us_triple_mode/keyboard.json"
PRERELEASE_RE = re.compile(r"^[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*$")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", required=True, help="Git tag，例如 v1.0.2 或 v1.0.2-test.1")
    return parser.parse_args()


def read_product_version() -> str:
    with KEYBOARD_JSON.open(encoding="utf-8") as handle:
        data = json.load(handle)
    version = data.get("version")
    if not isinstance(version, str) or not version:
        raise ValueError(f"{KEYBOARD_JSON} 缺少有效的 version")
    return version


def tag_matches(tag: str, version: str) -> bool:
    if not tag.startswith("v"):
        return False
    suffix = tag[1:]
    if suffix == version:
        return True
    prefix = f"{version}-"
    if not suffix.startswith(prefix):
        return False
    prerelease = suffix[len(prefix) :]
    return bool(prerelease and PRERELEASE_RE.fullmatch(prerelease))


def main() -> int:
    args = parse_args()
    try:
        version = read_product_version()
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"读取产品版本失败：{error}", file=sys.stderr)
        return 1

    if not tag_matches(args.tag, version):
        print(
            f"tag {args.tag!r} 与产品版本 {version!r} 不一致；"
            f"应为 v{version} 或 v{version}-<prerelease>",
            file=sys.stderr,
        )
        return 1

    print(f"tag {args.tag} 与产品版本 {version} 一致")
    return 0


if __name__ == "__main__":
    sys.exit(main())
