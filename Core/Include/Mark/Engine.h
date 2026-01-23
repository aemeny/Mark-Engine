#pragma once

namespace Mark
{
    struct EngineAppInfo
    {
        const char* appName{ "My project" };
        const int appVersion[4]{ 0, 1, 0, 0 }; // Variant, Major, Minor, Patch
        const bool enableVulkanValidation{ false };
    };

    struct Engine
    {
        static int Run(const EngineAppInfo& _appInfo);

    private:
        Engine() = delete;
    };
} // namespace Mark