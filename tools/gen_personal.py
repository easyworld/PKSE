#!/usr/bin/env python3
"""Generate include/Pokemon/PersonalInfoTable.h + src/Pokemon/PersonalInfoTable.cpp
from PKHeX's binary personal tables.

PKSE's BaseStatsGen89 table is stats-only; this fills the gap with the per-species
"personal info" PKSE lacks: the three ability slots, the raw gender byte, base
friendship, form count, and a per-game presence bitmask.

Species-level fields (ability1/2/H, gender, friendship) come from the most COMPLETE
PKHeX personal table -- Scarlet/Violet (personal_sv), which spans the full National
Dex 0..1025 with correct data for every species even when the species is not
obtainable in S/V. Alternate forms are enumerated as the UNION of forms across all
six supported games (S/V does not carry the Mega-Evolution form slots that Legends:
Z-A adds); each form row's fields are filled from the first game (priority
SV > ZA > SWSH > PLA > BDSP > GG) whose personal table actually defines that form.
Form indices are globally consistent across these games (verified: 0 ability
mismatches across 377 shared S/V-vs-Z-A alternate forms), so this union is safe.

The per-game presence bitmask (one bit per game: GG, SWSH, BDSP, PLA, SV, ZA) is
computed per (species, form) by replicating each game's PersonalTable*.IsPresentInGame:
  * GG   -- species 1..151 or Meltan/Melmetal; Pikachu only form 8; else HasForm.
  * BDSP -- species <= 493; form 0 always present; else HasForm (no per-entry flag).
  * SWSH / PLA / SV / ZA -- the per-entry IsPresentInGame flag, gated by HasForm.

Forms are addressed exactly like PKHeX's PersonalInfo.FormIndex: form 0 -> Table[species];
form N (0 < N < FormCount, FormStatsIndex > 0) -> Table[FormStatsIndex + N - 1]. The
emitted table mirrors this with its own `formIndex` redirection (see getPersonalInfo).

Per-format byte offsets (verified against PKHeX.Core/PersonalInfo/Info/PersonalInfo*.cs):
                SIZE  ab-width  Ability1  Ability2  AbilityH  Gender  Friend  FormIdx  FormCnt  Present
  GG (7GG)      0x54     u8      0x18      0x19      0x1A     0x12    0x14    0x1C     0x20    (special)
  SWSH (8SWSH)  0xB0     u16     0x18      0x1A      0x1C     0x12    0x14    0x1E     0x20    bit6@0x21
  BDSP (8BDSP)  0x44     u16     0x18      0x1A      0x1C     0x12    0x14    0x1E     0x20    (species<=493)
  PLA (8LA)     0xB0     u16     0x18      0x1A      0x1C     0x12    0x14    0x1E     0x20    bit6@0x21
  SV (9SV)      0x50     u16     0x12      0x14      0x16     0x0C    0x0E    0x18     0x1A    byte@0x1C
  ZA (9ZA)      0x50     u16     0x12      0x14      0x16     0x0C    0x0E    0x18     0x1A    byte@0x1C

NOTE on ability slots: PKHeX stores "no second ability" as a DUPLICATE of Ability1
(e.g. Bulbasaur = Overgrow/Overgrow/Chlorophyll), not 0. These values are emitted
verbatim from PKHeX; a consumer treats "ability2 == ability1" as "single ability".

TYPES live at 0x06 / 0x07 in ALL FIVE modern formats above (the stat block is the same
six bytes in each, and the type pair follows it), so they need no per-format offsets.
They are emitted PER FORM like everything else here, which is the point: types are one
of the things a regional form actually changes -- Hisuian Braviary is Psychic/Flying
where the Unovan one is Normal/Flying. PKHeX again stores a single-typed species as
Type2 == Type1; that is normalised to 255 ("no second type") on the way out, matching
PKSE's TYPE_NONE.

GEN 3 TYPES need their own table for the same reason its abilities do: Gen 3 predates the
Fairy type, so 18 of its 386 species are typed differently there -- Clefairy is Normal in
FireRed and Fairy in Scarlet/Violet, Marill is pure Water, Gardevoir is pure Psychic.
Reading the modern row for a Gen 3 mon claims a type that game has never heard of.

The ENCODING needs no conversion, though: PKHeX's personal_fr already stores Gen 6+ type
ids rather than the raw Gen 3 ones (Charmander is 9 = Fire there, not the in-game 10 --
Gen 3 spends id 9 on the unused "???" type and shifts everything above it). Verified by
histogram: personal_fr uses ids 0..16 with no 17, i.e. the modern numbering minus Fairy.

GENERATION 3 gets its OWN ability table (personal_fr, PersonalInfo3, SIZE 0x1C,
Ability1 @0x16 / Ability2 @0x17, both u8, indexed straight by National Dex id --
PersonalTable3.GetFormIndex is `species`, so Gen 3 forms share one row). The S/V
slots above are NOT usable for FireRed/LeafGreen: 101 of the 386 Gen 3 species have
a different slot pair there (Sableye is Keen Eye alone in Gen 3, Keen Eye/Stall in
S/V) and 355 of them have an S/V hidden ability, a slot Gen 3 does not have at all.
A PK3 stores only a BIT, and the game resolves it through its own table, so offering
an S/V-only ability for a Gen 3 mon writes a bit that displays as something else.
FireRed and LeafGreen carry identical ability data (verified: 0 differences), so one
table serves both.

Regenerate:  python tools/gen_personal.py
Pulls the PKHeX personal binaries it reads from GitHub on demand
(tools/pkhex_source.py); no local PKHeX checkout required.
"""
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pkhex_source import pkhex_path  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_H = os.path.join(ROOT, "include", "Pokemon", "PersonalInfoTable.h")
OUT_CPP = os.path.join(ROOT, "src", "Pokemon", "PersonalInfoTable.cpp")
SPECIES_NAMES_SRC = os.path.join(ROOT, "src", "Names", "SpeciesNames.cpp")

MAX_SPECIES = 1025            # National Dex #1025 Pecharunt (PKHeX MaxSpeciesID_9)
BASE_ROWS = MAX_SPECIES + 1   # indices 0..1025

# Generation 3 ability table (PersonalInfo3). Indexed by National Dex id, forms share
# a row, abilities are single bytes. FR and LG are byte-identical here.
G3_MAX_SPECIES = 386          # PKHeX MaxSpeciesID_3 (Deoxys)
G3_SIZE = 0x1C                # PersonalInfo3.SIZE
G3_AB1, G3_AB2 = 0x16, 0x17

# Type pair. Same offsets in every modern format AND in PersonalInfo3 -- the six stat
# bytes come first in all of them and the types follow. All of these tables (personal_fr
# included) are already in Gen 6+ type numbering, so no id conversion is needed anywhere;
# check_g3_type_encoding() below enforces that rather than assuming it.
TYPE1, TYPE2 = 0x06, 0x07
TYPE_NONE = 255               # PKSE's "single-typed"; PKHeX duplicates Type1 instead
TYPE_FIRE = 9                 # Gen 6+ id; the raw Gen 3 id for Fire is 10
CHARMANDER = 4

# National Dex numbers used by the GG (Let's Go) presence rule.
MELTAN, MELMETAL, PIKACHU = 808, 809, 25

# Game -> presence bit (order defines the bitmask layout emitted into the header).
GAMES = ["GG", "SWSH", "BDSP", "PLA", "SV", "ZA"]
GAME_BIT = {g: 1 << i for i, g in enumerate(GAMES)}

# When a (species, form) exists in more than one game, fields are taken from the
# first game in this order that actually defines the form. SV is authoritative.
PRIORITY = ["SV", "ZA", "SWSH", "PLA", "BDSP", "GG"]

# key -> (filename, SIZE, ab_width, ab1, ab2, abH, gender, friend, formIdx, formCnt, present_kind,
#         max_species, height, weight)  -- Height/Weight are u16 base values (LGPE absolute size).
FMT = {
    "GG":   ("personal_gg",   0x54, 1, 0x18, 0x19, 0x1A, 0x12, 0x14, 0x1C, 0x20, "gg",     809,  0x24, 0x26),
    "SWSH": ("personal_swsh", 0xB0, 2, 0x18, 0x1A, 0x1C, 0x12, 0x14, 0x1E, 0x20, "flag21", 898,  0x24, 0x26),
    "BDSP": ("personal_bdsp", 0x44, 2, 0x18, 0x1A, 0x1C, 0x12, 0x14, 0x1E, 0x20, "bdsp",   493,  0x24, 0x26),
    "PLA":  ("personal_la",   0xB0, 2, 0x18, 0x1A, 0x1C, 0x12, 0x14, 0x1E, 0x20, "flag21", 905,  0x24, 0x26),
    "SV":   ("personal_sv",   0x50, 2, 0x12, 0x14, 0x16, 0x0C, 0x0E, 0x18, 0x1A, "flag1C", 1025, 0x20, 0x22),
    "ZA":   ("personal_za",   0x50, 2, 0x12, 0x14, 0x16, 0x0C, 0x0E, 0x18, 0x1A, "flag1C", 1000, 0x20, 0x22),
}


class Table:
    """One game's personal binary, decoded per the format spec above."""

    def __init__(self, key):
        (fn, size, aw, a1, a2, ah, g, fr, fi, fc, pk, maxsp, hh, ww) = FMT[key]
        self.key, self.SIZE, self.aw = key, size, aw
        self.a1, self.a2, self.ah = a1, a2, ah
        self.g, self.fr, self.fi, self.fc = g, fr, fi, fc
        self.pk, self.maxsp = pk, maxsp
        self.hh, self.ww = hh, ww
        with open(pkhex_path("Resources/byte/personal/" + fn), "rb") as fh:
            self.raw = fh.read()
        if len(self.raw) % size != 0:
            raise SystemExit(f"{key}: {len(self.raw)} not a multiple of SIZE {size}")
        self.count = len(self.raw) // size

    def _ent(self, idx):
        return self.raw[idx * self.SIZE:(idx + 1) * self.SIZE]

    def _u16(self, e, off):
        return struct.unpack_from("<H", e, off)[0]

    def _ab(self, e, off):
        return e[off] if self.aw == 1 else self._u16(e, off)

    def form_stats_index(self, species):
        return self._u16(self._ent(species), self.fi)

    def form_count(self, species):
        return self._ent(species)[self.fc]

    def has_form(self, species, form):
        # PersonalInfo.HasForm: form>0, FormStatsIndex>0, form<FormCount
        return form != 0 and self.form_stats_index(species) > 0 and form < self.form_count(species)

    def form_index(self, species, form):
        # PersonalInfo.FormIndex redirection
        if species > self.maxsp or species >= self.count:
            return 0
        if not self.has_form(species, form):
            return species
        return self.form_stats_index(species) + form - 1

    def defines_form(self, species, form):
        """True if this game's table has a real entry for (species, form)."""
        if species > self.maxsp or species >= self.count:
            return False
        return form == 0 or self.has_form(species, form)

    def fields(self, species, form):
        e = self._ent(self.form_index(species, form))
        t1, t2 = e[TYPE1], e[TYPE2]
        return dict(a1=self._ab(e, self.a1), a2=self._ab(e, self.a2), ah=self._ab(e, self.ah),
                    gender=e[self.g], friendship=e[self.fr],
                    t1=t1, t2=(TYPE_NONE if t2 == t1 else t2),
                    height=self._u16(e, self.hh), weight=self._u16(e, self.ww))

    def present(self, species, form):
        """Replicate this game's PersonalTable*.IsPresentInGame(species, form)."""
        if species > self.maxsp:
            return False
        if self.pk == "gg":
            if not ((1 <= species <= 151) or species in (MELTAN, MELMETAL)):
                return False
            if form == 0:
                return True
            if species == PIKACHU:
                return form == 8
            return self.has_form(species, form)
        if self.pk == "bdsp":
            if form == 0:
                return True
            return self.has_form(species, form)
        # flag1C / flag21: per-entry IsPresentInGame flag, gated by HasForm for forms.
        if form != 0 and not self.has_form(species, form):
            return False
        e = self._ent(self.form_index(species, form))
        if self.pk == "flag1C":
            return e[0x1C] != 0
        return ((e[0x21] >> 6) & 1) == 1


def load_species_names():
    """Parse the flat SPECIES_NAMES[] string array out of PKSE's SpeciesNames.cpp."""
    with open(SPECIES_NAMES_SRC, encoding="utf-8") as fh:
        text = fh.read()
    m = re.search(r"SPECIES_NAMES\[\]\s*=\s*\{(.*?)\};", text, re.S)
    if not m:
        raise SystemExit("could not locate SPECIES_NAMES[] in " + SPECIES_NAMES_SRC)
    names = re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1))
    if len(names) <= MAX_SPECIES:
        raise SystemExit(f"SpeciesNames only has {len(names)} entries; need > {MAX_SPECIES}")
    return names


def build():
    tables = {k: Table(k) for k in FMT}
    names = load_species_names()

    # Union form count per species: the max FormCount any supported game defines.
    union_fc = [1] * BASE_ROWS
    for sp in range(BASE_ROWS):
        fc = 1
        for t in tables.values():
            if sp <= t.maxsp and sp < t.count:
                fc = max(fc, t.form_count(sp))
        union_fc[sp] = fc

    def pick_fields(sp, form):
        for k in PRIORITY:
            if tables[k].defines_form(sp, form):
                return tables[k].fields(sp, form)
        # No game defines this form (shouldn't happen for form 0); fall back to SV form 0.
        return tables["SV"].fields(sp, 0)

    def presence(sp, form):
        mask = 0
        for g in GAMES:
            if tables[g].present(sp, form):
                mask |= GAME_BIT[g]
        return mask

    def make_row(sp, form, form_index):
        f = pick_fields(sp, form)
        return dict(sp=sp, form=form, a1=f["a1"], a2=f["a2"], ah=f["ah"],
                    gender=f["gender"], friendship=f["friendship"],
                    t1=f["t1"], t2=f["t2"],
                    formCount=union_fc[sp], formIndex=form_index,
                    presence=presence(sp, form), height=f["height"], weight=f["weight"])

    # Base rows 0..1025 first (so TABLE[species] indexes the form-0 entry), then a
    # contiguous block of alternate-form rows. Each base row's formIndex points at
    # where its form-1 row lives (0 if the species has no alternate forms).
    base_rows = [None] * BASE_ROWS
    alt_rows = []
    next_index = BASE_ROWS
    for sp in range(BASE_ROWS):
        fc = union_fc[sp]
        fi = 0
        if fc > 1:
            fi = next_index
            for form in range(1, fc):
                alt_rows.append(make_row(sp, form, 0))
            next_index += fc - 1
        base_rows[sp] = make_row(sp, 0, fi)

    return names, base_rows, alt_rows


def build_g3():
    """Gen 3 (species, ability1, ability2, type1, type2) rows for National Dex 0..386.

    Cross-checks FireRed against LeafGreen and refuses to emit if they ever disagree
    -- one table stands in for both, so a divergence has to be caught here.
    """
    with open(pkhex_path("Resources/byte/personal/personal_fr"), "rb") as fh:
        fr = fh.read()
    with open(pkhex_path("Resources/byte/personal/personal_lg"), "rb") as fh:
        lg = fh.read()
    need = (G3_MAX_SPECIES + 1) * G3_SIZE
    for tag, raw in (("personal_fr", fr), ("personal_lg", lg)):
        if len(raw) < need:
            raise SystemExit(f"{tag}: {len(raw)} bytes, need >= {need}")

    rows = []
    for sp in range(G3_MAX_SPECIES + 1):
        o = sp * G3_SIZE
        a1, a2 = fr[o + G3_AB1], fr[o + G3_AB2]
        if (a1, a2) != (lg[o + G3_AB1], lg[o + G3_AB2]):
            raise SystemExit(f"species {sp}: FR/LG ability mismatch -- they need separate tables")
        t1, t2 = fr[o + TYPE1], fr[o + TYPE2]
        if (t1, t2) != (lg[o + TYPE1], lg[o + TYPE2]):
            raise SystemExit(f"species {sp}: FR/LG type mismatch -- they need separate tables")
        rows.append((sp, a1, a2, t1, TYPE_NONE if t2 == t1 else t2))

    # personal_fr must already be in Gen 6+ type numbering, not raw Gen 3 ids. Pin it on
    # Charmander (pure Fire -> 9 modern, 10 raw) and on the absence of Fairy, which is the
    # only id the two numberings put out of range of each other.
    o = CHARMANDER * G3_SIZE
    if fr[o + TYPE1] != TYPE_FIRE:
        raise SystemExit(f"personal_fr: Charmander type is {fr[o + TYPE1]}, expected {TYPE_FIRE} "
                         "-- the table is in raw Gen 3 ids and now needs a remap")
    worst = max(max(t1, t2) for _, _, _, t1, t2 in rows if t2 != TYPE_NONE)
    if worst > 16:
        raise SystemExit(f"personal_fr: type id {worst} > 16 -- Gen 3 has no Fairy, so this is "
                         "either raw Gen 3 numbering or a bad parse")
    return rows


HDR = '''/**
 * PersonalInfoTable.h - Per-species "personal info" (abilities / gender / friendship
 *                       / form count / per-game presence)
 *
 * Auto-generated by tools/gen_personal.py from PKHeX's binary personal tables.
 * DO NOT EDIT BY HAND -- rerun the generator instead.
 *
 * Species-level fields come from PKHeX's Scarlet/Violet table (the most complete,
 * spanning National Dex 0..{MAX}); alternate forms are the union across the six
 * supported games. See the generator's module docstring for offsets and provenance.
 */
#ifndef PKM_PERSONAL_INFO_TABLE_H
#define PKM_PERSONAL_INFO_TABLE_H

#include <cstdint>
#include <cstddef>

namespace Pokemon {{
    // Per-game presence bits packed into PersonalInfo::presence.
    enum PersonalGameBit : uint8_t {{
        PERSONAL_GAME_GG   = 1 << 0,  // Let's Go, Pikachu! / Eevee!
        PERSONAL_GAME_SWSH = 1 << 1,  // Sword / Shield
        PERSONAL_GAME_BDSP = 1 << 2,  // Brilliant Diamond / Shining Pearl
        PERSONAL_GAME_PLA  = 1 << 3,  // Legends: Arceus
        PERSONAL_GAME_SV   = 1 << 4,  // Scarlet / Violet
        PERSONAL_GAME_ZA   = 1 << 5,  // Legends: Z-A
    }};

    // One entry per (species, form). Ability IDs are National ability indices (same
    // space as PKSE's Names::ABILITY_NAMES). genderRatio is the raw PKHeX gender byte:
    //   0   = always male      254 = always female      255 = genderless
    //   else = threshold; PID gender-value < genderRatio means female.
    // PKHeX stores "no second ability" as ability2 == ability1 (not 0).
    struct PersonalInfo {{
        uint16_t ability1;
        uint16_t ability2;
        uint16_t abilityHidden;
        uint16_t formIndex;       // table index of this species' form 1 (0 = no alt forms)
        uint8_t  genderRatio;
        uint8_t  baseFriendship;
        uint8_t  formCount;       // number of forms across supported games; valid forms are 0..formCount-1
        uint8_t  presence;        // OR of PersonalGameBit: which games this species+form exists in
        uint8_t  type1;           // Gen 6+ type ids -- the PokemonTypes.h TYPE_* constants
        uint8_t  type2;           // TYPE_NONE (255) when single-typed
        uint16_t height;          // PKHeX personal Height (base) -- used for the LGPE absolute size
        uint16_t weight;          // PKHeX personal Weight (base)
    }};

    constexpr uint16_t PERSONAL_MAX_SPECIES = {MAX};
    constexpr size_t   PERSONAL_INFO_TABLE_COUNT = {COUNT};

    // Flat table: indices 0..{MAX} are the form-0 (base) entries; {BASE}.. are the
    // alternate-form entries a base row's formIndex redirects into.
    extern const PersonalInfo PERSONAL_INFO_TABLE[PERSONAL_INFO_TABLE_COUNT];

    // Resolve a (species, form) to its entry, mirroring PKHeX's FormIndex redirection.
    // Out-of-range species or forms fall back to the species' form-0 (or index 0) entry.
    const PersonalInfo& getPersonalInfo(uint16_t species, uint8_t form);

    // ---- Generation 3 (FireRed/LeafGreen) abilities + types ----
    // Gen 3's slot pair is NOT the one above: the S/V row disagrees for 101 of these
    // species and always carries a hidden ability, which Gen 3 has no slot for. A PK3
    // stores only a selector BIT and the game resolves it through its own table, so
    // this is the only table that describes what a Gen 3 mon can actually hold.
    // Forms share a row (PKHeX PersonalTable3.GetFormIndex is the species id).
    //
    // Its TYPES disagree with the modern table too, for {G3TYPEDIFF} of the {G3MAX} species: Gen 3
    // predates the Fairy type, so Clefairy is Normal there and Fairy in Scarlet/Violet.
    // These are the SAME Gen 6+ type ids used above -- only the values differ, not the
    // encoding -- so a caller switches table without translating.
    struct PersonalAbilityG3 {{
        uint8_t ability1;
        uint8_t ability2;   // == ability1 when the species has only one ability
        uint8_t type1;
        uint8_t type2;      // TYPE_NONE (255) when single-typed
    }};

    constexpr uint16_t PERSONAL_G3_MAX_SPECIES = {G3MAX};

    extern const PersonalAbilityG3 PERSONAL_ABILITY_G3[PERSONAL_G3_MAX_SPECIES + 1];

    // Gen 3 ability slots + types for a species. Out-of-range species return all-zero
    // abilities and Normal/none, the same shape the modern lookup falls back to.
    const PersonalAbilityG3& getPersonalAbilityG3(uint16_t species);
}}

#endif  // PKM_PERSONAL_INFO_TABLE_H
'''


def fmt_row(r, names):
    sp = r["sp"]
    name = names[sp] if sp < len(names) else "?"
    if r["form"] == 0:
        if r["formCount"] > 1:
            comment = f"{sp} {name}  ({r['formCount']} forms @ {r['formIndex']})"
        else:
            comment = f"{sp} {name}"
    else:
        comment = f"{sp} {name} form {r['form']}"
    return ("        {{ {a1:>4}, {a2:>4}, {ah:>4}, {fi:>5}, {g:>4}, {fr:>4}, {fc:>3}, 0x{pr:02X},"
            " {t1:>3}, {t2:>4}, {h:>4}, {w:>5} }},  // {c}\n").format(
        a1=r["a1"], a2=r["a2"], ah=r["ah"], fi=r["formIndex"],
        g=r["gender"], fr=r["friendship"], fc=r["formCount"], pr=r["presence"],
        t1=r["t1"], t2=r["t2"], h=r["height"], w=r["weight"], c=comment)


def main():
    names, base_rows, alt_rows = build()
    g3_rows = build_g3()
    total = len(base_rows) + len(alt_rows)

    # How far Gen 3 typing drifts from the modern table -- reported, and baked into the
    # header comment, so the reason this second table exists stays visible.
    g3_type_diff = sum(1 for sp, _, _, t1, t2 in g3_rows
                       if sp and (t1, t2) != (base_rows[sp]["t1"], base_rows[sp]["t2"]))

    # Header
    with open(OUT_H, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(HDR.format(MAX=MAX_SPECIES, COUNT=total, BASE=BASE_ROWS,
                            G3MAX=G3_MAX_SPECIES, G3TYPEDIFF=g3_type_diff))

    # Source
    p = []
    p.append('/**\n'
             ' * PersonalInfoTable.cpp - Per-species personal info table.\n'
             ' *\n'
             ' * Auto-generated by tools/gen_personal.py from PKHeX\'s binary personal tables.\n'
             ' * DO NOT EDIT BY HAND -- rerun the generator instead.\n'
             ' *\n'
             ' * Layout: rows 0..%d are the form-0 (base) entries indexed directly by National\n'
             ' * Dex species id; rows %d.. are alternate forms. A base row\'s formIndex points at\n'
             ' * its form-1 row; form N resolves to formIndex + N - 1 (see getPersonalInfo).\n'
             ' *\n'
             ' * Columns: { ability1, ability2, abilityHidden, formIndex, genderRatio,\n'
             ' *            baseFriendship, formCount, presence, type1, type2, height, weight }\n'
             ' */\n\n' % (MAX_SPECIES, BASE_ROWS))
    p.append('#include "Pokemon/PersonalInfoTable.h"\n\n')
    p.append("namespace Pokemon {\n\n")
    p.append("    const PersonalInfo PERSONAL_INFO_TABLE[PERSONAL_INFO_TABLE_COUNT] = {\n")
    p.append("        // ---- Base (form 0) entries, indexed by National Dex species id ----\n")
    for r in base_rows:
        p.append(fmt_row(r, names))
    p.append("\n        // ---- Alternate-form entries (targets of formIndex redirection) ----\n")
    for r in alt_rows:
        p.append(fmt_row(r, names))
    p.append("    };\n\n")

    p.append("    const PersonalInfo& getPersonalInfo(uint16_t species, uint8_t form) {\n")
    p.append("        if (species > PERSONAL_MAX_SPECIES)\n")
    p.append("            return PERSONAL_INFO_TABLE[0];\n")
    p.append("        const PersonalInfo& base = PERSONAL_INFO_TABLE[species];\n")
    p.append("        if (form == 0 || base.formIndex == 0 || form >= base.formCount)\n")
    p.append("            return base;\n")
    p.append("        return PERSONAL_INFO_TABLE[base.formIndex + form - 1];\n")
    p.append("    }\n\n")

    # ---- Gen 3 ability + type table ----
    p.append("    // Generation 3 (FireRed/LeafGreen) ability slots and types, from PKHeX's personal_fr.\n")
    p.append("    // Indexed by National Dex id; ability2 == ability1 means \"single ability\", and\n")
    p.append("    // type2 == 255 means \"single type\". Type ids are the same Gen 6+ numbering used\n")
    p.append("    // everywhere else in PKSE -- Gen 3 differs in its VALUES (no Fairy), not its encoding.\n")
    p.append("    const PersonalAbilityG3 PERSONAL_ABILITY_G3[PERSONAL_G3_MAX_SPECIES + 1] = {\n")
    for sp, a1, a2, t1, t2 in g3_rows:
        name = names[sp] if sp < len(names) else "?"
        p.append("        {{ {a1:>3}, {a2:>3}, {t1:>3}, {t2:>4} }},  // {sp} {n}\n".format(
            a1=a1, a2=a2, t1=t1, t2=t2, sp=sp, n=name))
    p.append("    };\n\n")
    p.append("    const PersonalAbilityG3& getPersonalAbilityG3(uint16_t species) {\n")
    p.append("        static const PersonalAbilityG3 none = { 0, 0, 0, 255 };\n")
    p.append("        if (species > PERSONAL_G3_MAX_SPECIES)\n")
    p.append("            return none;\n")
    p.append("        return PERSONAL_ABILITY_G3[species];\n")
    p.append("    }\n")
    p.append("}\n")

    with open(OUT_CPP, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("".join(p))

    dual3 = sum(1 for _, a1, a2, _, _ in g3_rows if a1 != a2)
    # Forms whose type pair differs from their own species' form 0 -- exactly the ones a
    # species-keyed type table gets wrong.
    form_type_diff = sum(1 for r in alt_rows
                         if (r["t1"], r["t2"]) != (base_rows[r["sp"]]["t1"], base_rows[r["sp"]]["t2"]))
    print("Wrote", OUT_H)
    print("Wrote", OUT_CPP)
    print(f"  base rows      : {len(base_rows)} (species 0..{MAX_SPECIES})")
    print(f"  alt-form rows  : {len(alt_rows)}")
    print(f"  TOTAL rows     : {total}")
    print(f"  Gen 3 ability  : {len(g3_rows)} rows (species 0..{G3_MAX_SPECIES}), "
          f"{dual3} with two distinct abilities")
    print(f"  types          : {form_type_diff} alternate forms retype their base species; "
          f"{g3_type_diff} species are typed differently in Gen 3")


if __name__ == "__main__":
    main()
