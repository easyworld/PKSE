#!/usr/bin/env python3
"""Generate PKSE's per-species personal tables -- ONE FILE PER SAVE-FORMAT GROUP.

WHY THIS EXISTS, since it replaces three generators' worth of output.

PKHeX ships EXACTLY ONE personal table per group PKSE supports -- nineteen resources for
nineteen groups, a clean 1:1 -- and PKSE used to collapse them into six tables from three
different sources. The collapse was lossy, not merely untidy:

  * gen_oldgens.py picked one REPRESENTATIVE per generation and fed it to every group in that
    generation: personal_hgss served Diamond/Pearl AND Platinum, personal_b2w2 served
    Black/White, personal_ao served X/Y, personal_uu served Sun/Moon. Its own comment records
    the loss -- "personal_b2w2 ... superset of BW: B2W2 filled in hidden abilities" -- so a
    Black/White Pokemon was read against Black 2's table.
  * personal_gg and personal_za were never opened at all. Let's Go and Legends: Z-A took their
    base stats from BaseStatsGen7/BaseStatsGen89, which are not PKHeX-derived: they came from
    pokemondb.net through an external exporter, so they could not be regenerated at the pinned
    PKHEX_REF and could not pick up an upstream correction.
  * BaseStatsGen89 served FIVE distinct Switch groups from one table, while the Switch games'
    other per-species data lived in a sixth (PersonalInfoTable, sourced from Scarlet/Violet).
    One species' stats and its abilities came from different files with different provenance.

So: one resource in, one file out, no sharing. A group whose games genuinely agree still gets
its own copy -- that is the same rule the entity, trainer, encryption and inventory layers
already follow, and it is what makes each game readable in isolation.

WHAT A ROW CARRIES is PersonalRecord (include/Pokemon/PersonalRecord.h), one shape for every
group. Fields a generation does not have are ZERO and must be gated on the predicates there
rather than read as data -- 0 is a real ability id and a real friendship value, so a getter
cannot tell "absent" from "zero". Base EXP is deliberately absent: nothing reads it.

TWO CONVERSIONS HAPPEN HERE so no call site can forget them:

  * GEN 1 HAS ONE SPECIAL, not SpA/SpD. The single byte is written into BOTH slots, which is
    what Pokemon1RBY already does at the entity level ("both interface slots read and write the
    same underlying value"). Reading either gives the right number; personalHasSplitSpecial()
    says whether they are independent.
  * GEN 1 AND GEN 2 TYPE IDS ARE ROM NUMBERING, and it is neither modern nor contiguous: Bug 7,
    Ghost 8 (6 is the unused "Bird" slot), then the special block at 20-26. Mapped to PKSE
    TYPE_* here, at generation time. Gen 3 onward already store the Gen 6+ numbering with a gap
    where Fairy will be, so they pass through untouched.

Run:  python tools/gen_personaltables.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pkhex_source import pkhex_path, pkhex_ref  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INC = os.path.join(ROOT, "include", "Pokemon")
SRC = os.path.join(ROOT, "src", "Pokemon")

TYPE_NONE = 255

# Gen 1 / Gen 2 ROM type id -> PKSE TYPE_* (the contiguous MoveType numbering in PokemonTypes.h).
G1_TYPE_TO_PKSE = {0: 0, 1: 1, 2: 2, 3: 3, 4: 4, 5: 5, 7: 6, 8: 7,
                   20: 9, 21: 10, 22: 11, 23: 12, 24: 13, 25: 14, 26: 15}
G2_TYPE_TO_PKSE = dict(G1_TYPE_TO_PKSE)
G2_TYPE_TO_PKSE.update({9: 8, 27: 16})   # Steel and Dark are Gen 2's additions

# group code -> layout. Offsets verified against PKHeX.Core/PersonalInfo/Info/PersonalInfo*.cs
# at the pinned ref. `stats` is the offset of the first of HP/ATK/DEF/SPE/SPA/SPD in PKHeX's
# order -- NOT the order PersonalRecord declares them in, which is why decode() names each one.
#   fields: resource, size, stats, type1, type2, catch, gender, friend, growth,
#           ability1, ability2, abilityHidden, abilityWidth, formCount, formIndex, maxSpecies, typemap
GROUPS = [
    # code      resource         SIZE  stats t1    t2    catch gend  frnd  grow  ab1   ab2   abH   aw fc    fi    max   typemap
    ("1RBY",   "personal_rb",   0x1C, 0x01, 0x06, 0x07, 0x08, 0x00, None, 0x13, None, None, None, 1, None, None,  151, "g1"),
    ("2GSC",   "personal_c",    0x20, 0x01, 0x07, 0x08, None, 0x0D, None, 0x16, None, None, None, 1, None, None,  251, "g2"),
    ("3RSE",   "personal_rs",   0x1C, 0x00, 0x06, 0x07, 0x08, 0x10, 0x12, 0x13, 0x16, 0x17, None, 1, None, None,  386, None),
    ("3FRLG",  "personal_fr",   0x1C, 0x00, 0x06, 0x07, 0x08, 0x10, 0x12, 0x13, 0x16, 0x17, None, 1, None, None,  386, None),
    ("4DP",    "personal_dp",   0x2C, 0x00, 0x06, 0x07, 0x08, 0x10, 0x12, 0x13, 0x16, 0x17, None, 1, 0x29, 0x2A,  493, None),
    ("4PT",    "personal_pt",   0x2C, 0x00, 0x06, 0x07, 0x08, 0x10, 0x12, 0x13, 0x16, 0x17, None, 1, 0x29, 0x2A,  493, None),
    ("4HGSS",  "personal_hgss", 0x2C, 0x00, 0x06, 0x07, 0x08, 0x10, 0x12, 0x13, 0x16, 0x17, None, 1, 0x29, 0x2A,  493, None),
    ("5BW",    "personal_bw",   0x3C, 0x00, 0x06, 0x07, 0x08, 0x12, 0x14, 0x15, 0x18, 0x19, 0x1A, 1, 0x20, 0x1C,  649, None),
    ("5B2W2",  "personal_b2w2", 0x4C, 0x00, 0x06, 0x07, 0x08, 0x12, 0x14, 0x15, 0x18, 0x19, 0x1A, 1, 0x20, 0x1C,  649, None),
    ("6XY",    "personal_xy",   0x40, 0x00, 0x06, 0x07, 0x08, 0x12, 0x14, 0x15, 0x18, 0x19, 0x1A, 1, 0x20, 0x1C,  721, None),
    ("6ORAS",  "personal_ao",   0x50, 0x00, 0x06, 0x07, 0x08, 0x12, 0x14, 0x15, 0x18, 0x19, 0x1A, 1, 0x20, 0x1C,  721, None),
    ("7SM",    "personal_sm",   0x54, 0x00, 0x06, 0x07, 0x08, 0x12, 0x14, 0x15, 0x18, 0x19, 0x1A, 1, 0x20, 0x1C,  802, None),
    ("7USUM",  "personal_uu",   0x54, 0x00, 0x06, 0x07, 0x08, 0x12, 0x14, 0x15, 0x18, 0x19, 0x1A, 1, 0x20, 0x1C,  807, None),
    ("7LGPE",  "personal_gg",   0x54, 0x00, 0x06, 0x07, 0x08, 0x12, 0x14, 0x15, 0x18, 0x19, 0x1A, 1, 0x20, 0x1C,  809, None),
    ("8SWSH",  "personal_swsh", 0xB0, 0x00, 0x06, 0x07, 0x08, 0x12, 0x14, 0x15, 0x18, 0x1A, 0x1C, 2, 0x20, 0x1E,  898, None),
    ("8BDSP",  "personal_bdsp", 0x44, 0x00, 0x06, 0x07, 0x08, 0x12, 0x14, 0x15, 0x18, 0x1A, 0x1C, 2, 0x20, 0x1E,  493, None),
    ("8LA",    "personal_la",   0xB0, 0x00, 0x06, 0x07, 0x08, 0x12, 0x14, 0x15, 0x18, 0x1A, 0x1C, 2, 0x20, 0x1E,  905, None),
    ("9SV",    "personal_sv",   0x50, 0x00, 0x06, 0x07, 0x08, 0x0C, 0x0E, 0x0F, 0x12, 0x14, 0x16, 2, 0x1A, 0x18, 1025, None),
    ("9LZA",   "personal_za",   0x50, 0x00, 0x06, 0x07, 0x08, 0x0C, 0x0E, 0x0F, 0x12, 0x14, 0x16, 2, 0x1A, 0x18, 1000, None),
]

KEYS = ("code resource size stats type1 type2 catch gender friend growth "
        "ability1 ability2 abilityHidden abilityWidth formCount formIndex maxSpecies typemap").split()


def layout(entry):
    return dict(zip(KEYS, entry))


def u16(entry_bytes, offset):
    return entry_bytes[offset] | (entry_bytes[offset + 1] << 8)


def convert_type(typemap, raw, code):
    if typemap is None:
        return raw
    table = G1_TYPE_TO_PKSE if typemap == "g1" else G2_TYPE_TO_PKSE
    if raw not in table:
        raise SystemExit(f"{code}: unmapped ROM type id {raw}. Extend the conversion table.")
    return table[raw]


def decode(lay, entry_bytes):
    stats = lay["stats"]
    singleSpecial = lay["typemap"] == "g1"   # Gen 1 stores ONE Special where the rest store two
    hp, atk, dfn, spe = (entry_bytes[stats + k] for k in range(4))
    if singleSpecial:
        spa = spd = entry_bytes[stats + 4]
    else:
        spa, spd = entry_bytes[stats + 4], entry_bytes[stats + 5]

    def ability(offset):
        if offset is None:
            return 0
        return u16(entry_bytes, offset) if lay["abilityWidth"] == 2 else entry_bytes[offset]

    type1 = convert_type(lay["typemap"], entry_bytes[lay["type1"]], lay["code"])
    type2 = convert_type(lay["typemap"], entry_bytes[lay["type2"]], lay["code"])
    if type2 == type1:
        type2 = TYPE_NONE   # PKSE's convention for a single-typed species

    return dict(
        hp=hp, atk=atk, dfn=dfn, spe=spe, spa=spa, spd=spd,
        type1=type1, type2=type2,
        ability1=ability(lay["ability1"]),
        ability2=ability(lay["ability2"]),
        abilityHidden=ability(lay["abilityHidden"]),
        # Gen 2 stores no per-species base friendship; PKHeX hard-codes 70 for it.
        genderRatio=entry_bytes[lay["gender"]],
        baseFriendship=70 if lay["friend"] is None else entry_bytes[lay["friend"]],
        growthRate=entry_bytes[lay["growth"]],
        catchRate=0 if lay["catch"] is None else entry_bytes[lay["catch"]],
        # A generation with no alternate forms reads as one form and never redirects.
        formCount=1 if lay["formCount"] is None else max(1, entry_bytes[lay["formCount"]]),
        formIndex=0 if lay["formIndex"] is None else u16(entry_bytes, lay["formIndex"]),
    )


def sanity(code, rows):
    """Pin the values a wrong entry size or a wrong offset would break.

    AN ALL-ZERO ROW IS REAL DATA, NOT A BAD READ. From Gen 8 on, a personal table carries a
    row for every National Dex id but ZEROES the ones that game does not have -- Sword/Shield
    has no Weedle, Legends: Arceus no Bulbasaur. That is precisely the dex-presence answer, and
    it is why the checks below skip empty rows instead of failing on them: a wrong offset shows
    up as most of the table being empty, which the population count catches, while a handful of
    empty rows is the game telling the truth about its own roster.
    """
    # PKHeX's gender byte is a closed set: always-male 0, the four thresholds, 87.5% female,
    # always-female 254, genderless 255. Anything else means the byte read is not the ratio.
    allowed = {0, 31, 63, 127, 191, 225, 254, 255}
    present = 0
    for species, row in enumerate(rows):
        if species == 0:
            continue
        if row["hp"] == 0 and row["atk"] == 0 and row["def" if False else "dfn"] == 0:
            continue   # this game does not have this species
        present += 1
        if row["genderRatio"] not in allowed:
            raise SystemExit(f"{code}: species {species} gender byte {row['genderRatio']} is not a "
                             f"ratio -- the entry size or the gender offset is wrong.")
        if row["growthRate"] > 5:
            raise SystemExit(f"{code}: species {species} growth rate {row['growthRate']} is out of "
                             f"range 0-5 -- the growth offset is wrong.")
    # A wrong stat offset empties the table; a real roster never does. The floor is deliberately
    # low (Let's Go carries only 153 species) and is a smoke test, not a roster check.
    if present < 100:
        raise SystemExit(f"{code}: only {present} species have base stats -- the stat offset or "
                         f"the entry size is wrong.")
    return present


def emit(lay, rows, formRows):
    code, count = lay["code"], len(rows) + len(formRows)
    guard = f"POKEMON_PERSONALINFO{code.upper()}_H"
    header = [
        f"/**\n * GENERATED from PKHeX's {lay['resource']} -- do not hand-edit. Regenerate with"
        f"\n * `python tools/gen_personaltables.py`.\n *\n"
        f" * Fields this group's generation does not have read 0 -- gate them on the predicates in\n"
        f" * PersonalRecord.h rather than reading them as data.\n *\n"
        f" * AN ALL-ZERO ROW IS REAL DATA, NOT A GAP, and it does not mean the same thing in every\n"
        f" * table. Sword/Shield and Legends: Arceus ZERO the ids their game does not have (664 of\n"
        f" * 898, and 226 of 905) -- there, an empty row is the roster speaking. Let's Go and\n"
        f" * Scarlet/Violet carry full stats for every id whether the game has it or not, so an\n"
        f" * empty row there means nothing at all. DO NOT READ DEX PRESENCE OUT OF THIS TABLE:\n"
        f" * PersonalInfoTable's `presence` bits are the cross-game answer and are still the only\n"
        f" * one. What a caller must do here is not compute against a zero base stat.\n */\n",
        f"#ifndef {guard}\n#define {guard}\n\n",
        '#include "Pokemon/PersonalRecord.h"\n\nnamespace Pokemon\n{\n',
        f"    /// Highest National Dex id this group has a row for.\n"
        f"    inline constexpr uint16_t PERSONAL_MAX_SPECIES_{code} = {lay['maxSpecies']};\n\n",
        f"    /// Rows in the table: the form-0 entries, then the alternate-form entries a base\n"
        f"    /// row's formIndex redirects into.\n"
        f"    inline constexpr size_t PERSONAL_COUNT_{code} = {count};\n\n",
        f"    extern const PersonalRecord PERSONAL_{code}[PERSONAL_COUNT_{code}];\n\n",
        f"    /// Row for (species, form), mirroring PKHeX's FormStatsIndex redirection. An unknown\n"
        f"    /// species or a form this group does not define falls back to the species' form-0 row;\n"
        f"    /// a species this group does not have at all returns the empty record, whose zero base\n"
        f"    /// stats mean NO DATA and must not be computed against.\n"
        f"    const PersonalRecord &getPersonalInfo{code}(uint16_t species, uint8_t form) noexcept;\n",
        "}\n\n#endif  // " + guard + "\n",
    ]

    def row_text(row):
        return ("        {%3d,%4d,%4d,%4d,%4d,%4d, %3d,%4d, %4d,%4d,%4d, %3d,%4d,%2d,%4d, %2d,%5d},"
                % (row["hp"], row["atk"], row["dfn"], row["spe"], row["spa"], row["spd"],
                   row["type1"], row["type2"], row["ability1"], row["ability2"], row["abilityHidden"],
                   row["genderRatio"], row["baseFriendship"], row["growthRate"], row["catchRate"],
                   row["formCount"], row["formIndex"]))

    source = [
        f"/**\n * GENERATED from PKHeX's {lay['resource']}; regenerate with\n"
        f" * `python tools/gen_personaltables.py`.\n */\n",
        f'#include "Pokemon/PersonalInfo{code}.h"\n\nnamespace Pokemon\n{{\n',
        f"    // hp atk def spe spa spd | type1 type2 | ability1 ability2 abilityHidden |\n"
        f"    // genderRatio baseFriendship growthRate catchRate | formCount formIndex\n",
        f"    const PersonalRecord PERSONAL_{code}[PERSONAL_COUNT_{code}] = {{\n",
    ]
    for species, row in enumerate(rows):
        source.append(row_text(row) + f"  // {species}\n")
    if formRows:
        source.append(f"        // alternate-form rows, indexed by a base row's formIndex\n")
        for offset, row in enumerate(formRows):
            source.append(row_text(row) + f"  // form row {len(rows) + offset}\n")
    source.append("    };\n\n")
    source.append(
        f"    const PersonalRecord &getPersonalInfo{code}(uint16_t species, uint8_t form) noexcept\n"
        f"    {{\n"
        f"        if (species == 0 || species > PERSONAL_MAX_SPECIES_{code})\n"
        f"        {{\n            return PERSONAL_RECORD_EMPTY;\n        }}\n"
        f"        const PersonalRecord &base = PERSONAL_{code}[species];\n"
        f"        // Guarding formIndex == 0 matters: index 0 is a real row (species 0) and would\n"
        f"        // otherwise be returned as though it were a form.\n"
        f"        if (form == 0 || base.formIndex == 0 || form >= base.formCount)\n"
        f"        {{\n            return base;\n        }}\n"
        f"        const size_t formRowIndex = static_cast<size_t>(base.formIndex) + form - 1;\n"
        f"        if (formRowIndex >= PERSONAL_COUNT_{code})\n"
        f"        {{\n            return base;\n        }}\n"
        f"        return PERSONAL_{code}[formRowIndex];\n"
        f"    }}\n}}\n")

    with open(os.path.join(INC, f"PersonalInfo{code}.h"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write("".join(header))
    with open(os.path.join(SRC, f"PersonalInfo{code}.cpp"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write("".join(source))
    return count


# ---------------------------------------------------------------------------------------------
# Gen 2 items
#
# Gen 2's items are their own id space -- its
# TMs are ordinary bag items at 191-249, which the modern numbering has nothing at -- so naming a
# Gen 2 bag through the modern table is wrong in every slot rather than blank, which is the
# failure mode that hides. Gen1Tables carries Gen 1's for the same reason; Gen2Tables is its
# sibling, and the pairing is the point.
# ---------------------------------------------------------------------------------------------
MAX_ITEM_2 = 255   # Gen 2 item ids are one byte; 0xFF terminates a pouch


def load_gen2_items():
    raw = open(pkhex_path("Resources/text/items/gen2/text_ItemsG2_en.txt"), "rb").read()
    lines = raw.decode("utf-8-sig").splitlines()
    names = [ln.strip() for ln in lines[:MAX_ITEM_2 + 1]]
    while len(names) <= MAX_ITEM_2:
        names.append("")
    return names


def pouch_gen2_legal():
    """Every id PKHeX puts in ANY Gen 2 pouch -- the union of General, Balls, KeyGS, KeyCrystal
    and Machine. Reuses the same C# span parser the pouch generator uses, so the two cannot drift."""
    from gen_itempresence import Storage
    st = Storage("ItemStorage2")
    out = []
    for span in ("General", "Balls", "KeyGS", "KeyCrystal", "Machine"):
        out.extend(i for i in st.span(span) if 0 < i <= MAX_ITEM_2)
    return out


def emit_gen2_items():
    names = load_gen2_items()
    legal = sorted(set(pouch_gen2_legal()))
    header = [
        "/**\n * GENERATED -- do not hand-edit. Generation 2 (Gold/Silver/Crystal) item data.\n"
        " *\n * Regenerate with `python tools/gen_personaltables.py`. Source of truth is PKHeX.\n"
        " *\n * The sibling of Gen1Tables.h, and separate for the same reason: Gen 2's item ids are its own\n"
        " * space, sharing nothing with the modern table. Its TMs are ordinary bag items at 191-249, where\n"
        " * the modern numbering has nothing at all, so naming a Gen 2 bag through Names::getItemName is\n"
        " * wrong in every slot rather than blank -- and a plausible wrong answer is the failure that hides.\n"
        " *\n * Gen 2's PER-SPECIES data is not here. It lives in PersonalInfo2GSC.h, one file per\n"
        " * save-format group like every other generation's.\n */\n",
        "#ifndef POKEMON_GEN2TABLES_H\n#define POKEMON_GEN2TABLES_H\n\n#include <cstdint>\n\nnamespace Pokemon\n{\n",
        f"    /// Highest Gen 2 item id. Ids are ONE BYTE and 0xFF terminates a pouch.\n"
        f"    inline constexpr uint8_t MAX_ITEM_GEN2 = {MAX_ITEM_2};\n\n",
        "    /// Gen 2 item name by Gen 2 item id. \"???\" for an id the games never use.\n"
        "    const char *getItemNameGen2(uint8_t itemId) noexcept;\n\n",
        "    /// May this id legitimately sit in a Gen 2 bag? Narrower than \"has a name\": the unused\n"
        "    /// slots are named `???` and the picker must not offer them. The DISPLAY names everything,\n"
        "    /// because a save can hold a byte the picker would never offer and the user has to be able\n"
        "    /// to see what it is rather than a blank row.\n"
        "    bool isItemLegalGen2(uint8_t itemId) noexcept;\n",
        "}\n\n#endif  // POKEMON_GEN2TABLES_H\n",
    ]
    source = ["/**\n * GENERATED by tools/gen_personaltables.py.\n */\n",
              '#include "Pokemon/Gen2Tables.h"\n\n#include <cstddef>\n\nnamespace Pokemon\n{\n',
              "    namespace\n    {\n        const char *const ITEM_NAMES_G2[] = {\n"]
    for itemId, name in enumerate(names):
        escaped = name.replace("\\", "\\\\").replace('"', '\\"') or "???"
        source.append('            "%s",%s// %d\n' % (escaped, " " * max(1, 24 - len(escaped)), itemId))
    source.append("        };\n\n        const uint8_t ITEM_LEGAL_G2[] = {\n")
    for start in range(0, len(legal), 16):
        source.append("            " + ", ".join(str(i) for i in legal[start:start + 16]) + ",\n")
    source.append("        };\n    }\n\n")
    source.append(
        "    const char *getItemNameGen2(uint8_t itemId) noexcept\n    {\n"
        "        const char *name = ITEM_NAMES_G2[itemId];\n"
        "        return (name[0] != '\\0') ? name : \"???\";\n    }\n\n"
        "    bool isItemLegalGen2(uint8_t itemId) noexcept\n    {\n"
        "        // Sorted, so a binary search; the list is the union of every Gen 2 pouch.\n"
        "        size_t low = 0, high = sizeof(ITEM_LEGAL_G2) / sizeof(ITEM_LEGAL_G2[0]);\n"
        "        while (low < high)\n        {\n"
        "            const size_t mid = (low + high) / 2;\n"
        "            if (ITEM_LEGAL_G2[mid] == itemId)\n            {\n                return true;\n            }\n"
        "            if (ITEM_LEGAL_G2[mid] < itemId)\n            {\n                low = mid + 1;\n            }\n"
        "            else\n            {\n                high = mid;\n            }\n        }\n"
        "        return false;\n    }\n}\n")
    with open(os.path.join(INC, "Gen2Tables.h"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write("".join(header))
    with open(os.path.join(SRC, "Gen2Tables.cpp"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write("".join(source))
    print(f"  Gen2Tables  {len(names)} item names, {len(legal)} legal ids")


def main():
    total = 0
    for entry in GROUPS:
        lay = layout(entry)
        with open(pkhex_path("Resources/byte/personal/" + lay["resource"]), "rb") as fh:
            raw = fh.read()
        size = lay["size"]
        if len(raw) % size:
            raise SystemExit(f"{lay['code']}: {lay['resource']} is {len(raw)} bytes, not a multiple "
                             f"of the entry size 0x{size:X} -- the size is wrong.")
        entries = [raw[i * size:(i + 1) * size] for i in range(len(raw) // size)]
        maxSpecies = lay["maxSpecies"]
        if len(entries) <= maxSpecies:
            raise SystemExit(f"{lay['code']}: {lay['resource']} holds {len(entries)} rows but the "
                             f"group claims species up to {maxSpecies}.")
        rows = [decode(lay, entries[i]) for i in range(maxSpecies + 1)]
        formRows = [decode(lay, entries[i]) for i in range(maxSpecies + 1, len(entries))]
        present = sanity(lay["code"], rows)
        count = emit(lay, rows, formRows)
        total += count
        print(f"  {lay['code']:<7} {lay['resource']:<15} {present:>4} of {maxSpecies:>4} species "
              f"present + {len(formRows):>4} form rows = {count:>5} rows")
    emit_gen2_items()
    # Printed, never embedded. PKSE tracks PKHeX's default branch, so stamping the
    # commit into every generated header would rewrite all nineteen files on every run
    # and bury the only question a regeneration diff has to answer: did the DATA move?
    print(f"\n{len(GROUPS)} groups, {total} rows total, from PKHeX {pkhex_ref()}.")


if __name__ == "__main__":
    main()
