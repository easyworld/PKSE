#include <cstdio>
#include <vector>
#include <locale>
#include <codecvt>

#include <errno.h>
#include <bits/basic_string.h>
#include <sys/stat.h>
#include <dirent.h>

#include "Globals.h"
#include "Save/Block.h"
#include "Save/GetSaveFileContents.h"
#include "Utils/FileUtilities.h"
#include "Utils/Logger.h"
#include "Utils/HelperUtilities.h"
#include "Utils/StringHelpers.h"
#include "Trainer/Trainer.h"
#include "Trainer/Trainer7LGPE.h"
#include "Trainer/Trainer8SWSH.h"
#include "Trainer/Trainer9LZA.h"
#include "Trainer/Trainer9SV.h"
#include "Trainer/Trainer8LA.h"
#include "Trainer/Trainer8BDSP.h"
#include "Encryption/Encryption.h"
#include "Enums/GameVersion.h"

using namespace Utils;
using namespace Trainer;
using namespace Encryption;
using namespace Enums;

namespace Save {
    // ========================================
    // Generic Functions (Auto-detect game)
    // ========================================

    TrainerVariant readTrainerInfo(const char* backupDir, u64 titleId) {
        /**
         * Auto-detects the game version and calls the appropriate reading function.
         *
         * This function determines which game is being loaded based on the title ID,
         * then calls the game-specific reading function with the correct save file name
         * and decryption method.
         */

        GameVersion version = getGameVersion(titleId);
        GameVersion group = getGameGroup(version);

        char buffer[512];
        snprintf(buffer, sizeof(buffer), "Detected game: %s (Group: %s)",
            getGameVersionName(version).c_str(), getGameVersionName(group).c_str());
        Utils::logInfoToFile(buffer);

        switch (group) {
            case GameVersion::GG:  // Let's Go Pikachu/Eevee
                return readTrainerInfoLetsGo(backupDir);

            case GameVersion::SWSH:  // Sword/Shield
                return readTrainerInfoSwSh(backupDir);

            case GameVersion::ZA:
                return readTrainerInfoLZA(backupDir);

            case GameVersion::SV:  // Scarlet/Violet — dedicated Gen 9 class (packed, no-gap slots)
                return readTrainerInfoSV(backupDir);

            case GameVersion::PLA:  // Legends: Arceus — PA8 (read path)
                return readTrainerInfoLA(backupDir);
            case GameVersion::BDSP: // Brilliant Diamond/Shining Pearl — PB8 (flat read path)
                return readTrainerInfoBDSP(backupDir);

            case GameVersion::FRLG: // FireRed/LeafGreen — PK3 (GBA section-based read path)
                return readTrainerInfoFRLG(backupDir);

            default:
                logErrorToFile("Unsupported game version");
                // Return empty Trainer7 as fallback (better error handling needed)
                return Trainer::Trainer7LGPE(std::vector<Block>());
        }
    }

    bool saveTrainerInfo(Trainer::Trainer& trainer, const char* backupDir, u64 titleId, AccountUid userUid, bool injectToTitle) {
        /**
         * Auto-detects the game version and calls the appropriate saving function.
         * Uses virtual getGameGroup() method to determine concrete type without RTTI.
         */

        GameVersion version = getGameVersion(titleId);
        GameVersion group = getGameGroup(version);

        char buffer[512];
        snprintf(buffer, sizeof(buffer), "Saving for game: %s (Group: %s)",
            getGameVersionName(version).c_str(), getGameVersionName(group).c_str());
        Utils::logInfoToFile(buffer);

        GameVersion trainerGroup = trainer.getGameGroup();

        if (trainerGroup == GameVersion::GG) {
            // Let's Go - cast is safe because we checked the type via virtual method
            return saveTrainerInfoLetsGo(static_cast<Trainer::Trainer7LGPE&>(trainer), backupDir, titleId, userUid, injectToTitle);
        } else if (trainerGroup == GameVersion::SWSH) {
            // Sword/Shield - cast is safe because we checked the type via virtual method
            return saveTrainerInfoSwSh(static_cast<Trainer::Trainer8SWSH&>(trainer), backupDir, titleId, userUid, injectToTitle);
        } else if (trainerGroup == GameVersion::ZA) {
            return saveTrainerInfoLZA(static_cast<Trainer9LZA&>(trainer), backupDir, titleId, userUid, injectToTitle);
        } else if (trainerGroup == GameVersion::SV) {
            return saveTrainerInfoSV(static_cast<Trainer9SV&>(trainer), backupDir, titleId, userUid, injectToTitle);
        } else if (trainerGroup == GameVersion::PLA) {
            return saveTrainerInfoLA(static_cast<Trainer8LA&>(trainer), backupDir, titleId, userUid, injectToTitle);
        } else if (trainerGroup == GameVersion::BDSP) {
            return saveTrainerInfoBDSP(static_cast<Trainer8BDSP&>(trainer), backupDir, titleId, userUid, injectToTitle);
        } else if (trainerGroup == GameVersion::FRLG) {
            return saveTrainerInfoFRLG(static_cast<Trainer3FRLG&>(trainer), backupDir, titleId, userUid, injectToTitle);
        } else {
            logErrorToFile("Unsupported trainer type");
            return false;
        }
    }

    // ========================================
    // Game-Specific Functions - Let's Go
    // ========================================

    Trainer7LGPE readTrainerInfoLetsGo(const char* backupDir) {
        /**
         * Reads Pokemon Let's Go Pikachu/Eevee save file.
         *
         * Let's Go save format:
         * - File: savedata.bin (1,048,576 bytes = 1MB exactly)
         * - Active save data: First 0xB8800 bytes (757,760 bytes)
         * - Backup area follows after
         * - No external hash like SWSH
         *
         * The save data is NOT encrypted at file level like SWSH.
         * Individual Pokemon data IS encrypted using Gen 6/7 algorithm.
         */

        char savePath[512];
        snprintf(savePath, sizeof(savePath), "%s/savedata.bin", backupDir);

        char buffer[LOG_BUFFER_SIZE];
        snprintf(buffer, sizeof(buffer), "Reading Let's Go save from: %s", savePath);
        Utils::logInfoToFile(buffer);

        size_t fileSize = 0;
        uint8_t* file = readAllBytes(savePath, &fileSize);

        if (file == nullptr) {
            logErrorToFile("Failed to read Let's Go save file");
            return Trainer7LGPE(std::vector<Block>());
        }

        char fileSizeBuffer[512];
        snprintf(fileSizeBuffer, sizeof(fileSizeBuffer), "0x%016llX", static_cast<unsigned long long>(fileSize));
        logInfoToFile("Filesize", fileSizeBuffer);

        // Expected size: 1,048,576 bytes (1MB)
        if (fileSize < SAVE_SIZE7_LGPE) {
            logErrorToFile("Let's Go save file is too small");
            delete[] file;
            return Trainer7LGPE(std::vector<Block>());
        }

        // Extract the active save area (first SAVE_SIZE7_LGPE bytes)
        std::vector<uint8_t> saveData(file, file + SAVE_SIZE7_LGPE);

        std::vector<Block> blocks = createBlocksFromSaveData7LGPE(saveData);

        snprintf(buffer, sizeof(buffer), "Created %zu blocks from Let's Go save data", blocks.size());
        logInfoToFile(buffer);

        Trainer7LGPE trainer(std::move(blocks));

        delete[] file;
        return trainer;
    }

    bool saveTrainerInfoLetsGo(Trainer7LGPE& trainer, const char* backupDir, u64 titleId, AccountUid userUid, bool injectToTitle) {
        /**
         * Saves a Pokemon Let's Go Pikachu/Eevee save.
         *
         * LGPE isn't encrypted at the file level (only individual Pokemon are). We apply the
         * edits back into the trainer's blocks, re-read the ORIGINAL 1MB file (the trainer only
         * kept a few blocks, so we need the untouched blocks + the BEEF checksum footer + the
         * padding intact), patch the edited blocks in place, recompute their CRC-16/ARC block
         * checksums into the footer, and write the whole file back.
         */

        // Create ModifiedSave directory
        char modifiedSaveDir[512];
        snprintf(modifiedSaveDir, sizeof(modifiedSaveDir), "%s/ModifiedSave", backupDir);
        if (mkdir(modifiedSaveDir, 0777) != 0 && errno != EEXIST) {
            logErrorToFile("Failed to create ModifiedSave directory", modifiedSaveDir);
            return false;
        }

        // Apply edits into the trainer's blocks: lay down all storage, then overlay party.
        trainer.updateItemBlock();   // serialize inventory back into the MY_ITEM block
        trainer.updateBoxBlock();    // full unified 1000-slot storage
        trainer.updatePartyBlock();  // overlay party members at their storage indices

        // Re-read the original full save file.
        char srcPath[512];
        snprintf(srcPath, sizeof(srcPath), "%s/savedata.bin", backupDir);
        size_t fileSize = 0;
        uint8_t* file = readAllBytes(srcPath, &fileSize);
        if (file == nullptr) {
            logErrorToFile("Failed to re-read Let's Go save for writing", srcPath);
            return false;
        }
        if (fileSize < SAVE_SIZE7_LGPE) {
            logErrorToFile("Let's Go save file too small to write");
            delete[] file;
            return false;
        }

        std::vector<uint8_t> raw(file, file + fileSize);
        delete[] file;

        // Patch edited blocks + recompute block checksums into the BEEF footer.
        trainer.updateTrainerInfoBlock();   // money / OT name; mutates blocks before the copy
        trainer.updatePokedexBlock();       // Zukan flags for everything in storage; also before the copy
        writeBlocksToSaveData7LGPE(raw, trainer.getBlocks());

        // Write the full file to ModifiedSave/savedata.bin.
        char savePath[1024];
        snprintf(savePath, sizeof(savePath), "%s/savedata.bin", modifiedSaveDir);
        FILE* outFile = fopen(savePath, "wb");
        if (!outFile) {
            logErrorToFile("Failed to open file for writing", savePath);
            return false;
        }
        size_t written = fwrite(raw.data(), 1, raw.size(), outFile);
        fclose(outFile);
        if (written != raw.size()) {
            logErrorToFile("Failed to write complete save file", savePath);
            return false;
        }

        logInfoToFile("Successfully wrote modified Let's Go save", savePath);

        if (injectToTitle) {
            logInfoToFile("Restoring modified Let's Go save to game save device...");
            std::vector<std::string> saveFiles = {"savedata.bin"};
            if (!restoreModifiedSave(userUid, titleId, modifiedSaveDir, backupDir, saveFiles)) {
                logErrorToFile("Failed to restore modified Let's Go save to game");
                return false;
            }
        } else {
            logInfoToFile("Not injecting to the game save - Let's Go save written to the backup only");
        }

        return true;
    }

    // ========================================
    // Game-Specific Functions - Sword/Shield
    // ========================================

    Trainer8SWSH readTrainerInfoSwSh(const char* backupDir) {
        char mainPath[512];
        snprintf(mainPath, sizeof(mainPath), "%s/main", backupDir);

        char buffer[LOG_BUFFER_SIZE];
        snprintf(buffer, sizeof(buffer), "Reading trainer info from: %s", mainPath);
        logInfoToFile(buffer);

        size_t fileSize = 0;
        uint8_t* file = readAllBytes(mainPath, &fileSize);

        char fileSizeBuffer[512];
        snprintf(fileSizeBuffer, sizeof(fileSizeBuffer), "0x%016llX", static_cast<unsigned long long>(fileSize));
        // Filesize should be 1,603,146 bytes (size)
        logInfoToFile("Filesize", fileSizeBuffer);

        std::vector<Block> blocks = decrypt(file, fileSize);
        Trainer8SWSH trainer(blocks);

        delete[] file;
        return trainer;
    }

    bool saveTrainerInfoSwSh(Trainer8SWSH& trainer, const char* backupDir, u64 titleId, AccountUid userUid, bool injectToTitle) {
        // Create ModifiedSave directory
        char modifiedSaveDir[512];
        snprintf(modifiedSaveDir, sizeof(modifiedSaveDir), "%s/ModifiedSave", backupDir);

        if (mkdir(modifiedSaveDir, 0777) != 0 && errno != EEXIST) {
            logErrorToFile("Failed to create ModifiedSave directory", modifiedSaveDir);
            return false;
        }

        trainer.updateItemBlock();

        trainer.updatePartyBlock();

        trainer.updateBoxBlock();
        trainer.updateBoxNameBlock();   // must precede this game's checksum/hash pass
        trainer.updateCurrentBoxBlock();

        // Encrypt the modified blocks (hash is calculated automatically)
        trainer.updateTrainerInfoBlock();   // money / OT name; before the hash pass
        trainer.updatePokedexBlock();       // Zukan flags for everything in storage; before the hash pass
        std::vector<uint8_t> encryptedData = encrypt(trainer.getBlocks());

        // Write to file
        char savePath[1024];
        snprintf(savePath, sizeof(savePath), "%s/main", modifiedSaveDir);

        FILE* outFile = fopen(savePath, "wb");
        if (!outFile) {
            logErrorToFile("Failed to open file for writing", savePath);
            return false;
        }

        size_t written = fwrite(encryptedData.data(), 1, encryptedData.size(), outFile);
        fclose(outFile);

        if (written != encryptedData.size()) {
            logErrorToFile("Failed to write complete save file", savePath);
            return false;
        }

        std::string successMsg = std::string("Successfully saved modified save to: ") + savePath;
        logInfoToFile(successMsg.c_str());

        // Only write into the real game save when THIS save was told to inject
        if (injectToTitle) {
            logInfoToFile("Restoring modified save to game save device...");
            std::vector<std::string> saveFiles = {"main", "backup", "poke_trade"};
            if (!restoreModifiedSave(userUid, titleId, modifiedSaveDir, backupDir, saveFiles)) {
                logErrorToFile("Failed to restore modified save to game");
                return false;
            }
        } else {
            logInfoToFile("Not injecting to the game save - written to the backup/ModifiedSave only");
        }

        return true;
    }

    // ========================================
    // Game-Specific Functions - Legends: Z-A
    // ========================================

    Trainer9LZA readTrainerInfoLZA(const char* backupDir) {
        char mainPath[512];
        snprintf(mainPath, sizeof(mainPath), "%s/main", backupDir);

        char buffer[LOG_BUFFER_SIZE];
        snprintf(buffer, sizeof(buffer), "Reading trainer info from: %s", mainPath);
        logInfoToFile(buffer);

        size_t fileSize = 0;
        uint8_t* file = readAllBytes(mainPath, &fileSize);

        std::vector<Block> blocks = decrypt(file, fileSize);
        Trainer9LZA trainer(blocks);

        delete[] file;
        return trainer;
    }

    Trainer9SV readTrainerInfoSV(const char* backupDir) {
        char mainPath[512];
        snprintf(mainPath, sizeof(mainPath), "%s/main", backupDir);

        char buffer[LOG_BUFFER_SIZE];
        snprintf(buffer, sizeof(buffer), "Reading trainer info from: %s", mainPath);
        logInfoToFile(buffer);

        size_t fileSize = 0;
        uint8_t* file = readAllBytes(mainPath, &fileSize);

        std::vector<Block> blocks = decrypt(file, fileSize);
        Trainer9SV trainer(blocks);

        delete[] file;
        return trainer;
    }

    Trainer8LA readTrainerInfoLA(const char* backupDir) {
        char mainPath[512];
        snprintf(mainPath, sizeof(mainPath), "%s/main", backupDir);

        char buffer[LOG_BUFFER_SIZE];
        snprintf(buffer, sizeof(buffer), "Reading trainer info from: %s", mainPath);
        logInfoToFile(buffer);

        size_t fileSize = 0;
        uint8_t* file = readAllBytes(mainPath, &fileSize);

        std::vector<Block> blocks = decrypt(file, fileSize);
        Trainer8LA trainer(blocks);

        delete[] file;
        return trainer;
    }

    Trainer8BDSP readTrainerInfoBDSP(const char* backupDir) {
        char mainPath[512];
        snprintf(mainPath, sizeof(mainPath), "%s/SaveData.bin", backupDir);

        char buffer[LOG_BUFFER_SIZE];
        snprintf(buffer, sizeof(buffer), "Reading BDSP trainer info from: %s", mainPath);
        logInfoToFile(buffer);

        size_t fileSize = 0;
        uint8_t* file = readAllBytes(mainPath, &fileSize);

        // BDSP is a FLAT blob (no SwishCrypto) — hand the raw bytes straight to Trainer8BDSP.
        std::vector<uint8_t> data(file, file + fileSize);
        Trainer8BDSP trainer(std::move(data));

        delete[] file;
        return trainer;
    }

    bool saveTrainerInfoBDSP(Trainer8BDSP& trainer, const char* backupDir, u64 titleId, AccountUid userUid, bool injectToTitle) {
        char modifiedSaveDir[512];
        snprintf(modifiedSaveDir, sizeof(modifiedSaveDir), "%s/ModifiedSave", backupDir);
        if (mkdir(modifiedSaveDir, 0777) != 0 && errno != EEXIST) {
            logErrorToFile("Failed to create ModifiedSave directory", modifiedSaveDir);
            return false;
        }

        // BDSP is a FLAT blob (no SwishCrypto). Re-serialize party + box into the buffer, then
        // recompute the whole-file MD5 — after which getSaveData() IS the final on-disk save. Write
        // both SaveData.bin and its Backup.bin mirror so the game loads the edits either way.
        trainer.updateItemBlock();
        trainer.updatePartyBlock();
        trainer.updateBoxBlock();
        trainer.updateBoxNameBlock();   // must precede this game's checksum/hash pass
        trainer.updateCurrentBoxBlock();
        trainer.updateTrainerInfoBlock();   // money / OT name; before the MD5 hash pass
        trainer.updatePokedexBlock();       // Zukan flags for everything in storage; before the hash pass
        trainer.recomputeHash();
        const std::vector<uint8_t>& saveData = trainer.getSaveData();

        const char* fileNames[] = { "SaveData.bin", "Backup.bin" };
        for (const char* fname : fileNames) {
            char savePath[1024];
            snprintf(savePath, sizeof(savePath), "%s/%s", modifiedSaveDir, fname);
            FILE* outFile = fopen(savePath, "wb");
            if (!outFile) {
                logErrorToFile("Failed to open file for writing", savePath);
                return false;
            }
            size_t written = fwrite(saveData.data(), 1, saveData.size(), outFile);
            fclose(outFile);
            if (written != saveData.size()) {
                logErrorToFile("Failed to write complete save file", savePath);
                return false;
            }
        }
        logInfoToFile("Successfully wrote BDSP ModifiedSave (SaveData.bin + Backup.bin)");

        if (injectToTitle) {
            logInfoToFile("Restoring modified BDSP save to game save device...");
            std::vector<std::string> saveFiles = { "SaveData.bin", "Backup.bin" };
            if (!restoreModifiedSave(userUid, titleId, modifiedSaveDir, backupDir, saveFiles)) {
                logErrorToFile("Failed to restore modified save to game");
                return false;
            }
        } else {
            logInfoToFile("Not injecting to the game save - written to the backup/ModifiedSave only");
        }
        return true;
    }

    bool saveTrainerInfoLA(Trainer8LA& trainer, const char* backupDir, u64 titleId, AccountUid userUid, bool injectToTitle) {
        // Create ModifiedSave directory
        char modifiedSaveDir[512];
        snprintf(modifiedSaveDir, sizeof(modifiedSaveDir), "%s/ModifiedSave", backupDir);

        if (mkdir(modifiedSaveDir, 0777) != 0 && errno != EEXIST) {
            logErrorToFile("Failed to create ModifiedSave directory", modifiedSaveDir);
            return false;
        }

        // Re-serialize the edited party + box blocks, then encrypt (hash is computed inside).
        // updateItemBlock is a no-op stub for LA (item write deferred).
        trainer.updateItemBlock();
        trainer.updatePartyBlock();
        trainer.updateBoxBlock();
        trainer.updateBoxNameBlock();   // must precede this game's checksum/hash pass
        trainer.updateCurrentBoxBlock();
        trainer.updateTrainerInfoBlock();   // money / OT name; before the hash pass
        trainer.updatePokedexBlock();       // research + statistics entries; before the hash pass
        std::vector<uint8_t> encryptedData = encrypt(trainer.getBlocks());

        char savePath[1024];
        snprintf(savePath, sizeof(savePath), "%s/main", modifiedSaveDir);

        FILE* outFile = fopen(savePath, "wb");
        if (!outFile) {
            logErrorToFile("Failed to open file for writing", savePath);
            return false;
        }

        size_t written = fwrite(encryptedData.data(), 1, encryptedData.size(), outFile);
        fclose(outFile);

        if (written != encryptedData.size()) {
            logErrorToFile("Failed to write complete save file", savePath);
            return false;
        }

        std::string successMsg = std::string("Successfully saved modified save to: ") + savePath;
        logInfoToFile(successMsg.c_str());

        if (injectToTitle) {
            logInfoToFile("Restoring modified save to game save device...");
            std::vector<std::string> saveFiles = {"main"};
            if (!restoreModifiedSave(userUid, titleId, modifiedSaveDir, backupDir, saveFiles)) {
                logErrorToFile("Failed to restore modified save to game");
                return false;
            }
        } else {
            logInfoToFile("Not injecting to the game save - written to the backup/ModifiedSave only");
        }

        return true;
    }

    bool saveTrainerInfoLZA(Trainer9LZA& trainer, const char* backupDir, u64 titleId, AccountUid userUid, bool injectToTitle) {
        // Create ModifiedSave directory
        char modifiedSaveDir[512];
        snprintf(modifiedSaveDir, sizeof(modifiedSaveDir), "%s/ModifiedSave", backupDir);

        if (mkdir(modifiedSaveDir, 0777) != 0 && errno != EEXIST) {
            logErrorToFile("Failed to create ModifiedSave directory", modifiedSaveDir);
            return false;
        }

        trainer.updateItemBlock();

        trainer.updatePartyBlock();

        trainer.updateBoxBlock();
        trainer.updateBoxNameBlock();   // must precede this game's checksum/hash pass
        trainer.updateCurrentBoxBlock();

        // Encrypt the modified blocks (hash is calculated automatically)
        trainer.updateTrainerInfoBlock();   // money / OT name; before the hash pass
        trainer.updatePokedexBlock();       // Zukan flags for everything in storage; before the hash pass
        std::vector<uint8_t> encryptedData = encrypt(trainer.getBlocks());

        // Write to file
        char savePath[1024];
        snprintf(savePath, sizeof(savePath), "%s/main", modifiedSaveDir);

        FILE* outFile = fopen(savePath, "wb");
        if (!outFile) {
            logErrorToFile("Failed to open file for writing", savePath);
            return false;
        }

        size_t written = fwrite(encryptedData.data(), 1, encryptedData.size(), outFile);
        fclose(outFile);

        if (written != encryptedData.size()) {
            logErrorToFile("Failed to write complete save file", savePath);
            return false;
        }

        std::string successMsg = std::string("Successfully saved modified save to: ") + savePath;
        logInfoToFile(successMsg.c_str());

        // Only write into the real game save when THIS save was told to inject
        if (injectToTitle) {
            logInfoToFile("Restoring modified save to game save device...");
            std::vector<std::string> saveFiles = {"main"};
            if (!restoreModifiedSave(userUid, titleId, modifiedSaveDir, backupDir, saveFiles)) {
                logErrorToFile("Failed to restore modified save to game");
                return false;
            }
        } else {
            logInfoToFile("Not injecting to the game save - written to the backup/ModifiedSave only");
        }

        return true;
    }

    bool saveTrainerInfoSV(Trainer9SV& trainer, const char* backupDir, u64 titleId, AccountUid userUid, bool injectToTitle) {
        // Create ModifiedSave directory
        char modifiedSaveDir[512];
        snprintf(modifiedSaveDir, sizeof(modifiedSaveDir), "%s/ModifiedSave", backupDir);

        if (mkdir(modifiedSaveDir, 0777) != 0 && errno != EEXIST) {
            logErrorToFile("Failed to create ModifiedSave directory", modifiedSaveDir);
            return false;
        }

        // Re-serialize the edited blocks (item, party, box), then encrypt (hash is computed inside).
        trainer.updateItemBlock();
        trainer.updatePartyBlock();
        trainer.updateBoxBlock();
        trainer.updateBoxNameBlock();   // must precede this game's checksum/hash pass
        trainer.updateCurrentBoxBlock();
        trainer.updateTrainerInfoBlock();   // money / OT name; before the hash pass
        trainer.updatePokedexBlock();       // Zukan flags for everything in storage; before the hash pass
        std::vector<uint8_t> encryptedData = encrypt(trainer.getBlocks());

        char savePath[1024];
        snprintf(savePath, sizeof(savePath), "%s/main", modifiedSaveDir);

        FILE* outFile = fopen(savePath, "wb");
        if (!outFile) {
            logErrorToFile("Failed to open file for writing", savePath);
            return false;
        }

        size_t written = fwrite(encryptedData.data(), 1, encryptedData.size(), outFile);
        fclose(outFile);

        if (written != encryptedData.size()) {
            logErrorToFile("Failed to write complete save file", savePath);
            return false;
        }

        std::string successMsg = std::string("Successfully saved modified save to: ") + savePath;
        logInfoToFile(successMsg.c_str());

        if (injectToTitle) {
            logInfoToFile("Restoring modified save to game save device...");
            std::vector<std::string> saveFiles = {"main"};
            if (!restoreModifiedSave(userUid, titleId, modifiedSaveDir, backupDir, saveFiles)) {
                logErrorToFile("Failed to restore modified save to game");
                return false;
            }
        } else {
            logInfoToFile("Not injecting to the game save - written to the backup/ModifiedSave only");
        }

        return true;
    }

    // ========================================
    // Game-Specific Functions - FireRed/LeafGreen (Gen 3 GBA)
    // ========================================

    // Locate the GBA save inside a backup dir: prefer an exactly-128 KiB file, else any *.sav. Handles
    // Checkpoint exports ("FireRed_e.sav") and any mount name without hardcoding a filename.
    static std::string findGen3SaveFile(const char* dir) {
        DIR* d = opendir(dir);
        if (!d) return "";
        std::string best, anySav;
        struct dirent* ent;
        while ((ent = readdir(d)) != nullptr) {
            std::string name = ent->d_name;
            if (name == "." || name == ".." || name == "ModifiedSave") continue;
            char full[1024];
            snprintf(full, sizeof(full), "%s/%s", dir, name.c_str());
            struct stat st;
            if (stat(full, &st) != 0 || !S_ISREG(st.st_mode)) continue;
            if (static_cast<size_t>(st.st_size) == Trainer::FRLG_SAVE_SIZE) { best = name; break; }
            if (name.size() > 4 && name.compare(name.size() - 4, 4, ".sav") == 0) anySav = name;
        }
        closedir(d);
        return !best.empty() ? best : anySav;
    }

    Trainer3FRLG readTrainerInfoFRLG(const char* backupDir) {
        std::string fileName = findGen3SaveFile(backupDir);
        if (fileName.empty()) {
            logErrorToFile("No FRLG save (128 KiB / *.sav) found in backup dir", backupDir);
            return Trainer3FRLG(std::vector<uint8_t>(), "");
        }

        char savePath[1024];
        snprintf(savePath, sizeof(savePath), "%s/%s", backupDir, fileName.c_str());
        logInfoToFile("Reading FRLG save from", savePath);

        size_t fileSize = 0;
        uint8_t* file = readAllBytes(savePath, &fileSize);
        if (file == nullptr) {
            logErrorToFile("Failed to read FRLG save file", savePath);
            return Trainer3FRLG(std::vector<uint8_t>(), fileName);
        }
        std::vector<uint8_t> data(file, file + fileSize);
        delete[] file;

        char szBuf[64];
        snprintf(szBuf, sizeof(szBuf), "0x%zX", fileSize);
        logInfoToFile("FRLG filesize", szBuf);

        return Trainer3FRLG(std::move(data), fileName);
    }

    bool saveTrainerInfoFRLG(Trainer3FRLG& trainer, const char* backupDir, u64 titleId, AccountUid userUid, bool injectToTitle) {
        char modifiedSaveDir[512];
        snprintf(modifiedSaveDir, sizeof(modifiedSaveDir), "%s/ModifiedSave", backupDir);
        if (mkdir(modifiedSaveDir, 0777) != 0 && errno != EEXIST) {
            logErrorToFile("Failed to create ModifiedSave directory", modifiedSaveDir);
            return false;
        }

        // Apply edits into the raw save, then recompute every sector checksum.
        trainer.updateItemBlock();
        trainer.updateBoxBlock();
        trainer.updateBoxNameBlock();   // must precede this game's checksum/hash pass
        trainer.updateCurrentBoxBlock();
        trainer.updatePartyBlock();
        trainer.updatePokedexBlock();       // seen/caught for everything now in storage; before the checksums
        trainer.updateTrainerInfoBlock();   // money / OT name; before the sector checksums
        trainer.finalizeChecksums();

        const std::string name = trainer.fileName().empty() ? std::string("save.sav") : trainer.fileName();
        char savePath[1024];
        snprintf(savePath, sizeof(savePath), "%s/%s", modifiedSaveDir, name.c_str());

        const std::vector<uint8_t>& raw = trainer.getSaveData();
        FILE* outFile = fopen(savePath, "wb");
        if (!outFile) {
            logErrorToFile("Failed to open FRLG save for writing", savePath);
            return false;
        }
        const size_t written = fwrite(raw.data(), 1, raw.size(), outFile);
        fclose(outFile);
        if (written != raw.size()) {
            logErrorToFile("Failed to write complete FRLG save file", savePath);
            return false;
        }
        logInfoToFile("Successfully wrote modified FRLG save", savePath);

        if (injectToTitle) {
            logInfoToFile("Restoring modified FRLG save to game save device...");
            std::vector<std::string> saveFiles = { name };
            if (!restoreModifiedSave(userUid, titleId, modifiedSaveDir, backupDir, saveFiles)) {
                logErrorToFile("Failed to restore modified FRLG save to game");
                return false;
            }
        } else {
            logInfoToFile("Not injecting to the game save - FRLG save written to the backup only");
        }

        return true;
    }
}