// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Core.hpp"
#include "Panels/RenderProcedurePanel.hpp"
#include "Surge/Graphics/RenderProcedure/GeometryProcedure.hpp"
#include "Surge/Graphics/RenderProcedure/ShadowMapProcedure.hpp"
#include "Surge/Graphics/RenderProcedure/PreDepthProcedure.hpp"
#include "Surge/Graphics/RenderProcedure/LightCullingProcedure.hpp"
#include "Utility/ImGuiAux.hpp"

namespace Surge
{
    template <typename T, typename F>
    static void PropertyRenderProcedure(const char* name, F uiFunction)
    {
        if (ImGuiAux::PropertyGridHeader(name, false))
        {
            if (ImGui::BeginTable("Props", 2, ImGuiTableFlags_Resizable))
            {
                RenderProcedureManager* renderProcManager = Core::GetRenderer()->GetRenderProcManager();
                T* proc = renderProcManager->GetProcedure<T>();
                typename T::InternalData* internaldata = renderProcManager->GetRenderProcData<T>();

                bool isProcActive = renderProcManager->IsProcecureActive<T>();
                if (ImGuiAux::TProperty<bool>("Active", &isProcActive))
                    renderProcManager->SetProcecureActive<T>(isProcActive);
                if (isProcActive)
                {
                    if (ImGuiAux::TButton("RestartProcedure", "Restart"))
                        renderProcManager->RestartProcedure<T>();
                }

                if (isProcActive)
                    uiFunction(proc, internaldata);

                ImGui::EndTable();
            }

            ImGui::TreePop();
        }
    }

    void RenderProcedurePanel::Init(void* panelInitArgs)
    {
        mCode = GetStaticCode();
    }

    void RenderProcedurePanel::Render(bool* show)
    {
        if (!*show)
            return;

        if (ImGui::Begin(PanelCodeToString(mCode), show))
        {
            PropertyRenderProcedure<PreDepthProcedure>("Pre Depth Procedure", [](PreDepthProcedure* proc, PreDepthProcedure::InternalData* internalData) {
            });

            PropertyRenderProcedure<LightCullingProcedure>("Light Culling Procedure", [](LightCullingProcedure* proc, LightCullingProcedure::InternalData* internalData) {
                ImGuiAux::TProperty<bool>("Show Light Complexity", &internalData->ShowLightComplexity);
            });

            PropertyRenderProcedure<ShadowMapProcedure>("Shadow Map Procedure", [](ShadowMapProcedure* proc, ShadowMapProcedure::InternalData* internalData) {
                static Uint selectedCascadeIndex = 0;

                const char* cascadeCountStrings[] = {"2", "3", "4"};
                ImGuiAux::TComboBox("Cascade Count", cascadeCountStrings, 3, static_cast<int>(proc->GetCascadeCount()) - 2, [&](int i) {
                    proc->SetCascadeCount(static_cast<CascadeCount>(i + 2));
                });

                const char* shadowQualityStrings[] = {"Low", "Medium", "Ultra", "Epic"};
                ImGuiAux::TComboBox("Shadow Quality", shadowQualityStrings, 4, static_cast<int>(proc->GetShadowQuality()), [&](int i) {
                    proc->SetShadowQuality(static_cast<ShadowQuality>(i));
                });

                ImGuiAux::TProperty<bool>("Visualize Cascades", &internalData->VisualizeCascades);
                ImGuiAux::TProperty<float>("Cascade Split Lambda", &internalData->CascadeSplitLambda);
                int shadowMapResolution = static_cast<int>(proc->GetShadowMapsResolution());
                if (ImGuiAux::TProperty<int>("Shadow Map Resolution", &shadowMapResolution, 1024, 8192))
                    proc->SetShadowMapsResolution(shadowMapResolution);
            });

            PropertyRenderProcedure<GeometryProcedure>("Geometry Procedure", [](GeometryProcedure* proc, GeometryProcedure::InternalData* internalData) {
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Total PointLight Count");
                ImGui::TableNextColumn();
                ImGui::Text("%i", Core::GetRenderer()->GetData()->LightData.PointLightCount);
            });
        }
        ImGui::End();
    }

    void RenderProcedurePanel::Shutdown()
    {
    }

} // namespace Surge
