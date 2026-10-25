// Copyright (c) - SurgeTechnologies - All rights reserved
#include "ProjectBrowser.hpp"

#include "Surge/Core/Core.hpp"
#include "Surge/Core/Project.hpp"
#include "Surge/Core/Events/Event.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/ECS/Scene.hpp"
#include "Surge/Utility/FileDialogs.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Utility/Platform.hpp"

#include "Editor.hpp"
#include "Panels/ContentBrowserPanel.hpp"
#include "Utility/ImGuiAux.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <json/json.hpp>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <format>
#include <iterator>

namespace Surge
{
    namespace
    {
        constexpr const char* RECENT_PROJECTS_FILENAME = "RecentProjects.json";
        constexpr const char* PROJECT_EXTENSION = ".surgeproj";
        constexpr const char* PROJECT_FILTER = "Surge Project (*.surgeproj)\0*.surgeproj\0All Files (*.*)\0*.*\0";
        constexpr size_t MAX_RECENT_PROJECTS = 20; // Pinned projects don't count and are never evicted

        constexpr float SIDEBAR_WIDTH = 248.0f;
        constexpr float PAGE_PADDING = 36.0f;
        constexpr float CONTROL_HEIGHT = 36.0f;
        constexpr float ROW_HEIGHT = 66.0f;
        constexpr float ROW_SPACING = 6.0f;
        constexpr float ROUNDING = 8.0f;
        constexpr float BUTTON_ROUNDING = 2.0f;
        constexpr float FORM_MAX_WIDTH = 640.0f;
        constexpr float CARD_SHADOW_END = 0.5f;

        namespace Palette
        {
            constexpr ImVec4 BORDER = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);
            constexpr ImVec4 WINDOW_BG = ImVec4(0.074f, 0.074f, 0.074f, 1.0f);
            constexpr ImVec4 SIDEBAR_BG = ImVec4(0.074f, 0.074f, 0.074f, 1.0f);

            constexpr ImVec4 CARD = ImVec4(0.067f, 0.067f, 0.067f, 1.0f);
            constexpr ImVec4 CARD_HOVERED = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
            constexpr ImVec4 CARD_SELECTED = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);

            constexpr ImVec4 CONTROL = ImVec4(0.118f, 0.118f, 0.126f, 1.0f);
            constexpr ImVec4 CONTROL_HOVERED = ImVec4(0.158f, 0.158f, 0.168f, 1.0f);
            constexpr ImVec4 CONTROL_ACTIVE = ImVec4(0.190f, 0.190f, 0.200f, 1.0f);

            constexpr ImVec4 TEXT = ImVec4(0.930f, 0.930f, 0.930f, 1.0f);
            constexpr ImVec4 TEXT_MUTED = ImVec4(0.540f, 0.540f, 0.540f, 1.0f);
            constexpr ImVec4 TEXT_FAINT = ImVec4(0.340f, 0.340f, 0.340f, 1.0f);
            constexpr ImVec4 ON_ACCENT = ImVec4(0.060f, 0.045f, 0.020f, 1.0f);
            constexpr ImVec4 ACCENT = ImGuiAux::Colors::ThemeColor1;
            constexpr ImVec4 ACCENT_HOVERED = ImVec4(1.0f, 0.720f, 0.220f, 1.0f);
            constexpr ImVec4 DANGER = ImVec4(0.920f, 0.340f, 0.320f, 1.0f);
            constexpr ImVec4 SUCCESS = ImVec4(0.420f, 0.800f, 0.470f, 1.0f);

        } // namespace Palette

        // Whole pixels only: ImGui bakes glyphs at the rounded size and scales the quads to a fractional one, which blurs the text
        constexpr float FONT_TITLE = 28.0f;
        constexpr float FONT_BRAND = 30.0f;
        constexpr float FONT_HEADING = 21.0f;
        constexpr float FONT_NAV = 18.0f;
        constexpr float FONT_NAME = 17.0f;
        constexpr float FONT_BUTTON = 16.0f;
        constexpr float FONT_SMALL = 14.0f;
        constexpr float FONT_CAPTION = 13.0f;
        constexpr float FONT_BADGE = 12.0f;

        enum class ButtonStyle
        {
            PRIMARY,
            SECONDARY,
            GHOST
        };

        ImU32 Col(const ImVec4& color, float alpha = 1.0f)
        {
            return ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, color.w * alpha));
        }

        ImFont* RegularFont() { return ImGui::GetIO().Fonts->Fonts[0]; }
        ImFont* BoldFont() { return ImGui::GetIO().Fonts->Fonts[1]; } // VulkanImGui.cpp

        // Rounded tint, strongest (the color's alpha) on the left edge (right edge when fromRight), fading to nothing at endFraction of the width (0..1)
        void FillShadowRect(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, float rounding, ImU32 color, float endFraction = 1.0f, bool fromRight = false)
        {
            const int firstVertex = drawList->VtxBuffer.Size;
            drawList->AddRectFilled(min, max, color, rounding);

            const float invWidth = 1.0f / std::max((max.x - min.x) * endFraction, 1.0f);
            for(int i = firstVertex; i < drawList->VtxBuffer.Size; i++)
            {
                ImDrawVert& vertex = drawList->VtxBuffer[i];
                const float distance = fromRight ? max.x - vertex.pos.x : vertex.pos.x - min.x;
                const float fade = 1.0f - std::clamp(distance * invWidth, 0.0f, 1.0f);
                const ImU32 alpha = static_cast<ImU32>(static_cast<float>(vertex.col >> IM_COL32_A_SHIFT) * fade);
                vertex.col = (vertex.col & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT); // The anti-aliased fringe keeps its zero alpha
            }
        }

        // Pixel rect ImGui draws without anti-aliasing, stays sharp
        void FillRect(ImDrawList* drawList, float x, float y, float width, float height, ImU32 color)
        {
            const float left = std::floor(x);
            const float top = std::floor(y);
            drawList->AddRectFilled(ImVec2(left, top), ImVec2(left + width, top + height), color);
        }

        void DrawText(ImDrawList* drawList, ImFont* font, float fontSize, float x, float y, ImU32 color, const char* text)
        {
            drawList->AddText(font, fontSize, ImVec2(std::floor(x), std::floor(y)), color, text);
        }

        float ActionButtonWidth(const char* label)
        {
            return std::ceil(BoldFont()->CalcTextSizeA(FONT_BUTTON, FLT_MAX, 0.0f, label).x + 32.0f);
        }

        // Custom drawn so labels and states stay consistent across the browser
        bool ActionButton(const char* id, const char* label, ButtonStyle style, const ImVec2& size, bool enabled = true)
        {
            const ImVec2 min = ImGui::GetCursorScreenPos();
            const ImVec2 max(min.x + size.x, min.y + size.y);

            ImGui::BeginDisabled(!enabled);
            const bool pressed = ImGui::InvisibleButton(id, size);
            ImGui::EndDisabled();
            const bool hovered = ImGui::IsItemHovered();
            const bool held = ImGui::IsItemActive();

            ImVec4 background = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
            ImVec4 text = Palette::TEXT;
            switch(style)
            {
                case ButtonStyle::PRIMARY:
                    background = !enabled ? Palette::CONTROL : (hovered && !held ? Palette::ACCENT_HOVERED : Palette::ACCENT);
                    text = enabled ? Palette::ON_ACCENT : Palette::TEXT_FAINT;
                    break;
                case ButtonStyle::SECONDARY:
                    background = held ? Palette::CONTROL_ACTIVE : (hovered ? Palette::CONTROL_HOVERED : Palette::CONTROL);
                    text = enabled ? Palette::TEXT : Palette::TEXT_FAINT;
                    break;
                case ButtonStyle::GHOST:
                    background = held ? Palette::CONTROL_ACTIVE : (hovered ? Palette::CONTROL : background);
                    text = !enabled ? Palette::TEXT_FAINT : (hovered ? Palette::TEXT : Palette::TEXT_MUTED);
                    break;
            }

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(min, max, Col(background), BUTTON_ROUNDING);
            if(style == ButtonStyle::SECONDARY)
                drawList->AddRect(min, max, Col(Palette::BORDER), BUTTON_ROUNDING);

            const ImVec2 textSize = BoldFont()->CalcTextSizeA(FONT_BUTTON, FLT_MAX, 0.0f, label);
            DrawText(drawList, BoldFont(), FONT_BUTTON, min.x + (size.x - textSize.x) * 0.5f, min.y + (size.y - textSize.y) * 0.5f, Col(text), label);
            return pressed;
        }

        // "..." button, three pixel squares
        bool MoreButton(const char* id, float size, bool menuOpen)
        {
            const ImVec2 min = ImGui::GetCursorScreenPos();
            const bool pressed = ImGui::InvisibleButton(id, ImVec2(size, size));
            const bool hovered = ImGui::IsItemHovered();

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            if(hovered || menuOpen)
                drawList->AddRectFilled(min, ImVec2(min.x + size, min.y + size), Col(ImGui::IsItemActive() || menuOpen ? Palette::CONTROL_ACTIVE : Palette::CONTROL_HOVERED), BUTTON_ROUNDING);

            const ImU32 color = Col(hovered || menuOpen ? Palette::TEXT : Palette::TEXT_MUTED);
            const float centerX = std::floor(min.x + size * 0.5f);
            const float centerY = std::floor(min.y + size * 0.5f);
            for(int i = -1; i <= 1; i++)
                FillRect(drawList, centerX - 1.0f + static_cast<float>(i) * 6.0f, centerY - 1.0f, 3.0f, 3.0f, color);
            return pressed;
        }

        float BadgeWidth(const char* text)
        {
            return std::ceil(BoldFont()->CalcTextSizeA(FONT_BADGE, FLT_MAX, 0.0f, text).x) + 12.0f;
        }

        // Returns the width taken
        float DrawBadge(ImDrawList* drawList, const ImVec2& pos, const char* text, const ImVec4& color)
        {
            const float width = BadgeWidth(text);
            const float x = std::floor(pos.x);
            const float y = std::floor(pos.y);
            DrawText(drawList, BoldFont(), FONT_BADGE, x + 6.0f, y + 3.0f, Col(color), text);
            return width;
        }

        void DrawTextCentered(ImDrawList* drawList, ImFont* font, float fontSize, float centerX, float y, ImU32 color, const char* text)
        {
            const float width = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text).x;
            DrawText(drawList, font, fontSize, centerX - width * 0.5f, y, color, text);
        }

        void FieldLabel(const char* text)
        {
            ImGui::PushFont(BoldFont(), FONT_CAPTION);
            ImGui::PushStyleColor(ImGuiCol_Text, Palette::TEXT_MUTED);
            ImGui::TextUnformatted(text);
            ImGui::PopStyleColor();
            ImGui::PopFont();
        }

        void ItemOutline(const ImVec4& color)
        {
            ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), Col(color), 6.0f, 0, 1.0f);
        }

        void FocusRing()
        {
            if(!ImGui::IsItemActive())
                return;

            ItemOutline(Palette::ACCENT);
            ImGui::SetNavCursorVisible(false); // The ring already marks the field, no second (nav) rectangle around it
        }

        struct ScopedInputStyle
        {
            SURGE_DISABLE_COPY_AND_MOVE(ScopedInputStyle);

            ScopedInputStyle()
            {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, std::floor((CONTROL_HEIGHT - ImGui::GetFontSize()) * 0.5f)));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, Palette::CONTROL);
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Palette::CONTROL_HOVERED);
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Palette::CONTROL_HOVERED);
                ImGui::PushStyleColor(ImGuiCol_Border, Palette::BORDER);
                ImGui::PushStyleColor(ImGuiCol_TextDisabled, Palette::TEXT_FAINT); // Hint text
            }

            ~ScopedInputStyle()
            {
                ImGui::PopStyleColor(5);
                ImGui::PopStyleVar(3);
            }
        };

        int64_t NowSeconds()
        {
            return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }

        void FormatDate(int64_t timestamp, const char* format, char* buffer, size_t size)
        {
            const std::time_t time = static_cast<std::time_t>(timestamp);
            std::tm localTime = {};
            localtime_s(&localTime, &time);
            if(std::strftime(buffer, size, format, &localTime) == 0 && size > 0)
                buffer[0] = '\0';
        }

        void FormatRelativeTime(int64_t timestamp, char* buffer, size_t size)
        {
            constexpr int64_t MINUTE = 60;
            constexpr int64_t HOUR = 60 * MINUTE;
            constexpr int64_t DAY = 24 * HOUR;

            const auto plural = [&](int64_t count, const char* unit) {
                std::snprintf(buffer, size, "%lld %s%s ago", static_cast<long long>(count), unit, count == 1 ? "" : "s");
            };

            if(timestamp <= 0)
            {
                std::snprintf(buffer, size, "Never opened");
                return;
            }

            const int64_t elapsed = std::max<int64_t>(0, NowSeconds() - timestamp);
            if(elapsed < MINUTE)
                std::snprintf(buffer, size, "Just now");
            else if(elapsed < HOUR)
                plural(elapsed / MINUTE, "minute");
            else if(elapsed < DAY)
                plural(elapsed / HOUR, "hour");
            else if(elapsed < 2 * DAY)
                std::snprintf(buffer, size, "Yesterday");
            else if(elapsed < 7 * DAY)
                plural(elapsed / DAY, "day");
            else if(elapsed < 30 * DAY)
                plural(elapsed / (7 * DAY), "week");
            else
                FormatDate(timestamp, "%b %d, %Y", buffer, size);
        }

        String Trim(const char* text)
        {
            const String value(text);
            const size_t first = value.find_first_not_of(" \t");
            if(first == String::npos)
                return {};
            return value.substr(first, value.find_last_not_of(" \t") - first + 1);
        }

        bool ContainsCaseInsensitive(const String& haystack, const char* needle)
        {
            if(needle[0] == '\0')
                return true;

            const char* needleEnd = needle + std::strlen(needle);
            return std::search(haystack.begin(), haystack.end(), needle, needleEnd, [](char a, char b) {
                       return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
                   }) != haystack.end();
        }

        bool LessCaseInsensitive(const String& a, const String& b)
        {
            return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [](char x, char y) {
                return std::tolower(static_cast<unsigned char>(x)) < std::tolower(static_cast<unsigned char>(y));
            });
        }

        // Nullptr when the name works as a folder and file name on Windows
        const char* ValidateProjectName(const String& name)
        {
            if(name.find_first_of("<>:\"/\\|?*") != String::npos)
                return "The name can't contain any of < > : \" / \\ | ? *";
            for(char c : name)
            {
                if(static_cast<unsigned char>(c) < 32)
                    return "The name contains invalid characters";
            }
            if(name.back() == '.')
                return "The name can't end with a dot";

            String stem = name.substr(0, name.find('.'));
            std::transform(stem.begin(), stem.end(), stem.begin(), [](char c) { return static_cast<char>(std::toupper(static_cast<unsigned char>(c))); });
            constexpr const char* reserved[] = { "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
                                                 "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9" };
            for(const char* word : reserved)
            {
                if(stem == word)
                    return "This name is reserved by Windows";
            }
            return nullptr;
        }

        // Project::Load throws on malformed JSON, a corrupt project file must not take the Editor down
        bool ReadProjectFile(const Path& path, Project& outProject)
        {
            try
            {
                outProject.Load(path);
            }
            catch(const nlohmann::json::exception&)
            {
                outProject.Clear();
            }
            return outProject.IsValid();
        }

        String GetDirectoryString(const String& filepath)
        {
            return Path(filepath).parent_path().generic_string();
        }

        Path GetRecentProjectsPath()
        {
            return Path(Platform::GetPersistantStoragePath()) / RECENT_PROJECTS_FILENAME;
        }

    } // namespace

    //////////////////////////////////////////////////////////////////////////
    // Lifetime
    //////////////////////////////////////////////////////////////////////////

    void ProjectBrowser::Init()
    {
        mAssetManager = Core::GetAssetManager();
        LoadRecentProjects();
        Refresh();
        RebuildVisibleList();
        if(!mVisible.empty())
            mSelectedPath = mRecentProjects[mVisible.front()].Filepath;
    }

    void ProjectBrowser::Refresh()
    {
        for(RecentProject& recent : mRecentProjects)
        {
            std::error_code error;
            recent.Missing = !std::filesystem::is_regular_file(recent.Filepath, error);
            if(recent.Missing)
                continue;

            Project project;
            if(ReadProjectFile(recent.Filepath, project))
            {
                recent.Name = project.Name;
                recent.Version = project.Version;
            }

            // Lists written before timestamps were stored: the project file's write time is the best guess
            if(recent.LastOpened == 0)
            {
                const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(recent.Filepath, error);
                if(!error)
                    recent.LastOpened = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::clock_cast<std::chrono::system_clock>(writeTime).time_since_epoch()).count();
            }
        }
        mVisibleDirty = true;
        mValidationDirty = true;
    }

    void ProjectBrowser::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<FilesDroppedEvent>([this](FilesDroppedEvent& dropEvent) {
            for(const String& dropped : dropEvent.GetPaths())
            {
                const Path path(dropped);
                std::error_code error;
                if(std::filesystem::is_directory(path, error))
                {
                    const Vector<Path> projectFiles = Filesystem::GetFilesInDirectory(path, PROJECT_EXTENSION);
                    if(!projectFiles.empty())
                    {
                        mDroppedProject = projectFiles.front().string();
                        return;
                    }
                }
                else if(path.extension().string() == PROJECT_EXTENSION)
                {
                    mDroppedProject = dropped;
                    return;
                }
            }
            SetError("Drop a .surgeproj file, or the folder holding one, to open a project.");
        });
    }

    //////////////////////////////////////////////////////////////////////////
    // Rendering
    //////////////////////////////////////////////////////////////////////////

    void ProjectBrowser::Render(float titleBarHeight)
    {
        if(!mDroppedProject.empty())
        {
            const String dropped = std::move(mDroppedProject);
            mDroppedProject.clear();
            OpenProject(dropped);
        }
        if(mVisibleDirty)
            RebuildVisibleList();

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + titleBarHeight));
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - titleBarHeight));
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Palette::WINDOW_BG);
        ImGui::PushStyleColor(ImGuiCol_Text, Palette::TEXT);
        ImGui::PushStyleColor(ImGuiCol_NavCursor, Palette::ACCENT); // Tab navigation on the New Project page

        constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                                 ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollWithMouse;
        if(ImGui::Begin("##SurgeProjectBrowser", nullptr, windowFlags))
        {
            const float height = ImGui::GetContentRegionAvail().y;
            RenderSidebar(height);
            ImGui::SameLine(0.0f, 0.0f);

            // Arrow keys drive the project list, keyboard navigation would fight it (the New Project page keeps it for Tab)
            ImGuiWindowFlags pageFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground;
            if(mPage == Page::PROJECTS)
                pageFlags |= ImGuiWindowFlags_NoNavInputs;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(PAGE_PADDING, PAGE_PADDING - 8.0f));
            const bool pageOpen = ImGui::BeginChild("##Page", ImVec2(0.0f, height), ImGuiChildFlags_AlwaysUseWindowPadding, pageFlags);
            ImGui::PopStyleVar();
            if(pageOpen)
            {
                if(mPage == Page::PROJECTS)
                    RenderProjectsPage();
                else
                    RenderNewProjectPage();
            }
            ImGui::EndChild();

            HandleShortcuts();
        }
        ImGui::End();

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(4);
    }

    void ProjectBrowser::RenderSidebar(float height)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Palette::SIDEBAR_BG);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 26.0f));
        const bool open = ImGui::BeginChild("##Sidebar", ImVec2(SIDEBAR_WIDTH, height), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNav);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        if(open)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 windowPos = ImGui::GetWindowPos();
            const ImU32 shadowLeft = IM_COL32(0, 0, 0, 10);
            const ImU32 shadowRight = IM_COL32(0, 0, 0, 200);
            drawList->AddRectFilledMultiColor(windowPos, ImVec2(windowPos.x + SIDEBAR_WIDTH, windowPos.y + height), shadowLeft, shadowRight, shadowRight, shadowLeft);
            FillRect(drawList, windowPos.x + SIDEBAR_WIDTH - 1.0f, windowPos.y, 1.0f, height, Col(Palette::BORDER));

            // Brand
            ImGuiAux::ShiftCursorX(10.0f);
            ImGui::PushFont(BoldFont(), FONT_BRAND);
            ImGui::TextColored(Palette::ACCENT, "SURGE");
            ImGui::PopFont();
            ImGuiAux::ShiftCursorX(10.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, Palette::TEXT_MUTED);
#ifdef SURGE_DEBUG
            ImGui::TextUnformatted("Project Hub  DEBUG");
#else
            ImGui::TextUnformatted("Project Hub");
#endif
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0.0f, 18.0f));

            const auto navItem = [&](const char* id, const char* label, bool active, int count) {
                constexpr float itemHeight = 40.0f;
                const float width = ImGui::GetContentRegionAvail().x;
                const ImVec2 min = ImGui::GetCursorScreenPos();
                const ImVec2 max(min.x + width, min.y + itemHeight);
                const bool pressed = ImGui::InvisibleButton(id, ImVec2(width, itemHeight));
                const bool hovered = ImGui::IsItemHovered();

                if(active || hovered)
                    drawList->AddRectFilled(min, max, Col(active ? Palette::CONTROL_HOVERED : Palette::CONTROL), 6.0f);
                if(active)
                    FillRect(drawList, min.x, min.y + 10.0f, 3.0f, itemHeight - 20.0f, Col(Palette::ACCENT));

                const ImU32 textColor = Col(active || hovered ? Palette::TEXT : Palette::TEXT_MUTED);
                DrawText(drawList, BoldFont(), FONT_NAV, min.x + 16.0f, min.y + (itemHeight - FONT_NAV) * 0.5f, textColor, label);

                if(count > 0)
                {
                    char countText[16];
                    std::snprintf(countText, sizeof(countText), "%d", count);
                    const float countWidth = std::ceil(RegularFont()->CalcTextSizeA(FONT_CAPTION, FLT_MAX, 0.0f, countText).x);
                    const ImVec2 pillMin(std::floor(max.x - countWidth - 24.0f), std::floor(min.y + (itemHeight - 20.0f) * 0.5f));
                    drawList->AddRectFilled(pillMin, ImVec2(max.x - 10.0f, pillMin.y + 20.0f), Col(Palette::WINDOW_BG), 10.0f);
                    DrawText(drawList, RegularFont(), FONT_CAPTION, pillMin.x + 7.0f, pillMin.y + (20.0f - FONT_CAPTION) * 0.5f, Col(Palette::TEXT_MUTED), countText);
                }
                return pressed;
            };

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 4.0f));
            if(navItem("##NavProjects", "Projects", mPage == Page::PROJECTS, static_cast<int>(mRecentProjects.size())))
                mPage = Page::PROJECTS;
            if(navItem("##NavNewProject", "New Project", mPage == Page::NEW_PROJECT, 0))
                OpenNewProjectPage();
            ImGui::PopStyleVar();

            ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 40.0f);
            ImGui::TextColored(Palette::TEXT_MUTED, "Drop a .surgeproj file or its folder here\nto open it");
        }
        ImGui::EndChild();
    }

    void ProjectBrowser::RenderProjectsPage()
    {
        const float startX = ImGui::GetCursorPosX();
        const float headerY = ImGui::GetCursorPosY();
        const float pageWidth = ImGui::GetContentRegionAvail().x;

        // Header
        ImGui::PushFont(BoldFont(), FONT_TITLE);
        ImGui::TextUnformatted("Projects");
        ImGui::PopFont();
        ImGui::PushStyleColor(ImGuiCol_Text, Palette::TEXT_MUTED);
        ImGui::TextUnformatted("Start something new or pick up where you left off!");
        ImGui::PopStyleColor();
        const float afterHeaderY = ImGui::GetCursorPosY();

        // Action buttons
        const float openWidth = ActionButtonWidth("Open...");
        const float newWidth = ActionButtonWidth("New Project");
        ImGui::SetCursorPos(ImVec2(startX + pageWidth - newWidth - openWidth - 8.0f, headerY + 6.0f));
        if(ActionButton("##OpenProject", "Open...", ButtonStyle::SECONDARY, ImVec2(openWidth, CONTROL_HEIGHT)))
            BrowseForProject();
        ImGuiAux::DelayedToolTip("Open a .surgeproj file (Ctrl+O)");
        ImGui::SameLine(0.0f, 8.0f);
        if(ActionButton("##NewProject", "New Project", ButtonStyle::PRIMARY, ImVec2(newWidth, CONTROL_HEIGHT)))
            OpenNewProjectPage();
        ImGuiAux::DelayedToolTip("Create a new project (Ctrl+N)");

        ImGui::SetCursorPos(ImVec2(startX, afterHeaderY + 14.0f));
        RenderErrorBanner(pageWidth);

        if(!mRecentProjects.empty())
        {
            // Search
            const float sortWidth = ActionButtonWidth("Sort: Last Opened");
            if(mFocusSearch)
            {
                ImGui::SetKeyboardFocusHere();
                mFocusSearch = false;
            }

            const bool searchWasActive = ImGui::GetActiveID() == ImGui::GetID("##Search");
            ImGui::SetNextItemWidth(pageWidth - sortWidth - 8.0f);
            {
                ScopedInputStyle inputStyle;
                if(ImGui::InputTextWithHint("##Search", "Search projects (Ctrl+F)", mSearchBuffer, IM_COUNTOF(mSearchBuffer), ImGuiInputTextFlags_EscapeClearsAll))
                    mVisibleDirty = true;
            }
            FocusRing();

            // Typing filters, arrows pick, Enter opens the pick
            if(searchWasActive)
            {
                if(mVisibleDirty)
                    RebuildVisibleList();
                if(ImGui::IsKeyPressed(ImGuiKey_UpArrow))
                    MoveSelection(-1);
                if(ImGui::IsKeyPressed(ImGuiKey_DownArrow))
                    MoveSelection(1);
                if((ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) && !mSelectedPath.empty())
                {
                    const String selected = mSelectedPath;
                    OpenProject(selected);
                }
            }

            // Sort
            ImGui::SameLine(0.0f, 8.0f);
            const ImVec2 sortMin = ImGui::GetCursorScreenPos();
            if(ActionButton("##Sort", mSortMode == SortMode::NAME ? "Sort: Name" : "Sort: Last Opened", ButtonStyle::SECONDARY, ImVec2(sortWidth, CONTROL_HEIGHT)))
                ImGui::OpenPopup("##SortPopup");

            ImGui::SetNextWindowPos(ImVec2(sortMin.x, sortMin.y + CONTROL_HEIGHT + 4.0f), ImGuiCond_Appearing);
            ImGui::SetNextWindowSize(ImVec2(sortWidth, 0.0f), ImGuiCond_Appearing);
            const SortMode previousSortMode = mSortMode;
            ImGuiAux::StyledPopupVars::Push();
            if(ImGui::BeginPopup("##SortPopup"))
            {
                bool byLastOpened = mSortMode == SortMode::LAST_OPENED;
                bool byName = mSortMode == SortMode::NAME;
                if(ImGuiAux::StyledMenuItem("Last Opened", nullptr, &byLastOpened))
                    mSortMode = SortMode::LAST_OPENED;
                if(ImGuiAux::StyledMenuItem("Name", nullptr, &byName))
                    mSortMode = SortMode::NAME;
                ImGui::EndPopup();
            }
            ImGuiAux::StyledPopupVars::Pop();

            if(mSortMode != previousSortMode)
            {
                mVisibleDirty = true;
                SaveRecentProjects(); // The sort mode is remembered
            }
            ImGui::Dummy(ImVec2(0.0f, 2.0f));
        }

        RenderProjectList();
    }

    void ProjectBrowser::RenderProjectList()
    {
        if(mVisibleDirty) // Typing, sorting or opening a project earlier this frame
            RebuildVisibleList();

        if(mVisible.empty())
        {
            RenderEmptyState(!mRecentProjects.empty());
            return;
        }

        enum class Action
        {
            NONE,
            OPEN,
            LOCATE,
            REMOVE,
            TOGGLE_PIN,
            SHOW_IN_EXPLORER
        };
        Action action = Action::NONE;
        size_t actionIndex = 0;

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, ROW_SPACING));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 10.0f);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        const bool open = ImGui::BeginChild("##ProjectList", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        if(open)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const float rowWidth = std::floor(ImGui::GetContentRegionAvail().x - (ImGui::GetCurrentWindow()->ScrollbarY ? 8.0f : 1.0f));

            for(uint32_t index : mVisible)
            {
                const RecentProject& project = mRecentProjects[index];
                const bool selected = project.Filepath == mSelectedPath;
                ImGui::PushID(project.Filepath.c_str());

                const ImVec2 min(std::floor(ImGui::GetCursorScreenPos().x), std::floor(ImGui::GetCursorScreenPos().y));
                const ImVec2 max(min.x + rowWidth, min.y + ROW_HEIGHT);
                ImGui::SetNextItemAllowOverlap();
                if(ImGui::InvisibleButton("##Row", ImVec2(rowWidth, ROW_HEIGHT)))
                    mSelectedPath = project.Filepath;
                const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlappedByItem);
                if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    action = Action::OPEN;
                    actionIndex = index;
                }
                if(ImGui::IsItemClicked(ImGuiMouseButton_Right))
                {
                    mSelectedPath = project.Filepath;
                    ImGui::OpenPopup("##RowContext");
                }

                if(selected && mScrollToSelected)
                {
                    if(!ImGui::IsItemVisible())
                        ImGui::SetScrollHereY(0.5f);
                    mScrollToSelected = false;
                }

                if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_Stationary))
                {
                    char lastOpened[64] = "never";
                    if(project.LastOpened > 0)
                        FormatDate(project.LastOpened, "%b %d, %Y at %I:%M %p", lastOpened, sizeof(lastOpened));
                    const String tooltip = project.Missing ? std::format("{}\nThe project file could not be found", project.Filepath)
                                                           : std::format("{}\nLast opened {}", project.Filepath, lastOpened);
                    ImGuiAux::DelayedToolTip(tooltip.c_str());
                }

                // Card
                const ImVec4& background = selected ? Palette::CARD_SELECTED : (hovered ? Palette::CARD_HOVERED : Palette::CARD);
                drawList->AddRectFilled(min, max, Col(background), ROUNDING);

                const ImU32 shadowColor = selected ? IM_COL32(0, 0, 0, 255) : (hovered ? IM_COL32(30, 30, 30, 255) : IM_COL32(12, 12, 12, 255));
                FillShadowRect(drawList, min, max, ROUNDING, shadowColor, selected ? 1.0f : CARD_SHADOW_END, selected);

                // Right side: last opened + "..." menu
                constexpr float moreSize = 28.0f;
                const float moreX = max.x - 12.0f - moreSize;

                char timeText[64];
                FormatRelativeTime(project.LastOpened, timeText, sizeof(timeText));
                const float timeWidth = std::ceil(RegularFont()->CalcTextSizeA(FONT_SMALL, FLT_MAX, 0.0f, timeText).x);
                const float timeX = moreX - 12.0f - timeWidth;
                DrawText(drawList, RegularFont(), FONT_SMALL, timeX, min.y + (ROW_HEIGHT - FONT_SMALL) * 0.5f, Col(Palette::TEXT_MUTED), timeText);

                // Name + badges, folder
                const float textX = min.x + 16.0f;
                const float textMaxX = timeX - 20.0f;

                char versionText[40] = {};
                const char* infoBadge = nullptr;
                if(project.Missing)
                    infoBadge = "MISSING";
                else if(!project.Version.empty())
                {
                    std::snprintf(versionText, sizeof(versionText), "v%s", project.Version.c_str());
                    infoBadge = versionText;
                }
                const float badgeSpace = (infoBadge ? BadgeWidth(infoBadge) + 8.0f : 0.0f) + (project.Pinned ? BadgeWidth("PINNED") + 6.0f : 0.0f);

                const float nameY = min.y + 12.0f;
                const float nameMaxX = std::max(textX + 40.0f, textMaxX - badgeSpace);
                ImGui::PushFont(BoldFont(), FONT_NAME);
                ImGui::PushStyleColor(ImGuiCol_Text, project.Missing ? Palette::TEXT_MUTED : Palette::TEXT);
                const ImVec2 nameSize = ImGui::CalcTextSize(project.Name.c_str());
                ImGui::RenderTextEllipsis(drawList, ImVec2(textX, nameY), ImVec2(nameMaxX, nameY + nameSize.y), nameMaxX, project.Name.c_str(), nullptr, &nameSize);
                ImGui::PopStyleColor();
                ImGui::PopFont();

                float badgeX = std::min(textX + nameSize.x, nameMaxX) + 8.0f;
                const float badgeY = nameY + std::floor((FONT_NAME - FONT_BADGE - 6.0f) * 0.5f) + 1.0f;
                if(infoBadge)
                    badgeX += DrawBadge(drawList, ImVec2(badgeX, badgeY), infoBadge, project.Missing ? Palette::DANGER : Palette::TEXT_MUTED) + 6.0f;
                if(project.Pinned)
                    DrawBadge(drawList, ImVec2(badgeX, badgeY), "PINNED", Palette::ACCENT);

                const float pathY = min.y + 37.0f;
                ImGui::PushFont(RegularFont(), FONT_SMALL);
                ImGui::PushStyleColor(ImGuiCol_Text, Palette::TEXT_MUTED);
                ImGui::RenderTextEllipsis(drawList, ImVec2(textX, pathY), ImVec2(textMaxX, pathY + FONT_SMALL + 4.0f), textMaxX, project.Directory.c_str(), nullptr, nullptr);
                ImGui::PopStyleColor();
                ImGui::PopFont();

                const ImVec2 nextRow = ImGui::GetCursorScreenPos();
                const bool menuOpen = ImGui::IsPopupOpen("##RowContext");
                if(selected || menuOpen)
                {
                    const float moreY = min.y + std::floor((ROW_HEIGHT - moreSize) * 0.5f);
                    ImGui::SetCursorScreenPos(ImVec2(moreX, moreY));
                    if(MoreButton("##More", moreSize, menuOpen) && !menuOpen)
                    {
                        mSelectedPath = project.Filepath;
                        ImGui::OpenPopup("##RowContext");
                        ImGui::SetNextWindowPos(ImVec2(moreX + moreSize, moreY + moreSize + 4.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f)); // Right aligned under the button
                    }
                }
                ImGui::SetCursorScreenPos(nextRow);

                // Right click or the "..." button. The row is selected while its menu is open, so the button and highlight stay put
                ImGuiAux::StyledPopupVars::Push();
                if(ImGui::BeginPopup("##RowContext"))
                {
                    if(!project.Missing && ImGuiAux::StyledMenuItem("Open", "Enter"))
                        action = Action::OPEN;
                    if(project.Missing && ImGuiAux::StyledMenuItem("Locate..."))
                        action = Action::LOCATE;
                    if(!project.Missing && ImGuiAux::StyledMenuItem("Show in Explorer"))
                        action = Action::SHOW_IN_EXPLORER;
                    if(ImGuiAux::StyledMenuItem("Copy Path"))
                        ImGui::SetClipboardText(project.Filepath.c_str());
                    if(ImGuiAux::StyledMenuItem(project.Pinned ? "Unpin" : "Pin to Top"))
                        action = Action::TOGGLE_PIN;
                    ImGuiAux::StyledSeparator();
                    if(ImGuiAux::StyledMenuItem("Remove from List", "Del"))
                        action = Action::REMOVE;
                    ImGuiAux::EndStyledPopup();
                    if(action != Action::NONE)
                        actionIndex = index;
                }
                else
                    ImGuiAux::StyledPopupVars::Pop();

                ImGui::PopID();
            }
            ImGui::Dummy(ImVec2(0.0f, 0.0f)); // Validates the cursor moved back below the last row
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();

        switch(action)
        {
            case Action::OPEN:
            {
                const String filepath = mRecentProjects[actionIndex].Filepath; // OpenProject reorders the list
                OpenProject(filepath);
                break;
            }
            case Action::LOCATE:
                LocateProject(actionIndex);
                break;
            case Action::REMOVE:
                RemoveRecentProject(actionIndex);
                break;
            case Action::TOGGLE_PIN:
                mRecentProjects[actionIndex].Pinned = !mRecentProjects[actionIndex].Pinned;
                mVisibleDirty = true;
                SaveRecentProjects();
                break;
            case Action::SHOW_IN_EXPLORER:
                Platform::OpenInExplorer(mRecentProjects[actionIndex].Filepath);
                break;
            case Action::NONE:
                break;
        }
    }

    void ProjectBrowser::RenderEmptyState(bool noMatches)
    {
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const ImVec2 origin(std::floor(ImGui::GetCursorScreenPos().x), std::floor(ImGui::GetCursorScreenPos().y));
        const ImVec2 max(origin.x + std::floor(avail.x), origin.y + std::floor(avail.y) - 4.0f);
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        const float centerX = std::floor(origin.x + avail.x * 0.5f);
        float y = std::floor(origin.y + avail.y * 0.5f - 50.0f);
        DrawTextCentered(drawList, BoldFont(), FONT_HEADING, centerX, y, Col(Palette::TEXT), noMatches ? "No matching projects" : "No projects yet");
        y += 32.0f;

        char message[200];
        if(noMatches)
            std::snprintf(message, sizeof(message), "Nothing matches \"%s\".", mSearchBuffer);
        else
            std::snprintf(message, sizeof(message), "Create a new project, or open an existing .surgeproj file.");
        DrawTextCentered(drawList, RegularFont(), ImGui::GetFontSize(), centerX, y, Col(Palette::TEXT_MUTED), message);
        y += 38.0f;

        if(noMatches)
        {
            const float clearWidth = ActionButtonWidth("Clear Search");
            ImGui::SetCursorScreenPos(ImVec2(std::floor(centerX - clearWidth * 0.5f), y));
            if(ActionButton("##ClearSearch", "Clear Search", ButtonStyle::SECONDARY, ImVec2(clearWidth, CONTROL_HEIGHT)))
            {
                mSearchBuffer[0] = '\0';
                mVisibleDirty = true;
            }
            return;
        }

        const float newWidth = ActionButtonWidth("New Project");
        const float openWidth = ActionButtonWidth("Open...");
        ImGui::SetCursorScreenPos(ImVec2(std::floor(centerX - (newWidth + openWidth + 8.0f) * 0.5f), y));
        if(ActionButton("##EmptyNewProject", "New Project", ButtonStyle::PRIMARY, ImVec2(newWidth, CONTROL_HEIGHT)))
            OpenNewProjectPage();
        ImGui::SameLine(0.0f, 8.0f);
        if(ActionButton("##EmptyOpenProject", "Open...", ButtonStyle::SECONDARY, ImVec2(openWidth, CONTROL_HEIGHT)))
            BrowseForProject();
    }

    void ProjectBrowser::RenderNewProjectPage()
    {
        if(mValidationDirty)
            ValidateNewProject();

        const float startX = ImGui::GetCursorPosX();
        const float formWidth = std::floor(std::min(ImGui::GetContentRegionAvail().x, FORM_MAX_WIDTH));

        ImGui::PushFont(BoldFont(), FONT_TITLE);
        ImGui::TextUnformatted("New Project");
        ImGui::PopFont();
        ImGui::PushStyleColor(ImGuiCol_Text, Palette::TEXT_MUTED);
        ImGui::TextUnformatted("Creates a project folder with an Assets directory and a very interesting starter scene!");
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.0f, 14.0f));
        RenderErrorBanner(formWidth);

        // Name
        FieldLabel("PROJECT NAME");
        if(mFocusName)
        {
            ImGui::SetKeyboardFocusHere();
            mFocusName = false;
        }
        ImGui::SetNextItemWidth(formWidth);
        {
            ScopedInputStyle inputStyle;
            if(ImGui::InputTextWithHint("##ProjectName", "My Game", mNameBuffer, IM_COUNTOF(mNameBuffer)))
                mValidationDirty = true;
        }
        FocusRing();
        if(mNameError)
            ItemOutline(Palette::DANGER);

        ImGui::PushFont(RegularFont(), FONT_CAPTION);
        ImGui::PushStyleColor(ImGuiCol_Text, mNameError ? Palette::DANGER : ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::TextUnformatted(mNameError ? mValidationMessage.c_str() : " "); // Keeps its line so the form doesn't jump
        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0.0f, 2.0f));

        // Location
        FieldLabel("LOCATION");
        const float browseWidth = ActionButtonWidth("Browse...");
        ImGui::SetNextItemWidth(formWidth - browseWidth - 8.0f);
        {
            ScopedInputStyle inputStyle;
            if(ImGui::InputTextWithHint("##Location", "Folder the project is created in", mLocationBuffer, IM_COUNTOF(mLocationBuffer)))
                mValidationDirty = true;
        }
        FocusRing();
        ImGui::SameLine(0.0f, 8.0f);
        if(ActionButton("##BrowseLocation", "Browse...", ButtonStyle::SECONDARY, ImVec2(browseWidth, CONTROL_HEIGHT)))
        {
            const String folder = FileDialog::ChooseFolder();
            if(!folder.empty())
            {
                std::snprintf(mLocationBuffer, sizeof(mLocationBuffer), "%s", Path(folder).generic_string().c_str());
                mValidationDirty = true;
            }
        }
        ImGui::Dummy(ImVec2(0.0f, 14.0f));

        // Preview of what gets written
        FieldLabel("WILL CREATE");
        {
            const String name = Trim(mNameBuffer);
            const bool hasName = !name.empty() && !mNameError;
            const String folderText = !mNewProjectFile.empty() ? mNewProjectFile.parent_path().generic_string() + "/" : (hasName ? name + "/" : String("<Project Name>/"));
            const String projectFileText = (hasName ? name : String("<Project Name>")) + PROJECT_EXTENSION;
            const String sceneText = std::format("Assets/Scenes/Main{}", GetExtensionFromAssetType(AssetType::SCENE));

            constexpr float lineHeight = 24.0f;
            constexpr float padding = 14.0f;
            const ImVec2 cardMin(std::floor(ImGui::GetCursorScreenPos().x), std::floor(ImGui::GetCursorScreenPos().y));
            const ImVec2 cardMax(cardMin.x + formWidth, cardMin.y + lineHeight * 3.0f + padding * 2.0f);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(cardMin, cardMax, Col(Palette::CARD), ROUNDING);

            const auto treeLine = [&](int line, int depth, const char* text, const ImVec4& color) {
                const float x = cardMin.x + padding + static_cast<float>(depth) * 20.0f;
                const float y = std::floor(cardMin.y + padding + static_cast<float>(line) * lineHeight + (lineHeight - ImGui::GetFontSize()) * 0.5f);
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::RenderTextEllipsis(drawList, ImVec2(x, y), ImVec2(cardMax.x - padding, y + lineHeight), cardMax.x - padding, text, nullptr, nullptr);
                ImGui::PopStyleColor();
            };
            treeLine(0, 0, folderText.c_str(), mNewProjectFile.empty() ? Palette::TEXT_MUTED : Palette::ACCENT);
            treeLine(1, 1, projectFileText.c_str(), Palette::TEXT);
            treeLine(2, 1, sceneText.c_str(), Palette::TEXT_MUTED);
            ImGui::Dummy(ImVec2(formWidth, cardMax.y - cardMin.y));
        }
        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        // Status + actions
        const bool canCreate = mValidationMessage.empty();
        const float rowY = ImGui::GetCursorPosY();
        const ImVec2 rowMin = ImGui::GetCursorScreenPos();
        const float statusY = rowMin.y + (CONTROL_HEIGHT - ImGui::GetFontSize()) * 0.5f;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if(canCreate)
            DrawText(drawList, RegularFont(), ImGui::GetFontSize(), rowMin.x, statusY, Col(Palette::SUCCESS), "Ready to create");
        else if(!mNameError)
            DrawText(drawList, RegularFont(), ImGui::GetFontSize(), rowMin.x, statusY, Col(Palette::TEXT_MUTED), mValidationMessage.c_str());

        const float createWidth = ActionButtonWidth("Create Project");
        const float cancelWidth = ActionButtonWidth("Cancel");
        ImGui::SetCursorPos(ImVec2(startX + formWidth - createWidth - cancelWidth - 8.0f, rowY));
        if(ActionButton("##CancelNewProject", "Cancel", ButtonStyle::GHOST, ImVec2(cancelWidth, CONTROL_HEIGHT)))
            mPage = Page::PROJECTS;
        ImGui::SameLine(0.0f, 8.0f);
        if(ActionButton("##CreateProject", "Create Project", ButtonStyle::PRIMARY, ImVec2(createWidth, CONTROL_HEIGHT), canCreate))
            CreateProject();
        ImGuiAux::ItemTooltip(canCreate ? "Create the project and open it (Enter)" : mValidationMessage.c_str());
    }

    void ProjectBrowser::RenderErrorBanner(float width)
    {
        if(mError.empty())
            return;

        constexpr float padding = 12.0f;
        const float fontSize = ImGui::GetFontSize();
        const float dismissWidth = ActionButtonWidth("Dismiss");
        const float textWidth = width - padding * 2.0f - 8.0f - dismissWidth;
        const ImVec2 textSize = RegularFont()->CalcTextSizeA(fontSize, FLT_MAX, textWidth, mError.c_str());

        const ImVec2 min(std::floor(ImGui::GetCursorScreenPos().x), std::floor(ImGui::GetCursorScreenPos().y));
        const float height = std::ceil(std::max(CONTROL_HEIGHT + 12.0f, textSize.y + padding * 2.0f));
        const ImVec2 max(min.x + width, min.y + height);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRect(min, max, Col(Palette::DANGER, 0.12f), 6.0f);
        FillRect(drawList, min.x, min.y + 8.0f, 3.0f, height - 16.0f, Col(Palette::DANGER));
        drawList->AddText(RegularFont(), fontSize, ImVec2(min.x + padding + 4.0f, std::floor(min.y + (height - textSize.y) * 0.5f)), Col(Palette::TEXT), mError.c_str(), nullptr, textWidth);

        ImGui::SetCursorScreenPos(ImVec2(max.x - dismissWidth - 6.0f, std::floor(min.y + (height - CONTROL_HEIGHT) * 0.5f)));
        if(ActionButton("##DismissError", "Dismiss", ButtonStyle::GHOST, ImVec2(dismissWidth, CONTROL_HEIGHT)))
            mError.clear();

        ImGui::SetCursorScreenPos(ImVec2(min.x, max.y + 6.0f));
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
    }

    void ProjectBrowser::HandleShortcuts()
    {
        if(ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId))
            return;

        const ImGuiIO& io = ImGui::GetIO();
        if(io.KeyCtrl)
        {
            if(ImGui::IsKeyPressed(ImGuiKey_N, false))
                OpenNewProjectPage();
            else if(ImGui::IsKeyPressed(ImGuiKey_O, false))
                BrowseForProject();
            else if(ImGui::IsKeyPressed(ImGuiKey_F, false))
            {
                mPage = Page::PROJECTS;
                mFocusSearch = true;
            }
            return;
        }

        const bool enterPressed = ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
        if(mPage == Page::NEW_PROJECT)
        {
            // Text fields eat Escape themselves (revert + deactivate), Enter also finishes typing in them
            if(!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
                mPage = Page::PROJECTS;
            else if(enterPressed)
            {
                ValidateNewProject();
                if(mValidationMessage.empty())
                    CreateProject();
            }
            return;
        }

        if(io.WantTextInput) // The search box handles its own keys
            return;

        if(ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            MoveSelection(-1);
        if(ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            MoveSelection(1);
        if(enterPressed && !mSelectedPath.empty())
        {
            const String selected = mSelectedPath;
            OpenProject(selected);
        }
        else if(ImGui::IsKeyPressed(ImGuiKey_Delete, false))
        {
            const int index = FindRecentProject(mSelectedPath);
            if(index != -1)
                RemoveRecentProject(static_cast<size_t>(index));
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // State
    //////////////////////////////////////////////////////////////////////////

    void ProjectBrowser::OpenNewProjectPage()
    {
        mPage = Page::NEW_PROJECT;
        mFocusName = true;
        mValidationDirty = true;

        // Projects tend to live side by side: default to where the last one was created, or next to the most recent one
        if(mLocationBuffer[0] == '\0')
        {
            String location = mLastLocation;
            if(location.empty() && !mRecentProjects.empty())
                location = Path(mRecentProjects.front().Directory).parent_path().generic_string();
            std::snprintf(mLocationBuffer, sizeof(mLocationBuffer), "%s", location.c_str());
        }
    }

    void ProjectBrowser::RebuildVisibleList()
    {
        mVisibleDirty = false;
        mVisible.clear();
        for(uint32_t i = 0; i < static_cast<uint32_t>(mRecentProjects.size()); i++)
        {
            const RecentProject& project = mRecentProjects[i];
            if(ContainsCaseInsensitive(project.Name, mSearchBuffer) || ContainsCaseInsensitive(project.Filepath, mSearchBuffer))
                mVisible.push_back(i);
        }

        std::stable_sort(mVisible.begin(), mVisible.end(), [this](uint32_t a, uint32_t b) {
            const RecentProject& first = mRecentProjects[a];
            const RecentProject& second = mRecentProjects[b];
            if(first.Pinned != second.Pinned)
                return first.Pinned;
            if(mSortMode == SortMode::NAME)
            {
                if(LessCaseInsensitive(first.Name, second.Name))
                    return true;
                if(LessCaseInsensitive(second.Name, first.Name))
                    return false;
            }
            return first.LastOpened > second.LastOpened;
        });

        // While searching, the best match stays selected so Enter opens it
        if(mSearchBuffer[0] != '\0' && !mVisible.empty())
        {
            const bool selectionVisible = std::any_of(mVisible.begin(), mVisible.end(), [this](uint32_t index) { return mRecentProjects[index].Filepath == mSelectedPath; });
            if(!selectionVisible)
            {
                mSelectedPath = mRecentProjects[mVisible.front()].Filepath;
                mScrollToSelected = true;
            }
        }
    }

    void ProjectBrowser::MoveSelection(int delta)
    {
        if(mVisible.empty())
            return;

        const int last = static_cast<int>(mVisible.size()) - 1;
        int current = -1;
        for(int i = 0; i <= last; i++)
        {
            if(mRecentProjects[mVisible[i]].Filepath == mSelectedPath)
                current = i;
        }

        const int next = current == -1 ? (delta > 0 ? 0 : last) : std::clamp(current + delta, 0, last);
        mSelectedPath = mRecentProjects[mVisible[next]].Filepath;
        mScrollToSelected = true;
    }

    void ProjectBrowser::ValidateNewProject()
    {
        mValidationDirty = false;
        mNewProjectFile.clear();
        mNameError = false;

        const String name = Trim(mNameBuffer);
        const String location = Trim(mLocationBuffer);
        if(name.empty())
        {
            mValidationMessage = "Give your project a name";
            return;
        }
        if(const char* nameError = ValidateProjectName(name))
        {
            mValidationMessage = nameError;
            mNameError = true;
            return;
        }
        if(location.empty())
        {
            mValidationMessage = "Choose where the project is created";
            return;
        }

        const Path locationPath(location);
        if(!locationPath.is_absolute())
        {
            mValidationMessage = "The location must be an absolute path";
            return;
        }

        std::error_code error;
        const Path projectDirectory = locationPath / name;
        if(std::filesystem::exists(projectDirectory, error) && !(std::filesystem::is_directory(projectDirectory, error) && std::filesystem::is_empty(projectDirectory, error)))
        {
            mValidationMessage = std::format("'{}' already exists in this location and is not empty", name);
            return;
        }

        mNewProjectFile = projectDirectory / (name + PROJECT_EXTENSION);
        mValidationMessage.clear();
    }

    //////////////////////////////////////////////////////////////////////////
    // Project actions
    //////////////////////////////////////////////////////////////////////////

    void ProjectBrowser::CreateProject()
    {
        ValidateNewProject();
        if(!mValidationMessage.empty())
            return;

        const Path projectFile = mNewProjectFile;
        const Path projectDirectory = projectFile.parent_path();

        std::error_code error;
        std::filesystem::create_directories(projectDirectory / "Assets" / "Scenes", error);
        if(error)
        {
            SetError(std::format("Could not create '{}': {}", projectDirectory.generic_string(), error.message()));
            return;
        }

        Project project;
        project.Name = Trim(mNameBuffer);
        project.Version = "1.0";

        ActivateAssetManager(projectFile);
        Ref<Scene> startScene = mAssetManager->Create<Scene>(std::format("Scenes/Main{}", GetExtensionFromAssetType(AssetType::SCENE)), "Main", Core::GetSceneContext());
        project.StartScene = startScene->GetID();
        project.Save(projectFile);

        auto* editor = static_cast<Editor*>(Core::GetClient());
        editor->SetCurrentProject(project, projectFile);
        editor->LoadScene(std::move(startScene));

        mLastLocation = projectDirectory.parent_path().generic_string();
        AddRecentProject(project, projectFile.generic_string());

        mNameBuffer[0] = '\0';
        mValidationDirty = true;
        mPage = Page::PROJECTS;
        mError.clear();
        Core::GetWindow()->Maximize();
    }

    void ProjectBrowser::OpenProject(const String& filepath)
    {
        const String genericPath = Path(filepath).generic_string();
        std::error_code error;
        if(!std::filesystem::is_regular_file(genericPath, error))
        {
            const int index = FindRecentProject(genericPath);
            if(index != -1)
            {
                mRecentProjects[index].Missing = true;
                mVisibleDirty = true;
            }
            SetError(std::format("'{}' could not be found. It may have been moved or deleted: locate it, or remove it from the list.", genericPath));
            return;
        }

        Project project;
        if(!ReadProjectFile(genericPath, project))
        {
            SetError(std::format("'{}' is not a valid Surge project file.", genericPath));
            return;
        }

        ActivateAssetManager(genericPath);
        auto* editor = static_cast<Editor*>(Core::GetClient());
        editor->SetCurrentProject(project, genericPath);
        editor->LoadStartupScene();

        AddRecentProject(project, genericPath);
        mError.clear();
        Core::GetWindow()->Maximize();
    }

    void ProjectBrowser::BrowseForProject()
    {
        const String filepath = FileDialog::OpenFile(PROJECT_FILTER);
        if(!filepath.empty())
            OpenProject(filepath);
    }

    void ProjectBrowser::LocateProject(size_t index)
    {
        const String selected = FileDialog::OpenFile(PROJECT_FILTER);
        if(selected.empty())
            return;

        Project project;
        if(!ReadProjectFile(selected, project))
        {
            SetError(std::format("'{}' is not a valid Surge project file.", Path(selected).generic_string()));
            return;
        }

        // Relinking onto a project that is already listed merges the two entries
        const String filepath = Path(selected).generic_string();
        const int existing = FindRecentProject(filepath);
        if(existing != -1 && existing != static_cast<int>(index))
        {
            mRecentProjects[existing].Pinned |= mRecentProjects[index].Pinned;
            mRecentProjects.erase(mRecentProjects.begin() + static_cast<std::ptrdiff_t>(index));
        }
        else
        {
            RecentProject& recent = mRecentProjects[index];
            recent.Name = project.Name;
            recent.Version = project.Version;
            recent.Filepath = filepath;
            recent.Directory = GetDirectoryString(filepath);
            recent.Missing = false;
        }

        mSelectedPath = filepath;
        mVisibleDirty = true;
        mError.clear();
        SaveRecentProjects();
    }

    void ProjectBrowser::RemoveRecentProject(size_t index)
    {
        // The neighbor in the visible list takes over the selection
        if(mRecentProjects[index].Filepath == mSelectedPath)
        {
            mSelectedPath.clear();
            const auto it = std::find(mVisible.begin(), mVisible.end(), static_cast<uint32_t>(index));
            if(it != mVisible.end())
            {
                if(it + 1 != mVisible.end())
                    mSelectedPath = mRecentProjects[*(it + 1)].Filepath;
                else if(it != mVisible.begin())
                    mSelectedPath = mRecentProjects[*(it - 1)].Filepath;
            }
        }

        mRecentProjects.erase(mRecentProjects.begin() + static_cast<std::ptrdiff_t>(index));
        mVisibleDirty = true;
        SaveRecentProjects();
    }

    void ProjectBrowser::ActivateAssetManager(const Path& projectFile)
    {
        const Path assetsDirectory = projectFile.parent_path() / "Assets";
        std::error_code error;
        std::filesystem::create_directories(assetsDirectory, error);

        const AssetContext context = mAssetManager->GetContext(); // Set up by Core, stays the same across projects
        mAssetManager->Shutdown();
        mAssetManager->Initialize(assetsDirectory, context);
        static_cast<Editor*>(Core::GetClient())->GetPanelManager().GetPanel<ContentBrowserPanel>()->OnAssetManagerInit();
    }

    void ProjectBrowser::AddRecentProject(const Project& project, const String& filepath)
    {
        RecentProject recent;
        const int existing = FindRecentProject(filepath);
        if(existing != -1)
        {
            recent = std::move(mRecentProjects[existing]);
            mRecentProjects.erase(mRecentProjects.begin() + existing);
        }

        recent.Name = project.Name;
        recent.Version = project.Version;
        recent.Filepath = filepath;
        recent.Directory = GetDirectoryString(filepath);
        recent.LastOpened = NowSeconds();
        recent.Missing = false;
        mRecentProjects.insert(mRecentProjects.begin(), std::move(recent));

        // Evict the oldest unpinned projects
        size_t unpinned = static_cast<size_t>(std::count_if(mRecentProjects.begin(), mRecentProjects.end(), [](const RecentProject& p) { return !p.Pinned; }));
        for(size_t i = mRecentProjects.size(); i-- > 0 && unpinned > MAX_RECENT_PROJECTS;)
        {
            if(!mRecentProjects[i].Pinned)
            {
                mRecentProjects.erase(mRecentProjects.begin() + static_cast<std::ptrdiff_t>(i));
                unpinned--;
            }
        }

        mSelectedPath = filepath;
        mVisibleDirty = true;
        SaveRecentProjects();
    }

    int ProjectBrowser::FindRecentProject(const String& filepath) const
    {
        for(size_t i = 0; i < mRecentProjects.size(); i++)
        {
            if(mRecentProjects[i].Filepath == filepath)
                return static_cast<int>(i);
        }
        return -1;
    }

    void ProjectBrowser::SetError(String message)
    {
        mError = std::move(message);
        Log<Severity::Warn>("[ProjectBrowser] {}", mError);
    }

    //////////////////////////////////////////////////////////////////////////
    // Persistence
    //////////////////////////////////////////////////////////////////////////

    void ProjectBrowser::SaveRecentProjects()
    {
        nlohmann::json root;
        root["INFO"] = "Generated by the Surge Editor";
        root["LastLocation"] = mLastLocation;
        root["SortMode"] = mSortMode == SortMode::NAME ? "Name" : "LastOpened";

        nlohmann::json& list = root["RecentProjects"] = nlohmann::json::array();
        for(const RecentProject& project : mRecentProjects)
            list.push_back({ { "Name", project.Name }, { "Filepath", project.Filepath }, { "LastOpenedTime", project.LastOpened }, { "Pinned", project.Pinned } });

        const Path storePath = GetRecentProjectsPath();
        if(!Filesystem::WriteTextFile(storePath, root.dump(4)))
            Log<Severity::Warn>("[ProjectBrowser] Failed to save the recent projects to '{}'", storePath.string());
    }

    void ProjectBrowser::LoadRecentProjects()
    {
        mRecentProjects.clear();

        const Path storePath = GetRecentProjectsPath();
        String contents;
        if(!Filesystem::Exists(storePath) || !Filesystem::ReadTextFile(storePath, contents))
            return;

        const nlohmann::json root = nlohmann::json::parse(contents, nullptr, false);
        if(root.is_discarded() || !root.is_object())
        {
            Log<Severity::Warn>("[ProjectBrowser] '{}' is corrupt, starting with an empty recent projects list", storePath.string());
            return;
        }

        // json::value() throws when a key holds another type, a hand edited file must not take the editor down
        const auto readString = [](const nlohmann::json& object, const char* key) -> String {
            const auto it = object.find(key);
            return it != object.end() && it->is_string() ? it->get<String>() : String();
        };

        mLastLocation = readString(root, "LastLocation");
        mSortMode = readString(root, "SortMode") == "Name" ? SortMode::NAME : SortMode::LAST_OPENED;

        const auto list = root.find("RecentProjects");
        if(list == root.end() || !list->is_array())
            return;

        for(const nlohmann::json& item : *list)
        {
            if(!item.is_object())
                continue;

            RecentProject project;
            project.Filepath = Path(readString(item, "Filepath")).generic_string();
            if(project.Filepath.empty() || FindRecentProject(project.Filepath) != -1)
                continue;

            project.Name = readString(item, "Name");
            project.Directory = GetDirectoryString(project.Filepath);
            const auto pinned = item.find("Pinned");
            project.Pinned = pinned != item.end() && pinned->is_boolean() && pinned->get<bool>();
            const auto lastOpened = item.find("LastOpenedTime"); // Older lists only stored a display string
            if(lastOpened != item.end() && lastOpened->is_number_integer())
                project.LastOpened = lastOpened->get<int64_t>();
            mRecentProjects.push_back(std::move(project));
        }
    }

} // namespace Surge
