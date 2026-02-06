#pragma once

namespace Mark::Settings
{
    enum ImGuiStyleColors
    {
        Classic,
        Light,
        Dark
    };
    struct ImGuiSettings
    {
        const float fontScale;
        const ImGuiStyleColors colourStyle;

        constexpr ImGuiSettings(float scale = 0.95f, ImGuiStyleColors style = ImGuiStyleColors::Dark) :
            fontScale(scale), colourStyle(style)
        {}
        ImGuiSettings(const ImGuiSettings&) = default;
    };
}