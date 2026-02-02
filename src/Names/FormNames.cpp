/**
 * FormNames.cpp - Pokemon Form Name Lookup Implementation
 *
 * Maps (species ID, form ID) pairs to form names for display purposes.
 * Only includes permanent forms - excludes battle-only forms like:
 * - Mega Evolutions
 * - Gigantamax forms
 * - Dynamax forms
 * - Zen Mode (Darmanitan)
 * - Schooling (Wishiwashi)
 * - Battle forms (Aegislash, Minior shields, etc.)
 */

#include <cstdint>
#include <string>

#include "Names/FormNames.h"
#include "Utils/Logger.h"

namespace Names {
    const char* getFormName(uint16_t speciesId, uint8_t formId) {
        // Regional Variants - Form 1 = Regional Form
        switch (speciesId) {
            // Alolan Forms (Gen 7)
            case 19:  // Rattata
            case 20:  // Raticate
            case 26:  // Raichu
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
            case 105: // Marowak
                if (formId == 1) return "阿罗拉的样子";
                break;

            // Meowth/Persian - Has both Alolan and Galarian
            case 52:  // Meowth
            case 53:  // Persian
                if (formId == 1) return "阿罗拉的样子";
                if (formId == 2) return "伽勒尔的样子";
                break;

            // Galarian Forms (Gen 8)
            case 77:  // Ponyta
            case 78:  // Rapidash
            case 79:  // Slowpoke
            case 80:  // Slowbro
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
            case 555: // Darmanitan
            case 562: // Yamask
            case 618: // Stunfisk
                if (formId == 1) return "伽勒尔的样子";
                break;

            // Hisuian Forms (Gen 8 - Legends: Arceus)
            case 58:  // Growlithe
            case 59:  // Arcanine
            case 100: // Voltorb
            case 101: // Electrode
            case 157: // Typhlosion
            case 211: // Qwilfish
            case 215: // Sneasel
            case 503: // Samurott
            case 549: // Lilligant
            case 570: // Zorua
            case 571: // Zoroark
            case 705: // Sliggoo
            case 706: // Goodra
            case 713: // Avalugg
            case 724: // Decidueye
                if (formId == 1) return "洗翠的样子";
                break;

            // Wooper - Paldean form
            case 194: // Wooper
                if (formId == 1) return "帕底亚的样子";
                break;

            // Tauros - Paldean breeds
            case 128: // Tauros
                if (formId == 1) return "Combat Breed";
                if (formId == 2) return "Blaze Breed";
                if (formId == 3) return "Aqua Breed";
                break;

            // Pokemon with multiple permanent forms
            // ========================================

            // Gender difference
            case 678: // Meowstic
            case 876: // Indeedee
            case 902: // Basculegion
            case 916: // Oinkologne
                if (formId == 0) return "雄性的样子";
                if (formId == 1) return "雌性的样子";
                break;

            // Deoxys - Different stats per form
            case 386: // Deoxys
                if (formId == 0) return "一般";
                if (formId == 1) return "攻击形态";
                if (formId == 2) return "防御形态";
                if (formId == 3) return "速度形态";
                break;

            // Burmy/Wormadam - Cloak forms
            case 412: // Burmy
            case 413: // Wormadam
                if (formId == 0) return "Plant Cloak";
                if (formId == 1) return "Sandy Cloak";
                if (formId == 2) return "Trash Cloak";
                break;

            // Rotom - Appliance forms
            case 479: // Rotom
                if (formId == 1) return "加热";
                if (formId == 2) return "清洗";
                if (formId == 3) return "结冰";
                if (formId == 4) return "旋转";
                if (formId == 5) return "切割";
                break;

            // Giratina - Altered/Origin
            case 487: // Giratina
                if (formId == 0) return "别种形态";
                if (formId == 1) return "起源形态";
                break;

            // Shaymin - Land/Sky
            case 492: // Shaymin
                if (formId == 0) return "陆上形态";
                if (formId == 1) return "天空形态";
                break;

            // Basculin - Forms
            case 550: // Basculin
                if (formId == 0) return "Red-Striped";
                if (formId == 1) return "Blue-Striped";
                if (formId == 2) return "White-Striped";
                break;

            // Tornadus/Thundurus/Landorus - Incarnate/Therian forms
            case 641: // Tornadus
            case 642: // Thundurus
            case 645: // Landorus
                if (formId == 0) return "化身形态";
                if (formId == 1) return "灵兽形态";
                break;

            // Kyurem - Fusions
            case 646: // Kyurem
                if (formId == 1) return "白色";
                if (formId == 2) return "暗黑酋雷姆";
                break;

            // Keldeo - Resolute
            case 647: // Keldeo
                if (formId == 0) return "平常的样子";
                if (formId == 1) return "觉悟的样子";
                break;

            // Meloetta - Pirouette
            case 648: // Meloetta
                if (formId == 0) return "歌声形态";
                if (formId == 1) return "舞步形态";
                break;

            // Pumpkaboo/Gourgeist - Size variants
            case 710: // Pumpkaboo
            case 711: // Gourgeist
                if (formId == 0) return "普通尺寸";
                if (formId == 1) return "小颗种";
                if (formId == 2) return "大颗种";
                if (formId == 3) return "特大尺寸";
                break;

            // Zygarde - Forms
            case 718: // Zygarde
                if (formId == 0) return "50％形态";
                if (formId == 1) return "10％形态";
                if (formId == 4) return "完全体形态";
                break;

            // Hoopa - Forms
            case 720: // Zygarde
                if (formId == 0) return "惩戒胡帕";
                if (formId == 1) return "解放胡帕";
                break;

            // Oricorio - Styles
            case 741: // Oricorio
                if (formId == 0) return "热辣热辣风格";
                if (formId == 1) return "啪滋啪滋风格";
                if (formId == 2) return "Pa'u";
                if (formId == 3) return "轻盈轻盈风格";
                break;

            // Lycanroc - Forms
            case 745: // Lycanroc
                if (formId == 0) return "白昼的样子";
                if (formId == 1) return "黑夜的样子";
                if (formId == 2) return "黄昏之鬃";
                break;

            // Necrozma - Fusions
            case 800: // Necrozma
                if (formId == 1) return "Dusk Mane";
                if (formId == 2) return "Dawn Wings";
                if (formId == 3) return "究极奈克洛兹玛";
                break;

            // Toxtricity - Forms
            case 849: // Toxtricity
                if (formId == 0) return "Amped";
                if (formId == 1) return "低调的样子";
                break;

            // Zacian/Zamazenta - Crowned forms
            case 888: // Zacian
                if (formId == 1) return "Crowned Sword";
                break;
            case 889: // Zamazenta
                if (formId == 1) return "Crowned Shield";
                break;

            // Urshifu - Styles
            case 892: // Urshifu
                if (formId == 1) return "连击流";
                break;

            // Calyrex - Riders
            case 898: // Calyrex
                if (formId == 1) return "Ice Rider";
                if (formId == 2) return "Shadow Rider";
                break;

            // Ursaluna - Bloodmoon
            case 901: // Ursaluna
                if (formId == 1) return "赫月";
                break;

            // Enamorus - Therian
            case 905: // Enamorus
                if (formId == 1) return "灵兽形态";
                break;

            // Maushold - Family size
            case 925: // Maushold
                if (formId == 1) return "三只家庭";
                break;

            // Squawkabilly - Plumages
            case 931: // Squawkabilly
                if (formId == 1) return "Blue Plumage";
                if (formId == 2) return "Yellow Plumage";
                if (formId == 3) return "White Plumage";
                break;

            // Tatsugiri - Forms
            case 978: // Tatsugiri
                if (formId == 1) return "下垂姿势";
                if (formId == 2) return "平挺姿势";
                break;

            // Dudunsparce - Segment count
            case 982: // Dudunsparce
                if (formId == 1) return "三节形态";
                break;

            // Gimmighoul - Forms
            case 999: // Gimmighoul
                if (formId == 1) return "徒步形态";
                break;

            // Poltchageist/Sinistcha - Forms
            case 1012: // Poltchageist
            case 1013: // Sinistcha
                if (formId == 1) return "高档货的样子";
                break;

            // Ogerpon - Masks (permanent forms when holding masks)
            case 1017: // Ogerpon
                if (formId == 1) return "Wellspring Mask";
                if (formId == 2) return "Hearthflame Mask";
                if (formId == 3) return "Cornerstone Mask";
                break;

            // Terapagos - Forms
            case 1024: // Terapagos
                if (formId == 1) return "太晶形态";
                if (formId == 2) return "星晶形态";
                break;
        }

        // If no match found, return empty string
        return "";
    }
}
