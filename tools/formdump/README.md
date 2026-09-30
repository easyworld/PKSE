# formdump — PKHeX's own form lists, dumped for tools/gen_formnames.py

PKSE does not re-implement `FormConverter`. It is 1159 lines of context-dependent C#, and a port
would be a second copy of logic that has to agree with the first forever. This calls the real thing
instead and writes JSON: `{ language: { species: [form name per form id] } }`.

It is the ONE generator input that needs more than a text fetch: PKHeX.Core has to be COMPILED, so
this needs the .NET SDK. It does not need a checkout -- the source is downloaded at the resolved
PKHEX_REF and cached under `tools/.pkhex_cache/<sha>/_repo/`, like every other PKHeX input.
PKHEX_LOCAL points it at a checkout instead, for someone who already has one.

    python tools/gen_formnames.py
    PKHEX_LOCAL=~/repos/PKHeX python tools/gen_formnames.py

THE EMITTED FILE RECORDS WHAT IT WAS BUILT FROM, and that has to come from the resolved ref rather
than from git: a downloaded tarball carries no `.git`, and `git -C <dir> rev-parse HEAD` does not
fail there -- it walks UP and answers with the enclosing repository, which is PKSE's own. The
stamp then reads `PKHeX commit <PKSE's own HEAD>`, which looks exactly like a right answer.

WHY A UNION ACROSS CONTEXTS. `GetFormList` answers per `EntityContext`, and PKSE names every form a
buffer can hold -- including ones a later generation dropped. Raticate's Totem form exists only in
the Gen 7 list, so asking Gen 9 alone silently loses it. The dump merges per form index, newest
context first.
