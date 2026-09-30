#!/usr/bin/env python3
"""Generate src/Names/FormNamesLocalized.cpp -- form names in the eight non-English languages.

WHY THIS DOES NOT GENERATE ENGLISH. PKSE's English form names are hand-curated in
FormNames.cpp and they are MORE faithful to the games than PKHeX's, which are terse labels for its
own dropdown. Diffed entry by entry, 334 matched and 195 differed, and the games back PKSE nearly
every time: `Alolan` / `Galarian` / `Hisuian` where PKHeX says `Alola` / `Galar` / `Hisui` (53
entries between them), `Plant Cloak` / `Red Flower` / `West Sea` where PKHeX says `Plant` / `Red` /
`West`, and `Male` / `Female` where PKHeX uses the bare gender symbols. Replacing that with PKHeX's
wording would have been a regression dressed as an upgrade -- so the English column stays exactly
where it is, and this supplies only the languages PKSE had nothing for.

(The diff earned its keep the other way too: it found `Poke Ball` missing its accent, which PKHeX
had right and PKSE did not. Fixed in FormNames.cpp, not here.)

HOW THE TWO HALVES STAY IN STEP. `getFormName` asks the English switch FIRST. If English names
nothing for that (species, form) -- form 0 of most species -- every language names nothing, so the
two can never disagree about WHICH forms exist. Only once English has a name is this table
consulted, and a missing row falls back to English rather than showing a blank.

TWO SOURCES, AND POKEAPI WINS WHERE IT HAS THE GAMES' WORDING. PKHeX's form labels are terse in
the LATIN LANGUAGES ONLY: it says `Alola` where the game's Pokedex titles the form `Forme d'Alola`,
`Alola-Form`, `Forma di Alola`, `Forma de Alola`, and `Attack` where the game says `Forme Attaque`.
Measured against PokeAPI on the cells where both have a label, PKHeX matches the game only 18% of
the time in French, 20% in German, Italian and Spanish -- but 67% in Japanese and 83% in Korean,
and its Chinese is already the games' full wording (`\u963f\u7f57\u62c9\u7684\u6837\u5b50`, `\u653b\u51fb\u5f62\u6001`, `\u6c34\u4e95\u9762\u5177`).
So PokeAPI is preferred where it has a row, PKHeX is the fallback, and the CJK columns are mostly
PKHeX's -- PokeAPI carries no Chinese form names at all, which is not a gap to fill here.

HOW A POKEAPI ROW IS MATCHED TO A PKSE ROW. By the ENGLISH LABEL, within ONE SPECIES. A wrong match
can then only ever be another form of the same Pokemon, and two forms of one species that share an
English key are refused rather than guessed -- which is what keeps Minior's seven identically
labelled "Meteor Form" colours out. Three normalisations make the two spellings comparable, and
each is symmetric, so none of them can equate two genuinely different forms:

  * the SPECIES' OWN NAME is removed, per language. PokeAPI's `names` is a full name by design and
    its `form_names` sometimes is too ("Mega Venusaur", "Heat Rotom"); PKSE composes the species
    separately, so leaving it in renders "Florizarre (Mega-Florizarre)". Per language is what makes
    it work for Japanese and Chinese, which have no spaces to split on.
  * the generic form-category NOUN is dropped from both sides -- the one way the two spellings
    differ ("Archipelago" / "Archipelago Pattern", "Baile" / "Baile Style").
  * tokens are SORTED, because the two do not always agree on order ("Core Blue" / "Blue Core").

Then a per-language collision guard runs: PokeAPI's German says "Mega-Form" for BOTH of Charizard's
megas, so that language is dropped for those rows and PKHeX names them apart.

WHAT NEITHER SOURCE CAN NAME. 29 labels, which keep PKHeX's wording: Alcremie's nine creams, the
four Terastal masks and Zygarde's Power Construct forms (PKSE composes those itself, so no single
game string exists), Paldean Tauros's three breeds, Unown's `!` and `?`, and the Legends: Arceus
Lord/Lady bosses. And no source gives a COMPOSED name: the games show the form as a separate label
(the Pokedex titles it "Alola Form") and their running text often drops the regional adjective
entirely, so "Alolan Vulpix" is a wiki convention rather than a game string. Composing one is
therefore PKSE's own affordance; see getDisplayName.

Needs the .NET SDK, because it BUILDS PKHeX.Core (tools/formdump/) rather than reading a file out
of it -- the one generator that needs more than a text fetch. The source is downloaded like every
other PKHeX input; PKHEX_LOCAL points it at a checkout instead, if one is already to hand.

    python tools/gen_formnames.py
    PKHEX_LOCAL=~/repos/PKHeX python tools/gen_formnames.py
"""
import json
import os
import re
import subprocess
import sys
import time
import urllib.error
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from name_languages import LANGUAGES, ENGLISH_INDEX, escape  # noqa: E402
from pkhex_source import pkhex_path, pkhex_ref, pkhex_repo  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "src", "Names", "FormNamesLocalized.cpp")
DUMPER = os.path.join(ROOT, "tools", "formdump")
SPRITE_MAP_SOURCE = os.path.join(ROOT, "tools", "formspritemap.cpp")
POKEAPI_CACHE = os.path.join(ROOT, "tools", ".pokeapi_cache")

# PokeAPI language codes -> this project's, for the ones it carries. It has no Chinese form names.
POKEAPI_LANGUAGES = {"ja": "ja", "en": "en", "fr": "fr", "it": "it", "de": "de",
                     "es": "es", "ko": "ko"}
NATIONAL_DEX_MAX = 1025

# Generic form-category nouns, dropped from BOTH sides of a comparison so PKSE's terse English and
# PokeAPI's noun-carrying English compare equal. Every one of these names a KIND of form, never
# which form -- so removing it can never equate two different forms of a species.
# "flower" is here for Floette's Eternal Flower alone, and it was found by asking the data rather
# than by guessing: sweeping every unmatched row for a same-species PokeAPI label differing by
# exactly one token turned up this word and no other.
CATEGORY_NOUNS = {"form", "forme", "pattern", "style", "mode", "size", "core", "family",
                  "reversion", "cloak", "flower"}
# Separators a species-name removal can leave stranded at either end.
STRIP_CHARACTERS = " \t-\u2010\u2011\u2012\u2013\u2014\u30fb\u00b7:,"

SPECIES_BY_LANGUAGE = None


def pkhex_checkout():
    """The PKHeX.Core project formdump builds against, downloaded unless PKHEX_LOCAL says otherwise.

    This generator needs PKHeX COMPILED rather than read, which is why it is the only one that
    wants the repo instead of a file out of it -- not a reason to make a checkout a precondition.
    """
    root = pkhex_repo()
    core = os.path.join(root, "PKHeX.Core", "PKHeX.Core.csproj")
    if not os.path.isfile(core):
        raise SystemExit("no PKHeX.Core/PKHeX.Core.csproj under %s" % root)
    return root, core


def pkhex_commit(root):
    """What the dumped form list was actually built from, for the emitted file to record.

    A downloaded tarball carries no .git, and `git -C <dir> rev-parse HEAD` does not fail there --
    it walks UP and answers with the enclosing repository, which is PKSE's own. So the resolved ref
    is the answer whenever the source came from GitHub, and git is asked only of a checkout that
    really is the top of its own repository.
    """
    toplevel = subprocess.run(["git", "-C", root, "rev-parse", "--show-toplevel"],
                              capture_output=True, text=True).stdout.strip()
    if toplevel and os.path.realpath(toplevel) == os.path.realpath(root):
        head = subprocess.run(["git", "-C", root, "rev-parse", "HEAD"],
                              capture_output=True, text=True).stdout.strip()
        if head:
            return head
    return pkhex_ref()


def run_dumper(core):
    result = subprocess.run(
        ["dotnet", "run", "-c", "Release", "--project", DUMPER, "-p:PKHexCore=%s" % core],
        capture_output=True, text=True)
    if result.returncode != 0:
        raise SystemExit("formdump failed:\n" + result.stderr[-3000:])
    # dotnet prints build chatter before the JSON; the payload is the last line.
    for line in reversed(result.stdout.split("\n")):
        if line.startswith("{"):
            return json.loads(line)
    raise SystemExit("formdump produced no JSON")


def pokeapi_get(url):
    """Fetch and cache one PokeAPI document. None for a 404, which is a normal answer here."""
    os.makedirs(POKEAPI_CACHE, exist_ok=True)
    key = os.path.join(POKEAPI_CACHE,
                       url.replace("https://pokeapi.co/api/v2/", "").strip("/").replace("/", "_") + ".json")
    if os.path.exists(key):
        try:
            return json.load(open(key, encoding="utf-8"))
        except ValueError:
            pass
    for attempt in range(3):
        try:
            # PokeAPI rejects urllib's default User-Agent with a 403.
            request = urllib.request.Request(url, headers={"User-Agent": "PKSE-name-generator/1.0"})
            with urllib.request.urlopen(request, timeout=30) as response:
                document = json.load(response)
            json.dump(document, open(key, "w", encoding="utf-8"), ensure_ascii=False)
            return document
        except urllib.error.HTTPError as error:
            if error.code == 404:
                json.dump(None, open(key, "w", encoding="utf-8"))
                return None
            time.sleep(2)
        except Exception:
            time.sleep(2)
    raise SystemExit("PokeAPI request failed after 3 attempts: " + url)


def sprite_map():
    """One row per named (species, form): its PokeAPI id, name stem and English form name."""
    binary = os.path.join(POKEAPI_CACHE, "formspritemap")
    os.makedirs(POKEAPI_CACHE, exist_ok=True)
    compile_result = subprocess.run(
        ["g++", "-std=c++20", "-O1", "-I", os.path.join(ROOT, "include"), SPRITE_MAP_SOURCE,
         os.path.join(ROOT, "src", "Names", "FormNames.cpp"),
         os.path.join(ROOT, "src", "Names", "FormNamesLocalized.cpp"),
         os.path.join(ROOT, "src", "Names", "TypeNames.cpp"),
         os.path.join(ROOT, "src", "Names", "NameLanguage.cpp"),
         os.path.join(ROOT, "src", "Pokemon", "FormSpriteMapping.cpp"),
         "-o", binary], capture_output=True, text=True)
    if compile_result.returncode != 0:
        raise SystemExit("could not build formspritemap:\n" + compile_result.stderr[-2000:])
    run_result = subprocess.run([binary], capture_output=True, text=True)
    if run_result.returncode != 0:
        raise SystemExit("formspritemap failed:\n" + run_result.stderr[-2000:])
    return json.loads(run_result.stdout)


def species_names():
    """Per-language species-name lists, so a FULL NAME can be reduced to a form LABEL."""
    global SPECIES_BY_LANGUAGE
    if SPECIES_BY_LANGUAGE is None:
        SPECIES_BY_LANGUAGE = {}
        for lang in LANGUAGES:
            resource = "Resources/text/other/%s/text_Species_%s.txt" % (lang, lang)
            with open(pkhex_path(resource), encoding="utf-8") as handle:
                SPECIES_BY_LANGUAGE[lang] = handle.read().split("\n")
    return SPECIES_BY_LANGUAGE


def strip_species(label, speciesId, lang):
    """Remove the species' own name, in THAT language, from a form label.

    A label carrying the species is a full NAME, not a label -- PokeAPI's `names` is exactly that,
    and even its `form_names` sometimes is ("Mega Venusaur", "Heat Rotom"). PKSE composes the
    species separately in getDisplayName, so leaving it in renders "Florizarre (Mega-Florizarre)".
    Doing it per language is what makes it work for Japanese and Chinese, which have no spaces:
    "\u30e1\u30ac\u30ea\u30b6\u30fc\u30c9\u30f3\uff38" minus "\u30ea\u30b6\u30fc\u30c9\u30f3" is "\u30e1\u30ac\uff38".
    """
    table = species_names().get(lang) or []
    name = table[speciesId] if speciesId < len(table) else ""
    text = re.sub(re.escape(name), "", label, flags=re.IGNORECASE) if name else label
    # Tidy what the removal left behind: a dangling separator, or parentheses now wrapping the whole
    # remainder ("Arceus (Kampf)" -> " (Kampf)" -> "Kampf").
    text = re.sub(r"\s*[-\u2010\u2011\u2012\u2013\u2014]\s*(?=\s|$)", " ", text)
    text = re.sub(r"(^|\s)[-\u2010\u2011\u2012\u2013\u2014]\s*", r"\1", text)
    text = re.sub(r"\(\s*\)", "", text)
    text = re.sub(r"\s+", " ", text).strip(STRIP_CHARACTERS)
    # A REMAINDER THAT IS WHOLLY PARENTHESISED IS POKEAPI'S DISAMBIGUATION, NOT A GAME LABEL.
    # Its German spells Zygarde's forms "Zygarde (10%)" while its English spells the same form
    # "10% Zygarde" -- the brackets are an editorial convention, and unwrapping them yielded a
    # bare "10%" that DISPLACED PKHeX's better "Forma 10%" in Spanish and Italian. Refusing the
    # cell lets PKHeX name it instead.
    if text.startswith("(") and text.endswith(")"):
        return ""
    return text


def form_key(label, speciesId):
    """The comparison key two spellings of one form must share.

    Species-stripped, lowercased, and with the generic form-category nouns dropped -- the ONE way
    PKSE's terse English and PokeAPI's differ is that noun ("Archipelago" / "Archipelago Pattern",
    "Baile" / "Baile Style", "Blue-Striped" / "Blue-Striped Form"). Dropping it from BOTH sides is
    symmetric, so it can only ever equate two labels that differ by that noun alone. Tokens are
    sorted because the two do not always agree on order ("Core Blue" / "Blue Core").
    """
    text = strip_species(label, speciesId, "en").lower().replace("-", " ")
    text = re.sub(r"[^\w\s%']", " ", text)
    return " ".join(sorted(token for token in text.split() if token and token not in CATEGORY_NOUNS))


def extract_names(document, field):
    """The localized strings of one PokeAPI form document, in this project's language codes."""
    names = {}
    for item in document.get(field, []):
        code = item["language"]["name"]
        if code in POKEAPI_LANGUAGES and item["name"]:
            names[POKEAPI_LANGUAGES[code]] = item["name"]
    return names


def sprite_map_slugs(rows):
    """(species, form) -> PokeAPI form slug, through PKSE's OWN verified sprite table.

    This is the most specific join there is: every row of FormSpriteMapping was checked against the
    API's own `name` when the sprite was added, so it names the exact form with no inference. It
    matters where two forms share a label -- Zygarde's "10% Forme" is the form label of BOTH the
    ordinary and the Power Construct row, so the same-species English join refuses them and only
    the id can tell them apart.
    """
    slugs = {}
    speciesSlugs = {}
    byTarget = {}
    for row in rows:
        stem = row.get("stem") or ""
        target = ("stem", stem) if stem else ("id", row["spriteId"])
        byTarget.setdefault(target, []).append(row)
    for target, group in byTarget.items():
        kind, value = target
        if kind == "id":
            pokemon = pokeapi_get("https://pokeapi.co/api/v2/pokemon/%d/" % value)
            slug = pokemon["name"] if pokemon else None
        else:
            speciesId = group[0]["species"]
            if speciesId not in speciesSlugs:
                base = pokeapi_get("https://pokeapi.co/api/v2/pokemon/%d/" % speciesId)
                speciesSlugs[speciesId] = base["name"] if base else None
            baseSlug = speciesSlugs[speciesId]
            suffix = value.split("-", 1)[1] if "-" in value else ""
            slug = "%s-%s" % (baseSlug, suffix) if baseSlug and suffix else None
        if slug:
            for row in group:
                slugs[(row["species"], row["form"])] = slug
    return slugs


def pokeapi_forms(field):
    """PokeAPI's whole form index, keyed by species -> [(english key, slug, localized names)].

    THE JOIN IS PER SPECIES, not global. A wrong match can then only ever be another form of the
    same Pokemon, and two forms of one species sharing an English key are REFUSED rather than
    guessed -- which is what keeps Minior's seven "Meteor Form" colours out.

    `field` is "form_names" (the games' form label) or "names" (the games' full name, reduced to a
    label by strip_species). The first is preferred; the second is a second pass for the ~40 forms
    PokeAPI labels only as a full name.
    """
    index = pokeapi_get("https://pokeapi.co/api/v2/pokemon-form?limit=3000")
    bySpecies = {}
    speciesOfPokemon = {}
    for entry in index["results"]:
        document = pokeapi_get("https://pokeapi.co/api/v2/pokemon-form/%s/" % entry["name"])
        if not document:
            continue
        names = extract_names(document, field)
        if not names.get("en"):
            continue
        pokemonId = int(document["pokemon"]["url"].rstrip("/").rsplit("/", 1)[-1])
        if pokemonId <= NATIONAL_DEX_MAX:
            speciesId = pokemonId
        else:
            if pokemonId not in speciesOfPokemon:
                pokemon = pokeapi_get("https://pokeapi.co/api/v2/pokemon/%d/" % pokemonId)
                speciesOfPokemon[pokemonId] = (
                    int(pokemon["species"]["url"].rstrip("/").rsplit("/", 1)[-1]) if pokemon else 0)
            speciesId = speciesOfPokemon[pokemonId]
        if speciesId:
            bySpecies.setdefault(speciesId, []).append(
                (form_key(names["en"], speciesId), entry["name"], names))
    for speciesId, entries in bySpecies.items():
        counts = {}
        for key, _, _ in entries:
            counts[key] = counts.get(key, 0) + 1
        bySpecies[speciesId] = [item for item in entries if counts[item[0]] == 1]
    return bySpecies


def fill(labels, speciesId, formId, names):
    """Record every language this PokeAPI row can name that is not already spoken for."""
    slot = labels.setdefault((speciesId, formId), {})
    for lang in LANGUAGES:
        if lang == "en" or lang in slot or not names.get(lang):
            continue
        value = strip_species(names[lang], speciesId, lang)
        if value:
            slot[lang] = value


def pokeapi_labels(rows):
    """PokeAPI's form label per (species, form), per language.

    THREE PASSES, MOST SPECIFIC FIRST. The sprite map names the exact form, so it is asked first
    and is the only thing that can separate two forms sharing a label. Then the games' form label
    matched by English within one species, then the games' full NAME reduced to a label the same
    way -- that last one is what names Greninja's Battle Bond, which PokeAPI labels only as part of
    a full name.
    """
    labels = {}
    print("   ...matching PokeAPI through PKSE's sprite map", flush=True)
    slugs = sprite_map_slugs(rows)
    for row in rows:
        if is_type_derived(row["species"], row["form"]):
            continue
        slug = slugs.get((row["species"], row["form"]))
        if not slug:
            continue
        document = pokeapi_get("https://pokeapi.co/api/v2/pokemon-form/%s/" % slug)
        if not document:
            continue
        names = extract_names(document, "form_names")
        english = names.get("en")
        # The English still has to agree -- the sprite map names the right FORM, but PokeAPI's
        # label for it is sometimes a full name or a differently-scoped one (Alcremie bakes the
        # Sweet into a label PKSE keeps in its own field).
        if not english or form_key(english, row["species"]) != form_key(row["english"], row["species"]):
            continue
        fill(labels, row["species"], row["form"], names)
    for field in ("form_names", "names"):
        print("   ...matching PokeAPI %s" % field, flush=True)
        source = pokeapi_forms(field)
        for row in rows:
            if is_type_derived(row["species"], row["form"]):
                continue
            key = form_key(row["english"], row["species"])
            hits = [(slug, names) for candidate, slug, names in source.get(row["species"], [])
                    if candidate == key and candidate]
            if len(hits) != 1:
                continue
            fill(labels, row["species"], row["form"], hits[0][1])
    drop_collisions(labels)
    return labels


def drop_collisions(labels):
    """Two forms of one species must not end up sharing a localized label.

    PokeAPI's German says "Mega-Form" for BOTH of Charizard's megas, which would lose a distinction
    PKHeX keeps -- so the colliding language is dropped for those rows and PKHeX names them apart.
    """
    formsOfSpecies = {}
    for speciesId, formId in labels:
        formsOfSpecies.setdefault(speciesId, []).append(formId)
    dropped = 0
    for speciesId, forms in formsOfSpecies.items():
        for lang in LANGUAGES:
            if lang == "en":
                continue
            counts = {}
            for formId in forms:
                value = labels[(speciesId, formId)].get(lang)
                if value:
                    counts[value] = counts.get(value, 0) + 1
            for formId in forms:
                value = labels[(speciesId, formId)].get(lang)
                if value and counts[value] > 1:
                    del labels[(speciesId, formId)][lang]
                    dropped += 1
    print("  dropped %d cell(s) where two forms of one species collided" % dropped)


def is_type_derived(speciesId, formId):
    """Arceus and Silvally name their forms with the TYPE table, which is already language-indexed.

    FormNames.cpp returns getTypeName(formId) for these, so a row here would shadow that table and
    let the two drift -- and PokeAPI spells them differently anyway ("Arceus (Kampf)").
    """
    return speciesId in (493, 773) and formId < 18
def wrap_cells(cells, indent, prefix, suffix, width=112):
    """Emit `prefix{cells}suffix` wrapped to `width`, so a generated row stays readable.

    Size is not a constraint for this project (PKSE runs under title override), so the table is
    wrapped rather than packed -- a reader checking a translation should not have to scroll.
    """
    out = []
    line = indent + prefix
    for index, cell in enumerate(cells):
        piece = cell + ("," if index + 1 < len(cells) else "")
        if len(line) + len(piece) + 1 > width and line.strip() != prefix.strip():
            out.append(line.rstrip() + "\n")
            line = indent + " " * len(prefix)
        line += piece + " "
    out.append(line.rstrip() + suffix + "\n")
    return out


def main():
    local, core = pkhex_checkout()
    commit = pkhex_commit(local)
    dump = run_dumper(core)

    # Every (species, form) any language names, so the row set is one thing rather than nine.
    rows = {}
    for languageIndex, lang in enumerate(LANGUAGES):
        for species, forms in dump[lang].items():
            for form, name in enumerate(forms):
                rows.setdefault((int(species), form), [""] * len(LANGUAGES))[languageIndex] = name

    # PokeAPI's label wins where it has one: it carries the games' own wording ("Forme d'Alola")
    # where PKHeX carries its own terse dropdown label ("Alola").
    # PKHEX'S OWN STRINGS GET THE SAME TREATMENT. Its Chinese for Rotom's forms is the full name
    # ("\u52a0\u71b1\u6d1b\u6258\u59c6" -- Heat Rotom), which PKSE would render as "\u6d1b\u6258\u59c6 (\u52a0\u71b1\u6d1b\u6258\u59c6)".
    for (species, form), names in rows.items():
        for languageIndex, lang in enumerate(LANGUAGES):
            if lang == "en" or not names[languageIndex]:
                continue
            names[languageIndex] = strip_species(names[languageIndex], species, lang)

    # Arceus and Silvally are named by the type table, which is already language-indexed.
    for key in [key for key in rows if is_type_derived(*key)]:
        del rows[key]

    print("  asking PokeAPI for the games' own form labels...")
    labels = pokeapi_labels(sprite_map())
    replaced = 0
    for key, names in labels.items():
        if key not in rows:
            continue
        for languageIndex, lang in enumerate(LANGUAGES):
            better = names.get(lang)
            if better and better != rows[key][languageIndex]:
                rows[key][languageIndex] = better
                replaced += 1
    print("  PokeAPI supplied %d label(s) across %d (species, form) rows" % (replaced, len(labels)))

    lines = []
    lines.append("// AUTO-GENERATED form names for the eight non-English languages.\n")
    lines.append("// Regenerate with tools/gen_formnames.py (needs a PKHeX checkout + .NET SDK).\n")
    lines.append("// Sources: PokeAPI where it has the games' own wording -- form_names first, then\n")
    lines.append("// names reduced to a label by removing the species -- else PKHeX's own label, from\n")
    lines.append("// FormConverter.GetFormList unioned across EntityContexts, PKHeX commit %s.\n" % commit)
    lines.append("//\n")
    lines.append("// PKHeX IS TERSE ONLY IN THE LATIN LANGUAGES. It drops the head noun there -- \"Alola\"\n")
    lines.append("// for the games' \"Forme d'Alola\" / \"Alola-Form\" -- which is why PokeAPI is preferred.\n")
    lines.append("// Its Japanese, Korean and BOTH Chinese are already the games' full wording, so those\n")
    lines.append("// columns are largely PKHeX's; PokeAPI carries no Chinese form names at all.\n")
    lines.append("//\n")
    lines.append("// Arceus and Silvally are absent on purpose: getTypeName() already names their forms\n")
    lines.append("// in every language, and a row here would shadow it.\n")
    lines.append("//\n")
    lines.append("// ENGLISH IS DELIBERATELY ABSENT. PKSE's English names live in FormNames.cpp and are\n")
    lines.append("// closer to the games than PKHeX's terse dropdown labels -- \"Alolan\" not \"Alola\",\n")
    lines.append("// \"Plant Cloak\" not \"Plant\". getFormName() asks English first and only consults this\n")
    lines.append("// table for another language, so the two can never disagree about which forms exist.\n")
    lines.append("#include <cstddef>\n")
    lines.append("#include <cstdint>\n")
    lines.append("\n")
    lines.append('#include "Names/FormNames.h"\n')
    lines.append('#include "Names/NameLanguage.h"\n')
    lines.append("\n")
    lines.append("namespace Names\n")
    lines.append("{\n")
    lines.append("    namespace\n")
    lines.append("    {\n")
    lines.append("        struct LocalizedFormName\n")
    lines.append("        {\n")
    lines.append("            uint16_t species;\n")
    lines.append("            uint8_t form;\n")
    lines.append("            const char *names[LANGUAGE_COUNT];\n")
    lines.append("        };\n")
    lines.append("\n")
    lines.append("        // Sorted by (species, form) so the lookup can binary-search.\n")
    lines.append("        const LocalizedFormName LOCALIZED_FORM_NAMES[] = {\n")
    for species, form in sorted(rows):
        names = rows[(species, form)]
        cells = []
        for languageIndex in range(len(LANGUAGES)):
            if languageIndex == ENGLISH_INDEX:
                cells.append("nullptr")   # English comes from the curated switch
            else:
                cells.append('"%s"' % escape(names[languageIndex]))
        lines.extend(wrap_cells(cells, "            ",
                                "{%4d, %2d, {" % (species, form), "}},"))
    lines.append("        };\n")
    lines.append("    }\n")
    lines.append("\n")
    lines.append("    /// Localized form name, or nullptr when there is none for this language.\n")
    lines.append("    const char *getFormNameLocalized(uint16_t speciesId, uint8_t formId, size_t languageIndex)\n")
    lines.append("    {\n")
    lines.append("        if (languageIndex >= LANGUAGE_COUNT)\n")
    lines.append("            return nullptr;\n")
    lines.append("        constexpr size_t count = sizeof(LOCALIZED_FORM_NAMES) / sizeof(LOCALIZED_FORM_NAMES[0]);\n")
    lines.append("        size_t low = 0, high = count;\n")
    lines.append("        while (low < high)\n")
    lines.append("        {\n")
    lines.append("            const size_t middle = low + (high - low) / 2;\n")
    lines.append("            const LocalizedFormName &row = LOCALIZED_FORM_NAMES[middle];\n")
    lines.append("            if (row.species < speciesId || (row.species == speciesId && row.form < formId))\n")
    lines.append("                low = middle + 1;\n")
    lines.append("            else if (row.species == speciesId && row.form == formId)\n")
    lines.append("                return row.names[languageIndex];\n")
    lines.append("            else\n")
    lines.append("                high = middle;\n")
    lines.append("        }\n")
    lines.append("        return nullptr;\n")
    lines.append("    }\n")
    lines.append("}\n")

    with open(OUT, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("".join(lines))
    print("Wrote %s with %d (species, form) rows x %d languages (PKHeX %s)"
          % (OUT, len(rows), len(LANGUAGES) - 1, commit[:9]))


if __name__ == "__main__":
    main()
