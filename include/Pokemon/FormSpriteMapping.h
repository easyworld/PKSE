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
}

#endif
