#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>
#include <string>
#include <algorithm>

#include <switch.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "Globals.h"
#include "Utils/Logger.h"

namespace Utils {
    uint8_t* readAllBytes(const char* path, size_t* outSize) {
        FILE* file = fopen(path, "rb");
        if (!file) {
            perror("Failed to open file");
            return NULL;
        }

        // Get the file size
        fseek(file, 0, SEEK_END);
        size_t fileSize = ftell(file);
        rewind(file);

        // Allocate memory to hold the file contents
        uint8_t* buffer = (unsigned char*)malloc(fileSize);
        if (!buffer) {
            perror("Failed to allocate memory");
            fclose(file);
            return NULL;
        }

        // Read the file contents into the buffer
        size_t bytesRead = fread(buffer, 1, fileSize, file);
        if (bytesRead != fileSize) {
            perror("Failed to read the entire file");
            free(buffer);
            fclose(file);
            return NULL;
        }

        fclose(file);

        // Return the buffer and the size of the file
        if (outSize)
            *outSize = fileSize;
        return buffer;
    }

    bool copyDirectoryRecursive(const char* srcPath, const char* destPath) {
        DIR* dir = opendir(srcPath);
        if (!dir) {
            logErrorToFile("Failed to open source directory", srcPath);
            logErrorToFile("opendir error", strerror(errno));
            return false;
        }

        bool overallSuccess = true;
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

            char srcFilePath[512];
            char destFilePath[512];
            snprintf(srcFilePath, sizeof(srcFilePath), "%s/%s", srcPath, entry->d_name);
            snprintf(destFilePath, sizeof(destFilePath), "%s/%s", destPath, entry->d_name);

            if (entry->d_type == DT_DIR) {
                // Recursively create and copy subdirectory
                if (mkdir(destFilePath, 0777) != 0 && errno != EEXIST) {
                    logErrorToFile("Failed to create subdirectory", destFilePath);
                    overallSuccess = false;
                } else {
                    if (!copyDirectoryRecursive(srcFilePath, destFilePath)) {
                        overallSuccess = false;
                    }
                }
            } else if (entry->d_type == DT_REG) {
                size_t size = 0;
                unsigned char* data = readAllBytes(srcFilePath, &size);
                if (!data) {
                    logErrorToFile("Failed to read file", srcFilePath);
                    overallSuccess = false;
                    continue;
                }

                FILE* out = fopen(destFilePath, "wb");
                if (!out) {
                    logErrorToFile("Failed to open for writing", destFilePath);
                    logErrorToFile("fopen error", strerror(errno));
                    free(data);
                    overallSuccess = false;
                    continue;
                }

                if (fwrite(data, 1, size, out) != size) {
                    logErrorToFile("Failed to write complete file", destFilePath);
                    overallSuccess = false;
                } else {
                    logInfoToFile("Successfully copied file", entry->d_name);
                    logInfoToFile("File size (bytes)", std::to_string(size).c_str());
                }
                fclose(out);
                free(data);
            }
        }
        closedir(dir);
        return overallSuccess;
    }

    bool copyDirectory(const char* srcPath, const char* destPath) {
        // Create destination directory if needed
        if (mkdir(destPath, 0777) != 0 && errno != EEXIST) {
            logErrorToFile("Failed to create destination directory", destPath);
            logErrorToFile("mkdir error", strerror(errno));
            return false;
        }
        logInfoToFile("Created/copied to destination directory", destPath);
        return copyDirectoryRecursive(srcPath, destPath);
    }

    bool copyFile(const char* srcPath, const char* destPath) {
        size_t size = 0;
        unsigned char* data = readAllBytes(srcPath, &size);
        if (!data) {
            logErrorToFile("Failed to read file", srcPath);
            return false;
        }

        FILE* out = fopen(destPath, "wb");
        if (!out) {
            logErrorToFile("Failed to open for writing", destPath);
            logErrorToFile("fopen error", strerror(errno));
            free(data);
            return false;
        }

        bool success = true;
        if (fwrite(data, 1, size, out) != size) {
            logErrorToFile("Failed to write complete file", destPath);
            success = false;
        } else {
            logInfoToFile("Successfully copied file to", destPath);
        }

        fclose(out);
        free(data);
        return success;
    }

    bool deleteDirectoryRecursive(const char* path) {
        DIR* dir = opendir(path);
        if (!dir) {
            logErrorToFile("Failed to open directory for deletion", path);
            return false;
        }

        bool success = true;
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            char fullPath[1024];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", path, entry->d_name);

            if (entry->d_type == DT_DIR) {
                // Recursively delete subdirectory
                if (!deleteDirectoryRecursive(fullPath)) {
                    success = false;
                }
            } else {
                // Delete file
                if (remove(fullPath) != 0) {
                    logErrorToFile("Failed to delete file", fullPath);
                    success = false;
                }
            }
        }
        closedir(dir);

        // Remove the directory itself
        if (rmdir(path) != 0) {
            logErrorToFile("Failed to remove directory", path);
            return false;
        }

        return success;
    }

    std::string getTimestamp() {
        time_t now = time(nullptr);
        struct tm* timeinfo = localtime(&now);

        char buffer[32];
        strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", timeinfo);
        return std::string(buffer);
    }

    static const char* backupTitleSlug(u64 titleId) {
        switch (titleId) {
            case 0x0100554023408000: return "Pokemon FireRed";
            case 0x010034D02340E000: return "Pokemon LeafGreen";
            case 0x010003F003A34000: return "Pokemon Lets Go Pikachu";
            case 0x0100187003A36000: return "Pokemon Lets Go Eevee";
            case 0x0100ABF008968000: return "Pokemon Sword";
            case 0x01008DB008C2C000: return "Pokemon Shield";
            case 0x0100000011D90000: return "Pokemon Brilliant Diamond";
            case 0x010018E011D92000: return "Pokemon Shining Pearl";
            case 0x01001F5010DFA000: return "Pokemon Legends Arceus";
            case 0x0100A3D008C5C000: return "Pokemon Scarlet";
            case 0x01008F6008C5E000: return "Pokemon Violet";
            case 0x0100F43008C44000: return "Pokemon Legends Z-A";
            default:                 return "Pokemon Unknown";
        }
    }

    std::string getBackupGameDirectory(u64 titleId) {
        char leaf[96];
        snprintf(leaf, sizeof(leaf), "%s [%016llX]", backupTitleSlug(titleId),
                 static_cast<unsigned long long>(titleId));
        return BASE_SAVE_DIRECTORY + "/" + leaf;
    }

    void migrateLegacyBackupDirectory(u64 titleId, const std::string& titleName) {
        const std::string legacy = BASE_SAVE_DIRECTORY + "/" + titleName;
        const std::string stable = getBackupGameDirectory(titleId);
        if (legacy == stable) return;

        struct stat legacyStat{};
        if (stat(legacy.c_str(), &legacyStat) != 0 || !S_ISDIR(legacyStat.st_mode)) return;

        struct stat stableStat{};
        if (stat(stable.c_str(), &stableStat) == 0) {
            logInfoToFile("Legacy backup directory left unchanged; stable directory already exists", legacy.c_str());
            return;
        }

        if (rename(legacy.c_str(), stable.c_str()) == 0) {
            logInfoToFile("Migrated legacy backup directory to", stable.c_str());
        } else {
            logErrorToFile("Could not migrate legacy backup directory", legacy.c_str());
            logErrorToFile("rename error", strerror(errno));
        }
    }

    std::string backupSaveData(AccountUid userUid, u64 titleId, std::string titleName, bool timestamped) {
        char titleBuf[32];
        snprintf(titleBuf, sizeof(titleBuf), "0x%016llX", static_cast<unsigned long long>(titleId));
        logInfoToFile("Pokemon titleId: ", titleBuf);
        logInfoToFile("Pokemon Title name: ", titleName.c_str());

        const std::string gameDirectory = getBackupGameDirectory(titleId);

        // History backup -> per-title/{timestamp}/ ; auto-backup off -> per-title/Working/
        std::string folderName = timestamped ? getTimestamp() : std::string("Working");
        char backupDirectory[1024];
        snprintf(backupDirectory, sizeof(backupDirectory), "%s/%s", gameDirectory.c_str(), folderName.c_str());

        logInfoToFile("Backup directory", backupDirectory);
        logInfoToFile("Backing up save for title", titleName.c_str());

        if (mkdir(BASE_SAVE_DIRECTORY.c_str(), 0777) != 0 && errno != EEXIST) {
            logErrorToFile("Failed to create base directory", BASE_SAVE_DIRECTORY.c_str());
            logErrorToFile("mkdir error", strerror(errno));
            return "";
        }
        if (mkdir(gameDirectory.c_str(), 0777) != 0 && errno != EEXIST) {
            logErrorToFile("Failed to create game directory", gameDirectory.c_str());
            logErrorToFile("mkdir error", strerror(errno));
            return "";
        }
        if (mkdir(backupDirectory, 0777) != 0 && errno != EEXIST) {
            logErrorToFile("Failed to create backup directory", backupDirectory);
            logErrorToFile("mkdir error", strerror(errno));
            return "";
        }

        char buffer[LOG_BUFFER_SIZE];

        Result result = fsdevMountSaveData("save", titleId, userUid);

        if (R_FAILED(result)) {
            snprintf(buffer, sizeof(buffer), "fsdevMountSaveData failed for titleId 0x%016lX: 0x%x", titleId, result);
            logErrorToFile(buffer);
            return "";
        }

        logInfoToFile("Successfully mounted save:/");

        bool copySuccess = copyDirectory("save:/", backupDirectory);

        fsdevUnmountDevice("save");

        if (copySuccess) {
            logInfoToFile("Backup completed successfully!");
            return std::string(backupDirectory);
        }
        logErrorToFile("Backup failed during file copying.");
        return "";
    }

    // Tells "the backup never held this file" apart from "the copy failed". Both reach copyFile as
    // the same failed fopen, but only the second one is an error worth reporting.
    static bool backupHasFile(const char* path) {
        struct stat st{};
        return stat(path, &st) == 0 && S_ISREG(st.st_mode);
    }

    /**
     * Copy a backup's save files onto the real game save.
     *
     * The backup directory IS the edited save: PKSE writes edits straight into it. This used to
     * take a second "modified save" path as well, because edits went to a `ModifiedSave`
     * subdirectory and only the inject path ever read them back out -- which meant saving to a
     * backup silently changed nothing the loader would ever see. That directory is gone, so the
     * two paths collapsed into one and the second copy pass with them.
     *
     * Only `primaryFile` has to exist. Which companion files sit beside it is a property of the
     * individual save, not of the game: Sword/Shield's `poke_trade` holds a Surprise Trade result
     * that has not been collected yet, so a save that never sent one simply has no such file. The
     * list used to be a flat per-game array of required names, which made that ordinary case fatal
     * -- the copy loop broke on the missing file and returned before `fsdevCommitDevice`, so the
     * writes already made to save:/ were dropped on unmount and the user's edit disappeared with a
     * "failed to copy" error naming a file they were never supposed to have.
     *
     * So an absent optional file is skipped and not logged as a failure. A file that IS there and
     * will not copy still stops the restore, before the commit, leaving the game save untouched.
     */
    bool restoreBackupToTitle(AccountUid userUid, u64 titleId, const char* backupDir,
                              const std::string& primaryFile,
                              const std::vector<std::string>& optionalFiles) {
        char buffer[LOG_BUFFER_SIZE];
        logInfoToFile("Restoring backup to game save", backupDir);

        if (primaryFile.empty()) {
            logErrorToFile("No primary save file specified for restore");
            return false;
        }

        // Checked before mounting: if the one required file is missing there is nothing this can
        // do, and bailing here means never having opened the game's save data at all.
        char primaryPath[512];
        snprintf(primaryPath, sizeof(primaryPath), "%s/%s", backupDir, primaryFile.c_str());
        if (!backupHasFile(primaryPath)) {
            snprintf(buffer, sizeof(buffer), "Backup has no %s to restore", primaryFile.c_str());
            logErrorToFile(buffer);
            return false;
        }

        Result result = fsdevMountSaveData("save", titleId, userUid);

        if (R_FAILED(result)) {
            snprintf(buffer, sizeof(buffer), "fsdevMountSaveData failed for titleId 0x%016lX: 0x%x", titleId, result);
            logErrorToFile(buffer);
            return false;
        }

        logInfoToFile("Successfully mounted save:/ for restore");

        // Copy the named save files only, never subdirectories.
        logInfoToFile("Copying backup save files to save:/", backupDir);

        char srcPath[512];
        char destPath[512];
        bool copyAllSuccess = true;

        snprintf(destPath, sizeof(destPath), "save:/%s", primaryFile.c_str());
        if (!copyFile(primaryPath, destPath)) {
            snprintf(buffer, sizeof(buffer), "Failed to copy %s", primaryFile.c_str());
            logErrorToFile(buffer);
            copyAllSuccess = false;
        }

        for (size_t i = 0; copyAllSuccess && i < optionalFiles.size(); i++) {
            snprintf(srcPath, sizeof(srcPath), "%s/%s", backupDir, optionalFiles[i].c_str());

            if (!backupHasFile(srcPath)) {
                // Ordinary: this save never produced the file. Whatever the game currently has
                // under that name stays where it is.
                logInfoToFile("Not in this backup, leaving the game's copy alone", optionalFiles[i].c_str());
                continue;
            }

            snprintf(destPath, sizeof(destPath), "save:/%s", optionalFiles[i].c_str());
            if (!copyFile(srcPath, destPath)) {
                snprintf(buffer, sizeof(buffer), "Failed to copy %s", optionalFiles[i].c_str());
                logErrorToFile(buffer);
                copyAllSuccess = false;
            }
        }

        if (!copyAllSuccess) {
            logErrorToFile("Failed to copy backup files to the game save");
            fsdevUnmountDevice("save");
            return false;
        }

        // No second pass. The copies above already wrote the edited primary file, because the
        // backup directory holds the edits -- there is no separate "modified" copy to overlay.

        // CRITICAL: Commit changes to the save device before unmounting
        // Without this, changes remain in memory buffers and are never written to disk
        logInfoToFile("Committing changes to save device...");

        Result commitResult = fsdevCommitDevice("save");
        if (R_FAILED(commitResult)) {
            snprintf(buffer, sizeof(buffer), "fsdevCommitDevice failed: 0x%x", commitResult);
            logErrorToFile(buffer);
            fsdevUnmountDevice("save");
            return false;
        }

        logInfoToFile("Successfully committed changes to save device");

        fsdevUnmountDevice("save");

        logInfoToFile("Backup restored to the game save successfully!");
        return true;
    }

    // True only for an auto-history folder shaped exactly like getTimestamp(): YYYYMMDD_HHMMSS.
    // User-named backups and the reusable "Working" copy are, by definition, everything else.
    static bool isTimestampName(const std::string& name) {
        if (name.length() != 15 || name[8] != '_') return false;
        for (size_t i = 0; i < name.length(); ++i) {
            if (i == 8) continue;
            if (name[i] < '0' || name[i] > '9') return false;
        }
        return true;
    }

    std::vector<std::string> listBackupDirectories(const char* gameDirectory) {
        std::vector<std::string> backupDirs;

        // A missing game directory just means "no backups for this title yet" — the ordinary state
        // before the first backup — so don't treat opendir failing as an error worth logging.
        DIR* dir = opendir(gameDirectory);
        if (!dir) {
            return backupDirs;
        }

        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            // "Working" is the internal reusable staging copy used when auto-backup is off, not a
            // user backup — keep it out of the picker so it never reads as one (C5).
            if (strcmp(entry->d_name, "Working") == 0) {
                continue;
            }

            // Every other subdirectory of the game folder IS a user-facing backup: an auto-history
            // timestamp or a user-named backup. The old code kept only the timestamp shape, which
            // hid every custom-named backup the destination picker can create.
            if (entry->d_type == DT_DIR) {
                backupDirs.push_back(entry->d_name);
            }
        }
        closedir(dir);

        // User-named backups first (alphabetical), then auto-history newest-first. A named backup is
        // a deliberate choice, so it belongs at the top rather than sorted into the middle of the
        // timestamps by ASCII accident.
        std::sort(backupDirs.begin(), backupDirs.end(), [](const std::string& a, const std::string& b) {
            const bool at = isTimestampName(a), bt = isTimestampName(b);
            if (at != bt) return !at;   // named before timestamped
            if (at)       return a > b; // both timestamps: lexicographically greatest (newest) first
            return a < b;               // both named: alphabetical
        });

        return backupDirs;
    }

}
