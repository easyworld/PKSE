#!/usr/bin/env python3
"""Generate include/Names/MoveInfo.h + src/Names/MoveInfo.cpp -- PER-GAME move id -> base
PP tables, transcribed from PKHeX's MoveInfo3/7b/8/8a/9/9a PP arrays.

The creator/move-picker set a move id but the entity's setMove() deliberately does NOT
set PP (it needs a base-PP table); without this the game shows every created/picked move
at "0 PP".

Base PP is NOT stable across generations -- it is the single most rebalanced per-move
number in the series, and a move that kept its id kept nothing else. Giga Drain is 5 PP in
Gen 3 and 10 from Gen 4 on; Recover is 20 in Gen 3, 10 in Gen 4-8 and 5 in Gen 9; Legends:
Arceus re-tuned 92 moves wholesale. This file used to emit ONE array (Gen 9's) and hand it
to all seven games, so every one of those moves was written with Scarlet/Violet's PP into
a save that disagreed.

So: one array per game, keyed on the game group, exactly like MovePresence. PKHeX's own
mapping (MoveInfo.GetPPTable) is the source of truth for which table a game reads, and it
is not one-per-file -- BDSP has no PP array of its own and reads SwSh's, so both groups
point at PP_SWSH here.

NOTE on parsing: MoveInfo7b's array carries `//` comments *inside* the literal (a note
about Absorb/Mega Drain's rebalance) whose digits a naive `\\d+` scrape swallows as PP
values, silently shifting every entry after them by eight. Comments are stripped before
the numbers are read -- do not "simplify" that away.

Regenerate:  python tools/gen_moveinfo.py
Pulls the PKHeX source it reads from GitHub on demand (tools/pkhex_source.py);
no local PKHeX checkout required.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pkhex_source import pkhex_path  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_H = os.path.join(ROOT, "include", "Names", "MoveInfo.h")
OUT_CPP = os.path.join(ROOT, "src", "Names", "MoveInfo.cpp")

# Game group -> the PKHeX MoveInfo file whose PP array that group reads, per
# MoveInfo.GetPPTable(EntityContext). BDSP (Gen8b) deliberately has no table of its own.
SOURCES = [
    ("FRLG", "MoveInfo3.cs"),
    ("GG",   "MoveInfo7b.cs"),
    ("SWSH", "MoveInfo8.cs"),
    ("PLA",  "MoveInfo8a.cs"),
    ("SV",   "MoveInfo9.cs"),
    ("ZA",   "MoveInfo9a.cs"),
]

# Groups that share another group's array rather than getting one of their own.
ALIASES = {"BDSP": "SWSH"}   # PKHeX: Gen8b => MoveInfo8.PP

# Spot-checks printed on every run: moves whose PP differs between games, so a bad parse
# (or a PKHeX bump that moves the goalposts) is visible in the generator's own output.
CANARIES = [(202, "Giga Drain"), (105, "Recover"), (33, "Tackle"), (875, "Psyblade")]


def load_pp(fname):
    """The PP array from a PKHeX MoveInfo file, indexed by move id (entry 0 == 0)."""
    path = pkhex_path("Moves/" + fname)
    with open(path, encoding="utf-8") as fh:
        text = fh.read()
    m = re.search(r"public static ReadOnlySpan<byte> PP\s*=>\s*\[(.*?)\];", text, re.DOTALL)
    if not m:
        raise SystemExit("Could not find the PP array in " + path)
    body = re.sub(r"//[^\n]*", "", m.group(1))              # line comments carry digits
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.DOTALL)  # block comments too
    pp = [int(x) for x in re.findall(r"\d+", body)]
    if any(v > 255 for v in pp):
        raise SystemExit("PP value out of byte range in " + path)
    # Alignment check. Entry 0 is NOT a usable anchor: every table but MoveInfo9a starts it
    # at 0, and 9a starts it at 35 upstream. Struggle (165) and Sketch (166) are 1 PP in
    # every generation and nothing else nearby is, so a table that lost or gained an entry
    # -- the failure mode a stray digit in a comment produces -- shows up here.
    if len(pp) <= 166 or pp[165] != 1 or pp[166] != 1 or not (15 <= pp[1] <= 40):
        raise SystemExit("PP array in %s looks misaligned (len=%d, [1]=%s, [165:167]=%s)" % (
            path, len(pp), pp[1] if len(pp) > 1 else "?", pp[165:167]))
    pp[0] = 0   # move id 0 is the empty slot; normalize MoveInfo9a's stray 35
    return pp


def emit_array(cname, pp):
    rows = []
    for i in range(0, len(pp), 20):
        rows.append("        " + " ".join("%d," % v for v in pp[i:i + 20]))
    return "    static const uint8_t %s[%d] = {\n%s\n    };\n" % (cname, len(pp), "\n".join(rows))


def main():
    tables = {name: load_pp(fname) for name, fname in SOURCES}

    for name, fname in SOURCES:
        print("  %-4s %-14s %4d entries (max move id %d)" % (
            name, fname, len(tables[name]), len(tables[name]) - 1))
    for alias, target in ALIASES.items():
        print("  %-4s shares %s's table (PKHeX has no separate Gen8b PP array)" % (alias, target))

    print("Spot-check (base PP by game):")
    order = [n for n, _ in SOURCES]
    print("        %-14s %s" % ("move", " ".join("%5s" % n for n in order)))
    for mid, label in CANARIES:
        cells = []
        for n in order:
            t = tables[n]
            cells.append("%5s" % (t[mid] if mid < len(t) else "-"))
        print("   %4d %-14s %s" % (mid, label, " ".join(cells)))

    with open(OUT_H, "w", encoding="utf-8", newline="\n") as fh:
        fh.write('''/**
 * MoveInfo.h - Base PP of a move, PER GAME, by move id.
 *
 * Auto-generated by tools/gen_moveinfo.py from PKHeX's MoveInfo3/7b/8/8a/9/9a PP arrays.
 * DO NOT EDIT BY HAND -- rerun the generator instead.
 *
 * Used to set a move's PP when it is created or picked in the editor; the entity's
 * setMove() does not set PP on its own, so without this the game reads 0 PP.
 *
 * Base PP is per-generation data, not a constant of the move. Giga Drain has 5 PP in
 * FireRed/LeafGreen and 10 from Gen 4 on; Recover is 20 / 10 / 5 across Gen 3 / 8 / 9;
 * Legends: Arceus re-tuned 92 moves. Always pass the group the entity actually lives in
 * -- Pokemon::getGameGroup() -- never a default.
 */
#ifndef NAMES_MOVE_INFO_H
#define NAMES_MOVE_INFO_H

#include <cstdint>

#include "Enums/GameVersion.h"

namespace Names {
    /// Base PP (with 0 PP-Ups) of a move as `group` defines it. Returns 0 for the empty
    /// slot (id 0). An id outside that game's table is never offered by the picker (it is
    /// filtered by isMovePresent, whose range comes from this same table); it returns a
    /// small non-zero value rather than 0, because 0 PP is a real unusable-move state.
    uint8_t getMoveBasePP(uint16_t moveId, Enums::GameVersion group);

    /// Max PP of a move with `ppUps` PP-Ups applied, as `group` defines it.
    /// PKHeX PKM.GetMovePP: basePP * (5 + ppUps) / 5. Returns 0 for the empty slot.
    uint8_t getMoveMaxPP(uint16_t moveId, uint8_t ppUps, Enums::GameVersion group);
}

#endif  // NAMES_MOVE_INFO_H
''')

    with open(OUT_CPP, "w", encoding="utf-8", newline="\n") as fh:
        fh.write('''/**
 * MoveInfo.cpp - Per-game base-PP tables.
 *
 * Auto-generated by tools/gen_moveinfo.py from PKHeX's MoveInfo3/7b/8/8a/9/9a PP arrays.
 * DO NOT EDIT BY HAND -- rerun the generator instead.
 */
#include "Names/MoveInfo.h"

namespace Names {

''')
        for name, _ in SOURCES:
            fh.write(emit_array("PP_" + name, tables[name]))
            fh.write("\n")

        cases = []
        for name, _ in SOURCES:
            cases.append("            case Enums::GameVersion::%-4s t = PP_%s; n = %d; break;" % (
                name + ":", name, len(tables[name])))
        for alias, target in ALIASES.items():
            cases.append("            case Enums::GameVersion::%-4s t = PP_%s; n = %d; break;   // shares %s's table" % (
                alias + ":", target, len(tables[target]), target))

        fh.write('''    static inline uint8_t basePP(uint16_t moveId, Enums::GameVersion group) {
        if (moveId == 0) return 0;   // empty move slot -- a real 0, not a missing lookup
        const uint8_t* t;
        uint16_t n;
        switch (group) {
%s
            // A group with no table of its own: fall back to the newest game's numbers.
            // Unreachable today -- every Pokemon subclass returns one of the cases above.
            default: t = PP_SV; n = %d; break;
        }
        return moveId < n ? t[moveId] : 5;
    }

    uint8_t getMoveBasePP(uint16_t moveId, Enums::GameVersion group) {
        return basePP(moveId, group);
    }

    uint8_t getMoveMaxPP(uint16_t moveId, uint8_t ppUps, Enums::GameVersion group) {
        const uint32_t base = basePP(moveId, group);
        if (base == 0) return 0;
        if (ppUps > 3) ppUps = 3;
        const uint32_t pp = base * (5u + ppUps) / 5u;   // PKHeX PKM.GetMovePP
        return static_cast<uint8_t>(pp > 255u ? 255u : pp);
    }

}
''' % ("\n".join(cases), len(tables["SV"])))

    print("Wrote", OUT_H)
    print("Wrote", OUT_CPP)


if __name__ == "__main__":
    main()
