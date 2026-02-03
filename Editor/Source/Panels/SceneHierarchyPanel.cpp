// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/SceneHierarchyPanel.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/ECS/Components.hpp"
#include "Surge/Core/Input/Input.hpp"
#include <imgui.h>
#include <imgui_internal.h>

namespace Surge
{
    void SceneHierarchyPanel::Init(void* panelInitArgs)
    {
        mCode = GetStaticCode();
        mSceneContext = nullptr;
        mSelectedEntity = {};
    }

    void SceneHierarchyPanel::Render(bool* show)
    {
        if (!*show || !mSceneContext)
            return;

        // Draw the entities
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
        if (ImGui::Begin(PanelCodeToString(mCode), show))
        {
            if (ImGui::Button("Add Entity", {ImGui::GetWindowWidth() - 15, 0.0f}))
                ImGui::OpenPopup("Add Entity");

            if (ImGui::BeginPopup("Add Entity") || (ImGui::BeginPopupContextWindow(nullptr, 1, false)))
            {
                if (ImGui::MenuItem("Empty Entity"))
                {
                    mSceneContext->CreateEntity(mSelectedEntity, "Entity");
                    mRenamingMech.SetRenamingState(true);
                }
                if (ImGui::MenuItem("Mesh"))
                {
                    mSceneContext->CreateEntity(mSelectedEntity, "Mesh");
                    mSelectedEntity.AddComponent<MeshComponent>();
                    mRenamingMech.SetRenamingState(true);
                }
                if (ImGui::MenuItem("Camera"))
                {
                    mSceneContext->CreateEntity(mSelectedEntity, "Camera");
                    mSelectedEntity.AddComponent<CameraComponent>();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Point Light"))
                {
                    mSceneContext->CreateEntity(mSelectedEntity, "Point Light");
                    mSelectedEntity.AddComponent<PointLightComponent>();
                }
                if (ImGui::MenuItem("Directional Light"))
                {
                    mSceneContext->CreateEntity(mSelectedEntity, "Directional Light");
                    mSelectedEntity.AddComponent<DirectionalLightComponent>();
                }
                ImGui::EndPopup();
            }

            constexpr ImGuiTableFlags flags = ImGuiTableFlags_Resizable;
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {0.0f, 2.8f});
            if (ImGui::BeginTable("HierarchyTable", 2, flags))
            {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("A").x * 12.0f);
                ImGui::TableHeadersRow();

                Uint idCounter = 0;
                mSceneContext->GetRegistry().each([&](entt::entity e) {
                    ImGui::PushID(idCounter);
                    Entity ent = Entity(e, mSceneContext);
                    ImGui::TableNextColumn();
                    DrawEntityNode(ent);
                    ImGui::PopID();
                    idCounter++;
                });
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
        ImGuiTreeNodeFlags flags = ((mSelectedEntity == e) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;

        bool isSelectedEntity = false;
        if (mSelectedEntity == e)
        {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.123f, 0.123f, 0.123f, 1.0f));
            isSelectedEntity = true;
        }

        bool opened = false;
        {
            ImGuiAux::ScopedColor style({ImGuiCol_Header, ImGuiCol_HeaderHovered, ImGuiCol_HeaderActive}, {0.0, 0.0, 0.0, 0.0});
            opened = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uint64_t>(static_cast<Uint>(e.Raw()))), flags, name.c_str());
        }

        if (ImGui::IsItemClicked() && !mRenamingMech)
            mSelectedEntity = e;

        if (isSelectedEntity)
        {
            if (!mRenamingMech && mSelectedEntity)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiAux::Colors::ExtraDark));

            mRenamingMech.Update(name);
            ImGui::PopStyleColor();
        }

        if (ImGui::BeginPopupContextItem() && !mRenamingMech)
        {
            if (ImGui::MenuItem("Delete"))
            {
                if (mSelectedEntity == e)
                    mSelectedEntity = {};

                // Only execute when the frame ends, else it will give crash on "Entity not found"
                Surge::Core::AddFrameEndCallback([=]() { mSceneContext->DestroyEntity(e); });
            }
            ImGui::EndPopup();
        }

        if (opened)
            ImGui::TreePop();

        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Entity");
    }

    void SceneHierarchyPanel::Shutdown()
    {
    }

} // namespace Surge
