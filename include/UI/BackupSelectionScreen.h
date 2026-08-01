#ifndef UI_BACKUP_SELECTION_SCREEN_H
#define UI_BACKUP_SELECTION_SCREEN_H

#include <vector>
#include <string>

#include <switch.h>

#include "UI/UIScreen.h"
#include "UI/PKSEFramebuffer.h"

namespace UI {
    class BackupSelectionScreen : public UIScreen {
    public:
        BackupSelectionScreen(u64 titleId, const std::string& titleName);
        void update(const PadState& pad, const TouchInput& touch) override;
        void draw(PKSEFramebuffer& fb) override;
        bool shouldExit() const override { return goBack; }

        bool hasSelectedBackup() const { return backupSelected; }
        bool shouldCreateNewBackup() const { return createNewBackup; }
        const std::string& getSelectedBackupPath() const { return selectedBackupPath; }

        /**
         * Report that acting on the selection failed, so the screen can say so and take the choice
         * back. Without this the caller's only option is to bail out of the whole flow, which
         * drops the user back at the save picker with no indication that anything went wrong — the
         * failure is indistinguishable from having pressed B.
         */
        void reportFailure(const std::string& message) {
            statusMessage = message;
            statusFrames = 480;
            backupSelected = false;      // hand the choice back rather than re-firing it every frame
            createNewBackup = false;
        }

    private:
        struct BackupInfo {
            std::string timestamp;
            std::string displayName;
        };

        u64 titleId;
        std::string titleName;
        std::string gameDirectory;
        std::vector<BackupInfo> backups;
        int selectedIndex;
        bool backupSelected;
        bool createNewBackup;  // True if user wants to load from title directly
        bool goBack;
        std::string selectedBackupPath;
        bool showDeleteConfirmation;
        int deleteConfirmationIndex;
        // Delete-confirm button rects, captured during draw and hit-tested next frame (this screen
        // has no touchButtons framework; same one-frame-late contract as TrainerViewScreen).
        struct DlgBtn { int x = 0, y = 0, w = 0, h = 0; };
        DlgBtn deleteCancelBtn, deleteDeleteBtn;
        std::string statusMessage;   // transient failure notice, shown just above the nav bar
        int statusFrames = 0;

        void loadBackups();
        void drawBackupList(PKSEFramebuffer& fb);
        void drawDeleteConfirmation(PKSEFramebuffer& fb);
        std::string formatTimestamp(const std::string& timestamp);
        void deleteBackup(int index);
    };
}

#endif
