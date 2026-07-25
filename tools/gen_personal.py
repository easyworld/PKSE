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
        return dict(a1=self._ab(e, self.a1), a2=self._ab(e, self.a2), ah=self._ab(e, self.ah),
                    gender=e[self.g], friendship=e[self.fr],
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
    return ("        {{ {a1:>4}, {a2:>4}, {ah:>4}, {fi:>5}, {g:>4}, {fr:>4}, {fc:>3}, 0x{pr:02X}, {h:>4}, {w:>5} }},"
            "  // {c}\n").format(
        a1=r["a1"], a2=r["a2"], ah=r["ah"], fi=r["formIndex"],
        g=r["gender"], fr=r["friendship"], fc=r["formCount"], pr=r["presence"],
        h=r["height"], w=r["weight"], c=comment)


def main():
    names, base_rows, alt_rows = build()
    total = len(base_rows) + len(alt_rows)

    # Header
    with open(OUT_H, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(HDR.format(MAX=MAX_SPECIES, COUNT=total, BASE=BASE_ROWS))

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
             ' *            baseFriendship, formCount, presence, height, weight }\n'
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
    p.append("    }\n")
    p.append("}\n")

    with open(OUT_CPP, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("".join(p))

    print("Wrote", OUT_H)
    print("Wrote", OUT_CPP)
    print(f"  base rows      : {len(base_rows)} (species 0..{MAX_SPECIES})")
    print(f"  alt-form rows  : {len(alt_rows)}")
    print(f"  TOTAL rows     : {total}")


if __name__ == "__main__":
    main()
