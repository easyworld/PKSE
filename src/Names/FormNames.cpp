/**
 * FormNames.cpp - Pokemon Form Name Lookup Implementation
 *
 * Maps (species ID, form ID) pairs to form names for display purposes.
 *
 * Every form a save can hold a value for is named here, INCLUDING battle-only ones (Zen Mode,
 * Schooling, Aegislash's Blade, Minior's cores, Mega). The details view has to render what is
 * actually in the buffer, and a Zen-form Darmanitan showing as "Form 1" is exactly how a
 * mislabelled form goes unnoticed. Whether a form can be *selected* is a separate question,
 * answered by Pokemon::isBattleOnlyForm(), which gates the Form picker.
 *
 * Ordering follows PKHeX's FormConverter, NOT dex or appearance order, and it has real
 * exceptions: Slowbro's Mega comes before its Galarian form, and Darmanitan's Galarian form is
 * 2 because form 1 is Zen. Gigantamax is a flag in the save, not a form index, so it is absent.
 */

#include <cstdint>
#include <string>

#include "Names/FormNames.h"
#include "Names/TypeNames.h"   // Arceus / Silvally name one form per type
#include "Utils/Logger.h"

namespace Names {
    const char* getFormName(uint16_t speciesId, uint8_t formId) {
        // Regional Variants - Form 1 = Regional Form
        switch (speciesId) {
            // Alolan Forms (Gen 7)
            case 19:  // Rattata
            case 27:  // Sandshrew
            case 28:  // Sandslash
            case 37:  // Vulpix
            case 38:  // Ninetales
            case 50:  // Diglett
            case 51:  // Dugtrio
            case 74:  // Geodude
            case 75:  // Graveler
            case 76:  // Golem
            case 88:  // Grimer
            case 89:  // Muk
            case 103: // Exeggutor
                if (formId == 1) return "阿罗拉的样子";
                break;

            // Raticate and Marowak have an Alolan form AND a Gen 7 Totem form at 2, so they
            // can't ride the plain Alolan group.
            case 20:  // Raticate
            case 105: // Marowak
                if (formId == 1) return "阿罗拉的样子";
                else if (formId == 2) return "霸主";
                break;

            // Meowth/Persian - Has both Alolan and Galarian
            // Meowth has both Alolan and Galarian; Persian only Alolan (Galarian Meowth
            // evolves into Perrserker, so there is no Galarian Persian).
            case 52:  // Meowth
                if (formId == 1) return "阿罗拉的样子";
                else if (formId == 2) return "伽勒尔的样子";
                break;

            case 53:  // Persian
                if (formId == 1) return "阿罗拉的样子";
                break;

            // Galarian Forms (Gen 8)
            case 77:  // Ponyta
            case 78:  // Rapidash
            case 79:  // Slowpoke
            case 83:  // Farfetch'd
            case 110: // Weezing
            case 122: // Mr. Mime
            case 144: // Articuno
            case 145: // Zapdos
            case 146: // Moltres
            case 199: // Slowking
            case 222: // Corsola
            case 263: // Zigzagoon
            case 264: // Linoone
            case 554: // Darumaka
            case 562: // Yamask
            case 618: // Stunfisk
                if (formId == 1) return "伽勒尔的样子";
                break;

            // Hisuian Forms (Gen 8 - Legends: Arceus)
            case 58:  // Growlithe
            case 100: // Voltorb
            case 157: // Typhlosion
            case 211: // Qwilfish
            case 215: // Sneasel
            case 503: // Samurott
            case 570: // Zorua
            case 571: // Zoroark
            case 628: // Braviary
            case 705: // Sliggoo
            case 706: // Goodra
            case 724: // Decidueye
                if (formId == 1) return "洗翠的样子";
                break;

            // Legends: Arceus noble/alpha variants sit at form 2, after the Hisuian form.
            case 59:  // Arcanine
            case 101: // Electrode
            case 713: // Avalugg
                if (formId == 1) return "洗翠的样子";
                else if (formId == 2) return "王的样子";
                break;

            case 549: // Lilligant
                if (formId == 1) return "洗翠的样子";
                else if (formId == 2) return "女王的样子";
                break;

            // Wooper - Paldean form
            case 194: // Wooper
                if (formId == 1) return "帕底亚的样子";
                break;

            // Tauros - Paldean breeds
            case 128: // Tauros
                if (formId == 1) return "斗战";
                else if (formId == 2) return "火炽";
                else if (formId == 3) return "水澜";
                break;

            // Pokemon with multiple permanent forms
            // ========================================

            // Raichu / Slowbro / Meowstic sit apart from their regional + gender groups
            // because Legends: Z-A gives each of them Mega forms after those.
            case 26:  // Raichu
                if (formId == 1) return "阿罗拉的样子";
                else if (formId == 2) return "超级 Ｘ";
                else if (formId == 3) return "超级 Ｙ";
                break;

            // Slowbro kept Mega at form 1 from Gen 6, so Galar was appended at 2 -- the one
            // species where a Mega precedes a regional form. (Ability check: form 1 is Shell
            // Armor = Mega, form 2 is Quick Draw = Galarian.)
            case 80:  // Slowbro
                if (formId == 1) return "超级";
                else if (formId == 2) return "伽勒尔的样子";
                break;

            case 678: // Meowstic
                if (formId == 0) return "雄性的样子";
                else if (formId == 1) return "雌性的样子";
                else if (formId == 2) return "雄性（超级）";
                else if (formId == 3) return "雌性（超级）";
                break;

            // Gender difference
            case 876: // Indeedee
            case 902: // Basculegion
            case 916: // Oinkologne
                if (formId == 0) return "雄性的样子";
                else if (formId == 1) return "雌性的样子";
                break;

            // Deoxys - Different stats per form
            case 386: // Deoxys
                if (formId == 0) return "普通形态";
                else if (formId == 1) return "攻击形态";
                else if (formId == 2) return "防御形态";
                else if (formId == 3) return "速度形态";
                break;

            // Burmy/Wormadam/Mothim - Cloak forms (Mothim's is inherited and cosmetic)
            case 412: // Burmy
            case 413: // Wormadam
            case 414: // Mothim
                if (formId == 0) return "草木蓑衣";
                else if (formId == 1) return "砂土蓑衣";
                else if (formId == 2) return "垃圾蓑衣";
                break;

            // Rotom - Appliance forms
            case 479: // Rotom
                if (formId == 1) return "加热";
                else if (formId == 2) return "清洗";
                else if (formId == 3) return "结冰";
                else if (formId == 4) return "旋转";
                else if (formId == 5) return "切割";
                break;

            // Giratina - Altered/Origin
            case 487: // Giratina
                if (formId == 0) return "别种形态";
                else if (formId == 1) return "起源形态";
                break;

            // Shaymin - Land/Sky
            case 492: // Shaymin
                if (formId == 0) return "陆上形态";
                else if (formId == 1) return "天空形态";
                break;

            // Basculin - Forms
            case 550: // Basculin
                if (formId == 0) return "红条纹的样子";
                else if (formId == 1) return "蓝条纹的样子";
                else if (formId == 2) return "白条纹的样子";
                break;

            // Tornadus/Thundurus/Landorus - Incarnate/Therian forms
            case 641: // Tornadus
            case 642: // Thundurus
            case 645: // Landorus
                if (formId == 0) return "化身形态";
                else if (formId == 1) return "灵兽形态";
                break;

            // Kyurem - Fusions
            case 646: // Kyurem
                if (formId == 1) return "焰白酋雷姆";
                else if (formId == 2) return "暗黑酋雷姆";
                break;

            // Keldeo - Resolute
            case 647: // Keldeo
                if (formId == 0) return "平常的样子";
                else if (formId == 1) return "觉悟的样子";
                break;

            // Meloetta - Pirouette
            case 648: // Meloetta
                if (formId == 0) return "歌声形态";
                else if (formId == 1) return "舞步形态";
                break;

            // Pumpkaboo/Gourgeist - Size variants
            case 710: // Pumpkaboo
            case 711: // Gourgeist
                if (formId == 0) return "普通尺寸";
                else if (formId == 1) return "小尺寸";
                else if (formId == 2) return "大尺寸";
                else if (formId == 3) return "特大尺寸";
                break;

            // Zygarde - Forms
            case 718: // Zygarde
                if (formId == 0) return "50％形态";
                else if (formId == 1) return "10％形态";
                else if (formId == 2) return "10％形态（群聚变形）";
                else if (formId == 3) return "50％形态（群聚变形）";
                else if (formId == 4) return "完全体形态";
                else if (formId == 5) return "超级";
                break;

            // Hoopa - Forms
            case 720: // Hoopa
                if (formId == 0) return "惩戒胡帕";
                else if (formId == 1) return "解放胡帕";
                break;

            // Oricorio - Styles
            case 741: // Oricorio
                if (formId == 0) return "热辣热辣风格";
                else if (formId == 1) return "啪滋啪滋风格";
                else if (formId == 2) return "呼拉呼拉风格";
                else if (formId == 3) return "轻盈轻盈风格";
                break;

            // Lycanroc - Forms
            case 745: // Lycanroc
                if (formId == 0) return "白昼的样子";
                else if (formId == 1) return "黑夜的样子";
                else if (formId == 2) return "黄昏的样子";
                break;

            // Necrozma - Fusions
            case 800: // Necrozma
                if (formId == 1) return "黄昏之鬃";
                else if (formId == 2) return "拂晓之翼";
                else if (formId == 3) return "究极奈克洛兹玛";
                break;

            // Toxtricity - Forms
            case 849: // Toxtricity
                if (formId == 0) return "高调的样子";
                else if (formId == 1) return "低调的样子";
                break;

            // Zacian/Zamazenta - Crowned forms
            case 888: // Zacian
                if (formId == 0) return "百战勇者";
                else if (formId == 1) return "剑之王";
                break;
            case 889: // Zamazenta
                if (formId == 0) return "百战勇者";
                else if (formId == 1) return "盾之王";
                break;

            // Urshifu - Styles
            case 892: // Urshifu
                if (formId == 0) return "一击流";
                else if (formId == 1) return "连击流";
                break;

            // Calyrex - Riders
            case 898: // Calyrex
                if (formId == 1) return "骑白马的样子";
                else if (formId == 2) return "骑黑马的样子";
                break;

            // Ursaluna - Bloodmoon
            case 901: // Ursaluna
                if (formId == 1) return "赫月";
                break;

            // Enamorus - Therian
            case 905: // Enamorus
                if (formId == 0) return "化身形态";
                else if (formId == 1) return "灵兽形态";
                break;

            // Maushold - Family size
            // This form variant is unique in that the rare variant is form 0, while the "base" form is form 1
            case 925: // Maushold
                if (formId == 0) return "三只家庭";
                else if (formId == 1) return "四只家庭";
                break;

            // Squawkabilly - Plumages
            case 931: // Squawkabilly
                if (formId == 0) return "绿羽毛";
                else if (formId == 1) return "蓝羽毛";
                else if (formId == 2) return "黄羽毛";
                else if (formId == 3) return "白羽毛";
                break;

            // Tatsugiri - Forms
            case 978: // Tatsugiri
                if (formId == 0) return "上弓姿势";
                else if (formId == 1) return "下垂姿势";
                else if (formId == 2) return "平挺姿势";
                else if (formId == 3) return "超级（上弓姿势）";
                else if (formId == 4) return "超级（下垂姿势）";
                else if (formId == 5) return "超级（平挺姿势）";
                break;

            // Dudunsparce - Segment count
            case 982: // Dudunsparce
                if (formId == 0) return "二节形态";
                else if (formId == 1) return "三节形态";
                break;

            // Gimmighoul - Forms
            case 999: // Gimmighoul
                if (formId == 0) return "宝箱形态";
                else if (formId == 1) return "徒步形态";
                break;

            // Poltchageist / Sinistcha - Forms (different names, so not a shared case)
            case 1012: // Poltchageist
                if (formId == 0) return "冒牌货的样子";
                else if (formId == 1) return "高档货的样子";
                break;

            case 1013: // Sinistcha
                if (formId == 0) return "凡作的样子";
                else if (formId == 1) return "杰作的样子";
                break;

            // Palafin - Zero/Hero
            case 964: // Palafin
                if (formId == 0) return "平凡形态";
                else if (formId == 1) return "全能形态";
                break;

            // Ogerpon - Masks (permanent when holding a mask); 4-7 are the Terastallized
            // Embody Aspect forms, which only exist mid-battle.
            case 1017: // Ogerpon
                if (formId == 0) return "碧草面具";
                else if (formId == 1) return "水井面具";
                else if (formId == 2) return "火灶面具";
                else if (formId == 3) return "础石面具";
                else if (formId == 4) return "碧草面具太晶化";
                else if (formId == 5) return "水井面具太晶化";
                else if (formId == 6) return "火灶面具太晶化";
                else if (formId == 7) return "础石面具太晶化";
                break;

            // Terapagos - Forms
            case 1024: // Terapagos
                if (formId == 1) return "太晶形态";
                else if (formId == 2) return "星晶形态";
                break;

            // Pikachu - event caps. PKHeX's Gen 8+ list; "Starter" is the Let's Go partner,
            // which is a different form from "Partner Cap".
            case 25:  // Pikachu
                if (formId == 1) return "初始帽子";
                else if (formId == 2) return "丰缘帽子";
                else if (formId == 3) return "神奥帽子";
                else if (formId == 4) return "合众帽子";
                else if (formId == 5) return "卡洛斯帽子";
                else if (formId == 6) return "阿罗拉帽子";
                else if (formId == 7) return "就决定是你了之帽子";
                else if (formId == 8) return "搭档";
                else if (formId == 9) return "世界帽子";
                break;

            // Eevee - Let's Go partner
            case 133: // Eevee
                if (formId == 1) return "搭档";
                break;

            // Unown - one form per glyph
            case 201: { // Unown
                static const char* const LETTERS[28] = {
                    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
                    "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
                    "!", "?",
                };
                if (formId < 28) return LETTERS[formId];
                break;
            }

            // Arceus / Silvally - one form per type, in the internal type order, so the shared
            // type table names them. Arceus form 18 is the Legends: Arceus Legend Plate.
            case 493: // Arceus
                if (formId == 18) return "传说";
                if (formId < 18) return getTypeName(formId);
                break;

            case 773: // Silvally
                if (formId < 18) return getTypeName(formId);
                break;

            // Genesect - Drives
            case 649: // Genesect
                if (formId == 1) return "水流卡带";
                else if (formId == 2) return "闪电卡带";
                else if (formId == 3) return "火焰卡带";
                else if (formId == 4) return "冰冻卡带";
                break;

            // Gen 7 Totem forms (Raticate, Marowak and Mimikyu handle theirs above, since
            // they have a regional or busted form in the way).
            case 735: // Gumshoos
            case 738: // Vikavolt
            case 743: // Ribombee
            case 752: // Araquanid
            case 754: // Lurantis
            case 758: // Salazzle
            case 777: // Togedemaru
            case 784: // Kommo-o
                if (formId == 1) return "霸主";
                break;

            // Rockruff - the Own Tempo (Dusk-evolving) variant
            case 744: // Rockruff
                if (formId == 1) return "黄昏的样子";
                break;

            // Zarude - Dada
            case 893: // Zarude
                if (formId == 1) return "阿爸";
                break;

            // Kleavor - noble
            case 900: // Kleavor
                if (formId == 1) return "王的样子";
                break;

            // ---- Families that were showing "Base" / "Form N" ------------------------------
            // PKHeX names form 0 of each of these. Leaving form 0 as "Base" is the same class of
            // gap that let Maushold's swapped labels sit unnoticed: with no name to contradict,
            // a wrong form index looks exactly like a right one.

            // Darmanitan - Zen is form 1, so Galarian lands at 2. NOT the usual "form 1 =
            // regional" shape, which is why it sat in the Galarian group mislabelled.
            case 555: // Darmanitan
                if (formId == 0) return "普通模式";
                else if (formId == 1) return "达摩模式";
                else if (formId == 2) return "伽勒尔的样子";
                else if (formId == 3) return "伽勒尔达摩模式";
                break;

            // Castform - Weather
            case 351: // Castform
                if (formId == 1) return "太阳的样子";
                else if (formId == 2) return "雨水的样子";
                else if (formId == 3) return "雪云的样子";
                break;

            // Cramorant - Gulp Missile
            case 845: // Cramorant
                if (formId == 1) return "一口吞的样子";
                else if (formId == 2) return "大口吞的样子";
                break;

            // Eternatus - Eternamax
            case 890: // Eternatus
                if (formId == 1) return "无极巨化";
                break;

            // Cherrim - Weather
            case 421: // Cherrim
                if (formId == 0) return "阴天形态";
                else if (formId == 1) return "晴天形态";
                break;

            // Shellos/Gastrodon - Sea
            case 422: // Shellos
            case 423: // Gastrodon
                if (formId == 0) return "西海";
                else if (formId == 1) return "东海";
                break;

            // Deerling/Sawsbuck - Seasons
            case 585: // Deerling
            case 586: // Sawsbuck
                if (formId == 0) return "春天的样子";
                else if (formId == 1) return "夏天的样子";
                else if (formId == 2) return "秋天的样子";
                else if (formId == 3) return "冬天的样子";
                break;

            // Scatterbug/Spewpa/Vivillon - Patterns (inherited, so all three carry the same 20)
            case 664: // Scatterbug
            case 665: // Spewpa
            case 666: // Vivillon
                if (formId == 0) return "冰雪花纹";
                else if (formId == 1) return "雪国花纹";
                else if (formId == 2) return "雪原花纹";
                else if (formId == 3) return "大陆花纹";
                else if (formId == 4) return "庭园花纹";
                else if (formId == 5) return "高雅花纹";
                else if (formId == 6) return "花园花纹";
                else if (formId == 7) return "摩登花纹";
                else if (formId == 8) return "大海花纹";
                else if (formId == 9) return "群岛花纹";
                else if (formId == 10) return "荒野花纹";
                else if (formId == 11) return "沙尘花纹";
                else if (formId == 12) return "大河花纹";
                else if (formId == 13) return "骤雨花纹";
                else if (formId == 14) return "热带草原花纹";
                else if (formId == 15) return "太阳花纹";
                else if (formId == 16) return "大洋花纹";
                else if (formId == 17) return "热带雨林花纹";
                else if (formId == 18) return "幻彩花纹";
                else if (formId == 19) return "球球花纹";
                break;

            // Flabebe/Florges - Flower colours (Floette shares these, plus Eternal + Mega)
            case 669: // Flabebe
            case 671: // Florges
                if (formId == 0) return "红花";
                else if (formId == 1) return "黄花";
                else if (formId == 2) return "橙花";
                else if (formId == 3) return "蓝花";
                else if (formId == 4) return "白花";
                break;

            // Furfrou - Trims
            case 676: // Furfrou
                if (formId == 0) return "野生的样子";
                else if (formId == 1) return "心形造型";
                else if (formId == 2) return "星形造型";
                else if (formId == 3) return "菱形造型";
                else if (formId == 4) return "淑女造型";
                else if (formId == 5) return "贵妇造型";
                else if (formId == 6) return "绅士造型";
                else if (formId == 7) return "女王造型";
                else if (formId == 8) return "歌舞伎造型";
                else if (formId == 9) return "国王造型";
                break;

            // Aegislash - Stance
            case 681: // Aegislash
                if (formId == 0) return "盾牌形态";
                else if (formId == 1) return "刀剑形态";
                break;

            // Xerneas - Mode
            case 716: // Xerneas
                if (formId == 0) return "放松模式";
                else if (formId == 1) return "活跃模式";
                break;

            // Wishiwashi - Schooling
            case 746: // Wishiwashi
                if (formId == 0) return "单独的样子";
                else if (formId == 1) return "鱼群的样子";
                break;

            // Minior - Meteor (shields up) 0-6, then Core (shields down) 7-13
            case 774: // Minior
                if (formId == 0) return "流星的样子（红色）";
                else if (formId == 1) return "流星的样子（橙色）";
                else if (formId == 2) return "流星的样子（黄色）";
                else if (formId == 3) return "流星的样子（绿色）";
                else if (formId == 4) return "流星的样子（浅蓝色）";
                else if (formId == 5) return "流星的样子（蓝色）";
                else if (formId == 6) return "流星的样子（紫色）";
                else if (formId == 7) return "红色核心";
                else if (formId == 8) return "橙色核心";
                else if (formId == 9) return "黄色核心";
                else if (formId == 10) return "绿色核心";
                else if (formId == 11) return "浅蓝色核心";
                else if (formId == 12) return "蓝色核心";
                else if (formId == 13) return "紫色核心";
                break;

            // Mimikyu - Disguise; 2/3 are the Gen 7 Totem entries
            case 778: // Mimikyu
                if (formId == 0) return "化形的样子";
                else if (formId == 1) return "现形的样子";
                else if (formId == 2) return "霸主（化形的样子）";
                else if (formId == 3) return "霸主（现形的样子）";
                break;

            // Sinistea/Polteageist - Authenticity
            case 854: // Sinistea
            case 855: // Polteageist
                if (formId == 0) return "赝品";
                else if (formId == 1) return "真品";
                break;

            // Alcremie - Cream. The sweet is a separate form-argument byte, not a form index.
            case 869: // Alcremie
                if (formId == 0) return "奶香香草";
                else if (formId == 1) return "奶香红钻";
                else if (formId == 2) return "奶香抹茶";
                else if (formId == 3) return "奶香薄荷";
                else if (formId == 4) return "奶香柠檬";
                else if (formId == 5) return "奶香雪盐";
                else if (formId == 6) return "红钻综合";
                else if (formId == 7) return "焦糖综合";
                else if (formId == 8) return "三色综合";
                break;

            // Eiscue - Ice Face
            case 875: // Eiscue
                if (formId == 0) return "结冻头";
                else if (formId == 1) return "解冻头";
                break;

            // Morpeko - Hunger Switch
            case 877: // Morpeko
                if (formId == 0) return "满腹花纹";
                else if (formId == 1) return "空腹花纹";
                break;

            // Koraidon/Miraidon - the ride builds/modes exist only while riding
            case 1007: // Koraidon
                if (formId == 0) return "完全形态";
                else if (formId == 1) return "制限形态";
                else if (formId == 2) return "疾驰形态";
                else if (formId == 3) return "破浪形态";
                else if (formId == 4) return "乘风形态";
                break;

            case 1008: // Miraidon
                if (formId == 0) return "完整模式";
                else if (formId == 1) return "受限模式";
                else if (formId == 2) return "行驶模式";
                else if (formId == 3) return "浮水模式";
                else if (formId == 4) return "滑翔模式";
                break;

            // Mega Evolution -- storable again in Legends: Z-A. Single-mega species all
            // put Mega at form 1 (PKHeX FormConverter).
            case 3: case 9: case 15: case 18: case 36: case 65: case 71: case 94:
            case 115: case 121: case 127: case 130: case 142: case 149: case 154: case 160:
            case 181: case 208: case 212: case 214: case 227: case 229: case 248: case 254:
            case 257: case 260: case 282: case 302: case 303: case 306: case 308: case 310:
            case 319: case 323: case 334: case 354: case 358: case 362: case 373: case 376:
            case 380: case 381: case 384: case 398: case 428: case 460: case 475: case 478:
            case 485: case 491: case 500: case 530: case 531: case 545: case 560: case 604:
            case 609: case 623: case 652: case 655: case 668: case 687: case 689: case 691:
            case 701: case 719: case 740: case 768: case 780: case 807: case 870: case 952:
            case 970: case 998:
                if (formId == 1) return "超级";
                break;
            case 6: // Charizard
                if (formId == 1) return "超级 Ｘ";
                else if (formId == 2) return "超级 Ｙ";
                break;

            case 150: // Mewtwo
                if (formId == 1) return "超级 Ｘ";
                else if (formId == 2) return "超级 Ｙ";
                break;

            case 359: // Absol
                if (formId == 1) return "超级";
                else if (formId == 2) return "超级 Ｚ";
                break;

            case 382: // Kyogre
                if (formId == 1) return "原始回归";
                break;

            case 383: // Groudon
                if (formId == 1) return "原始回归";
                break;

            case 445: // Garchomp
                if (formId == 1) return "超级";
                else if (formId == 2) return "超级 Ｚ";
                break;

            case 448: // Lucario
                if (formId == 1) return "超级";
                else if (formId == 2) return "超级 Ｚ";
                break;

            case 483: // Dialga
                if (formId == 1) return "起源形态";
                break;

            case 484: // Palkia
                if (formId == 1) return "起源形态";
                break;

            case 658: // Greninja
                if (formId == 1) return "牵绊变身";
                else if (formId == 2) return "小智版甲贺忍蛙";
                else if (formId == 3) return "超级";
                break;

            case 670: // Floette
                if (formId == 0) return "红花";
                else if (formId == 1) return "黄花";
                else if (formId == 2) return "橙花";
                else if (formId == 3) return "蓝花";
                else if (formId == 4) return "白花";
                else if (formId == 5) return "永恒";
                else if (formId == 6) return "超级";
                break;

            case 801: // Magearna
                if (formId == 1) return "５００年前的颜色";
                else if (formId == 2) return "超级";
                else if (formId == 3) return "超级（５００年前的颜色）";
                break;
        }

        // If no match found, return empty string
        return "";
    }

    std::string getDisplayName(uint16_t speciesId, uint8_t formId, const std::string& baseName) {
        const char* f = getFormName(speciesId, formId);
        if (f && f[0] != '\0') return std::string(f) + " " + baseName;
        return baseName;
    }
}
