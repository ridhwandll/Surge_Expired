// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/SceneHierarchyPanel.hpp"
#include "Surge/Core/Core.hpp"

#include "Surge/ECS/Components/Components.hpp"
#include "Surge/ECS/Components/ScriptComponent.hpp"

#include "Utility/ImGuiAux.hpp"
#include "Editor.hpp"
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

    void SceneHierarchyPanel::OnEvent(Event&)
    {
        // Delete/Duplicate/Copy/Paste shortcuts live in Editor::HandleShortcuts (they go through the undo history)
    }

    void SceneHierarchyPanel::Render(bool* show)
    {
        if(!*show || !mSceneContext)
            return;

        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));

        mHierarchyFocused = false;
        mHierarchyHovered = false;
        if(ImGui::Begin(PanelCodeToString(mCode), show))
        {
            Editor* editor = static_cast<Editor*>(Core::GetClient());
            mHierarchyHovered = ImGui::IsWindowHovered();
            mHierarchyFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

            {
                ImGuiAux::ScopedBoldFont font;
                constexpr const char* addButtonLabel = " ADD ";

                // Search Bar
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(addButtonLabel).x - 20.0f); // 20.0f is padding + scroll bar width (approx.)

                ImGui::InputTextWithHint("##Search", std::format("Search in Total {} Entities...", mSceneContext->GetRegistry().alive()).c_str(), sSearchBuffer, 256);

                ImGui::SameLine();

                // ADD Button
                if(ImGui::Button(addButtonLabel, ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                    ImGui::OpenPopup("AddEntityContext");
            }

            ImGui::PopStyleVar();
            ImGui::Spacing();

            // Unified Popup Context (Handles both the ADD button and right-clicking empty space)
            // Every item below creates into mSelectedEntity, a changed selection afterwards means "created" (recorded for undo)
            const Entity selectionBeforeMenu = mSelectedEntity;
            ImGuiAux::StyledPopupVars::Push();
            if(ImGui::BeginPopup("AddEntityContext") || ImGui::BeginPopupContextWindow("HierarchySpace", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
            {
                ImFont* boldFont = ImGui::GetIO().Fonts->Fonts[1];

                if(editor->HasEntityInClipboard() && !editor->IsPlaying())
                {
                    if(ImGuiAux::StyledMenuItem("Paste", "Ctrl+V"))
                        Core::AddFrameEndCallback([editor]() { editor->PasteEntity(); });
                    ImGuiAux::StyledSeparator();
                }

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
                if(ImGuiAux::StyledMenuItem("Audio Source"))
                {
                    mSceneContext->CreateEntity(mSelectedEntity, "AudioSource");
                    mSelectedEntity.AddComponent<AudioSourceComponent>();
                }
                ImGuiAux::EndStyledPopup();
            }
            else
                ImGuiAux::StyledPopupVars::Pop();

            if(mSelectedEntity && mSelectedEntity != selectionBeforeMenu)
                editor->OnEntityCreated(mSelectedEntity);

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
                        editor->ReparentEntity(dropped, Entity {});
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
            opened = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uint64_t>(static_cast<Uint>(e.Raw()))), flags, "%s", name.c_str());

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
                // Cyclic parenting protection happens in there
                Entity dropped(*(const entt::entity*)payload->Data, mSceneContext);
                static_cast<Editor*>(Core::GetClient())->ReparentEntity(dropped, e);
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

            Editor* editor = static_cast<Editor*>(Core::GetClient());
            if(rel.Parent != entt::null)
            {
                if(ImGuiAux::StyledMenuItem("Unparent"))
                    editor->ReparentEntity(e, Entity {});
                ImGuiAux::StyledSeparator();
            }

            // Creating entities while the registry is being iterated is not safe, these run at the end of the frame
            if(ImGuiAux::StyledMenuItem("Copy", "Ctrl+C"))
                editor->CopyEntity(e);
            if(ImGuiAux::StyledMenuItem("Paste", "Ctrl+V", nullptr, editor->HasEntityInClipboard() && !editor->IsPlaying()))
            {
                Entity target = e;
                Core::AddFrameEndCallback([editor, target]() {
                    editor->SelectEntity(target); // Pastes right after the clicked entity
                    editor->PasteEntity();
                });
            }
            if(ImGuiAux::StyledMenuItem("Duplicate", "Ctrl+D"))
                Core::AddFrameEndCallback([editor, e]() { editor->DuplicateEntity(e); });
            if(ImGuiAux::StyledMenuItem("Delete", "Del"))
                editor->DeleteEntity(e); // Only queues the destruction
            ImGuiAux::EndStyledPopup();
        }

        ImGui::TableNextColumn();

        String typeTag = "Entity";
        if(e.HasComponent<CameraComponent>())              typeTag = "CAMERA";
        else if(e.HasComponent<AudioSourceComponent>())    typeTag = "AUDIO";
        else if(e.HasComponent<AudioListenerComponent>())  typeTag = "AUDIO";
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