#!/usr/bin/env python3
"""Regenerate Chinese ability names and localize item/ribbon tables from PKHeX."""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pkhex_source import pkhex_path  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def load_lines(relpath):
    with open(pkhex_path(relpath), encoding="utf-8") as fh:
        lines = fh.read().splitlines()
    return ["" if line and all(ch == "—" for ch in line) else line for line in lines]


def esc(value):
    return value.replace("\\", "\\\\").replace('"', '\\"')


def write_abilities():
    rel = "Resources/text/other/zh-Hans/text_Abilities_zh-Hans.txt"
    names = load_lines(rel)
    out = [
        "// AUTO-GENERATED from PKHeX's Simplified Chinese ability names.\n",
        f"// Source: PKHeX.Core/{rel}\n",
        "#include <cstddef>\n#include <cstdint>\n\nnamespace Names {\n",
        "    static const char* const ABILITY_NAMES[] = {\n",
    ]
    for idx, name in enumerate(names):
        out.append(f'        "{esc(name)}",  // {idx}\n')
    out.extend([
        "    };\n\n",
        "    constexpr size_t ABILITY_NAMES_COUNT = sizeof(ABILITY_NAMES) / sizeof(ABILITY_NAMES[0]);\n\n",
        "    const char* getAbilityName(uint16_t abilityId) {\n",
        '        return abilityId < ABILITY_NAMES_COUNT ? ABILITY_NAMES[abilityId] : "未知";\n',
        "    }\n}\n",
    ])
    path = os.path.join(ROOT, "src", "Names", "AbilityNames.cpp")
    with open(path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("".join(out))


def replace_indexed_array(text, array_name, names):
    start = text.index(f"static const char* {array_name}[] = {{")
    end = text.index("    };", start)
    block = text[start:end]
    pattern = re.compile(r'^(\s*)"(?:[^"\\]|\\.)*",[ \t]*//[ \t]*(\d+)[ \t]*$', re.MULTILINE)

    def repl(match):
        idx = int(match.group(2))
        value = names[idx] if idx < len(names) else ""
        return f'{match.group(1)}"{esc(value)}",  // {idx}'

    return text[:start] + pattern.sub(repl, block) + text[end:]


def localize_items():
    path = os.path.join(ROOT, "src", "Names", "ItemNames.cpp")
    with open(path, encoding="utf-8") as fh:
        text = fh.read()
    modern = load_lines("Resources/text/items/text_Items_zh-Hans.txt")
    gen3 = load_lines("Resources/text/items/gen3/text_ItemsG3_zh-Hans.txt")
    text = replace_indexed_array(text, "ITEM_NAMES", modern)
    text = replace_indexed_array(text, "ITEM3_NAMES", gen3)
    text = text.replace('return "???";', 'return "未知道具";')
    with open(path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)


def localize_ribbons():
    path = os.path.join(ROOT, "src", "Names", "RibbonNames.cpp")
    with open(path, encoding="utf-8") as fh:
        text = fh.read()
    translations = {}
    rel = "Resources/text/other/zh-Hans/text_Ribbons_zh-Hans.txt"
    with open(pkhex_path(rel), encoding="utf-8") as fh:
        for line in fh:
            key, value = line.rstrip("\n").split("\t", 1)
            translations[key] = value

    common_keys = [
        "RibbonChampionKalos", "RibbonChampionG3", "RibbonChampionSinnoh", "RibbonBestFriends",
        "RibbonTraining", "RibbonBattlerSkillful", "RibbonBattlerExpert", "RibbonEffort",
        "RibbonAlert", "RibbonShock", "RibbonDowncast", "RibbonCareless", "RibbonRelax",
        "RibbonSnooze", "RibbonSmile", "RibbonGorgeous", "RibbonRoyal", "RibbonGorgeousRoyal",
        "RibbonArtist", "RibbonFootprint", "RibbonRecord", "RibbonLegend", "RibbonCountry",
        "RibbonNational", "RibbonEarth", "RibbonWorld", "RibbonClassic", "RibbonPremier",
        "RibbonEvent", "RibbonBirthday", "RibbonSpecial", "RibbonSouvenir", "RibbonWishing",
        "RibbonChampionBattle", "RibbonChampionRegional", "RibbonChampionNational",
        "RibbonChampionWorld", "RibbonCountMemoryContest", "RibbonCountMemoryBattle",
        "RibbonChampionG6Hoenn", "RibbonContestStar", "RibbonMasterCoolness", "RibbonMasterBeauty",
        "RibbonMasterCuteness", "RibbonMasterCleverness", "RibbonMasterToughness",
    ]
    gen89_keys = common_keys + [
        "RibbonChampionAlola", "RibbonBattleRoyale", "RibbonBattleTreeGreat", "RibbonBattleTreeMaster",
        "RibbonChampionGalar", "RibbonTowerMaster", "RibbonMasterRank", "RibbonMarkLunchtime",
        "RibbonMarkSleepyTime", "RibbonMarkDusk", "RibbonMarkDawn", "RibbonMarkCloudy",
        "RibbonMarkRainy", "RibbonMarkStormy", "RibbonMarkSnowy", "RibbonMarkBlizzard",
        "RibbonMarkDry", "RibbonMarkSandstorm", "RibbonMarkMisty", "RibbonMarkDestiny",
        "RibbonMarkFishing", "RibbonMarkCurry", "RibbonMarkUncommon", "RibbonMarkRare",
        "RibbonMarkRowdy", "RibbonMarkAbsentMinded", "RibbonMarkJittery", "RibbonMarkExcited",
        "RibbonMarkCharismatic", "RibbonMarkCalmness", "RibbonMarkIntense", "RibbonMarkZonedOut",
        "RibbonMarkJoyful", "RibbonMarkAngry", "RibbonMarkSmiley", "RibbonMarkTeary",
        "RibbonMarkUpbeat", "RibbonMarkPeeved", "RibbonMarkIntellectual", "RibbonMarkFerocious",
        "RibbonMarkCrafty", "RibbonMarkScowling", "RibbonMarkKindly", "RibbonMarkFlustered",
        "RibbonMarkPumpedUp", "RibbonMarkZeroEnergy", "RibbonMarkPrideful", "RibbonMarkUnsure",
        "RibbonMarkHumble", "RibbonMarkThorny", "RibbonMarkVigor", "RibbonMarkSlump",
        "RibbonHisui", "RibbonTwinklingStar", "RibbonChampionPaldea", "RibbonMarkJumbo",
        "RibbonMarkMini", "RibbonMarkItemfinder", "RibbonMarkPartner", "RibbonMarkGourmand",
        "RibbonOnceInALifetime", "RibbonMarkAlpha", "RibbonMarkMightiest", "RibbonMarkTitan",
        "RibbonPartner",
    ]

    def replace_array(source, array_name, localized):
        start = source.index(f"const RibbonFlag {array_name}[] = {{")
        end = source.index("        };", start)
        block = source[start:end]
        index = 0

        def repl(match):
            nonlocal index
            if index >= len(localized):
                return match.group(0)
            value = localized[index]
            index += 1
            return f'{match.group(1)}"{esc(value)}"{match.group(3)}'

        pattern = re.compile(r'^(\s*\{0x[0-9A-F]+,\s*\d+,\s*)"([^"]*)"(\},?.*)$', re.MULTILINE)
        return source[:start] + pattern.sub(repl, block) + source[end:]

    text = replace_array(text, "GEN89_RIBBONS", [translations[key] for key in gen89_keys])
    text = replace_array(text, "GG_RIBBONS", [translations[key] for key in gen89_keys[:50]])
    with open(path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)


if __name__ == "__main__":
    write_abilities()
    localize_items()
    localize_ribbons()
    print("Localized ability, item, and ribbon name tables (zh-Hans)")
