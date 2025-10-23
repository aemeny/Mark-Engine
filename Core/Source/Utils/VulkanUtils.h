#pragma once
#include "ErrorHandling.h"
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