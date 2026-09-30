#ifndef UI_DIALOGS_SAVE_CONFIRM_DIALOG_H
#define UI_DIALOGS_SAVE_CONFIRM_DIALOG_H

namespace UI
{
    class PKSEFramebuffer;
    class TrainerViewScreen;
}

namespace UI
{
    namespace Dialogs
    {
        void drawSaveConfirmDialog(UI::TrainerViewScreen &screen, UI::PKSEFramebuffer &framebuffer);
        /// Final confirmation before overwriting the player's live game save.
        void drawSaveInjectConfirm(UI::TrainerViewScreen &screen, UI::PKSEFramebuffer &framebuffer);
    }
}

#endif
