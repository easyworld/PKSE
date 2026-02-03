#ifndef ENUMS_GAME_VERSION_H
#define ENUMS_GAME_VERSION_H

#include <string>

namespace Enums {
    // Game Version ID enum shared between actual Version IDs and lumped version groupings
    enum class GameVersion
    {
        // Indicators for method empty arguments & result indication. Not stored values.
        Any = 0,
        Invalid = 255,

        // The following values are IDs stored within PKM data, and can also identify individual games.

        // Nintendo Switch
        GP = 42, // Pokémon: Let's Go, Pikachu!
        GE = 43, // Pokémon: Let's Go, Eevee!
        SW = 44, // Pokémon Sword
        SH = 45, // Pokémon Shield
        // HOME = 46, // Not used (?)
        PLA = 47, // Pokémon Legends: Arceus
        BD = 48,  // Pokémon Brilliant Diamond
        SP = 49,  // Pokémon Shining Pearl
        SL = 50,  // Pokémon Scarlet
        VL = 51,  // Pokémon Violet
        ZA = 52,  // Pokémon Legends: Z-A

        // The following values are not actually stored values in pk data,
        // These values are assigned as properties for various logic branching.

        // Game Groupings
        GG = 73,   // Pokémon Let's Go Pikachu & Eevee group
        SWSH = 74, // Pokémon Sword & Shield group
        BDSP = 75, // Pokémon Brilliant Diamond & Shining Pearl group
        SV = 76,   // Pokémon Scarlet & Violet group

        // Generational Groupings
        Gen7B = 84,
        Gen8 = 85,
        Gen9 = 86
    };

    /**
     * Gets the GameVersion from a Nintendo Switch title ID.
     *
     * This function maps Nintendo Switch title IDs to their corresponding
     * GameVersion enum values. This is essential for determining which save
     * file format, encryption method, and data structures to use.
     *
     * @param titleId The Nintendo Switch title ID
     * @return The corresponding GameVersion, or GameVersion::Invalid if not recognized
     */
    inline GameVersion getGameVersion(uint64_t titleId) {
        switch (titleId) {
            // // Gen 7 - Let's Go
            // case 0x010003F003A34000: return GameVersion::GP;  // Let's Go Pikachu
            // case 0x0100187003A36000: return GameVersion::GE;  // Let's Go Eevee

            // Gen 8 - Sword/Shield
            case 0x0100ABF008968000: return GameVersion::SW;  // Sword
            case 0x01008DB008C2C000: return GameVersion::SH;  // Shield

            // // Gen 8 - BDSP
            // case 0x0100000011D90000: return GameVersion::BD;  // Brilliant Diamond
            // case 0x010018E011D92000: return GameVersion::SP;  // Shining Pearl

            // // Gen 8 - Legends Arceus
            // case 0x01001F5010DFA000: return GameVersion::PLA;

            // // Gen 9 - Scarlet/Violet
            // case 0x0100A3D008C5C000: return GameVersion::SL;  // Scarlet
            // case 0x01008F6008C5E000: return GameVersion::VL;  // Violet

            // Gen 9 - Legends: Z-A
            case 0x0100F43008C44000: return GameVersion::ZA;

            default: return GameVersion::Invalid;
        }
    }

    /**
     * Gets the game group for a given GameVersion.
     *
     * This groups games that share the same save file format and encryption.
     * Games in the same group can use the same save reading/writing functions.
     *
     * @param version The GameVersion to check
     * @return The corresponding game group GameVersion
     */
    inline GameVersion getGameGroup(GameVersion version) {
        switch (version) {
            case GameVersion::GP:
            case GameVersion::GE:
                return GameVersion::GG;  // Let's Go group

            case GameVersion::SW:
            case GameVersion::SH:
                return GameVersion::SWSH;  // Sword/Shield group

            case GameVersion::BD:
            case GameVersion::SP:
                return GameVersion::BDSP;  // BDSP group

            case GameVersion::SL:
            case GameVersion::VL:
                return GameVersion::SV;  // Scarlet/Violet group

            case GameVersion::PLA:
                return GameVersion::PLA;  // Legends Arceus (its own group)

            case GameVersion::ZA:
                return GameVersion::ZA;  // Legends ZA (its own group)

            default:
                return GameVersion::Invalid;
        }
    }

    /**
     * Gets a human-readable name for a GameVersion.
     *
     * @param version The GameVersion
     * @return A string name for the game
     */
    inline std::string getGameVersionName(GameVersion version) {
        switch (version) {
            case GameVersion::GP: return "Let's Go! 皮卡丘";
            case GameVersion::GE: return "Let's Go! 伊布";
            case GameVersion::SW: return "剑";
            case GameVersion::SH: return "盾";
            case GameVersion::BD: return "晶灿钻石";
            case GameVersion::SP: return "明亮珍珠";
            case GameVersion::PLA: return "传说 阿尔宙斯";
            case GameVersion::SL: return "朱";
            case GameVersion::VL: return "紫";
            case GameVersion::ZA: return "传说 Z-A";
            case GameVersion::GG: return "Let's Go! 皮卡丘/伊布";
            case GameVersion::SWSH: return "剑/盾";
            case GameVersion::BDSP: return "晶灿钻石/明亮珍珠";
            case GameVersion::SV: return "朱/紫";
            default: return "未知";
        }
    }

    /**
     * Gets the save file name for a game group.
     *
     * Different game groups use different save file names:
     * - Let's Go (GG): savedata.bin (1,048,576 bytes)
     * - Sword/Shield (SWSH): main (varies)
     * - Others: main (typically)
     *
     * @param group The game group GameVersion
     * @return The save file name (without path)
     */
    inline const char* getSaveFileName(GameVersion group) {
        switch (group) {
            case GameVersion::GG: return "savedata.bin";  // Let's Go
            case GameVersion::SWSH: return "main";        // Sword/Shield
            case GameVersion::BDSP: return "main";        // BDSP (TODO: Verify)
            case GameVersion::PLA: return "main";         // Legends Arceus (TODO: Verify)
            case GameVersion::SV: return "main";          // Scarlet/Violet (TODO: Verify)
            default: return "main";
        }
    }

    /**
     * Checks if a game version is currently supported by PKSE.
     *
     * Games that are supported have fully implemented save file reading/writing.
     *
     * @param version The GameVersion to check
     * @return true if the game is supported, false otherwise
     */
    inline bool isGameSupported(GameVersion version) {
        switch (version) {
            case GameVersion::GP:  // Let's Go Pikachu
            case GameVersion::GE:  // Let's Go Eevee
            case GameVersion::SW:  // Sword
            case GameVersion::SH:  // Shield
                return true;

            // Not yet implemented:
            case GameVersion::BD:
            case GameVersion::SP:
            case GameVersion::PLA:
            case GameVersion::SL:
            case GameVersion::VL:
            default:
                return false;
        }
    }
}

#endif