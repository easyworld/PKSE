#ifndef NAMES_NAMELANGUAGE_H
#define NAMES_NAMELANGUAGE_H
/**
 * NameLanguage.h - which language the generated name tables are read in.
 *
 * PKSE shipped English-only name tables, so a French save's Bulbizarre read "Bulbasaur", a German
 * player's item pouch was in English, and a Japanese save's species names were unreadable twice
 * over -- wrong language AND, until the console's shared fonts were added as NanoVG fallbacks, no
 * glyphs to draw them with. The two halves are independent: the fonts fixed STORED text (nicknames
 * and trainer names, which the save carries), and this fixes TABLE text (species, moves, items,
 * abilities, natures, types, forms and locations, which PKSE supplies).
 *
 * WHAT SELECTS THE LANGUAGE. Nothing yet -- see DISPLAY_LANGUAGE_SELECTABLE below. It shipped as a
 * setting defaulting to the console's own system language, and that is the wrong control: the
     * language a save's names should be read in is a property of the SAVE, not of the person holding
 * the console, so a French console showed French names for an English save and the user had to go
 * and correct it by hand. The intent is to take it from the OPENED SAVE instead. Until that lands
 * the tables are pinned to English and the Settings row is drawn disabled; everything below is
 * complete and is what that work builds on.
 *
 * WHAT THIS IS NOT. Stored text -- a nickname, a trainer name -- is bytes in the save, decoded by
 * that generation's own codec and drawn exactly as written, in any language, whatever this says.
 * The two halves are independent and only TABLE text is pinned.
 *
 * THE INDEX IS THE CONTRACT. Generated tables are arrays of `LANGUAGE_COUNT` pointers in the order
 * below, and `displayLanguageIndex()` is what indexes them. The order is NOT `Enums::LanguageID`'s
 * numbering -- that starts at 1, reserves 0 for "unset" and has an unused hole at 6, none of which
 * an array wants -- so the mapping lives in one place here and every generator emits to it. Adding
 * a language means adding a row here, adding its PKHeX suffix to the generators, and regenerating;
 * the `static_assert` in the generated files catches a table that did not come along.
 */
#include <cstddef>
#include <cstdint>

#include "Enums/LanguageID.h"

namespace Names {

    /// Languages PKHeX ships name tables for, in the order the generated tables store them.
    /// English is index 1 and is the fallback for anything unrecognised.
    enum class NameLanguage : uint8_t
    {
        Japanese = 0,
        English = 1,
        French = 2,
        Italian = 3,
        German = 4,
        Spanish = 5,
        Korean = 6,
        ChineseSimplified = 7,
        ChineseTraditional = 8,
    };

    inline constexpr size_t LANGUAGE_COUNT = 9;
    inline constexpr size_t LANGUAGE_INDEX_ENGLISH = 1;

    /// Whether the display language is USER-SELECTABLE yet. False, and everything it disables is
    /// deliberately still here: `Utils::loadSettings` still reads `language=` and still asks the
    /// console, then pins English because of this; `Utils::saveSettings` still writes the key; the
    /// Settings row, its label, its value and its A-to-cycle handler are all still written, and the
    /// screen simply stops one row short of drawing it (SETTINGS_ROW_VISIBLE_COUNT). Turning this
    /// true is what re-enables the lot, once the language is being taken from the opened save.
    ///
    /// HIDDEN rather than shown-but-greyed. A disabled control still reads as something the user
    /// ought to be able to reach, so a row that does nothing is a question the screen keeps asking
    /// and cannot answer.
    inline constexpr bool DISPLAY_LANGUAGE_SELECTABLE = false;

    /// PKHeX resource suffix per index ("ja", "en", "fr", ...). The generators emit in this order;
    /// this is here so a reader can check the two against each other without opening a .py file.
    inline const char *nameLanguageSuffix(size_t languageIndex)
    {
        static const char *const SUFFIXES[LANGUAGE_COUNT] = {
            "ja", "en", "fr", "it", "de", "es", "ko", "zh-Hans", "zh-Hant"};
        return languageIndex < LANGUAGE_COUNT ? SUFFIXES[languageIndex] : "en";
    }

    /// Display name of a language, in that language -- what a picker row should read.
    inline const char *nameLanguageLabel(size_t languageIndex)
    {
        static const char *const LABELS[LANGUAGE_COUNT] = {
            "日本語", "English", "Français", "Italiano", "Deutsch",
            "Español", "한국어", "简体中文", "繁體中文"};
        return languageIndex < LANGUAGE_COUNT ? LABELS[languageIndex] : "English";
    }

    /// Table index for a save/entity language byte. Anything PKSE has no table for -- including
    /// LanguageID::Hacked (0), which is a real value a Gen 5 in-game trade stores -- falls back to
    /// English rather than indexing out of range.
    inline size_t languageIndexFor(Enums::LanguageID languageId)
    {
        switch (languageId)
        {
        case Enums::LanguageID::Japanese:
            return static_cast<size_t>(NameLanguage::Japanese);
        case Enums::LanguageID::English:
            return static_cast<size_t>(NameLanguage::English);
        case Enums::LanguageID::French:
            return static_cast<size_t>(NameLanguage::French);
        case Enums::LanguageID::Italian:
            return static_cast<size_t>(NameLanguage::Italian);
        case Enums::LanguageID::German:
            return static_cast<size_t>(NameLanguage::German);
        case Enums::LanguageID::Spanish:
        case Enums::LanguageID::SpanishL:
            // Legends: Z-A's Latin American Spanish shares the Spanish tables rather than needing
            // a tenth: the games differ from European Spanish in interface and dialogue, not in
            // what a species, move or item is CALLED, and PKHeX ships no separate es-419 name set.
            return static_cast<size_t>(NameLanguage::Spanish);
        case Enums::LanguageID::Korean:
            return static_cast<size_t>(NameLanguage::Korean);
        case Enums::LanguageID::ChineseSimplified:
            return static_cast<size_t>(NameLanguage::ChineseSimplified);
        case Enums::LanguageID::ChineseTraditional:
            return static_cast<size_t>(NameLanguage::ChineseTraditional);
        default:
            return LANGUAGE_INDEX_ENGLISH;
        }
    }

    /// The language every name lookup reads. Set once at startup from settings.cfg (which itself
    /// defaults to the console's system language) and again whenever the user changes it.
    void setDisplayLanguage(size_t languageIndex);
    size_t displayLanguageIndex();

    /// The console's own system language as a table index, or English when it names one PKSE has
    /// no tables for (Dutch, Portuguese, Russian) -- those consoles get the games' own fallback,
    /// which is what the Pokemon titles themselves do for an unsupported system language.
    size_t systemLanguageIndex();
}

#endif // NAMES_NAMELANGUAGE_H
