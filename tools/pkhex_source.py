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
  PKHEX_LOCAL  path to a PKHeX checkout, used INSTEAD of downloading. A shortcut for
               someone who already has one, never a requirement: every generator runs
               from GitHub alone. Either the repo root or the PKHeX.Core directory
               inside it is accepted.

A 404 means the file was moved or renamed on the chosen ref: pin PKHEX_REF to a ref
that still has it, or point PKHEX_LOCAL at a checkout.
"""
import os
import sys
import json
import shutil
import tarfile
import time
import urllib.error
import urllib.request

_REPO = "kwsch/PKHeX"

# PKSE TRACKS PKHEX's DEFAULT BRANCH rather than pinning a commit. Upstream corrections to the
# data are improvements PKSE wants, and the generated-table diff is the review gate for every
# one of them -- a pin only delays adopting a fix and makes "regenerate" mean two decisions
# instead of one. Set PKHEX_REF to a commit to reproduce an older table exactly.
_DEFAULT_REF = "master"

_REF = os.environ.get("PKHEX_REF", _DEFAULT_REF)
_LOCAL = os.environ.get("PKHEX_LOCAL")


def _resolve_ref(ref):
    """A branch name is resolved to the commit it points at, ONCE per run.

    The cache is keyed by what this returns, so a moving ref must not be used as the key: a
    directory called "master" would be written on the first run and then served forever, which
    is a pin again -- and a silent one, which is worse than the explicit pin this replaced.
    Resolving to a SHA means the cache is immutable, a run is internally consistent even if
    upstream moves halfway through it, and the tables can say exactly what they were built from.
    """
    if len(ref) == 40 and all(c in "0123456789abcdef" for c in ref.lower()):
        return ref
    url = "https://api.github.com/repos/%s/commits/%s" % (_REPO, ref)
    try:
        with urllib.request.urlopen(url) as resp:
            return json.load(resp)["sha"]
    except (urllib.error.HTTPError, urllib.error.URLError, KeyError, ValueError) as e:
        raise SystemExit(
            "Could not resolve PKHeX ref '%s' to a commit: %s\n"
            "  Set PKHEX_REF to a commit SHA, or PKHEX_LOCAL to a local PKHeX.Core checkout."
            % (ref, e))


_RESOLVED = _REF if _LOCAL else _resolve_ref(_REF)
_CACHE = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".pkhex_cache", _RESOLVED)


def pkhex_ref():
    """The commit the tables in this run are built from -- a SHA, even when PKHEX_REF named a
    branch. Generators put it in the file they emit so the output says where it came from."""
    return _RESOLVED


def pkhex_path(relpath):
    """Local path to PKHeX.Core/<relpath>, fetched from GitHub + cached on first use.

    `relpath` is relative to PKHeX.Core and may use "/" or OS separators.
    """
    parts = relpath.replace("\\", "/").strip("/").split("/")
    if _LOCAL:
        # PKHEX_LOCAL is normally the repo root, but a path that already IS PKHeX.Core works too:
        # a caller who sets it should not have to know which one this wanted.
        inner = os.path.join(_LOCAL, "PKHeX.Core")
        return os.path.join(inner if os.path.isdir(inner) else _LOCAL, *parts)

    dest = os.path.join(_CACHE, *parts)
    if os.path.exists(dest):
        return dest

    url = "https://raw.githubusercontent.com/%s/%s/PKHeX.Core/%s" % (
        _REPO, _RESOLVED, "/".join(parts))
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    sys.stderr.write("  fetch %s\n" % url)
    try:
        with urllib.request.urlopen(url) as resp:
            data = resp.read()
    except urllib.error.HTTPError as e:
        raise SystemExit(
            "PKHeX fetch failed (HTTP %s): %s\n"
            "  The file may have moved on ref '%s'. Set PKHEX_REF to a ref that has "
            "it, or PKHEX_LOCAL to a local PKHeX.Core checkout." % (e.code, url, _RESOLVED))
    except urllib.error.URLError as e:
        raise SystemExit("PKHeX fetch failed (network): %s\n  %s" % (url, e.reason))

    tmp = dest + ".part"
    with open(tmp, "wb") as fh:
        fh.write(data)
    os.replace(tmp, dest)
    return dest


def pkhex_repo():
    """Local path to a PKHeX REPO ROOT at the resolved ref, for the one generator that has to
    BUILD PKHeX.Core rather than read a file out of it.

    Fetched as a source tarball (~23 MB) and cached beside the per-file cache, so this is a
    download like every other one rather than a checkout the caller has to already own.
    """
    if _LOCAL:
        return _LOCAL

    root = os.path.join(_CACHE, "_repo")
    marker = os.path.join(root, ".extracted")
    if os.path.isfile(marker):
        with open(marker, encoding="utf-8") as fh:
            extracted = fh.read().strip()
        if os.path.isdir(extracted):
            return extracted

    url = "https://codeload.github.com/%s/tar.gz/%s" % (_REPO, _RESOLVED)
    sys.stderr.write("  fetch %s\n" % url)
    archive = os.path.join(root, "source.tar.gz")

    # Retried, unlike the single-file fetches: this is a ~23 MB chunked stream and codeload drops
    # one often enough to matter. Every attempt starts from an EMPTY directory, because a failure
    # can land mid-extraction and leave a partial tree that would otherwise be mistaken for the
    # real one. The marker is written last, so a half-download is never what gets cached.
    last = None
    ATTEMPTS = 4
    for attempt in range(ATTEMPTS):
        shutil.rmtree(root, ignore_errors=True)
        os.makedirs(root, exist_ok=True)
        try:
            with urllib.request.urlopen(url, timeout=120) as resp, open(archive, "wb") as fh:
                shutil.copyfileobj(resp, fh)
            with tarfile.open(archive) as tar:
                # filter="data" refuses absolute paths and links that escape the destination. It
                # is the default from Python 3.14 and a DeprecationWarning before it, so it is
                # passed explicitly.
                try:
                    tar.extractall(root, filter="data")
                except TypeError:
                    tar.extractall(root)
            break
        except (urllib.error.HTTPError, urllib.error.URLError, OSError, tarfile.TarError) as e:
            last = e
            if attempt + 1 == ATTEMPTS:
                raise SystemExit("PKHeX source fetch failed after %d attempts: %s\n  %s"
                                 % (ATTEMPTS, url, last))
            sys.stderr.write("  retry %d/%d after %s\n"
                             % (attempt + 1, ATTEMPTS - 1, type(e).__name__))
            time.sleep(2 * (attempt + 1))
    os.remove(archive)

    inner = [os.path.join(root, n) for n in os.listdir(root)
             if os.path.isdir(os.path.join(root, n))]
    if len(inner) != 1:
        raise SystemExit("PKHeX tarball did not extract to one directory: %s" % inner)
    with open(marker, "w", encoding="utf-8") as fh:
        fh.write(inner[0])
    return inner[0]
