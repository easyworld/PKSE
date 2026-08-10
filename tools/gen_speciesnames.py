#!/usr/bin/env python3
"""Generate src/Names/SpeciesNames.cpp -- species id -> display name -- from PKHeX's
Simplified Chinese species-name text resource.

Species id = array index, so PKHeX's flat one-name-per-line list maps directly.
Names are taken verbatim from PKHeX so terminology stays aligned with the games.

Index 0 is PKSE's empty-slot label "无", NOT PKHeX's "蛋": species 0 means "no
Pokemon here" throughout PKSE, and the egg state is a flag on a real species.

Regenerate:  python tools/gen_speciesnames.py
Pulls the PKHeX text resource it reads from GitHub on demand (tools/pkhex_source.py);
no local PKHeX checkout required.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pkhex_source import pkhex_path  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "src", "Names", "SpeciesNames.cpp")
SRC_REL = "Resources/text/other/zh-Hans/text_Species_zh-Hans.txt"

EMPTY_SLOT = "无"   # index 0; PKHeX says "蛋" there


def load_entries(path):
    with open(path, encoding="utf-8") as fh:
        lines = fh.read().split("\n")
    if lines and lines[-1] == "":       # drop the trailing empty from the final newline
        lines = lines[:-1]
    if not lines:
        raise SystemExit("species text resource was empty")
    lines[0] = EMPTY_SLOT
    return lines


def esc(s):
    return s.replace("\\", "\\\\").replace('"', '\\"')


def main():
    names = load_entries(pkhex_path(SRC_REL))

    p = []
    p.append("// AUTO-GENERATED from PKHeX's Simplified Chinese species-name text (species id = array index).\n")
    p.append("// Source: PKHeX.Core/%s\n" % SRC_REL)
    p.append("// Regenerate with tools/gen_speciesnames.py (fetches from GitHub; see tools/pkhex_source.py).\n")
    p.append("//\n")
    p.append("// Names are verbatim and use the official Simplified Chinese terminology.\n")
    p.append("//\n")
    p.append("// Index 0 is \"%s\" (PKSE's empty slot), where PKHeX has \"蛋\".\n" % EMPTY_SLOT)
    p.append("#include <cstdint>\n")
    p.append("#include <cstddef>\n")
    p.append("\n")
    p.append("namespace Names {\n")
    p.append("    static const char* const SPECIES_NAMES[] = {\n")
    for nm in names:
        p.append('        "%s",\n' % esc(nm))
    p.append("    };\n")
    p.append("\n")
    p.append("    // Out of range returns \"未知\", which the legality checker tests for by value.\n")
    p.append("    const char* getSpeciesName(uint16_t speciesId) {\n")
    p.append("        constexpr size_t count = sizeof(SPECIES_NAMES) / sizeof(SPECIES_NAMES[0]);\n")
    p.append('        if (speciesId >= count) return "未知";\n')
    p.append("        return SPECIES_NAMES[speciesId];\n")
    p.append("    }\n")
    p.append("}\n")

    with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("".join(p))
    print("Wrote", OUT, "with", len(names), "entries")


if __name__ == "__main__":
    main()
