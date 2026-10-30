// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Viewport/GameViewport.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/ECS/Scene.hpp"
#include "Surge/ECS/Components/Components.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Editor.hpp"
#include "Panels/UIEditorPanel.hpp"
#include "Settings/EditorSettings.hpp"
#include "Utility/EditorUI.hpp"

#include <cstdio>
#include <iterator>

namespace Surge
{
    namespace
    {
        struct AspectPreset
        {
            const char* Name;
            float Aspect; // Width / height, 0 = fills the window
        };

        constexpr AspectPreset ASPECT_PRESETS[] = {
            { "Free Aspect", 0.0f },
            { "16:9 Landscape", 16.0f / 9.0f },
            { "9:16 Portrait", 9.0f / 16.0f },
            { "21:9 Cinematic", 21.0f / 9.0f },
            { "4:3 Classic", 4.0f / 3.0f },
        };
        constexpr int ASPECT_PRESET_COUNT = static_cast<int>(std::size(ASPECT_PRESETS));

        // Typical phone insets as fractions of the screen (left, top, right, bottom)
        struct SafeAreaPreset
        {
            const char* Name;
            glm::vec4 Insets;
        };

        constexpr SafeAreaPreset SAFE_AREA_PRESETS[] = {
            { "No Safe Area", { 0.0f, 0.0f, 0.0f, 0.0f } },
            { "Phone Portrait", { 0.0f, 0.05f, 0.0f, 0.03f } },
            { "Phone Landscape", { 0.06f, 0.0f, 0.06f, 0.05f } },
            { "Tablet", { 0.0f, 0.03f, 0.0f, 0.025f } },
        };
        constexpr int SAFE_AREA_PRESET_COUNT = static_cast<int>(std::size(SAFE_AREA_PRESETS));

        float ToolbarHeight() { return ImGui::GetFrameHeight() + 6.0f; }

    } // namespace

    GameViewport::GameViewport()
        : EditorViewport(ViewportType::GAME, 1)
    {
    }

    void GameViewport::ApplySettings(const ViewportSettings& settings)
    {
        mAspectMode = glm::clamp(settings.AspectMode, 0, ASPECT_PRESET_COUNT - 1);
    }

    void GameViewport::CaptureSettings(ViewportSettings& settings) const
    {
        settings.AspectMode = mAspectMode;
    }

    void GameViewport::UpdateViewCamera(Scene& scene)
    {
        // Same pick as the Player (Scene::GetMainCameraEntity), with this view's aspect unless the camera fixes its own
        mHasCamera = false;
        auto view = scene.GetRegistry().view<TransformComponent, CameraComponent>();
        for(const auto& [entity, transform, camera] : view.each())
        {
            if(!camera.Primary || !camera.Active)
                continue;

            RuntimeCamera runtimeCamera = camera.Camera;
            if(!camera.FixedAspectRatio && mImageSize.x > 0.0f && mImageSize.y > 0.0f)
                runtimeCamera.SetViewportSize(mImageSize.x, mImageSize.y);

            const bool orthographic = runtimeCamera.GetProjectionType() == RuntimeCamera::ProjectionType::Orthographic;
            const glm::vec2 nearFar = orthographic ? glm::vec2(runtimeCamera.GetOrthographicNearClip(), runtimeCamera.GetOrthographicFarClip())
                                                   : glm::vec2(runtimeCamera.GetPerspectiveNearClip(), runtimeCamera.GetPerspectiveFarClip());
            Core::GetRenderer()->SetViewCamera(mView, glm::inverse(transform.GetTransform()), runtimeCamera.GetProjectionMatrix(), nearFar);
            mHasCamera = true;
            return;
        }
    }

    void GameViewport::ComputeImageRect(const ImVec2& available, ImVec2& outOffset, ImVec2& outSize)
    {
        // Toolbar strip on top, the image letterboxed below it
        const float toolbarHeight = ToolbarHeight();
        const ImVec2 area(available.x, std::max(0.0f, available.y - toolbarHeight));
        const float aspect = ASPECT_PRESETS[mAspectMode].Aspect;

        ImVec2 size = area;
        if(aspect > 0.0f && area.x > 0.0f && area.y > 0.0f)
        {
            size = { area.x, area.x / aspect };
            if(size.y > area.y)
                size = { area.y * aspect, area.y };
        }

        outOffset = { (area.x - size.x) * 0.5f, toolbarHeight + (area.y - size.y) * 0.5f };
        outSize = size;
    }

    void GameViewport::OnViewportGUI(const ImVec2& imageMin, const ImVec2& imageSize)
    {
        Editor* editor = static_cast<Editor*>(Core::GetClient());

        // In-game UI input area, and the UI Editor's handles (edit mode)
        const ImVec2 windowPos = ImGui::GetMainViewport()->Pos;
        UI::Manager& uiManager = Core::GetRenderer()->GetUIManager();
        uiManager.SetViewportBounds(imageMin.x - windowPos.x, imageMin.y - windowPos.y, imageSize.x, imageSize.y);
        const glm::vec4 safeInsets = SAFE_AREA_PRESETS[mSafeAreaMode].Insets;
        uiManager.SetSimulatedSafeArea(safeInsets, mSafeAreaMode != 0);
        if(mSafeAreaMode != 0)
        {
            // Unsafe bands, what a notch / the system bars would cover
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 imageMax = { imageMin.x + imageSize.x, imageMin.y + imageSize.y };
            const ImU32 color = EditorUI::ToU32(EditorUI::WithAlpha(EditorUI::Colors::Error, 0.18f));
            drawList->AddRectFilled(imageMin, { imageMin.x + imageSize.x * safeInsets.x, imageMax.y }, color);
            drawList->AddRectFilled(imageMin, { imageMax.x, imageMin.y + imageSize.y * safeInsets.y }, color);
            drawList->AddRectFilled({ imageMax.x - imageSize.x * safeInsets.z, imageMin.y }, imageMax, color);
            drawList->AddRectFilled({ imageMin.x, imageMax.y - imageSize.y * safeInsets.w }, imageMax, color);
        }
        editor->GetPanelManager().GetPanel<UIEditorPanel>()->OnViewportGUI(imageMin, imageSize);

        if(!mHasCamera)
        {
            const char* title = "No primary cameras rendering";
            const char* hint = "Add an Entity -> Add a Camera component to it -> Mark 'Primary' as true";
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const float centerX = imageMin.x + imageSize.x * 0.5f;
            const float centerY = imageMin.y + imageSize.y * 0.5f;

            ImGui::PushFont(EditorUI::BoldFont(), 20.0f);
            const ImVec2 titleSize = ImGui::CalcTextSize(title);
            drawList->AddText(ImVec2(std::floor(centerX - titleSize.x * 0.5f), std::floor(centerY - titleSize.y)), EditorUI::ToU32(EditorUI::Colors::Viewport::TextDim), title);
            ImGui::PopFont();
            const ImVec2 hintSize = ImGui::CalcTextSize(hint);
            drawList->AddText(ImVec2(std::floor(centerX - hintSize.x * 0.5f), std::floor(centerY + 6.0f)), EditorUI::ToU32(EditorUI::Colors::Viewport::TextFaint), hint);
        }

        // Toolbar: aspect preset left, output resolution right
        const ImVec2 contentMin = mContentMin;
        const float contentWidth = mContentSize.x;
        const float toolbarHeight = ToolbarHeight();
        ImGui::GetWindowDrawList()->AddRectFilled(contentMin, ImVec2(contentMin.x + contentWidth, contentMin.y + toolbarHeight), EditorUI::ToU32(EditorUI::Colors::Tone09));

        ImGui::SetCursorScreenPos(ImVec2(contentMin.x + 4.0f, contentMin.y + 3.0f));
        ImGui::SetNextItemWidth(150.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, EditorUI::Colors::Tone06);
        if(ImGui::BeginCombo("##Aspect", ASPECT_PRESETS[mAspectMode].Name))
        {
            for(int i = 0; i < ASPECT_PRESET_COUNT; i++)
            {
                const bool selected = mAspectMode == i;
                if(ImGui::Selectable(ASPECT_PRESETS[i].Name, selected))
                    mAspectMode = i;
                if(selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if(ImGui::BeginCombo("##SafeArea", SAFE_AREA_PRESETS[mSafeAreaMode].Name))
        {
            for(int i = 0; i < SAFE_AREA_PRESET_COUNT; i++)
            {
                if(ImGui::Selectable(SAFE_AREA_PRESETS[i].Name, mSafeAreaMode == i))
                    mSafeAreaMode = i;
            }
            ImGui::EndCombo();
        }
        if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("Simulated device safe area for Safe Area widgets (notch, status and navigation bars)");
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::Text("%s", Core::GetEngineBuildConfiguration().c_str());

        char resolution[48];
        std::snprintf(resolution, sizeof(resolution), "%d x %d", static_cast<int>(imageSize.x), static_cast<int>(imageSize.y));
        const float textWidth = ImGui::CalcTextSize(resolution).x;
        ImGui::GetWindowDrawList()->AddText(ImVec2(contentMin.x + contentWidth - textWidth - 8.0f, contentMin.y + (toolbarHeight - ImGui::GetTextLineHeight()) * 0.5f), EditorUI::ToU32(EditorUI::Colors::TextMuted), resolution);
    }

} // namespace Surge
