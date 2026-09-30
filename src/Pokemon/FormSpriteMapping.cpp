/**
 * Maps species+form combinations to PokeAPI sprite IDs.
 * PokeAPI uses IDs 10001+ for alternate forms.
 *
 * Reference: https://pokeapi.co/api/v2/pokemon-form/
 */

#include "Pokemon/FormSpriteMapping.h"

namespace Pokemon
{

    uint32_t getFormSpriteId(uint16_t speciesId, uint8_t formId)
    {
        // Base forms use species ID directly
        // Maushold is special in that its base form is 1 -- not 0.
        if (formId == 0 && speciesId != 925)
        {
            return speciesId;
        }

        switch (speciesId)
        {
        // Deoxys forms
        case 386:
            // Attack
            if (formId == 1) return 10001;
            // Defense
            else if (formId == 2) return 10002;
            // Speed
            else if (formId == 3) return 10003;
            break;

        // Wormadam forms
        case 413:
            // Sandy Cloak
            if (formId == 1) return 10004;
            // Trash Cloak
            else if (formId == 2) return 10005;
            break;

        // Shaymin
        case 492:
            // Sky Forme
            if (formId == 1) return 10006;
            break;

        // Giratina
        case 487:
            // Origin Forme
            if (formId == 1) return 10007;
            break;

        // Rotom forms
        case 479:
            // Heat
            if (formId == 1) return 10008;
            // Wash
            else if (formId == 2) return 10009;
            // Frost
            else if (formId == 3) return 10010;
            // Fan
            else if (formId == 4) return 10011;
            // Mow
            else if (formId == 5) return 10012;
            break;

        // Basculin
        case 550:
            // Blue-Striped
            if (formId == 1) return 10016;
            // White-Striped (10246 is palkia-origin)
            else if (formId == 2) return 10247;
            break;

        // Tornadus
        case 641:
            // Therian
            if (formId == 1) return 10019;
            break;

        // Thundurus
        case 642:
            // Therian
            if (formId == 1) return 10020;
            break;

        // Landorus
        case 645:
            // Therian
            if (formId == 1) return 10021;
            break;

        // Kyurem
        case 646:
            // White (PokeAPI orders black=10022, white=10023)
            if (formId == 1) return 10023;
            // Black
            else if (formId == 2) return 10022;
            break;

        // Meloetta
        case 648:
            // Pirouette
            if (formId == 1) return 10018;
            break;

        // Keldeo
        case 647:
            // Resolute
            if (formId == 1) return 10024;
            break;

        // Meowstic
        case 678:
            if (formId == 1) return 10025;
            // Male Mega
            else if (formId == 2) return 10314;
            // Female Mega
            else if (formId == 3) return 10326;
            break;

        // Pumpkaboo sizes
        case 710:
            // Small
            if (formId == 1) return 10027;
            // Large
            else if (formId == 2) return 10028;
            // Super
            else if (formId == 3) return 10029;
            break;

        // Gourgeist sizes
        case 711:
            // Small
            if (formId == 1) return 10030;
            // Large
            else if (formId == 2) return 10031;
            // Super
            else if (formId == 3) return 10032;
            break;

        // Zygarde forms
        case 718:
            // 10118 is zygarde-10-POWER-CONSTRUCT; plain 10% (Aura Break) is 10181.
            if (formId == 1)
                return 10181; // 10%
            // 10% Power Construct
            else if (formId == 2) return 10118;
            // 50% Power Construct
            else if (formId == 3) return 10119;
            // Complete
            else if (formId == 4) return 10120;
            // Mega
            else if (formId == 5) return 10301;
            break;

        // Hoopa
        case 720:
            // Unbound
            if (formId == 1) return 10086;
            break;

        // Oricorio
        case 741:
            // Pom-Pom
            if (formId == 1) return 10123;
            // Pa'u
            else if (formId == 2) return 10124;
            // Sensu
            else if (formId == 3) return 10125;
            break;

        // Lycanroc
        case 745:
            // Midnight
            if (formId == 1) return 10126;
            // Dusk
            else if (formId == 2) return 10152;
            break;

        // Gen 7 Totem forms. Raticate/Marowak carry theirs at form 2 (above) because an Alolan
        // form is in the way; Mimikyu's are 2/3 for the same reason.
        case 735: // Gumshoos
            if (formId == 1)
                return 10121;
            break;
        case 738: // Vikavolt
            if (formId == 1)
                return 10122;
            break;
        case 743: // Ribombee
            if (formId == 1)
                return 10150;
            break;
        case 752: // Araquanid
            if (formId == 1)
                return 10153;
            break;
        case 754: // Lurantis
            if (formId == 1)
                return 10128;
            break;
        case 758: // Salazzle
            if (formId == 1)
                return 10129;
            break;
        case 777: // Togedemaru
            if (formId == 1)
                return 10154;
            break;
        case 784: // Kommo-o
            if (formId == 1)
                return 10146;
            break;

        // Rockruff -- the Own Tempo (Dusk-evolving) variant
        case 744:
            if (formId == 1)
                return 10151;
            break;

        // Zarude -- Dada
        case 893:
            if (formId == 1)
                return 10192;
            break;

        // Minior -- Meteor shells 0-6 then Cores 7-13. Form 0 (Meteor Red) is PokeAPI's
        // default variety (774), so it takes the form-0 short-circuit above.
        case 774:
            // Meteor Orange
            if (formId == 1) return 10130;
            // Meteor Yellow
            else if (formId == 2) return 10131;
            // Meteor Green
            else if (formId == 3) return 10132;
            // Meteor Blue
            else if (formId == 4) return 10133;
            // Meteor Indigo
            else if (formId == 5) return 10134;
            // Meteor Violet
            else if (formId == 6) return 10135;
            // Core Red
            else if (formId == 7) return 10136;
            // Core Orange
            else if (formId == 8) return 10137;
            // Core Yellow
            else if (formId == 9) return 10138;
            // Core Green
            else if (formId == 10) return 10139;
            // Core Blue
            else if (formId == 11) return 10140;
            // Core Indigo
            else if (formId == 12) return 10141;
            // Core Violet
            else if (formId == 13) return 10142;
            break;

        // Mimikyu -- 2/3 are the Gen 7 Totem entries
        case 778:
            // Busted
            if (formId == 1) return 10143;
            // Totem Disguised
            else if (formId == 2) return 10144;
            // Totem Busted
            else if (formId == 3) return 10145;
            break;

        // Battle-only forms. Hidden from the picker by default (isBattleOnlyForm), but a save can
        // hold one and "Allow illegal edits" reveals them, so they still need art.
        // Koraidon/Miraidon's ride builds are absent on purpose: PokeAPI indexes the ids
        // (10264-10271) but publishes no HOME render for any of them.
        case 351: // Castform
            // Sunny
            if (formId == 1) return 10013;
            // Rainy
            else if (formId == 2) return 10014;
            // Snowy
            else if (formId == 3) return 10015;
            break;
        case 681: // Aegislash
            // Blade
            if (formId == 1) return 10026;
            break;
        case 746: // Wishiwashi
            // School
            if (formId == 1) return 10127;
            break;
        case 845: // Cramorant
            // Gulping
            if (formId == 1) return 10182;
            // Gorging
            else if (formId == 2) return 10183;
            break;
        case 875: // Eiscue
            // Noice Face
            if (formId == 1) return 10185;
            break;
        case 877: // Morpeko
            // Hangry
            if (formId == 1) return 10187;
            break;
        case 890: // Eternatus
            // Eternamax
            if (formId == 1) return 10190;
            break;

        // Necrozma
        case 800:
            // Dusk Mane
            if (formId == 1) return 10155;
            // Dawn Wings
            else if (formId == 2) return 10156;
            // Ultra
            else if (formId == 3) return 10157;
            break;

        // Toxtricity
        case 849:
            // Low Key
            if (formId == 1) return 10184;
            break;

        // Indeedee
        case 876:
            if (formId == 1) return 10186;
            break;

        // Zacian
        case 888:
            // Crowned Sword
            if (formId == 1) return 10188;
            break;

        // Zamazenta
        case 889:
            // Crowned Shield
            if (formId == 1) return 10189;
            break;

        // Urshifu
        case 892:
            // Rapid Strike
            if (formId == 1) return 10191;
            break;

        // Calyrex
        case 898:
            // Ice Rider
            if (formId == 1) return 10193;
            // Shadow Rider
            else if (formId == 2) return 10194;
            break;

        // Ursaluna
        case 901:
            // Blood Moon
            if (formId == 1) return 10272;
            break;

        // Basculegion
        case 902:
            if (formId == 1) return 10248;
            break;

        // Enamorus
        case 905:
            // Therian
            if (formId == 1) return 10249;
            break;

        // Oinkologne
        case 916:
            if (formId == 1) return 10254;
            break;

        // Maushold
        // The forms have been switched here for some reason — form 0 is the rare variant while form 1 is the base
        // variant.
        case 925:
            // Family of Three
            if (formId == 0) return 10257;
            // Family of Four
            else if (formId == 1) return 925;
            break;

        // Squawkabilly
        case 931:
            // Blue Plumage
            if (formId == 1) return 10260;
            // Yellow Plumage
            else if (formId == 2) return 10261;
            // White Plumage
            else if (formId == 3) return 10262;
            break;

        // Tatsugiri
        case 978:
            // Droopy (10268/10269 are Miraidon ride modes)
            if (formId == 1) return 10258;
            // Stretchy
            else if (formId == 2) return 10259;
            // Curly Mega
            else if (formId == 3) return 10322;
            // Droopy Mega
            else if (formId == 4) return 10323;
            // Stretchy Mega
            else if (formId == 5) return 10324;
            break;

        // Dudunsparce
        case 982:
            // Three-Segment
            if (formId == 1) return 10255;
            break;

        // Gimmighoul
        case 999:
            // Roaming
            if (formId == 1) return 10263;
            break;

        // For Poltchageist and Sinistcha there are no sprites for their Artisan/Masterpiece forms — use the base
        // sprites for now
        // // Poltchageist
        // case 1012:
        //     if (formId == 1) return 10273; // Artisan // Wrong sprite
        //     break;

        // // Sinistcha
        // case 1013:
        //     if (formId == 1) return 10274; // Masterpiece // Wrong sprite
        //     break;

        // Ogerpon
        case 1017:
            // Wellspring Mask
            if (formId == 1) return 10273;
            // Hearthflame Mask
            else if (formId == 2) return 10274;
            // Cornerstone Mask
            else if (formId == 3) return 10275;
            break;

        // Palafin
        case 964:
            // Hero
            if (formId == 1) return 10256;
            break;

        // Terapagos
        case 1024:
            // Terastal
            if (formId == 1) return 10276;
            // Stellar
            else if (formId == 2) return 10277;
            break;

        // === Alolan Forms ===
        case 19: // Rattata
            if (formId == 1)
                return 10091;
            break;
        case 20: // Raticate
            if (formId == 1)
                return 10092;
            // Totem (the Alolan one)
            else if (formId == 2) return 10093;
            break;

        // Pikachu -- event caps. PokeAPI has no HOME render for the Let's Go partner (form 8),
        // and the caps have no shiny render, so both fall back to the base species.
        case 25: // Pikachu
            // Original Cap
            if (formId == 1) return 10094;
            // Hoenn Cap
            else if (formId == 2) return 10095;
            // Sinnoh Cap
            else if (formId == 3) return 10096;
            // Unova Cap
            else if (formId == 4) return 10097;
            // Kalos Cap
            else if (formId == 5) return 10098;
            // Alola Cap
            else if (formId == 6) return 10099;
            // Partner Cap
            else if (formId == 7) return 10148;
            // World Cap
            else if (formId == 9) return 10160;
            break;
        case 26: // Raichu
            if (formId == 1)
                return 10100;
            // Mega X
            else if (formId == 2) return 10304;
            // Mega Y
            else if (formId == 3) return 10305;
            break;
        case 27: // Sandshrew
            if (formId == 1)
                return 10101;
            break;
        case 28: // Sandslash
            if (formId == 1)
                return 10102;
            break;
        case 37: // Vulpix
            if (formId == 1)
                return 10103;
            break;
        case 38: // Ninetales
            if (formId == 1)
                return 10104;
            break;
        case 50: // Diglett
            if (formId == 1)
                return 10105;
            break;
        case 51: // Dugtrio
            if (formId == 1)
                return 10106;
            break;
        case 52: // Meowth
            // Alolan
            if (formId == 1) return 10107;
            // Galarian
            else if (formId == 2) return 10161;
            break;
        case 53: // Persian
            // Alolan
            if (formId == 1) return 10108;
            break;
        case 74: // Geodude
            if (formId == 1)
                return 10109;
            break;
        case 75: // Graveler
            if (formId == 1)
                return 10110;
            break;
        case 76: // Golem
            if (formId == 1)
                return 10111;
            break;
        case 88: // Grimer
            if (formId == 1)
                return 10112;
            break;
        case 89: // Muk
            if (formId == 1)
                return 10113;
            break;
        case 103: // Exeggutor
            if (formId == 1)
                return 10114;
            break;
        case 105: // Marowak
            if (formId == 1)
                return 10115;
            // Totem (the Alolan one)
            else if (formId == 2) return 10149;
            break;

        // === Galarian Forms ===
        case 77: // Ponyta
            if (formId == 1)
                return 10162;
            break;
        case 78: // Rapidash
            if (formId == 1)
                return 10163;
            break;
        case 79: // Slowpoke
            if (formId == 1)
                return 10164;
            break;
        case 80: // Slowbro -- Mega kept form 1 from Gen 6, so Galar was appended at 2
            // Mega
            if (formId == 1) return 10071;
            // Galarian
            else if (formId == 2) return 10165;
            break;
        case 83: // Farfetch'd
            if (formId == 1)
                return 10166;
            break;
        case 110: // Weezing
            if (formId == 1)
                return 10167;
            break;
        case 122: // Mr. Mime
            if (formId == 1)
                return 10168;
            break;
        case 144: // Articuno
            if (formId == 1)
                return 10169;
            break;
        case 145: // Zapdos
            if (formId == 1)
                return 10170;
            break;
        case 146: // Moltres
            if (formId == 1)
                return 10171;
            break;
        case 199: // Slowking
            if (formId == 1)
                return 10172;
            break;
        case 222: // Corsola
            if (formId == 1)
                return 10173;
            break;
        case 263: // Zigzagoon
            if (formId == 1)
                return 10174;
            break;
        case 264: // Linoone
            if (formId == 1)
                return 10175;
            break;
        case 554: // Darumaka
            if (formId == 1)
                return 10176;
            break;
        case 555: // Darmanitan -- Zen holds form 1, so Galarian sits at 2 (not the usual shape)
            // Zen
            if (formId == 1) return 10017;
            // Galarian Standard
            else if (formId == 2) return 10177;
            // Galarian Zen
            else if (formId == 3) return 10178;
            break;
        case 562: // Yamask
            // 10178 is darmanitan-galar-zen
            if (formId == 1) return 10179;
            break;
        case 618: // Stunfisk
            // 10179 is yamask-galar
            if (formId == 1) return 10180;
            break;

        // === Hisuian Forms ===
        case 58: // Growlithe
            if (formId == 1)
                return 10229;
            break;
        case 59: // Arcanine
            if (formId == 1)
                return 10230;
            break;
        case 100: // Voltorb
            if (formId == 1)
                return 10231;
            break;
        case 101: // Electrode
            if (formId == 1)
                return 10232;
            break;
        case 157: // Typhlosion
            if (formId == 1)
                return 10233;
            break;
        case 211: // Qwilfish
            if (formId == 1)
                return 10234;
            break;
        case 215: // Sneasel
            if (formId == 1)
                return 10235;
            break;
        case 503: // Samurott
            if (formId == 1)
                return 10236;
            break;
        case 549: // Lilligant
            if (formId == 1)
                return 10237;
            break;
        case 570: // Zorua
            if (formId == 1)
                return 10238;
            break;
        case 571: // Zoroark
            if (formId == 1)
                return 10239;
            break;
        case 628: // Braviary
            if (formId == 1)
                return 10240;
            break;
        case 705: // Sliggoo
            if (formId == 1)
                return 10241;
            break;
        case 706: // Goodra
            if (formId == 1)
                return 10242;
            break;
        case 713: // Avalugg
            if (formId == 1)
                return 10243;
            break;
        case 724: // Decidueye
            if (formId == 1)
                return 10244;
            break;

        // === Paldean Forms ===
        case 194: // Wooper
            if (formId == 1)
                return 10253;
            break;
        case 128: // Tauros
            // Combat Breed
            if (formId == 1) return 10250;
            // Blaze Breed
            else if (formId == 2) return 10251;
            // Aqua Breed
            else if (formId == 3) return 10252;
            break;

            // === Mega Evolutions / Primal Reversions ===
            // Mega is a storable form again in Legends: Z-A, so these are pickable. Form indices
            // follow PKHeX's FormConverter: a single-mega species puts Mega at form 1; Charizard,
            // Mewtwo and Raichu use Mega X/Y; Absol, Garchomp and Lucario use Mega + Mega Z;
            // Slowbro's Mega sits after its Galarian form. Every id below was resolved by NAME
            // against PokeAPI -- do not hand-edit them.

        case 3: // Venusaur
            // Mega
            if (formId == 1) return 10033;
            break;

        case 6: // Charizard
            // Mega X
            if (formId == 1) return 10034;
            // Mega Y
            else if (formId == 2) return 10035;
            break;

        case 9: // Blastoise
            // Mega
            if (formId == 1) return 10036;
            break;

        case 15: // Beedrill
            // Mega
            if (formId == 1) return 10090;
            break;

        case 18: // Pidgeot
            // Mega
            if (formId == 1) return 10073;
            break;

        case 36: // Clefable
            // Mega
            if (formId == 1) return 10278;
            break;

        case 65: // Alakazam
            // Mega
            if (formId == 1) return 10037;
            break;

        case 71: // Victreebel
            // Mega
            if (formId == 1) return 10279;
            break;

        case 94: // Gengar
            // Mega
            if (formId == 1) return 10038;
            break;

        case 115: // Kangaskhan
            // Mega
            if (formId == 1) return 10039;
            break;

        case 121: // Starmie
            // Mega
            if (formId == 1) return 10280;
            break;

        case 127: // Pinsir
            // Mega
            if (formId == 1) return 10040;
            break;

        case 130: // Gyarados
            // Mega
            if (formId == 1) return 10041;
            break;

        case 142: // Aerodactyl
            // Mega
            if (formId == 1) return 10042;
            break;

        case 149: // Dragonite
            // Mega
            if (formId == 1) return 10281;
            break;

        case 150: // Mewtwo
            // Mega X
            if (formId == 1) return 10043;
            // Mega Y
            else if (formId == 2) return 10044;
            break;

        case 154: // Meganium
            // Mega
            if (formId == 1) return 10282;
            break;

        case 160: // Feraligatr
            // Mega
            if (formId == 1) return 10283;
            break;

        case 181: // Ampharos
            // Mega
            if (formId == 1) return 10045;
            break;

        case 208: // Steelix
            // Mega
            if (formId == 1) return 10072;
            break;

        case 212: // Scizor
            // Mega
            if (formId == 1) return 10046;
            break;

        case 214: // Heracross
            // Mega
            if (formId == 1) return 10047;
            break;

        case 227: // Skarmory
            // Mega
            if (formId == 1) return 10284;
            break;

        case 229: // Houndoom
            // Mega
            if (formId == 1) return 10048;
            break;

        case 248: // Tyranitar
            // Mega
            if (formId == 1) return 10049;
            break;

        case 254: // Sceptile
            // Mega
            if (formId == 1) return 10065;
            break;

        case 257: // Blaziken
            // Mega
            if (formId == 1) return 10050;
            break;

        case 260: // Swampert
            // Mega
            if (formId == 1) return 10064;
            break;

        case 282: // Gardevoir
            // Mega
            if (formId == 1) return 10051;
            break;

        case 302: // Sableye
            // Mega
            if (formId == 1) return 10066;
            break;

        case 303: // Mawile
            // Mega
            if (formId == 1) return 10052;
            break;

        case 306: // Aggron
            // Mega
            if (formId == 1) return 10053;
            break;

        case 308: // Medicham
            // Mega
            if (formId == 1) return 10054;
            break;

        case 310: // Manectric
            // Mega
            if (formId == 1) return 10055;
            break;

        case 319: // Sharpedo
            // Mega
            if (formId == 1) return 10070;
            break;

        case 323: // Camerupt
            // Mega
            if (formId == 1) return 10087;
            break;

        case 334: // Altaria
            // Mega
            if (formId == 1) return 10067;
            break;

        case 354: // Banette
            // Mega
            if (formId == 1) return 10056;
            break;

        case 358: // Chimecho
            // Mega
            if (formId == 1) return 10306;
            break;

        case 359: // Absol
            // Mega
            if (formId == 1) return 10057;
            // Mega Z
            else if (formId == 2) return 10307;
            break;

        case 362: // Glalie
            // Mega
            if (formId == 1) return 10074;
            break;

        case 373: // Salamence
            // Mega
            if (formId == 1) return 10089;
            break;

        case 376: // Metagross
            // Mega
            if (formId == 1) return 10076;
            break;

        case 380: // Latias
            // Mega
            if (formId == 1) return 10062;
            break;

        case 381: // Latios
            // Mega
            if (formId == 1) return 10063;
            break;

        case 382: // Kyogre
            // Primal
            if (formId == 1) return 10077;
            break;

        case 383: // Groudon
            // Primal
            if (formId == 1) return 10078;
            break;

        case 384: // Rayquaza
            // Mega
            if (formId == 1) return 10079;
            break;

        case 398: // Staraptor
            // Mega
            if (formId == 1) return 10308;
            break;

        case 428: // Lopunny
            // Mega
            if (formId == 1) return 10088;
            break;

        case 445: // Garchomp
            // Mega
            if (formId == 1) return 10058;
            // Mega Z
            else if (formId == 2) return 10309;
            break;

        case 448: // Lucario
            // Mega
            if (formId == 1) return 10059;
            // Mega Z
            else if (formId == 2) return 10310;
            break;

        case 460: // Abomasnow
            // Mega
            if (formId == 1) return 10060;
            break;

        case 475: // Gallade
            // Mega
            if (formId == 1) return 10068;
            break;

        case 478: // Froslass
            // Mega
            if (formId == 1) return 10285;
            break;

        case 483: // Dialga
            // Origin
            if (formId == 1) return 10245;
            break;

        case 484: // Palkia
            // Origin
            if (formId == 1) return 10246;
            break;

        case 485: // Heatran
            // Mega
            if (formId == 1) return 10311;
            break;

        case 491: // Darkrai
            // Mega
            if (formId == 1) return 10312;
            break;

        case 500: // Emboar
            // Mega
            if (formId == 1) return 10286;
            break;

        case 530: // Excadrill
            // Mega
            if (formId == 1) return 10287;
            break;

        case 531: // Audino
            // Mega
            if (formId == 1) return 10069;
            break;

        case 545: // Scolipede
            // Mega
            if (formId == 1) return 10288;
            break;

        case 560: // Scrafty
            // Mega
            if (formId == 1) return 10289;
            break;

        case 604: // Eelektross
            // Mega
            if (formId == 1) return 10290;
            break;

        case 609: // Chandelure
            // Mega
            if (formId == 1) return 10291;
            break;

        case 623: // Golurk
            // Mega
            if (formId == 1) return 10313;
            break;

        case 652: // Chesnaught
            // Mega
            if (formId == 1) return 10292;
            break;

        case 655: // Delphox
            // Mega
            if (formId == 1) return 10293;
            break;

        case 658: // Greninja
            // Battle Bond
            if (formId == 1) return 10116;
            // Ash
            else if (formId == 2) return 10117;
            // Mega
            else if (formId == 3) return 10294;
            break;

        case 668: // Pyroar
            // Mega
            if (formId == 1) return 10295;
            break;

        case 670: // Floette
            // Eternal
            if (formId == 5) return 10061;
            // Mega
            else if (formId == 6) return 10296;
            break;

        case 687: // Malamar
            // Mega
            if (formId == 1) return 10297;
            break;

        case 689: // Barbaracle
            // Mega
            if (formId == 1) return 10298;
            break;

        case 691: // Dragalge
            // Mega
            if (formId == 1) return 10299;
            break;

        case 701: // Hawlucha
            // Mega
            if (formId == 1) return 10300;
            break;

        case 719: // Diancie
            // Mega
            if (formId == 1) return 10075;
            break;

        case 740: // Crabominable
            // Mega
            if (formId == 1) return 10315;
            break;

        case 768: // Golisopod
            // Mega
            if (formId == 1) return 10316;
            break;

        case 780: // Drampa
            // Mega
            if (formId == 1) return 10302;
            break;

        case 801: // Magearna
            // Original Color
            if (formId == 1) return 10147;
            // Mega
            else if (formId == 2) return 10317;
            // Mega Original Color
            else if (formId == 3) return 10318;
            break;

        case 807: // Zeraora
            // Mega
            if (formId == 1) return 10319;
            break;

        case 870: // Falinks
            // Mega
            if (formId == 1) return 10303;
            break;

        case 952: // Scovillain
            // Mega
            if (formId == 1) return 10320;
            break;

        case 970: // Glimmora
            // Mega
            if (formId == 1) return 10321;
            break;

        case 998: // Baxcalibur
            // Mega
            if (formId == 1) return 10325;
            break;
        }

        // No form sprite found, use base species sprite
        return speciesId;
    }

    namespace
    {
        // Unown: A-Z, then ! and ?
        static const char *const UNOWN[] = {
            "201-a",
            "201-b",
            "201-c",
            "201-d",
            "201-e",
            "201-f",
            "201-g",
            "201-h",
            "201-i",
            "201-j",
            "201-k",
            "201-l",
            "201-m",
            "201-n",
            "201-o",
            "201-p",
            "201-q",
            "201-r",
            "201-s",
            "201-t",
            "201-u",
            "201-v",
            "201-w",
            "201-x",
            "201-y",
            "201-z",
            "201-exclamation",
            "201-question",
        };

        static const char *const BURMY[] = {
            "412-plant",
            "412-sandy",
            "412-trash",
        };

        static const char *const CHERRIM[] = {
            "421-overcast",
            "421-sunshine",
        };

        static const char *const SHELLOS[] = {
            "422-west",
            "422-east",
        };

        static const char *const GASTRODON[] = {
            "423-west",
            "423-east",
        };

        // Arceus: plate types (form 0 = plain 493; form 18 = Legend, no art)
        static const char *const ARCEUS[] = {
            "",
            "493-fighting",
            "493-flying",
            "493-poison",
            "493-ground",
            "493-rock",
            "493-bug",
            "493-ghost",
            "493-steel",
            "493-fire",
            "493-water",
            "493-grass",
            "493-electric",
            "493-psychic",
            "493-ice",
            "493-dragon",
            "493-dark",
            "493-fairy",
        };

        static const char *const DEERLING[] = {
            "585-spring",
            "585-summer",
            "585-autumn",
            "585-winter",
        };

        static const char *const SAWSBUCK[] = {
            "586-spring",
            "586-summer",
            "586-autumn",
            "586-winter",
        };

        // Genesect: drives (form 0 has no -suffix file)
        static const char *const GENESECT[] = {
            "",
            "649-douse",
            "649-shock",
            "649-burn",
            "649-chill",
        };

        static const char *const VIVILLON[] = {
            "666-icy-snow",
            "666-polar",
            "666-tundra",
            "666-continental",
            "666-garden",
            "666-elegant",
            "666-meadow",
            "666-modern",
            "666-marine",
            "666-archipelago",
            "666-high-plains",
            "666-sandstorm",
            "666-river",
            "666-monsoon",
            "666-savanna",
            "666-sun",
            "666-ocean",
            "666-jungle",
            "666-fancy",
            "666-poke-ball",
        };

        // Flabebe: flower colours
        static const char *const FLABEBE[] = {
            "669-red",
            "669-yellow",
            "669-orange",
            "669-blue",
            "669-white",
        };

        // Floette: flower colours (5 Eternal / 6 Mega are numeric ids)
        static const char *const FLOETTE[] = {
            "670-red",
            "670-yellow",
            "670-orange",
            "670-blue",
            "670-white",
        };

        // Florges: flower colours
        static const char *const FLORGES[] = {
            "671-red",
            "671-yellow",
            "671-orange",
            "671-blue",
            "671-white",
        };

        static const char *const FURFROU[] = {
            "676-natural",
            "676-heart",
            "676-star",
            "676-diamond",
            "676-debutante",
            "676-matron",
            "676-dandy",
            "676-la-reine",
            "676-kabuki",
            "676-pharaoh",
        };

        static const char *const XERNEAS[] = {
            "716-neutral",
            "716-active",
        };

        // Silvally: memory types
        static const char *const SILVALLY[] = {
            "773-normal",
            "773-fighting",
            "773-flying",
            "773-poison",
            "773-ground",
            "773-rock",
            "773-bug",
            "773-ghost",
            "773-steel",
            "773-fire",
            "773-water",
            "773-grass",
            "773-electric",
            "773-psychic",
            "773-ice",
            "773-dragon",
            "773-dark",
            "773-fairy",
        };

        // Alcremie: creams (sweet is a form argument, not a form -- strawberry used)
        static const char *const ALCREMIE[] = {
            "869-vanilla-cream-strawberry-sweet",
            "869-ruby-cream-strawberry-sweet",
            "869-matcha-cream-strawberry-sweet",
            "869-mint-cream-strawberry-sweet",
            "869-lemon-cream-strawberry-sweet",
            "869-salted-cream-strawberry-sweet",
            "869-ruby-swirl-strawberry-sweet",
            "869-caramel-swirl-strawberry-sweet",
            "869-rainbow-swirl-strawberry-sweet",
        };
        // Empty string = this form is numeric-keyed (or has no art); the caller falls back.
        inline const char *pick(const char *const *table, int count, uint8_t formId)
        {
            return formId < count ? table[formId] : "";
        }
    }

    const char *getFormSpriteName(uint16_t speciesId, uint8_t formId)
    {
        switch (speciesId)
        {
        case 201:
            return pick(UNOWN, 28, formId);
        case 412:
            return pick(BURMY, 3, formId);
        case 421:
            return pick(CHERRIM, 2, formId);
        case 422:
            return pick(SHELLOS, 2, formId);
        case 423:
            return pick(GASTRODON, 2, formId);
        case 493:
            return pick(ARCEUS, 18, formId);
        case 585:
            return pick(DEERLING, 4, formId);
        case 586:
            return pick(SAWSBUCK, 4, formId);
        case 649:
            return pick(GENESECT, 5, formId);
        case 666:
            return pick(VIVILLON, 20, formId);
        case 669:
            return pick(FLABEBE, 5, formId);
        case 670:
            return pick(FLOETTE, 5, formId);
        case 671:
            return pick(FLORGES, 5, formId);
        case 676:
            return pick(FURFROU, 10, formId);
        case 716:
            return pick(XERNEAS, 2, formId);
        case 773:
            return pick(SILVALLY, 18, formId);
        case 869:
            return pick(ALCREMIE, 9, formId);
        default:
            return "";
        }
    }

} // namespace Pokemon
