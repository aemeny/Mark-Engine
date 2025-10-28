#pragma once
#include "ErrorHandling.h"
#include <mutex>
#include <chrono>
#include <thread>
#include <string>
#include <ctime>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#if defined(_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
        #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
    #endif
#endif

namespace Mark::Utils
{
    enum class Level : int { Trace = 0, Debug, Info, Warn, Error, Fatal };

    // Compile-time toggle (on in Debug, off in Release by default)
#ifndef MARK_LOG_ENABLED
    #ifdef NDEBUG
        #define MARK_LOG_ENABLED 0
    #else
        #define MARK_LOG_ENABLED 1
    #endif
#endif

// --------- Logger core ---------
    struct Logger 
    {
        static void init(const char* _filePath = nullptr, Level _minLevel = Level::Info, bool _useColor = true, bool _appendFile = false)
        {
#if MARK_LOG_ENABLED
            levelRef() = _minLevel;
            colorRef() = _useColor;
            if (_filePath && *_filePath)
            {
                fileRef() = std::fopen(_filePath, _appendFile ? "ab" : "wb");
            }

    #if defined(_WIN32)
            if (_useColor)
            {
                HANDLE handles[2] = { 
                    GetStdHandle(STD_OUTPUT_HANDLE), 
                    GetStdHandle(STD_ERROR_HANDLE) 
                };
                for (HANDLE handle : handles) 
                {
                    if (handle && handle != INVALID_HANDLE_VALUE) 
                    {
                        DWORD mode = 0;
                        if (GetConsoleMode(handle, &mode)) 
                        {
                            SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
                        }
                    }
                }
            }
    #endif
#endif
        }

        static void shutdown() 
        {
#if MARK_LOG_ENABLED
            std::lock_guard<std::mutex> lock(mutexRef());
            if (fileRef()) 
            { 
                std::fflush(fileRef()); 
                std::fclose(fileRef()); 
                fileRef() = nullptr; 
            }
#endif
        }

        static void setLevel(Level _l) { levelRef() = _l; }
        static Level level() { return levelRef(); }

        static void write(Level _lvl, const char* _file, int _line, const char* _func, const char* _fmt, ...)
        {
#if MARK_LOG_ENABLED
            if ((int)_lvl < (int)levelRef()) return;

            // Format user message
            va_list args;
            va_start(args, _fmt);
            va_list copy;
            va_copy(copy, args);
            int need = std::vsnprintf(nullptr, 0, _fmt, copy);
            va_end(copy);

            std::string msg;
            if (need < 0) 
            {
                msg = "<formatting error>";
            }
            else 
            {
                msg.resize(need + 1);
                std::vsnprintf(msg.data(), msg.size(), _fmt, args);
                if (!msg.empty() && msg.back() == '\0') msg.pop_back();
            }
            va_end(args);

            // Timestamp HH:MM:SS.mmm + thread id
            using namespace std::chrono;
            auto now = system_clock::now();
            auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
            std::time_t tt = system_clock::to_time_t(now);
            std::tm tm{};
    #if defined(_WIN32)
            localtime_s(&tm, &tt);
        #else
            localtime_r(&tt, &tm);
    #endif
            size_t tid = std::hash<std::thread::id>{}(std::this_thread::get_id()) & 0xFFFF;

            const char* name = levelName(_lvl);
            const char* cOn = colorRef() ? levelAnsi(_lvl) : "";
            const char* cOff = colorRef() ? "\x1b[0m" : "";

            char prefix[128];
            std::snprintf(prefix, sizeof(prefix),
                "%02d:%02d:%02d.%03d [T:%04zx] %-5s ",
                tm.tm_hour, tm.tm_min, tm.tm_sec, (int)ms.count(), tid, name);

            std::lock_guard<std::mutex> lock(mutexRef());

            // Console
            std::fputs(cOn, stdout);
            std::fputs(prefix, stdout);
            std::fputs(cOff, stdout);
            std::fputs(msg.c_str(), stdout);
            std::fputc('\n', stdout);
            if (_lvl >= Level::Warn) 
                std::fflush(stdout);

            // File (with file:line func suffix)
            if (FILE* fp = fileRef()) 
            {
                std::fprintf(fp, "%s%s (%s:%d %s)\n", prefix, msg.c_str(), _file, _line, _func);
                if (_lvl >= Level::Warn) std::fflush(fp);
            }

            if (_lvl == Level::Fatal) 
            {
                if (FILE* fp = fileRef()) 
                    std::fflush(fp);
                std::fflush(stdout);
            }
#endif
        }

    private:
#if MARK_LOG_ENABLED
        static const char* levelName(Level _l) 
        {
            switch (_l) 
            {
            case Level::Trace: return "TRACE";
            case Level::Debug: return "DEBUG";
            case Level::Info:  return "INFO";
            case Level::Warn:  return "WARN";
            case Level::Error: return "ERROR";
            default:           return "FATAL";
            }
        }
        static const char* levelAnsi(Level _l) 
        {
            switch (_l) 
            {
            case Level::Trace: return "\x1b[90m"; // gray
            case Level::Debug: return "\x1b[36m"; // cyan
            case Level::Info:  return "\x1b[32m"; // green
            case Level::Warn:  return "\x1b[33m"; // yellow
            case Level::Error: return "\x1b[31m"; // red
            default:           return "\x1b[35m"; // magenta
            }
        }
        static std::mutex& mutexRef() { static std::mutex m; return m; }
        static FILE*& fileRef() { static FILE* f = nullptr; return f; }
        static Level& levelRef() { static Level l = Level::Info; return l; }
        static bool& colorRef() { static bool c = true; return c; }
#endif
    };
} // namespace Mark::Utils

// --------- Logger macros ---------
#if MARK_LOG_ENABLED
    #define MARK_LOG_WRITE(lvl, fmt, ...) ::Mark::Utils::Logger::write((lvl), __FILE__, __LINE__, __func__, (fmt), ##__VA_ARGS__)
    #define MARK_TRACE(fmt, ...)   MARK_LOG_WRITE(::Mark::Utils::Level::Trace, fmt, ##__VA_ARGS__)
    #define MARK_DEBUG(fmt, ...)   MARK_LOG_WRITE(::Mark::Utils::Level::Debug, fmt, ##__VA_ARGS__)
    #define MARK_INFO(fmt, ...)    MARK_LOG_WRITE(::Mark::Utils::Level::Info,  fmt, ##__VA_ARGS__)
    #define MARK_WARN(fmt, ...)    MARK_LOG_WRITE(::Mark::Utils::Level::Warn,  fmt, ##__VA_ARGS__)
    // Non-fatal error log (distinct from MARK_ERROR which quickly aborts)
    #define MARK_LOG_ERROR(fmt, ...) MARK_LOG_WRITE(::Mark::Utils::Level::Error, fmt, ##__VA_ARGS__)
    // Fatal helper that logs AND then calls existing MARK_ERROR
    #define MARK_FATAL(fmt, ...) do { \
       MARK_LOG_WRITE(::Mark::Utils::Level::Fatal, fmt, ##__VA_ARGS__); \
       MARK_ERROR(fmt, ##__VA_ARGS__); \
  } while(0)
#else
    #define MARK_TRACE(...)      ((void)0)
    #define MARK_DEBUG(...)      ((void)0)
    #define MARK_INFO(...)       ((void)0)
    #define MARK_WARN(...)       ((void)0)
    #define MARK_LOG_ERROR(...)  ((void)0)
    #define MARK_FATAL(...)      do { MARK_ERROR("%s","Fatal"); } while(0)
#endif