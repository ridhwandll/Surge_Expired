// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Viewport/EditorViewport.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Input/Input.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/ECS/Components/Components.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Editor.hpp"
#include "Panels/ContentBrowserPanel.hpp"
#include "Panels/UIEditorPanel.hpp"
#include "Panels/ParticleEditorPanel.hpp"
#include "Utility/EditorUI.hpp"

#include <format>

namespace Surge
{
    EditorViewport::EditorViewport(ViewportType type, Uint index)
        : mType(type), mIndex(index), mWindowName(MakeWindowName(type, index))
    {
        RenderViewDesc desc;
        desc.DebugName = type == ViewportType::SCENE ? std::format("Scene View {}", index) : String("Game View"); // Also names it in the Renderer panel
        desc.EditorOverlay = type == ViewportType::SCENE; // Colliders, axes
        desc.ShowUI = type == ViewportType::GAME;
        mView = Core::GetRenderer()->CreateView(desc);
    }

    EditorViewport::~EditorViewport()
    {
        Core::GetRenderer()->DestroyView(mView);
    }

    String EditorViewport::MakeWindowName(ViewportType type, Uint index)
    {
        // The part after ### is the ImGui ID: the docking layout (imgui.ini) follows the index, not the title
        const char* label = type == ViewportType::SCENE ? "Scene" : "Game";
        if(index <= 1)
            return std::format("{}###{}View{}", label, label, index);
        return std::format("{} ({})###{}View{}", label, index, label, index);
    }

    void EditorViewport::Render()
    {
        mVisible = false;
        mHovered = false;
        mFocused = false;

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        if(mIsFullscreen)
        {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);
            ImGui::SetNextWindowViewport(viewport->ID);
            windowFlags |= ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        }
        else if(mRestoreDockAfterFullscreen)
        {
            ImGui::SetNextWindowDockID(mDockBeforeFullscreen, ImGuiCond_Always);
            mRestoreDockAfterFullscreen = false;
            mFocusRequested = true;
        }
        else if(mInitialDockID)
        {
            ImGui::SetNextWindowDockID(mInitialDockID, ImGuiCond_FirstUseEver);
            mInitialDockID = 0;
        }

        if(mFocusRequested)
        {
            ImGui::SetNextWindowFocus();
            mFocusRequested = false;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
        const bool expanded = ImGui::Begin(mWindowName.c_str(), mIsFullscreen ? nullptr : &mOpen, windowFlags);
        ImGui::PopStyleVar(); // Popups opened from the overlays keep the regular padding

        if(expanded)
        {
            mVisible = true;
            mHovered = ImGui::IsWindowHovered();
            mFocused = ImGui::IsWindowFocused();
            if(!mIsFullscreen)
                mDockID = ImGui::GetWindowDockID();

            mContentMin = ImGui::GetCursorScreenPos();
            mContentSize = ImGui::GetContentRegionAvail();
            ImGui::GetWindowDrawList()->AddRectFilled(mContentMin, ImVec2(mContentMin.x + mContentSize.x, mContentMin.y + mContentSize.y), EditorUI::ToU32(EditorUI::Colors::Tone06)); // Letterbox

            ImVec2 offset = { 0.0f, 0.0f };
            ImVec2 size = mContentSize;
            ComputeImageRect(mContentSize, offset, size);
            offset = { std::floor(offset.x), std::floor(offset.y) };
            size = { std::floor(size.x), std::floor(size.y) };

            if(size.x >= 1.0f && size.y >= 1.0f)
            {
                // Only moved when an item follows: ImGui errors on a cursor that extends the window without one (a Game view shorter than its toolbar)
                const ImVec2 cursorStart = ImGui::GetCursorPos();
                ImGui::SetCursorPos({ cursorStart.x + offset.x, cursorStart.y + offset.y });
                const ImVec2 imageMin = ImGui::GetCursorScreenPos();
                mImageSize = { size.x, size.y };

                Renderer* renderer = Core::GetRenderer();
                renderer->ResizeView(mView, static_cast<Uint>(size.x), static_cast<Uint>(size.y));

                // Requested for the next frame: ImGui runs after the views were drawn
                const bool wantsRender = WantsRender();
                if(wantsRender)
                    renderer->RequestViewRender(mView);

                if(wantsRender && renderer->HasViewOutput(mView))
                    ImGui::Image(static_cast<ImTextureID>(renderer->GetViewImGuiID(mView)), size);
                else
                    ImGui::Dummy(size); // Not drawn yet (or no camera): the dark background shows

                HandleAssetDrop();
                OnViewportGUI(imageMin, size);
            }
            else
                mImageSize = { 0.0f, 0.0f };
        }
        else
            mImageSize = { 0.0f, 0.0f };

        ImGui::End();
    }

    void EditorViewport::OnEvent(Event& e)
    {
        if(!mHovered && !mIsFullscreen)
            return;

        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& keyEvent) {
            if(Input::IsMouseButtonPressed(Mouse::ButtonRight) || Input::IsKeyPressed(Key::LeftControl))
                return;

            const bool toggle = keyEvent.GetKeyCode() == Key::F11;
            const bool leave = keyEvent.GetKeyCode() == Key::Escape && mIsFullscreen;
            if(!toggle && !leave)
                return;

            mIsFullscreen = toggle ? !mIsFullscreen : false;
            if(mIsFullscreen)
                mDockBeforeFullscreen = mDockID;
            else
                mRestoreDockAfterFullscreen = true;

            static_cast<Editor*>(Core::GetClient())->ShowTitlebar(!mIsFullscreen);
        });
    }

    void EditorViewport::ComputeImageRect(const ImVec2& available, ImVec2& outOffset, ImVec2& outSize)
    {
        outOffset = { 0.0f, 0.0f };
        outSize = available;
    }

    void EditorViewport::DrawCornerLabel(const char* text, const ImVec2& imageMin, const ImVec2& imageSize) const
    {
        constexpr float padding = 3.0f;
        ImGui::PushFont(EditorUI::BoldFont());
        const float textHeight = ImGui::GetTextLineHeight();
        const float textWidth = ImGui::CalcTextSize(text).x;

        const ImVec2 bgMax(imageMin.x + imageSize.x - padding, imageMin.y + padding + textHeight + padding * 2.0f);
        const ImVec2 bgMin(bgMax.x - textWidth - padding * 2.0f, imageMin.y + padding);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(bgMin, bgMax, EditorUI::ToU32(EditorUI::Colors::Overlay), 4.0f);
        drawList->AddText(ImVec2(bgMin.x + padding, bgMin.y + padding), EditorUI::ToU32(EditorUI::Colors::Tone90), text);
        ImGui::PopFont();
    }

    void EditorViewport::HandleAssetDrop()
    {
        if(!ImGui::BeginDragDropTarget())
            return;

        if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_PAYLOAD))
        {
            SG_ASSERT(payload->DataSize == sizeof(AssetID), "Payload size mismatch!");
            const AssetID droppedAssetID = *static_cast<const AssetID*>(payload->Data);
            AssetManager* assetManager = Core::GetAssetManager();
            Editor* editor = static_cast<Editor*>(Core::GetClient());

            switch(assetManager->GetMetadata(droppedAssetID).Type)
            {
                case AssetType::UI_LAYOUT:
                    editor->GetPanelManager().GetPanel<UIEditorPanel>()->OpenLayout(droppedAssetID);
                    break;
                case AssetType::SCENE:
                    editor->OpenScene(droppedAssetID);
                    break;
                case AssetType::MESH:
                {
                    Ref<Mesh> droppedMesh = assetManager->Load<Mesh>(droppedAssetID);
                    if(droppedMesh && !editor->IsPlaying())
                    {
                        Entity newEntity;
                        editor->GetCurrentScene()->CreateEntity(newEntity, "Mesh");
                        newEntity.AddComponent<MeshComponent>().MeshAsset = droppedMesh;
                        editor->OnEntityCreated(newEntity);
                    }
                    break;
                }
                case AssetType::PARTICLE_EFFECT:
                {
                    Ref<ParticleEffect> effect = assetManager->Load<ParticleEffect>(droppedAssetID);
                    if(effect && !editor->IsPlaying())
                    {
                        Entity newEntity;
                        editor->GetCurrentScene()->CreateEntity(newEntity, Path(assetManager->GetMetadata(droppedAssetID).RelativePath).stem().string());
                        newEntity.AddComponent<ParticleSystemComponent>().Effect = effect;
                        editor->OnEntityCreated(newEntity);
                    }
                    break;
                }
                default:
                    Log<Severity::Warn>("[Viewport] This asset type can't be dropped in a viewport");
                    break;
            }
        }
        ImGui::EndDragDropTarget();
    }

} // namespace Surge
