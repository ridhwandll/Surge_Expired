// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/SceneHierarchyPanel.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/ECS/Components.hpp"
#include "Utility/ImGuiAux.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Graphics/HighLevel/DefaultMeshes.hpp"
#include <imgui.h>
#include <imgui_internal.h>

namespace Surge
{
    void SceneHierarchyPanel::Init(void*)
    {
        mCode = GetStaticCode();
        mSceneContext = nullptr;
        mSelectedEntity = {};
    }

    void SceneHierarchyPanel::OnEvent(Event& e)
    {
        if(!mSelectedEntity || !mSceneContext)
            return;

        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<KeyPressedEvent>([&](KeyPressedEvent& keyEvent) {
            if(keyEvent.GetKeyCode() == Key::Delete && mHierarchyHovered)
            {
                mSceneContext->DestroyEntity(mSelectedEntity);
                mSelectedEntity = {};
            }
            if(keyEvent.GetKeyCode() == Key::ScrollLock)
            {
                Entity e = mSceneContext->DuplicateEntity(mSelectedEntity);
                mSelectedEntity = e;
            }
        });
    }

    void SceneHierarchyPanel::Render(bool* show)
    {
        if(!*show || !mSceneContext)
            return;

        // Draw the entities
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
        if(ImGui::Begin(PanelCodeToString(mCode), show))
        {
            mHierarchyHovered = ImGui::IsWindowHovered();
            if(ImGuiAux::Button("Add Entity", { ImGui::GetWindowWidth() - 15, 0.0f }))
                ImGui::OpenPopup("Add Entity");

            if(ImGui::BeginPopup("Add Entity") || (ImGui::BeginPopupContextWindow(nullptr, 1)))
            {
                if(ImGui::MenuItem("Empty Entity"))
                {
                    mSceneContext->CreateEntity(mSelectedEntity, "Entity");
                }
                if(ImGui::MenuItem("Camera"))
                {
                    mSceneContext->CreateEntity(mSelectedEntity, "Camera");
                    mSelectedEntity.AddComponent<CameraComponent>();
                }
                ImGui::Separator();
                if(ImGui::MenuItem("Sprite Renderer"))
                {
                    mSceneContext->CreateEntity(mSelectedEntity, "Sprite");
                    mSelectedEntity.AddComponent<SpriteRendererComponent>(ImGuiAux::Colors::ThemeColor2);
                }
                ImGui::Separator();
                if(ImGui::BeginMenu("Mesh"))
                {
                    const char* defMesh = "";
                    if(ImGui::MenuItem("Empty Mesh"))
                        defMesh = "Empty";
                    if(ImGui::MenuItem("Cube"))
                        defMesh = DefaultMesh::CUBE;
                    if(ImGui::MenuItem("Sphere"))
                        defMesh = DefaultMesh::SPHERE;
                    if(ImGui::MenuItem("Bean"))
                        defMesh = DefaultMesh::BEAN;
                    if(ImGui::MenuItem("Cone"))
                        defMesh = DefaultMesh::CONE;
                    if(ImGui::MenuItem("Cylinder"))
                        defMesh = DefaultMesh::CYLINDER;
                    if(ImGui::MenuItem("Torus"))
                        defMesh = DefaultMesh::TORUS;
                    if(ImGui::MenuItem("Plane"))
                        defMesh = DefaultMesh::PLANE;

                    if(strcmp(defMesh, "Empty") == 0)
                    {
                        mSceneContext->CreateEntity(mSelectedEntity, "Mesh");
                        mSelectedEntity.AddComponent<MeshComponent>();
                    }
                    else if(strcmp(defMesh, "") != 0)
                    {
                        mSceneContext->CreateEntity(mSelectedEntity, "Mesh");
                        MeshComponent& meshComponent = mSelectedEntity.AddComponent<MeshComponent>();
                        meshComponent.MeshID = Core::GetAssetManager()->Import(defMesh, AssetType::MESH);
                    }
                    ImGui::EndPopup();
                }
                ImGui::Separator();
                if(ImGui::MenuItem("Light"))
                {
                    mSceneContext->CreateEntity(mSelectedEntity, "Light");
                    mSelectedEntity.AddComponent<LightComponent>();
                }
                if(ImGui::MenuItem("Environment"))
                {
                    mSceneContext->CreateEntity(mSelectedEntity, "Environment");
                    mSelectedEntity.AddComponent<EnvironmentComponent>();
                }
                ImGui::EndPopup();
            }

            constexpr ImGuiTableFlags flags = ImGuiTableFlags_Resizable;
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 0.0f, 2.8f });
            if(ImGui::BeginTable("HierarchyTable", 2, flags))
            {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("A").x * 12.0f);
                ImGui::TableHeadersRow();

                static Vector<entt::entity> sActiveEntities;
                sActiveEntities.clear();
                mSceneContext->GetRegistry().each([&](entt::entity e) { sActiveEntities.push_back(e); });

                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(sActiveEntities.size()));
                while(clipper.Step())
                {
                    for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                    {
                        ImGui::PushID(i);
                        Entity ent = Entity(sActiveEntities[i], mSceneContext);

                        ImGui::TableNextColumn();
                        DrawEntityNode(ent);

                        ImGui::PopID();
                    }
                }
                clipper.End();

                ImGui::EndTable();
            }
            ImGui::PopStyleVar();
        }
        ImGui::PopStyleColor(2);
        ImGui::End();
    }

    void SceneHierarchyPanel::DrawEntityNode(Entity& e)
    {
        String& name = e.GetComponent<NameComponent>().Name;
        // ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_Leaf added as we dont have entity parenting yet
        ImGuiTreeNodeFlags flags = ((mSelectedEntity == e) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanFullWidth;
        bool isSelectedEntity = false;
        if(mSelectedEntity == e)
            isSelectedEntity = true;

        {
            ImGuiAux::ScopedColor style({ ImGuiCol_Header, ImGuiCol_HeaderHovered, ImGuiCol_HeaderActive }, { 0.0, 0.0, 0.0, 0.0 });
            if(isSelectedEntity)
            {
                ImGuiAux::ScopedBoldFont font;
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.123f, 0.123f, 0.123f, 1.0f));
                ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uint64_t>(static_cast<Uint>(e.Raw()))), flags, "%s", name.c_str());
                ImGui::PopStyleColor();
            }
            else
                ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uint64_t>(static_cast<Uint>(e.Raw()))), flags, "%s", name.c_str());
        }

        if(ImGui::IsItemClicked())
        {
            mSelectedEntity = e;
            mSceneContext->SetSelectedEntity(mSelectedEntity);
            ImGui::SetWindowFocus("Inspector");
        }

        if(isSelectedEntity)
        {
            if(mSelectedEntity)
            {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::ColorConvertFloat4ToU32(ImGuiAux::Colors::ThemeColor2));
            }
        }

        if(ImGui::BeginPopupContextItem())
        {
            if(ImGui::MenuItem("Delete"))
            {
                if(mSelectedEntity == e)
                {
                    mSelectedEntity = {};
                    mSceneContext->SetSelectedEntity(mSelectedEntity);
                }

                // Only execute when the frame ends, else it will give crash on "Entity not found"
                Surge::Core::AddFrameEndCallback([this, e]() { mSceneContext->DestroyEntity(e); });
            }
            ImGui::EndPopup();
        }
        ImGui::TreePop();

        ImGui::TableNextColumn();
        {

            if(isSelectedEntity)
            {
                ImGuiAux::ScopedBoldFont font;
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.123f, 0.123f, 0.123f, 1.0f));
                ImGui::TextUnformatted("Selected Entity");
                ImGui::PopStyleColor();
            }
            else ImGui::TextUnformatted("Entity");
        }
    }

    void SceneHierarchyPanel::Shutdown()
    {}

} // namespace Surge