// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/PerformancePanel.hpp"
#include "Surge/Core/Core.hpp"
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
            ImGui::Text("Frame Time: % .2f ms ", Core::GetClock().GetMilliseconds());
            ImGui::Text("FPS: % .2f", ImGui::GetIO().Framerate);

            if (ImGuiAux::PropertyGridHeader("Shaders", false))
            {
                // Show the PBR shader reload button
                Ref<Shader>& pbrShader = Core::GetRenderer()->GetShader("PBR");
                if (pbrShader)
                {
                    ImGui::PushID(pbrShader->GetPath());
                    {
                        ImGuiAux::ScopedBoldFont boldFont;
                        ImGui::Text("%s", Filesystem::GetNameWithExtension(pbrShader->GetPath()).c_str());
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Reload"))
                        pbrShader->Reload();
                    ImGui::PopID();
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