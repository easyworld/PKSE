/**
 * THE NINETEEN-WAY SWITCH LIVES HERE, ONCE, the same way Bank owns the one mapping from a
 * save-format group to its entity class. A second copy of it in a caller is a table that
 * has to be kept in step with this one, which is the failure the split exists to end.
 */
#include "Pokemon/PersonalInfo1RBY.h"
#include "Pokemon/PersonalInfo2GSC.h"
#include "Pokemon/PersonalInfo3RSE.h"
#include "Pokemon/PersonalInfo3FRLG.h"
#include "Pokemon/PersonalInfo4DP.h"
#include "Pokemon/PersonalInfo4PT.h"
#include "Pokemon/PersonalInfo4HGSS.h"
#include "Pokemon/PersonalInfo5BW.h"
#include "Pokemon/PersonalInfo5B2W2.h"
#include "Pokemon/PersonalInfo6XY.h"
#include "Pokemon/PersonalInfo6ORAS.h"
#include "Pokemon/PersonalInfo7SM.h"
#include "Pokemon/PersonalInfo7USUM.h"
#include "Pokemon/PersonalInfo7LGPE.h"
#include "Pokemon/PersonalInfo8SWSH.h"
#include "Pokemon/PersonalInfo8BDSP.h"
#include "Pokemon/PersonalInfo8LA.h"
#include "Pokemon/PersonalInfo9SV.h"
#include "Pokemon/PersonalInfo9LZA.h"

namespace Pokemon
{
    const PersonalRecord PERSONAL_RECORD_EMPTY{};

    const PersonalRecord & getPersonalRecord(Enums::GameVersion group, uint16_t species, uint8_t form) noexcept
    {
        switch (group)
        {
        case Enums::GameVersion::RBY:
            return getPersonalInfo1RBY(species, form);
        case Enums::GameVersion::GSC:
            return getPersonalInfo2GSC(species, form);
        case Enums::GameVersion::RSE:
            return getPersonalInfo3RSE(species, form);
        case Enums::GameVersion::FRLG:
            return getPersonalInfo3FRLG(species, form);
        case Enums::GameVersion::DP:
            return getPersonalInfo4DP(species, form);
        case Enums::GameVersion::PT:
            return getPersonalInfo4PT(species, form);
        case Enums::GameVersion::HGSS:
            return getPersonalInfo4HGSS(species, form);
        case Enums::GameVersion::BW:
            return getPersonalInfo5BW(species, form);
        case Enums::GameVersion::B2W2:
            return getPersonalInfo5B2W2(species, form);
        case Enums::GameVersion::XY:
            return getPersonalInfo6XY(species, form);
        case Enums::GameVersion::ORAS:
            return getPersonalInfo6ORAS(species, form);
        case Enums::GameVersion::SM:
            return getPersonalInfo7SM(species, form);
        case Enums::GameVersion::USUM:
            return getPersonalInfo7USUM(species, form);
        case Enums::GameVersion::GG:
            return getPersonalInfo7LGPE(species, form);
        case Enums::GameVersion::SWSH:
            return getPersonalInfo8SWSH(species, form);
        case Enums::GameVersion::BDSP:
            return getPersonalInfo8BDSP(species, form);
        case Enums::GameVersion::PLA:
            return getPersonalInfo8LA(species, form);
        case Enums::GameVersion::SV:
            return getPersonalInfo9SV(species, form);
        case Enums::GameVersion::ZA:
            return getPersonalInfo9LZA(species, form);
        default:
            return PERSONAL_RECORD_EMPTY;
        }
    }

    uint16_t personalMaxSpecies(Enums::GameVersion group) noexcept
    {
        switch (group)
        {
        case Enums::GameVersion::RBY:
            return PERSONAL_MAX_SPECIES_1RBY;
        case Enums::GameVersion::GSC:
            return PERSONAL_MAX_SPECIES_2GSC;
        case Enums::GameVersion::RSE:
            return PERSONAL_MAX_SPECIES_3RSE;
        case Enums::GameVersion::FRLG:
            return PERSONAL_MAX_SPECIES_3FRLG;
        case Enums::GameVersion::DP:
            return PERSONAL_MAX_SPECIES_4DP;
        case Enums::GameVersion::PT:
            return PERSONAL_MAX_SPECIES_4PT;
        case Enums::GameVersion::HGSS:
            return PERSONAL_MAX_SPECIES_4HGSS;
        case Enums::GameVersion::BW:
            return PERSONAL_MAX_SPECIES_5BW;
        case Enums::GameVersion::B2W2:
            return PERSONAL_MAX_SPECIES_5B2W2;
        case Enums::GameVersion::XY:
            return PERSONAL_MAX_SPECIES_6XY;
        case Enums::GameVersion::ORAS:
            return PERSONAL_MAX_SPECIES_6ORAS;
        case Enums::GameVersion::SM:
            return PERSONAL_MAX_SPECIES_7SM;
        case Enums::GameVersion::USUM:
            return PERSONAL_MAX_SPECIES_7USUM;
        case Enums::GameVersion::GG:
            return PERSONAL_MAX_SPECIES_7LGPE;
        case Enums::GameVersion::SWSH:
            return PERSONAL_MAX_SPECIES_8SWSH;
        case Enums::GameVersion::BDSP:
            return PERSONAL_MAX_SPECIES_8BDSP;
        case Enums::GameVersion::PLA:
            return PERSONAL_MAX_SPECIES_8LA;
        case Enums::GameVersion::SV:
            return PERSONAL_MAX_SPECIES_9SV;
        case Enums::GameVersion::ZA:
            return PERSONAL_MAX_SPECIES_9LZA;
        default:
            return 0;
        }
    }

    /// Gens 1 and 2 have no abilities.
    bool personalHasAbilities(Enums::GameVersion group) noexcept
    {
        switch (group)
        {
        case Enums::GameVersion::RBY:
        case Enums::GameVersion::GSC:
            return false;
        default:
            return personalMaxSpecies(group) != 0;
        }
    }

    /// Gen 5 introduced the hidden slot.
    bool personalHasHiddenAbility(Enums::GameVersion group) noexcept
    {
        switch (group)
        {
        case Enums::GameVersion::RBY:
        case Enums::GameVersion::GSC:
        case Enums::GameVersion::RSE:
        case Enums::GameVersion::FRLG:
        case Enums::GameVersion::DP:
        case Enums::GameVersion::PT:
        case Enums::GameVersion::HGSS:
            return false;
        default:
            return personalMaxSpecies(group) != 0;
        }
    }

    /// Gens 1-3 store no alternate-form rows.
    bool personalHasForms(Enums::GameVersion group) noexcept
    {
        switch (group)
        {
        case Enums::GameVersion::RBY:
        case Enums::GameVersion::GSC:
        case Enums::GameVersion::RSE:
        case Enums::GameVersion::FRLG:
            return false;
        default:
            return personalMaxSpecies(group) != 0;
        }
    }

    /// Gen 1 has ONE Special, in both slots.
    bool personalHasSplitSpecial(Enums::GameVersion group) noexcept
    {
        switch (group)
        {
        case Enums::GameVersion::RBY:
            return false;
        default:
            return personalMaxSpecies(group) != 0;
        }
    }

}
