#include <cstdio>

#include "UI/BackupSelectionScreen.h"
#include "UI/Common.h"
#include "Utils/HelperUtilities.h"
#include "Utils/FileUtilities.h"
#include "Globals.h"

using namespace Utils;

namespace UI {
    // UI Layout constants
    constexpr int LEFT_PANEL_X = 0;
    constexpr int LEFT_PANEL_Y = 70;

    BackupSelectionScreen::BackupSelectionScreen(u64 titleId, const std::string& titleName)
        : titleId(titleId), titleName(titleName), selectedIndex(0), backupSelected(false),
        createNewBackup(false), goBack(false), showDeleteConfirmation(false), deleteConfirmationIndex(-1) {

        char gameDirBuf[512];
        // snprintf(gameDirBuf, sizeof(gameDirBuf), "%s/%s", BASE_SAVE_DIRECTORY.c_str(),
        //          sanitizeTitleName(titleName).c_str());
        snprintf(gameDirBuf, sizeof(gameDirBuf), "%s/%s", BASE_SAVE_DIRECTORY.c_str(), titleName.c_str());
        gameDirectory = gameDirBuf;

        loadBackups();
    }

    void BackupSelectionScreen::loadBackups() {
        // Get list of timestamped backup directories
        std::vector<std::string> backupDirs = listBackupDirectories(gameDirectory.c_str());

        // Add "Load from Title (Create New Backup)" option
        BackupInfo newBackupOption;
        newBackupOption.timestamp = "";
        newBackupOption.displayName = "从游戏载入（创建新备份）";
        backups.push_back(newBackupOption);

        // Add existing backups
        for (const auto& timestamp : backupDirs) {
            BackupInfo info;
            info.timestamp = timestamp;
            info.displayName = formatTimestamp(timestamp);
            backups.push_back(info);
        }
    }

    std::string BackupSelectionScreen::formatTimestamp(const std::string& timestamp) {
        // Convert YYYYMMDD_HHMMSS to readable format: YYYY-MM-DD HH:MM:SS
        if (timestamp.length() != 15) return timestamp;

        std::string formatted;
        formatted += timestamp.substr(0, 4);  // YYYY
        formatted += "-";
        formatted += timestamp.substr(4, 2);  // MM
        formatted += "-";
        formatted += timestamp.substr(6, 2);  // DD
        formatted += " ";
        formatted += timestamp.substr(9, 2);  // HH
        formatted += ":";
        formatted += timestamp.substr(11, 2); // MM
        formatted += ":";
        formatted += timestamp.substr(13, 2); // SS

        return formatted;
    }

    void BackupSelectionScreen::update(const PadState& pad) {
        u64 kDown = padGetButtonsDown(&pad);

        // Handle delete confirmation dialog
        if (showDeleteConfirmation) {
            if (kDown & HidNpadButton_A) {
                // Confirm deletion
                deleteBackup(deleteConfirmationIndex);
                showDeleteConfirmation = false;
                deleteConfirmationIndex = -1;
                return;
            }
            if (kDown & HidNpadButton_B) {
                // Cancel deletion
                showDeleteConfirmation = false;
                deleteConfirmationIndex = -1;
                return;
            }
            return;  // Ignore other inputs while confirmation is shown
        }

        if (kDown & HidNpadButton_B) {
            goBack = true;
            return;
        }

        if (kDown & HidNpadButton_X) {
            // X button pressed - show delete confirmation for existing backups only
            if (selectedIndex > 0 && selectedIndex < (int)backups.size()) {
                showDeleteConfirmation = true;
                deleteConfirmationIndex = selectedIndex;
            }
        }

        if (kDown & HidNpadButton_Up) {
            if (selectedIndex > 0) {
                selectedIndex--;
            }
        }

        if (kDown & HidNpadButton_Down) {
            if (selectedIndex < (int)backups.size() - 1) {
                selectedIndex++;
            }
        }

        if (kDown & HidNpadButton_A) {
            if (selectedIndex == 0) {
                // First option: Load from Title (Create New Backup)
                createNewBackup = true;
                backupSelected = true;
            } else {
                // Existing backup selected
                createNewBackup = false;
                char backupPath[512];
                snprintf(backupPath, sizeof(backupPath), "%s/%s",
                    gameDirectory.c_str(), backups[selectedIndex].timestamp.c_str());
                selectedBackupPath = backupPath;
                backupSelected = true;
            }
        }
    }

    void BackupSelectionScreen::draw(PKSEFramebuffer& fb) {
        fb.clear(Colors::Background);

        // Title
        fb.drawText(LEFT_PANEL_X + 20, 40, "选择存档备份", Colors::Text);
        fb.drawText(LEFT_PANEL_X + 20, 70, titleName, Colors::TextDim);

        drawBackupList(fb);

        // Instructions
        if (selectedIndex > 0) {
            fb.drawText(LEFT_PANEL_X + 20, 650, "A: 选择  X: 删除  B: 返回", Colors::TextDim);
        } else {
            fb.drawText(LEFT_PANEL_X + 20, 650, "A: 选择  B: 返回", Colors::TextDim);
        }

        // Draw delete confirmation if active
        if (showDeleteConfirmation) {
            drawDeleteConfirmation(fb);
        }
    }

    void BackupSelectionScreen::drawBackupList(PKSEFramebuffer& fb) {
        int itemHeight = 50;
        int startY = LEFT_PANEL_Y + 60;

        for (size_t i = 0; i < backups.size(); i++) {
            int itemY = startY + (i * itemHeight);

            if ((int)i == selectedIndex) {
                fb.drawFilledRect(LEFT_PANEL_X + 10, itemY, 1240, itemHeight - 5, Colors::Selected);
            }

            std::string displayText = "> " + backups[i].displayName;
            fb.drawText(LEFT_PANEL_X + 30, itemY + 15, displayText, Colors::Text);
        }
    }

    void BackupSelectionScreen::drawDeleteConfirmation(PKSEFramebuffer& fb) {
        // Draw semi-transparent overlay
        constexpr int MODAL_WIDTH = 600;
        constexpr int MODAL_HEIGHT = 200;
        const int modalX = (fb.getWidth() - MODAL_WIDTH) / 2;
        const int modalY = (fb.getHeight() - MODAL_HEIGHT) / 2;

        // Draw modal background
        fb.drawFilledRect(modalX, modalY, MODAL_WIDTH, MODAL_HEIGHT, Colors::Panel);
        fb.drawRect(modalX, modalY, MODAL_WIDTH, MODAL_HEIGHT, Colors::Border);

        // Draw confirmation text
        fb.drawText(modalX + 30, modalY + 30, "删除备份？", Colors::Text);
        fb.drawText(modalX + 30, modalY + 70, "此操作无法撤销。", Colors::TextDim);

        if (deleteConfirmationIndex >= 0 && deleteConfirmationIndex < (int)backups.size()) {
            std::string backupName = backups[deleteConfirmationIndex].displayName;
            fb.drawText(modalX + 30, modalY + 100, backupName, Colors::Text);
        }

        // Draw buttons
        fb.drawText(modalX + 30, modalY + 150, "A: 确认  B: 取消", Colors::TextDim);
    }

    void BackupSelectionScreen::deleteBackup(int index) {
        if (index <= 0 || index >= (int)backups.size()) {
            return;  // Can't delete the "Load from Title" option or invalid index
        }

        char backupPath[512];
        snprintf(backupPath, sizeof(backupPath), "%s/%s",
            gameDirectory.c_str(), backups[index].timestamp.c_str());

        // Delete the directory
        if (deleteDirectoryRecursive(backupPath)) {
            // Remove from backups list
            backups.erase(backups.begin() + index);

            // Adjust selected index if needed
            if (selectedIndex >= (int)backups.size()) {
                selectedIndex = (int)backups.size() - 1;
            }
        }
    }
}
