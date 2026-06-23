#!/usr/bin/env python3
"""Read metadata from khipro.meta."""

from __future__ import annotations

from pathlib import Path

DEFAULT_PATH = Path(__file__).resolve().parent.parent.parent / "khipro.meta"


def load(path: Path = DEFAULT_PATH) -> dict:
    out: dict[str, str] = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, _, value = line.partition("=")
        out[key.strip()] = value.strip()
    return out


def library_version(meta: dict | None = None) -> str:
    m = meta or load()
    return f"{m['layout_version']}-{m['library_patch']}"


def set_fields(path: Path, updates: dict[str, str]) -> None:
    """In-place edit of one or more KEY=value lines, preserving comments and order."""
    lines = path.read_text(encoding="utf-8").splitlines()
    pending = dict(updates)
    for i, raw in enumerate(lines):
        stripped = raw.strip()
        if not stripped or stripped.startswith("#") or "=" not in stripped:
            continue
        key, _, _ = stripped.partition("=")
        key = key.strip()
        if key in pending:
            lines[i] = f"{key}={pending.pop(key)}"
    if pending:
        raise KeyError(f"Unknown metadata keys: {list(pending.keys())}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
