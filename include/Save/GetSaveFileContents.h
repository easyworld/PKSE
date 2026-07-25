#ifndef GET_SAVE_FILE_CONTENTS_H
#define GET_SAVE_FILE_CONTENTS_H

#include <string>
#include <variant>

#include <switch.h>

#include "Enums/GameVersion.h"
#include "Trainer/Trainer.h"
#include "Trainer/Trainer7LGPE.h"
#include "Trainer/Trainer8SWSH.h"
#include "Trainer/Trainer9LZA.h"
#include "Trainer/Trainer9SV.h"
#include "Trainer/Trainer8LA.h"
#include "Trainer/Trainer8BDSP.h"
#include "Trainer/Trainer3FRLG.h"

using namespace Enums;
using namespace Trainer;

namespace Save {
    // Type alias for trainer variants (supports different generation trainers)
    using TrainerVariant = std::variant<Trainer7LGPE, Trainer8SWSH, Trainer9LZA, Trainer9SV, Trainer8LA, Trainer8BDSP, Trainer3FRLG>;

    /**
     * Save File Reading/Writing Functions
     *
     * Different Pokemon games use different save file formats:
     * - Let's Go (GG): savedata.bin (1MB, simpler encryption)
     * - Sword/Shield (SWSH): main (1.6MB, block-based encryption)
     *
     * These functions provide game-specific save file handling.
     */

    // ========================================
    // Generic Functions (Auto-detect game)
    // ========================================

    /**
     * Reads trainer info from a save file, auto-detecting the game version.
     *
     * @param backupDir The backup directory containing the save file
     * @param titleId The game's title ID (used to detect game version)
     * @return TrainerVariant holding the game-specific Trainer* subclass (one of the seven)
     */
    TrainerVariant readTrainerInfo(const char* backupDir, u64 titleId);

    /**
     * Saves trainer info to a save file, auto-detecting the game version.
     *
     * @param trainer The trainer data to save (base Trainer reference)
     * @param backupDir The backup directory to save to
     * @param titleId The game's title ID (used to detect game version)
     * @param userUid The user account ID for save data access
     * @return true if save was successful, false otherwise
     */
    bool saveTrainerInfo(Trainer::Trainer& trainer, const char* backupDir, u64 titleId, AccountUid userUid, bool injectToTitle);

    // ========================================
    // Game-Specific Functions
    // ========================================

    /**
     * Reads trainer info from a Pokemon Let's Go Pikachu/Eevee save file.
     *
     * Let's Go save format:
     * - File: "savedata.bin" (1,048,576 bytes = 1MB)
     * - Encryption: Simpler than SwSh, different block structure
     * - No external hash (checksum is internal)
     *
     * @param backupDir The backup directory containing the "savedata.bin" file
     * @return Trainer7 object with loaded data
     */
    Trainer7LGPE readTrainerInfoLetsGo(const char* backupDir);

    /**
     * Saves trainer info to a Pokemon Let's Go Pikachu/Eevee save file.
     *
     * @param trainer The trainer data to save (Trainer7)
     * @param backupDir The backup directory to save to
     * @param titleId The game's title ID
     * @param userUid The user account ID for save data access
     * @return true if save was successful, false otherwise
     */
    bool saveTrainerInfoLetsGo(Trainer7LGPE& trainer, const char* backupDir, u64 titleId, AccountUid userUid, bool injectToTitle);

    /**
     * Reads trainer info from a Pokemon Sword/Shield save file.
     *
     * Sword/Shield save format:
     * - File: "main" (1,603,146 bytes typically)
     * - Encryption: Block-based with SwSh encryption
     * - Hash: Last 32 bytes
     *
     * @param backupDir The backup directory containing the "main" file
     * @return Trainer8 object with loaded data
     */
    Trainer8SWSH readTrainerInfoSwSh(const char* backupDir);

    /**
     * Saves trainer info to a Pokemon Sword/Shield save file.
     *
     * @param trainer The trainer data to save (Trainer8)
     * @param backupDir The backup directory to save to
     * @param titleId The game's title ID
     * @param userUid The user account ID for save data access
     * @return true if save was successful, false otherwise
     */
    bool saveTrainerInfoSwSh(Trainer8SWSH& trainer, const char* backupDir, u64 titleId, AccountUid userUid, bool injectToTitle);

    /**
     * Reads trainer info from a Pokemon Legends: Z-A save file.
     *
     * Legends: Z-A save format:
     * - File: "main" (Gen 9 SCBlock container)
     * - Encryption: Block-based SwishCrypto (SCBlocks)
     * - Hash: Last 32 bytes
     *
     * @param backupDir The backup directory containing the "main" file
     * @return Trainer9LZA object with loaded data
     */
    Trainer9LZA readTrainerInfoLZA(const char* backupDir);

    /**
     * Reads trainer info from a Pokemon Scarlet/Violet save file.
     * S/V uses the Gen 9 SCBlock format but PACKS its box/party slots (no gap), so it has its own
     * dedicated Trainer9SV class (Trainer9LZA is the gapped Legends: Z-A counterpart).
     */
    Trainer9SV readTrainerInfoSV(const char* backupDir);

    /**
     * Saves trainer info to a Pokemon Legends: Z-A save file.
     *
     * @param trainer The trainer data to save (Trainer9LZA)
     * @param backupDir The backup directory to save to
     * @param titleId The game's title ID
     * @param userUid The user account ID for save data access
     * @return true if save was successful, false otherwise
     */
    bool saveTrainerInfoLZA(Trainer9LZA& trainer, const char* backupDir, u64 titleId, AccountUid userUid, bool injectToTitle);

    /**
     * Saves trainer info to a Pokemon Scarlet/Violet save file (dedicated Trainer9SV, packed slots).
     */
    bool saveTrainerInfoSV(Trainer9SV& trainer, const char* backupDir, u64 titleId, AccountUid userUid, bool injectToTitle);

    /** Reads trainer info from a Pokemon Legends: Arceus save file (PA8; box slots are stored-size). */
    Trainer8LA readTrainerInfoLA(const char* backupDir);
    /** Saves a Legends: Arceus save (party + box serialize; LA item write is deferred). */
    bool saveTrainerInfoLA(Trainer8LA& trainer, const char* backupDir, u64 titleId, AccountUid userUid, bool injectToTitle);

    /** Reads a BDSP (SAV8BDSP) FLAT save (SaveData.bin) into a Trainer8BDSP — no SwishCrypto decrypt. */
    Trainer8BDSP readTrainerInfoBDSP(const char* backupDir);
    /** Saves a BDSP save: flat party/box serialize + whole-file MD5 rehash (SaveData.bin + Backup.bin). */
    bool saveTrainerInfoBDSP(Trainer8BDSP& trainer, const char* backupDir, u64 titleId, AccountUid userUid, bool injectToTitle);

    /** Reads a GBA FireRed/LeafGreen (SAV3FRLG) 128 KiB save into a Trainer3FRLG (scans the dir for the
     *  128 KiB / *.sav file so Checkpoint exports like "FireRed_e.sav" are found automatically). */
    Trainer3FRLG readTrainerInfoFRLG(const char* backupDir);
    /** Saves an FRLG save: re-encrypt party/boxes/items in place + recompute all 14 sector checksums. */
    bool saveTrainerInfoFRLG(Trainer3FRLG& trainer, const char* backupDir, u64 titleId, AccountUid userUid, bool injectToTitle);
}

#endif