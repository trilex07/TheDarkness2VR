#!/usr/bin/env python3
"""Classify Helix Darkness II shader overrides by pass type."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "extracted_helix_3dvis" / "ShaderOverride"


def classify(text: str) -> str:
    t = text.lower()
    if "inverse" in t and "shadow" in t:
        return "SHADOW"
    if "inverseviewprojection" in t or "inverseprojection" in t:
        return "DEFERRED_RECON"
    if "worldviewprojection" in t:
        if "projector" in t or "decal" in t:
            return "PROJECTOR"
        return "WORLD"
    if "position" in t and "texcoord" in t and "worldviewprojection" not in t:
        return "UI_OR_SCREEN"
    return "UNKNOWN"


def main():
    rows = []
    for path in sorted(ROOT.rglob("*.txt")):
        text = path.read_text(errors="replace")
        stage = "VS" if "Vertex" in str(path.parent) else "PS"
        rows.append((path.stem, stage, classify(text), "WVP" in text, "Inverse" in text))
    print("hash\tstage\tpass\twvp\tinverse")
    for r in rows:
        print("\t".join(map(str, r)))


if __name__ == "__main__":
    main()
