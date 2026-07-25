#include <cstdint>
#include <cstddef>
#include "Pokemon/BaseStatsGen89.h"

// Forward declarations for Names namespace functions
namespace Names {
    extern const char* getSpeciesName(uint16_t speciesId);
    extern const char* getItemName(uint16_t itemId);
    extern const char* getNatureName(uint8_t natureId);
    // uint16_t, matching the real definition in Names/AbilityNames.cpp. This used to say uint8_t,
    // which declared a DIFFERENT overload that does not exist anywhere -- so getAbilityNameGen89()
    // below referenced a symbol that could never resolve. It went unnoticed because nothing calls
    // it and --gc-sections drops the function before the linker ever has to find the target.
    extern const char* getAbilityName(uint16_t abilityId);
}

namespace Pokemon {
    /**
     * Helper function to search for a species in a form-specific array.
     * Returns nullptr if not found.
     */
    static const BaseStatsGen89* searchFormArray(
        const BaseStatsGen89* array,
        size_t arraySize,
        uint16_t speciesId)
    {
        for (size_t i = 0; i < arraySize; i++) {
            if (array[i].id == speciesId) {
                return &array[i];
            }
        }
        return nullptr;
    }

    const BaseStatsGen89* getBaseStatsGen89(uint16_t speciesId, uint8_t form) {
        static const BaseStatsGen89 empty = {0, 0, 0, 0, 0, 0, 0};

        // Form-specific lookups
        // Regional variants typically use form 1 (some Pokemon have both Alolan and Galarian, using forms 1 and 2)

        // Check Alolan forms (form 1 for most, check Meowth/Persian)
        if (form == 1) {
            switch (speciesId) {
                // Alolan-only Pokemon (form 1 = Alolan)
                case 19: case 20: case 26: case 27: case 28: case 37: case 38:
                case 50: case 51: case 52: case 53: case 74: case 75: case 76:
                case 88: case 89: case 103: case 105: {
                    const BaseStatsGen89* stats = searchFormArray(
                        BASE_STATS_TABLE_ALOLAN_GEN89,
                        sizeof(BASE_STATS_TABLE_ALOLAN_GEN89) / sizeof(BASE_STATS_TABLE_ALOLAN_GEN89[0]),
                        speciesId
                    );
                    if (stats) return stats;
                    break;
                }
                // Galarian-only Pokemon (form 1 = Galarian)
                case 77: case 78: case 79: case 80: case 83: case 110: case 122:
                case 144: case 145: case 146: case 199: case 222: case 263: case 264:
                case 554: case 555: case 562: case 618: {
                    const BaseStatsGen89* stats = searchFormArray(
                        BASE_STATS_TABLE_GALARIAN_GEN89,
                        sizeof(BASE_STATS_TABLE_GALARIAN_GEN89) / sizeof(BASE_STATS_TABLE_GALARIAN_GEN89[0]),
                        speciesId
                    );
                    if (stats) return stats;
                    break;
                }
                // Hisuian Pokemon (form 1 = Hisuian)
                case 58: case 59: case 100: case 101: case 157: case 211: case 215:
                case 503: case 549: case 570: case 571: case 705: case 706: case 713: case 724: {
                    const BaseStatsGen89* stats = searchFormArray(
                        BASE_STATS_TABLE_HISUIAN_GEN89,
                        sizeof(BASE_STATS_TABLE_HISUIAN_GEN89) / sizeof(BASE_STATS_TABLE_HISUIAN_GEN89[0]),
                        speciesId
                    );
                    if (stats) return stats;
                    break;
                }
                // Paldean Pokemon
                case 194: { // Wooper
                    const BaseStatsGen89* stats = searchFormArray(
                        BASE_STATS_TABLE_PALDEAN_GEN89,
                        sizeof(BASE_STATS_TABLE_PALDEAN_GEN89) / sizeof(BASE_STATS_TABLE_PALDEAN_GEN89[0]),
                        speciesId
                    );
                    if (stats) return stats;
                    break;
                }
                // Tauros Paldean breeds
                case 128: { // Tauros form 1 = Combat Breed
                    const BaseStatsGen89* stats = searchFormArray(
                        BASE_STATS_TABLE_TAUROS_FORMS_GEN89,
                        sizeof(BASE_STATS_TABLE_TAUROS_FORMS_GEN89) / sizeof(BASE_STATS_TABLE_TAUROS_FORMS_GEN89[0]),
                        speciesId
                    );
                    if (stats) return &stats[0]; // Combat Breed is first
                    break;
                }
            }
        }

        // Form 2 for Pokemon with multiple regional variants or specific forms
        if (form == 2) {
            // Meowth/Persian: form 2 = Galarian
            if (speciesId == 52 || speciesId == 53) {
                const BaseStatsGen89* stats = searchFormArray(
                    BASE_STATS_TABLE_GALARIAN_GEN89,
                    sizeof(BASE_STATS_TABLE_GALARIAN_GEN89) / sizeof(BASE_STATS_TABLE_GALARIAN_GEN89[0]),
                    speciesId
                );
                if (stats) return stats;
            }
        }

        // Pokemon-specific form lookups
        // Handle Pokemon with unique form mechanics (Deoxys, Rotom, Giratina, etc.)

        switch (speciesId) {
            case 128:
                return &BASE_STATS_TABLE_TAUROS_FORMS_GEN89[form];
                break;
            // Deoxys - Forms: 0=Normal, 1=Attack, 2=Defense, 3=Speed
            case 386:
                return &BASE_STATS_TABLE_DEOXYS_FORMS_GEN89[form];
                break;

            // Burmy - Forms: 0=Plant, 1=Sandy, 2=Trash
            case 412:
                return &BASE_STATS_TABLE_BURMY_WORMADAM_FORMS_GEN89[form];
                break;

            // Wormadam - Forms: 0=Plant, 1=Sandy, 2=Trash
            case 413:
                return &BASE_STATS_TABLE_BURMY_WORMADAM_FORMS_GEN89[3 + form]; // Offset by 3 (Burmy entries)
                break;

            // Rotom - Forms: 0=Base, 1=Heat, 2=Wash, 3=Frost, 4=Fan, 5=Mow
            case 479:
                if (form > 0) {
                    return &BASE_STATS_TABLE_ROTOM_FORMS_GEN89[form - 1]; // Array starts at Heat
                }
                break;

            // Dialga - Forms: 0=Base, 1=Origin
            case 483:
                if (form == 1) {
                    return &BASE_STATS_TABLE_DIALGA_PALKIA_GIRATINA_FORMS_GEN89[0]; // Origin Forme
                }
                break;

            // Palkia - Forms: 0=Base, 1=Origin
            case 484:
                if (form == 1) {
                    return &BASE_STATS_TABLE_DIALGA_PALKIA_GIRATINA_FORMS_GEN89[1]; // Origin Forme
                }
                break;

            // Giratina - Forms: 0=Altered, 1=Origin
            case 487:
                if (form == 0) {
                    return &BASE_STATS_TABLE_DIALGA_PALKIA_GIRATINA_FORMS_GEN89[2]; // Altered
                } else if (form == 1) {
                    return &BASE_STATS_TABLE_DIALGA_PALKIA_GIRATINA_FORMS_GEN89[3]; // Origin
                }
                break;

            // Shaymin - Forms: 0=Land, 1=Sky
            case 492:
                return &BASE_STATS_TABLE_SHAYMIN_FORMS_GEN89[form];
                break;

            // Basculin - Forms: 0=Red-Striped, 1=Blue-Striped, 2=White-Striped
            case 550:
                return &BASE_STATS_TABLE_BASCULIN_FORMS_GEN89[form];
                break;

            // Tornadus - Forms: 0=Incarnate, 1=Therian
            case 641:
                return &BASE_STATS_TABLE_TORNADUS_THUNDURUS_LANDORUS_FORMS_GEN89[form];
                break;

            // Thundurus - Forms: 0=Incarnate, 1=Therian
            case 642:
                if (form == 1) {
                    return &BASE_STATS_TABLE_TORNADUS_THUNDURUS_LANDORUS_FORMS_GEN89[1];
                }
                break;

            // Landorus - Forms: 0=Incarnate, 1=Therian
            case 645:
                if (form == 1) {
                    return &BASE_STATS_TABLE_TORNADUS_THUNDURUS_LANDORUS_FORMS_GEN89[2];
                }
                break;

            // Kyurem - Forms: 0=Base, 1=White, 2=Black
            case 646:
                if (form > 0) {
                    return &BASE_STATS_TABLE_KYUREM_FORMS_GEN89[form - 1];
                }
                break;

            // Keldeo - Forms: 0=Ordinary, 1=Resolute
            case 647:
                return &BASE_STATS_TABLE_KELDEO_FORMS_GEN89[form];
                break;

            // Meloetta - Forms: 0=Aria, 1=Pirouette
            case 648:
                return &BASE_STATS_TABLE_MELOETTA_FORMS_GEN89[form];
                break;

            // Meowstic - Forms: 0=Male, 1=Female
            case 678:
                return &BASE_STATS_TABLE_MEOWSTIC_FORMS_GEN89[form];
                break;

            // Pumpkaboo - Forms: 0=Average, 1=Small, 2=Large, 3=Super
            case 710:
                return &BASE_STATS_TABLE_PUMPKABOO_GOURGEIST_FORMS_GEN89[form];
                break;

            // Gourgeist - Forms: 0=Average, 1=Small, 2=Large, 3=Super
            case 711:
                // TODO: Gourgeist form 0 is not working for some reason...
                return &BASE_STATS_TABLE_PUMPKABOO_GOURGEIST_FORMS_GEN89[4 + form]; // Offset by 4 (Pumpkaboo entries)
                break;

            // Zygarde - Forms: 0=50%, 1=10%, 4=Complete (note: form 4!)
            case 718:
                if (form == 0 || form == 1) {
                    return &BASE_STATS_TABLE_ZYGARDE_FORMS_GEN89[form]; // 50% and 10%
                } else if (form == 4) {
                    return &BASE_STATS_TABLE_ZYGARDE_FORMS_GEN89[2]; // Complete
                }
                break;

            // Hoopa - Forms: 0=Confined, 1=Unbound
            case 720:
                return &BASE_STATS_TABLE_HOOPA_FORMS_GEN89[form];
                break;

            // Oricorio - Forms: 0=Baile, 1=Pom-Pom, 2=Pa'u, 3=Sensu
            case 741:
                return &BASE_STATS_TABLE_ORICORIO_FORMS_GEN89[form];
                break;

            // Lycanroc - Forms: 0=Midday, 1=Midnight, 2=Dusk
            case 745:
                return &BASE_STATS_TABLE_LYCANROC_FORMS_GEN89[form];
                break;

            // Necrozma - Forms: 0=Base, 1=Dusk Mane, 2=Dawn Wings, 3=Ultra
            case 800:
                if (form > 0) {
                    return &BASE_STATS_TABLE_NECROZMA_FORMS_GEN89[form - 1];
                }
                break;

            // Toxtricity - Forms: 0=Amped, 1=Low Key
            case 849:
                return &BASE_STATS_TABLE_TOXTRICITY_FORMS_GEN89[form];
                break;

            // Indeedee - Forms: 0=Male, 1=Female
            case 876:
                return &BASE_STATS_TABLE_INDEEDEE_FORMS_GEN89[form];
                break;

            // Zacian - Forms: 0=Hero, 1=Crowned
            case 888:
                return &BASE_STATS_TABLE_ZACIAN_ZAMAZENTA_FORMS_GEN89[form];
                break;

            // Zamazenta - Forms: 0=Hero, 1=Crowned
            case 889:
                return &BASE_STATS_TABLE_ZACIAN_ZAMAZENTA_FORMS_GEN89[2 + form]; // Offset by 2 (Zacian entries)
                break;

            // Urshifu - Forms: 0=Single Strike, 1=Rapid Strike
            case 892:
                return &BASE_STATS_TABLE_URSHIFU_FORMS_GEN89[form];
                break;

            // Calyrex - Forms: 0=Base, 1=Ice Rider, 2=Shadow Rider
            case 898:
                if (form > 0) {
                    return &BASE_STATS_TABLE_CALYREX_FORMS_GEN89[form - 1];
                }
                break;

            // Ursaluna - Forms: 0=Base, 1=Bloodmoon
            case 901:
                return &BASE_STATS_TABLE_URSALUNA_FORMS_GEN89[form];
                break;

            // Basculegion - Forms: 0=Male, 1=Female
            case 902:
                return &BASE_STATS_TABLE_BASCULEGION_FORMS_GEN89[form];
                break;

            // Enamorus - Forms: 0=Incarnate, 1=Therian
            case 905:
                return &BASE_STATS_TABLE_ENAMORUS_FORMS_GEN89[form];
                break;

            // Oinkologne - Forms: 0=Male, 1=Female
            case 916:
                return &BASE_STATS_TABLE_OINKOLOGNE_FORMS_GEN89[form];
                break;

            // Maushold - Forms: 0=Family of Four, 1=Family of Three
            case 925:
                return &BASE_STATS_TABLE_MAUSHOLD_FORMS_GEN89[form];
                break;

            // Squawkabilly - Forms: 0=Green, 1=Blue, 2=Yellow, 3=White
            case 931:
                return &BASE_STATS_TABLE_SQUAWKABILLY_FORMS_GEN89[form];
                break;

            // Tatsugiri - Forms: 0=Curly, 1=Droopy, 2=Stretchy
            case 978:
                return &BASE_STATS_TABLE_TATSUGIRI_FORMS_GEN89[form];
                break;

            // Dudunsparce - Forms: 0=Two-Segment, 1=Three-Segment
            case 982:
                return &BASE_STATS_TABLE_DUDUNSPARCE_FORMS_GEN89[form];
                break;

            // Gimmighoul - Forms: 0=Chest, 1=Roaming
            case 999:
                return &BASE_STATS_TABLE_GIMMIGHOUL_FORMS_GEN89[form];
                break;

            // Terapagos - Forms: 0=Normal, 1=Terastal, 2=Stellar
            case 1024:
                if (form > 0) {
                    return &BASE_STATS_TABLE_TERAPAGOS_FORMS_GEN89[form];
                    break;
                }
                break;
        }

        // If no form-specific entry found, fall back to base form
        if (speciesId >= BASE_STATS_COUNT_GEN89) {
            return &empty;
        }
        return &BASE_STATS_TABLE_GEN89[speciesId];
    }

    // Wrapper functions that forward to Names namespace
    const char* getSpeciesNameGen89(uint16_t speciesId) {
        return Names::getSpeciesName(speciesId);
    }

    const char* getItemNameGen89(uint16_t itemId) {
        return Names::getItemName(itemId);
    }

    const char* getNatureNameGen89(uint8_t natureId) {
        return Names::getNatureName(natureId);
    }

    const char* getAbilityNameGen89(uint8_t abilityId) {
        return Names::getAbilityName(abilityId);
    }
}
