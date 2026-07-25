#!/usr/bin/env python3
"""Generate src/Names/MoveNames.cpp -- move id -> display name -- from PKHeX's
Simplified Chinese move-name text resource.

Move id = array index, so PKHeX's flat one-name-per-line list maps directly (line 0
= move 0). PKHeX's "no move" sentinel (a run of em dashes) and any blank id are
emitted as "-" to match the app's empty-slot display. Names are taken verbatim
otherwise, so punctuation (hyphens, apostrophes) is the game-canonical form and
matches the other Names tables.

This replaces the older gen_movenames.sh, which CamelCase-split PKSE's own Move enum
and needed a hand-kept override map for punctuation (and still mis-split a few names
like "Roar of Time").

Regenerate:  python tools/gen_movenames.py
Pulls the PKHeX text resource it reads from GitHub on demand (tools/pkhex_source.py);
no local PKHeX checkout required.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pkhex_source import pkhex_path  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "src", "Names", "MoveNames.cpp")
SRC_REL = "Resources/text/other/zh-Hans/text_Moves_zh-Hans.txt"

EMDASH = "—"


def load_entries(path):
    with open(path, encoding="utf-8") as fh:
        lines = fh.read().split("\n")
    # Drop the single trailing empty element a final newline produces.
    if lines and lines[-1] == "":
        lines = lines[:-1]
    # PKHeX's "no move" sentinel is a run of em dashes; a blank is an unused id.
    return ["-" if (l == "" or all(c == EMDASH for c in l)) else l for l in lines]


def esc(s):
    return s.replace("\\", "\\\\").replace('"', '\\"')


def main():
    names = load_entries(pkhex_path(SRC_REL))

    p = []
    p.append("// AUTO-GENERATED from PKHeX's move-name text (move id = array index).\n")
    p.append("// Source: PKHeX.Core/%s\n" % SRC_REL)
    p.append("// Regenerate with tools/gen_movenames.py (fetches from GitHub; see tools/pkhex_source.py).\n")
    p.append('#include "Names/MoveNames.h"\n')
    p.append("\n")
    p.append("namespace Names {\n")
    p.append("    static const char* const MOVE_NAMES[] = {\n")
    for nm in names:
        p.append('        "%s",\n' % esc(nm))
    p.append("    };\n")
    p.append("\n")
    p.append("    const char* getMoveName(uint16_t moveId) {\n")
    p.append("        constexpr unsigned count = sizeof(MOVE_NAMES) / sizeof(MOVE_NAMES[0]);\n")
    p.append('        if (moveId >= count) return "-";\n')
    p.append("        return MOVE_NAMES[moveId];\n")
    p.append("    }\n")
    p.append("\n")
    p.append("    unsigned getMoveCount() { return sizeof(MOVE_NAMES) / sizeof(MOVE_NAMES[0]); }\n")
    p.append("}\n")

    with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("".join(p))
    print("Wrote", OUT, "with", len(names), "entries")


if __name__ == "__main__":
    main()
