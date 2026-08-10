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

    std::string backupSaveData(AccountUid userUid, u64 titleId, std::string titleName, bool timestamped) {
        char titleBuf[32];
        snprintf(titleBuf, sizeof(titleBuf), "0x%016llX", static_cast<unsigned long long>(titleId));
        logInfoToFile("Pokemon titleId: ", titleBuf);
        logInfoToFile("Pokemon Title name: ", titleName.c_str());

        // Create base game directory: PKSE/{titleName}/
        char gameDirectory[512];
        snprintf(gameDirectory, sizeof(gameDirectory), "%s/%s", BASE_SAVE_DIRECTORY.c_str(), titleName.c_str());

        // History backup -> PKSE/{titleName}/{timestamp}/ ; auto-backup off -> reuse PKSE/{titleName}/Working/
        std::string folderName = timestamped ? getTimestamp() : std::string("Working");
        char backupDirectory[1024];
        snprintf(backupDirectory, sizeof(backupDirectory), "%s/%s", gameDirectory, folderName.c_str());

        logInfoToFile("Backup directory", backupDirectory);
        logInfoToFile("Backing up save for title", titleName.c_str());

        if (mkdir(BASE_SAVE_DIRECTORY.c_str(), 0777) != 0 && errno != EEXIST) {
            logErrorToFile("Failed to create base directory", BASE_SAVE_DIRECTORY.c_str());
            logErrorToFile("mkdir error", strerror(errno));
            return "";
        }
        if (mkdir(gameDirectory, 0777) != 0 && errno != EEXIST) {
            logErrorToFile("Failed to create game directory", gameDirectory);
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

    /**
     * Copy a backup's save files onto the real game save.
     *
     * The backup directory IS the edited save: PKSE writes edits straight into it. This used to
     * take a second "modified save" path as well, because edits went to a `ModifiedSave`
     * subdirectory and only the inject path ever read them back out -- which meant saving to a
     * backup silently changed nothing the loader would ever see. That directory is gone, so the
     * two paths collapsed into one and the second copy pass with them.
     */
    bool restoreBackupToTitle(AccountUid userUid, u64 titleId, const char* backupDir, std::vector<std::string> saveFiles) {
        char buffer[LOG_BUFFER_SIZE];
        logInfoToFile("Restoring backup to game save", backupDir);

        Result result = fsdevMountSaveData("save", titleId, userUid);

        if (R_FAILED(result)) {
            snprintf(buffer, sizeof(buffer), "fsdevMountSaveData failed for titleId 0x%016lX: 0x%x", titleId, result);
            logErrorToFile(buffer);
            return false;
        }

        logInfoToFile("Successfully mounted save:/ for restore");

        // Copy the named save files only, never subdirectories. The list is per-game:
        // Sword/Shield has main, backup and poke_trade; most others are a single file.
        logInfoToFile("Copying backup save files to save:/", backupDir);

        bool copyAllSuccess = true;

        for (size_t i = 0; i < saveFiles.size(); i++) {
            char srcPath[512];
            char destPath[512];
            snprintf(srcPath, sizeof(srcPath), "%s/%s", backupDir, saveFiles[i].c_str());
            snprintf(destPath, sizeof(destPath), "save:/%s", saveFiles[i].c_str());

            if (!copyFile(srcPath, destPath)) {
                snprintf(buffer, sizeof(buffer), "Failed to copy %s", saveFiles[i].c_str());
                logErrorToFile(buffer);
                copyAllSuccess = false;
                break;
            }
        }

        if (!copyAllSuccess) {
            logErrorToFile("Failed to copy backup files to the game save");
            fsdevUnmountDevice("save");
            return false;
        }

        // No second pass. The loop above already copied the edited primary file, because the
        // backup directory holds the edits -- there is no separate "modified" copy to overlay.
        if (saveFiles.empty()) {
            logErrorToFile("No save files specified for restore");
            fsdevUnmountDevice("save");
            return false;
        }

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