#ifndef UI_PANELS_PARTY_POKEMON_PANEL_H
#define UI_PANELS_PARTY_POKEMON_PANEL_H

#include <vector>
#include <memory>
#include <cstdint>

// Forward declarations
namespace UI {
    class PKSEFramebuffer;
}
namespace Pokemon {
    class Pokemon;
}
namespace Trainer {
    class Trainer;
}

namespace UI {
namespace Panels {
    void drawPartyPokemon(UI::PKSEFramebuffer& fb, const Trainer::Trainer& trainer, int x, int y, int width, int height, int selectedIndex = -1);
}
}

#endif
