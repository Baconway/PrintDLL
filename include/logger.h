#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <windows.h>

enum LogLevel {
    LOG_LEVEL_OFF   = 0,
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARN  = 2,
    LOG_LEVEL_INFO  = 3,
    LOG_LEVEL_DEBUG = 4,
    LOG_LEVEL_TRACE = 5
};

class Logger {
public:
    static Logger& Instance() {
        static Logger instance;
        return instance;
    }

    static LogLevel ParseLevel(const std::string& str) {
        if (str.empty()) return LOG_LEVEL_DEBUG;
        if (_stricmp(str.c_str(), "OFF") == 0 || _stricmp(str.c_str(), "NONE") == 0 || str == "0") return LOG_LEVEL_OFF;
        if (_stricmp(str.c_str(), "ERROR") == 0 || _stricmp(str.c_str(), "ERR") == 0 || str == "1") return LOG_LEVEL_ERROR;
        if (_stricmp(str.c_str(), "WARN") == 0 || _stricmp(str.c_str(), "WARNING") == 0 || str == "2") return LOG_LEVEL_WARN;
        if (_stricmp(str.c_str(), "INFO") == 0 || str == "3") return LOG_LEVEL_INFO;
        if (_stricmp(str.c_str(), "DEBUG") == 0 || str == "4") return LOG_LEVEL_DEBUG;
        if (_stricmp(str.c_str(), "TRACE") == 0 || _stricmp(str.c_str(), "ALL") == 0 || str == "5") return LOG_LEVEL_TRACE;
        return LOG_LEVEL_DEBUG;
    }

    static const char* LevelToString(LogLevel lvl) {
        switch (lvl) {
            case LOG_LEVEL_OFF:   return "OFF";
            case LOG_LEVEL_ERROR: return "ERROR";
            case LOG_LEVEL_WARN:  return "WARN";
            case LOG_LEVEL_INFO:  return "INFO";
            case LOG_LEVEL_DEBUG: return "DEBUG";
            case LOG_LEVEL_TRACE: return "TRACE";
            default:              return "INFO";
        }
    }

    void Init(const std::string& filename = "PrintDLL.log", bool append = true) {
        std::lock_guard<std::mutex> lock(mutex_);
        OpenFileInternal(filename, append);
    }

    void Configure(bool enabled,
                   LogLevel level,
                   const std::string& filename,
                   bool append,
                   bool logToConsole,
                   bool logToDebugger,
                   bool includeTimestamp,
                   bool includeThreadId,
                   bool logApiCalls) {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = enabled;
        level_ = level;
        logToConsole_ = logToConsole;
        logToDebugger_ = logToDebugger;
        includeTimestamp_ = includeTimestamp;
        includeThreadId_ = includeThreadId;
        logApiCalls_ = logApiCalls;

        if (!enabled_ || level_ == LOG_LEVEL_OFF) {
            if (logFile_.is_open()) {
                logFile_.close();
            }
            return;
        }

        if (!filename.empty()) {
            OpenFileInternal(filename, append);
        }
    }

    bool IsEnabled() const { return enabled_ && level_ != LOG_LEVEL_OFF; }
    bool IsApiLoggingEnabled() const { return enabled_ && logApiCalls_ && level_ >= LOG_LEVEL_DEBUG; }
    LogLevel GetLevel() const { return level_; }
    std::string GetLogFilePath() {
        std::lock_guard<std::mutex> lock(mutex_);
        return currentFilePath_;
    }

    void Log(LogLevel msgLevel, const char* category, const std::string& message) {
        if (!enabled_ || level_ == LOG_LEVEL_OFF || msgLevel > level_) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream oss;

        if (includeTimestamp_) {
            SYSTEMTIME st;
            GetLocalTime(&st);
            char timeBuf[64];
            snprintf(timeBuf, sizeof(timeBuf), "[%04d-%02d-%02d %02d:%02d:%02d.%03d] ",
                     st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
            oss << timeBuf;
        }

        if (includeThreadId_) {
            oss << "[TID:" << std::setw(5) << GetCurrentThreadId() << "] ";
        }

        if (category && category[0] != '\0') {
            oss << "[" << category << "] ";
        } else {
            oss << "[" << LevelToString(msgLevel) << "] ";
        }

        oss << message << "\n";
        std::string fullMsg = oss.str();

        if (logToDebugger_) {
            OutputDebugStringA(fullMsg.c_str());
        }
        if (logToConsole_) {
            std::cout << fullMsg;
            std::cout.flush();
        }
        if (logFile_.is_open()) {
            logFile_ << fullMsg;
            logFile_.flush();
        }
    }

    // Legacy overload for compatibility
    void Log(const std::string& message) {
        Log(LOG_LEVEL_INFO, nullptr, message);
    }

private:
    Logger() = default;
    ~Logger() {
        if (logFile_.is_open()) logFile_.close();
    }

    static void CreateParentDirs(const std::string& filePath) {
        size_t pos = 0;
        while ((pos = filePath.find_first_of("\\/", pos)) != std::string::npos) {
            std::string dir = filePath.substr(0, pos);
            if (!dir.empty() && dir.back() != ':') {
                CreateDirectoryA(dir.c_str(), NULL);
            }
            pos++;
        }
    }

    void OpenFileInternal(const std::string& filename, bool append) {
        if (filename.empty()) return;
        char fullPath[MAX_PATH] = {0};
        if (GetFullPathNameA(filename.c_str(), MAX_PATH, fullPath, NULL) == 0) {
            strncpy_s(fullPath, filename.c_str(), _TRUNCATE);
        }
        std::string resolved(fullPath);
        if (logFile_.is_open() && _stricmp(currentFilePath_.c_str(), resolved.c_str()) == 0) {
            return;
        }
        if (logFile_.is_open()) {
            logFile_.close();
        }
        CreateParentDirs(resolved);
        std::ios_base::openmode mode = std::ios::out | (append ? std::ios::app : std::ios::trunc);
        logFile_.open(resolved, mode);
        if (logFile_.is_open()) {
            currentFilePath_ = resolved;
        }
    }

    std::ofstream logFile_;
    std::string currentFilePath_;
    std::mutex mutex_;
    bool enabled_ = true;
    LogLevel level_ = LOG_LEVEL_DEBUG;
    bool logToConsole_ = false;
    bool logToDebugger_ = true;
    bool includeTimestamp_ = true;
    bool includeThreadId_ = true;
    bool logApiCalls_ = true;
};

#define LOG_ERR(msg)   Logger::Instance().Log(LOG_LEVEL_ERROR, "ERROR", (msg))
#define LOG_WARN(msg)  Logger::Instance().Log(LOG_LEVEL_WARN,  "WARN",  (msg))
#define LOG_INFO(msg)  Logger::Instance().Log(LOG_LEVEL_INFO,  "INFO",  (msg))
#define LOG_DEBUG(msg) Logger::Instance().Log(LOG_LEVEL_DEBUG, "DEBUG", (msg))
#define LOG_TRACE(msg) Logger::Instance().Log(LOG_LEVEL_TRACE, "TRACE", (msg))

#define LOG_CAT(lvl, cat, msg) Logger::Instance().Log((lvl), (cat), (msg))
#define LOG_API(msg)   do { if (Logger::Instance().IsApiLoggingEnabled()) Logger::Instance().Log(LOG_LEVEL_DEBUG, "API", (msg)); } while(0)
#define LOG_STUB(msg)  Logger::Instance().Log(LOG_LEVEL_INFO,  "STUB",    (msg))
#define LOG_CFG(msg)   Logger::Instance().Log(LOG_LEVEL_INFO,  "CONFIG",  (msg))
#define LOG_PRINTER(lvl, msg) Logger::Instance().Log((lvl), "PRINTER", (msg))

