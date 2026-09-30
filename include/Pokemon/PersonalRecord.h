/**
 * PersonalRecord.h - the row type every per-group personal table shares, and the dispatcher.
 *
 * PKSE keeps ONE PERSONAL TABLE PER SAVE-FORMAT GROUP -- nineteen of them, PersonalInfo1RBY
 * through PersonalInfo9LZA, each generated from the one PKHeX resource that belongs to it. This
 * header is what they have in common: the row shape, the predicates that say which fields a
 * group's generation actually has, and getPersonalRecord(), the single place that maps a group
 * to its table.
 *
 * WHY NINETEEN COPIES rather than one table per generation. The layer used to share: one Gen 4
 * table served Diamond/Pearl, Platinum and HeartGold/SoulSilver, one Gen 5 table served
 * Black/White and Black 2/White 2, and five Switch groups shared a single base-stat file. That
 * is the same mistake the entity layer made when every PK5 reported B2W2 -- a Black/White
 * Pokemon was checked against Black 2's abilities, which Black/White never had. Sharing a table
 * between games is indistinguishable from correct until the games disagree, and then it is
 * silently wrong. One resource in, one file out; a group whose games genuinely agree still gets
 * its own copy, exactly as Pokemon9SV duplicates Pokemon9LZA.
 *
 * A FIELD A GENERATION DOES NOT HAVE READS 0, and 0 is a real ability id and a real friendship
 * value, so a getter cannot tell "absent" from "zero". Ask the predicates below -- the same
 * reason Pokemon::hasNature() and Pokemon::hasBall() exist one layer up.
 *
 * THIS TABLE DOES NOT ANSWER DEX PRESENCE. Sword/Shield and Legends: Arceus zero the rows for
 * species they do not have, but Let's Go and Scarlet/Violet carry full stats for every id
 * regardless, so an empty row means different things in different tables. PersonalInfoTable's
 * `presence` bits remain the cross-game answer.
 */
#ifndef POKEMON_PERSONALRECORD_H
#define POKEMON_PERSONALRECORD_H

#include <cstdint>
#include <cstddef>

#include "Enums/GameVersion.h"

namespace Pokemon
{
    /**
     * One (species, form) row.
     *
     * Base stats are declared in PKSE's own order -- HP, ATK, DEF, SPE, SPA, SPD -- which is
     * PKHeX's personal-table order and NOT the order the old BaseStatsGen89 rows used. Access
     * them by name, never by position.
     *
     * `type2` is TYPE_NONE (255) for a single-typed species rather than a repeat of `type1`,
     * which is what the tables themselves store. Type ids are PKSE TYPE_* throughout: Gen 1 and
     * Gen 2 ROM numbering is converted at generation time, so no raw ROM id can escape.
     */
    struct PersonalRecord
    {
        uint8_t  hp, atk, def, spe, spa, spd;
        uint8_t  type1;
        uint8_t  type2;              ///< TYPE_NONE (255) when single-typed
        uint16_t ability1;
        uint16_t ability2;           ///< == ability1 when the species has only one
        uint16_t abilityHidden;      ///< 0 before Gen 5, which introduced the slot
        /// PKHeX raw byte: 0 male-only, 254 female-only, 255 genderless, else the female threshold.
        uint8_t  genderRatio;
        uint8_t  baseFriendship;
        uint8_t  growthRate;         ///< 0-5, the six curves in Experience.cpp
        uint8_t  catchRate;
        uint8_t  formCount;          ///< forms this group defines; 1 when the species has none
        uint16_t formIndex;          ///< table index of form 1; 0 when the species has no alt forms
    };

    inline constexpr uint8_t PERSONAL_TYPE_NONE = 255;

    /// Returned for a species a group has no row for. Its zero base stats mean NO DATA -- a
    /// caller must treat that as "unknown", never as a Pokemon with no HP.
    extern const PersonalRecord PERSONAL_RECORD_EMPTY;

    /// The table for `group`, with PKHeX's FormStatsIndex redirection applied. An unknown group
    /// returns PERSONAL_RECORD_EMPTY rather than a plausible wrong row.
    const PersonalRecord &getPersonalRecord(Enums::GameVersion group, uint16_t species, uint8_t form) noexcept;

    /// Highest National Dex id `group`'s table has a row for; 0 for a group with no table.
    uint16_t personalMaxSpecies(Enums::GameVersion group) noexcept;

    /// Does this group's generation store abilities at all? False for Gens 1 and 2.
    bool personalHasAbilities(Enums::GameVersion group) noexcept;
    /// Does it have a hidden-ability slot? Gen 5 introduced it, so false through Gen 4.
    bool personalHasHiddenAbility(Enums::GameVersion group) noexcept;
    /// Does its table carry alternate-form rows? False for Gens 1-3, whose tables have none.
    bool personalHasForms(Enums::GameVersion group) noexcept;
    /// Are SpA and SpD independent? False for Gen 1, which has ONE Special written into both
    /// slots -- reading either gives the right number, but they cannot differ.
    bool personalHasSplitSpecial(Enums::GameVersion group) noexcept;
}

#endif  // POKEMON_PERSONALRECORD_H
