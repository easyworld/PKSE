/**
 * FormSpriteMapping.h - Pokemon Form to Sprite ID Mapping
 *
 * Maps (species ID, form ID) pairs to PokeAPI sprite IDs.
 * PokeAPI uses sprite IDs 10001+ for alternate forms.
 */

#ifndef POKEMON_FORM_SPRITE_MAPPING_H
#define POKEMON_FORM_SPRITE_MAPPING_H

#include <cstdint>

namespace Pokemon {
    /**
     * Gets the PokeAPI sprite ID for a Pokemon form.
     * @param speciesId Pokemon species ID (1-1025)
     * @param formId Form ID (0 = base form)
     * @return Sprite ID (speciesId for base forms, 10000+ for alternate forms)
     *         Returns speciesId if no specific form sprite exists.
     */
    uint32_t getFormSpriteId(uint16_t speciesId, uint8_t formId);

    /**
     * Gets the NAME-keyed sprite stem for a Pokemon form, e.g. "666-meadow".
     *
     * Most PokeAPI HOME renders are keyed by a numeric id, but ~200 of them are keyed by name
     * instead -- the families whose forms are a set of peers rather than a base plus variants
     * (Unown letters, Arceus/Silvally types, Vivillon patterns, Alcremie creams, Furfrou trims,
     * flower colours, seasons, seas). Those have no numeric id at all, so getFormSpriteId cannot
     * reach them; this is the only way to address that art.
     *
     * @return The stem WITHOUT extension or shiny suffix, or "" when this form is numeric-keyed
     *         (or has no dedicated art). Callers should try this first and fall back to
     *         getFormSpriteId when it returns empty.
     */
    const char* getFormSpriteName(uint16_t speciesId, uint8_t formId);
}

#endif
