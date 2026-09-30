#ifndef UI_MODALS_POKEMON_DETAILS_MODAL_H
#define UI_MODALS_POKEMON_DETAILS_MODAL_H

namespace UI
{
    class PKSEFramebuffer;
    class TrainerViewScreen;
}

namespace UI
{
    namespace Modals
    {
        void drawPokemonDetailsModal(UI::TrainerViewScreen &screen, UI::PKSEFramebuffer &framebuffer);
    }
}

#endif
