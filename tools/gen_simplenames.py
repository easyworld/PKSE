#!/usr/bin/env python3
"""Generate the three flat id-indexed name tables: abilities, natures and types.

One generator rather than three because the tables are the same shape -- a flat PKHeX text list
whose line number IS the id -- and three near-identical scripts is how two of them end up with
different escaping or a different index-0 convention.

These replace HAND-WRITTEN tables. Before the swap, PKSE's English tables were compared against
PKHeX's line by line: types and natures were already byte-identical, and abilities differed at
exactly one entry -- index 0, where PKSE says "None" and PKHeX says an em dash. That substitution
is kept deliberately (the same choice gen_speciesnames.py makes at index 0, where PKHeX says
"Egg"), so the only thing this changes for an English user is nothing at all.

TYPES EMIT ALL 19, AND THE NINETEENTH IS REACHABLE ONLY AS A TERA TYPE. Index 18 is Stellar, which
no species and no move has, so the type space a species is drawn from is still 18. That is why
there are two counts and they must not be merged: `Names::getTypeCount()` (18) bounds what a
SPECIES can be, and is what bounds a personal entry's `type1`/`type2` -- widening it would let a
corrupt type of 18 pass a check that catches it today. `Names::getTypeNameCount()` (19) bounds what
can be NAMED or DRAWN, which is what the Tera type and its icon need.

The stored value is not the table index: PK9 writes Stellar as **99**, not 18 (PKHeX
`TeraTypeUtil.Stellar`), with 19 meaning "not overridden". `Pokemon::teraTypeNameIndex()` is the one
place that maps the byte onto this table.

Regenerate:  python tools/gen_simplenames.py
Pulls the PKHeX text resources from GitHub on demand (tools/pkhex_source.py); no local checkout.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pkhex_source import pkhex_path  # noqa: E402
from name_languages import LANGUAGES, ENGLISH_INDEX, emit_tables  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

TYPE_NOTES = [
    "Index 18 is STELLAR, which is a TERA type and nothing else: no species and no move",
    "has it, so Names::getTypeCount() still answers 18 and is what bounds personal.type1",
    "in the tables suite. Names::getTypeNameCount() answers 19 and is what bounds anything",
    "that names or draws a type, the Tera type included. Those are two different questions",
    "and conflating them would let a corrupt personal type of 18 pass a check that catches",
    "it today. NOTE THE STORED VALUE IS NOT 18: PK9 writes Stellar as 99 (PKHeX",
    "TeraTypeUtil.Stellar) -- Pokemon::teraTypeNameIndex() maps the byte onto this table.",
]

# (output file, PKHeX resource stem, C++ table base, getter, id type, out-of-range sentinel,
#  index-0 override or None, extra header comment lines)
TABLES = [
    ("AbilityNames.cpp", "Abilities", "ABILITY_NAMES", "getAbilityName", "uint16_t",
     "Unknown", "None", []),
    ("NatureNames.cpp", "Natures", "NATURE_NAMES", "getNatureName", "uint8_t",
     "Unknown", None, []),
    ("TypeNames.cpp", "Types", "TYPE_NAMES", "getTypeName", "uint8_t",
     "???", None, TYPE_NOTES),
]


def load_entries(path, indexZeroOverride):
    with open(path, encoding="utf-8") as handle:
        lines = handle.read().split("\n")
    if lines and lines[-1] == "":
        lines = lines[:-1]
    if not lines:
        raise SystemExit("text resource was empty: " + path)
    if indexZeroOverride is not None:
        lines[0] = indexZeroOverride
    return lines


def main():
    for outName, stem, base, getter, idType, sentinel, zeroOverride, notes in TABLES:
        template = "Resources/text/other/{lang}/text_%s_{lang}.txt" % stem
        perLanguage = [load_entries(pkhex_path(template.format(lang=lang)), zeroOverride)
                       for lang in LANGUAGES]

        lines = []
        lines.append("// AUTO-GENERATED from PKHeX's %s text (id = array index).\n" % stem.lower())
        lines.append("// Source: PKHeX.Core/%s, one table per language.\n" % template)
        lines.append("// Regenerate with tools/gen_simplenames.py (see tools/pkhex_source.py).\n")
        if zeroOverride is not None:
            lines.append('// Index 0 is "%s" (PKSE\'s spelling), where PKHeX has an em dash.\n' % zeroOverride)
        for note in notes:
            lines.append("// %s\n" % note)
        lines.append("#include <cstdint>\n")
        lines.append("#include <cstddef>\n")
        lines.append("\n")
        lines.append('#include "Names/NameLanguage.h"\n')
        lines.append("\n")
        lines.append("namespace Names\n")
        lines.append("{\n")
        emit_tables(lines, base, perLanguage)
        lines.append("    const char *%s(%s id)\n" % (getter, idType))
        lines.append("    {\n")
        lines.append("        constexpr size_t count = sizeof(%s_EN) / sizeof(%s_EN[0]);\n" % (base, base))
        lines.append('        if (id >= count) return "%s";\n' % sentinel)
        lines.append("        return %s_BY_LANGUAGE[displayLanguageIndex()][id];\n" % base)
        lines.append("    }\n")
        lines.append("}\n")

        outPath = os.path.join(ROOT, "src", "Names", outName)
        with open(outPath, "w", encoding="utf-8", newline="\n") as handle:
            handle.write("".join(lines))
        print("Wrote", outPath, "with", len(perLanguage[ENGLISH_INDEX]), "entries x",
              len(LANGUAGES), "languages")


if __name__ == "__main__":
    main()
