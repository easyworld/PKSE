# Contributing to PKSE

Thanks for helping. PKSE is maintained by one person, so the most useful contributions are the ones that are easy to check.

## Reporting a bug

Use the **Bug report** form on the [new issue](https://github.com/kiasta/PKSE/issues/new/choose) page. The most useful thing you can attach is a debug log:

1. In PKSE, open **Settings** and turn on **Enable Debug Logging**.
2. Reproduce the problem.
3. Attach the newest file from `sdmc:/PKSE/logs/` to the issue.

Logging is off by default and writes nothing until you turn it on. Please search the existing issues first -- if yours is already there, add your log and details to it rather than opening a new one.

Questions and "how do I..." belong in [Discussions -> Q&A](https://github.com/kiasta/PKSE/discussions/categories/q-a). Security problems go through [private reporting](https://github.com/kiasta/PKSE/security/advisories/new) -- see [SECURITY.md](SECURITY.md).

## Suggesting a feature or an improvement

Use **Feature request** for something PKSE can't do at all, and **Enhancement** for something it does that could work better. Say which games it affects and what you'd use it for.

## Pull requests

**Every pull request needs an approved issue. A pull request without one will be closed.**

1. **Open an issue first**, using one of the [issue forms](https://github.com/kiasta/PKSE/issues/new/choose), and describe the change you want to make. New issues are labelled `needs triage`.
2. **Wait for approval before you start.** When the change is agreed, the maintainer comments on the issue and replaces `needs triage` with `approved`. Only an issue labelled `approved` counts.
3. **Link the approved issue from your pull request's description** with a closing keyword, such as `Closes #123`. A plain `#123` mention does not link it. A pull request that doesn't link an approved issue will be closed without review.

Then:

- **Do your work on a new branch.** Fork the repository, create a branch for the change (for example `fix-bank-transfer`), and open the pull request from that branch. Pull requests opened from a fork's `master` or `version-*` branch won't be accepted.
- **Target the newest `version-*` branch**, not `master`. `master` holds the latest release and only changes when a new version ships.
- **The maintainer merges your branch as it is, or makes changes to it first.** A pull request may be adjusted before merging -- to fit the code's conventions, or the rest of the release -- so leave **Allow edits by maintainers** ticked when you open it.
- **One change per pull request.** Say what it changes and why, and how you tested it -- which games, and whether on a console.
- **The build must stay warning-free.** Build with `make` (see the README's *Building PKSE* section) and run `python tools/syntax_check.py --warnings`, which fails on any warning.

### Code

- **C++20 for devkitA64 / libnx, built with `-fno-exceptions -fno-rtti`.** No `throw`/`try`/`catch`, no `dynamic_cast` or `typeid`, and no standard-library calls that report errors by throwing (`.at()`, `std::stoi`).
- **Names are spelled out and say what they are**: `framebuffer`, not `fb`; `boxIndex`, not `i`; `panelWidth`, not `width`. Before naming something new, search for the name the codebase already uses for that concept and reuse it.
- **`g_` marks a mutable global, and it is the only prefix**: no `m_`, no trailing underscores, no `k`-prefixed constants.
- **4 spaces, no tabs, 120 columns.** Keep to the style of the surrounding code, and don't reformat lines you didn't otherwise change.
- **Include guards, not `#pragma once`.**
- **Comments explain *why*, not what.** Describe the problem a fix solves rather than citing an issue number.

### Data

- **Generated tables are never edited by hand.** Species, move, item and location names, learnsets, encounter tables and the other data tables are produced by the scripts in `tools/`. Change the generator and re-run it -- see the README's *Regenerating the data tables* section.
- **Vendored code is left as it is**: `nanovg/`, `memecrypto/` and `include/Libs/`.
- **Save-editing logic follows [PKHeX](https://github.com/kwsch/PKHeX); anything a player reads follows the games.** Offsets, checksums and data come from PKHeX. On-screen text uses the games' own wording.
- **Never commit save files or the `romfs/` assets.**

## License

PKSE is licensed under the [GNU Affero General Public License v3.0](../LICENSE). By opening a pull request, you agree that your contribution is licensed under the same terms.
