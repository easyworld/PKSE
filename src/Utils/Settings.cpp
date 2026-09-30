#include "Utils/Settings.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>

#include "Globals.h"
#include "Names/NameLanguage.h"
#include "UI/Common.h"
#include "Utils/Logger.h"

namespace Utils
{
    static std::string settingsPath()
    {
        return BASE_SAVE_DIRECTORY + "/settings.cfg";
    }

    void loadSettings()
    {
        // The language default is the CONSOLE's, not English: a French player should get French
        // names without configuring anything, which is the whole point of reading it here. An
        // explicit `language=` line below then overrides it, so a choice once made is kept even if
        // it happens to equal the system language. A console set to a language the Pokemon games
        // have never shipped in (Dutch, Portuguese, Russian) falls back to English, as they do.
        Names::setDisplayLanguage(Names::systemLanguageIndex());

        // No config yet -> keep the compiled-in defaults, but still fall through to the language
        // pin below. Returning early here skipped it, which is the FIRST run on every console --
        // exactly the case the pin exists for.
        FILE *f = fopen(settingsPath().c_str(), "r");
        char line[128];
        while (f && fgets(line, sizeof(line), f))
        {
            // Trim the trailing newline / carriage return.
            char *nl = strpbrk(line, "\r\n");
            if (nl)
                *nl = '\0';

            char *eq = strchr(line, '=');
            if (!eq)
                continue;
            *eq = '\0';
            const char *key = line;
            const char *value = eq + 1;

            if (strcmp(key, "theme") == 0)
            {
                UI::applyTheme(strcmp(value, "light") == 0 ? UI::ThemeMode::Light : UI::ThemeMode::Dark);
            }
            else if (strcmp(key, "autoBackup") == 0)
            {
                g_autoBackupEnabled = (strcmp(value, "0") != 0);
            }
            else if (strcmp(key, "allowIllegal") == 0)
            {
                g_allowIllegalEdits = (strcmp(value, "0") != 0);
            }
            else if (strcmp(key, "autoLegalize") == 0)
            {
                g_autoLegalizeTransfers = (strcmp(value, "0") != 0);
            }
            else if (strcmp(key, "moveWarn") == 0 || strcmp(key, "lgpeMoveWarn") == 0)
            {
                // "lgpeMoveWarn" is the old key for the same toggle, still read so an existing
                // settings.cfg doesn't silently revert the user's choice to the default. Only the
                // new key is written back, so it ages out on the first save.
                g_moveWarn = (strcmp(value, "0") != 0);
            }
            else if (strcmp(key, "injectToGame") == 0)
            {
                g_injectToGameSave = (strcmp(value, "0") != 0);
            }
            else if (strcmp(key, "debugLogging") == 0)
            {
                g_debugLogging = (strcmp(value, "0") != 0);
            }
            else if (strcmp(key, "language") == 0)
            {
                // Stored as the PKHeX resource suffix ("fr", "zh-Hant") rather than an index, so
                // the file stays readable and a future reordering of the table cannot silently
                // repoint an existing config at a different language.
                for (size_t languageIndex = 0; languageIndex < Names::LANGUAGE_COUNT; ++languageIndex)
                {
                    if (strcmp(value, Names::nameLanguageSuffix(languageIndex)) == 0)
                    {
                        Names::setDisplayLanguage(languageIndex);
                        break;
                    }
                }
            }
        }
        if (f)
            fclose(f);

        // Pin Simplified Chinese while the language is not user-selectable. Everything above still runs --
        // the console is still asked, an existing `language=` line is still honoured -- so
        // re-enabling the feature is deleting this block and nothing else. See NameLanguage.h for
        // why a manual override was the wrong control and what replaces it.
        if (!Names::DISPLAY_LANGUAGE_SELECTABLE)
            Names::setDisplayLanguage(static_cast<size_t>(Names::NameLanguage::ChineseSimplified));
    }

    void saveSettings()
    {
        // Make sure the base dir exists (it normally does once a backup has been taken).
        mkdir(BASE_SAVE_DIRECTORY.c_str(), 0777);

        FILE *f = fopen(settingsPath().c_str(), "w");
        if (!f)
        {
            logErrorToFile("Failed to write settings file", settingsPath().c_str());
            return;
        }
        fprintf(f, "theme=%s\n", (UI::g_themeMode == UI::ThemeMode::Light) ? "light" : "dark");
        fprintf(f, "autoBackup=%d\n", g_autoBackupEnabled ? 1 : 0);
        fprintf(f, "allowIllegal=%d\n", g_allowIllegalEdits ? 1 : 0);
        fprintf(f, "autoLegalize=%d\n", g_autoLegalizeTransfers ? 1 : 0);
        fprintf(f, "moveWarn=%d\n", g_moveWarn ? 1 : 0);
        fprintf(f, "injectToGame=%d\n", g_injectToGameSave ? 1 : 0);
        fprintf(f, "debugLogging=%d\n", g_debugLogging ? 1 : 0);
        fprintf(f, "language=%s\n", Names::nameLanguageSuffix(Names::displayLanguageIndex()));
        fclose(f);
    }
}
