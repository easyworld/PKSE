/**
 * GENERATED from PKHeX's personal_xy -- do not hand-edit. Regenerate with
 * `python tools/gen_personaltables.py`.
 *
 * Fields this group's generation does not have read 0 -- gate them on the predicates in
 * PersonalRecord.h rather than reading them as data.
 *
 * AN ALL-ZERO ROW IS REAL DATA, NOT A GAP, and it does not mean the same thing in every
 * table. Sword/Shield and Legends: Arceus ZERO the ids their game does not have (664 of
 * 898, and 226 of 905) -- there, an empty row is the roster speaking. Let's Go and
 * Scarlet/Violet carry full stats for every id whether the game has it or not, so an
 * empty row there means nothing at all. DO NOT READ DEX PRESENCE OUT OF THIS TABLE:
 * PersonalInfoTable's `presence` bits are the cross-game answer and are still the only
 * one. What a caller must do here is not compute against a zero base stat.
 */
#ifndef POKEMON_PERSONALINFO6XY_H
#define POKEMON_PERSONALINFO6XY_H

#include "Pokemon/PersonalRecord.h"

namespace Pokemon
{
    /// Highest National Dex id this group has a row for.
    inline constexpr uint16_t PERSONAL_MAX_SPECIES_6XY = 721;

    /// Rows in the table: the form-0 entries, then the alternate-form entries a base
    /// row's formIndex redirects into.
    inline constexpr size_t PERSONAL_COUNT_6XY = 799;

    extern const PersonalRecord PERSONAL_6XY[PERSONAL_COUNT_6XY];

    /// Row for (species, form), mirroring PKHeX's FormStatsIndex redirection. An unknown
    /// species or a form this group does not define falls back to the species' form-0 row;
    /// a species this group does not have at all returns the empty record, whose zero base
    /// stats mean NO DATA and must not be computed against.
    const PersonalRecord &getPersonalInfo6XY(uint16_t species, uint8_t form) noexcept;
}

#endif  // POKEMON_PERSONALINFO6XY_H
