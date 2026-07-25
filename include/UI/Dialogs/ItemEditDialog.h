#ifndef UI_DIALOGS_ITEM_EDIT_DIALOG_H
#define UI_DIALOGS_ITEM_EDIT_DIALOG_H

// Forward declarations
namespace UI {
    class PKSEFramebuffer;
    class TrainerViewScreen;
}

namespace UI {
namespace Dialogs {
    void drawItemEditDialog(UI::TrainerViewScreen& screen, UI::PKSEFramebuffer& fb);
    // Confirm dialog for removing the selected item from the Items list (Y). Red frame, B/A glyphs.
    void drawItemRemoveConfirm(UI::TrainerViewScreen& screen, UI::PKSEFramebuffer& fb);
}
}

#endif
