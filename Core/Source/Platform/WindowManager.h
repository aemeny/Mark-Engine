#pragma once
#include <memory>
#include <vector>

namespace Mark::Platform
{
    struct Window;
    
    struct WindowManager
    {
        WindowManager();
        ~WindowManager();

        Window& main();
        Window& create(int _width, int _height, const char* _title, bool _borderless = true);
        
        void pollAll() const;
        bool anyOpen() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
} // namespace Mark::Platform