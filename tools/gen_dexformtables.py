#!/usr/bin/env python3
"""Generate the per-game Pokedex FORM-INDEX tables PKSE cannot compute.

WHICH GAMES NEED ONE, AND WHY ONLY THESE. Most Pokedex blocks are indexed arithmetically -- Gen 1
and Gen 3 put a species' bit at `species - 1`, BDSP indexes arrays the same way, Legends: Z-A keys
on the Gen 9 internal species id PKSE already converts for the entity layer. None of those needs a
table, and giving one a lookup for `species - 1` would be inventing work.

The games here are the ones whose dex numbering is ARBITRARY -- an ordering that exists only in
PKHeX's data and cannot be derived from the species number:

  * X/Y and Omega Ruby/Alpha Sapphire keep the base seen/caught bits at `species - 1`, but their
    alternate-FORM flags live in a separate region indexed by a hand-authored per-species table.
    PKHeX holds it as the switch in Zukan6.GetFormIndexXY / GetFormIndexAO -- Vivillon at 83,
    Flabebe at 103, then the Megas from 131, in no order a formula could produce.
  * Sun/Moon, Ultra Sun/Ultra Moon and Let's Go do the same thing in a different shape: parallel
    (species, form-count) arrays walked to a running total (PKHeX DexFormUtil).

Sword/Shield, Scarlet/Violet and Legends: Arceus have their own generators, because their tables
are a different shape again -- whole-dex membership rather than a form-bit offset.

LET'S GO IS THE REASON THIS FILE EXISTS. Its tables were hand-transcribed into Trainer7LGPE.cpp,
correct and carefully sourced but unreachable by any generator -- the same position the pokemondb
base-stat tables were in before they were removed. PKHeX-derived data that cannot be regenerated is
data that will eventually be wrong and have no way to say so.

X/Y, ORAS, Sun/Moon and Ultra Sun/Ultra Moon are emitted AHEAD OF A CALLER: those games do not write
their Pokedex yet (Trainer::updatePokedexBlock is a no-op for them), and the integration guides say
this table is what they will need. A generated table cannot go stale, so having it early costs
nothing but a regeneration.

Run:  python tools/gen_dexformtables.py
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pkhex_source import pkhex_path  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INC = os.path.join(ROOT, "include", "Pokemon")
SRC = os.path.join(ROOT, "src", "Pokemon")

DEX_FORM_UTIL = "Saves/Util/DexFormUtil.cs"
ZUKAN6 = "Saves/Substructures/PokeDex/Zukan6.cs"
ZUKAN7B = "Saves/Substructures/PokeDex/Zukan7b.cs"


def read(relpath):
    with open(pkhex_path(relpath), encoding="utf-8-sig") as fh:
        return fh.read()


def span_array(text, name):
    """The numbers in a `private static ReadOnlySpan<T> NAME => [ ... ];` block."""
    match = re.search(r"\b" + re.escape(name) + r"\s*=>\s*\[(.*?)\]\s*;", text, re.S)
    if not match:
        raise SystemExit(f"{name}: not found -- PKHeX moved or renamed it.")
    body = re.sub(r"//[^\n]*", "", match.group(1))
    return [int(n) for n in re.findall(r"\d+", body)]


def switch_pairs(text, name):
    """`NNN => (IDX, CNT),` arms of a `GetFormIndexXX` switch expression, as {species: (idx, cnt)}."""
    match = re.search(r"\b" + re.escape(name) + r"\s*\(ushort species\)\s*=>\s*species switch\s*\{(.*?)\n\s*\};",
                      text, re.S)
    if not match:
        raise SystemExit(f"{name}: not found -- PKHeX moved or renamed it.")
    out = {}
    for species, index, count in re.findall(r"(\d+)\s*=>\s*\(\s*(\d+)\s*,\s*(\d+)\s*\)", match.group(1)):
        out[int(species)] = (int(index), int(count))
    if not out:
        raise SystemExit(f"{name}: matched no arms -- the switch shape changed.")
    return out


def from_span_pair(text, suffix):
    """DexFormUtil's parallel arrays -> {species: (bitIndex, formCount)}.

    The bit index is a RUNNING TOTAL, exactly as PKHeX's GetDexFormBitIndex computes it: each
    listed species occupies `count` slots, of which the base form is already covered by the
    species' own bit, so each contributes `count - 1`.
    """
    species = span_array(text, "DexSpeciesWithForm_" + suffix)
    counts = span_array(text, "DexSpeciesCount_" + suffix)
    if len(species) != len(counts):
        raise SystemExit(f"{suffix}: {len(species)} species but {len(counts)} counts -- parse is wrong.")
    if species != sorted(species):
        raise SystemExit(f"{suffix}: the species list is not sorted; PKHeX binary-searches it.")
    out, running = {}, 0
    for position, sp in enumerate(species):
        out[sp] = (running, counts[position])
        running += counts[position] - 1
    return out


def emit(code, game, table, entries=None, notes=""):
    """One header/source pair. `table` is {species: (bitIndex, formCount)}."""
    maxsp = max(table)
    guard = f"POKEMON_DEXTABLE{code.upper()}_H"
    extra_decl = ""
    extra_def = ""
    if entries is not None:
        extra_decl = (
            "\n    /// Dex entry index for a (species, form), or -1 when the dex has no entry for it --\n"
            "    /// which is also the GATE: a Pokemon absent from this table is not recorded at all.\n"
            "    /// 0-150 are Kanto, 151/152 are Meltan and Melmetal, and 153+ are the alternate forms\n"
            "    /// below, in table order. PKHeX Zukan7b.TryGetSizeEntryIndex.\n"
            f"    int getDexEntryIndex{code}(uint16_t species, uint8_t form) noexcept;\n")
        rows = "".join("        { %3d, %d },%s" % (sp, fm, "\n" if (n % 6 == 5) else " ")
                       for n, (sp, fm) in enumerate(entries))
        extra_def = f"""
    namespace
    {{
        // The (species, form) pairs Let's Go's Pokedex has an entry for, beyond plain form 0.
        // PKHeX Zukan7b.SizeDexInfoTable, in order -- the index into this list IS the entry offset.
        constexpr uint16_t FORM_ENTRIES[][2] = {{
{rows}
        }};
        constexpr int FORM_ENTRY_COUNT = static_cast<int>(sizeof(FORM_ENTRIES) / sizeof(FORM_ENTRIES[0]));
        constexpr int FORM_ENTRY_BASE = 153; // 0-150 Kanto, 151 Meltan, 152 Melmetal
    }}

    int getDexEntryIndex{code}(uint16_t species, uint8_t form) noexcept
    {{
        if (form == 0)
        {{
            if (species >= 1 && species <= 151)
            {{
                return static_cast<int>(species) - 1;
            }}
            if (species == 808)
            {{
                return 151;
            }}
            if (species == 809)
            {{
                return 152;
            }}
            return -1;
        }}
        for (int entryIndex = 0; entryIndex < FORM_ENTRY_COUNT; ++entryIndex)
        {{
            if (FORM_ENTRIES[entryIndex][0] == species && FORM_ENTRIES[entryIndex][1] == form)
            {{
                return FORM_ENTRY_BASE + entryIndex;
            }}
        }}
        return -1;
    }}
"""

    header = f"""/**
 * GENERATED -- do not hand-edit.
 *
 * Regenerate with `python tools/gen_dexformtables.py`. Source of truth is PKHeX.
 *
 * THE SEEN/CAUGHT BITS DO NOT NEED THIS. They sit at `species - 1`, the way every arithmetic dex
 * works. What needs it is the ALTERNATE-FORM region: {game} indexes those by a hand-authored
 * per-species ordering that exists only in PKHeX's data and cannot be derived from the species
 * number.{notes}
 */
#ifndef {guard}
#define {guard}

#include <cstdint>

namespace Pokemon
{{
    /// Highest species with form bits in {game}.
    inline constexpr uint16_t DEX{code.upper()}_MAX_FORM_SPECIES = {maxsp};

    /// Bit index of this species' FIRST alternate form, or -1 when it has no form bits at all.
    /// The base form is not here -- it is the species' own seen/caught bit.
    int getDexFormBitIndex{code}(uint16_t species) noexcept;

    /// How many dex forms {game} gives this species (including the base form), or 0 for none.
    /// A caller holding the species' real form count should refuse to write when that count is
    /// smaller than this, which is the guard PKHeX's GetDexFormBitIndex applies.
    uint8_t getDexFormCount{code}(uint16_t species) noexcept;
{extra_decl}}}

#endif  // {guard}
"""

    rows = "".join("        { %4d, %4d, %2d },%s" % (sp, table[sp][0], table[sp][1],
                                                     "\n" if (n % 4 == 3) else " ")
                   for n, sp in enumerate(sorted(table)))
    source = f"""/**
 * GENERATED by tools/gen_dexformtables.py.
 */
#include "Pokemon/DexTable{code}.h"

#include <cstddef>

namespace Pokemon
{{
    namespace
    {{
        // {{ species, first form bit, dex form count }}, sorted by species.
        constexpr uint16_t FORM_BITS[][3] = {{
{rows}
        }};
        constexpr size_t FORM_BIT_COUNT = sizeof(FORM_BITS) / sizeof(FORM_BITS[0]);
    }}

    int getDexFormBitIndex{code}(uint16_t species) noexcept
    {{
        for (size_t rowIndex = 0; rowIndex < FORM_BIT_COUNT; ++rowIndex)
        {{
            if (FORM_BITS[rowIndex][0] == species)
            {{
                return static_cast<int>(FORM_BITS[rowIndex][1]);
            }}
        }}
        return -1;
    }}

    uint8_t getDexFormCount{code}(uint16_t species) noexcept
    {{
        for (size_t rowIndex = 0; rowIndex < FORM_BIT_COUNT; ++rowIndex)
        {{
            if (FORM_BITS[rowIndex][0] == species)
            {{
                return static_cast<uint8_t>(FORM_BITS[rowIndex][2]);
            }}
        }}
        return 0;
    }}
{extra_def}}}
"""
    with open(os.path.join(INC, f"DexTable{code}.h"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write(header)
    with open(os.path.join(SRC, f"DexTable{code}.cpp"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write(source)
    print(f"  DexTable{code:<7} {len(table):>3} species with form bits"
          + (f", {len(entries)} dex form entries" if entries else ""))


def main():
    utils = read(DEX_FORM_UTIL)
    zukan6 = read(ZUKAN6)
    zukan7b = read(ZUKAN7B)

    size_table = span_array(zukan7b, "SizeDexInfoTable")
    if len(size_table) % 2:
        raise SystemExit("SizeDexInfoTable: odd number of bytes -- it is (species, form) pairs.")
    lgpe_entries = [(size_table[i], size_table[i + 1]) for i in range(0, len(size_table), 2)]

    emit("6XY", "X/Y", switch_pairs(zukan6, "GetFormIndexXY"))
    emit("6ORAS", "Omega Ruby/Alpha Sapphire", switch_pairs(zukan6, "GetFormIndexAO"))
    emit("7SM", "Sun/Moon", from_span_pair(utils, "SM"))
    emit("7USUM", "Ultra Sun/Ultra Moon", from_span_pair(utils, "USUM"))
    emit("7LGPE", "Let's Go", from_span_pair(utils, "GG"), entries=lgpe_entries,
         notes="\n *\n * Let's Go also gates on WHICH (species, form) pairs its dex has an entry for at\n"
               " * all, which is a second table and a different question from the form bit --\n"
               " * see getDexEntryIndex7LGPE.")


if __name__ == "__main__":
    main()
