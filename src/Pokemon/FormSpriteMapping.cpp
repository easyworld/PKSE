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
    // Maushold is special in that its base form is 1 -- not 0.
    if (formId == 0 && speciesId != 925) {
        return speciesId;
    }

    // Map species+form to PokeAPI sprite ID
    switch (speciesId) {
        // Deoxys forms
        case 386:
            if (formId == 1) return 10001; // Attack
            else if (formId == 2) return 10002; // Defense
            else if (formId == 3) return 10003; // Speed
            break;

        // Wormadam forms
        case 413:
            if (formId == 1) return 10004; // Sandy Cloak
            else if (formId == 2) return 10005; // Trash Cloak
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
            else if (formId == 2) return 10009; // Wash
            else if (formId == 3) return 10010; // Frost
            else if (formId == 4) return 10011; // Fan
            else if (formId == 5) return 10012; // Mow
            break;

        // Basculin
        case 550:
            if (formId == 1) return 10016; // Blue-Striped
            else if (formId == 2) return 10247; // White-Striped (10246 is palkia-origin)
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
            if (formId == 1) return 10023; // White (PokeAPI orders black=10022, white=10023)
            else if (formId == 2) return 10022; // Black
            break;

        // Meloetta
        case 648:
            if (formId == 1) return 10018; // Pirouette
            break;

        // Keldeo
        case 647:
            if (formId == 1) return 10024; // Resolute
            break;

        // Meowstic
        case 678:
            if (formId == 1) return 10025; // Female
            else if (formId == 2) return 10314; // Male Mega
            else if (formId == 3) return 10326; // Female Mega
            break;

        // Pumpkaboo sizes
        case 710:
            if (formId == 1) return 10027; // Small
            else if (formId == 2) return 10028; // Large
            else if (formId == 3) return 10029; // Super
            break;

        // Gourgeist sizes
        case 711:
            if (formId == 1) return 10030; // Small
            else if (formId == 2) return 10031; // Large
            else if (formId == 3) return 10032; // Super
            break;

        // Zygarde forms
        case 718:
            // 10118 is zygarde-10-POWER-CONSTRUCT; plain 10% (Aura Break) is 10181.
            if (formId == 1) return 10181; // 10%
            else if (formId == 2) return 10118; // 10% Power Construct
            else if (formId == 3) return 10119; // 50% Power Construct
            else if (formId == 4) return 10120; // Complete
            else if (formId == 5) return 10301; // Mega
            break;

        // Hoopa
        case 720:
            if (formId == 1) return 10086; // Unbound
            break;

        // Oricorio
        case 741:
            if (formId == 1) return 10123; // Pom-Pom
            else if (formId == 2) return 10124; // Pa'u
            else if (formId == 3) return 10125; // Sensu
            break;

        // Lycanroc
        case 745:
            if (formId == 1) return 10126; // Midnight
            else if (formId == 2) return 10152; // Dusk
            break;

        // Gen 7 Totem forms. Raticate/Marowak carry theirs at form 2 (above) because an Alolan
        // form is in the way; Mimikyu's are 2/3 for the same reason.
        case 735: // Gumshoos
            if (formId == 1) return 10121;
            break;
        case 738: // Vikavolt
            if (formId == 1) return 10122;
            break;
        case 743: // Ribombee
            if (formId == 1) return 10150;
            break;
        case 752: // Araquanid
            if (formId == 1) return 10153;
            break;
        case 754: // Lurantis
            if (formId == 1) return 10128;
            break;
        case 758: // Salazzle
            if (formId == 1) return 10129;
            break;
        case 777: // Togedemaru
            if (formId == 1) return 10154;
            break;
        case 784: // Kommo-o
            if (formId == 1) return 10146;
            break;

        // Rockruff -- the Own Tempo (Dusk-evolving) variant
        case 744:
            if (formId == 1) return 10151;
            break;

        // Zarude -- Dada
        case 893:
            if (formId == 1) return 10192;
            break;

        // Minior -- Meteor shells 0-6 then Cores 7-13. Form 0 (Meteor Red) is PokeAPI's
        // default variety (774), so it takes the form-0 short-circuit above.
        case 774:
            if (formId == 1) return 10130;       // Meteor Orange
            else if (formId == 2) return 10131;  // Meteor Yellow
            else if (formId == 3) return 10132;  // Meteor Green
            else if (formId == 4) return 10133;  // Meteor Blue
            else if (formId == 5) return 10134;  // Meteor Indigo
            else if (formId == 6) return 10135;  // Meteor Violet
            else if (formId == 7) return 10136;  // Core Red
            else if (formId == 8) return 10137;  // Core Orange
            else if (formId == 9) return 10138;  // Core Yellow
            else if (formId == 10) return 10139; // Core Green
            else if (formId == 11) return 10140; // Core Blue
            else if (formId == 12) return 10141; // Core Indigo
            else if (formId == 13) return 10142; // Core Violet
            break;

        // Mimikyu -- 2/3 are the Gen 7 Totem entries
        case 778:
            if (formId == 1) return 10143;       // Busted
            else if (formId == 2) return 10144;  // Totem Disguised
            else if (formId == 3) return 10145;  // Totem Busted
            break;

        // Battle-only forms. Hidden from the picker by default (isBattleOnlyForm), but a save can
        // hold one and "Allow illegal edits" reveals them, so they still need art.
        // Koraidon/Miraidon's ride builds are absent on purpose: PokeAPI indexes the ids
        // (10264-10271) but publishes no HOME render for any of them.
        case 351: // Castform
            if (formId == 1) return 10013;       // Sunny
            else if (formId == 2) return 10014;  // Rainy
            else if (formId == 3) return 10015;  // Snowy
            break;
        case 681: // Aegislash
            if (formId == 1) return 10026;       // Blade
            break;
        case 746: // Wishiwashi
            if (formId == 1) return 10127;       // School
            break;
        case 845: // Cramorant
            if (formId == 1) return 10182;       // Gulping
            else if (formId == 2) return 10183;  // Gorging
            break;
        case 875: // Eiscue
            if (formId == 1) return 10185;       // Noice Face
            break;
        case 877: // Morpeko
            if (formId == 1) return 10187;       // Hangry
            break;
        case 890: // Eternatus
            if (formId == 1) return 10190;       // Eternamax
            break;

        // Necrozma
        case 800:
            if (formId == 1) return 10155; // Dusk Mane
            else if (formId == 2) return 10156; // Dawn Wings
            else if (formId == 3) return 10157; // Ultra
            break;

        // Toxtricity
        case 849:
            if (formId == 1) return 10184; // Low Key
            break;

        // Indeedee
        case 876:
            if (formId == 1) return 10186; // Female
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
            else if (formId == 2) return 10194; // Shadow Rider
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
        // The forms have been switched here for some reason — form 0 is the rare variant while form 1 is the base variant.
        case 925:
            if (formId == 0) return 10257; // Family of Three
            else if (formId == 1) return 925; // Family of Four
            break;

        // Squawkabilly
        case 931:
            if (formId == 1) return 10260; // Blue Plumage
            else if (formId == 2) return 10261; // Yellow Plumage
            else if (formId == 3) return 10262; // White Plumage
            break;

        // Tatsugiri
        case 978:
            if (formId == 1) return 10258; // Droopy (10268/10269 are Miraidon ride modes)
            else if (formId == 2) return 10259; // Stretchy
            else if (formId == 3) return 10322; // Curly Mega
            else if (formId == 4) return 10323; // Droopy Mega
            else if (formId == 5) return 10324; // Stretchy Mega
            break;

        // Dudunsparce
        case 982:
            if (formId == 1) return 10255; // Three-Segment
            break;

        // Gimmighoul
        case 999:
            if (formId == 1) return 10263; // Roaming
            break;

        // For Poltchageist and Sinistcha there are no sprites for their Artisan/Masterpiece forms — use the base sprites for now
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
            if (formId == 1) return 10273; // Wellspring Mask
            else if (formId == 2) return 10274; // Hearthflame Mask
            else if (formId == 3) return 10275; // Cornerstone Mask
            break;

        // Palafin
        case 964:
            if (formId == 1) return 10256; // Hero
            break;

        // Terapagos
        case 1024:
            if (formId == 1) return 10276; // Terastal
            else if (formId == 2) return 10277; // Stellar
            break;

        // === Alolan Forms ===
        case 19:  // Rattata
            if (formId == 1) return 10091;
            break;
        case 20:  // Raticate
            if (formId == 1) return 10092;
            else if (formId == 2) return 10093;  // Totem (the Alolan one)
            break;

        // Pikachu -- event caps. PokeAPI has no HOME render for the Let's Go partner (form 8),
        // and the caps have no shiny render, so both fall back to the base species.
        case 25:  // Pikachu
            if (formId == 1) return 10094;       // Original Cap
            else if (formId == 2) return 10095;  // Hoenn Cap
            else if (formId == 3) return 10096;  // Sinnoh Cap
            else if (formId == 4) return 10097;  // Unova Cap
            else if (formId == 5) return 10098;  // Kalos Cap
            else if (formId == 6) return 10099;  // Alola Cap
            else if (formId == 7) return 10148;  // Partner Cap
            else if (formId == 9) return 10160;  // World Cap
            break;
        case 26:  // Raichu
            if (formId == 1) return 10100;
            else if (formId == 2) return 10304; // Mega X
            else if (formId == 3) return 10305; // Mega Y
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
            else if (formId == 2) return 10161; // Galarian
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
            else if (formId == 2) return 10149;  // Totem (the Alolan one)
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
        case 80:  // Slowbro -- Mega kept form 1 from Gen 6, so Galar was appended at 2
            if (formId == 1) return 10071;       // Mega
            else if (formId == 2) return 10165;  // Galarian
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
        case 555: // Darmanitan -- Zen holds form 1, so Galarian sits at 2 (not the usual shape)
            if (formId == 1) return 10017;       // Zen
            else if (formId == 2) return 10177;  // Galarian Standard
            else if (formId == 3) return 10178;  // Galarian Zen
            break;
        case 562: // Yamask
            if (formId == 1) return 10179;   // 10178 is darmanitan-galar-zen
            break;
        case 618: // Stunfisk
            if (formId == 1) return 10180;   // 10179 is yamask-galar
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
            else if (formId == 2) return 10251; // Blaze Breed
            else if (formId == 3) return 10252; // Aqua Breed
            break;

        // === Mega Evolutions / Primal Reversions ===
        // Mega is a storable form again in Legends: Z-A, so these are pickable. Form indices
        // follow PKHeX's FormConverter: a single-mega species puts Mega at form 1; Charizard,
        // Mewtwo and Raichu use Mega X/Y; Absol, Garchomp and Lucario use Mega + Mega Z;
        // Slowbro's Mega sits after its Galarian form. Every id below was resolved by NAME
        // against PokeAPI -- do not hand-edit them.

        case 3: // Venusaur
            if (formId == 1) return 10033; // Mega
            break;

        case 6: // Charizard
            if (formId == 1) return 10034; // Mega X
            else if (formId == 2) return 10035; // Mega Y
            break;

        case 9: // Blastoise
            if (formId == 1) return 10036; // Mega
            break;

        case 15: // Beedrill
            if (formId == 1) return 10090; // Mega
            break;

        case 18: // Pidgeot
            if (formId == 1) return 10073; // Mega
            break;

        case 36: // Clefable
            if (formId == 1) return 10278; // Mega
            break;

        case 65: // Alakazam
            if (formId == 1) return 10037; // Mega
            break;

        case 71: // Victreebel
            if (formId == 1) return 10279; // Mega
            break;

        case 94: // Gengar
            if (formId == 1) return 10038; // Mega
            break;

        case 115: // Kangaskhan
            if (formId == 1) return 10039; // Mega
            break;

        case 121: // Starmie
            if (formId == 1) return 10280; // Mega
            break;

        case 127: // Pinsir
            if (formId == 1) return 10040; // Mega
            break;

        case 130: // Gyarados
            if (formId == 1) return 10041; // Mega
            break;

        case 142: // Aerodactyl
            if (formId == 1) return 10042; // Mega
            break;

        case 149: // Dragonite
            if (formId == 1) return 10281; // Mega
            break;

        case 150: // Mewtwo
            if (formId == 1) return 10043; // Mega X
            else if (formId == 2) return 10044; // Mega Y
            break;

        case 154: // Meganium
            if (formId == 1) return 10282; // Mega
            break;

        case 160: // Feraligatr
            if (formId == 1) return 10283; // Mega
            break;

        case 181: // Ampharos
            if (formId == 1) return 10045; // Mega
            break;

        case 208: // Steelix
            if (formId == 1) return 10072; // Mega
            break;

        case 212: // Scizor
            if (formId == 1) return 10046; // Mega
            break;

        case 214: // Heracross
            if (formId == 1) return 10047; // Mega
            break;

        case 227: // Skarmory
            if (formId == 1) return 10284; // Mega
            break;

        case 229: // Houndoom
            if (formId == 1) return 10048; // Mega
            break;

        case 248: // Tyranitar
            if (formId == 1) return 10049; // Mega
            break;

        case 254: // Sceptile
            if (formId == 1) return 10065; // Mega
            break;

        case 257: // Blaziken
            if (formId == 1) return 10050; // Mega
            break;

        case 260: // Swampert
            if (formId == 1) return 10064; // Mega
            break;

        case 282: // Gardevoir
            if (formId == 1) return 10051; // Mega
            break;

        case 302: // Sableye
            if (formId == 1) return 10066; // Mega
            break;

        case 303: // Mawile
            if (formId == 1) return 10052; // Mega
            break;

        case 306: // Aggron
            if (formId == 1) return 10053; // Mega
            break;

        case 308: // Medicham
            if (formId == 1) return 10054; // Mega
            break;

        case 310: // Manectric
            if (formId == 1) return 10055; // Mega
            break;

        case 319: // Sharpedo
            if (formId == 1) return 10070; // Mega
            break;

        case 323: // Camerupt
            if (formId == 1) return 10087; // Mega
            break;

        case 334: // Altaria
            if (formId == 1) return 10067; // Mega
            break;

        case 354: // Banette
            if (formId == 1) return 10056; // Mega
            break;

        case 358: // Chimecho
            if (formId == 1) return 10306; // Mega
            break;

        case 359: // Absol
            if (formId == 1) return 10057; // Mega
            else if (formId == 2) return 10307; // Mega Z
            break;

        case 362: // Glalie
            if (formId == 1) return 10074; // Mega
            break;

        case 373: // Salamence
            if (formId == 1) return 10089; // Mega
            break;

        case 376: // Metagross
            if (formId == 1) return 10076; // Mega
            break;

        case 380: // Latias
            if (formId == 1) return 10062; // Mega
            break;

        case 381: // Latios
            if (formId == 1) return 10063; // Mega
            break;

        case 382: // Kyogre
            if (formId == 1) return 10077; // Primal
            break;

        case 383: // Groudon
            if (formId == 1) return 10078; // Primal
            break;

        case 384: // Rayquaza
            if (formId == 1) return 10079; // Mega
            break;

        case 398: // Staraptor
            if (formId == 1) return 10308; // Mega
            break;

        case 428: // Lopunny
            if (formId == 1) return 10088; // Mega
            break;

        case 445: // Garchomp
            if (formId == 1) return 10058; // Mega
            else if (formId == 2) return 10309; // Mega Z
            break;

        case 448: // Lucario
            if (formId == 1) return 10059; // Mega
            else if (formId == 2) return 10310; // Mega Z
            break;

        case 460: // Abomasnow
            if (formId == 1) return 10060; // Mega
            break;

        case 475: // Gallade
            if (formId == 1) return 10068; // Mega
            break;

        case 478: // Froslass
            if (formId == 1) return 10285; // Mega
            break;

        case 483: // Dialga
            if (formId == 1) return 10245; // Origin
            break;

        case 484: // Palkia
            if (formId == 1) return 10246; // Origin
            break;

        case 485: // Heatran
            if (formId == 1) return 10311; // Mega
            break;

        case 491: // Darkrai
            if (formId == 1) return 10312; // Mega
            break;

        case 500: // Emboar
            if (formId == 1) return 10286; // Mega
            break;

        case 530: // Excadrill
            if (formId == 1) return 10287; // Mega
            break;

        case 531: // Audino
            if (formId == 1) return 10069; // Mega
            break;

        case 545: // Scolipede
            if (formId == 1) return 10288; // Mega
            break;

        case 560: // Scrafty
            if (formId == 1) return 10289; // Mega
            break;

        case 604: // Eelektross
            if (formId == 1) return 10290; // Mega
            break;

        case 609: // Chandelure
            if (formId == 1) return 10291; // Mega
            break;

        case 623: // Golurk
            if (formId == 1) return 10313; // Mega
            break;

        case 652: // Chesnaught
            if (formId == 1) return 10292; // Mega
            break;

        case 655: // Delphox
            if (formId == 1) return 10293; // Mega
            break;

        case 658: // Greninja
            if (formId == 1) return 10116; // Battle Bond
            else if (formId == 2) return 10117; // Ash
            else if (formId == 3) return 10294; // Mega
            break;

        case 668: // Pyroar
            if (formId == 1) return 10295; // Mega
            break;

        case 670: // Floette
            if (formId == 5) return 10061; // Eternal
            else if (formId == 6) return 10296; // Mega
            break;

        case 687: // Malamar
            if (formId == 1) return 10297; // Mega
            break;

        case 689: // Barbaracle
            if (formId == 1) return 10298; // Mega
            break;

        case 691: // Dragalge
            if (formId == 1) return 10299; // Mega
            break;

        case 701: // Hawlucha
            if (formId == 1) return 10300; // Mega
            break;

        case 719: // Diancie
            if (formId == 1) return 10075; // Mega
            break;

        case 740: // Crabominable
            if (formId == 1) return 10315; // Mega
            break;

        case 768: // Golisopod
            if (formId == 1) return 10316; // Mega
            break;

        case 780: // Drampa
            if (formId == 1) return 10302; // Mega
            break;

        case 801: // Magearna
            if (formId == 1) return 10147; // Original Color
            else if (formId == 2) return 10317; // Mega
            else if (formId == 3) return 10318; // Mega Original Color
            break;

        case 807: // Zeraora
            if (formId == 1) return 10319; // Mega
            break;

        case 870: // Falinks
            if (formId == 1) return 10303; // Mega
            break;

        case 952: // Scovillain
            if (formId == 1) return 10320; // Mega
            break;

        case 970: // Glimmora
            if (formId == 1) return 10321; // Mega
            break;

        case 998: // Baxcalibur
            if (formId == 1) return 10325; // Mega
            break;
    }

    // No form sprite found, use base species sprite
    return speciesId;
}


// ---------------------------------------------------------------------------------------------
// Name-keyed sprites.
//
// PokeAPI keys most HOME renders by a numeric id, but for the families whose forms are a set of
// peers rather than a base plus variants -- Unown's letters, Arceus/Silvally's types, Vivillon's
// patterns, Alcremie's creams, Furfrou's trims, flower colours, seasons, seas -- it keys them by
// NAME instead ("666-meadow"). Those renders have no numeric id, so getFormSpriteId() literally
// cannot address them and every one of these species used to fall back to base-species art.
//
// Tables below are generated and validated against romfs: every stem named here has both a normal
// and a shiny file on disk. Order is PKHeX FormConverter's, NOT PokeAPI's alphabetical listing.
// ---------------------------------------------------------------------------------------------
namespace {
    // Unown: A-Z, then ! and ?
    static const char* const UNOWN[] = {
        "201-a", "201-b", "201-c", "201-d", "201-e", "201-f", "201-g", "201-h", "201-i", "201-j", "201-k",
        "201-l", "201-m", "201-n", "201-o", "201-p", "201-q", "201-r", "201-s", "201-t", "201-u", "201-v",
        "201-w", "201-x", "201-y", "201-z", "201-exclamation", "201-question",
    };

    // Burmy: cloaks
    static const char* const BURMY[] = {
        "412-plant", "412-sandy", "412-trash",
    };

    // Cherrim: weather
    static const char* const CHERRIM[] = {
        "421-overcast", "421-sunshine",
    };

    // Shellos: sea
    static const char* const SHELLOS[] = {
        "422-west", "422-east",
    };

    // Gastrodon: sea
    static const char* const GASTRODON[] = {
        "423-west", "423-east",
    };

    // Arceus: plate types (form 0 = plain 493; form 18 = Legend, no art)
    static const char* const ARCEUS[] = {
        "", "493-fighting", "493-flying", "493-poison", "493-ground", "493-rock", "493-bug", "493-ghost",
        "493-steel", "493-fire", "493-water", "493-grass", "493-electric", "493-psychic", "493-ice",
        "493-dragon", "493-dark", "493-fairy",
    };

    // Deerling: seasons
    static const char* const DEERLING[] = {
        "585-spring", "585-summer", "585-autumn", "585-winter",
    };

    // Sawsbuck: seasons
    static const char* const SAWSBUCK[] = {
        "586-spring", "586-summer", "586-autumn", "586-winter",
    };

    // Genesect: drives (form 0 has no -suffix file)
    static const char* const GENESECT[] = {
        "", "649-douse", "649-shock", "649-burn", "649-chill",
    };

    // Vivillon: patterns
    static const char* const VIVILLON[] = {
        "666-icy-snow", "666-polar", "666-tundra", "666-continental", "666-garden", "666-elegant",
        "666-meadow", "666-modern", "666-marine", "666-archipelago", "666-high-plains", "666-sandstorm",
        "666-river", "666-monsoon", "666-savanna", "666-sun", "666-ocean", "666-jungle", "666-fancy",
        "666-poke-ball",
    };

    // Flabebe: flower colours
    static const char* const FLABEBE[] = {
        "669-red", "669-yellow", "669-orange", "669-blue", "669-white",
    };

    // Floette: flower colours (5 Eternal / 6 Mega are numeric ids)
    static const char* const FLOETTE[] = {
        "670-red", "670-yellow", "670-orange", "670-blue", "670-white",
    };

    // Florges: flower colours
    static const char* const FLORGES[] = {
        "671-red", "671-yellow", "671-orange", "671-blue", "671-white",
    };

    // Furfrou: trims
    static const char* const FURFROU[] = {
        "676-natural", "676-heart", "676-star", "676-diamond", "676-debutante", "676-matron", "676-dandy",
        "676-la-reine", "676-kabuki", "676-pharaoh",
    };

    // Xerneas: mode
    static const char* const XERNEAS[] = {
        "716-neutral", "716-active",
    };

    // Silvally: memory types
    static const char* const SILVALLY[] = {
        "773-normal", "773-fighting", "773-flying", "773-poison", "773-ground", "773-rock", "773-bug",
        "773-ghost", "773-steel", "773-fire", "773-water", "773-grass", "773-electric", "773-psychic",
        "773-ice", "773-dragon", "773-dark", "773-fairy",
    };

    // Alcremie: creams (sweet is a form argument, not a form -- strawberry used)
    static const char* const ALCREMIE[] = {
        "869-vanilla-cream-strawberry-sweet", "869-ruby-cream-strawberry-sweet",
        "869-matcha-cream-strawberry-sweet", "869-mint-cream-strawberry-sweet",
        "869-lemon-cream-strawberry-sweet", "869-salted-cream-strawberry-sweet",
        "869-ruby-swirl-strawberry-sweet", "869-caramel-swirl-strawberry-sweet",
        "869-rainbow-swirl-strawberry-sweet",
    };
    // Empty string = this form is numeric-keyed (or has no art); the caller falls back.
    inline const char* pick(const char* const* table, int count, uint8_t formId) {
        return formId < count ? table[formId] : "";
    }
}

const char* getFormSpriteName(uint16_t speciesId, uint8_t formId) {
    switch (speciesId) {
        case 201: return pick(UNOWN, 28, formId);
        case 412: return pick(BURMY, 3, formId);
        case 421: return pick(CHERRIM, 2, formId);
        case 422: return pick(SHELLOS, 2, formId);
        case 423: return pick(GASTRODON, 2, formId);
        case 493: return pick(ARCEUS, 18, formId);
        case 585: return pick(DEERLING, 4, formId);
        case 586: return pick(SAWSBUCK, 4, formId);
        case 649: return pick(GENESECT, 5, formId);
        case 666: return pick(VIVILLON, 20, formId);
        case 669: return pick(FLABEBE, 5, formId);
        case 670: return pick(FLOETTE, 5, formId);
        case 671: return pick(FLORGES, 5, formId);
        case 676: return pick(FURFROU, 10, formId);
        case 716: return pick(XERNEAS, 2, formId);
        case 773: return pick(SILVALLY, 18, formId);
        case 869: return pick(ALCREMIE, 9, formId);
        default: return "";
    }
}

} // namespace Pokemon
