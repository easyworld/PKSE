#!/usr/bin/env python3
"""Regenerate Simplified Chinese name tables from PKHeX resources."""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pkhex_source import pkhex_path  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


# FormNames.cpp uses descriptive/composed labels that are not always present as
# exact strings in PKHeX's flat form resource. These overrides cover those cases
# and resolve labels whose English text maps to more than one Chinese entry.
FORM_OVERRIDES = {
    "10% Power Construct": "10％形态（群聚变形）",
    "50% Power Construct": "50％形态（群聚变形）",
    "Alola Cap": "阿罗拉帽子",
    "Alolan": "阿罗拉的样子",
    "Amped": "高调的样子",
    "Apex Build": "完全形态",
    "Aqua Breed": "水澜",
    "Aquatic Mode": "浮水模式",
    "Battle Bond": "牵绊变身",
    "Blaze Breed": "火炽",
    "Blue Flower": "蓝花",
    "Blue Plumage": "蓝羽毛",
    "Blue-Striped": "蓝条纹的样子",
    "Burn Drive": "火焰卡带",
    "Chill Drive": "冰冻卡带",
    "Combat Breed": "斗战",
    "Core Blue": "浅蓝色核心",
    "Core Green": "绿色核心",
    "Core Indigo": "蓝色核心",
    "Core Orange": "橙色核心",
    "Core Red": "红色核心",
    "Core Violet": "紫色核心",
    "Core Yellow": "黄色核心",
    "Cornerstone Mask": "础石面具",
    "Cornerstone Mask Terastal": "础石面具太晶化",
    "Crowned Shield": "盾之王",
    "Crowned Sword": "剑之王",
    "Curly Mega": "超级（上弓姿势）",
    "Dandy Trim": "绅士造型",
    "Dawn Wings": "拂晓之翼",
    "Debutante Trim": "淑女造型",
    "Diamond Trim": "菱形造型",
    "Douse Drive": "水流卡带",
    "Drive Mode": "行驶模式",
    "Droopy Mega": "超级（下垂姿势）",
    "Dusk": "黄昏的样子",
    "Dusk Mane": "黄昏之鬃",
    "East Sea": "东海",
    "Female Mega": "雌性（超级）",
    "Galarian": "伽勒尔的样子",
    "Galarian Zen": "伽勒尔达摩模式",
    "Glide Mode": "滑翔模式",
    "Gliding Build": "乘风形态",
    "Green Plumage": "绿羽毛",
    "Heart Trim": "心形造型",
    "Hearthflame Mask": "火灶面具",
    "Hearthflame Mask Terastal": "火灶面具太晶化",
    "Hero": "全能形态",
    "Hero of Many Battles": "百战勇者",
    "Hisuian": "洗翠的样子",
    "Hoenn Cap": "丰缘帽子",
    "Ice Rider": "骑白马的样子",
    "Kabuki Trim": "歌舞伎造型",
    "Kalos Cap": "卡洛斯帽子",
    "La Reine Trim": "女王造型",
    "Large": "大尺寸",
    "Limited Build": "制限形态",
    "Low-Power Mode": "受限模式",
    "Male Mega": "雄性（超级）",
    "Matron Trim": "贵妇造型",
    "Mega": "超级",
    "Mega Original Color": "超级（５００年前的颜色）",
    "Meteor Blue": "流星的样子（浅蓝色）",
    "Meteor Green": "流星的样子（绿色）",
    "Meteor Indigo": "流星的样子（蓝色）",
    "Meteor Orange": "流星的样子（橙色）",
    "Meteor Red": "流星的样子（红色）",
    "Meteor Violet": "流星的样子（紫色）",
    "Meteor Yellow": "流星的样子（黄色）",
    "Normal": "普通形态",
    "Orange Flower": "橙花",
    "Original Cap": "初始帽子",
    "Original Color": "５００年前的颜色",
    "Pa'u": "呼拉呼拉风格",
    "Paldean": "帕底亚的样子",
    "Partner Cap": "就决定是你了之帽子",
    "Pharaoh Trim": "国王造型",
    "Plant Cloak": "草木蓑衣",
    "Poke Ball": "球球花纹",
    "Primal": "原始回归",
    "Red Flower": "红花",
    "Red-Striped": "红条纹的样子",
    "Sandy Cloak": "砂土蓑衣",
    "Shadow Rider": "骑黑马的样子",
    "Shock Drive": "闪电卡带",
    "Sinnoh Cap": "神奥帽子",
    "Small": "小尺寸",
    "Sprinting Build": "疾驰形态",
    "Star Trim": "星形造型",
    "Starter": "搭档",
    "Stretchy Mega": "超级（平挺姿势）",
    "Swimming Build": "破浪形态",
    "Teal Mask": "碧草面具",
    "Teal Mask Terastal": "碧草面具太晶化",
    "Totem": "霸主",
    "Totem Busted": "霸主（现形的样子）",
    "Totem Disguised": "霸主（化形的样子）",
    "Trash Cloak": "垃圾蓑衣",
    "Ultimate Mode": "完整模式",
    "Unova Cap": "合众帽子",
    "Wellspring Mask": "水井面具",
    "Wellspring Mask Terastal": "水井面具太晶化",
    "West Sea": "西海",
    "White": "焰白酋雷姆",
    "White Flower": "白花",
    "White Plumage": "白羽毛",
    "White-Striped": "白条纹的样子",
    "World Cap": "世界帽子",
    "Yellow Flower": "黄花",
    "Yellow Plumage": "黄羽毛",
}


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


def localize_forms():
    en_rel = "Resources/text/other/en/text_Forms_en.txt"
    zh_rel = "Resources/text/other/zh-Hans/text_Forms_zh-Hans.txt"
    english = load_lines(en_rel)
    chinese = load_lines(zh_rel)
    if len(english) != len(chinese):
        raise SystemExit("PKHeX English and Chinese form resources are not aligned")

    candidates = {}
    for source, localized in zip(english, chinese):
        if source and localized:
            candidates.setdefault(source, set()).add(localized)
    translations = {
        source: next(iter(values))
        for source, values in candidates.items()
        if len(values) == 1
    }
    translations.update(FORM_OVERRIDES)

    path = os.path.join(ROOT, "src", "Names", "FormNames.cpp")
    with open(path, encoding="utf-8") as fh:
        text = fh.read()
    pattern = re.compile(r'return "([^"\\]*)";')

    def repl(match):
        source = match.group(1)
        if not source:
            return match.group(0)
        localized = translations.get(source)
        if localized is None:
            return match.group(0)
        return f'return "{esc(localized)}";'

    text = pattern.sub(repl, text)
    remaining = sorted({
        value for value in pattern.findall(text)
        if value and re.search(r"[A-Za-z]", value)
    })
    if remaining:
        raise SystemExit("Untranslated form names: " + ", ".join(remaining))

    with open(path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)


if __name__ == "__main__":
    import gen_speciesnames

    gen_speciesnames.main()
    write_abilities()
    localize_items()
    localize_ribbons()
    localize_forms()
    print("Localized species, form, ability, item, and ribbon name tables (zh-Hans)")
