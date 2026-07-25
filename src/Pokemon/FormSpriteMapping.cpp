/**
 * FormSpriteMapping.cpp - Pokemon Form to Sprite ID Mapping Implementation
 *
 * Maps species+form combinations to PokeAPI sprite IDs.
 * PokeAPI uses IDs 10001+ for alternate forms.
 *
 * Reference: https://pokeapi.co/api/v2/pokemon-form/
 */

#include "Pokemon/FormSpriteMapping.h"

namespace Pokemon {

uint32_t getFormSpriteId(uint16_t speciesId, uint8_t formId) {
    // Base forms use species ID directly
    if (formId == 0) {
        return speciesId;
    }

    // Map species+form to PokeAPI sprite ID
    switch (speciesId) {
        // Deoxys forms
        case 386:
            if (formId == 1) return 10001; // Attack
            if (formId == 2) return 10002; // Defense
            if (formId == 3) return 10003; // Speed
            break;

        // Wormadam forms
        case 413:
            if (formId == 1) return 10004; // Sandy Cloak
            if (formId == 2) return 10005; // Trash Cloak
            break;

        // Shaymin
        case 492:
            if (formId == 1) return 10006; // Sky Forme
            break;

        // Giratina
        case 487:
            if (formId == 1) return 10007; // Origin Forme
            break;

        // Rotom forms
        case 479:
            if (formId == 1) return 10008; // Heat
            if (formId == 2) return 10009; // Wash
            if (formId == 3) return 10010; // Frost
            if (formId == 4) return 10011; // Fan
            if (formId == 5) return 10012; // Mow
            break;

        // Basculin
        case 550:
            if (formId == 1) return 10016; // Blue-Striped
            if (formId == 2) return 10246; // White-Striped
            break;

        // Tornadus
        case 641:
            if (formId == 1) return 10019; // Therian
            break;

        // Thundurus
        case 642:
            if (formId == 1) return 10020; // Therian
            break;

        // Landorus
        case 645:
            if (formId == 1) return 10021; // Therian
            break;

        // Kyurem
        case 646:
            if (formId == 1) return 10022; // White
            if (formId == 2) return 10023; // Black
            break;

        // Keldeo
        case 647:
            if (formId == 1) return 10024; // Resolute
            break;

        // Meowstic
        case 678:
            if (formId == 1) return 10025; // Female
            break;

        // Pumpkaboo sizes
        case 710:
            if (formId == 1) return 10027; // Small
            if (formId == 2) return 10028; // Large
            if (formId == 3) return 10029; // Super
            break;

        // Gourgeist sizes
        case 711:
            if (formId == 1) return 10030; // Small
            if (formId == 2) return 10031; // Large
            if (formId == 3) return 10032; // Super
            break;

        // Zygarde forms
        case 718:
            if (formId == 1) return 10118; // 10%
            if (formId == 4) return 10120; // Complete
            break;

        // Hoopa
        case 720:
            if (formId == 1) return 10086; // Unbound
            break;

        // Oricorio
        case 741:
            if (formId == 1) return 10123; // Pom-Pom
            if (formId == 2) return 10124; // Pa'u
            if (formId == 3) return 10125; // Sensu
            break;

        // Lycanroc
        case 745:
            if (formId == 1) return 10126; // Midnight
            if (formId == 2) return 10152; // Dusk
            break;

        // Necrozma
        case 800:
            if (formId == 1) return 10155; // Dusk Mane
            if (formId == 2) return 10156; // Dawn Wings
            if (formId == 3) return 10157; // Ultra
            break;

        // Toxtricity
        case 849:
            if (formId == 1) return 10184; // Low Key
            break;

        // Indeedee
        case 876:
            if (formId == 1) return 10185; // Female
            break;

        // Zacian
        case 888:
            if (formId == 1) return 10188; // Crowned Sword
            break;

        // Zamazenta
        case 889:
            if (formId == 1) return 10189; // Crowned Shield
            break;

        // Urshifu
        case 892:
            if (formId == 1) return 10191; // Rapid Strike
            break;

        // Calyrex
        case 898:
            if (formId == 1) return 10193; // Ice Rider
            if (formId == 2) return 10194; // Shadow Rider
            break;

        // Ursaluna
        case 901:
            if (formId == 1) return 10272; // Blood Moon
            break;

        // Basculegion
        case 902:
            if (formId == 1) return 10248; // Female
            break;

        // Enamorus
        case 905:
            if (formId == 1) return 10249; // Therian
            break;

        // Oinkologne
        case 916:
            if (formId == 1) return 10254; // Female
            break;

        // Maushold
        case 925:
            if (formId == 1) return 10256; // Family of Three
            break;

        // Squawkabilly
        case 931:
            if (formId == 1) return 10258; // Blue Plumage
            if (formId == 2) return 10259; // Yellow Plumage
            if (formId == 3) return 10260; // White Plumage
            break;

        // Tatsugiri
        case 978:
            if (formId == 1) return 10268; // Droopy
            if (formId == 2) return 10269; // Stretchy
            break;

        // Dudunsparce
        case 982:
            if (formId == 1) return 10270; // Three-Segment
            break;

        // Gimmighoul
        case 999:
            if (formId == 1) return 10271; // Roaming
            break;

        // Poltchageist
        case 1012:
            if (formId == 1) return 10273; // Artisan
            break;

        // Sinistcha
        case 1013:
            if (formId == 1) return 10274; // Masterpiece
            break;

        // Ogerpon
        case 1017:
            if (formId == 1) return 10275; // Wellspring Mask
            if (formId == 2) return 10276; // Hearthflame Mask
            if (formId == 3) return 10277; // Cornerstone Mask
            break;

        // === Alolan Forms ===
        case 19:  // Rattata
            if (formId == 1) return 10091;
            break;
        case 20:  // Raticate
            if (formId == 1) return 10092;
            break;
        case 26:  // Raichu
            if (formId == 1) return 10100;
            break;
        case 27:  // Sandshrew
            if (formId == 1) return 10101;
            break;
        case 28:  // Sandslash
            if (formId == 1) return 10102;
            break;
        case 37:  // Vulpix
            if (formId == 1) return 10103;
            break;
        case 38:  // Ninetales
            if (formId == 1) return 10104;
            break;
        case 50:  // Diglett
            if (formId == 1) return 10105;
            break;
        case 51:  // Dugtrio
            if (formId == 1) return 10106;
            break;
        case 52:  // Meowth
            if (formId == 1) return 10107; // Alolan
            if (formId == 2) return 10161; // Galarian
            break;
        case 53:  // Persian
            if (formId == 1) return 10108; // Alolan
            break;
        case 74:  // Geodude
            if (formId == 1) return 10109;
            break;
        case 75:  // Graveler
            if (formId == 1) return 10110;
            break;
        case 76:  // Golem
            if (formId == 1) return 10111;
            break;
        case 88:  // Grimer
            if (formId == 1) return 10112;
            break;
        case 89:  // Muk
            if (formId == 1) return 10113;
            break;
        case 103: // Exeggutor
            if (formId == 1) return 10114;
            break;
        case 105: // Marowak
            if (formId == 1) return 10115;
            break;

        // === Galarian Forms ===
        case 77:  // Ponyta
            if (formId == 1) return 10162;
            break;
        case 78:  // Rapidash
            if (formId == 1) return 10163;
            break;
        case 79:  // Slowpoke
            if (formId == 1) return 10164;
            break;
        case 80:  // Slowbro
            if (formId == 1) return 10165;
            break;
        case 83:  // Farfetch'd
            if (formId == 1) return 10166;
            break;
        case 110: // Weezing
            if (formId == 1) return 10167;
            break;
        case 122: // Mr. Mime
            if (formId == 1) return 10168;
            break;
        case 144: // Articuno
            if (formId == 1) return 10169;
            break;
        case 145: // Zapdos
            if (formId == 1) return 10170;
            break;
        case 146: // Moltres
            if (formId == 1) return 10171;
            break;
        case 199: // Slowking
            if (formId == 1) return 10172;
            break;
        case 222: // Corsola
            if (formId == 1) return 10173;
            break;
        case 263: // Zigzagoon
            if (formId == 1) return 10174;
            break;
        case 264: // Linoone
            if (formId == 1) return 10175;
            break;
        case 554: // Darumaka
            if (formId == 1) return 10176;
            break;
        case 555: // Darmanitan
            if (formId == 1) return 10177; // Galarian Standard
            break;
        case 562: // Yamask
            if (formId == 1) return 10178;
            break;
        case 618: // Stunfisk
            if (formId == 1) return 10179;
            break;

        // === Hisuian Forms ===
        case 58:  // Growlithe
            if (formId == 1) return 10229;
            break;
        case 59:  // Arcanine
            if (formId == 1) return 10230;
            break;
        case 100: // Voltorb
            if (formId == 1) return 10231;
            break;
        case 101: // Electrode
            if (formId == 1) return 10232;
            break;
        case 157: // Typhlosion
            if (formId == 1) return 10233;
            break;
        case 211: // Qwilfish
            if (formId == 1) return 10234;
            break;
        case 215: // Sneasel
            if (formId == 1) return 10235;
            break;
        case 503: // Samurott
            if (formId == 1) return 10236;
            break;
        case 549: // Lilligant
            if (formId == 1) return 10237;
            break;
        case 570: // Zorua
            if (formId == 1) return 10238;
            break;
        case 571: // Zoroark
            if (formId == 1) return 10239;
            break;
        case 628: // Braviary
            if (formId == 1) return 10240;
            break;
        case 705: // Sliggoo
            if (formId == 1) return 10241;
            break;
        case 706: // Goodra
            if (formId == 1) return 10242;
            break;
        case 713: // Avalugg
            if (formId == 1) return 10243;
            break;
        case 724: // Decidueye
            if (formId == 1) return 10244;
            break;

        // === Paldean Forms ===
        case 194: // Wooper
            if (formId == 1) return 10253;
            break;
        case 128: // Tauros
            if (formId == 1) return 10250; // Combat Breed
            if (formId == 2) return 10251; // Blaze Breed
            if (formId == 3) return 10252; // Aqua Breed
            break;
    }

    // No form sprite found, use base species sprite
    return speciesId;
}

} // namespace Pokemon
