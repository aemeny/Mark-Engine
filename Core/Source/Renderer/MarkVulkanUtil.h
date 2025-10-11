#pragma once
#include <volk.h>

#include <string_view>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <sstream>

typedef unsigned int uint;
#define CHECK_VK_RESULT(res, msg) \
    if (res != VK_SUCCESS){       \
        fprintf(stderr, "Error in %s:%d - %s, code %x\n", __FILE__, __LINE__, msg, res);  \
        exit(1);                  \
    }

inline const char* GetDebugSeverityStr(VkDebugUtilsMessageSeverityFlagBitsEXT _severity)
{
    switch (_severity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        return "Verbose";

    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        return "Info";

    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        return "Warning";

    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        return "Error";

    default:
        return "Invalid severity code";
    }

    return "NOT REAL SEVERITY STRENGTH!";
}

inline const char* GetDebugType(VkDebugUtilsMessageTypeFlagsEXT _type)
{
    switch (_type)
    {
    case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
        return "General";

    case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
        return "Validation";

    case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
        return "Performance";

    default:
        return "Invalid type code";
    }

    return "NOT REAL TYPE!";
}

namespace mark::error 
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
#define MARK_ERROR(fmt, ...) ::mark::error::errorf_impl(__FILE__, __LINE__, (fmt), ##__VA_ARGS__)