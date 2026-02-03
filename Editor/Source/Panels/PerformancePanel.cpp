// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/PerformancePanel.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Graphics/RenderContext.hpp"
#include "Surge/Graphics/RenderProcedure/ShadowMapProcedure.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Editor.hpp"
#include "SceneHierarchyPanel.hpp"
#include "Surge/ECS/Components.hpp"
#include "Utility/ImGuiAux.hpp"
#include <imgui.h>
#include <filesystem>

namespace Surge
{
    void PerformancePanel::Init(void* panelInitArgs)
    {
        mCode = GetStaticCode();
    }

    void PerformancePanel::Render(bool* show)
    {
        if (!*show)
            return;

        if (ImGui::Begin(PanelCodeToString(mCode), show))
        {
            RenderContext* renderContext = Core::GetRenderContext();
            const float headerSpacingOffset = -(ImGui::GetStyle().ItemSpacing.y + 1.0f);

            ImGui::Text("Device: %s", renderContext->GetGPUInfo().Name.c_str());
            ImGui::Text("Frame Time: % .2f ms ", Core::GetClock().GetMilliseconds());
            ImGui::Text("FPS: % .2f", ImGui::GetIO().Framerate);

            if (ImGuiAux::PropertyGridHeader("GPU Memory Status", false))
            {
                Surge::GPUMemoryStats memoryStatus = renderContext->GetMemoryStatus();
                float used = memoryStatus.Used / 1000000.0f;
                float free = memoryStatus.Free / 1000000.0f;
                ImGui::Text("Used: %f Mb", used);
                ImGui::Text("Local-Free: %f Mb", free);
                ImGui::Text("Total Allocated: %f Mb", used + free);
                ImGui::TreePop();
            }

            if (ImGuiAux::PropertyGridHeader("Shaders", false))
            {
                Vector<Ref<Shader>>& allAhaders = Core::GetRenderer()->GetData()->ShaderSet.GetAllShaders();
                if (ImGui::BeginTable("ShaderTable", 3, ImGuiTableFlags_Resizable))
                {
                    ImGui::TableSetupColumn("Name");
                    ImGui::TableSetupColumn("Type");
                    ImGui::TableSetupColumn("Action");
                    ImGui::TableHeadersRow();

                    for (Ref<Shader>& shader : allAhaders)
                    {
                        ImGui::PushID(shader->GetPath());
                        String typeString;
                        ShaderType types = shader->GetTypes();
                        if (ShaderType::Vertex & types && ShaderType::Pixel & types)
                            typeString.append("Vertex & Pixel");
                        else if (ShaderType::Compute & types)
                            typeString.append("Compute");

                        {
                            ImGuiAux::ScopedBoldFont bondFont;
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(Filesystem::GetNameWithExtension(shader->GetPath()).c_str());
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(typeString.c_str());
                        }
                        ImGui::TableNextColumn();
                        {
                            auto& style = ImGui::GetStyle();
                            ImVec4 buttonCol = style.Colors[ImGuiCol_Button];
                            ImGuiAux::ScopedColor color({ImGuiCol_ButtonHovered}, buttonCol);
                            if (ImGui::Button("Reload"))
                                shader->Reload();
                            if (ImGui::IsItemHovered() || ImGui::IsItemActive())
                                ImGuiAux::DrawRectAroundWidget({1.0f, 0.5f, 0.1f, 1.0f}, 1.5f, 1.0f);
                        }

                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
                ImGui::TreePop();
            }

#ifdef SURGE_DEBUG
            Editor* editor = static_cast<Editor*>(Core::GetClient());
            if (ImGuiAux::PropertyGridHeader("All Entities (Debug Only)", false))
            {
                SceneHierarchyPanel* hierarchy = editor->GetPanelManager().GetPanel<SceneHierarchyPanel>();
                Scene* scene = hierarchy->GetSceneContext();
                scene->GetRegistry().each([&scene](entt::entity e) {
                    Entity ent = Entity(e, scene);
                    ImGui::Text("%i -", e);
                    ImGui::SameLine();
                    ImGui::Text(ent.GetComponent<NameComponent>().Name.c_str());
                });
                ImGui::TreePop();
            }
#endif
        }
        ImGui::End();
    }

    void PerformancePanel::Shutdown()
    {
    }
} // namespace Surge