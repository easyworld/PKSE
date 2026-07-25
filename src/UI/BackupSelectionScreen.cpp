#include <algorithm>
#include <cstdio>

#include "UI/BackupSelectionScreen.h"
#include "UI/Common.h"
#include "UI/ScreenChrome.h"
#include "UI/TouchInput.h"
#include "UI/Dialogs/DialogFrame.h"
#include "Utils/HelperUtilities.h"
#include "Utils/FileUtilities.h"
#include "Globals.h"

using namespace Utils;

namespace UI {
    // UI Layout constants
    constexpr int CARD_X = 40;
    constexpr int CARD_Y = 84;
    constexpr int CARD_W = 1200;
    constexpr int LIST_ROW_H = 62;
    constexpr int LIST_MAX_VISIBLE = 8;   // rows that fit in the card before scrolling

    // Scroll window: keep the selected row visible (centered when scrolling).
    static int firstVisibleRow(int sel, int total) {
        if (total <= LIST_MAX_VISIBLE) return 0;
        int first = sel - LIST_MAX_VISIBLE / 2;
        if (first < 0) first = 0;
        if (first > total - LIST_MAX_VISIBLE) first = total - LIST_MAX_VISIBLE;
        return first;
    }

    BackupSelectionScreen::BackupSelectionScreen(u64 titleId, const std::string& titleName)
        : titleId(titleId), titleName(titleName), selectedIndex(0), backupSelected(false),
        createNewBackup(false), goBack(false), showDeleteConfirmation(false), deleteConfirmationIndex(-1) {

        char gameDirBuf[512];
        snprintf(gameDirBuf, sizeof(gameDirBuf), "%s/%s", BASE_SAVE_DIRECTORY.c_str(), titleName.c_str());
        gameDirectory = gameDirBuf;

        loadBackups();
    }

    void BackupSelectionScreen::loadBackups() {
        // Get list of timestamped backup directories
        std::vector<std::string> backupDirs = listBackupDirectories(gameDirectory.c_str());

        // Add the "从游戏载入" action. With auto-backup ON this creates a new timestamped
        // backup; with it OFF the load reuses a single "工作副本" copy (see UIManager::handleBackupSelection),
        // so label it honestly rather than always promising a new backup.
        BackupInfo newBackupOption;
        newBackupOption.timestamp = "";
        newBackupOption.displayName = g_autoBackupEnabled
            ? "从游戏载入（创建新备份）"
            : "从游戏载入（工作副本）";
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

    void BackupSelectionScreen::update(const PadState& pad, const TouchInput& touch) {
        u64 kDown = padGetButtonsDown(&pad) | navTouchButton(touch);   // nav-bar badges are tappable (#22)
        if (statusFrames > 0) --statusFrames;                          // expire the failure notice

        // Handle delete confirmation dialog
        if (showDeleteConfirmation) {
            // Tappable buttons (captured last frame): A = Delete, B = Cancel.
            if (touch.justPressed()) {
                auto in = [&](const DlgBtn& b) {
                    return touch.x() >= b.x && touch.x() < b.x + b.w &&
                           touch.y() >= b.y && touch.y() < b.y + b.h;
                };
                if (in(deleteDeleteBtn))      kDown |= HidNpadButton_A;
                else if (in(deleteCancelBtn)) kDown |= HidNpadButton_B;
            }
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

        // Touch: tap a backup tile to select + open it (account for the scroll window).
        if (touch.justPressed()) {
            const int startY = CARD_Y + 62, tileX = CARD_X + 14, tileW = CARD_W - 28;
            if (touch.x() >= tileX && touch.x() < tileX + tileW && touch.y() >= startY) {
                int visIdx = (touch.y() - startY) / LIST_ROW_H;
                int idx = firstVisibleRow(selectedIndex, (int)backups.size()) + visIdx;
                if (visIdx >= 0 && visIdx < LIST_MAX_VISIBLE && idx < (int)backups.size()) { selectedIndex = idx; kDown |= HidNpadButton_A; }
            }
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
        drawTitleBar(fb, titleName);

        // Leave a real gap under the card so the nav bar's shadow falls on the background, not on
        // the card edge. (Was a bare "- 46", which sat flush against the taller bar.)
        const int cardH = fb.getHeight() - CARD_Y - kNavBarH - 12;
        fb.drawCard(CARD_X, CARD_Y, CARD_W, cardH);
        fb.drawText(CARD_X + 18, CARD_Y + 14, "存档备份", Colors::Text, TextStyle::Heading);
        fb.drawHDivider(CARD_X + 18, CARD_Y + 50, CARD_W - 36);

        drawBackupList(fb);

        if (selectedIndex > 0) {
            drawNavBar(fb, "A：选择  |  X：删除  |  B：返回");
        } else {
            drawNavBar(fb, "A：选择  |  B：返回");
        }

        // Transient failure notice, centred just above the nav bar (same treatment as the editor's).
        if (statusFrames > 0 && !statusMessage.empty()) {
            int tw, th; fb.measureText(statusMessage, tw, th);
            const int padX = 18, bw = tw + padX * 2, bh = th + 14;
            const int bx = (fb.getWidth() - bw) / 2, by = fb.getHeight() - kNavBarH - bh - 12;
            fb.drawSoftShadow(bx, by, bw, bh, 8);
            fb.drawFilledRoundedRect(bx, by, bw, bh, 8, Colors::Panel);
            fb.drawRoundedRect(bx, by, bw, bh, 8, Colors::Orange, 2);
            fb.drawText(bx + padX, by + 7, statusMessage, Colors::Text);
        }

        if (showDeleteConfirmation) {
            drawDeleteConfirmation(fb);
        }
    }

    void BackupSelectionScreen::drawBackupList(PKSEFramebuffer& fb) {
        const int startY = CARD_Y + 62;
        const int total = (int)backups.size();
        const int first = firstVisibleRow(selectedIndex, total);
        const int last = std::min(total, first + LIST_MAX_VISIBLE);
        for (int i = first; i < last; i++) {
            int itemY = startY + (i - first) * LIST_ROW_H;
            bool selected = (i == selectedIndex);
            // The first row is the "创建新备份" action — accent it to stand out.
            drawHomeTile(fb, CARD_X + 14, itemY, CARD_W - 28, LIST_ROW_H - 10, backups[i].displayName, selected, (i == 0));
        }
        // Scrollbar on the card's right edge when the list overflows -- same thumb as the details editor.
        drawScrollbar(fb, CARD_X + CARD_W - 10, startY, LIST_MAX_VISIBLE * LIST_ROW_H,
                      total * LIST_ROW_H, first * LIST_ROW_H);
    }

    void BackupSelectionScreen::drawDeleteConfirmation(PKSEFramebuffer& fb) {
        constexpr int w = 560, h = 248;
        const int x = (fb.getWidth() - w) / 2;
        const int y = (fb.getHeight() - h) / 2;

        int cy = Dialogs::drawDialogFrame(fb, x, y, w, h, "删除备份？", Colors::Warning);
        fb.drawText(x + 24, cy, "此操作无法撤销。", Colors::TextDim);
        if (deleteConfirmationIndex >= 0 && deleteConfirmationIndex < (int)backups.size()) {
            fb.drawText(x + 24, cy + 28, backups[deleteConfirmationIndex].displayName, Colors::Text);
        }

        // Buttons carry their glyph (B: Cancel, A: Delete); Delete stays red. Rects are stashed so
        // update() can hit-test taps next frame.
        const int bh = TouchTargetMin, by = y + h - bh - 16, bw = (w - 48 - 16) / 2;
        const int cancelX = x + 24, delX = x + w - 24 - bw;
        drawGlyphButton(fb, cancelX, by, bw, bh, "B", "取消", Colors::PanelAlt);
        drawGlyphButton(fb, delX,    by, bw, bh, "A", "删除", Colors::Red, Colors::White);
        deleteCancelBtn = { cancelX, by, bw, bh };
        deleteDeleteBtn = { delX,    by, bw, bh };
    }

    void BackupSelectionScreen::deleteBackup(int index) {
        if (index <= 0 || index >= (int)backups.size()) {
            return;  // Can't delete the "从游戏载入" option or invalid index
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
