#ifndef UI_PANELS_BOX_POKEMON_PANEL_H
#define UI_PANELS_BOX_POKEMON_PANEL_H

// Forward declarations
namespace UI {
    class PKSEFramebuffer;
    class TrainerViewScreen;
}

namespace UI {
namespace Panels {
    void drawBoxPokemon(UI::TrainerViewScreen& screen, UI::PKSEFramebuffer& fb, int x, int y, int width, int height);
    // HOME summary side-panel (render + hexagon + info) shown right of the box when entered.
    void drawBoxSummaryPanel(UI::TrainerViewScreen& screen, UI::PKSEFramebuffer& fb, int x, int y, int width, int height);
}
}

#endif
