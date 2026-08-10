#include <string>

#ifdef __SWITCH__
#include <switch.h>   // not actually used below; kept for console builds
#endif
#include <time.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>

#include "Utils/Logger.h"
#include "Globals.h"

//---------------------------------------------------------------------------------------------
// Production builds compile every SD-card log sink out.
//
// PKSE_PROD comes from `make ... prod` (see the Makefile). A release build must not leave a dated
// debug log on the user's card every time they open the app, nor grow test-trace.log forever --
// that is a diagnostic for development and hardware test passes, not something to ship.
//
// The whole implementation is replaced rather than each fopen being guarded, so there is exactly
// one place to check and no way for a new sink to be added below and quietly survive into a
// release. The DECLARATIONS in Logger.h are deliberately untouched: every call site still compiles
// unchanged, the host harnesses keep their own stubs, and nothing needs to know which build it is.
//
// Call sites still build their argument strings in a prod build -- that work is not eliminated,
// only the file I/O is. It costs strictly less than the logging build it replaces, so there is no
// regression; if it ever matters, the no-ops can move into the header to let the optimiser drop
// the arguments too.
//---------------------------------------------------------------------------------------------
#ifdef PKSE_PROD

namespace Utils {
    void logTest(const std::string&)              {}
    void logTestSession(const std::string&)       {}
    void logInfoToFile(const char*)               {}
    void logInfoToFile(std::string)               {}
    void logErrorToFile(const char*)              {}
    void logErrorToFile(std::string)              {}
    void logInfoToFile(const char*, const char*)  {}
    void logErrorToFile(const char*, const char*) {}

    // Also a no-op: a build that writes no logs has no business deleting files either. Logs left
    // by an earlier development build stay where they are -- removing them is the user's call.
    void cleanupOldLogs()                         {}
}

#else

namespace Utils {
    #define LOG_DIRECTORY "sdmc:/PKSE/logs"

    constexpr const char *LOG_TYPE_INFO = "INFO";
    constexpr const char *LOG_TYPE_ERROR = "ERROR";

    // One flat file, next to the backups rather than inside logs/, so it is obvious and easy to
    // grab over MTP without digging through dated debug logs.
    #define TEST_TRACE_PATH "sdmc:/PKSE/test-trace.log"

    void logTest(const std::string& line) {
        FILE* f = fopen(TEST_TRACE_PATH, "a");
        if (!f) return;   // tracing must never be able to break the app it is observing
        char timeBuffer[16];
        const time_t now = time(NULL);
        strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", localtime(&now));
        fprintf(f, "[%s] %s\n", timeBuffer, line.c_str());
        fclose(f);
    }

    void logTestSession(const std::string& details) {
        FILE* f = fopen(TEST_TRACE_PATH, "a");
        if (!f) return;
        char dateBuffer[32];
        const time_t now = time(NULL);
        strftime(dateBuffer, sizeof(dateBuffer), "%Y-%m-%d %H:%M:%S", localtime(&now));
        // A visible separator so several sessions in one file stay readable, and so a reader can
        // tell at a glance where a run began rather than inferring it from timestamps.
        fprintf(f, "\n========================================================================\n");
        fprintf(f, "SESSION  %s  %s\n", dateBuffer, details.c_str());
        fprintf(f, "========================================================================\n");
        fclose(f);
    }

    static std::string getCurrentLogFilePath() {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);

        char dateBuffer[16];
        strftime(dateBuffer, sizeof(dateBuffer), "%Y-%m-%d", t);

        return std::string(LOG_DIRECTORY) + "/debug_" + dateBuffer + ".log";
    }
    // For internal use only
    void logToFile(const char *type, const char *message, const char *context = NULL) {
        bool hasContext = (context == NULL || (context != NULL && context[0] == '\0'));

        // Ensure the directory exists
        if (mkdir(LOG_DIRECTORY, 0777) != 0 && errno != EEXIST)
        {
            printf("Failed to create log directory: %s\n", LOG_DIRECTORY);
            consoleUpdate(NULL);
            return;
        }

        std::string logFilePath = getCurrentLogFilePath();

        // Open the log file in append mode
        FILE *logFile = fopen(logFilePath.c_str(), "a");
        if (logFile)
        {
            // Get the current time
            time_t now = time(NULL);
            struct tm *t = localtime(&now);

            // Format the timestamp (e.g., "YYYY-MM-DD HH:MM:SS")
            char timeBuffer[20];
            strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", t);
            if (hasContext) {
                fprintf(logFile, "[%s][%s] %s: %s\n", timeBuffer, type, message, context);
                fclose(logFile);
            }
            else {
                fprintf(logFile, "[%s][%s] %s\n", timeBuffer, type, message);
                fclose(logFile);
            }
        }
        else
        {
            // Log to console if the file cannot be opened
            printf("Failed to open log file: %s\n", logFilePath.c_str());
        }
    }

    void logInfoToFile(const char *message)
    {
        logToFile(LOG_TYPE_INFO, message);
    }

    void logInfoToFile(std::string message)
    {
        logToFile(LOG_TYPE_INFO, message.c_str());
    }

    void logErrorToFile(const char *message)
    {
        logToFile(LOG_TYPE_ERROR, message);
    }

    void logErrorToFile(std::string message)
    {
        logToFile(LOG_TYPE_ERROR, message.c_str());
    }

    void logInfoToFile(const char *message, const char *context)
    {
        logToFile(LOG_TYPE_INFO, message, context);
    }

    void logErrorToFile(const char *message, const char *context)
    {
        logToFile(LOG_TYPE_ERROR, message, context);
    }

    void cleanupOldLogs()
    {
        DIR* dir = opendir(LOG_DIRECTORY);
        if (!dir) {
            return; // Directory doesn't exist or can't be opened
        }

        time_t now = time(NULL);
        time_t retentionTime = LOG_RETENTION_DAYS * 24 * 60 * 60; // Convert days to seconds

        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            // Check if filename starts with "debug_" and ends with ".log"
            std::string filename(entry->d_name);
            if (filename.find("debug_") == 0 && filename.find(".log") == filename.length() - 4) {
                // Build full path
                std::string fullPath = std::string(LOG_DIRECTORY) + "/" + filename;

                // Get file modification time
                struct stat fileInfo;
                if (stat(fullPath.c_str(), &fileInfo) == 0) {
                    // Calculate file age in seconds
                    time_t fileAge = now - fileInfo.st_mtime;

                    // Delete if older than retention period
                    if (fileAge > retentionTime) {
                        unlink(fullPath.c_str());
                    }
                }
            }
        }

        closedir(dir);
    }
}

#endif  // PKSE_PROD