#pragma once
#include "ErrorHandling.h"
#include "MarkUtils.h"
#include <volk.h>

#define CHECK_VK_RESULT(res, msg)      \
    do { if (res != VK_SUCCESS)        \
        MARK_ERROR("Error in %s:%d - %s, code %x\n", __FILE__, __LINE__, msg, res); } while(0)

#define REQ_FEATURE(feats, member)     \
    do { if (feats.member != VK_TRUE)  \
        MARK_ERROR("Required device feature not supported: %s", #member); } while(0)


inline const char* GetDebugSeverityStr(VkDebugUtilsMessageSeverityFlagBitsEXT _severity)
{
    switch (_severity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: return "Verbose";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:    return "Info";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: return "Warning";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:   return "Error";
    default:                                              return "Invalid severity code";
    }
    return "NOT REAL SEVERITY STRENGTH!";
}
inline ::Mark::Utils::Level VkSeverityToLevel(VkDebugUtilsMessageSeverityFlagBitsEXT _severity)
{
    switch (_severity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: return ::Mark::Utils::Level::Debug;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:    return ::Mark::Utils::Level::Info;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: return ::Mark::Utils::Level::Warn;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:   return ::Mark::Utils::Level::Error;
    default:                                              return ::Mark::Utils::Level::Trace;
    }
}
inline std::string GetDebugType(VkDebugUtilsMessageTypeFlagsEXT _type)
{
    std::string s;
    if (_type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)     s += "General|";
    if (_type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)  s += "Validation|";
    if (_type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) s += "Performance|";
    if (s.empty()) s = "Unknown";
    else s.pop_back(); // drop ending '|'
    return s;
}