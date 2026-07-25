#ifndef UI_PANELS_STORAGE_PANEL_H
#define UI_PANELS_STORAGE_PANEL_H

namespace UI {
    class TrainerViewScreen;
    class PKSEFramebuffer;

    namespace Panels {
        /**
         * Draws the PKSM-style dual-pane storage view: the save's PC boxes on the left and the
         * persistent on-SD bank on the right. The cursor only appears once the view is entered
         * (like Boxes) and is colored by the active CursorMode (Menu = red, Move = blue,
         * Multi = green). Multi-selected slots are marked; a carried Pokemon rides the cursor.
         */
        void drawStorageView(TrainerViewScreen& screen, PKSEFramebuffer& fb, int x, int y, int width, int height);

        /// Red-mode per-Pokemon action menu (Move / Edit / Release / Cancel).
        void drawStorageActionMenu(TrainerViewScreen& screen, PKSEFramebuffer& fb);
        /// Green-mode group menu (Move / Release / Clear / Cancel).
        void drawStorageGroupMenu(TrainerViewScreen& screen, PKSEFramebuffer& fb);
        /// Release confirmation (single Pokemon or the whole multi-selection).
        void drawStorageReleaseConfirm(TrainerViewScreen& screen, PKSEFramebuffer& fb);
        /// Save / Discard / Cancel prompt shown when leaving the storage view with bank changes.
        void drawStorageExitConfirm(TrainerViewScreen& screen, PKSEFramebuffer& fb);
        void drawCreatorKeepConfirm(TrainerViewScreen& screen, PKSEFramebuffer& fb);
        /// "传入或传出 Let's Go 会重置觉醒值／努力值" acknowledgement (gated by g_lgpeMoveWarn).
        void drawLgpeMoveConfirm(TrainerViewScreen& screen, PKSEFramebuffer& fb);
    }
}

#endif
