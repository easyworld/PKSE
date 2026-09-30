#ifndef ENUMS_ENUMS_H
#define ENUMS_ENUMS_H

#include <cstdint>
#include <cstddef>

#include "Enums/GameVersion.h"

namespace Enums
{
    /// Contiguous series Game Language IDs
    enum class LanguageID
    {
        /// Undefined Language ID, usually indicative of a value not being set.
        /// Gen5 Japanese In-game Trades happen to not have their Language value set, and express Language=0.
        Hacked,

        /// Japanese (日本語)
        Japanese,

        /// English (US/UK/AU)
        English,

        /// French (Français)
        French,

        /// Italian (Italiano)
        Italian,

        /// German (Deutsch)
        German,

        /// Unused Language ID
        /// Was reserved for Korean in Gen3 but never utilized.
        UNUSED_6,

        /// Spanish (Español)
        Spanish,

        /// Korean (한국어)
        Korean,

        /// Chinese Simplified (简体中文)
        ChineseSimplified,

        /// Chinese Traditional (繁體中文)
        ChineseTraditional,

        /// Spanish, Latin America (Español latinoamericano)
        /// Legends: Z-A introduced it; no earlier title has a game in it.
        SpanishL
    };

    /// Short language name for a stored language id (indices match LanguageID above).
    inline const char *getLanguageName(uint8_t languageId)
    {
        static const char *const names[] = {
            "-", "Japanese", "English", "French", "Italian", "German",
            "-", "Spanish", "Korean", "Chinese (S)", "Chinese (T)", "Spanish (LATAM)"};
        return languageId < (sizeof(names) / sizeof(names[0])) ? names[languageId] : "-";
    }

    /**
     * Whether a game in `group` was ever released in `languageId`.
     *
     * THE SET GREW OVER TIME AND NOTHING IN A RECORD SAYS SO. Korean arrived with the Gen 4 DS
     * titles (and Gold/Silver, whose Korean release is the reason Gen 2 differs from Gen 1 and
     * Gen 3), the two Chinese scripts with Sun/Moon, and Latin American Spanish with Legends: Z-A.
     * A language byte is copied verbatim by every transfer, so a record moving DOWN can arrive
     * carrying one its destination has no game in -- a Korean Pokemon written into a PK3 stores an
     * id Gen 3 reserved and never used, and no Gen 3 cartridge can draw its alphabet. That is what
     * safeLanguageForGroup exists to stop.
     *
     * PKHeX Language.GetAvailableGameLanguages, expressed per storage-format group rather than per
     * generation because a group is what PKSE dispatches on. Hacked (0) and UNUSED_6 belong to no
     * game in any generation: 0 is a real value a Gen 5 in-game trade stores, so it has to be
     * answered rather than indexed with.
     */
    inline constexpr bool groupHasLanguage(GameVersion group, uint8_t languageId) noexcept
    {
        const LanguageID language = static_cast<LanguageID>(languageId);
        if (language == LanguageID::Hacked || language == LanguageID::UNUSED_6)
        {
            return false;
        }
        if (language <= LanguageID::Spanish)
        {
            return true; // Japanese through Spanish: every generation has them
        }
        switch (group)
        {
        case GameVersion::RBY:  // Gen 1 and Gen 3 are the two that never got a Korean release
        case GameVersion::FRLG:
        case GameVersion::RSE:
            return false;
        case GameVersion::GSC: // Korean Gold/Silver exists; Korean Crystal does not, and the
        case GameVersion::DP:  // group cannot tell them apart -- that is the checker's question
        case GameVersion::PT:
        case GameVersion::HGSS:
        case GameVersion::BW:
        case GameVersion::B2W2:
        case GameVersion::XY:
        case GameVersion::ORAS:
            return language == LanguageID::Korean;
        case GameVersion::ZA:
            return true; // the only group with a game in Latin American Spanish
        default:
            // Gen 7 onward: Korean plus both Chinese scripts, and nothing later.
            return language != LanguageID::SpanishL;
        }
    }

    /**
     * `languageId` if `group` has a game in it, English otherwise.
     *
     * English is the substitute because it is the one language every generation shipped and the
     * one every character table can spell -- PKHeX picks it for the same reason (Language.cs
     * `SafeLanguage`). Clamping is not cosmetic: the language decides which character table a
     * Gen 3 name is written and read in, so a record left claiming a language its destination
     * never had would be decoded through a table that cannot represent it.
     */
    inline constexpr uint8_t safeLanguageForGroup(GameVersion group, uint8_t languageId) noexcept
    {
        return groupHasLanguage(group, languageId) ? languageId : static_cast<uint8_t>(LanguageID::English);
    }
}

#endif