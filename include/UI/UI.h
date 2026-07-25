#ifndef UI_UI_H
#define UI_UI_H

#include <switch.h>

#include "UI/Common.h"
#include "UI/PKSEFramebuffer.h"
#include "UI/UIScreen.h"
#include "UI/TouchInput.h"
#include "UI/SaveSelectScreen.h"
#include "UI/BackupSelectionScreen.h"
#include "UI/TrainerViewScreen.h"

namespace Trainer {
    class Trainer;
}

namespace Pokemon {
    struct Pokemon8SWSH;
}

namespace UI {

    class UIManager {
    public:
        UIManager();
        ~UIManager();

        void run();

    private:
        PKSEFramebuffer fb;
        PadState pad;
        TouchInput touch;
        bool running;

        void handleSaveSelection();
        void handleBackupSelection(AccountUid userUid, u64 titleId, const std::string& titleName);
        // loadedFromCart: true when this session was read from the LIVE game save (the "load from
        // title" path), false when an older backup was opened. Writing back to the game is the
        // normal, expected thing to do in the first case and a rollback in the second, so the two
        // are gated very differently at save time (#47).
        void handleTrainerView(AccountUid userUid, u64 titleId, const std::string& titleName,
                               const std::string& backupDir, bool loadedFromCart);
    };
}

#endif
