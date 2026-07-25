#ifndef UTILS_SETTINGS_H
#define UTILS_SETTINGS_H

namespace Utils {
    // Persisted app settings live in sdmc:/PKSE/settings.cfg (simple key=value text).
    // Currently: theme (dark/light) and auto-backup (on/off).

    // Load settings into the runtime globals (UI::g_themeMode via applyTheme,
    // ::g_autoBackupEnabled). Safe when the file is missing — defaults are kept.
    // Call once at startup, before any screen draws.
    void loadSettings();

    // Write the current settings back to settings.cfg. Call after a setting changes.
    void saveSettings();
}

#endif
