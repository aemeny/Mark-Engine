#pragma once
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Mark::Fatal
{
    inline void ensure_newline(const char* fmt) noexcept
    {
        if (!fmt || !*fmt) { std::fputc('\n', stderr); return; }
        const size_t n = std::strlen(fmt);
        if (n > 0 && fmt[n - 1] != '\n') std::fputc('\n', stderr);
    }

    [[noreturn]] inline void abort_now() noexcept
    {
        std::fflush(stderr);
        std::abort();
    }

    [[noreturn]] inline void fatalf_impl(const char* file, int line, const char* fmt, ...) noexcept
    {
        std::fprintf(stderr, "[FATAL] %s:%d | ", file, line);
        va_list args;
        va_start(args, fmt);
        if (fmt) std::vfprintf(stderr, fmt, args);
        va_end(args);
        ensure_newline(fmt);
        abort_now();
    }
} // namespace Mark::Fatal
#define MARK_FATAL_STOP(fmt, ...) ::Mark::Fatal::fatalf_impl(__FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#define MARK_ABORT()              ::Mark::Fatal::abort_now()