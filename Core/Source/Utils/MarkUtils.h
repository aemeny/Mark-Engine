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
#include <functional>
#include <filesystem>
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
    // --------- CONSOLE LOGGING  ---------
    enum class Level : uint32_t { All = 0, Debug, Info, Warn, Error, Fatal };
    enum class Category : uint32_t { General = 0, Vulkan, GLFW, Window, Engine, System, Shader };
    constexpr uint64_t categoryBit(Category _c) { return 1ull << static_cast<uint32_t>(_c); }

    // Compile-time toggle (on in Debug, off in Release by default)
#ifndef MARK_LOG_ENABLED
    #ifdef NDEBUG
        #define MARK_LOG_ENABLED 0
    #else
        #define MARK_LOG_ENABLED 1
    #endif
#endif

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

        // Category controls (runtime)
        static void setCategoryMask(uint64_t _mask) { categoriesRef() = _mask; }
        static uint64_t categoryMask() { return categoriesRef(); }
        static void enableCategory(Category _c) { categoriesRef() |= categoryBit(_c); }
        static void disableCategory(Category _c) { categoriesRef() &= ~categoryBit(_c); }
        static bool isEnabled(Category _c) { return (categoriesRef() & categoryBit(_c)) != 0; }
        // Indent scope (per-thread)
        static void pushIndent() { indentRef()++; }
        static void popIndent() { if (indentRef() > 0) indentRef()--; }

        static void write(Level _lvl, const char* _file, int _line, const char* _func, const char* _fmt, ...)
        {
#if MARK_LOG_ENABLED
            if ((int)_lvl < (int)levelRef()) return;

            va_list args; va_start(args, _fmt);
            vwrite(_lvl, _file, _line, _func, /*category*/nullptr, _fmt, args);
            va_end(args);
#endif
        }

        // Category-aware writer
        static void writeCategory(Level _lvl, Category _cat, const char* _file, int _line, const char* _func, const char* _fmt, ...)
        {
#if MARK_LOG_ENABLED
            if (!isEnabled(_cat)) return;
            if ((int)_lvl < (int)levelRef()) return;
            va_list args; va_start(args, _fmt);
            vwrite(_lvl, _file, _line, _func, categoryName(_cat), _fmt, args);
            va_end(args);
#endif
        }

        // Continuation/body line (no timestamp/category/level header). Uses current indent
        static void writeCont(Level _lvl, Category _cat, const char* _fmt, ...)
        {
#if MARK_LOG_ENABLED
            if (!isEnabled(_cat)) return;
            if ((int)_lvl < (int)levelRef()) return;

            // Format user message
            char buf[2048];
            va_list args; va_start(args, _fmt);
            std::vsnprintf(buf, sizeof(buf), _fmt, args);
            va_end(args);

            std::lock_guard<std::mutex> lock(mutexRef());
            const char* indent = indentStr();
            std::fputs(indent, stdout);
            std::fputs(buf, stdout);
            std::fputc('\n', stdout);
            std::fflush(stdout);
#endif
        }

    private:
#if MARK_LOG_ENABLED
        // -------- core sink (shared by write / writeCategory) --------
        static void vwrite(Level _lvl, const char* _file, int _line, const char* _func,
            const char* _category, const char* _fmt, va_list _args)
        {
            // Format user message
            va_list copy; va_copy(copy, _args);
            int need = std::vsnprintf(nullptr, 0, _fmt, copy);
            va_end(copy);
            std::string msg = (need < 0) ? std::string("<formatting error>") : std::string();
            if (need >= 0)
            {
                msg.resize(need + 1);
                std::vsnprintf(msg.data(), msg.size(), _fmt, _args);
                if (!msg.empty() && msg.back() == '\0') msg.pop_back();
            }

            // Timestamp HH:MM:SS.mmm
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
            const char* name = levelName(_lvl);
            const char* cOn = colorRef() ? levelAnsi(_lvl) : "";
            const char* cOff = colorRef() ? "\x1b[0m" : "";

            const char* cat = _category ? _category : "General";
            constexpr int kCatW = 7;
            char prefix[200];
            std::snprintf(prefix, sizeof(prefix),
                "%02d:%02d:%02d.%03d [%-*s] [%-5s] ",
                tm.tm_hour, tm.tm_min, tm.tm_sec, (int)ms.count(), kCatW, cat, name
            );

            // Compute indent (2 spaces per level)
            const char* indent = indentStr();
            // Build a continuation pad so lines after '\n' align under the first char of the message.
            const size_t contCount = std::strlen(prefix) + std::strlen(indent);
            const std::string contPad(contCount, ' ');
            if (!msg.empty()) 
            {
                size_t pos = 0;
                const size_t add = contPad.size();
                while ((pos = msg.find('\n', pos)) != std::string::npos) 
                {
                    msg.insert(pos + 1, contPad);
                    pos += 1 + add;
                }
            }

            std::lock_guard<std::mutex> lock(mutexRef());

            // Console
            std::fputs(cOn, stdout);
            std::fputs(prefix, stdout);
            std::fputs(cOff, stdout);
            std::fputs(indent, stdout);
            std::fputs(msg.c_str(), stdout);
            std::fputc('\n', stdout);
            if (_lvl >= Level::Warn) std::fflush(stdout);

            // File (with file:line func suffix)
            if (FILE* fp = fileRef())
            {
                std::fprintf(fp, "%s%s%s (%s:%d %s)\n", prefix, indent, msg.c_str(), _file, _line, _func);
                if (_lvl >= Level::Warn) std::fflush(fp);
            }

            if (_lvl == Level::Fatal)
            {
                if (FILE* fp = fileRef()) std::fflush(fp);
                std::fflush(stdout);
            }
        }
        static const char* levelName(Level _l)
        {
            switch (_l)
            {
            case Level::All: return "ALL";
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
            case Level::All: return "\x1b[90m"; // gray
            case Level::Debug: return "\x1b[36m"; // cyan
            case Level::Info:  return "\x1b[32m"; // green
            case Level::Warn:  return "\x1b[33m"; // yellow
            case Level::Error: return "\x1b[31m"; // red
            default:           return "\x1b[35m"; // magenta
            }
        }
        // Categories
        static uint64_t& categoriesRef() { static uint64_t m = ~0ull; return m; }
        static const char* categoryName(Category _c)
        {
            switch (_c)
            {
            case Category::General: return "General";
            case Category::Vulkan:  return "Vulkan";
            case Category::GLFW:    return "GLFW";
            case Category::Window:  return "Window";
            case Category::Engine:  return "Engine";
            case Category::System:  return "System";
            case Category::Shader:  return "Shader";
            default:                return "Other";
            }
        }

        static std::mutex& mutexRef() { static std::mutex m; return m; }
        static FILE*& fileRef() { static FILE* f = nullptr; return f; }
        static Level& levelRef() { static Level l = Level::Info; return l; }
        static bool& colorRef() { static bool c = true; return c; }
        // Per-thread indent (scopes)
        static int& indentRef() { static thread_local int s = 0; return s; }
        static const char* indentStr() 
        {
            static thread_local char buf[128];
            int n = indentRef(); if (n < 0) n = 0;
            int count = n * 2; if (count > 120) count = 120;
            for (int i = 0; i < count; ++i) buf[i] = ' ';
            buf[count] = '\0';
            return buf;
        }
#endif
    };
    // RAII indentation helper (prints an optional section title)
    struct ScopeFmtTag {};
    struct ScopedIndent 
    {
        ScopedIndent(Category _cat = Category::General, Level _lvl = Level::Info, const char* _title = nullptr) :
            m_title(_title), m_cat(_cat), m_lvl(_lvl)
        {
            if (_title) 
                Logger::writeCategory(_lvl, _cat, __FILE__, __LINE__, __func__, "%s", _title);
        }
        ScopedIndent(ScopeFmtTag, Category _cat, Level _lvl, const char* _fmt, ...)
            : m_title(nullptr), m_cat(_cat), m_lvl(_lvl)
        {
            if (_fmt)
            {
                char buf[512];
                va_list args;
                va_start(args, _fmt);
                std::vsnprintf(buf, sizeof(buf), _fmt, args);
                va_end(args);
                Logger::writeCategory(_lvl, _cat, __FILE__, __LINE__, __func__, "%s", buf);
            }
        }
        ~ScopedIndent() = default;
    private:
        const char* m_title;
        Category    m_cat;
        Level       m_lvl;
    };
    // --------- End CONSOLE LOGGING  ---------



     // --------- ARRAY SIZE HELPER  ---------
     template <typename T, size_t N>
     constexpr size_t ArrayCount(const T(&)[N]) noexcept { return N; }
     // --------- End ARRAY SIZE HELPER  ---------


     // --------- HASH COMBINE HELPER  ---------
     // from: https://stackoverflow.com/a/57595105
     template <typename T, typename... Rest>
     void hashCombine(std::size_t& seed, const T& v, const Rest&... rest)
     {
         seed ^= std::hash<T>{}(v)+0x9e3779b9 + (seed << 6) + (seed >> 2);
         (hashCombine(seed, rest), ...);
     };
     // --------- End HASH COMBINE HELPER  ---------


     // --------- SHORT PATH FOR LOGGING HELPER  ---------
     inline std::string ShortPathForLog(std::string_view _absPath)
     {
         namespace fs = std::filesystem;
         fs::path path{ std::string(_absPath) };
         path = path.lexically_normal();

#ifdef MARK_PROJECT_ROOT
         fs::path root{ MARK_PROJECT_ROOT };
         root = root.lexically_normal();

         std::error_code ec;
         fs::path relative = fs::relative(path, root, ec);
         if (!ec && !relative.empty())
         {
             const std::string proj = root.filename().string();
             return proj + "/" + relative.generic_string();
         }
#endif
         // Fallback: look for common repo folder name in the string
         std::string s = path.string();
         if (size_t pos = s.find("Mark-Engine"); pos != std::string::npos)
             return s.substr(pos);

         // Last resort: just the filename
         return path.filename().string();
     }
     // --------- End SHORT PATH FOR LOGGING HELPER  ---------

} // namespace Mark::Utils


//  --------- Array count macro ---------
#define ARRAY_COUNT(arr) static_cast<uint32_t>(::Mark::Utils::ArrayCount(arr))
// --------- End Array count macro ---------


// --------- Logger macros ---------
#ifndef MARK_LOG_COLOR
    #define MARK_LOG_COLOR 1
#endif
#if MARK_LOG_COLOR
    #define MARK_COL_RESET  "\x1b[0m"
    #define MARK_COL_LABEL  "\x1b[38;5;88m"
    #define MARK_COL_LABEL2 "\x1b[38;5;170m"
    #define MARK_COL_LABEL3 "\x1b[32m"
#else
    #define MARK_COL_RESET  ""
    #define MARK_COL_LABEL  ""
#endif

#if MARK_LOG_ENABLED
    #define MARK_LOG_WRITE(lvl, fmt, ...) ::Mark::Utils::Logger::write((lvl), __FILE__, __LINE__, __func__, (fmt), ##__VA_ARGS__)
    #define MARK_TRACE(fmt, ...)   MARK_LOG_WRITE(::Mark::Utils::Level::Trace, fmt, ##__VA_ARGS__)
    #define MARK_DEBUG(fmt, ...)   MARK_LOG_WRITE(::Mark::Utils::Level::Debug, fmt, ##__VA_ARGS__)
    #define MARK_INFO(fmt, ...)    MARK_LOG_WRITE(::Mark::Utils::Level::Info,  fmt, ##__VA_ARGS__)
    #define MARK_WARN(fmt, ...)    MARK_LOG_WRITE(::Mark::Utils::Level::Warn,  fmt, ##__VA_ARGS__)
    // Non-fatal error log (distinct from MARK_ERROR which quickly aborts)
    #define MARK_LOG_ERROR(fmt, ...) MARK_LOG_WRITE(::Mark::Utils::Level::Error, fmt, ##__VA_ARGS__)
    // Fatal helper that logs AND then calls existing MARK_ERROR
    #define MARK_FATAL(fmt, ...) do {  \
       MARK_LOG_WRITE(::Mark::Utils::Level::Fatal, fmt, ##__VA_ARGS__); \
       MARK_ERROR(fmt, ##__VA_ARGS__); \
    } while(0)

    // Category-aware writing
    #define MARK_LOG_WRITE_C(lvl, cat, fmt, ...) ::Mark::Utils::Logger::writeCategory((lvl), (cat), __FILE__, __LINE__, __func__, (fmt), ##__VA_ARGS__)
    #define MARK_DEBUG_C(cat, fmt, ...)          MARK_LOG_WRITE_C(::Mark::Utils::Level::Debug, (cat), fmt, ##__VA_ARGS__)
    #define MARK_INFO_C(cat, fmt, ...)           MARK_LOG_WRITE_C(::Mark::Utils::Level::Info, (cat), fmt, ##__VA_ARGS__)
    #define MARK_WARN_C(cat, fmt, ...)           MARK_LOG_WRITE_C(::Mark::Utils::Level::Warn, (cat), fmt, ##__VA_ARGS__)
    #define MARK_LOG_ERROR_C(cat, fmt, ...)      MARK_LOG_WRITE_C(::Mark::Utils::Level::Error, (cat), fmt, ##__VA_ARGS__)

    // Indented section (prints a title once, indents all inner logs)
    #define MARK_SCOPE(title)         ::Mark::Utils::ScopedIndent _mark_scope_##__LINE__((title), ::Mark::Utils::Category::General)
    #define MARK_SCOPE_C(cat, title)  ::Mark::Utils::ScopedIndent _mark_scope_##__LINE__((title), (cat))
    #define MARK_IN_SCOPE(cat, lvl, fmt, ...)   ::Mark::Utils::Logger::writeCont((lvl), (cat), (fmt), ##__VA_ARGS__)
    // Scopes with a dynamic log level
#ifdef MARK_SCOPE_C_L
    #undef  MARK_SCOPE_C_L
#endif
    #define MARK_SCOPE_C_L(cat, lvl, fmt, ...)     ::Mark::Utils::ScopedIndent _mark_scope_##__LINE__(::Mark::Utils::ScopeFmtTag{}, (cat), (lvl), (fmt), ##__VA_ARGS__)
    #define MARK_SCOPE_L(lvl, title)               ::Mark::Utils::ScopedIndent _mark_scope_##__LINE__((title), ::Mark::Utils::Category::General, (lvl))
#else
    #define MARK_TRACE(...)         ((void)0)
    #define MARK_DEBUG(...)         ((void)0)
    #define MARK_INFO(...)          ((void)0)
    #define MARK_WARN(...)          ((void)0)
    #define MARK_LOG_ERROR(...)     ((void)0)
    #define MARK_FATAL(...)         do { MARK_ERROR("%s","Fatal"); } while(0)
    #define MARK_DEBUG_C(...)       ((void)0)
    #define MARK_INFO_C(...)        ((void)0)
    #define MARK_WARN_C(...)        ((void)0)
    #define MARK_ERROR_C(...)       ((void)0)
    #define MARK_SCOPE(title)       ((void)0)
    #define MARK_SCOPE_C(c, t)      ((void)0)
    #define MARK_SCOPE_L(l, t)      ((void)0)
    #define MARK_SCOPE_C_L(c, l, t) ((void)0)
    #define MARK_IN_SCOPE(...)      ((void)0)
#endif
// --------- End Logger macros ---------