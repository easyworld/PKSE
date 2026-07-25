#!/usr/bin/env python3
"""Generate include/Names/ItemPouches.h + src/Names/ItemPouches.cpp -- the list of
item ids that legally belong in each POUCH of each game.

#31 (gen_itempresence.py) answers "can a Pokemon HOLD this item". This answers the
different question the item editor needs: "which items may be ADDED to the pouch the
user is looking at" -- so a Master Ball can't be created in the medicine pocket.

Source of truth is the same PKHeX ItemStorage classes, but a different accessor:
GetItems(InventoryType) / GetLegal(InventoryType) rather than GetAllHeld(). The
mapping below pairs each PKSE pouch, IN ITS OWN ENUM ORDER, with the PKHeX span it
corresponds to -- PKSE's per-game pouch enums are not in a shared order, so this has
to be spelled out per game rather than derived.

Notes on the awkward corners, all verified against the PKHeX sources:
  * FRLG's storage class pulls General/Balls/Berry/Machine from ItemStorage3RS via
    `using static`; only Key is FRLG-specific. Hence the Class.Span syntax below.
  * FRLG's PC Items pouch holds the same set as the main Items pouch (PKHeX maps
    both InventoryType.Items and InventoryType.PCItems to General).
  * PLA's Stored pouch is the overflow box for the satchel, so same set as Regular.
  * S/V has no MegaStones pouch and Z-A has no BattleItems pouch. PKSE's 9SV/9LZA enums
    used to claim both (they were copies of each other) -- corrected in task #41, so each
    game now carries its real pockets and every entry below maps to a real PKHeX span.
  * S/V's key items live in PKHeX's `Event` span, not a `Key` one.

Regenerate with:  python tools/gen_itempouches.py
Reuses gen_itempresence's PKHeX parser, which fetches from GitHub on demand
(tools/pkhex_source.py); no local PKHeX checkout required.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_itempresence import Storage  # noqa: E402  (same C# span parser)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_H = os.path.join(ROOT, "include", "Names", "ItemPouches.h")
OUT_CPP = os.path.join(ROOT, "src", "Names", "ItemPouches.cpp")

# game key -> (default ItemStorage class, GameVersion ids, [(PKSE pouch name, span or None)])
# The pouch list order MUST match that game's PouchType* enum in include/Trainer/.
GAMES = [
    ("FRLG", "ItemStorage3RS", ["FRLG", "FR", "LG"], [
        ("Items",       "General"),
        ("KeyItems",    "ItemStorage3FRLG.Key"),   # FR/LG key items differ from R/S
        ("Balls",       "Balls"),
        ("TMHM",        "Machine"),
        ("Berries",     "Berry"),
        ("PCItems",     "General"),                # same set as Items
    ]),
    ("GG", "ItemStorage7GG", ["GG", "GP", "GE"], [
        ("Medicine",    "Medicine"),
        ("TMs",         "Machine"),
        ("PowerUp",     "PowerUp"),
        ("Candy",       "Candy"),
        ("Balls",       "Catching"),
        ("Battle",      "Battle"),
        # Let's Go has no separate key-item pocket -- PKHeX's Items pouch holds regular AND key
        # items mixed, and PKSE displays it as "Other Items" despite the enum name.
        ("KeyItems",    ["General", "Key"]),
    ]),
    ("SWSH", "ItemStorage8SWSH", ["SWSH", "SW", "SH"], [
        ("Medicine",    "Medicine"),
        ("Balls",       "Balls"),
        ("Battle",      "Battle"),
        ("Berries",     "Berry"),
        ("Items",       "General"),
        ("TMs",         "MachineRecord"),
        ("Treasures",   "Treasure"),
        ("Ingredients", "Ingredients"),
        ("KeyItems",    "Key"),
    ]),
    ("PLA", "ItemStorage8LA", ["PLA"], [
        ("Regular",     "General"),
        ("KeyItems",    "Key"),
        ("Stored",      "General"),                # storage box = same set
        ("Recipes",     "Recipe"),
    ]),
    # Order == the in-game bag tab order (Inventory8BDSP.h enum). The "Items"/General pouch shows as
    # "Other Items". BDSP's add-item picker reads getPouchItems (this table); the parser reads the
    # header's getValidItemIds8BDSP -- both must map index i to the same pouch as the enum.
    ("BDSP", "ItemStorage8BDSP", ["BDSP", "BD", "SP"], [
        ("Medicine",    "Medicine"),
        ("Balls",       "Balls"),
        ("BattleItems", "Battle"),
        ("Berries",     "Berry"),
        ("Items",       "General"),
        ("TMs",         "Machine"),
        ("Treasure",    "Treasure"),
        ("KeyItems",    "Key"),
    ]),
    # Order == the in-game bag order (Inventory9SV.h enum). "TM Materials" (Candy->Material in PKHeX)
    # is a real S/V pocket that was previously missing entirely, dropping every TM-material item.
    ("SV", "ItemStorage9SV", ["SV", "SL", "VL"], [
        ("Medicine",    "Medicine"),               # "Medicines"
        ("Balls",       "Balls"),                  # "Poke Balls"
        ("BattleItems", "Battle"),                 # "Battle Items"
        ("Berries",     "Berry"),
        ("Other",       "Other"),                  # "Other Items"
        ("TMs",         "Machine"),
        ("Material",    "Material"),                # "TM Materials" (PKHeX Candy span)
        ("Treasure",    "Treasure"),               # "Treasures"
        ("Ingredients", "Picnic"),                 # "Picnic Items" (picnic ingredients + furniture)
        ("KeyItems",    "Event"),                  # S/V keeps key items in `Event`
    ]),
    # Order == the in-game bag order (Inventory9LZA.h enum / PKHeX ItemStorage9ZA "Display Order").
    ("ZA", "ItemStorage9ZA", ["ZA"], [
        ("Medicine",    "Medicine"),               # "Medicines"
        ("Balls",       "Balls"),                  # "Poke Balls"
        ("Berries",     "Berry"),
        ("Other",       "Other"),                  # "Other Items" (Z-A has no Battle Items pocket)
        ("TMs",         "TM"),
        ("MegaStones",  "MegaStones"),             # "Mega Stones"
        ("Treasure",    "Treasure"),               # "Treasures"
        ("KeyItems",    "Key"),
    ]),
]

_cache = {}


def resolve(default_cls, spec):
    """`Span` on the game's own class, `OtherClass.Span` to borrow from another, or a LIST
    of either when one PKSE pouch corresponds to several PKHeX spans."""
    if spec is None:
        return []
    if isinstance(spec, (list, tuple)):
        out, seen = [], set()
        for part in spec:
            for i in resolve(default_cls, part):
                if i not in seen:
                    seen.add(i); out.append(i)
        return out
    cls, _, name = spec.rpartition(".")
    cls = cls or default_cls
    if cls not in _cache:
        _cache[cls] = Storage(cls)
    return [i for i in _cache[cls].span(name) if i > 0]


def main():
    data = {}
    for key, cls, _, pouches in GAMES:
        rows = [(pname, resolve(cls, spec)) for pname, spec in pouches]
        data[key] = rows
        total = sum(len(ids) for _, ids in rows)
        detail = "  ".join(f"{p}:{len(i)}" for p, i in rows)
        print(f"  {key:5} {total:5} ids over {len(rows)} pouches   {detail}")

    with open(OUT_H, "w", encoding="utf-8", newline="\n") as fh:
        fh.write('''/**
 * ItemPouches.h - Which item ids legally belong in each pouch of each game.
 *
 * Auto-generated by tools/gen_itempouches.py from PKHeX's ItemStorage classes.
 * DO NOT EDIT BY HAND -- rerun the generator instead.
 *
 * Answers "what may be ADDED here", for the item editor's create flow -- distinct
 * from ItemPresence.h, which answers "what may a Pokemon HOLD". Pouch indices are
 * that game's own PouchType* enum order (include/Trainer/Inventory*.h).
 *
 * A pouch a game does not actually have (S/V MegaStones, Z-A BattleItems) returns
 * an empty span rather than a guess.
 */
#ifndef NAMES_ITEM_POUCHES_H
#define NAMES_ITEM_POUCHES_H

#include <cstdint>
#include <cstddef>
#include <span>

#include "Enums/GameVersion.h"

namespace Names {
    /// Legal item ids for `pouchIndex` of `group`, in that game's PouchType* order.
    /// Empty when the group is unknown, the index is out of range, or the game has
    /// no such pouch.
    std::span<const uint16_t> getPouchItems(Enums::GameVersion group, size_t pouchIndex);

    /// Number of pouches the table carries for `group` (0 if unknown).
    size_t getPouchCount(Enums::GameVersion group);
}

#endif  // NAMES_ITEM_POUCHES_H
''')

    p = []
    p.append('/**\n'
             ' * ItemPouches.cpp - Per-game, per-pouch legal item id lists.\n'
             ' *\n'
             ' * Auto-generated by tools/gen_itempouches.py from PKHeX ItemStorage\n'
             ' * GetItems()/GetLegal(). DO NOT EDIT BY HAND -- rerun the generator.\n'
             ' */\n\n')
    p.append('#include "Names/ItemPouches.h"\n\n')
    p.append("namespace Names {\n\n")

    for key, _, _, _ in GAMES:
        for pname, ids in data[key]:
            if not ids:
                continue
            p.append(f"    static const uint16_t POUCH_{key}_{pname}[] = {{\n")
            for r in range(0, len(ids), 16):
                p.append("        " + " ".join("%d," % i for i in ids[r:r + 16]) + "\n")
            p.append("    };\n")
        p.append("\n")

    for key, _, _, _ in GAMES:
        p.append(f"    static const std::span<const uint16_t> POUCHES_{key}[] = {{\n")
        for pname, ids in data[key]:
            if ids:
                p.append(f"        POUCH_{key}_{pname},\n")
            else:
                p.append(f"        {{}},   // {pname}: not a pouch in this game\n")
        p.append("    };\n\n")

    p.append("    static std::span<const std::span<const uint16_t>> tableFor(Enums::GameVersion g) {\n")
    p.append("        switch (g) {\n")
    for key, _, versions, _ in GAMES:
        for v in versions:
            p.append(f"            case Enums::GameVersion::{v}:\n")
        p.append(f"                return POUCHES_{key};\n")
    p.append("            default:\n")
    p.append("                return {};\n")
    p.append("        }\n")
    p.append("    }\n\n")

    p.append("    std::span<const uint16_t> getPouchItems(Enums::GameVersion group, size_t pouchIndex) {\n")
    p.append("        const auto t = tableFor(group);\n")
    p.append("        return pouchIndex < t.size() ? t[pouchIndex] : std::span<const uint16_t>{};\n")
    p.append("    }\n\n")
    p.append("    size_t getPouchCount(Enums::GameVersion group) {\n")
    p.append("        return tableFor(group).size();\n")
    p.append("    }\n")
    p.append("}\n")

    with open(OUT_CPP, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("".join(p))

    print("\nWrote", OUT_H)
    print("Wrote", OUT_CPP)


if __name__ == "__main__":
    main()
