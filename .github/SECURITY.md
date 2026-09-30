# Security Policy

## Supported versions

Only the latest release of PKSE receives fixes. If you can, check that the problem still happens on the newest release before reporting it.

## Reporting a vulnerability

**Please don't open a public issue for a security problem.** Report it privately instead:

1. Open the repository's **Security and quality** tab.
2. Choose **Report a vulnerability**.

Or go straight to https://github.com/kiasta/PKSE/security/advisories/new. Only you and the maintainer can see the report.

Please include:

- the PKSE version, and the game or file type involved;
- what someone could do with the problem, and the steps to reproduce it;
- a sample file, if one is needed to reproduce it. A save file holds its trainer's name and ID, so only send one you're happy to hand over.

## What counts as a security problem

PKSE reads files it did not create: game saves, save files copied onto the SD card, and PKSM `.bnk` banks. A crafted file that makes PKSE crash, read or write out of bounds, or damage a save or bank other than the one being opened belongs here.

Ordinary bugs -- a wrong value, a missing feature, a Pokemon the legality checker misjudges -- belong in a normal [issue](https://github.com/kiasta/PKSE/issues/new/choose).

## What to expect

PKSE is maintained by one person in their spare time, so responses are best effort. Once a fix ships, you'll be credited in the advisory and the release notes, unless you'd rather not be.
