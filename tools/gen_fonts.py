#!/usr/bin/env python3
"""Fetch the bundled UI fonts into romfs/fonts/.

Three faces, all under the SIL Open Font License -- which is why they can be bundled inside the
.nro and redistributed at all:

    Nunito                the face everything in the UI is drawn in
    Noto Sans Symbols     the gender signs and the star, which Nunito has no glyph for
    Noto Sans Symbols 2   the card-suit heart, which Symbols itself omits

PKSEFramebuffer chains the two Symbols faces after Nunito as NanoVG fallbacks, in that order.

CJK IS NOT HERE, and adding it would be the wrong fix. Those glyphs come from the console's own
shared fonts through `pl` at runtime -- the same faces the console and the games render Pokemon
names with -- so they are never downloaded and never bundled.

Tracks google/fonts `main` rather than a pinned commit, deliberately: PKSE takes three files from
a repository of thousands, and an upstream font fix is something it wants. Nothing is re-fetched
once the file is on disk, so `main` moving cannot change a build underneath anyone; only --force
picks up a newer cut.

Needs nothing but Python 3.

Run:  python tools/gen_fonts.py            # fetch whatever is missing
      python tools/gen_fonts.py --force    # re-fetch all three
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from asset_fetch import fetch_all, present  # noqa: E402

FONT_BASE_URL = "https://github.com/google/fonts/raw/main/ofl"
OUT_DIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "romfs", "fonts",
)

# (label, romfs filename, url). The %5B/%5D are the literal square brackets in a variable font's
# filename; they are already percent-encoded here because that is what the raw host serves.
FONTS = [
    ("Nunito", "Nunito.ttf", "%s/nunito/Nunito%%5Bwght%%5D.ttf" % FONT_BASE_URL),
    ("Noto Sans Symbols", "NotoSansSymbols.ttf",
     "%s/notosanssymbols/NotoSansSymbols%%5Bwght%%5D.ttf" % FONT_BASE_URL),
    ("Noto Sans Symbols 2", "NotoSansSymbols2.ttf",
     "%s/notosanssymbols2/NotoSansSymbols2-Regular.ttf" % FONT_BASE_URL),
]

# Every one of these is a TrueType outline font, so the payload starts with the 0x00010000 version
# tag. An HTML error page or an LFS pointer file does not, and would otherwise sit in romfs as a
# font NanoVG silently refuses to load, which reads on console as "all the text is gone".
TRUETYPE_MAGIC = b"\x00\x01\x00\x00"


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--force", action="store_true",
                        help="re-fetch even when the font is already on disk")
    args = parser.parse_args()
    os.makedirs(OUT_DIR, exist_ok=True)

    jobs = [(label, url, os.path.join(OUT_DIR, filename)) for label, filename, url in FONTS]
    failures = fetch_all(jobs, force=args.force, magic=TRUETYPE_MAGIC)
    if failures:
        sys.exit(1)

    missing = [filename for _, filename, _ in FONTS
               if not present(os.path.join(OUT_DIR, filename))]
    if missing:
        print("ERROR: %d font(s) still missing: %s" % (len(missing), ", ".join(missing)),
              file=sys.stderr)
        sys.exit(1)
    print("all %d UI fonts present in %s" % (len(FONTS), OUT_DIR))


if __name__ == "__main__":
    main()
