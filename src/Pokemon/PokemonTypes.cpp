/**
 * PokemonTypes.cpp - Pokemon Type Data Lookup Implementation
 *
 * A lookup, not a table. The type pair lives in the generated personal table alongside
 * the abilities and gender ratio, per (species, form), because that is the only place it
 * can be kept correct: PersonalInfoTable is regenerated from PKHeX, and its form
 * redirection is the same one every other per-form fact already goes through.
 *
 * This file used to hold ~1000 hand-written rows scraped from a web dex, plus a switch
 * listing the forms someone had got around to. That shape cannot be right: the rows had
 * no form dimension at all, so the switch had to re-state every retyping form by hand,
 * and the ones it missed -- Hisuian Braviary among them -- quietly returned the base
 * species' types. 149 of the 465 alternate forms retype their base species.
 */

#include "Pokemon/PokemonTypes.h"

#include "Pokemon/PersonalInfoTable.h"

namespace Pokemon {

TypePair getPokemonTypes(uint16_t speciesId, uint8_t formId, Enums::GameVersion group) {
    // Gen 3 reads its own table for the same reason its abilities do -- it predates the
    // Fairy type, so a FireRed Clefairy is Normal and a FireRed Gardevoir is pure Psychic.
    // Forms share one row there, which is correct: Gen 3's only alternate forms are the
    // Unown letters and Deoxys, and neither retypes.
    if (group == Enums::GameVersion::FRLG) {
        const PersonalInfoG3& g3 = getPersonalInfoG3(speciesId);
        return { g3.type1, g3.type2 };
    }
    const PersonalInfo& pi = getPersonalInfo(speciesId, formId);
    return { pi.type1, pi.type2 };
}

}
