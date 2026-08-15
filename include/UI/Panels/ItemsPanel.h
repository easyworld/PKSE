#ifndef UI_PANELS_ITEMS_PANEL_H
#define UI_PANELS_ITEMS_PANEL_H

#include "Enums/GameVersion.h"

// Forward declarations
namespace UI {
    class PKSEFramebuffer;
    class TrainerViewScreen;
}

namespace UI {
namespace Panels {
    void drawItems(UI::TrainerViewScreen& screen, UI::PKSEFramebuffer& fb, int x, int y, int width, int height);

    /// Display name of a pouch/category index within a game group ("Medicine", "TMs", ...).
    ///
    /// Shared rather than inlined at the header draw.
    /// Never returns null -- an out-of-range category yields "?" so a log line cannot end
    /// up formatting a null pointer.
    const char* pouchDisplayName(Enums::GameVersion gameGroup, int category);
}
}

#endif
