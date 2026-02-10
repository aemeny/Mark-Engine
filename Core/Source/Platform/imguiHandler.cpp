#include "imguiHandler.h"
#include "Window.h"

#include "imgui_impl_vulkan.h"
#include "imgui_impl_glfw.h"

namespace Mark::Platform
{
    void ImGuiHandler::destroy()
    {
        m_imguiRenderer.destroy();

        ImGui::DestroyContext();

        m_mainWindowHandler = nullptr;
        m_vulkanCoreRef = nullptr;
        m_ImGuiSettings = nullptr;
    }

    void ImGuiHandler::initialize(const Settings::ImGuiSettings& _settings, WindowToVulkanHandler* _mainWindowHandler, VulkanCore* _vulkanCoreRef)
    {
        m_ImGuiSettings = &_settings;
        m_mainWindowHandler = _mainWindowHandler;
        m_vulkanCoreRef = _vulkanCoreRef;

        ImGui::CreateContext();

        m_imguiRenderer.initialize();
        
        handleUISettings();
    }

    void ImGuiHandler::updateGUI()
    {        
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        drawMainMenuBar();

        setScreenDocking();
        drawDockSpace();

        ImGui::End(); // Main dock space end
        ImGui::Render();
    }

    void ImGuiHandler::drawMainMenuBar()
    {
        setMainMenuStyle();

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Settings"))
            {
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Window"))
            {
                for (GUIWindow& window : m_GUIWindows)
                {
                    if (window.isOpen)
                        ImGui::MenuItem(window.title.c_str(), nullptr, window.isOpen);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // Back to body style for everything else
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(3);
        ImGui::PopFont();
    }


    void ImGuiHandler::setMainMenuStyle() const
    {
        // Change to smaller font and black text for better contrast on white menu bar
        ImGui::PushFont(m_fonts.menu);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

        // Add extra padding around text
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 6.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(9.0f, 6.5f));

        // Remove dark border around the bar
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    }

    void ImGuiHandler::drawMainToolbar() const
    {
        ImGuiStyle& style = ImGui::GetStyle();

        // Height proportional to current UI frame height so it scales with DPI/font
        const float frameH = ImGui::GetFrameHeight();
        const float toolbarHeight = frameH * 1.7f;

        // Dark strip background
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.005f, 0.005f, 0.005f, 1.0f));

        if (ImGui::BeginChild("MainToolbar", 
            ImVec2(0.0f, toolbarHeight), 
            false, // border
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            const float textHeight = ImGui::GetTextLineHeight();
            const float midTextY = (toolbarHeight - textHeight) * 0.5f;


            // ---- Left: Engine name ----
            ImGui::PushFont(m_fonts.bold);
            const char* leftLabel = "Mark";
            const float leftTextWidth = ImGui::CalcTextSize(leftLabel).x;
            const float leftMargin = 40.0f; // distance from left edge
            ImGui::SetCursorPos(ImVec2(leftMargin, midTextY));
            ImGui::TextUnformatted("Mark");
            ImGui::PopFont(); // Reset


            // ---- Center: Play / Pause / Stop ----
            const float buttonWidth = 85.0f;
            const float buttonHeight = 30.0f;
            ImVec2 buttonSize(buttonWidth, buttonHeight);

            // Local style just for these three buttons
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 3.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

            // Vertical centering inside the toolbar window
            float toolbarHeightPx = ImGui::GetWindowHeight();
            float centerY = (toolbarHeightPx - buttonHeight) * 0.5f;
            ImGui::SetCursorPosY(centerY);

            // Horizontal centering across the whole toolbar
            float contentMinX = ImGui::GetWindowContentRegionMin().x;
            float contentMaxX = ImGui::GetWindowContentRegionMax().x;
            float fullWidth = contentMaxX - contentMinX;

            float totalButtonsWidth = 3.0f * buttonSize.x + 2.0f * style.ItemSpacing.x;
            float startX = contentMinX + (fullWidth - totalButtonsWidth) * 0.5f;
            float buttonSpacing = style.ItemSpacing.x - 2.2f;
            ImGui::SetCursorPosX(startX);

            if (ImGui::Button("Play", buttonSize)) {
                // TODO: hook up play
            }

            ImGui::SameLine(0.0f, buttonSpacing);
            if (ImGui::Button("Pause", buttonSize)) {
                // TODO: hook up pause
            }

            // When editor had a play mode seperate from editor mode
            //ImGui::SameLine(0.0f, buttonSpacing);
            //if (ImGui::Button("Stop", buttonSize)) {
            //    // TODO: hook up stop
            //}
            
            ImGui::PopStyleVar(2);


            // ---- Right: "Game View" label ----
            const char* rightLabel = "Game View";
            const float rightTextWidth = ImGui::CalcTextSize(rightLabel).x;
            const float rightMargin = 25.0f; // distance from right edge
            const float rightTextX = fullWidth - rightTextWidth - rightMargin;

            ImGui::SetCursorPos(ImVec2(rightTextX, midTextY));
            ImGui::TextUnformatted(rightLabel);
        }
        ImGui::EndChild();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }

    void ImGuiHandler::drawDockSpace()
    {
        // Windows to apear across the scren (GUIWindows)
        for (GUIWindow& window : m_GUIWindows)
        {
            if (window.isOpen && !*window.isOpen)
                continue; // Skip rendering this window if it's closed

            if (ImGui::Begin(window.title.c_str(), window.isOpen))
            {
                window.drawFunction(); // Renders contents
            }
            ImGui::End();
        }
    }

    void ImGuiHandler::setScreenDocking()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags dockFlags =
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_MenuBar |
            ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("MainDockSpace", nullptr, dockFlags);

        drawMainToolbar();

        ImGuiID dockspaceID = ImGui::GetID("MainDockSpaceID");
        ImGuiDockNodeFlags dockNodeFlags = ImGuiDockNodeFlags_PassthruCentralNode;
        ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), dockNodeFlags);

        ImGui::PopStyleVar(3);
    }

    void ImGuiHandler::handleUISettings()
    {
        ImGuiIO& io = ImGui::GetIO();

        // ---- ImGui Config ----
        int frameBufferWidth, frameBufferHeight;
        m_mainWindowHandler->m_windowRef.frameBufferSize(frameBufferWidth, frameBufferHeight);
        io.DisplaySize.x = static_cast<float>(frameBufferWidth);
        io.DisplaySize.y = static_cast<float>(frameBufferHeight);
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos; // Enable Keyboard to move mouse cursor
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;


        // ---- Font / Style Handling ----
        // DPI Scale
        float xscale = 1.0f, yscale = 1.0f;
        m_mainWindowHandler->m_windowRef.getWindowContentScale(xscale, yscale);
        float dpiScale = std::max(xscale, yscale); // 1.0 on 1080p, ~2.0 on 4K

        // Apply DPI scaling to ImGui style and fonts
        float uiScale = dpiScale * m_ImGuiSettings->fontScale;
        const float baseFontSize = 16.0f * uiScale;  // 16px as a 1080p baseline

        // ---- Add custom font ----
        io.Fonts->Clear();
        std::string fontPath = std::string(MARK_ENGINE_CORE_DIR) + "/Handling/Fonts/MartianMono-Regular.ttf";
        std::string fontPathBold = std::string(MARK_ENGINE_CORE_DIR) + "/Handling/Fonts/MartianMono-ExtraBold.ttf";

        m_fonts.body = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), baseFontSize);
        m_fonts.menu = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), baseFontSize * 0.85f);
        m_fonts.bold = io.Fonts->AddFontFromFileTTF(fontPathBold.c_str(), baseFontSize * 0.95f);

        io.FontDefault = m_fonts.body;

        // ---- Style sizes ----
        ImGuiStyle& style = ImGui::GetStyle();
        style.FontScaleMain = m_ImGuiSettings->fontScale;
        style.ScaleAllSizes(uiScale);

        // ---- Colour and style theming ----
        applyTheme();
    }

    void ImGuiHandler::applyTheme()
    {
        ImGuiIO& io = ImGui::GetIO();
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        // ImGui Style
        switch (m_ImGuiSettings->colourStyle)
        {
        default:
        case Settings::ImGuiStyleColors::Dark:    ImGui::StyleColorsDark();    break;
        case Settings::ImGuiStyleColors::Classic: ImGui::StyleColorsClassic(); break;
        case Settings::ImGuiStyleColors::Light:   ImGui::StyleColorsLight();   break;
        }

        // Viewport handling
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 6.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        // Layout / Spacing
        style.WindowPadding = ImVec2(10.0f, 10.0f);
        style.FramePadding = ImVec2(8.0f, 5.0f);
        style.ItemSpacing = ImVec2(8.0f, 6.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
        style.ScrollbarSize = 14.0f;

        style.WindowRounding = 0.0f;
        style.ChildRounding = 0.0f;
        style.FrameRounding = 2.0f;
        style.PopupRounding = 2.0f;
        style.ScrollbarRounding = 0.0f;
        style.GrabRounding = 6.0f;
        style.TabRounding = 6.0f;

        style.WindowBorderSize = 0.0f;
        style.ChildBorderSize = 0.0f;
        style.PopupBorderSize = 0.0f;
        style.FrameBorderSize = 0.0f;
        style.TabBorderSize = 0.0f;

        // ---------- Base palette ----------
        ImVec4 col_bg = ImVec4(0.045f, 0.045f, 0.045f, 1.0f); // main window bg
        ImVec4 col_panel = ImVec4(0.23f, 0.23f, 0.23f, 1.0f); // panel / child bg
        ImVec4 col_panel_dark = ImVec4(0.14f, 0.14f, 0.14f, 1.0f); // very dark

        ImVec4 col_tab_active = ImVec4(0.05f, 0.05f, 0.05f, 1.0f); // active tab

        // Coral accent
        ImVec4 col_accent = ImVec4(0.96f, 0.47f, 0.40f, 1.0f); // #F57966
        ImVec4 col_accent_hi = ImVec4(1.00f, 0.60f, 0.52f, 1.0f); // brighter
        ImVec4 col_accent_soft = ImVec4(0.78f, 0.36f, 0.32f, 1.0f); // softer

        // Text
        ImVec4 col_text = ImVec4(0.92f, 0.92f, 0.92f, 1.0f);
        ImVec4 col_text_muted = ImVec4(0.60f, 0.60f, 0.60f, 1.0f);
        ImVec4 white = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
        ImVec4 black = ImVec4(0.005f, 0.005f, 0.005f, 1.0f);

        // --- Text ---
        colors[ImGuiCol_Text] = col_text;
        colors[ImGuiCol_TextDisabled] = col_text_muted;
        colors[ImGuiCol_TextSelectedBg] = ImVec4(col_accent.x, col_accent.y, col_accent.z, 0.35f);

        // --- Backgrounds ---
        colors[ImGuiCol_WindowBg] = col_bg;
        colors[ImGuiCol_ChildBg] = col_panel;
        colors[ImGuiCol_PopupBg] = col_panel_dark;

        colors[ImGuiCol_Border] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

        // --- Frames / controls ---
        colors[ImGuiCol_FrameBg] = col_panel;
        colors[ImGuiCol_FrameBgHovered] = col_accent_soft;
        colors[ImGuiCol_FrameBgActive] = col_accent;

        colors[ImGuiCol_CheckMark] = col_accent_hi;
        colors[ImGuiCol_SliderGrab] = col_accent_soft;
        colors[ImGuiCol_SliderGrabActive] = col_accent_hi;

        colors[ImGuiCol_Button] = col_panel;
        colors[ImGuiCol_ButtonHovered] = col_accent_soft;
        colors[ImGuiCol_ButtonActive] = col_accent;

        // --- Menu bar (top strip) ---
        colors[ImGuiCol_MenuBarBg] = white;

        // --- Window titles / dock tab bar ---
        colors[ImGuiCol_TitleBg] = black;
        colors[ImGuiCol_TitleBgActive] = black;
        colors[ImGuiCol_TitleBgCollapsed] = black;

        // --- Tabs ---
        colors[ImGuiCol_Tab] = black;
        colors[ImGuiCol_TabUnfocused] = black;
        colors[ImGuiCol_TabHovered] = col_tab_active;
        colors[ImGuiCol_TabActive] = col_tab_active;
        colors[ImGuiCol_TabUnfocusedActive] = col_tab_active;
        colors[ImGuiCol_TabSelectedOverline] = col_accent;
        colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(col_accent.x, col_accent.y, col_accent.z, 0.3f);

        // --- Docking ---
        colors[ImGuiCol_DockingEmptyBg] = black;
        colors[ImGuiCol_DockingPreview] = ImVec4(col_accent.x, col_accent.y, col_accent.z, 0.40f);

        // --- Separators / misc ---
        colors[ImGuiCol_Separator] = col_accent;
        colors[ImGuiCol_SeparatorHovered] = col_accent_soft;
        colors[ImGuiCol_SeparatorActive] = col_accent_hi;

        colors[ImGuiCol_ScrollbarBg] = black;
        colors[ImGuiCol_ScrollbarGrab] = black;
        colors[ImGuiCol_ScrollbarGrabHovered] = black;
        colors[ImGuiCol_ScrollbarGrabActive] = black;

        colors[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_ResizeGripHovered] = col_accent_soft;
        colors[ImGuiCol_ResizeGripActive] = col_accent_hi;

        // --- Navigation / focus ---
        colors[ImGuiCol_NavHighlight] = col_accent;
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(col_accent.x, col_accent.y, col_accent.z, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0, 0, 0, 0.4f);
        colors[ImGuiCol_DragDropTarget] = col_accent_hi;

        // Menu items-
        colors[ImGuiCol_Header] = col_panel;
        colors[ImGuiCol_HeaderHovered] = col_accent_soft;
        colors[ImGuiCol_HeaderActive] = col_accent;

        // --- Plots ---
        colors[ImGuiCol_PlotLines] = col_accent;
        colors[ImGuiCol_PlotLinesHovered] = col_accent_hi;
        colors[ImGuiCol_PlotHistogram] = col_accent;
        colors[ImGuiCol_PlotHistogramHovered] = col_accent_hi;
    }
}