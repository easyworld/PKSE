#ifndef UI_DIALOGS_STAT_EDIT_DIALOG_H
#define UI_DIALOGS_STAT_EDIT_DIALOG_H

// Forward declarations
namespace UI {
    class PKSEFramebuffer;
    class TrainerViewScreen;
}

namespace UI {
namespace Dialogs {
    enum class StatEditMode {
        IV,  // Individual Values (0-31)
        EV,  // Effort Values (0-252, total max 510) - Gen 8/9
        AV   // Awakening Values (0-200 each, no total cap) - Let's Go only
    };

    void drawStatEditDialog(UI::TrainerViewScreen& screen, UI::PKSEFramebuffer& fb);
}
}

#endif
