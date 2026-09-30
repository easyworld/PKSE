
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
#include "Trainer/Trainer1RBY.h"

using namespace Utils;
using namespace Trainer;

namespace UI
{
    UIManager::UIManager() : running(true)
    {
        padConfigureInput(1, HidNpadStyleSet_NpadStandard);
        padInitializeDefault(&pad);
        hidInitializeTouchScreen(); // enable the touchscreen alongside the gamepad
    }

    UIManager::~UIManager()
    {
    }

    void UIManager::run()
    {
        while (appletMainLoop() && running)
        {
            handleSaveSelection();
        }
    }

    // Combined JKSV-style user + title picker: pick a user's avatar and one of their supported
    // Pokemon game icons in a single screen, then go straight to backup selection.
    void UIManager::handleSaveSelection()
    {
        SaveSelectScreen selectScreen;
        framebuffer.startFade();

        while (appletMainLoop() && running && !selectScreen.shouldExit())
        {
            padUpdate(&pad);
            touch.update();
            selectScreen.update(pad, touch);
            // Captured every frame while the picker is up: it re-enumerates on a user switch, and
            // this is the only place the whole console's save list exists.
            consoleSaves.clear();
            for (const auto &user : selectScreen.allUsers())
            {
                for (const auto &title : user.titles)
                {
                    TrainerViewScreen::ConsoleSaveEntry entry;
                    entry.titleId = title.titleId;
                    entry.accountUid = user.accountUid;
                    entry.label = title.label;
                    entry.userName = user.name;
                    entry.folder = title.folder;
                    entry.gameVersion = title.gameVersion;
                    consoleSaves.push_back(std::move(entry));
                }
            }
            selectScreen.draw(framebuffer);
            framebuffer.drawFadeOverlay();
            framebuffer.flush();

            if (selectScreen.hasSelectedTitle())
            {
                handleBackupSelection(selectScreen.getSelectedUser(),
                                      selectScreen.getSelectedTitleId(),
                                      selectScreen.getSelectedTitleName(),
                                      selectScreen.getSelectedTitleFolder());
                // Back from backup/trainer -> return so run() rebuilds the picker (re-lists saves).
                return;
            }
            if (selectScreen.hasSelectedFile())
            {
                // A loose save skips backup selection entirely: there is no backup tree for a file
                // the user pointed at, and no "live save vs older backup" choice to make.
                handleExternalSave(selectScreen.getSelectedFilePath());
                return;
            }
        }

        running = false; // + pressed -> exit the app
    }

    void UIManager::handleBackupSelection(AccountUid userUid, u64 titleId, const std::string &titleName,
                                          const std::string &titleFolder)
    {
        BackupSelectionScreen backupScreen(titleId, titleName, titleFolder);
        framebuffer.startFade();

        while (appletMainLoop() && running && !backupScreen.shouldExit())
        {
            padUpdate(&pad);
            touch.update();
            backupScreen.update(pad, touch);
            backupScreen.draw(framebuffer);
            framebuffer.drawFadeOverlay();
            framebuffer.flush();

            if (backupScreen.hasSelectedBackup())
            {
                if (backupScreen.shouldCreateNewBackup())
                {
                    logInfoToFile("Creating new backup for", titleName.c_str());

                    // Auto-backup ON -> new timestamped history folder (kept indefinitely; the user
                    // decides when to delete backups, so we never prune). OFF -> reuse a single
                    // "工作副本" copy so backups don't pile up.
                    std::string backupPath = backupSaveData(userUid, titleId, titleFolder, g_autoBackupEnabled);
                    if (backupPath.empty())
                    {
                        logErrorToFile("Failed to back up save data");
                        // Tell the user and stay put. Returning here (the old behaviour) dropped them
                        // back at the save picker with no message, which is exactly what pressing B
                        // does -- so a failed backup was indistinguishable from a cancel.
                        backupScreen.reportFailure("无法创建备份。请检查 SD 卡空间后重试。");
                        continue;
                    }
                    handleTrainerView(userUid, titleId, titleName, backupPath, true); // read from the live save
                }
                else
                {
                    logInfoToFile("Loading existing backup", backupScreen.getSelectedBackupPath().c_str());
                    handleTrainerView(userUid, titleId, titleName, backupScreen.getSelectedBackupPath(), false);
                }
                return;
            }
        }
    }

    void UIManager::handleTrainerView(AccountUid userUid, u64 titleId, const std::string &titleName,
                                      const std::string &backupDir, bool loadedFromCart)
    {
        logInfoToFile("Loading save from", backupDir.c_str());

        // Read trainer data from the specified backup directory
        // Auto-detects game version and uses appropriate reading function
        TrainerVariant trainerVariant = readTrainerInfo(backupDir.c_str(), titleId);

        // Use std::visit to extract reference and create TrainerViewScreen
        std::visit([&](auto &trainer)
                   {
            TrainerViewScreen trainerScreen(trainer, titleName, backupDir, titleId, userUid, loadedFromCart);
            // The trade-partner picker offers the OTHER saves on this console -- captured while
            // the picker screen had them enumerated, because walking save data is fs/ns work the
            // editor has no business repeating.
            trainerScreen.setConsoleSaves(consoleSaves);
            framebuffer.startFade();

            while (appletMainLoop() && !trainerScreen.shouldExit() && !trainerScreen.hasRequestedExit()) {
                padUpdate(&pad);
                touch.update();
                trainerScreen.update(pad, touch);
                trainerScreen.draw(framebuffer);
                framebuffer.drawFadeOverlay();
                framebuffer.flush();
            }

            // If user pressed + to exit app, stop running
            if (trainerScreen.hasRequestedExit()) {
                running = false;
            } }, trainerVariant);
    }

    // A save the user pointed at on the SD card, rather than one belonging to an installed title.
    // Everything the normal path derives from the title id -- which game, where the backups live,
    // which account owns it -- is either absent or comes from the file itself here.
    void UIManager::handleExternalSave(const std::string &path)
    {
        logInfoToFile("Opening external save", path.c_str());

        size_t length = 0;
        uint8_t *raw = Utils::readAllBytes(path.c_str(), &length);
        if (!raw)
        {
            logErrorToFile("External save unreadable", path.c_str());
            return;
        }
        std::vector<uint8_t> bytes(raw, raw + length);
        delete[] raw;

        // The picker already validated this, but it is re-identified rather than assumed: the
        // file could have changed on the card between the pick and here, and constructing a
        // trainer from bytes that are not a save produces an empty editor with no explanation.
        //
        // The trainer is owned through the base pointer because the concrete type is not known
        // until the probe chain runs -- eleven formats across five generations arrive this way.
        std::string label;
        std::unique_ptr<Trainer::Trainer> trainer =
            Save::openExternalSave(std::move(bytes), path, &label);
        if (!trainer)
        {
            logErrorToFile("External save is not a format PKSE opens", path.c_str());
            return;
        }

        // There is no title id, so 0 is passed and the save path travels as the "backup dir" so
        // the write-back knows which file it came from.
        TrainerViewScreen trainerScreen(*trainer, label, path, 0, AccountUid{}, false);
        framebuffer.startFade();

        while (appletMainLoop() && !trainerScreen.shouldExit() && !trainerScreen.hasRequestedExit())
        {
            padUpdate(&pad);
            touch.update();
            trainerScreen.update(pad, touch);
            trainerScreen.draw(framebuffer);
            framebuffer.drawFadeOverlay();
            framebuffer.flush();
        }
        if (trainerScreen.hasRequestedExit())
            running = false;
    }
}
