#ifndef GLOBALS_H
#define GLOBALS_H

#include <string>

/// Derived from the Makefile's APP_VERSION_FULL via -DPKSE_VERSION. That is the long spelling; the
/// .nacp separately carries APP_VERSION, an abbreviation of it, because its display_version field
/// holds only 15 characters. Both are set side by side in the Makefile, so the two can differ in
/// how much of the version they show but not in which version it is.
///
/// The fallback is deliberately loud rather than a plausible-looking number. This header is also
/// parsed by the Visual Studio projects (IntelliSense only, never the real build), which don't pass
/// the define; a fallback like "0.0.3" would silently look correct while being a guess.
///
/// Deliberately `const` and not `constexpr`. A constant-evaluated std::string has to fit inside
/// libstdc++'s small-string buffer -- 15 characters -- because anything longer heap-allocates, and
/// an allocation cannot escape constant evaluation into the runtime. Any pre-release tag blows past
/// that, and the fallback below is already sitting exactly on 15, so `constexpr` here was one
/// character away from breaking on its own. Nothing evaluates this at compile time anyway.
#ifndef PKSE_VERSION
#define PKSE_VERSION "0.0.0-NOVERSION"
#endif
inline const std::string VERSION_STRING = PKSE_VERSION;

inline constexpr std::string BASE_SAVE_DIRECTORY = "sdmc:/PKSE";

inline constexpr uint32_t SIZE_HASH_IN_BYTES = 32;

/// Settings lock for writing an OLDER BACKUP over the live game save, persisted. Default OFF.
///
/// Scope is deliberately narrow. Saving a session that was loaded FROM the live save back TO it is
/// always allowed and needs no toggle — that is simply what a save editor does, and the data being
/// overwritten is the data that was just read. What this gates is the other case: opening a backup
/// from some earlier point and writing it over the current save, which rolls the game backwards and
/// loses everything played since. That is the only thing worth a lock, and it also raises a separate
/// confirmation at save time.
///
/// (Replaces a hard-coded `constexpr bool SAVE_TO_TITLE = true`, under which every save overwrote
/// the live save with no way to opt out.)
inline bool g_injectToGameSave = false;

/// Runtime setting (toggled in the in-app Settings screen, persisted to settings.cfg): when true,
/// "Load from Title" creates a new timestamped backup each time (kept indefinitely — backups are
/// never auto-pruned; deleting them is the user's decision). When false, it reuses a single
/// "Working" copy so backups don't pile up.
inline bool g_autoBackupEnabled = true;
inline bool g_allowIllegalEdits = false;  // Settings toggle: lift the legal EV/AV caps (0-252 / 0-200 -> 0-255) so illegal values can be set (e.g. to test the legality checker).
inline bool g_moveWarn = true;            // Settings toggle ("Move warning"): confirm before a bank move that loses data. Covers the Let's Go transfer (AV/EV training resets to 0); the Gen 3 down-convert warns regardless, since it rebuilds the PID and cannot be undone.

/// Number of days to retain debug log files
inline constexpr uint8_t LOG_RETENTION_DAYS = 30;

#endif