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

        // Gen 3 (GBA) -- values match the Origins version field in PK3
        FR = 4,  // Pokemon FireRed
        LG = 5,  // Pokemon LeafGreen

        // Nintendo Switch
        GP = 42, // Pokemon: Let's Go, Pikachu!
        GE = 43, // Pokemon: Let's Go, Eevee!
        SW = 44, // Pokemon Sword
        SH = 45, // Pokemon Shield
        // HOME = 46, // Not used (?)
        PLA = 47, // Pokemon Legends: Arceus
        BD = 48,  // Pokemon Brilliant Diamond
        SP = 49,  // Pokemon Shining Pearl
        SL = 50,  // Pokemon Scarlet
        VL = 51,  // Pokemon Violet
        ZA = 52,  // Pokemon Legends: Z-A

        // The following values are not actually stored values in pk data,
        // These values are assigned as properties for various logic branching.

        // Game Groupings
        FRLG = 72, // Pokemon FireRed & LeafGreen group
        GG = 73,   // Pokemon Let's Go Pikachu & Eevee group
        SWSH = 74, // Pokemon Sword & Shield group
        BDSP = 75, // Pokemon Brilliant Diamond & Shining Pearl group
        SV = 76,   // Pokemon Scarlet & Violet group

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
            // Gen 3 - FireRed/LeafGreen (Nintendo Switch Online GBA release)
            case 0x0100554023408000: return GameVersion::FR;  // FireRed   (title ids from real Switch backups)
            case 0x010034D02340E000: return GameVersion::LG;  // LeafGreen

            // // Gen 7 - Let's Go
            case 0x010003F003A34000: return GameVersion::GP;  // Let's Go Pikachu
            case 0x0100187003A36000: return GameVersion::GE;  // Let's Go Eevee

            // Gen 8 - Sword/Shield
            case 0x0100ABF008968000: return GameVersion::SW;  // Sword
            case 0x01008DB008C2C000: return GameVersion::SH;  // Shield

            // Gen 8 - BDSP
            case 0x0100000011D90000: return GameVersion::BD;  // Brilliant Diamond
            case 0x010018E011D92000: return GameVersion::SP;  // Shining Pearl

            // Gen 8 - Legends Arceus
            case 0x01001F5010DFA000: return GameVersion::PLA;

            // Gen 9 - Scarlet/Violet (reuse the Gen 9 save path with packed slots)
            case 0x0100A3D008C5C000: return GameVersion::SL;  // Scarlet
            case 0x01008F6008C5E000: return GameVersion::VL;  // Violet

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
            case GameVersion::FR:
            case GameVersion::LG:
                return GameVersion::FRLG;  // FireRed/LeafGreen group

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
            case GameVersion::FR: return "火红";
            case GameVersion::LG: return "叶绿";
            case GameVersion::FRLG: return "火红／叶绿";
            case GameVersion::GP: return "Let's Go！皮卡丘";
            case GameVersion::GE: return "Let's Go！伊布";
            case GameVersion::SW: return "剑";
            case GameVersion::SH: return "盾";
            case GameVersion::BD: return "晶灿钻石";
            case GameVersion::SP: return "明亮珍珠";
            case GameVersion::PLA: return "传说 阿尔宙斯";
            case GameVersion::SL: return "朱";
            case GameVersion::VL: return "紫";
            case GameVersion::ZA: return "传说 宝可梦Z-A";
            case GameVersion::GG: return "Let's Go！皮卡丘／伊布";
            case GameVersion::SWSH: return "剑／盾";
            case GameVersion::BDSP: return "晶灿钻石／明亮珍珠";
            case GameVersion::SV: return "朱／紫";
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
            case GameVersion::BDSP: return "SaveData.bin"; // BDSP (verified via Checkpoint backup)
            case GameVersion::PLA: return "main";         // Legends: Arceus
            case GameVersion::SV: return "main";          // Scarlet/Violet
            default: return "main";
        }
    }

    /**
     * True if a Pokemon originating from `version` displays its trainer ID as the Gen 7+
     * SIX-DIGIT id, rather than the classic 16-bit TID.
     *
     * The games pick this from the Pokemon's ORIGIN generation, not from the save it currently
     * lives in -- so a FireRed Rattata sitting in Shining Pearl still shows its 16-bit TID while
     * a native BDSP mon beside it shows six digits. Mirrors PKHeX
     * (GameDataCore.TrainerIDDisplayFormat: `Version.Generation >= 7 ? SixDigit : SixteenBit`).
     *
     * Takes a raw version byte rather than a GameVersion, because a banked mon can carry an origin
     * from a game PKSE does not itself support (a HOME-transferred Sun/Moon or Gen 4/5/6 mon), and
     * those ids are absent from the enum above. Gen 7+ is versions 30-33 (SM/USUM) plus 42 and up
     * (Let's Go, Gen 8, Gen 9). Note 35-41 are the Gen 1/2 Virtual Console games, which sit ABOVE
     * the Gen 7 SM/USUM ids numerically but are of course 16-bit.
     */
    inline bool usesSixDigitTrainerID(uint8_t version) {
        return (version >= 30 && version <= 33) || version >= 42;
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
            case GameVersion::FR:  // FireRed
            case GameVersion::LG:  // LeafGreen
            case GameVersion::GP:  // Let's Go Pikachu
            case GameVersion::GE:  // Let's Go Eevee
            case GameVersion::SW:  // Sword
            case GameVersion::SH:  // Shield
            case GameVersion::ZA:  // Legends: Z-A
            case GameVersion::SL:  // Scarlet
            case GameVersion::VL:  // Violet
            case GameVersion::PLA: // Legends: Arceus
            case GameVersion::BD:  // Brilliant Diamond
            case GameVersion::SP:  // Shining Pearl
                return true;

            // Not yet implemented:
            default:
                return false;
        }
    }
}

#endif
