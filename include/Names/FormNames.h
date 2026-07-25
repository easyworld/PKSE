/**
 * FormNames.h - Pokemon Form Name Lookup
 *
 * Provides form name mapping for Pokemon with regional variants and other permanent forms.
 * Excludes battle-only forms (Mega Evolution, Gigantamax, Dynamax, etc.)
 */

#ifndef NAMES_FORM_NAMES_H
#define NAMES_FORM_NAMES_H

#include <cstdint>
#include <string>

namespace Names {
    /**
     * Gets the form name for a given Pokemon species and form ID.
     *
     * @param speciesId The Pokemon's species ID (1-1025)
     * @param formId The form ID (0 = base form, 1+ = alternate forms)
     * @return Form name string, or empty string for base form (form 0)
     *
     * Examples:
     * - getFormName(26, 1) = "Alolan" (Alolan Raichu)
     * - getFormName(479, 1) = "Heat" (Heat Rotom)
     * - getFormName(25, 0) = "" (base Pikachu)
     */
    const char* getFormName(uint16_t speciesId, uint8_t formId);

    /**
     * Composes the user-facing display name: the variant label prefixed to the base species name,
     * e.g. "Alolan Raichu", "Combat Breed Tauros", "Hisuian Zoroark". Falls back to just `baseName`
     * when the form has no variant label (base form, or a form this table doesn't name). Use this at
     * every place the UI shows a species name so a variant reads as what it is instead of the plain
     * base species.
     *
     * @param speciesId species id (for the form lookup)
     * @param formId    the mon's form index (Pokemon::form())
     * @param baseName  the game-appropriate base species string (e.g. Pokemon::species())
     */
    std::string getDisplayName(uint16_t speciesId, uint8_t formId, const std::string& baseName);
}

#endif
