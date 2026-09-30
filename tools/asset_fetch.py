#!/usr/bin/env python3
"""Download files into romfs, for the asset fetchers that have no processing to do.

gen_typeicons.py and gen_fonts.py both want the same four things: skip what is already on disk,
re-fetch everything on --force, run the downloads in parallel, and FAIL LOUD when one of them does
not arrive. A half-fetched asset set is the bad case -- it builds into a .nro perfectly happily and
surfaces on the console as missing art or blank text, with nothing anywhere saying why.

gen_hdsprites.py deliberately does not use this. It lists an upstream tree and downscales every
file it pulls, so its download step is a different shape and sharing one would bend both.

`present()` treats a zero-length file as missing. An interrupted download leaves one behind, and
the shell that used to do this counted it as done -- so the asset never came back on its own.
"""
import os
import sys
import urllib.error
import urllib.request
from concurrent.futures import ThreadPoolExecutor

MAX_WORKERS = 16
USER_AGENT = "pkse-asset-fetch"


def present(path):
    """True when `path` exists and holds something."""
    return os.path.isfile(path) and os.path.getsize(path) > 0


def fetch_bytes(url):
    """The URL's bytes, or None when the server says 404. Every other HTTP error is raised."""
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    try:
        with urllib.request.urlopen(request, timeout=60) as response:
            return response.read()
    except urllib.error.HTTPError as error:
        if error.code == 404:
            return None
        raise


def fetch_all(jobs, force=False, magic=None, max_workers=MAX_WORKERS):
    """Fetch `jobs` -- (label, url, output_path) triples -- and return the ones that failed.

    `magic` is a byte prefix every payload must begin with. A reply that does not begin with it is
    a failure and is NOT written, so a redirect to an error page cannot land on disk as a .png the
    console then fails to decode -- which looks like a missing icon, not like a bad download.
    """
    wanted = [job for job in jobs if force or not present(job[2])]
    failures = []
    downloaded = 0

    def run(job):
        label, url, output_path = job
        data = fetch_bytes(url)
        if data is None:
            return label, "404 (is the pinned ref still valid?)"
        if magic is not None and not data.startswith(magic):
            return label, "unexpected file type, starts with %s" % data[:8].hex()
        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        with open(output_path, "wb") as handle:
            handle.write(data)
        return label, None

    if wanted:
        print("downloading %d of %d ..." % (len(wanted), len(jobs)), flush=True)
        with ThreadPoolExecutor(max_workers=min(max_workers, len(wanted))) as pool:
            for label, error in pool.map(run, wanted):
                if error is None:
                    downloaded += 1
                    print("  %s" % label, flush=True)
                else:
                    failures.append("%s -- %s" % (label, error))

    print("downloaded=%d  already-present=%d  failed=%d"
          % (downloaded, len(jobs) - len(wanted), len(failures)))
    for failure in failures:
        print("ERROR: %s" % failure, file=sys.stderr)
    return failures
