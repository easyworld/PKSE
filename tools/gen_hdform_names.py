#!/usr/bin/env python3
"""Generate tools/hd_form_names.txt -- the form-id -> pokemondb HOME sprite name map
used by the Makefile's `hdforms` target (task #13).

Base-species HD sprites come from pokemondb at 256px, keyed by NAME. PokeAPI keys by id
but only serves 512px, which is 4x the bytes and 4x the decoded memory for no visible
gain (the UI draws at most ~196px). So forms use pokemondb too, which means deriving the
id -> name mapping: take PokeAPI's own form name, rewrite it into pokemondb's spelling,
and VERIFY the resulting URL actually resolves before writing it out.

The verification matters more than it sounds. A trimmed name very often exists but is a
DIFFERENT Pokemon -- "tauros-paldea-combat-breed" trimmed to "tauros" is a perfectly
valid URL and completely the wrong sprite. So candidates are ordered most-specific-first,
and a guard rejects any result that silently dropped a regional marker.

Regenerate when FORM_SPRITE_IDS changes:  python tools/gen_hdform_names.py
"""
import io
import os
import re
import json
import urllib.request
import urllib.error
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MK = os.path.join(ROOT, "Makefile")
OUT = os.path.join(ROOT, "tools", "hd_form_names.txt")
PDB = "https://img.pokemondb.net/sprites/home/normal/{}.png"
API = "https://pokeapi.co/api/v2/pokemon/{}/"

# PokeAPI regional spelling -> pokemondb's. Applied ONCE, mid-name or trailing.
REGIONAL = {"alola": "alolan", "galar": "galarian", "hisui": "hisuian", "paldea": "paldean"}

# Families pokemondb names in a way no rewrite/trim reaches (its name is longer, or
# worded differently). Every one of these was confirmed by fetching the URL.
EXPLICIT = {
    "necrozma-dusk":             "necrozma-dusk-mane",
    "necrozma-dawn":             "necrozma-dawn-wings",
    "calyrex-ice":               "calyrex-ice-rider",
    "calyrex-shadow":            "calyrex-shadow-rider",
    "darmanitan-galar-standard": "darmanitan-galarian-standard",
    "darmanitan-galar-zen":      "darmanitan-galarian-zen",
    "raticate-totem-alola":      "raticate-alolan",
    "minior-orange-meteor":      "minior-meteor",
    "minior-yellow-meteor":      "minior-meteor",
    "minior-green-meteor":       "minior-meteor",
    "minior-blue-meteor":        "minior-meteor",
    "minior-indigo-meteor":      "minior-meteor",
    "minior-violet-meteor":      "minior-meteor",
    "minior-red-meteor":         "minior-meteor",
}

# Qualifiers pokemondb simply omits (same artwork either way).
DROP_SUFFIX = ("-power-construct", "-battle-bond", "-breed", "-plumage", "-mask",
               "-totem", "-family-of-three", "-family-of-four")


def get(url, timeout=25):
    req = urllib.request.Request(url, headers={"User-Agent": "pkse-sprite-tool"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return r.read()


def api_name(i):
    try:
        return json.loads(get(API.format(i)).decode("utf-8"))["name"]
    except Exception:
        return None


def regionalise(name):
    """Rewrite the regional marker into pokemondb's spelling. ONE replacement only --
    replacing '-paldea-' and then '-paldea' in sequence yields '-paldeann-'."""
    for src, dst in REGIONAL.items():
        if ("-" + src + "-") in name:
            return name.replace("-" + src + "-", "-" + dst + "-", 1)
        if name.endswith("-" + src):
            return name[: -len(src)] + dst
    return name


def candidates(name):
    """pokemondb spellings to try, MOST SPECIFIC FIRST."""
    out = []
    if name in EXPLICIT:
        out.append(EXPLICIT[name])
    reg = regionalise(name)
    if reg != name:
        out.append(reg)
    out.append(name)
    for base in (reg, name):                       # drop a qualifier pokemondb omits
        for q in DROP_SUFFIX:
            if base.endswith(q):
                out.append(base[: -len(q)])
    for base in (reg, name):                       # last resort: trim trailing segments
        parts = base.split("-")
        for k in range(len(parts) - 1, 0, -1):
            out.append("-".join(parts[:k]))
    return list(dict.fromkeys(out))


def lost_region(api, chosen):
    """True if the PokeAPI name carried a regional marker and the chosen one dropped it
    -- i.e. we are about to ship the base Pokemon's sprite for a regional variant.

    Only meaningful when the name was CHANGED: if pokemondb uses PokeAPI's spelling
    verbatim nothing can have been lost. That exemption is load-bearing for Pikachu's
    cap forms ("pikachu-alola-cap"), which are named after a region but are not
    regional variants, so the marker never gets rewritten to "alolan"."""
    if chosen == api:
        return False
    if not any(("-" + s) in api for s in REGIONAL):
        return False
    return not any(d in chosen for d in REGIONAL.values())


def resolve(i):
    api = api_name(i)
    if not api:
        return i, None, None, "pokeapi lookup failed"
    for cand in candidates(api):
        try:
            get(PDB.format(cand))
        except urllib.error.HTTPError:
            continue
        except Exception as e:
            return i, api, None, "network: " + str(e)
        if lost_region(api, cand):
            return i, api, None, "would drop the regional marker ('%s' -> '%s')" % (api, cand)
        return i, api, cand, None
    return i, api, None, "no pokemondb sprite for '%s'" % api


def main():
    mk = io.open(MK, encoding="utf-8").read()
    blk = mk[mk.index("FORM_SPRITE_IDS"):]
    blk = blk[:blk.index("\n\n")]
    ids = sorted({int(x) for x in re.findall(r"\b(10\d{3})\b", blk)})

    print("resolving %d form ids -> pokemondb names (fetching each URL)..." % len(ids))
    with ThreadPoolExecutor(max_workers=12) as ex:
        results = list(ex.map(resolve, ids))

    ok = [(i, a, n) for i, a, n, e in results if n]
    bad = [(i, a, e) for i, a, n, e in results if not n]

    print("  resolved + verified : %d/%d" % (len(ok), len(ids)))
    print("  unresolved          : %d" % len(bad))
    for i, a, e in bad:
        print("     %d: %s" % (i, e))
    if bad:
        raise SystemExit("refusing to write a partial map -- extend EXPLICIT/DROP_SUFFIX")

    with io.open(OUT, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("# form id | pokemondb HOME sprite name\n")
        fh.write("# Generated and URL-verified by tools/gen_hdform_names.py (task #13).\n")
        fh.write("# Regenerate whenever FORM_SPRITE_IDS changes in the Makefile.\n")
        for i, a, n in ok:
            fh.write("%d|%s\n" % (i, n))

    rewritten = [(i, a, n) for i, a, n in ok if a != n]
    print("  rewritten from PokeAPI spelling: %d" % len(rewritten))
    print("wrote", OUT)


if __name__ == "__main__":
    main()
