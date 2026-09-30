/**
 * GENERATED -- do not hand-edit.
 *
 * Regenerate with `python tools/gen_dexformtables.py`. Source of truth is PKHeX.
 *
 * THE SEEN/CAUGHT BITS DO NOT NEED THIS. They sit at `species - 1`, the way every arithmetic dex
 * works. What needs it is the ALTERNATE-FORM region: Let's Go indexes those by a hand-authored
 * per-species ordering that exists only in PKHeX's data and cannot be derived from the species
 * number.
 *
 * Let's Go also gates on WHICH (species, form) pairs its dex has an entry for at
 * all, which is a second table and a different question from the form bit --
 * see getDexEntryIndex7LGPE.
 */
#ifndef POKEMON_DEXTABLE7LGPE_H
#define POKEMON_DEXTABLE7LGPE_H

#include <cstdint>

namespace Pokemon
{
    /// Highest species with form bits in Let's Go.
    inline constexpr uint16_t DEX7LGPE_MAX_FORM_SPECIES = 150;

    /// Bit index of this species' FIRST alternate form, or -1 when it has no form bits at all.
    /// The base form is not here -- it is the species' own seen/caught bit.
    int getDexFormBitIndex7LGPE(uint16_t species) noexcept;

    /// How many dex forms Let's Go gives this species (including the base form), or 0 for none.
    /// A caller holding the species' real form count should refuse to write when that count is
    /// smaller than this, which is the guard PKHeX's GetDexFormBitIndex applies.
    uint8_t getDexFormCount7LGPE(uint16_t species) noexcept;

    /// Dex entry index for a (species, form), or -1 when the dex has no entry for it --
    /// which is also the GATE: a Pokemon absent from this table is not recorded at all.
    /// 0-150 are Kanto, 151/152 are Meltan and Melmetal, and 153+ are the alternate forms
    /// below, in table order. PKHeX Zukan7b.TryGetSizeEntryIndex.
    int getDexEntryIndex7LGPE(uint16_t species, uint8_t form) noexcept;
}

#endif  // POKEMON_DEXTABLE7LGPE_H
