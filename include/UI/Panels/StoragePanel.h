#ifndef UI_PANELS_STORAGE_PANEL_H
#define UI_PANELS_STORAGE_PANEL_H

namespace UI {
    class TrainerViewScreen;
    class PKSEFramebuffer;

    namespace Panels {
        /**
         * Draws the HOME-style dual-pane storage view: the save's PC boxes on the left and the
         * persistent on-SD bank on the right. The cursor only appears once the view is entered
         * (like Boxes): a pointer arrow colored by the active CursorMode (Menu = red, Move = blue,
         * Multi = green). In Multi mode a green rectangle rubber-bands out from the anchor cell,
         * and once grabbed the whole block rides the cursor -- drawn on top of both panes so the
         * Pokemon stay visible the entire way across the screen.
         */
        void drawStorageView(TrainerViewScreen& screen, PKSEFramebuffer& fb, int x, int y, int width, int height);

        /// Red-mode per-Pokemon action menu (Move / Edit / Release / Cancel).
        void drawStorageActionMenu(TrainerViewScreen& screen, PKSEFramebuffer& fb);
        /// Options for the block in hand (Release all / Put back / Cancel), opened with Minus.
        void drawStorageGroupMenu(TrainerViewScreen& screen, PKSEFramebuffer& fb);
        /// Release confirmation (single Pokemon or the whole carried block).
        void drawStorageReleaseConfirm(TrainerViewScreen& screen, PKSEFramebuffer& fb);
        /// Save / Discard / Cancel prompt shown when leaving the storage view with bank changes.
        void drawStorageExitConfirm(TrainerViewScreen& screen, PKSEFramebuffer& fb);
        void drawCreatorKeepConfirm(TrainerViewScreen& screen, PKSEFramebuffer& fb);
        void drawDetailsDiscardConfirm(TrainerViewScreen& screen, PKSEFramebuffer& fb);
        /// "要转换为第三世代格式吗？" acknowledgement -- the PID is rebuilt and cannot be undone, so this one
        /// is shown regardless of the Move warning setting.
        void drawGen3ConvertConfirm(TrainerViewScreen& screen, PKSEFramebuffer& fb);
        /// "传入或传出 Let's Go 会重置觉醒值／努力值" acknowledgement (gated by g_moveWarn).
        void drawLgpeTransferConfirm(TrainerViewScreen& screen, PKSEFramebuffer& fb);
    }
}

#endif
