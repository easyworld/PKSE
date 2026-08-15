#ifndef UTILS_LOGGER_H
#define UTILS_LOGGER_H

#include <string>

#define LOG_BUFFER_SIZE 1024

namespace Utils {
    void logInfoToFile(const char *message);
    void logInfoToFile(std::string message);
    void logErrorToFile(const char *message);
    void logErrorToFile(std::string message);
    void logInfoToFile(const char *message, const char *context);
    void logErrorToFile(const char *message, const char *context);
    void cleanupOldLogs();

    /**
     * One user ACTION, recorded in the dated debug log with an `[EVENT]` tag.
     *
     * Deliberately the same file as the INFO/ERROR diagnostics rather than a stream of its own: an
     * action log is only worth having next to the errors it caused. "The save failed" is not a bug
     * report; "the save failed immediately after a traded Zacian was moved out of the bank into box
     * 3" is one. Splitting the two into separate files is what makes that correlation impossible to
     * see, so they interleave here on purpose.
     *
     * Format is one line of `KEY=value` pairs, no wrapping, values quoted where they can contain
     * spaces -- so a whole class of event greps out with `grep 'EVENT.*RELEASE'` and individual
     * fields are readable without a parser. Build the line with the helpers in `Utils/EventLog.h`
     * rather than hand-rolling one, so the field names stay consistent between call sites.
     */
    void logEventToFile(const std::string& line);

    /**
     * Append one line to the TEST TRACE at sdmc:/PKSE/trace.log.
     *
     * Deliberately a separate file from the dated diagnostic logs. This one is meant to be copied
     * off the SD card whole and read by someone who wasn't there — so it stays one line per action,
     * `KEY=value` pairs, no wrapping, and it records the OUTCOME of each operation rather than
     * breadcrumbs leading to one. That is the difference between "I can see what you did" and
     * "I can check whether it was right".
     *
     * Never rotated or truncated by PKSE: a test pass that silently lost its first half would be
     * worse than no trace. Delete the file to start a fresh run.
     */
    void logTest(const std::string& line);

    /// Banner + environment for a run. Call once when a save is opened.
    void logTestSession(const std::string& details);
}

#endif