#!/usr/bin/env python3
"""Embed .mim layout specs into a C++ header as byte arrays."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from meta import library_version, load  # noqa: E402


def embed_file(name: str, path: Path) -> tuple[str, int]:
    data = path.read_bytes()
    lines = [f"static const unsigned char khipro_spec_{name}[] = {{"]
    row: list[str] = []
    for byte in data:
        row.append(f"0x{byte:02x}")
        if len(row) == 16:
            lines.append("    " + ", ".join(row) + ",")
            row = []
    if row:
        lines.append("    " + ", ".join(row) + ",")
    lines.append("};")
    lines.append(f"static const unsigned int khipro_spec_{name}_len = {len(data)};")
    return "\n".join(lines), len(data)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--default", type=Path, required=True, help="bn-khipro.mim (default layout)")
    parser.add_argument("--touchscreen", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()

    meta = load()
    layout_version = meta["layout_version"]
    lib_version = library_version(meta)

    default_block, default_len = embed_file("default", args.default)
    touch_block, touch_len = embed_file("touchscreen", args.touchscreen)

    out = f"""#pragma once

#define KHIPRO_LAYOUT_VERSION "{layout_version}"
#define KHIPRO_LIBRARY_VERSION "{lib_version}"

{default_block}

{touch_block}
"""
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(out, encoding="utf-8")
    print(
        f"Wrote {args.out} (default={default_len} bytes, touchscreen={touch_len} bytes, "
        f"layout={layout_version}, library={lib_version})"
    )


if __name__ == "__main__":
    main()
