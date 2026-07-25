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
     * Append one line to the TEST TRACE at sdmc:/PKSE/test-trace.log.
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