
#include "Globals.h"
#include "Save/GetSaveFileContents.h"
#include "UI/UI.h"
#include "UI/SaveSelectScreen.h"
#include "UI/BackupSelectionScreen.h"
#include "UI/TrainerViewScreen.h"
#include "Utils/HelperUtilities.h"
#include "Utils/Logger.h"
#include "Utils/FileUtilities.h"
#include "Trainer/Trainer.h"

using namespace Utils;
using namespace Trainer;

namespace UI {
    UIManager::UIManager() : running(true) {
        padConfigureInput(1, HidNpadStyleSet_NpadStandard);
        padInitializeDefault(&pad);
        hidInitializeTouchScreen();  // enable the touchscreen alongside the gamepad
    }

    UIManager::~UIManager() {
    }

    void UIManager::run() {
        while (appletMainLoop() && running) {
            handleSaveSelection();
        }
    }

    // Combined JKSV-style user + title picker: pick a user's avatar and one of their supported
    // Pokemon game icons in a single screen, then go straight to backup selection.
    void UIManager::handleSaveSelection() {
        SaveSelectScreen selectScreen;
        fb.startFade();

        while (appletMainLoop() && running && !selectScreen.shouldExit()) {
            padUpdate(&pad);
            touch.update();
            selectScreen.update(pad, touch);
            selectScreen.draw(fb);
            fb.drawFadeOverlay();
            fb.flush();

            if (selectScreen.hasSelectedTitle()) {
                handleBackupSelection(selectScreen.getSelectedUser(),
                                      selectScreen.getSelectedTitleId(),
                                      selectScreen.getSelectedTitleName());
                // Back from backup/trainer -> return so run() rebuilds the picker (re-lists saves).
                return;
            }
        }

        running = false;   // + pressed -> exit the app
    }

    void UIManager::handleBackupSelection(AccountUid userUid, u64 titleId, const std::string& titleName) {
        BackupSelectionScreen backupScreen(titleId, titleName);
        fb.startFade();

        while (appletMainLoop() && running && !backupScreen.shouldExit()) {
            padUpdate(&pad);
            touch.update();
            backupScreen.update(pad, touch);
            backupScreen.draw(fb);
            fb.drawFadeOverlay();
            fb.flush();

            if (backupScreen.hasSelectedBackup()) {
                if (backupScreen.shouldCreateNewBackup()) {
                    logInfoToFile("Creating new backup for", titleName.c_str());

                    // Auto-backup ON -> new timestamped history folder (kept indefinitely; the user
                    // decides when to delete backups, so we never prune). OFF -> reuse a single
                    // "工作副本" copy so backups don't pile up.
                    std::string backupPath = backupSaveData(userUid, titleId, titleName, g_autoBackupEnabled);
                    if (backupPath.empty()) {
                        logErrorToFile("Failed to back up save data");
                        // Tell the user and stay put. Returning here (the old behaviour) dropped them
                        // back at the save picker with no message, which is exactly what pressing B
                        // does -- so a failed backup was indistinguishable from a cancel (#48).
                        backupScreen.reportFailure("无法创建备份。请检查 SD 卡空间后重试。");
                        continue;
                    }
                    handleTrainerView(userUid, titleId, titleName, backupPath, true);   // read from the live save
                } else {
                    // Use existing backup
                    logInfoToFile("Loading existing backup", backupScreen.getSelectedBackupPath().c_str());
                    handleTrainerView(userUid, titleId, titleName, backupScreen.getSelectedBackupPath(), false);
                }
                return;
            }
        }
    }

    void UIManager::handleTrainerView(AccountUid userUid, u64 titleId, const std::string& titleName, const std::string& backupDir, bool loadedFromCart) {
        logInfoToFile("Loading save from", backupDir.c_str());

        // Read trainer data from the specified backup directory
        // Auto-detects game version and uses appropriate reading function
        TrainerVariant trainerVariant = readTrainerInfo(backupDir.c_str(), titleId);

        // Use std::visit to extract reference and create TrainerViewScreen
        std::visit([&](auto& trainer) {
            TrainerViewScreen trainerScreen(trainer, titleName, backupDir, titleId, userUid, loadedFromCart);
            fb.startFade();

            while (appletMainLoop() && !trainerScreen.shouldExit() && !trainerScreen.hasRequestedExit()) {
                padUpdate(&pad);
                touch.update();
                trainerScreen.update(pad, touch);
                trainerScreen.draw(fb);
                fb.drawFadeOverlay();
                fb.flush();
            }

            // If user pressed + to exit app, stop running
            if (trainerScreen.hasRequestedExit()) {
                running = false;
            }
        }, trainerVariant);
    }
}
