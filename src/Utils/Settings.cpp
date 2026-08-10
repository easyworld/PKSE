#include "Utils/Settings.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>

#include "Globals.h"
#include "UI/Common.h"
#include "Utils/Logger.h"

namespace Utils {
    static std::string settingsPath() {
        return BASE_SAVE_DIRECTORY + "/settings.cfg";
    }

    void loadSettings() {
        FILE* f = fopen(settingsPath().c_str(), "r");
        if (!f) return;  // no config yet -> keep compiled-in defaults

        char line[128];
        while (fgets(line, sizeof(line), f)) {
            // Trim the trailing newline / carriage return.
            char* nl = strpbrk(line, "\r\n");
            if (nl) *nl = '\0';

            char* eq = strchr(line, '=');
            if (!eq) continue;
            *eq = '\0';
            const char* key = line;
            const char* val = eq + 1;

            if (strcmp(key, "theme") == 0) {
                UI::applyTheme(strcmp(val, "light") == 0 ? UI::ThemeMode::Light : UI::ThemeMode::Dark);
            } else if (strcmp(key, "autoBackup") == 0) {
                g_autoBackupEnabled = (strcmp(val, "0") != 0);
            } else if (strcmp(key, "allowIllegal") == 0) {
                g_allowIllegalEdits = (strcmp(val, "0") != 0);
            } else if (strcmp(key, "moveWarn") == 0 || strcmp(key, "lgpeMoveWarn") == 0) {
                // "lgpeMoveWarn" is the old key for the same toggle, still read so an existing
                // settings.cfg doesn't silently revert the user's choice to the default. Only the
                // new key is written back, so it ages out on the first save.
                g_moveWarn = (strcmp(val, "0") != 0);
            } else if (strcmp(key, "injectToGame") == 0) {
                g_injectToGameSave = (strcmp(val, "0") != 0);
            }
        }
        fclose(f);
    }

    void saveSettings() {
        // Make sure the base dir exists (it normally does once a backup has been taken).
        mkdir(BASE_SAVE_DIRECTORY.c_str(), 0777);

        FILE* f = fopen(settingsPath().c_str(), "w");
        if (!f) {
            logErrorToFile("Failed to write settings file", settingsPath().c_str());
            return;
        }
        fprintf(f, "theme=%s\n", (UI::g_themeMode == UI::ThemeMode::Light) ? "light" : "dark");
        fprintf(f, "autoBackup=%d\n", g_autoBackupEnabled ? 1 : 0);
        fprintf(f, "allowIllegal=%d\n", g_allowIllegalEdits ? 1 : 0);
        fprintf(f, "moveWarn=%d\n", g_moveWarn ? 1 : 0);
        fprintf(f, "injectToGame=%d\n", g_injectToGameSave ? 1 : 0);
        fclose(f);
    }
}
