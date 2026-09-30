/**
 * Maps (species ID, form ID) pairs to PokeAPI sprite IDs.
 * PokeAPI uses sprite IDs 10001+ for alternate forms.
 */

#ifndef POKEMON_FORM_SPRITE_MAPPING_H
#define POKEMON_FORM_SPRITE_MAPPING_H

#include <cstdint>

namespace Pokemon
{
    /// The species id itself for a base form or one with no dedicated art; 10000+ otherwise.
    uint32_t getFormSpriteId(uint16_t speciesId, uint8_t formId);

    /// The stem without extension or shiny suffix, e.g. "666-meadow", or "" when this form is
    /// numeric-keyed. PokeAPI keys ~200 renders by name -- the families whose forms are a set of
    /// peers rather than a base plus variants (Unown letters, Arceus/Silvally types, Vivillon
    /// patterns, Alcremie creams, Furfrou trims, flower colours, seasons, seas). Those have no
    /// numeric id at all, so getFormSpriteId cannot reach them; try this first.
    const char *getFormSpriteName(uint16_t speciesId, uint8_t formId);
}

#endif
