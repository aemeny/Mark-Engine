#pragma once
#include <string_view>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace Mark::Error
{
    // Message only
    [[noreturn]] inline void error_impl(const char* _file, int _line, std::string_view _msg)
    {
        std::fprintf(stderr, "[ERROR CALL] %s:%d | %.*s\n",
            _file, _line,
            int(_msg.size()), _msg.data());
        std::fflush(stderr);
        std::abort();
    }

    // Message + related object (streamable via operator<<)
    template <class T>
    [[noreturn]] inline void error_impl(const char* _file, int _line, std::string_view _msg, const T& _related)
    {
        std::ostringstream os;
        os << _related;  // requires stream operator<< for T
        const auto s = os.str();
        std::fprintf(stderr, "[ERROR CALL] %s:%d | %.*s | related: %s\n",
            _file, _line,
            int(_msg.size()), _msg.data(),
            s.c_str());
        std::fflush(stderr);
        std::abort();
    }

    // message + printf-style args
    [[noreturn]] inline void errorf_impl(const char* file, int line, const char* fmt, ...) {
        std::fprintf(stderr, "[ERROR CALL] %s:%d | ", file, line);
        va_list args;
        va_start(args, fmt);
        std::vfprintf(stderr, fmt, args);
        va_end(args);
        if (fmt && *fmt && fmt[std::char_traits<char>::length(fmt) - 1] != '\n') {
            std::fputc('\n', stderr);
        }
        std::fflush(stderr);
        std::abort();
    }

} // namespace mark::error
#define MARK_ERROR(fmt, ...) ::Mark::Error::errorf_impl(__FILE__, __LINE__, (fmt), ##__VA_ARGS__)