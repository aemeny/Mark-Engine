#include <Mark/Engine.h>
using namespace Mark;

int main() 
{
    EngineAppInfo appInfo{
        .appName = "EngineTesting",
        .appVersion = {0, 1, 0, 0},
        .enableVulkanValidation = true
    };

    return Engine::Run(appInfo);
}