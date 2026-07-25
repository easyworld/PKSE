/**
 * RibbonNames.cpp - Ribbon & Mark lookup (per generation)
 *
 * Each table below is a straight transcription of the per-gen ribbon/mark
 * bit-flag definitions in PKHeX. A flag is stored as bit `bit` of the byte at
 * `byteOffset` in the decrypted entity buffer, read exactly like PKHeX's
 * FlagUtil.GetFlag(Data, byteOffset, bit) == ((Data[byteOffset] >> bit) & 1).
 *
 * Sources:
 *   - PKHeX.Core/PKM/Shared/G8PKM.cs  (PK8/PB8: SWSH + BDSP)  bytes 0x34..0x45
 *   - PKHeX.Core/PKM/PA8.cs           (Legends: Arceus)       bytes 0x34..0x45
 *   - PKHeX.Core/PKM/PK9.cs           (SV + Legends: Z-A)     bytes 0x34..0x45
 *   - PKHeX.Core/PKM/PB7.cs           (Let's Go: GG)          bytes 0x30..0x36
 *
 * G8PKM, PA8 and PK9 share the identical 0x34..0x45 HOME ribbon block, so a
 * single Gen-8/9 table serves SWSH, BDSP, PLA, SV and ZA. Gen 7 Let's Go has
 * its own smaller, mark-less block.
 *
 * Display names are derived from the PKHeX bool property names with the leading
 * "Ribbon"/"Mark" stripped and reflowed into human labels (RibbonChampionKalos
 * -> "Kalos Champion", RibbonEffort -> "Effort", RibbonMarkRare -> "Rare Mark").
 * Marks keep a trailing " Mark" so they read distinctly from ribbons.
 */

#include "Names/RibbonNames.h"

#include <cstddef>

namespace Names {

    namespace {

        struct RibbonFlag {
            uint8_t byteOffset;
            uint8_t bit;
            const char* name;
        };

        // ------------------------------------------------------------------
        // Gen 8/9 HOME ribbon + mark block (bytes 0x34..0x45).
        // Identical across PKHeX G8PKM (PK8/PB8), PA8 and PK9.
        // 58 ribbons + 53 marks = 111 flags. Omits the 0x3C/0x3D byte-count
        // fields and the unused RIB45_7 / RIB46_x / RIB47_x placeholder bits.
        // ------------------------------------------------------------------
        const RibbonFlag GEN89_RIBBONS[] = {
            // 0x34 - RibbonChampionKalos .. RibbonEffort
            {0x34, 0, "卡洛斯冠军奖章"},
            {0x34, 1, "冠军奖章 (3代)"},  // RibbonChampionG3
            {0x34, 2, "神奥冠军奖章"},
            {0x34, 3, "好友奖章"},
            {0x34, 4, "修行奖章"},
            {0x34, 5, "高手对战奖章"},        // RibbonBattlerSkillful
            {0x34, 6, "大师对战奖章"},           // RibbonBattlerExpert
            {0x34, 7, "努力奖章"},

            // 0x35 - RibbonAlert .. RibbonGorgeous
            {0x35, 0, "振奋奖章"},
            {0x35, 1, "心跳奖章"},
            {0x35, 2, "失望奖章"},
            {0x35, 3, "大意奖章"},
            {0x35, 4, "畅快奖章"},
            {0x35, 5, "酣睡奖章"},
            {0x35, 6, "欢笑奖章"},
            {0x35, 7, "豪华奖章"},

            // 0x36 - RibbonRoyal .. RibbonNational
            {0x36, 0, "高贵奖章"},
            {0x36, 1, "豪华高贵奖章"},
            {0x36, 2, "肖像奖章"},
            {0x36, 3, "脚印奖章"},
            {0x36, 4, "纪录奖章"},
            {0x36, 5, "传说奖章"},
            {0x36, 6, "地区奖章"},
            {0x36, 7, "国家奖章"},

            // 0x37 - RibbonEarth .. RibbonSouvenir
            {0x37, 0, "地球奖章"},
            {0x37, 1, "世界奖章"},
            {0x37, 2, "经典奖章"},
            {0x37, 3, "纪念奖章"},
            {0x37, 4, "活动奖章"},
            {0x37, 5, "生日奖章"},
            {0x37, 6, "特殊奖章"},
            {0x37, 7, "回忆奖章"},

            // 0x38 - RibbonWishing .. RibbonChampionG6Hoenn
            {0x38, 0, "许愿奖章"},
            {0x38, 1, "对战冠军奖章"},          // RibbonChampionBattle
            {0x38, 2, "地区冠军奖章"},        // RibbonChampionRegional
            {0x38, 3, "国家冠军奖章"},        // RibbonChampionNational
            {0x38, 4, "世界冠军奖章"},           // RibbonChampionWorld
            {0x38, 5, "华丽大赛回忆奖章"},           // HasContestMemoryRibbon
            {0x38, 6, "对战回忆奖章"},            // HasBattleMemoryRibbon
            {0x38, 7, "丰缘冠军奖章 (ORAS)"},   // RibbonChampionG6Hoenn

            // 0x39 - RibbonContestStar .. RibbonBattleRoyale
            {0x39, 0, "华丽大赛之星奖章"},
            {0x39, 1, "帅气大师奖章 (6代)"},          // RibbonMasterCoolness
            {0x39, 2, "美丽大师奖章 (6代)"},            // RibbonMasterBeauty
            {0x39, 3, "可爱大师奖章 (6代)"},          // RibbonMasterCuteness
            {0x39, 4, "聪明大师奖章 (6代)"},        // RibbonMasterCleverness
            {0x39, 5, "强壮大师奖章 (6代)"},         // RibbonMasterToughness
            {0x39, 6, "阿罗拉冠军奖章"},           // RibbonChampionAlola
            {0x39, 7, "皇家大师奖章"},

            // 0x3A - RibbonBattleTreeGreat .. RibbonMarkDusk
            {0x3A, 0, "高手对战树奖章"},
            {0x3A, 1, "大师对战树奖章"},
            {0x3A, 2, "伽勒尔冠军奖章"},           // RibbonChampionGalar
            {0x3A, 3, "对战塔大师奖章"},
            {0x3A, 4, "级别对战大师奖章"},
            {0x3A, 5, "正午之证"},           // RibbonMarkLunchtime
            {0x3A, 6, "午夜之证"},         // RibbonMarkSleepyTime
            {0x3A, 7, "黄昏之证"},                // RibbonMarkDusk

            // 0x3B - RibbonMarkDawn .. RibbonMarkSandstorm (all marks)
            {0x3B, 0, "拂晓之证"},
            {0x3B, 1, "阴云之证"},
            {0x3B, 2, "降雨之证"},
            {0x3B, 3, "落雷之证"},
            {0x3B, 4, "降雪之证"},
            {0x3B, 5, "暴雪之证"},
            {0x3B, 6, "干燥之证"},
            {0x3B, 7, "沙尘之证"},

            // (0x3C RibbonCountMemoryContest, 0x3D RibbonCountMemoryBattle:
            //  byte-value fields, not flags -- omitted.)

            // 0x40 - RibbonMarkMisty .. RibbonMarkAbsentMinded (all marks)
            {0x40, 0, "浓雾之证"},
            {0x40, 1, "命运之证"},
            {0x40, 2, "上钩之证"},
            {0x40, 3, "咖喱之证"},
            {0x40, 4, "偶遇之证"},
            {0x40, 5, "未知之证"},
            {0x40, 6, "淘气之证"},
            {0x40, 7, "无虑之证"},

            // 0x41 - RibbonMarkJittery .. RibbonMarkAngry (all marks)
            {0x41, 0, "紧张之证"},
            {0x41, 1, "期待之证"},
            {0x41, 2, "领袖之证"},
            {0x41, 3, "冷静之证"},
            {0x41, 4, "热情之证"},
            {0x41, 5, "疏忽之证"},
            {0x41, 6, "幸福之证"},
            {0x41, 7, "愤怒之证"},

            // 0x42 - RibbonMarkSmiley .. RibbonMarkScowling (all marks)
            {0x42, 0, "微笑之证"},
            {0x42, 1, "悲伤之证"},
            {0x42, 2, "爽快之证"},
            {0x42, 3, "激动之证"},
            {0x42, 4, "理性之证"},
            {0x42, 5, "本能之证"},
            {0x42, 6, "狡猾之证"},
            {0x42, 7, "凶悍之证"},

            // 0x43 - RibbonMarkKindly .. RibbonMarkThorny (all marks)
            {0x43, 0, "优雅之证"},
            {0x43, 1, "动摇之证"},
            {0x43, 2, "昂扬之证"},
            {0x43, 3, "倦怠之证"},
            {0x43, 4, "自信之证"},
            {0x43, 5, "自卑之证"},
            {0x43, 6, "木讷之证"},
            {0x43, 7, "不纯之证"},

            // 0x44 - RibbonMarkVigor .. RibbonMarkItemfinder (mixed)
            {0x44, 0, "活力之证"},               // RibbonMarkVigor
            {0x44, 1, "不振之证"},               // RibbonMarkSlump
            {0x44, 2, "洗翠奖章"},                    // RibbonHisui
            {0x44, 3, "闪亮之星奖章"},           // RibbonTwinklingStar
            {0x44, 4, "帕底亚冠军奖章"},          // RibbonChampionPaldea
            {0x44, 5, "大个子之证"},               // RibbonMarkJumbo
            {0x44, 6, "小不点之证"},                // RibbonMarkMini
            {0x44, 7, "捡拾之证"},          // RibbonMarkItemfinder

            // 0x45 - RibbonMarkPartner .. RibbonPartner (mixed; bit 7 unused)
            {0x45, 0, "搭档之证"},             // RibbonMarkPartner
            {0x45, 1, "美食之证"},            // RibbonMarkGourmand
            {0x45, 2, "千载难逢奖章"},       // RibbonOnceInALifetime
            {0x45, 3, "头目之证"},               // RibbonMarkAlpha
            {0x45, 4, "最强之证"},           // RibbonMarkMightiest
            {0x45, 5, "宝主之证"},               // RibbonMarkTitan
            {0x45, 6, "同伴奖章"},                  // RibbonPartner
        };

        // ------------------------------------------------------------------
        // Gen 7 Let's Go ribbon block (bytes 0x30..0x36). 50 ribbons, no marks.
        // PB7 stores these as RIB0=0x30 .. RIB6=0x36; only bits 0-1 of RIB6 are
        // used. Omits the 0x38/0x39 byte-count fields and RIB6_2..7 (unused).
        // ------------------------------------------------------------------
        const RibbonFlag GG_RIBBONS[] = {
            // 0x30 (RIB0)
            {0x30, 0, "卡洛斯冠军奖章"},
            {0x30, 1, "冠军奖章 (3代)"},
            {0x30, 2, "神奥冠军奖章"},
            {0x30, 3, "好友奖章"},
            {0x30, 4, "修行奖章"},
            {0x30, 5, "高手对战奖章"},
            {0x30, 6, "大师对战奖章"},
            {0x30, 7, "努力奖章"},

            // 0x31 (RIB1)
            {0x31, 0, "振奋奖章"},
            {0x31, 1, "心跳奖章"},
            {0x31, 2, "失望奖章"},
            {0x31, 3, "大意奖章"},
            {0x31, 4, "畅快奖章"},
            {0x31, 5, "酣睡奖章"},
            {0x31, 6, "欢笑奖章"},
            {0x31, 7, "豪华奖章"},

            // 0x32 (RIB2)
            {0x32, 0, "高贵奖章"},
            {0x32, 1, "豪华高贵奖章"},
            {0x32, 2, "肖像奖章"},
            {0x32, 3, "脚印奖章"},
            {0x32, 4, "纪录奖章"},
            {0x32, 5, "传说奖章"},
            {0x32, 6, "地区奖章"},
            {0x32, 7, "国家奖章"},

            // 0x33 (RIB3)
            {0x33, 0, "地球奖章"},
            {0x33, 1, "世界奖章"},
            {0x33, 2, "经典奖章"},
            {0x33, 3, "纪念奖章"},
            {0x33, 4, "活动奖章"},
            {0x33, 5, "生日奖章"},
            {0x33, 6, "特殊奖章"},
            {0x33, 7, "回忆奖章"},

            // 0x34 (RIB4)
            {0x34, 0, "许愿奖章"},
            {0x34, 1, "对战冠军奖章"},
            {0x34, 2, "地区冠军奖章"},
            {0x34, 3, "国家冠军奖章"},
            {0x34, 4, "世界冠军奖章"},
            {0x34, 5, "华丽大赛回忆奖章"},
            {0x34, 6, "对战回忆奖章"},
            {0x34, 7, "丰缘冠军奖章 (ORAS)"},

            // 0x35 (RIB5)
            {0x35, 0, "华丽大赛之星奖章"},
            {0x35, 1, "帅气大师奖章 (6代)"},
            {0x35, 2, "美丽大师奖章 (6代)"},
            {0x35, 3, "可爱大师奖章 (6代)"},
            {0x35, 4, "聪明大师奖章 (6代)"},
            {0x35, 5, "强壮大师奖章 (6代)"},
            {0x35, 6, "阿罗拉冠军奖章"},
            {0x35, 7, "皇家大师奖章"},

            // 0x36 (RIB6) - only bits 0-1 are defined
            {0x36, 0, "高手对战树奖章"},
            {0x36, 1, "大师对战树奖章"},
        };

        // ------------------------------------------------------------------
        // Gen 3 FireRed/LeafGreen (PK3): a single little-endian u32 at 0x4C.
        //
        // Unlike every later generation this is NOT purely bit flags. The five
        // contest ribbons take 3 bits each (bits 0-14) and hold a LEVEL, not a
        // flag. Bits 15-26 are ordinary flags, bits 27-30 are unused, and bit 31
        // is FatefulEncounter -- not a ribbon at all (PKHeX masks it out of
        // RibbonCount, and our PK3 remap reads it as the fateful flag).
        // Layout per PKHeX PK3.cs RIB0; names follow PKHeX's property naming,
        // matching the style of the tables above.
        // ------------------------------------------------------------------
        const RibbonFlag FRLG_RIBBONS[] = {
            {0x4D, 7, "冠军奖章"},            // bit 15
            {0x4E, 0, "成功奖章"},            // bit 16
            {0x4E, 1, "胜利奖章"},            // bit 17
            {0x4E, 2, "肖像奖章"},            // bit 18
            {0x4E, 3, "努力奖章"},            // bit 19
            {0x4E, 4, "对战冠军奖章"},        // bit 20
            {0x4E, 5, "地区冠军奖章"},        // bit 21
            {0x4E, 6, "国家冠军奖章"},        // bit 22
            {0x4E, 7, "地区奖章"},            // bit 23
            {0x4F, 0, "国家奖章"},            // bit 24
            {0x4F, 1, "地球奖章"},            // bit 25
            {0x4F, 2, "世界奖章"},            // bit 26
        };

        // The five 3-bit contest ribbons, in bit order (Cool at bits 0-2, etc).
        const char* const G3_CONTEST[] = { "帅气奖章", "美丽奖章", "可爱奖章", "聪明奖章", "强壮奖章" };
        // Level 0 = never won. 1-4 are the contest ranks; the field stores only the
        // HIGHEST rank reached, so that is the one ribbon we report.
        const char* const G3_CONTEST_RANK[] = { nullptr, "", "（超级）", "（高级）", "（大师）" };

        std::vector<std::string> collect(const uint8_t* data, const RibbonFlag* table, size_t count) {
            std::vector<std::string> out;
            for (size_t i = 0; i < count; ++i) {
                const RibbonFlag& f = table[i];
                if ((data[f.byteOffset] >> f.bit) & 1)
                    out.emplace_back(f.name);
            }
            return out;
        }

        std::vector<std::string> collectG3(const uint8_t* data) {
            const uint32_t rib = static_cast<uint32_t>(data[0x4C])
                               | (static_cast<uint32_t>(data[0x4D]) << 8)
                               | (static_cast<uint32_t>(data[0x4E]) << 16)
                               | (static_cast<uint32_t>(data[0x4F]) << 24);
            std::vector<std::string> out;
            for (int i = 0; i < 5; ++i) {
                const uint32_t lvl = (rib >> (i * 3)) & 7;
                if (lvl == 0 || lvl > 4)
                    continue;               // 0 = not won; 5-7 are undefined
                out.emplace_back(std::string(G3_CONTEST[i]) + G3_CONTEST_RANK[lvl]);
            }
            const std::vector<std::string> flags =
                collect(data, FRLG_RIBBONS, sizeof(FRLG_RIBBONS) / sizeof(FRLG_RIBBONS[0]));
            out.insert(out.end(), flags.begin(), flags.end());
            return out;
        }

    } // namespace

    std::vector<std::string> getMonRibbons(const uint8_t* entityData, Enums::GameVersion group) {
        if (entityData == nullptr)
            return {};

        using GV = Enums::GameVersion;
        switch (group) {
            // FireRed/LeafGreen (PK3) -- one u32 at 0x4C, mixed levels + flags.
            case GV::FR:
            case GV::LG:
            case GV::FRLG:
                return collectG3(entityData);

            // Let's Go Pikachu/Eevee (PB7)
            case GV::GP:
            case GV::GE:
            case GV::GG:
                return collect(entityData, GG_RIBBONS,
                               sizeof(GG_RIBBONS) / sizeof(GG_RIBBONS[0]));

            // Every Gen 8/9 HOME-format game shares the same 0x34..0x45 block.
            case GV::SW:   // Sword
            case GV::SH:   // Shield
            case GV::SWSH:
            case GV::BD:   // Brilliant Diamond
            case GV::SP:   // Shining Pearl
            case GV::BDSP:
            case GV::PLA:  // Legends: Arceus
            case GV::SL:   // Scarlet
            case GV::VL:   // Violet
            case GV::SV:
            case GV::ZA:   // Legends: Z-A
                return collect(entityData, GEN89_RIBBONS,
                               sizeof(GEN89_RIBBONS) / sizeof(GEN89_RIBBONS[0]));

            default:
                return {};
        }
    }
}
