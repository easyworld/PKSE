/**
 * AbilityInfo.h - Which abilities a (species, form) can legally hold in a given game.
 *
 * The ability a Pokemon is allowed to have is not a free id: every species has up to
 * three ABILITY SLOTS -- slot 1, slot 2 and Hidden -- and the mon holds one of them.
 * Most species have slot 2 == slot 1, meaning they have exactly one possible ability.
 *
 * The slot pair is per-GENERATION, not universal, so this resolves against the right
 * table for the mon's game:
 *   - FireRed/LeafGreen  -> PERSONAL_ABILITY_G3 (two slots, no Hidden). Gen 3 disagrees
 *     with the modern table for 101 of its 386 species, and a PK3 stores only a BIT
 *     that the game resolves through its OWN table -- so a modern id written to a Gen 3
 *     mon displays as whatever that game's slot holds instead.
 *   - everything else    -> PersonalInfo (Scarlet/Violet-sourced; three slots).
 *
 * PKHeX's ability NUMBER encoding is kept throughout: 1 = slot 1, 2 = slot 2, 4 = Hidden.
 */
#ifndef PKM_ABILITY_INFO_H
#define PKM_ABILITY_INFO_H

#include <cstdint>

#include "Enums/GameVersion.h"

namespace Pokemon {

    /** The ability slots a species/form can hold in one game. */
    struct AbilitySlots {
        uint16_t slot[3];   // slot[0] = ability 1, slot[1] = ability 2, slot[2] = hidden
        uint8_t  count;     // 2 for Gen 3 (it has no hidden slot), otherwise 3

        /** Distinct ability ids, in slot order. Most species yield 1 or 2, not 3. */
        int distinct(uint16_t out[3]) const noexcept {
            int n = 0;
            for (int i = 0; i < count; ++i) {
                if (slot[i] == 0) continue;
                bool dup = false;
                for (int j = 0; j < n; ++j) if (out[j] == slot[i]) { dup = true; break; }
                if (!dup) out[n++] = slot[i];
            }
            return n;
        }
    };

    /** Ability slots for a species/form as the given game group defines them. */
    AbilitySlots getAbilitySlots(uint16_t species, uint8_t form, Enums::GameVersion group);

    /** PKHeX ability number (1 / 2 / 4) for an ability id, or 0 if it fills no slot. */
    uint8_t getAbilityNumberForId(const AbilitySlots& slots, uint16_t abilityId) noexcept;

    /** True if the id occupies one of the species' slots in this game. */
    inline bool isAbilityLegal(const AbilitySlots& slots, uint16_t abilityId) noexcept {
        return getAbilityNumberForId(slots, abilityId) != 0;
    }
}

#endif  // PKM_ABILITY_INFO_H
