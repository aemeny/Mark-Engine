#pragma once

namespace Mark
{
    struct EngineAppInfo
    {
        const char* appName{ "My project" };
        int appVersion[4]{ 0, 1, 0, 0 }; // Variant, Major, Minor, Patch
        bool enableVulkanValidation{ false };
    };

    struct Engine
    {
        static int Run(const EngineAppInfo& _appInfo);

    private:
        Engine() = delete;
    };
} // namespace Mark