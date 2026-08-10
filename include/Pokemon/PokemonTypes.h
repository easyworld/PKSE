/**
 * PokemonTypes.h - Pokemon Type Data Lookup
 *
 * Type data for a (species, form) in a given game, read out of the generated personal
 * table (PersonalInfoTable.cpp) rather than a table of its own.
 *
 * Both qualifiers matter, and neither is optional:
 *
 *   - FORM. Types are one of the things a regional form actually changes: Hisuian
 *     Braviary is Psychic/Flying where the Unovan one is Normal/Flying, and 149 of the
 *     465 alternate forms retype their base species. A species-keyed lookup gets every
 *     one of them wrong, silently and plausibly.
 *   - GAME. Gen 3 predates the Fairy type, so 18 of its 386 species are typed
 *     differently there -- a FireRed Clefairy is Normal, and Marill is pure Water.
 *
 * Type ids are the Gen 6+ numbering in every game including Gen 3 (PKHeX normalises it),
 * so the TYPE_* constants below are the whole vocabulary.
 */

#ifndef POKEMON_TYPES_H
#define POKEMON_TYPES_H

#include <cstdint>

#include "Enums/GameVersion.h"

namespace Pokemon {
    /**
     * Type pair structure for dual-type Pokemon
     */
    struct TypePair {
        uint8_t type1;  // Primary type (0-17, matches MoveType enum)
        uint8_t type2;  // Secondary type (0-17, or 255 for single-type)
    };

    /**
     * Gets the types a Pokemon has in a particular game.
     * @param speciesId Pokemon species ID (1-1025)
     * @param formId Form ID (0 = base form) -- required; regional forms retype
     * @param group The mon's game group; only FRLG reads a different table
     * @return TypePair containing primary and secondary types
     */
    TypePair getPokemonTypes(uint16_t speciesId, uint8_t formId, Enums::GameVersion group);

    /**
     * Check if type2 is valid (not 255)
     */
    constexpr bool hasSecondType(const TypePair& types) {
        return types.type2 != 255;
    }

    // Type ID constants (matches MoveType enum)
    constexpr uint8_t TYPE_NORMAL   = 0;
    constexpr uint8_t TYPE_FIGHTING = 1;
    constexpr uint8_t TYPE_FLYING   = 2;
    constexpr uint8_t TYPE_POISON   = 3;
    constexpr uint8_t TYPE_GROUND   = 4;
    constexpr uint8_t TYPE_ROCK     = 5;
    constexpr uint8_t TYPE_BUG      = 6;
    constexpr uint8_t TYPE_GHOST    = 7;
    constexpr uint8_t TYPE_STEEL    = 8;
    constexpr uint8_t TYPE_FIRE     = 9;
    constexpr uint8_t TYPE_WATER    = 10;
    constexpr uint8_t TYPE_GRASS    = 11;
    constexpr uint8_t TYPE_ELECTRIC = 12;
    constexpr uint8_t TYPE_PSYCHIC  = 13;
    constexpr uint8_t TYPE_ICE      = 14;
    constexpr uint8_t TYPE_DRAGON   = 15;
    constexpr uint8_t TYPE_DARK     = 16;
    constexpr uint8_t TYPE_FAIRY    = 17;
    constexpr uint8_t TYPE_NONE     = 255;
}

#endif
