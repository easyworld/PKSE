#!/usr/bin/env python3
"""Generate include/Pokemon/LearnsetTable.h + src/Pokemon/LearnsetTable.cpp
from PKHeX's binary learnset resources (all six supported games).

PKSE needs a fast set-membership query for feature #15 (highlight legal moves in
the move picker) and legality Layer-2 (move learnability):
    isLearnable(species, form, group, moveId) -> bool

For each (species, form) present in a game, this table stores a *bitset* over move
ids: bit M is set iff that species+form can legally KNOW move M in that game. The
bit pool is the UNION of every learn method PKHeX models for that exact species+
form -- mirroring each LearnSource's GetAllMoves at max level:

    level-up  U  shared egg  U  TM/TR/HM  U  tutor  U  reminder  U  enhanced-tutor

That per-species pool is then UNIONED WITH ITS WHOLE PRE-EVOLUTION CHAIN, because a
Pokemon keeps the moves it knew before evolving: a Raichu may legally know Double
Kick even though only Pikachu learns it. The chain comes from PKHeX's evolution
binaries (evos_*.pkl, reverse lineage per EvolutionReversePersonal); ancestors that
are absent from the game are skipped, since you cannot have evolved from one there.

This fold happens HERE, at generation time, not at runtime -- the pool is not
deduplicated, so every row already owns its own bitset and the extra bits are free.
Getting it wrong is not merely cosmetic: isLearnable() also drives the cross-game
transfer sanitizer in Conversion::convert(), which CLEARS any move it reports as
unlearnable, so a missing pre-evolution move is silently deleted off a legitimate
Pokemon. (The reverse direction is a Bad Egg, so the sanitizer cannot just be
loosened -- the pool has to actually be right.)

--------------------------------------------------------------------------------
Per-game assembly (PKHeX.Core/Legality/LearnSource/Sources/LearnSource*.GetAllMoves
+ the matching PersonalInfo*.cs bitflag layout). Move-permit lists are parsed from
the C# source; TM/tutor flags are read from each game's personal binary.

  GG (7GG)   lvlmove_gg + TM(60 flags@0x28 x MachineMoves[LearnSource7GG])
             + partner Pikachu(25,f8)/Eevee(133,f1) tutor moves. No eggs.
  SWSH       lvlmove_swsh + eggmove_swsh(shared, via HatchSpecies@0x56)
             + TM(100@0x28 x MachineMovesTechnical) + TR(100@0x3C x MachineMovesRecord)
             + TypeTutor(8 bits@0x38 x TypeTutorMoves) + TutorSpecial(18@0xA8 x
             SpecialTutorMoves) + enhanced(Rotom/Necrozma) + Calyrex(898,f0)->Agility.
  BDSP       lvlmove_bdsp + eggmove_bdsp(shared, indexed by HatchSpecies@0x3E)
             + TM(100@0x28 x MachineMoves) + TypeTutor(4 bits@0x38 x TypeTutorMoves)
             + enhanced(Rotom).
  PLA (8LA)  lvlmove_la + mastery_la + MoveShop(61-bit ulong@0xA8 x MoveShopMoves)
             + enhanced(Rotom). No eggs. (mastery_la proven subset of level-up U
             move-shop, so it is a redundant no-op; included for fidelity.)
  SV (9SV)   lvlmove_sv + eggmove_sv(shared, via HatchSpecies@0x24)
             + TM(230@0x2C x MachineMoves) + reminder_sv + enhanced(Rotom/Necrozma).
  ZA (9ZA)   lvlmove_za + plus_za + TM(flags@0x2C x MachineMoves9ZA, 160 defined of a
             230-bit field). No eggs. (plus_za proven subset of level-up U TM per
             LegendsZAVerifier.CanLearnMovePlus, which validates Plus moves against
             TM+level-up; the plus_za learnset itself grants no new moves. Redundant
             no-op, included for fidelity.)

Binary formats (little-endian):
  * BinLinkerAccessor16 container: [2-byte ASCII magic][u16 count][u16 offset_i...],
    entry i = data[offset_i:offset_{i+1}]. Arrays indexed by that game's
    PersonalTable.GetFormIndex(species, form) -- the same FormIndex/HasForm
    redirection gen_personal.py replicates (verified vs PersonalInfo.FormIndex).
    Exception: eggmove_bdsp is indexed by raw species id (PKHeX optimization).
  * Level-up / mastery / plus entry: move[count] (u16) then level[count] (u8),
    count = len/3. We take ALL moves (ignore the level cap).
  * Egg / reminder entry: a flat u16[] move list.
  * TM/TR/tutor flags live in the personal entry (offsets above).

Enhanced tutor: Rotom(479) forms 1..5 -> Overheat/HydroPump/Blizzard/AirSlash/
LeafStorm; Necrozma(800) form 1 -> SunsteelStrike, form 2 -> MoongeistBeam.

Max move id per game (PKHeX Legal.cs): GG 742, SWSH/BDSP 826, PLA 850, SV 919
(MalignantChain), ZA 920 (NihilLight). Combined max = 920. All bitsets use a single
UNIFORM width sized to hold move id 920 (LEARN_BITSET_BYTES), so the move picker can
iterate the full move space against any game's bitset without a per-game bound.

--------------------------------------------------------------------------------
Row layout mirrors PersonalInfoTable exactly: rows 0..1025 are the form-0 entries
(indexed by National Dex id), rows 1026.. are alternate forms, addressed via a
formIndex redirection (form N -> LEARN_FORM_INDEX[species] + N - 1). The union form
count per species is the max FormCount across all six games, so a LearnsetTable row
index and a PersonalInfoTable row index are identical for any (species, form).

Storage (per-game bitsets, design (a)): one flat byte pool per game holds the
concatenated bitsets of the species+forms present in that game; a per-row u16 block
index (0xFFFF = absent) maps a row to its bitset (byte offset = block * width).

Regenerate with:  python tools/gen_learnsets.py
Pulls every PKHeX resource + source file it reads from GitHub on demand
(tools/pkhex_source.py); no local PKHeX checkout required.
"""
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pkhex_source import pkhex_path  # noqa: E402


def BYTE(*parts):
    """PKHeX.Core/Resources/byte/<...> -> local (fetched) path."""
    return pkhex_path("Resources/byte/" + "/".join(parts))


def MISC(*parts):
    """PKHeX.Core/Resources/legality/misc/<...> -> local (fetched) path."""
    return pkhex_path("Resources/legality/misc/" + "/".join(parts))


def CORE(relpath):
    """PKHeX.Core/<relpath> -> local (fetched) path."""
    return pkhex_path(relpath)


ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_H = os.path.join(ROOT, "include", "Pokemon", "LearnsetTable.h")
OUT_CPP = os.path.join(ROOT, "src", "Pokemon", "LearnsetTable.cpp")
SPECIES_NAMES_SRC = os.path.join(ROOT, "src", "Names", "SpeciesNames.cpp")

MAX_SPECIES = 1025            # National Dex #1025 Pecharunt (PKHeX MaxSpeciesID_9)
BASE_ROWS = MAX_SPECIES + 1   # indices 0..1025
MELTAN, MELMETAL, PIKACHU, EEVEE = 808, 809, 25, 133
ROTOM, NECROZMA, CALYREX = 479, 800, 898

# Uniform bitset width: hold move ids 0..MAX_MOVE_ID (920 = ZA's NihilLight).
MAX_MOVE_ID = 920
WIDTH = (MAX_MOVE_ID + 8) // 8  # 116 bytes

# Games in presence-bit order (matches PersonalInfoTable's PersonalGameBit).
GAMES = ["GG", "SWSH", "BDSP", "PLA", "SV", "ZA"]
GAMES_TO_EMIT = GAMES + ["FRLG"]  # all seven games; FRLG has no presence bit so it is not in GAMES

# Personal binary format per game: (filename, SIZE, formIdx_off, formCnt_off,
# maxspecies, present_kind). Mirrors gen_personal.py's FMT.
PERSONAL_FMT = {
    "GG":   ("personal_gg",   0x54, 0x1C, 0x20, 809,  "gg"),
    "SWSH": ("personal_swsh", 0xB0, 0x1E, 0x20, 898,  "flag21"),
    "BDSP": ("personal_bdsp", 0x44, 0x1E, 0x20, 493,  "bdsp"),
    "PLA":  ("personal_la",   0xB0, 0x1E, 0x20, 905,  "flag21"),
    "SV":   ("personal_sv",   0x50, 0x18, 0x1A, 1025, "flag1C"),
    "ZA":   ("personal_za",   0x50, 0x18, 0x1A, 1000, "flag1C"),
}

# Enums::GameVersion values that map to each game group (group + individual ids).
GAME_VERSION_IDS = {
    "GG":   ["GG", "GP", "GE"],
    "SWSH": ["SWSH", "SW", "SH"],
    "BDSP": ["BDSP", "BD", "SP"],
    "PLA":  ["PLA"],
    "SV":   ["SV", "SL", "VL"],
    "ZA":   ["ZA"],
    "FRLG": ["FRLG", "FR", "LG"],
}


# ----------------------------------------------------------------------------
# C# source + binary parsing
# ----------------------------------------------------------------------------
def _load(path):
    with open(path, "rb") as fh:
        return fh.read()


def load_move_ids():
    """Parse PKHeX's Move enum (Game/Enums/Move.cs) into a name -> id dict."""
    path = CORE("Game/Enums/Move.cs")
    with open(path, encoding="utf-8") as fh:
        text = fh.read()
    body = text[text.index("{", text.index("enum Move")):]
    names = re.findall(r'^\s*([A-Za-z_][A-Za-z0-9_]*)\s*,', body, re.M)
    if "MalignantChain" not in names:
        raise SystemExit("Move.cs parse failed")
    return {n: i for i, n in enumerate(names)}


MOVE = load_move_ids()


def parse_move_list(cs_relpath, array_name, expect=None):
    """Parse a `<array_name> => [ ... ];` list of move ids from a C# source file.
    Accepts both numeric literals and `(int)Move.Name` tokens (comments stripped)."""
    with open(CORE(cs_relpath), encoding="utf-8") as fh:
        text = fh.read()
    m = re.search(re.escape(array_name) + r"\s*=>\s*\[(.*?)\];", text, re.S)
    if not m:
        raise SystemExit(f"{array_name} not found in {cs_relpath}")
    seg = re.sub(r"//[^\n]*", "", m.group(1))
    out = []
    for name, num in re.findall(r"\(int\)Move\.([A-Za-z0-9_]+)|(\d+)", seg):
        out.append(MOVE[name] if name else int(num))
    if expect is not None and len(out) != expect:
        raise SystemExit(f"{cs_relpath}:{array_name} has {len(out)}, expected {expect}")
    return out


def bin_entries(data):
    """Unpack a BinLinkerAccessor16 container into a list of byte spans."""
    length = struct.unpack_from("<H", data, 2)[0]
    return [data[s:e] for (s, e) in
            (struct.unpack_from("<HH", data, 4 + i * 2) for i in range(length))]


def read_levelup(path):
    """Level-up / mastery / plus blob -> per-index list of move ids (levels ignored)."""
    res = []
    for e in bin_entries(_load(path)):
        if len(e) == 0:
            res.append([])
            continue
        count = len(e) // 3
        res.append(list(struct.unpack_from("<%dH" % count, e, 0)))
    return res


def read_movesource(path):
    """Egg / reminder blob -> per-index list of move ids."""
    res = []
    for e in bin_entries(_load(path)):
        n = len(e) // 2
        res.append(list(struct.unpack_from("<%dH" % n, e, 0)) if n else [])
    return res


# ----------------------------------------------------------------------------
# Personal table (form-index redirection + per-game presence), mirrors gen_personal
# ----------------------------------------------------------------------------
class Personal:
    def __init__(self, key):
        fn, size, fi, fc, maxsp, pk = PERSONAL_FMT[key]
        self.key = key
        self.raw = _load(BYTE("personal", fn))
        self.SIZE, self.fi, self.fc, self.maxsp, self.pk = size, fi, fc, maxsp, pk
        if len(self.raw) % size != 0:
            raise SystemExit(f"{key}: {len(self.raw)} not a multiple of SIZE {size}")
        self.count = len(self.raw) // size

    def ent(self, idx):
        return self.raw[idx * self.SIZE:(idx + 1) * self.SIZE]

    def u16(self, e, off):
        return struct.unpack_from("<H", e, off)[0]

    def form_stats_index(self, sp):
        return self.u16(self.ent(sp), self.fi)

    def form_count(self, sp):
        return self.ent(sp)[self.fc]

    def has_form(self, sp, form):
        return form != 0 and self.form_stats_index(sp) > 0 and form < self.form_count(sp)

    def form_index(self, sp, form):
        if sp > self.maxsp or sp >= self.count:
            return 0
        if not self.has_form(sp, form):
            return sp
        return self.form_stats_index(sp) + form - 1

    def present(self, sp, form):
        if sp > self.maxsp:
            return False
        if self.pk == "gg":
            if not ((1 <= sp <= 151) or sp in (MELTAN, MELMETAL)):
                return False
            if form == 0:
                return True
            if sp == PIKACHU:
                return form == 8
            return self.has_form(sp, form)
        if self.pk == "bdsp":
            return True if form == 0 else self.has_form(sp, form)
        if form != 0 and not self.has_form(sp, form):
            return False
        e = self.ent(self.form_index(sp, form))
        if self.pk == "flag1C":
            return e[0x1C] != 0
        return ((e[0x21] >> 6) & 1) == 1


# ----------------------------------------------------------------------------
# Gen 3 (FireRed/LeafGreen) personal table -- shaped unlike every other game's
# ----------------------------------------------------------------------------
# Three things differ from Gen 5+ and each one is a trap:
#   1. The table is FLAT and indexed by NATIONAL species. Gen 3 saves store an INTERNAL
#      species id, but PKHeX pre-converts to national ordering when building the binary,
#      so no SpeciesConverter is needed here (only when reading a save).
#   2. There are NO per-form entries -- PersonalTable3.GetFormIndex(species, form) is just
#      `species`, so every form of a species shares one move pool.
#   3. TM/HM compatibility is NOT in the personal entry. It lives in a separate blob
#      (hmtm_g3.pkl) that uses the **u32-offset** BinLinkerAccessor, not the u16 one every
#      other resource uses. 58 bits per species: TM01-50 at bits 0-49, HM01-08 at 50-57.
G3_MAX_SPECIES = 386
G3_PERSONAL_SIZE = 0x1C
G3_COUNT_TM = 50

# PersonalInfo3.MachineMovesTechnical / MachineMovesHidden, in TM/HM bit order.
G3_TM_MOVES = [
    264, 337, 352, 347,  46,  92, 258, 339, 331, 237,
    241, 269,  58,  59,  63, 113, 182, 240, 202, 219,
    218,  76, 231,  85,  87,  89, 216,  91,  94, 247,
    280, 104, 115, 351,  53, 188, 201, 126, 317, 332,
    259, 263, 290, 156, 213, 168, 211, 285, 289, 315,
]
G3_HM_MOVES = [15, 19, 57, 70, 148, 249, 127, 291]


def bin_entries32(data):
    """Unpack a u32-offset BinLinkerAccessor container (hmtm_g3.pkl only)."""
    length = struct.unpack_from("<H", data, 2)[0]
    return [data[s:e] for (s, e) in
            (struct.unpack_from("<II", data, 4 + i * 4) for i in range(length))]


class Personal3:
    """Gen 3 personal table + TM/HM flags. Exposes the same surface the rest of this
    script expects from `Personal`, so read_evolutions() and the fold work unchanged."""

    def __init__(self, fn="personal_fr"):
        self.raw = _load(BYTE("personal", fn))
        self.maxsp = G3_MAX_SPECIES
        self.count = len(self.raw) // G3_PERSONAL_SIZE
        self.tmhm = bin_entries32(_load(BYTE("personal", "hmtm_g3.pkl")))

    # Gen 3 evolution data is species-indexed too (PKHeX EvolutionTree.Evolves3 uses
    # GetViaSpecies, not GetViaPersonal), so the form index is the identity and there is
    # exactly one "form" to walk when building the reverse lineage.
    def form_count(self, sp):
        return 1

    def form_index(self, sp, form):
        return sp

    def present(self, sp, form):
        if sp == 0 or sp > G3_MAX_SPECIES:
            return False
        if form == 0:
            return True
        # PersonalTable3.IsPresentInGame -- only these three have forms in Gen 3.
        return (sp == 201 and form < 28) or (sp == 351 and form < 4) or (sp == 386 and form < 4)

    def tm_bit(self, sp, i):
        e = self.tmhm[sp] if sp < len(self.tmhm) else b""
        return bool((e[i >> 3] >> (i & 7)) & 1) if (i >> 3) < len(e) else False


# ----------------------------------------------------------------------------
# Evolution lineage (reverse map), for folding pre-evolution moves into the pool
# ----------------------------------------------------------------------------
# evos_<key>.pkl is the same BinLinkerAccessor16 container as the learnset blobs,
# indexed by the game's PersonalTable.GetFormIndex(species, form). Each entry is an
# array of 8-byte EvolutionMethod records (PKHeX EvolutionSet.GetMethod):
#     [0] type   [2:4] arg   [4:6] DESTINATION species   [6] dest form   [7] level
# Form 0xFF is PKHeX's AnyForm sentinel = "evolves into the same form it already is".
EVO_RESOURCE = {"GG": "gg", "SWSH": "ss", "BDSP": "bs", "PLA": "la", "SV": "sv", "ZA": "za",
                "FRLG": "g3"}
EVO_ANY_FORM = 0xFF
EVO_SIZE = 8
MAX_EVO_DEPTH = 4   # longest real chain is 3 (e.g. Bulbasaur->Ivysaur->Venusaur); +1 slack


def read_evolutions(key, personal):
    """evos_<key>.pkl -> reverse lineage {(dest_sp, dest_form): (src_sp, src_form)}.

    Mirrors PKHeX EvolutionReversePersonal.GetLineage: walk every source species+form,
    read the evolutions it leads TO, and register the reverse edge. Where a destination
    has more than one registered source we keep the FIRST, matching PKHeX's
    GetPreEvolutions ("no convergent evolutions; first method is enough").
    """
    path = BYTE("evolve", "evos_%s.pkl" % key)
    entries = bin_entries(_load(path))
    rev = {}
    last = min(personal.maxsp, personal.count - 1)
    for sp in range(1, last + 1):
        for form in range(personal.form_count(sp)):
            idx = personal.form_index(sp, form)
            if idx >= len(entries):
                continue
            e = entries[idx]
            for off in range(0, len(e) - (EVO_SIZE - 1), EVO_SIZE):
                dsp = struct.unpack_from("<H", e, off + 4)[0]
                if dsp == 0:
                    break
                dform = e[off + 6]
                if dform == EVO_ANY_FORM:
                    dform = form
                rev.setdefault((dsp, dform), (sp, form))
    return rev


def preevo_chain(rev, sp, form):
    """Full pre-evolution chain for (sp, form), nearest ancestor first."""
    out = []
    cur = (sp, form)
    seen = {cur}
    while len(out) < MAX_EVO_DEPTH:
        prev = rev.get(cur)
        if prev is None or prev in seen:   # `seen` guards against a cyclic data error
            break
        out.append(prev)
        seen.add(prev)
        cur = prev
    return out


# ----------------------------------------------------------------------------
# Per-game learnable-pool assemblers (each == that game's LearnSource.GetAllMoves)
# ----------------------------------------------------------------------------
ROTOM_FORM_MOVE = {1: MOVE["Overheat"], 2: MOVE["HydroPump"], 3: MOVE["Blizzard"],
                   4: MOVE["AirSlash"], 5: MOVE["LeafStorm"]}
SUNSTEEL, MOONGEIST, AGILITY = MOVE["SunsteelStrike"], MOVE["MoongeistBeam"], MOVE["Agility"]


def _tmbit(e, base, i):
    return (e[base + (i >> 3)] >> (i & 7)) & 1


def _add_rotom(pool, sp, form):
    if sp == ROTOM and form in ROTOM_FORM_MOVE:
        pool.add(ROTOM_FORM_MOVE[form])


def _add_necrozma(pool, sp, form):
    if sp == NECROZMA and form == 1:
        pool.add(SUNSTEEL)
    elif sp == NECROZMA and form == 2:
        pool.add(MOONGEIST)


def _hatch_form_everstone(p, e, local_off, regflag_off, regidx_off):
    local = p.u16(e, local_off) & 0xFF
    regf = p.u16(e, regflag_off)
    regidx = p.u16(e, regidx_off) & 0xFF
    return regidx if (regf & 1) else local


class Assembler:
    def __init__(self, key):
        self.p = Personal(key)

    def _levelup(self, path):
        lvl = read_levelup(path)
        if self.p.count != len(lvl):
            raise SystemExit(f"{self.p.key}: personal count {self.p.count} != levelup {len(lvl)}")
        return lvl


class GGAssembler(Assembler):
    def __init__(self):
        super().__init__("GG")
        self.lvl = self._levelup(BYTE("levelup", "lvlmove_gg.pkl"))
        cs = "Legality/LearnSource/Sources/LearnSource7GG.cs"
        self.machine = parse_move_list(cs, "MachineMoves", 60)
        self.pika = parse_move_list(cs, "TutorStarterPikachu", 3)
        self.eevee = parse_move_list(cs, "TutorStarterEevee", 8)

    def assemble(self, sp, form):
        idx = self.p.form_index(sp, form)
        e = self.p.ent(idx)
        pool = set(self.lvl[idx])
        for i in range(60):
            if _tmbit(e, 0x28, i):
                pool.add(self.machine[i])
        if sp == PIKACHU and form == 8:
            pool.update(self.pika)
        elif sp == EEVEE and form == 1:
            pool.update(self.eevee)
        return pool


class SWSHAssembler(Assembler):
    def __init__(self):
        super().__init__("SWSH")
        self.lvl = self._levelup(BYTE("levelup", "lvlmove_swsh.pkl"))
        self.egg = read_movesource(BYTE("eggmove", "eggmove_swsh.pkl"))
        cs = "PersonalInfo/Info/PersonalInfo8SWSH.cs"
        self.tech = parse_move_list(cs, "MachineMovesTechnical", 100)
        self.record = parse_move_list(cs, "MachineMovesRecord", 100)
        self.type_tutor = parse_move_list(cs, "TypeTutorMoves", 8)
        self.special_tutor = parse_move_list(cs, "SpecialTutorMoves", 18)

    def assemble(self, sp, form):
        p = self.p
        idx = p.form_index(sp, form)
        e = p.ent(idx)
        pool = set(self.lvl[idx])
        # Shared egg (HatchSpecies@0x56, HatchFormIndexEverstone from 0x58/0x5A/0x5E)
        hatch_sp = p.u16(e, 0x56)
        hatch_form = _hatch_form_everstone(p, e, 0x58, 0x5A, 0x5E)
        eidx = p.form_index(hatch_sp, hatch_form)
        if eidx < len(self.egg):
            pool.update(self.egg[eidx])
        for i in range(100):                    # TM @0x28
            if _tmbit(e, 0x28, i):
                pool.add(self.tech[i])
        for i in range(100):                    # TR @0x3C
            if _tmbit(e, 0x3C, i):
                pool.add(self.record[i])
        for i in range(8):                      # TypeTutor: 8 bits in byte 0x38
            if (e[0x38] >> i) & 1:
                pool.add(self.type_tutor[i])
        for i in range(18):                     # TutorSpecial (Armor) @0xA8, 3 bytes
            if _tmbit(e, 0xA8, i):
                pool.add(self.special_tutor[i])
        _add_rotom(pool, sp, form)
        _add_necrozma(pool, sp, form)
        if sp == CALYREX and form == 0:         # Agility Calyrex without TR glitch
            pool.add(AGILITY)
        return pool


class BDSPAssembler(Assembler):
    def __init__(self):
        super().__init__("BDSP")
        self.lvl = self._levelup(BYTE("levelup", "lvlmove_bdsp.pkl"))
        # eggmove_bdsp is indexed by raw species id (PKHeX optimization).
        self.egg = read_movesource(BYTE("eggmove", "eggmove_bdsp.pkl"))
        cs = "PersonalInfo/Info/PersonalInfo8BDSP.cs"
        self.machine = parse_move_list(cs, "MachineMoves", 100)
        self.type_tutor = parse_move_list(cs, "TypeTutorMoves", 4)

    def assemble(self, sp, form):
        p = self.p
        idx = p.form_index(sp, form)
        e = p.ent(idx)
        pool = set(self.lvl[idx])
        hatch_sp = p.u16(e, 0x3E)               # shared egg indexed by HatchSpecies
        if hatch_sp < len(self.egg):
            pool.update(self.egg[hatch_sp])
        for i in range(100):                    # TM @0x28
            if _tmbit(e, 0x28, i):
                pool.add(self.machine[i])
        for i in range(4):                      # TypeTutor: 4 bits in byte 0x38
            if (e[0x38] >> i) & 1:
                pool.add(self.type_tutor[i])
        _add_rotom(pool, sp, form)
        return pool


class PLAAssembler(Assembler):
    def __init__(self):
        super().__init__("PLA")
        self.lvl = self._levelup(BYTE("levelup", "lvlmove_la.pkl"))
        self.mastery = read_levelup(MISC("mastery_la.pkl"))
        self.moveshop = parse_move_list("PersonalInfo/Info/PersonalInfo8LA.cs",
                                        "MoveShopMoves", 61)

    def assemble(self, sp, form):
        p = self.p
        idx = p.form_index(sp, form)
        e = p.ent(idx)
        pool = set(self.lvl[idx])
        if idx < len(self.mastery):             # redundant subset, included for fidelity
            pool.update(self.mastery[idx])
        bits = struct.unpack_from("<Q", e, 0xA8)[0]  # MoveShop: 61-bit ulong
        for i in range(61):
            if (bits >> i) & 1:
                pool.add(self.moveshop[i])
        _add_rotom(pool, sp, form)
        return pool


class ZAAssembler(Assembler):
    def __init__(self):
        super().__init__("ZA")
        self.lvl = self._levelup(BYTE("levelup", "lvlmove_za.pkl"))
        self.plus = read_levelup(MISC("plus_za.pkl"))
        self.machine = parse_move_list("PersonalInfo/Info/PersonalInfo9ZA.cs",
                                       "MachineMoves", 160)  # 160 defined of a 230-bit field

    def assemble(self, sp, form):
        p = self.p
        idx = p.form_index(sp, form)
        e = p.ent(idx)
        pool = set(self.lvl[idx])
        if idx < len(self.plus):                # redundant subset, included for fidelity
            pool.update(self.plus[idx])
        for i in range(230):                    # TM @0x2C (only 0..159 ever set)
            if _tmbit(e, 0x2C, i):
                if i >= len(self.machine):
                    raise SystemExit(f"ZA TM bit {i} set but MachineMoves has {len(self.machine)}")
                pool.add(self.machine[i])
        return pool


class SVAssembler(Assembler):
    def __init__(self):
        super().__init__("SV")
        self.lvl = self._levelup(BYTE("levelup", "lvlmove_sv.pkl"))
        self.egg = read_movesource(BYTE("eggmove", "eggmove_sv.pkl"))
        self.rem = read_movesource(BYTE("personal", "reminder_sv.pkl"))
        self.machine = parse_move_list("PersonalInfo/Info/PersonalInfo9SV.cs",
                                       "MachineMoves", 230)

    def assemble(self, sp, form):
        p = self.p
        idx = p.form_index(sp, form)
        e = p.ent(idx)
        pool = set(self.lvl[idx])
        # Shared egg (HatchSpecies@0x24, HatchFormIndexEverstone from 0x26/0x28/0x2A)
        hatch_sp = p.u16(e, 0x24)
        hatch_form = _hatch_form_everstone(p, e, 0x26, 0x28, 0x2A)
        eidx = p.form_index(hatch_sp, hatch_form)
        if eidx < len(self.egg):
            pool.update(self.egg[eidx])
        for i in range(230):                    # TM @0x2C
            if _tmbit(e, 0x2C, i):
                pool.add(self.machine[i])
        if idx < len(self.rem):                 # move reminder
            pool.update(self.rem[idx])
        _add_rotom(pool, sp, form)
        _add_necrozma(pool, sp, form)
        return pool


class FRLGAssembler:
    """LearnSource3FR.GetAllMoves: level-up U TM U HM U shared egg U the 3 starter tutors.

    FR and LG share this pool -- PKHeX keeps separate lvlmove_fr/lvlmove_lg blobs, but the
    only differences are version-exclusive *encounters*, not learnsets, and PKSE treats
    FR/LG as one GameVersion group. Uses lvlmove_fr as the representative.
    """

    # LearnSource3FR.GetIsTutor -- the three starter-exclusive tutor moves, and the only
    # tutor moves the Gen 3 learn source models at all.
    TUTOR = {6: MOVE["BlastBurn"], 9: MOVE["HydroCannon"], 3: MOVE["FrenzyPlant"]}

    def __init__(self):
        self.p = Personal3()
        self.lvl = read_levelup(BYTE("levelup", "lvlmove_fr.pkl"))
        self.egg = read_movesource(BYTE("eggmove", "eggmove_rs.pkl"))

    def assemble(self, sp, form):
        pool = set()
        if sp < len(self.lvl):
            pool.update(self.lvl[sp])
        if sp < len(self.egg):
            pool.update(self.egg[sp])
        for i, mv in enumerate(G3_TM_MOVES):
            if self.p.tm_bit(sp, i):
                pool.add(mv)
        for i, mv in enumerate(G3_HM_MOVES):
            if self.p.tm_bit(sp, G3_COUNT_TM + i):
                pool.add(mv)
        tutor = self.TUTOR.get(sp)
        if tutor:
            pool.add(tutor)
        pool.discard(0)
        return pool


ASSEMBLERS = {"GG": GGAssembler, "SWSH": SWSHAssembler, "BDSP": BDSPAssembler,
              "FRLG": FRLGAssembler,
              "PLA": PLAAssembler, "SV": SVAssembler, "ZA": ZAAssembler}


# ----------------------------------------------------------------------------
# Row enumeration (identical to PersonalInfoTable)
# ----------------------------------------------------------------------------
def load_species_names():
    with open(SPECIES_NAMES_SRC, encoding="utf-8") as fh:
        text = fh.read()
    m = re.search(r"SPECIES_NAMES\[\]\s*=\s*\{(.*?)\};", text, re.S)
    if not m:
        raise SystemExit("could not locate SPECIES_NAMES[] in " + SPECIES_NAMES_SRC)
    names = re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1))
    if len(names) <= MAX_SPECIES:
        raise SystemExit(f"SpeciesNames only has {len(names)} entries")
    return names


def build_rows(personals):
    union_fc = [1] * BASE_ROWS
    for sp in range(BASE_ROWS):
        fc = 1
        for t in personals.values():
            if sp <= t.maxsp and sp < t.count:
                fc = max(fc, t.form_count(sp))
        union_fc[sp] = fc

    rows = [(sp, 0) for sp in range(BASE_ROWS)]
    form_index = [0] * BASE_ROWS
    next_index = BASE_ROWS
    for sp in range(BASE_ROWS):
        if union_fc[sp] > 1:
            form_index[sp] = next_index
            for form in range(1, union_fc[sp]):
                rows.append((sp, form))
            next_index += union_fc[sp] - 1
    return rows, form_index


# ----------------------------------------------------------------------------
# Emission
# ----------------------------------------------------------------------------
HDR_TMPL = '''/**
 * LearnsetTable.h - Per-species+form "learnable move" bitsets (feature #15 /
 *                   legality Layer-2: is a move learnable by this Pokemon?)
 *
 * Auto-generated by tools/gen_learnsets.py from PKHeX's binary learnset resources.
 * DO NOT EDIT BY HAND -- rerun the generator instead.
 *
 * For a (species, form) present in a game, that game's bitset holds bit M set =>
 * the species+form can legally KNOW move M in that game (union of level-up / egg /
 * TM / TR / tutor / reminder, then unioned with its whole pre-evolution chain --
 * an evolved Pokemon keeps what it learned earlier). Rows mirror
 * PersonalInfoTable (base rows 0..{MAX}, then alt forms via formIndex). All six
 * supported games are carried: GG, SWSH, BDSP, PLA, SV, ZA. Bitsets share one
 * uniform width sized to hold move id {MAXMOVE} (ZA's NihilLight).
 */
#ifndef PKM_LEARNSET_TABLE_H
#define PKM_LEARNSET_TABLE_H

#include <cstdint>
#include <cstddef>

#include "Enums/GameVersion.h"

namespace Pokemon {{
    constexpr uint16_t LEARN_MAX_SPECIES = {MAX};
    constexpr uint16_t LEARN_MAX_MOVE_ID = {MAXMOVE};   // ZA NihilLight; move space 0..{MAXMOVE}
    constexpr size_t   LEARN_BITSET_BYTES = {BYTES};    // uniform width for every game

    // Pointer to the {BYTES}-byte learnable bitset for (species, form) in the given
    // game group, or nullptr if the species+form is absent from that game. Accepts a
    // GameVersion group (GG/SWSH/BDSP/PLA/SV/ZA) or an individual version (e.g.
    // GP/GE, SW/SH, BD/SP, SL/VL) -- both resolve to the right game.
    const uint8_t* getLearnableBits(uint16_t species, uint8_t form, Enums::GameVersion group);

    // True iff moveId is in the (species, form) learnable pool for the game group.
    bool isLearnable(uint16_t species, uint8_t form, Enums::GameVersion group, uint16_t moveId);
}}

#endif  // PKM_LEARNSET_TABLE_H
'''


def emit_hex_pool(parts, name, blocks, width, names):
    parts.append("    const uint8_t %s[%d * %d] = {\n" % (name, max(len(blocks), 1), width))
    if not blocks:
        parts.append("        0,  // (no present species -- placeholder)\n")
    for (sp, form, bits, count) in blocks:
        nm = names[sp] if sp < len(names) else "?"
        label = f"#{sp} {nm}" + (f" form {form}" if form else "")
        parts.append(f"        // {label}  ({count} moves)\n")
        for r in range(0, width, 16):
            chunk = bits[r:r + 16]
            parts.append("        " + "".join("0x%02X, " % b for b in chunk).rstrip() + "\n")
    parts.append("    };\n\n")


def emit_u16_array(parts, name, values, per_line, decl_count):
    parts.append("    const uint16_t %s[%s] = {\n" % (name, decl_count))
    line = "        "
    for i, v in enumerate(values):
        line += "0x%04X," % (v & 0xFFFF)
        if (i + 1) % per_line == 0:
            parts.append(line + "\n")
            line = "        "
        else:
            line += " "
    if line.strip():
        parts.append(line.rstrip() + "\n")
    parts.append("    };\n\n")


def main():
    names = load_species_names()
    personals = {g: Personal(g) for g in GAMES}
    personals["FRLG"] = Personal3()   # Gen 3 is shaped differently -- see Personal3
    rows, form_index = build_rows(personals)
    print("union rows:", len(rows), "(base", BASE_ROWS, "+ alt", len(rows) - BASE_ROWS, ")")

    # Build every game's bitsets for its present species+forms.
    game_blocks = {}   # game -> [(sp, form, bytes, movecount)]
    game_rowblk = {}   # game -> [block index per row, 0xFFFF = absent]
    for g in GAMES_TO_EMIT:
        asm = ASSEMBLERS[g]()
        pers = personals[g]
        evo_rev = read_evolutions(EVO_RESOURCE[g], pers)
        cache = {}

        def own_pool(sp, form, _asm=asm, _cache=cache):
            key = (sp, form)
            if key not in _cache:
                _cache[key] = frozenset(_asm.assemble(sp, form))
            return _cache[key]

        row_block = [0xFFFF] * len(rows)
        blocks = []
        folded_rows = folded_moves = 0
        for ri, (sp, form) in enumerate(rows):
            if not pers.present(sp, form):
                continue
            pool = set(own_pool(sp, form))
            own = len(pool)
            # Fold in every ancestor's pool. A Pokemon keeps the moves it knew before
            # evolving, so its legally-knowable set is the UNION over its evolution
            # chain -- e.g. a Raichu may know Double Kick, which only Pikachu learns.
            # Ancestors absent from this game are skipped: you cannot have evolved
            # from one here (Pichu is not in Let's Go), so its moves aren't reachable.
            for (psp, pform) in preevo_chain(evo_rev, sp, form):
                if pers.present(psp, pform):
                    pool |= own_pool(psp, pform)
            if len(pool) > own:
                folded_rows += 1
                folded_moves += len(pool) - own
            bits = bytearray(WIDTH)
            for mv in pool:
                if mv > MAX_MOVE_ID:
                    raise SystemExit(f"{g}: move id {mv} exceeds width (sp {sp} form {form})")
                bits[mv >> 3] |= 1 << (mv & 7)
            row_block[ri] = len(blocks)
            blocks.append((sp, form, bits, len(pool)))
        game_blocks[g] = blocks
        game_rowblk[g] = row_block
        print(f"  {g:5} pre-evo fold: {folded_rows:4} rows gained {folded_moves:5} moves"
              f"  (lineage edges: {len(evo_rev)})")

    # ---- Header ----
    with open(OUT_H, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(HDR_TMPL.format(MAX=MAX_SPECIES, MAXMOVE=MAX_MOVE_ID, BYTES=WIDTH))

    # ---- Source ----
    p = []
    p.append('/**\n'
             ' * LearnsetTable.cpp - Per-species+form learnable-move bitsets (all six games).\n'
             ' *\n'
             ' * Auto-generated by tools/gen_learnsets.py from PKHeX\'s binary learnset\n'
             ' * resources. DO NOT EDIT BY HAND -- rerun the generator instead.\n'
             ' *\n'
             ' * Layout: for each game G, LEARN_<G>_BITS is a flat pool of %d-byte bitsets\n'
             ' * (one per G-present species+form, in row order) and LEARN_<G>_ROW_BLOCK[row]\n'
             ' * gives a row\'s block index (0xFFFF = absent); byte offset = block * %d.\n'
             ' * LEARN_FORM_INDEX redirects alternate forms exactly like PersonalInfoTable\n'
             ' * (form N -> LEARN_FORM_INDEX[species] + N - 1).\n'
             ' *\n'
             ' * Each pool is the species\' own learnable set UNIONED with its whole\n'
             ' * pre-evolution chain (a Raichu keeps Pikachu\'s Double Kick), folded in at\n'
             ' * generation time from PKHeX\'s evos_*.pkl lineage.\n'
             ' */\n\n' % (WIDTH, WIDTH))
    p.append('#include "Pokemon/LearnsetTable.h"\n\n')
    p.append("namespace Pokemon {\n\n")
    p.append("    constexpr uint16_t LEARN_ABSENT = 0xFFFF;\n\n")
    p.append("    // Row -> National Dex form redirection (0 = species has no alternate forms).\n")
    emit_u16_array(p, "LEARN_FORM_INDEX", form_index, 12, "LEARN_MAX_SPECIES + 1")

    for g in GAMES_TO_EMIT:
        blocks = game_blocks[g]
        p.append("    // ==== %s (%d present species+forms) ====\n" % (g, len(blocks)))
        p.append("    // Row -> %s bitset block index (0xFFFF = not present in %s).\n" % (g, g))
        emit_u16_array(p, "LEARN_%s_ROW_BLOCK" % g, game_rowblk[g], 12, str(len(rows)))
        emit_hex_pool(p, "LEARN_%s_BITS" % g, blocks, WIDTH, names)

    # accessors
    p.append("    static inline size_t learnRowIndex(uint16_t species, uint8_t form) {\n")
    p.append("        if (species > LEARN_MAX_SPECIES)\n")
    p.append("            return 0;\n")
    p.append("        uint16_t fi = LEARN_FORM_INDEX[species];\n")
    p.append("        if (form == 0 || fi == 0)\n")
    p.append("            return species;\n")
    p.append("        return static_cast<size_t>(fi) + form - 1;\n")
    p.append("    }\n\n")

    p.append("    static inline const uint8_t* learnLookup(const uint16_t* rowBlock, "
             "const uint8_t* pool, size_t row) {\n")
    p.append("        if (row >= %d)\n" % len(rows))
    p.append("            return nullptr;\n")
    p.append("        uint16_t block = rowBlock[row];\n")
    p.append("        if (block == LEARN_ABSENT)\n")
    p.append("            return nullptr;\n")
    p.append("        return &pool[static_cast<size_t>(block) * LEARN_BITSET_BYTES];\n")
    p.append("    }\n\n")

    p.append("    const uint8_t* getLearnableBits(uint16_t species, uint8_t form, Enums::GameVersion group) {\n")
    p.append("        size_t row = learnRowIndex(species, form);\n")
    p.append("        switch (group) {\n")
    for g in GAMES_TO_EMIT:
        cases = "".join("            case Enums::GameVersion::%s:\n" % v for v in GAME_VERSION_IDS[g])
        p.append(cases)
        p.append("                return learnLookup(LEARN_%s_ROW_BLOCK, LEARN_%s_BITS, row);\n" % (g, g))
    p.append("            default:\n")
    p.append("                return nullptr;\n")
    p.append("        }\n")
    p.append("    }\n\n")

    p.append("    bool isLearnable(uint16_t species, uint8_t form, Enums::GameVersion group, uint16_t moveId) {\n")
    p.append("        if (moveId > LEARN_MAX_MOVE_ID)\n")
    p.append("            return false;\n")
    p.append("        const uint8_t* bits = getLearnableBits(species, form, group);\n")
    p.append("        if (bits == nullptr)\n")
    p.append("            return false;\n")
    p.append("        return (bits[moveId >> 3] >> (moveId & 7)) & 1;\n")
    p.append("    }\n")
    p.append("}\n")

    with open(OUT_CPP, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("".join(p))

    # ---- Report ----
    print("\nWrote", OUT_H)
    print("Wrote", OUT_CPP)
    idx_bytes = BASE_ROWS * 2  # LEARN_FORM_INDEX
    pool_total = 0
    print(f"\n  uniform bitset width : {WIDTH} bytes (move ids 0..{MAX_MOVE_ID})")
    print("  per-game:")
    for g in GAMES_TO_EMIT:
        n = len(game_blocks[g])
        pool = n * WIDTH
        rb = len(rows) * 2
        pool_total += pool
        idx_bytes += rb
        print(f"    {g:5} present={n:4}  bits={pool:7}B ({pool/1024:5.1f}KB)  rowblock={rb}B")
    total = pool_total + idx_bytes
    print(f"  bitset pools total   : {pool_total} B ({pool_total/1024:.1f} KB)")
    print(f"  indices (rowblock*6 + formIndex): {idx_bytes} B ({idx_bytes/1024:.1f} KB)")
    print(f"  TABLE TOTAL          : {total} B ({total/1024:.1f} KB, {total/1024/1024:.3f} MB)")


if __name__ == "__main__":
    main()
