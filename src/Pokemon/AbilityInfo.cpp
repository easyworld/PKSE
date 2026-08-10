/**
 * AbilityInfo.cpp - per-game ability slot resolution. See AbilityInfo.h.
 */
#include "Pokemon/AbilityInfo.h"
#include "Pokemon/PersonalInfoTable.h"

namespace Pokemon {

    AbilitySlots getAbilitySlots(uint16_t species, uint8_t form, Enums::GameVersion group) {
        if (group == Enums::GameVersion::FRLG) {
            // Gen 3: two slots, no hidden ability. Forms share one row.
            const PersonalAbilityG3& g3 = getPersonalAbilityG3(species);
            return AbilitySlots{ { g3.ability1, g3.ability2, 0 }, 2 };
        }
        const PersonalInfo& pi = getPersonalInfo(species, form);
        return AbilitySlots{ { pi.ability1, pi.ability2, pi.abilityHidden }, 3 };
    }

    uint8_t getAbilityNumberForId(const AbilitySlots& slots, uint16_t abilityId) noexcept {
        if (abilityId == 0) return 0;
        // Lowest matching slot wins, so a species whose slot 2 duplicates slot 1 reads
        // as slot 1 -- the same collapse PKHeX's GetIndexOfAbility does.
        for (int i = 0; i < slots.count; ++i)
            if (slots.slot[i] == abilityId)
                return static_cast<uint8_t>(1 << i);   // 1 / 2 / 4
        return 0;
    }
}
