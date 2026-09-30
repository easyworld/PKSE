#!/usr/bin/env python3
"""Fetch the type icons PKSE draws into romfs/sprites/types/.

NINETEEN, NOT EIGHTEEN. The nineteenth is Stellar, which no species and no move has -- it is
reachable only as a TERA type, and the details view draws it like any other type. Keep this count
in step with `Names::getTypeNameCount()` (19, how many types can be NAMED or DRAWN) and NOT with
`Names::getTypeCount()` (18, how many a species or move can BE). The two answer different
questions on purpose; anything that merges them loses a check that catches a corrupt type of 18.

PokeAPI numbers its types from 1 -- 1=Normal, 2=Fighting, ... 18=Fairy, 19=Stellar -- and PKSE's
MoveType enum from 0, so upstream icon `i` is saved as `(i - 1).png`. Nothing at runtime then has
to know PokeAPI exists: SpriteManager builds the path straight from the type id.

The art is the Scarlet/Violet styling, pinned to the same PokeAPI/sprites commit gen_hdsprites.py
uses so the two fetchers can never disagree about which upstream snapshot romfs came from. Bump
them together.

Needs nothing but Python 3 -- no Pillow: these are drawn at their native size.

Run:  python tools/gen_typeicons.py            # fetch whatever is missing
      python tools/gen_typeicons.py --force    # re-fetch all 19 (after bumping PINNED_REF)
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from asset_fetch import fetch_all, present  # noqa: E402

# PokeAPI/sprites pinned commit -- the same one gen_hdsprites.py pins. See the note above.
PINNED_REF = "8dfa3d97e953caaafaafd4963eff7621811af08e"
ICON_STYLE = "generation-ix/scarlet-violet"
BASE_URL = "https://raw.githubusercontent.com/PokeAPI/sprites/%s/sprites/types/%s" % (
    PINNED_REF, ICON_STYLE,
)
OUT_DIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "romfs", "sprites", "types",
)
TYPE_ICON_COUNT = 19
PNG_MAGIC = b"\x89PNG\r\n\x1a\n"


def jobs():
    """(label, url, output_path) for every type icon, upstream id -> local id."""
    out = []
    for upstream_id in range(1, TYPE_ICON_COUNT + 1):
        local_id = upstream_id - 1
        out.append((
            "type %d -> %d.png" % (upstream_id, local_id),
            "%s/%d.png" % (BASE_URL, upstream_id),
            os.path.join(OUT_DIR, "%d.png" % local_id),
        ))
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--force", action="store_true",
                        help="re-fetch even when the icon is already on disk")
    args = parser.parse_args()
    os.makedirs(OUT_DIR, exist_ok=True)

    failures = fetch_all(jobs(), force=args.force, magic=PNG_MAGIC)
    if failures:
        sys.exit(1)

    # The count is the whole point: a set that is missing one icon draws a blank where a type
    # should be, and nothing on the console says which one.
    missing = [local_id for local_id in range(TYPE_ICON_COUNT)
               if not present(os.path.join(OUT_DIR, "%d.png" % local_id))]
    if missing:
        print("ERROR: %d type icon(s) still missing: %s"
              % (len(missing), ", ".join("%d.png" % local_id for local_id in missing)),
              file=sys.stderr)
        sys.exit(1)
    print("all %d type icons present in %s" % (TYPE_ICON_COUNT, OUT_DIR))


if __name__ == "__main__":
    main()
