// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/SceneHierarchyPanel.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/ECS/Components.hpp"
#include "Utility/ImGuiAux.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Graphics/HighLevel/DefaultMeshes.hpp"
#include "Surge/Graphics/HighLevel/Mesh.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>

#define IMGUI_ENTITY_PAYLOAD "ENTITY_PAYLOAD"

namespace Surge
{
    static char sSearchBuffer[256] = "";

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
                if(e) // Might be null if mSelectedEntity has a parent
                    mSelectedEntity = e;
            }
        });
    }

    void SceneHierarchyPanel::Render(bool* show)
    {
        if(!*show || !mSceneContext)
            return;

        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));

        if(ImGui::Begin(PanelCodeToString(mCode), show))
        {
            mHierarchyHovered = ImGui::IsWindowHovered();

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

            {
                ImGuiAux::ScopedBoldFont font;
                constexpr const char* addButtonLabel = " ADD ";

                // Search Bar
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(addButtonLabel).x - 20.0f); // 20.0f is padding + scroll bar width (approx.)
                ImGui::InputTextWithHint("##Search", "Search Entities...", sSearchBuffer, 256);

                ImGui::SameLine();

                // ADD Button
                if(ImGui::Button(addButtonLabel, ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                    ImGui::OpenPopup("AddEntityContext");
            }

            ImGui::PopStyleVar();
            ImGui::Spacing();

            // Unified Popup Context (Handles both the ADD button and right-clicking empty space)
            ImGuiAux::StyledPopupVars::Push();
            if(ImGui::BeginPopup("AddEntityContext") || ImGui::BeginPopupContextWindow("HierarchySpace", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
            {
                ImFont* boldFont = ImGui::GetIO().Fonts->Fonts[1];

                if(ImGuiAux::StyledMenuItem("Empty Entity"))
                    mSceneContext->CreateEntity(mSelectedEntity, "Entity");

                ImGuiAux::StyledSeparator();
                ImGui::PushFont(boldFont);
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "RENDERING");
                ImGui::PopFont();
                ImGuiAux::StyledSeparator();

                if(ImGuiAux::StyledMenuItem("Camera"))
                {
                    mSceneContext->CreateEntity(mSelectedEntity, "Camera");
                    mSelectedEntity.AddComponent<CameraComponent>();
                }
                if(ImGuiAux::StyledMenuItem("Sprite Renderer"))
                {
                    mSceneContext->CreateEntity(mSelectedEntity, "Sprite");
                    mSelectedEntity.AddComponent<SpriteRendererComponent>(glm::vec4 { 1.0f, 1.0f, 1.0f, 1.0f });
                }

                ImGuiAux::StyledSeparator();
                ImGui::PushFont(boldFont);
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "3D PRIMITIVES");
                ImGui::PopFont();
                ImGuiAux::StyledSeparator();

                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
                if(ImGui::BeginMenu("Meshes"))
                {
                    const char* defMesh = "";
                    if(ImGuiAux::StyledMenuItem("Empty Mesh")) defMesh = "Empty";
                    if(ImGuiAux::StyledMenuItem("Cube"))       defMesh = DefaultMesh::CUBE;
                    if(ImGuiAux::StyledMenuItem("Sphere"))     defMesh = DefaultMesh::SPHERE;
                    if(ImGuiAux::StyledMenuItem("Bean"))       defMesh = DefaultMesh::BEAN;
                    if(ImGuiAux::StyledMenuItem("Cone"))       defMesh = DefaultMesh::CONE;
                    if(ImGuiAux::StyledMenuItem("Cylinder"))   defMesh = DefaultMesh::CYLINDER;
                    if(ImGuiAux::StyledMenuItem("Torus"))      defMesh = DefaultMesh::TORUS;
                    if(ImGuiAux::StyledMenuItem("Plane"))      defMesh = DefaultMesh::PLANE;

                    if(strcmp(defMesh, "Empty") == 0)
                    {
                        mSceneContext->CreateEntity(mSelectedEntity, "Mesh");
                        mSelectedEntity.AddComponent<MeshComponent>();
                    }
                    else if(strcmp(defMesh, "") != 0)
                    {
                        mSceneContext->CreateEntity(mSelectedEntity, "Mesh");
                        MeshComponent& meshComponent = mSelectedEntity.AddComponent<MeshComponent>();
                        AssetManager* am = Core::GetAssetManager();
                        meshComponent.MeshAsset = am->Load<Mesh>(am->Import(defMesh, AssetType::MESH));
                    }
                    ImGui::EndMenu();
                }
                ImGui::PopStyleVar();

                ImGuiAux::StyledSeparator();
                ImGui::PushFont(boldFont);
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "WORLD & UI");
                ImGui::PopFont();
                ImGuiAux::StyledSeparator();

                if(ImGuiAux::StyledMenuItem("Environment"))
                {
                    mSceneContext->CreateEntity(mSelectedEntity, "Environment");
                    mSelectedEntity.AddComponent<EnvironmentComponent>();
                }
                if(ImGuiAux::StyledMenuItem("Directional Light"))
                {
                    mSceneContext->CreateEntity(mSelectedEntity, "Directional Light");
                    mSelectedEntity.AddComponent<LightComponent>().Type = LightType::DIRECTIONAL;
                }
                if(ImGuiAux::StyledMenuItem("Point Light"))
                {
                    mSceneContext->CreateEntity(mSelectedEntity, "Point Light");
                    mSelectedEntity.AddComponent<LightComponent>().Type = LightType::POINT;
                }
                if(ImGuiAux::StyledMenuItem("Text"))
                {
                    mSceneContext->CreateEntity(mSelectedEntity, "Text");
                    mSelectedEntity.AddComponent<TextComponent>();
                }
                if(ImGuiAux::StyledMenuItem("UI Canvas"))
                {
                    mSceneContext->CreateEntity(mSelectedEntity, "Canvas");
                    mSelectedEntity.AddComponent<UICanvasComponent>();
                }

                ImGuiAux::EndStyledPopup();
            }
            else
            {
                ImGuiAux::StyledPopupVars::Pop();
            }

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.0f, 2.0f));

            if(ImGui::BeginTable("HierarchyTable", 2, ImGuiTableFlags_BordersInnerV))
            {
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 75.0f);
                ImGui::TableHeadersRow();

                bool isSearching = strlen(sSearchBuffer) > 0;
                String searchStr = sSearchBuffer;
                if(isSearching)
                    std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

                mSceneContext->GetRegistry().each([&](entt::entity entityID) {
                    Entity ent = Entity(entityID, mSceneContext);

                    if(ent.HasComponent<RelationshipComponent>())
                    {
                        auto& rel = ent.GetComponent<RelationshipComponent>();

                        if(isSearching)
                        {
                            String nameLower = ent.GetComponent<NameComponent>().Name;
                            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

                            if(nameLower.find(searchStr) != String::npos)
                                DrawEntityNode(ent);
                        }
                        else
                        {
                            if(rel.Parent == entt::null) // Only dispatch roots, children drawn recursively
                                DrawEntityNode(ent);
                        }
                    }
                });
                ImGui::EndTable();
            }

            ImGui::PopStyleVar(3);

            float emptySpaceY = ImGui::GetContentRegionAvail().y;
            if(emptySpaceY > 0.0f)
            {
                ImGui::InvisibleButton("##RootDropZone", { ImGui::GetContentRegionAvail().x, emptySpaceY });

                if(ImGui::IsItemClicked(ImGuiMouseButton_Left))
                {
                    mSelectedEntity = {};
                    mSceneContext->SetSelectedEntity({});
                }

                if(ImGui::IsItemClicked(ImGuiMouseButton_Right))
                    ImGui::OpenPopup("AddEntityContext");

                if(ImGui::BeginDragDropTarget())
                {
                    if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(IMGUI_ENTITY_PAYLOAD))
                    {
                        entt::entity droppedEntityID = *(const entt::entity*)payload->Data;
                        Entity dropped(droppedEntityID, mSceneContext);
                        mSceneContext->SetParent(dropped, Entity {});
                    }
                    ImGui::EndDragDropTarget();
                }
            }
        }
        ImGui::PopStyleColor(2);
        ImGui::End();
    }

    void SceneHierarchyPanel::DrawEntityNode(Entity& e)
    {
        String& name = e.GetComponent<NameComponent>().Name;
        auto& rel = e.GetComponent<RelationshipComponent>();

        bool isSelectedEntity = (mSelectedEntity == e);
        bool isSearching = strlen(sSearchBuffer) > 0;
        bool hasChildren = (rel.FirstChild != entt::null) && !isSearching;

        ImGuiTreeNodeFlags flags = (isSelectedEntity ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
            | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_DrawLinesFull;

        if(!hasChildren)
            flags |= ImGuiTreeNodeFlags_Leaf;

        bool opened = false;

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        if(isSelectedEntity)
        {
            ImGuiAux::ScopedBoldFont font;
            ImGui::PushStyleColor(ImGuiCol_Header, ImGuiAux::Colors::ThemeColor2);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGuiAux::Colors::ThemeColor2);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGuiAux::Colors::ThemeColor2);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));

            opened = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uint64_t>(static_cast<Uint>(e.Raw()))), flags, "%s", name.c_str());

            ImGui::PopStyleColor(4);
        }
        else
        {
            opened = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uint64_t>(static_cast<Uint>(e.Raw()))), flags, "%s", name.c_str());
        }

        if(ImGui::BeginDragDropSource())
        {
            entt::entity entityHandle = e.Raw();
            ImGui::SetDragDropPayload(IMGUI_ENTITY_PAYLOAD, &entityHandle, sizeof(entt::entity));
            ImGui::Text("Move %s", name.c_str());
            ImGui::EndDragDropSource();
        }
        if(!isSearching && ImGui::BeginDragDropTarget())
        {
            if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(IMGUI_ENTITY_PAYLOAD))
            {
                entt::entity droppedEntityID = *(const entt::entity*)payload->Data;

                // Cyclic Parenting Protection
                bool isDescendant = false;
                entt::entity currentCheck = e.Raw();
                while(currentCheck != entt::null)
                {
                    if(currentCheck == droppedEntityID)
                    {
                        isDescendant = true;
                        break;
                    }
                    currentCheck = (entt::entity)mSceneContext->GetRegistry().get<RelationshipComponent>(currentCheck).Parent;
                }
                if(!isDescendant && droppedEntityID != e.Raw())
                {
                    Entity dropped(droppedEntityID, mSceneContext);
                    mSceneContext->SetParent(dropped, e);
                }
                else if(isDescendant)
                    Log<Severity::Warn>("Cannot parent an entity to its own descendant!");
            }
            ImGui::EndDragDropTarget();
        }

        // CLICK SELECTION
        if(ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
        {
            mSelectedEntity = e;
            mSceneContext->SetSelectedEntity(mSelectedEntity);
            ImGui::SetWindowFocus("Inspector");
        }
\
        if(ImGuiAux::BeginStyledPopupContextItem())
        {
            ImFont* boldFont = ImGui::GetIO().Fonts->Fonts[1];

            // Header showing the selected entity's name
            ImGui::PushFont(boldFont);
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", name.c_str());
            ImGui::PopFont();
            ImGuiAux::StyledSeparator();

            if(rel.Parent != entt::null)
            {
                if(ImGuiAux::StyledMenuItem("Unparent"))
                    mSceneContext->SetParent(e, Entity {});
                ImGuiAux::StyledSeparator();
            }

            if(ImGuiAux::StyledMenuItem("Duplicate", "ScrollLock"))
            {
                Entity clone = mSceneContext->DuplicateEntity(e);
                if(clone)
                    mSelectedEntity = clone;
            }
            if(ImGuiAux::StyledMenuItem("Delete", "Del"))
            {
                if(mSelectedEntity == e)
                {
                    mSelectedEntity = {};
                    mSceneContext->SetSelectedEntity(mSelectedEntity);
                }
                Core::AddFrameEndCallback([this, e]() { mSceneContext->DestroyEntity(e); });
            }
            ImGuiAux::EndStyledPopup();
        }

        ImGui::TableNextColumn();

        String typeTag = "Entity";
        if(e.HasComponent<CameraComponent>())              typeTag = "CAMERA";
        else if(e.HasComponent<TextComponent>())           typeTag = "TEXT";
        else if(e.HasComponent<LightComponent>())          typeTag = "LIGHT";
        else if(e.HasComponent<MeshComponent>())           typeTag = "MESH";
        else if(e.HasComponent<SpriteRendererComponent>()) typeTag = "SPRITE";
        else if(e.HasComponent<EnvironmentComponent>())    typeTag = "ENV";

        if(isSelectedEntity)
            ImGui::TextUnformatted(typeTag.c_str());
        else
            ImGui::TextDisabled("%s", typeTag.c_str());

        if(opened)
        {
            if(!isSearching)
            {
                entt::entity currentChild = (entt::entity)e.GetComponent<RelationshipComponent>().FirstChild;
                while(currentChild != entt::null)
                {
                    Entity childEnt(currentChild, mSceneContext);
                    DrawEntityNode(childEnt);
                    currentChild = (entt::entity)childEnt.GetComponent<RelationshipComponent>().NextSibling;
                }
            }
            ImGui::TreePop();
        }
    }

    void SceneHierarchyPanel::Shutdown()
    {
    }

} // namespace Surge