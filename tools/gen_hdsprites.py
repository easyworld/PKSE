#!/usr/bin/env python3
"""Fetch EVERY Pokemon HOME sprite from PokeAPI and downscale them into romfs.

This script deliberately does NOT decide which sprites are interesting -- it mirrors the whole
`sprites/pokemon/other/home` tree at the pinned commit. Picking a subset here is what silently
lost Meloetta Pirouette and raichu-mega-y: the download list and FormSpriteMapping.cpp were two
hand-maintained lists that had to agree, with nothing enforcing it. Now the mapping is free to
reference any id at any time and the art is already on disk; anything the mapping never
references is inert (SpriteManager builds paths from ids, it never enumerates the directory).

PokeAPI keys these renders by NUMERIC id -- base species by National Dex number, alternate forms
by PokeAPI's 10000+ form ids -- so there is no name map to get wrong. A minority are name-keyed
instead (`201-a`, `869-vanilla-cream-berry-sweet`); those are fetched too and simply sit unused
until something maps them.

The renders are 512px; we downscale to 256px (LANCZOS) so the Switch decode footprint stays
small -- a 512px RGBA is 4x the memory and the sprite cache does not evict.

Output (flat, in romfs/sprites/pokemon_hd/):
    <stem>.png      normal          <- home/<stem>.png
    <stem>s.png     shiny           <- home/shiny/<stem>.png
    <stem>f.png     female          <- home/female/<stem>.png
    <stem>fs.png    shiny female    <- home/shiny/female/<stem>.png

Fail-loud, in three places: an unrecognised subdirectory in the tree is an error (a PokeAPI
reorg must not silently drop art), a listed file that fails to download is an error (it was in
the tree, so it exists), and a missing base-species normal sprite is an error.

Source is pinned to a PokeAPI/sprites commit for reproducibility; bump PINNED_REF to adopt newer
sprites (e.g. a future generation), then review what changed.

Requires Pillow:  pip install pillow
Run:  python tools/gen_hdsprites.py                  # fetch only what's missing (downscaled)
      python tools/gen_hdsprites.py --force          # re-fetch everything (use when bumping the ref)
      python tools/gen_hdsprites.py --only 778 10091 # fetch just these stems, all variants (repair)
"""
import argparse
import io
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from concurrent.futures import ThreadPoolExecutor

from PIL import Image

# PokeAPI/sprites pinned commit (raw.githubusercontent; jsDelivr can't serve this repo -- too large).
PINNED_REF = "8dfa3d97e953caaafaafd4963eff7621811af08e"
SPRITE_DIR = "sprites/pokemon/other/home"
BASE_URL = "https://raw.githubusercontent.com/PokeAPI/sprites/%s/%s" % (PINNED_REF, SPRITE_DIR)
TREE_URL = "https://api.github.com/repos/PokeAPI/sprites/git/trees/%s%%3A%s?recursive=1" % (
    PINNED_REF, urllib.parse.quote(SPRITE_DIR),
)
OUT_DIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "romfs", "sprites", "pokemon_hd",
)
TARGET_PX = 256
DEX_MAX = 1025
MAX_WORKERS = int(os.environ.get("SPRITE_MAX_WORKERS", "8"))
DOWNLOAD_RETRIES = int(os.environ.get("DOWNLOAD_RETRIES", "5"))
DOWNLOAD_RETRY_DELAY = int(os.environ.get("DOWNLOAD_RETRY_DELAY", "2"))

# Subdirectory in the upstream tree -> filename suffix we store it under. "" is the tree root.
# An unlisted subdirectory is a hard error rather than a skip.
VARIANTS = {
    "": "",
    "shiny": "s",
    "female": "f",
    "shiny/female": "fs",
}


def _get(url, accept=None):
    headers = {"User-Agent": "pkse-gen-hdsprites"}
    if accept:
        headers["Accept"] = accept
    req = urllib.request.Request(url, headers=headers)
    for attempt in range(DOWNLOAD_RETRIES + 1):
        try:
            with urllib.request.urlopen(req, timeout=30) as resp:
                return resp.read()
        except urllib.error.HTTPError as error:
            if error.code == 404:
                return None
            if error.code not in (408, 429) and error.code < 500:
                raise
            last_error = error
        except (OSError, urllib.error.URLError) as error:
            last_error = error

        if attempt == DOWNLOAD_RETRIES:
            raise last_error
        delay = min(DOWNLOAD_RETRY_DELAY * (2 ** attempt), 30)
        print("download failed, retrying in %ds (%d/%d): %s" %
              (delay, attempt + 1, DOWNLOAD_RETRIES + 1, url), file=sys.stderr, flush=True)
        time.sleep(delay)

    raise RuntimeError("unreachable")


def listing():
    """Every PNG under the pinned home/ tree, as a list of (remote_path, local_name)."""
    raw = _get(TREE_URL, accept="application/vnd.github+json")
    if raw is None:
        raise SystemExit("tree listing 404'd -- is PINNED_REF (%s) valid?" % PINNED_REF[:10])
    tree = json.loads(raw.decode("utf-8"))
    if tree.get("truncated"):
        raise SystemExit("tree listing was truncated by the API -- cannot guarantee completeness")

    out = []
    for entry in tree.get("tree", ()):
        path = entry.get("path", "")
        if entry.get("type") != "blob" or not path.endswith(".png"):
            continue
        subdir, _, filename = path.rpartition("/")
        if subdir not in VARIANTS:
            raise SystemExit(
                "unknown subdirectory %r in %s -- upstream layout changed; add it to VARIANTS "
                "(and decide its filename suffix) rather than letting the art go missing"
                % (subdir, SPRITE_DIR)
            )
        stem = filename[:-4]
        out.append((path, "%s%s.png" % (stem, VARIANTS[subdir])))
    if not out:
        raise SystemExit("tree listing contained no PNGs -- has %s moved?" % SPRITE_DIR)
    return out


def _save_downscaled(data, path):
    img = Image.open(io.BytesIO(data)).convert("RGBA")
    if img.size != (TARGET_PX, TARGET_PX):
        img = img.resize((TARGET_PX, TARGET_PX), Image.LANCZOS)
    img.save(path)


def _one(remote, local, force):
    """Returns 'ok' | 'have' | 'miss'."""
    out = os.path.join(OUT_DIR, local)
    if not force and os.path.exists(out):
        return "have"
    data = _get("%s/%s" % (BASE_URL, remote))
    if data is None:
        return "miss"
    _save_downscaled(data, out)
    return "ok"


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--force", action="store_true", help="re-fetch even if the file already exists")
    ap.add_argument("--only", nargs="+", metavar="STEM",
                    help="fetch only these sprite stems (e.g. 778 10091 201-a), all variants")
    args = ap.parse_args()
    os.makedirs(OUT_DIR, exist_ok=True)

    print("listing %s @ %s ..." % (SPRITE_DIR, PINNED_REF[:10]), flush=True)
    jobs = listing()
    print("upstream has %d sprites." % len(jobs), flush=True)

    if args.only:
        want = set(args.only)
        jobs = [j for j in jobs if os.path.basename(j[0])[:-4] in want]
        if not jobs:
            raise SystemExit("no sprites matched --only %s" % " ".join(args.only))

    ok = have = 0
    missing = []

    def run(job):
        remote, local = job
        return remote, local, _one(remote, local, args.force)

    with ThreadPoolExecutor(max_workers=MAX_WORKERS) as pool:
        for i, (remote, local, res) in enumerate(pool.map(run, jobs), 1):
            if res == "ok":
                ok += 1
            elif res == "have":
                have += 1
            else:
                missing.append(remote)
            if i % 200 == 0 or i == len(jobs):
                print("processed: %d/%d" % (i, len(jobs)), flush=True)

    print("\ndownloaded=%d  already-present=%d  (256px, from PokeAPI @ %s)"
          % (ok, have, PINNED_REF[:10]))

    # Every job came out of the tree listing, so a 404 means the download path is wrong.
    if missing:
        print("\nERROR: %d listed sprites failed to download: %s"
              % (len(missing), ", ".join(missing[:10])), file=sys.stderr)
        sys.exit(1)

    on_disk = set(os.listdir(OUT_DIR))
    missing_base = [n for n in range(1, DEX_MAX + 1) if "%d.png" % n not in on_disk]
    if missing_base:
        print("\nERROR: %d base species have NO HOME sprite: %s"
              % (len(missing_base), missing_base), file=sys.stderr)
        sys.exit(1)
    print("all %d base species present." % DEX_MAX)


if __name__ == "__main__":
    main()
