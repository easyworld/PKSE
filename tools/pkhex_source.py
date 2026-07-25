#!/usr/bin/env python3
"""On-demand fetch of PKHeX.Core source/resource files from GitHub, cached locally.

The gen_*.py scripts derive PKSE's committed data tables from PKHeX. They are NOT
part of the build -- the build only compiles the tables they already produced. A
generator is run occasionally, by hand, when the PKHeX-derived data needs
regenerating (a new game, DLC, or an upstream correction).

Rather than require a local PKHeX checkout, every file a generator needs is pulled
from raw.githubusercontent.com the first time it is asked for and cached under
tools/.pkhex_cache/<ref>/ (gitignored), so re-runs are offline and fast.

Environment overrides:
  PKHEX_REF    git ref to fetch (commit / tag / branch). Defaults to the pinned
               commit the committed tables were generated from, so regenerating is
               reproducible. Set it to "master" (or a newer release tag) to adopt
               newer PKHeX data -- then review the resulting table diff before you
               commit it.
  PKHEX_LOCAL  path to a local PKHeX.Core directory. When set, files are read from
               there and nothing is downloaded (the old behavior).

A 404 means the file was moved or renamed on the chosen ref: pin PKHEX_REF to a ref
that still has it, or point PKHEX_LOCAL at a checkout.
"""
import os
import sys
import urllib.error
import urllib.request

_REPO = "kwsch/PKHeX"

# Commit the committed data tables were generated from (PKHeX master @ 2026-07-08).
# Bump this to adopt newer PKHeX data; the generated-table diff is your review gate.
_DEFAULT_REF = "6501f0ab46e8f8ca048539dbaf8cae8cb104e722"

_REF = os.environ.get("PKHEX_REF", _DEFAULT_REF)
_LOCAL = os.environ.get("PKHEX_LOCAL")
_CACHE = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".pkhex_cache", _REF)


def pkhex_path(relpath):
    """Local path to PKHeX.Core/<relpath>, fetched from GitHub + cached on first use.

    `relpath` is relative to PKHeX.Core and may use "/" or OS separators.
    """
    parts = relpath.replace("\\", "/").strip("/").split("/")
    if _LOCAL:
        return os.path.join(_LOCAL, *parts)

    dest = os.path.join(_CACHE, *parts)
    if os.path.exists(dest):
        return dest

    url = "https://raw.githubusercontent.com/%s/%s/PKHeX.Core/%s" % (
        _REPO, _REF, "/".join(parts))
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    sys.stderr.write("  fetch %s\n" % url)
    try:
        with urllib.request.urlopen(url) as resp:
            data = resp.read()
    except urllib.error.HTTPError as e:
        raise SystemExit(
            "PKHeX fetch failed (HTTP %s): %s\n"
            "  The file may have moved on ref '%s'. Set PKHEX_REF to a ref that has "
            "it, or PKHEX_LOCAL to a local PKHeX.Core checkout." % (e.code, url, _REF))
    except urllib.error.URLError as e:
        raise SystemExit("PKHeX fetch failed (network): %s\n  %s" % (url, e.reason))

    tmp = dest + ".part"
    with open(tmp, "wb") as fh:
        fh.write(data)
    os.replace(tmp, dest)
    return dest
