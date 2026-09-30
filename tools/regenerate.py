#!/usr/bin/env python3
"""Run every generator in one go, for one checkout or several.

The generators are not part of the build -- `make` compiles the tables they already
produced -- so they are run by hand, occasionally, when the PKHeX-derived data or the
fetched assets need refreshing. Doing that a script at a time means remembering which
scripts exist, which is how one of them went a year without being run.

    python tools/regenerate.py                 # every generator, every checkout found
    python tools/regenerate.py --list          # say what would run, do nothing
    python tools/regenerate.py --tables        # committed data tables only
    python tools/regenerate.py --assets        # romfs downloads only
    python tools/regenerate.py --ref <sha>     # pin PKHEX_REF for this run

WHICH CHECKOUTS. This script's own, plus any sibling directory that is also a PKSE
checkout -- so a working copy and the one it is mirrored into are both brought up to
date from one command. `--repo PATH` names them explicitly instead, and `--list` prints
what was picked before anything runs.

WHAT A CHANGED FILE MEANS. The generated tables are committed, and the diff is the only
review gate the data has, so every file each generator rewrites is hashed before and
after and the changed ones are listed. A table that moves where you did not expect it is
the finding -- upstream data rides along otherwise, mixed into whatever change you came
for. `--ref` pins the commit that reproduces the tables you already have.

COST. The asset fetchers download into `romfs/`, which is gitignored: fonts, the type
icons, the origin marks and ~148 MB of Pokemon renders. They fetch only what is missing,
so a checkout that already has them finishes in seconds; a fresh one does not.
"""
import argparse
import hashlib
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
OWN_REPO = os.path.dirname(HERE)


def is_checkout(path):
    """A directory holding a PKSE tree. Structure rather than name, so a checkout under
    any name is recognised and none has to be listed here."""
    return all(os.path.exists(os.path.join(path, *parts)) for parts in
               (("tools", "pkhex_source.py"), ("include", "Pokemon"), ("src", "Names")))


def find_repos():
    found = [OWN_REPO]
    parent = os.path.dirname(OWN_REPO)
    for name in sorted(os.listdir(parent)):
        sibling = os.path.join(parent, name)
        if os.path.isdir(sibling) and is_checkout(sibling):
            found.append(sibling)
    seen, unique = set(), []
    for path in found:
        real = os.path.realpath(path)
        if real not in seen:
            seen.add(real)
            unique.append(path)
    return unique


def generators(repo):
    """Every gen_*.py in a checkout, split into the ones that write committed tables and
    the ones that download into romfs.

    Read off the filesystem rather than listed here, so a new generator is picked up by
    existing. The split is read the same way -- an asset fetcher is one whose output
    directory is under romfs -- because a hand-kept list of which is which is a second
    list that falls behind the first."""
    tools = os.path.join(repo, "tools")
    tables, assets = [], []
    for name in sorted(os.listdir(tools)):
        if not name.startswith("gen_") or not name.endswith(".py"):
            continue
        with open(os.path.join(tools, name), encoding="utf-8", errors="replace") as handle:
            source = handle.read()
        (assets if '"romfs"' in source else tables).append(name)
    return tables, assets


def table_fingerprint(repo):
    """Hash every committed source file, so a generator's effect can be reported per file."""
    prints = {}
    for top in ("include", "src"):
        for root, _dirs, names in os.walk(os.path.join(repo, top)):
            for name in names:
                if not name.endswith((".h", ".cpp")):
                    continue
                path = os.path.join(root, name)
                with open(path, "rb") as handle:
                    prints[os.path.relpath(path, repo)] = hashlib.sha256(handle.read()).hexdigest()
    return prints


def romfs_fingerprint(repo):
    total = 0
    for root, _dirs, names in os.walk(os.path.join(repo, "romfs")):
        total += len(names)
    return total


def run(repo, script, extra, env):
    started = time.monotonic()
    result = subprocess.run([sys.executable, os.path.join(repo, "tools", script)] + extra,
                            capture_output=True, text=True, env=env)
    return result.returncode, time.monotonic() - started, (result.stderr or result.stdout)[-1500:]


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--repo", action="append", metavar="PATH",
                        help="a checkout to regenerate; repeatable. Default: this one and any "
                             "sibling checkout.")
    parser.add_argument("--tables", action="store_true", help="committed data tables only")
    parser.add_argument("--assets", action="store_true", help="romfs downloads only")
    parser.add_argument("--ref", metavar="REF",
                        help="PKHEX_REF for this run: a commit, tag or branch. Default: whatever "
                             "the generators default to, which is PKHeX's default branch.")
    parser.add_argument("--force", action="store_true",
                        help="re-fetch every asset rather than only what is missing")
    parser.add_argument("--list", action="store_true", help="print the plan and stop")
    args = parser.parse_args()

    repos = [os.path.abspath(p) for p in args.repo] if args.repo else find_repos()
    for repo in repos:
        if not is_checkout(repo):
            sys.exit("not a PKSE checkout: %s" % repo)

    want_tables = args.tables or not args.assets
    want_assets = args.assets or not args.tables

    env = dict(os.environ)
    if args.ref:
        env["PKHEX_REF"] = args.ref
    # PKHEX_LOCAL short-circuits the download to a checkout the caller already has. It is
    # left exactly as the caller set it -- including unset, which is the normal case.

    print("checkouts:")
    for repo in repos:
        print("  %s" % repo)
    plan = []
    for repo in repos:
        tables, assets = generators(repo)
        chosen = (tables if want_tables else []) + (assets if want_assets else [])
        plan.append((repo, chosen))
        print("\n%s\n  %d generator(s): %s" % (repo, len(chosen), ", ".join(chosen)))
    if args.list:
        return 0

    extra = ["--force"] if args.force else []
    failures, changed_total = [], 0

    for repo, chosen in plan:
        _tables, assets = generators(repo)
        print("\n" + "=" * 78 + "\n%s\n" % repo + "=" * 78)
        before, romfs_before = table_fingerprint(repo), romfs_fingerprint(repo)

        for script in chosen:
            is_asset = script in assets
            label = "%-24s" % script
            code, seconds, tail = run(repo, script, extra if is_asset else [], env)
            if code == 0:
                print("  %s ok    %5.1fs" % (label, seconds))
            else:
                print("  %s FAIL  %5.1fs" % (label, seconds))
                for line in tail.strip().split("\n")[-6:]:
                    print("      | %s" % line)
                failures.append((repo, script))

        after, romfs_after = table_fingerprint(repo), romfs_fingerprint(repo)
        touched = sorted(set(after) - set(before)) + \
            sorted(path for path in after if path in before and after[path] != before[path])
        removed = sorted(set(before) - set(after))
        changed_total += len(touched) + len(removed)

        print("\n  committed files changed: %d" % (len(touched) + len(removed)))
        for path in touched[:40]:
            print("    %s" % path)
        if len(touched) > 40:
            print("    ... and %d more" % (len(touched) - 40))
        for path in removed:
            print("    removed: %s" % path)
        if want_assets:
            print("  romfs files: %d -> %d" % (romfs_before, romfs_after))

    print("\n" + "=" * 78)
    if failures:
        print("FAILED: %d generator(s)" % len(failures))
        for repo, script in failures:
            print("  %s  in  %s" % (script, repo))
        return 1
    print("every generator ran; %d committed file(s) changed" % changed_total)
    if changed_total:
        print("Review that diff before committing -- it is the only gate the generated data has.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
