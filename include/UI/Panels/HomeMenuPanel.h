#ifndef UI_PANELS_HOME_MENU_PANEL_H
#define UI_PANELS_HOME_MENU_PANEL_H

// Forward declarations
namespace UI {
    class PKSEFramebuffer;
    class TrainerViewScreen;
}

namespace UI {
namespace Panels {
    // HOME-style main menu (shown when no mode is entered): a live box-preview card on the
    // left, big rounded destination pills (Pokemon / Party / Storage) + a circular icon row
    // (Items / Trainer / Settings) on the right. Replaces the old left trainer-info + mode-selector.
    void drawHomeMenu(UI::TrainerViewScreen& screen, UI::PKSEFramebuffer& fb);
}
}

#endif
