/**
 * GENERATED -- do not hand-edit.
 *
 * Regenerate with `python tools/gen_dexformtables.py`. Source of truth is PKHeX.
 *
 * THE SEEN/CAUGHT BITS DO NOT NEED THIS. They sit at `species - 1`, the way every arithmetic dex
 * works. What needs it is the ALTERNATE-FORM region: X/Y indexes those by a hand-authored
 * per-species ordering that exists only in PKHeX's data and cannot be derived from the species
 * number.
 */
#ifndef POKEMON_DEXTABLE6XY_H
#define POKEMON_DEXTABLE6XY_H

#include <cstdint>

namespace Pokemon
{
    /// Highest species with form bits in X/Y.
    inline constexpr uint16_t DEX6XY_MAX_FORM_SPECIES = 716;

    /// Bit index of this species' FIRST alternate form, or -1 when it has no form bits at all.
    /// The base form is not here -- it is the species' own seen/caught bit.
    int getDexFormBitIndex6XY(uint16_t species) noexcept;

    /// How many dex forms X/Y gives this species (including the base form), or 0 for none.
    /// A caller holding the species' real form count should refuse to write when that count is
    /// smaller than this, which is the guard PKHeX's GetDexFormBitIndex applies.
    uint8_t getDexFormCount6XY(uint16_t species) noexcept;
}

#endif  // POKEMON_DEXTABLE6XY_H
