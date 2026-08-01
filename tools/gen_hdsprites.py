#!/usr/bin/env python3
"""Fetch Pokemon HOME sprites from PokeAPI and downscale them into romfs.

PokeAPI serves the HOME renders keyed by NUMERIC id -- base species by National Dex number,
alternate forms by PokeAPI's 10000+ form ids -- so there is NO name map to get wrong. (That
name map like that is exactly what silently dropped Mimikyu & co. under the old name-keyed source.) The
renders are 512px; we downscale to 256px (LANCZOS) so the Switch decode footprint stays small
-- a 512px RGBA is 4x the memory and the sprite cache does not evict.

Output: romfs/sprites/pokemon_hd/<id>.png (normal) and <id>s.png (shiny).

A base-species NORMAL sprite that fails to download is a HARD error (fail loud -- no silent
gaps). A shiny, or an alternate form, may legitimately have no HOME render, so those are
reported and skipped.

Source is pinned to a PokeAPI/sprites commit for reproducibility; bump PINNED_REF to adopt
newer sprites (e.g. a future generation), then review what changed.

Requires Pillow:  pip install pillow
Run:  python tools/gen_hdsprites.py                 # fetch only what's missing (downscaled)
      python tools/gen_hdsprites.py --force         # re-fetch everything (use when switching source)
      python tools/gen_hdsprites.py --only 778 10091 # fetch just these ids (repair / test)
"""
import argparse
import io
import os
import sys
import time
import urllib.error
import urllib.request
from concurrent.futures import ThreadPoolExecutor

from PIL import Image

# PokeAPI/sprites pinned commit (raw.githubusercontent; jsDelivr can't serve this repo -- too large).
PINNED_REF = "8dfa3d97e953caaafaafd4963eff7621811af08e"
BASE_URL = "https://raw.githubusercontent.com/PokeAPI/sprites/%s/sprites/pokemon/other/home" % PINNED_REF
OUT_DIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "romfs", "sprites", "pokemon_hd",
)
TARGET_PX = 256
DEX_MAX = 1025
MAX_WORKERS = int(os.environ.get("SPRITE_MAX_WORKERS", "8"))
DOWNLOAD_RETRIES = int(os.environ.get("DOWNLOAD_RETRIES", "5"))
DOWNLOAD_RETRY_DELAY = int(os.environ.get("DOWNLOAD_RETRY_DELAY", "2"))

# Alternate-form ids (PokeAPI's 10000+ ids). PKSE's FormSpriteMapping references these; PokeAPI
# keys its HOME renders by the same ids, so no name lookup is needed.
FORM_IDS = [
    10001, 10002, 10003, 10004, 10005, 10006, 10007, 10008, 10009, 10010, 10011, 10012, 10016,
    10019, 10020, 10021, 10022, 10023, 10024, 10025, 10027, 10028, 10029, 10030, 10031, 10032,
    10033, 10034, 10035, 10036, 10037, 10038, 10039, 10040, 10041, 10086, 10087, 10088, 10089,
    10090, 10091, 10092, 10093, 10094, 10095, 10096, 10097, 10098, 10099, 10100, 10101, 10102,
    10103, 10104, 10105, 10106, 10107, 10108, 10109, 10110, 10111, 10112, 10113, 10114, 10115,
    10116, 10117, 10118, 10119, 10120, 10123, 10124, 10125, 10126, 10127, 10130, 10131, 10132,
    10133, 10134, 10152, 10155, 10156, 10157, 10161, 10162, 10163, 10164, 10165, 10166, 10167,
    10168, 10169, 10170, 10171, 10172, 10173, 10174, 10175, 10176, 10177, 10178, 10179, 10180,
    10184, 10185, 10186, 10188, 10189, 10191, 10192, 10193, 10194, 10229, 10230, 10231, 10232,
    10233, 10234, 10235, 10236, 10237, 10238, 10239, 10240, 10241, 10242, 10243, 10244, 10245,
    10246, 10247, 10248, 10249, 10250, 10251, 10252, 10253, 10254, 10255, 10256, 10257, 10258,
    10259, 10260, 10261, 10262, 10263, 10272, 10273, 10274, 10275, 10276, 10277,
]


def _get(url):
    req = urllib.request.Request(url, headers={"User-Agent": "pkse-gen-hdsprites"})
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


def _save_downscaled(data, path):
    img = Image.open(io.BytesIO(data)).convert("RGBA")
    if img.size != (TARGET_PX, TARGET_PX):
        img = img.resize((TARGET_PX, TARGET_PX), Image.LANCZOS)
    img.save(path)


def _one(spid, variant, force):
    """variant '' = normal, 's' = shiny. Returns 'ok' | 'have' | 'miss'."""
    out = os.path.join(OUT_DIR, "%d%s.png" % (spid, variant))
    if not force and os.path.exists(out):
        return "have"
    sub = "shiny/" if variant == "s" else ""
    data = _get("%s/%s%d.png" % (BASE_URL, sub, spid))
    if data is None:
        return "miss"
    _save_downscaled(data, out)
    return "ok"


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--force", action="store_true", help="re-fetch even if the file already exists")
    ap.add_argument("--only", type=int, nargs="+", metavar="ID", help="fetch only these ids")
    args = ap.parse_args()
    os.makedirs(OUT_DIR, exist_ok=True)

    ids = args.only if args.only else list(range(1, DEX_MAX + 1)) + FORM_IDS
    jobs = [(spid, variant) for spid in ids for variant in ("", "s")]

    ok = have = 0
    missing_base = []   # base normal that 404'd -> hard error
    missing_extra = []  # shiny / form that 404'd -> allowed

    def run(job):
        spid, variant = job
        return spid, variant, _one(spid, variant, args.force)

    with ThreadPoolExecutor(max_workers=MAX_WORKERS) as pool:
        for i, (spid, variant, res) in enumerate(pool.map(run, jobs), 1):
            if res == "ok":
                ok += 1
            elif res == "have":
                have += 1
            else:  # miss
                if variant == "" and spid <= DEX_MAX:
                    missing_base.append(spid)
                else:
                    missing_extra.append("%d%s" % (spid, variant))
            if i % 200 == 0 or i == len(jobs):
                print("processed: %d/%d" % (i, len(jobs)), flush=True)

    print("\ndownloaded=%d  already-present=%d  (256px, from PokeAPI @ %s)" % (ok, have, PINNED_REF[:10]))
    if missing_extra:
        print("no HOME render (skipped -- expected for some forms/shinies): %d" % len(missing_extra))
    if missing_base:
        missing_base.sort()
        print("\nERROR: %d base species have NO HOME sprite: %s" % (len(missing_base), missing_base),
              file=sys.stderr)
        sys.exit(1)
    print("all base species present.")


if __name__ == "__main__":
    main()
