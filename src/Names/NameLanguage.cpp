/**
 * NameLanguage.cpp - the active name-table language, and the console's own.
 *
 * See NameLanguage.h for why this exists and why the index order is what it is.
 */
#include "Names/NameLanguage.h"

#include "Utils/NXTypes.h"

namespace Names {

    namespace
    {
        /// English until something sets it. That matters for ordering rather than taste: the name
        /// tables are read during startup (the title picker labels games before settings load on
        /// some paths), and a valid index is the one thing every lookup needs.
        size_t g_displayLanguageIndex = static_cast<size_t>(NameLanguage::ChineseSimplified);
    }

    void setDisplayLanguage(size_t languageIndex)
    {
        g_displayLanguageIndex = languageIndex < LANGUAGE_COUNT ? languageIndex : LANGUAGE_INDEX_ENGLISH;
    }

    size_t displayLanguageIndex()
    {
        return g_displayLanguageIndex;
    }

    size_t systemLanguageIndex()
    {
#ifdef __SWITCH__
        u64 languageCode = 0;
        if (R_FAILED(setGetSystemLanguage(&languageCode)))
            return LANGUAGE_INDEX_ENGLISH;
        SetLanguage systemLanguage = SetLanguage_ENUS;
        if (R_FAILED(setMakeLanguage(languageCode, &systemLanguage)))
            return LANGUAGE_INDEX_ENGLISH;

        switch (systemLanguage)
        {
        case SetLanguage_JA:
            return static_cast<size_t>(NameLanguage::Japanese);
        // Both English variants and both French variants collapse: PKHeX ships one table each, and
        // the Pokemon games themselves make no en-GB/en-US or fr-FR/fr-CA distinction in names.
        case SetLanguage_ENUS:
        case SetLanguage_ENGB:
            return static_cast<size_t>(NameLanguage::English);
        case SetLanguage_FR:
        case SetLanguage_FRCA:
            return static_cast<size_t>(NameLanguage::French);
        case SetLanguage_DE:
            return static_cast<size_t>(NameLanguage::German);
        case SetLanguage_IT:
            return static_cast<size_t>(NameLanguage::Italian);
        // es-419 (Latin American Spanish) folds into Spanish. PKHeX does ship a separate es-419
        // set, but the Pokemon name tables are the same; the difference is elsewhere in its UI.
        case SetLanguage_ES:
        case SetLanguage_ES419:
            return static_cast<size_t>(NameLanguage::Spanish);
        case SetLanguage_KO:
            return static_cast<size_t>(NameLanguage::Korean);
        case SetLanguage_ZHCN:
        case SetLanguage_ZHHANS:
            return static_cast<size_t>(NameLanguage::ChineseSimplified);
        case SetLanguage_ZHTW:
        case SetLanguage_ZHHANT:
            return static_cast<size_t>(NameLanguage::ChineseTraditional);
        // Dutch, Portuguese (both), Russian: the Pokemon games have never shipped in these, so
        // there is no table to fall back to but English -- which is what the games do too.
        default:
            return LANGUAGE_INDEX_ENGLISH;
        }
#else
        return LANGUAGE_INDEX_ENGLISH;
#endif
    }
}
