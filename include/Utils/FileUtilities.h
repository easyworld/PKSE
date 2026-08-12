#ifndef UTILS_FILE_UTILITIES_H
#define UTILS_FILE_UTILITIES_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Utils/NXTypes.h"   // AccountUid/u64; <switch.h> on console. The IMPLEMENTATIONS
                             // here are Switch-only -- this just lets the header parse off-console
                             // so the save layer, which includes it transitively, can be compiled.

namespace Utils {
    uint8_t* readAllBytes(const char* path, size_t* outSize);
    bool copyDirectory(const char* srcPath, const char* destPath);
    bool copyFile(const char* srcPath, const char* destPath);
    bool deleteDirectoryRecursive(const char* path);
    // Copies the current game save into PKSE/{titleName}/. When `timestamped` is true a new
    // timestamped history folder is created; when false a single reusable "Working" folder is
    // overwritten (auto-backup disabled — no pile-up). Returns the created folder path, or "" on failure.
    std::string backupSaveData(AccountUid userUid, u64 titleId, std::string titleName, bool timestamped = true);
    // Copy a backup's save files onto the real game save. The backup directory IS the edited
    // save -- PKSE writes edits straight into it -- so there is no separate "modified" copy.
    // `primaryFile` is the one file a backup must hold; the game's `optionalFiles` beside it are
    // copied when present and passed over in silence when not, because whether they exist at all
    // depends on what the player has done in-game.
    bool restoreBackupToTitle(AccountUid userUid, u64 titleId, const char* backupDir,
                              const std::string& primaryFile,
                              const std::vector<std::string>& optionalFiles = {});
    std::string getTimestamp();
    std::vector<std::string> listBackupDirectories(const char* gameDirectory);
}

#endif
