// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_internal.h>

#define HIERARCHY_ENTITY_DND "HiEnT!"

// Function name prefixed with T means it is used inside ImGui::BeginTable
namespace Surge::ImGuiAux
{
    namespace Colors
    {
        constexpr glm::vec4 ThemeColor = glm::vec4(1.0f, 0.5f, 0.1f, 1.0f);
        constexpr glm::vec4 ThemeColorLight = glm::vec4(1.0f, 0.6f, 0.1f, 1.0f);
        constexpr glm::vec4 ExtraDark = glm::vec4(0.05f, 0.05f, 0.05f, 1.0f);
        constexpr glm::vec4 Red = glm::vec4(0.8f, 0.1f, 0.1f, 1.0f);
        constexpr glm::vec4 Green = glm::vec4(0.1f, 0.8f, 0.1f, 1.0f);
        constexpr glm::vec4 Blue = glm::vec4(0.1f, 0.1f, 0.8f, 1.0f);
        constexpr glm::vec4 White = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

        constexpr glm::vec4 Iron = glm::vec4(0.560f, 0.570f, 0.580f, 1.0f);
        constexpr glm::vec4 Silver = glm::vec4(0.972f, 0.960f, 0.915f, 1.0f);
        constexpr glm::vec4 Aluminum = glm::vec4(0.913f, 0.921f, 0.925f, 1.0f);
        constexpr glm::vec4 Gold = glm::vec4(1.0f, 0.766f, 0.336f, 1.0f);
        constexpr glm::vec4 Copper = glm::vec4(0.955f, 0.637f, 0.538f, 1.0f);
        constexpr glm::vec4 Chromium = glm::vec4(0.550f, 0.556f, 0.554f, 1.0f);
        constexpr glm::vec4 Nickel = glm::vec4(0.660f, 0.609f, 0.526f, 1.0f);
        constexpr glm::vec4 Titanium = glm::vec4(0.542f, 0.497f, 0.449f, 1.0f);
        constexpr glm::vec4 Cobalt = glm::vec4(0.662f, 0.655f, 0.634f, 1.0f);
        constexpr glm::vec4 Platinum = glm::vec4(0.672f, 0.637f, 0.585f, 1.0f);

    } // namespace Colors

    enum class CustomProprtyFlag
    {
        None,
        Color3,
        Color4,
    };

    struct ScopedBoldFont
    {
        ScopedBoldFont(const ScopedBoldFont&) = delete;
        ScopedBoldFont operator=(const ScopedBoldFont&) = delete;

        ScopedBoldFont()
        {
            // Bold font at index 1 (See Surge/Graphics/RHI/Vulkan/VulkanImGui.cpp)
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
        }
        ~ScopedBoldFont()
        {
            ImGui::PopFont();
        }
    };

    struct ScopedStyle
    {
        int Size;
        ScopedStyle(const ScopedStyle&) = delete;
        ScopedStyle operator=(const ScopedStyle&) = delete;

        ScopedStyle(const std::initializer_list<ImGuiStyleVar>& ids, float style)
            : Size(static_cast<int>(ids.size()))
        {
            for (auto& id : ids)
                ImGui::PushStyleVar(id, style);
        }

        ScopedStyle(const std::initializer_list<ImGuiStyleVar>& ids, const ImVec2& style)
            : Size(static_cast<int>(ids.size()))
        {

            for (auto& id : ids)
                ImGui::PushStyleVar(id, style);
        }

        ~ScopedStyle()
        {
            ImGui::PopStyleVar(Size);
        }
    };

    struct ScopedColor
    {
        int Size;
        ScopedColor(const ScopedColor&) = delete;
        ScopedColor operator=(const ScopedColor&) = delete;
        ScopedColor(const std::initializer_list<ImGuiCol>& ids, const ImVec4& col)
            : Size(static_cast<int>(ids.size()))
        {

            for (auto& id : ids)
                ImGui::PushStyleColor(id, col);
        }

        ~ScopedColor()
        {
            ImGui::PopStyleColor(Size);
        }
    };

    void DrawRectAroundWidget(const glm::vec4& color, float thickness = 1.5f, float rounding = 1.0f);
    void DockSpace();
    bool Spinner(const char* label, float radius, float thickness);
    bool PropertyGridHeader(const String& name, bool openByDefault = true, const glm::vec2& size = {4.5f, 4.5f}, bool spacing = false);

    void TextCentered(const char* text);
    bool ButtonCentered(const char* title);

    void Image(TextureHandle image, const glm::vec2& size);

    template <typename T>
    void TComboBox(const char* title, const char** stringArray, Uint stringArraySize, Uint currentStringIndexInArray, T callbackFunction)
    {
        ImGui::PushID(title);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(title);
        ImGui::TableNextColumn();
        const char* currentString = stringArray[currentStringIndexInArray];
        if (ImGui::BeginCombo("##cbox", currentString))
        {
            for (Uint i = 0; i < stringArraySize; i++)
            {
                const bool isSelected = currentString == stringArray[i];
                if (ImGui::Selectable(stringArray[i], isSelected))
                {
                    currentString = stringArray[i];
                    callbackFunction(i);
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopID();
    }

    template <typename T, CustomProprtyFlag F = CustomProprtyFlag::None>
    constexpr bool TProperty(const char* title, T* value, float dragMin = 0.0f, float dragMax = 0.0f)
    {
        ImGui::PushID(title);
        bool result = false;
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(title);
        ImGui::TableNextColumn();
        ImGui::PushItemWidth(-1);
        if constexpr (F == CustomProprtyFlag::None)
        {
            if constexpr (std::is_same_v<T, int>)
                result = ImGui::DragInt("##v", value, 1, static_cast<int>(dragMin), static_cast<int>(dragMax));
            else if constexpr (std::is_same_v<T, float>)
                result = ImGui::DragFloat("##v", value, 0.01, dragMin, dragMax);
            else if constexpr (std::is_same_v<T, glm::vec2>)
                result = ImGui::DragFloat2("##v", glm::value_ptr(*value), 0.01f, dragMin, dragMax, "%.2f");
            else if constexpr (std::is_same_v<T, glm::vec3>)
                result = ImGui::DragFloat3("##v", glm::value_ptr(*value), 0.01f, dragMin, dragMax, "%.2f");
            else if constexpr (std::is_same_v<T, glm::vec4>)
                result = ImGui::DragFloat4("##v", glm::value_ptr(*value), 0.01f, dragMin, dragMax, "%.2f");
            else if constexpr (std::is_same_v<T, bool>)
                result = ImGui::Checkbox("##v", value);
            else if constexpr (std::is_same_v<T, String>)
                ImGui::TextUnformatted(value->c_str());
            else
                static_assert(false);
        }
        else if constexpr (F == CustomProprtyFlag::Color3 && std::is_same_v<T, glm::vec3>)
            result = ImGui::ColorEdit3("##v", glm::value_ptr(*value));
        else if constexpr (F == CustomProprtyFlag::Color4 && std::is_same_v<T, glm::vec4>)
            result = ImGui::ColorEdit4("##v", glm::value_ptr(*value), ImGuiColorEditFlags_None | ImGuiColorEditFlags_AlphaBar);
        else
            static_assert(false, "Invalid case! Maybe you used wrong CustomProprtyFlag with wrong type? For example: Using glm::vec3 with CustomProprtyFlag::Color4");
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            DrawRectAroundWidget(Colors::ThemeColor, 1.5f, 1.0f);
        ImGui::PopID();

        return result;
    }

    inline bool TSelectable(const char* title)
    {
        ImGui::TableNextColumn();
        bool isSlected = ImGui::Selectable(title);
        if (ImGui::IsItemFocused())
            DrawRectAroundWidget(Colors::ThemeColor, 1.5f, 1.0f);
        return isSlected;
    }

    inline bool TButton(const char* title, const char* buttonText)
    {
        ImGui::PushID(title);
        bool result = false;
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(title);
        ImGui::TableNextColumn();

        auto& style = ImGui::GetStyle();
        ImVec4 buttonCol = style.Colors[ImGuiCol_Button];

        ImGuiAux::ScopedColor color({ImGuiCol_ButtonHovered}, buttonCol);
        result = ImGui::Button(buttonText);

        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            DrawRectAroundWidget(Colors::ThemeColor, 1.5f, 1.0f);
        ImGui::PopID();

        return result;
    }

    inline bool Button(const char* buttonText)
    {
        bool result = false;
        result = ImGui::Button(buttonText);

        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            DrawRectAroundWidget(Colors::ThemeColor, 1.5f, 1.0f);

        return result;
    }

    template <typename T>
    inline bool TSlider(const char* label, T& value, T min, T max)
    {
        bool modified = false;
        ImGui::PushID(label);

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(label);
        ImGui::TableNextColumn();

        ImGui::PushItemWidth(-1);

        if constexpr (std::is_same_v<T, float>)
        {
            if (ImGui::SliderFloat("##label", &value, min, max))
                modified = true;
        }
        else if constexpr (std::is_same_v<T, Uint>)
        {
            if (ImGui::SliderInt("##label", reinterpret_cast<int*>(&value), min, max))
                modified = true;
        }
        else
            static_assert(false);

        ImGui::PopItemWidth();

        ImGui::PopID();
        return modified;
    }

    inline void ToolTip(const char* tip)
    {
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(tip);
            ImGui::EndTooltip();
        }
    }

    inline void DelayedToolTip(const char* tip)
    {
        if (ImGui::IsItemHovered() && GImGui->HoveredIdTimer > 0.5f)
        {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(tip);
            ImGui::EndTooltip();
        }
    }

    inline void ShiftCursorX(float x)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + x);
    }
    inline void ShiftCursorY(float y)
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + y);
    }

    class RenamingMechanism
    {
    public:
        RenamingMechanism() = default;
        RenamingMechanism(const RenamingMechanism&) = delete;
        RenamingMechanism operator=(const RenamingMechanism&) = delete;
        ~RenamingMechanism() = default;

        FORCEINLINE void SetRenamingState(bool isRenaming) { mRenaming = isRenaming; }        
        void Update(String& name, const std::function<void(const String& newName)>& onRenameEnd = [](const String& newName) {}); 

        bool operator!() const { return !mRenaming; }

    private:
        bool mRenaming = false;
        String mOldName;
        String mTempBuffer;
    };


} // namespace Surge::ImGuiAux