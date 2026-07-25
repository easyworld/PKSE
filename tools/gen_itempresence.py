#!/usr/bin/env python3
"""Generate include/Names/ItemPresence.h + src/Names/ItemPresence.cpp from PKHeX's
per-game item storage classes.

PKSE's cross-game bank sanitizes a transferred Pokemon's MOVESET against the
destination game (see gen_learnsets.py). Its HELD ITEM needs the same treatment: an
item id that does not exist in the destination is meaningless there, and for the two
games with no held-item mechanic at all it is outright invalid.

Source of truth is PKHeX `Legal.HeldItems_*`, each of which is an ItemStorage class's
`GetAllHeld()` -- the union of the pouches whose contents a Pokemon may hold:

    FRLG  ItemStorage3RS.GetAllHeld()      (Gen 3 shares ONE held-item set across
                                            R/S/E/FR/LG; ItemStorage3FRLG exists but
                                            only differs in KEY items, and defines no
                                            GetAllHeld -- PKHeX uses the RS list for
                                            every Gen 3 game's held-item legality)
    GG    []                               Let's Go has NO held items
    SWSH  ItemStorage8SWSH.GetAllHeld()
    BDSP  ItemStorage8BDSP.GetAllHeld()
    PLA   []                               Legends: Arceus has NO held items either
    SV    ItemStorage9SV.GetAllHeld()
    ZA    ItemStorage9ZA.GetAllHeld()

Note the id SPACES differ: FRLG's ids are Gen 3 ids, everything else uses the modern
space. That is fine because the check runs on the already-converted entity, whose
held item is in the destination's own space.

Emits one bitset per game (bit N set => item N may be held) plus:
    bool isHeldItemPresent(uint16_t itemId, Enums::GameVersion group);

Regenerate with:  python tools/gen_itempresence.py
Pulls the PKHeX sources it reads from GitHub on demand (tools/pkhex_source.py);
no local PKHeX checkout required.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pkhex_source import pkhex_path  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_H = os.path.join(ROOT, "include", "Names", "ItemPresence.h")
OUT_CPP = os.path.join(ROOT, "src", "Names", "ItemPresence.cpp")

# game key -> (ItemStorage class file, GameVersion enum ids)
GAMES = [
    ("FRLG", "ItemStorage3RS",   ["FRLG", "FR", "LG"]),
    ("GG",   None,               ["GG", "GP", "GE"]),      # no held items
    ("SWSH", "ItemStorage8SWSH", ["SWSH", "SW", "SH"]),
    ("BDSP", "ItemStorage8BDSP", ["BDSP", "BD", "SP"]),
    ("PLA",  None,               ["PLA"]),                 # no held items
    ("SV",   "ItemStorage9SV",   ["SV", "SL", "VL"]),
    ("ZA",   "ItemStorage9ZA",   ["ZA"]),
]


def strip_comments(s):
    s = re.sub(r"/\*.*?\*/", "", s, flags=re.S)
    return re.sub(r"//[^\n]*", "", s)


class Storage:
    """Reads the named ushort spans out of one ItemStorage C# class."""

    def __init__(self, cls):
        self.cls = cls
        with open(pkhex_path("Items/" + cls + ".cs"), encoding="utf-8") as fh:
            self.text = strip_comments(fh.read())
        self._cache = {}

    def const(self, name):
        """Resolve a `const int NAME = N;` used inside a slice expression."""
        m = re.search(r"const\s+\w+\s+" + re.escape(name) + r"\s*=\s*(\d+)\s*;", self.text)
        if not m:
            raise SystemExit(f"{self.cls}: const {name} not found")
        return int(m.group(1))

    def span(self, name):
        """Resolve a named span to a list of ids, following compositions and aliases."""
        if name in self._cache:
            return self._cache[name]
        m = re.search(r"ReadOnlySpan<ushort>\s+" + re.escape(name) + r"\s*=>\s*(.*?);",
                      self.text, re.S)
        if not m:
            raise SystemExit(f"{self.cls}: span {name} not found")
        self._cache[name] = self._resolve(m.group(1).strip())
        return self._cache[name]

    def _resolve(self, body):
        """A span body is a literal list, a spread composition, or an alias/slice of
        another span (`Machine[..COUNT_TM]` = first N, `Other[..^2]` = drop last N)."""
        body = body.strip()
        if body.startswith("["):
            inner = body[1:-1]
            if ".." in inner:
                return self._compose(inner)
            return [int(x) for x in re.findall(r"\d+", inner)]
        return self._slice_expr(body)

    def _num(self, tok):
        return int(tok) if tok.isdigit() else self.const(tok)

    def _slice_expr(self, expr):
        """`Name`, `Name[..N]` (first N), `Name[..^N]` (drop last N),
        `Name.Slice(start)` or `Name.Slice(start, length)`."""
        expr = expr.strip()
        m = re.fullmatch(r"([A-Za-z_]\w*)\.Slice\(\s*([A-Za-z_]\w*|\d+)\s*"
                         r"(?:,\s*([A-Za-z_]\w*|\d+)\s*)?\)", expr)
        if m:
            ids = list(self.span(m.group(1)))
            start = self._num(m.group(2))
            return ids[start:start + self._num(m.group(3))] if m.group(3) else ids[start:]
        m = re.fullmatch(r"([A-Za-z_]\w*)\s*(?:\[\s*\.\.(\^?)\s*([A-Za-z_]\w*|\d+)\s*\])?", expr)
        if not m:
            raise SystemExit(f"{self.cls}: cannot parse span expression `{expr}`")
        ids = list(self.span(m.group(1)))
        if m.group(3) is None:
            return ids
        n = self._num(m.group(3))
        return ids[:-n] if m.group(2) == "^" else ids[:n]

    def _compose(self, inner):
        """`[..A, ..B[..^2], ..C[..COUNT_TM]]` -> the concatenation of those spans."""
        out = []
        for term in re.finditer(
                r"\.\.\s*([A-Za-z_]\w*(?:\.Slice\([^)]*\)|\s*\[\s*\.\.\^?\s*(?:[A-Za-z_]\w*|\d+)\s*\])?)",
                inner):
            out.extend(self._slice_expr(term.group(1)))
        if not out:
            raise SystemExit("empty composition: " + inner)
        return out

    def all_held(self):
        m = re.search(r"GetAllHeld\(\)\s*=>\s*(\[.*?\]);", self.text, re.S)
        if not m:
            raise SystemExit(f"{self.cls}: GetAllHeld() not found")
        return self._resolve(m.group(1))


def main():
    sets = {}
    for key, cls, _ in GAMES:
        if cls is None:
            sets[key] = set()
            continue
        ids = Storage(cls).all_held()
        ids = {i for i in ids if i > 0}          # 0 == "no item", never a real held item
        sets[key] = ids
        print(f"  {key:5} held items: {len(ids):5}  (max id {max(ids)})")

    max_id = max((max(s) for s in sets.values() if s), default=0)
    width = (max_id // 8) + 1
    print(f"\n  max item id {max_id} -> bitset width {width} bytes/game")

    hdr = f'''/**
 * ItemPresence.h - Per-game "can a Pokemon HOLD this item?" lookup.
 *
 * Auto-generated by tools/gen_itempresence.py from PKHeX's ItemStorage classes.
 * DO NOT EDIT BY HAND -- rerun the generator instead.
 *
 * Used to sanitize a transferred Pokemon's held item, the same way LearnsetTable
 * sanitizes its moveset. Let's Go and Legends: Arceus have NO held-item mechanic,
 * so they report false for every id. FireRed/LeafGreen ids are Gen 3 ids; every
 * other game uses the modern item space -- callers must pass an id already in the
 * queried game's own space.
 */
#ifndef NAMES_ITEM_PRESENCE_H
#define NAMES_ITEM_PRESENCE_H

#include <cstdint>

#include "Enums/GameVersion.h"

namespace Names {{
    constexpr uint16_t ITEM_PRESENCE_MAX_ID = {max_id};

    /// True if a Pokemon can legally hold `itemId` in `group`. False for an unknown
    /// group, an out-of-range id, or a game with no held-item mechanic at all.
    bool isHeldItemPresent(uint16_t itemId, Enums::GameVersion group);
}}

#endif  // NAMES_ITEM_PRESENCE_H
'''
    with open(OUT_H, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(hdr)

    p = []
    p.append('/**\n'
             ' * ItemPresence.cpp - Per-game held-item bitsets.\n'
             ' *\n'
             ' * Auto-generated by tools/gen_itempresence.py from PKHeX\'s ItemStorage\n'
             ' * GetAllHeld() unions. DO NOT EDIT BY HAND -- rerun the generator.\n'
             ' *\n'
             ' * Bit N of a game\'s table is set iff item N may be held in that game.\n'
             ' * Let\'s Go and Legends: Arceus have no held items, so they have no table.\n'
             ' */\n\n')
    p.append('#include "Names/ItemPresence.h"\n\n')
    p.append("namespace Names {\n\n")
    p.append(f"    constexpr size_t ITEM_BITSET_BYTES = {width};\n\n")

    for key, cls, _ in GAMES:
        ids = sets[key]
        if not ids:
            continue
        bits = bytearray(width)
        for i in ids:
            bits[i >> 3] |= 1 << (i & 7)
        p.append(f"    // {key}: {len(ids)} holdable items (PKHeX {cls}.GetAllHeld)\n")
        p.append(f"    static const uint8_t ITEM_{key}_BITS[ITEM_BITSET_BYTES] = {{\n")
        for r in range(0, width, 16):
            p.append("        " + "".join("0x%02X, " % b for b in bits[r:r + 16]).rstrip() + "\n")
        p.append("    };\n\n")

    p.append("    bool isHeldItemPresent(uint16_t itemId, Enums::GameVersion group) {\n")
    p.append("        if (itemId == 0 || itemId > ITEM_PRESENCE_MAX_ID)\n")
    p.append("            return false;\n")
    p.append("        const uint8_t* bits = nullptr;\n")
    p.append("        switch (group) {\n")
    for key, cls, versions in GAMES:
        cases = "".join(f"            case Enums::GameVersion::{v}:\n" for v in versions)
        p.append(cases)
        if sets[key]:
            p.append(f"                bits = ITEM_{key}_BITS; break;\n")
        else:
            p.append(f"                return false;   // {key} has no held-item mechanic\n")
    p.append("            default:\n")
    p.append("                return false;\n")
    p.append("        }\n")
    p.append("        return (bits[itemId >> 3] >> (itemId & 7)) & 1;\n")
    p.append("    }\n")
    p.append("}\n")

    with open(OUT_CPP, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("".join(p))

    print("\nWrote", OUT_H)
    print("Wrote", OUT_CPP)
    tables = sum(1 for k in sets if sets[k])
    print(f"  {tables} tables x {width} B = {tables * width} B")


if __name__ == "__main__":
    main()
