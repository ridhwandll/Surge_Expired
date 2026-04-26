// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/InspectorPanel.hpp"
#include "Surge/ECS/Components.hpp"
#include "Utility/ImGuiAux.hpp"
#include "Surge/Utility/FileDialogs.hpp"
#include "Surge/Core/Core.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <IconsFontAwesome.hpp>
#include <filesystem>

namespace Surge
{
    template <typename XComponent, typename Func>
    static void DrawComponent(Entity& entity, const String& name, Func&& function, bool isRemoveable = true)
    {
        const int64_t& hash = SurgeReflect::GetReflection<XComponent>()->GetHash();
        ImGui::PushID(static_cast<int>(hash));

        bool open = ImGuiAux::PropertyGridHeader(name.c_str());

        bool remvove = false;
        if (open)
        {
            const ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();
            const float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y;

            if (isRemoveable)
            {
                ImGui::SameLine();
                ImGui::SetCursorPosX(contentRegionAvailable.x + 13.0f);
                if (ImGui::Button(reinterpret_cast<const char*>(ICON_SURGE_TRASH_O)))
                    remvove = true;
            }
            if (ImGui::BeginTable("##ComponentTable", 2, ImGuiTableFlags_Resizable))
            {
                function();
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
        if (remvove)
            Surge::Core::AddFrameEndCallback([entity]() mutable { entity.RemoveComponent<XComponent>(); });

        ImGui::PopID();
    }

    static void DrawMatTexControl(const char* mapName, Ref<Material>& material)
    {
        ImGui::PushID(mapName);
        Ref<Texture2D>& texture = material->Get<Ref<Texture2D>>(mapName);
        if (ImGuiAux::TButton(mapName, "Open"))
        {
            String path = FileDialog::OpenFile("");
            if (!path.empty())
            {
                TextureSpecification spec;
                spec.UseMips = true;
                Ref<Texture2D> tex = Texture2D::Create(path, spec);
                material->Set<Ref<Texture2D>>(mapName, tex);
            }
        }

        ImGui::SameLine();
        if (ImGuiAux::Button("Remove"))
            material->RemoveTexture(mapName);

        ImGui::SameLine();
        float fontSize = ImGui::GetIO().FontDefault->FontSize + 6;
        ImGuiAux::Image(texture->GetImage2D(), {fontSize, fontSize});
        ImGui::PopID();
    }

    void InspectorPanel::Init(void* panelInitArgs)
    {
        mCode = GetStaticCode();
        mHierarchy = nullptr;
    }

    void InspectorPanel::Render(bool* show)
    {
        if (!*show)
            return;

        if (ImGui::Begin(PanelCodeToString(mCode), show))
        {
            Entity& entity = mHierarchy->GetSelectedEntity();
            if (entity)
            {
                DrawComponents(entity);

                if (ImGui::Button("Add Component", {ImGui::GetWindowWidth() - 15, 0.0f}))
                    ImGui::OpenPopup("AddComponentPopup");

                if (ImGui::BeginPopup("AddComponentPopup"))
                {
                    if (ImGui::MenuItem("Camera"))
                        entity.AddComponent<CameraComponent>();
                    if (ImGui::MenuItem("Mesh"))
                        entity.AddComponent<MeshComponent>();
                    if (ImGui::MenuItem("Point Light"))
                        entity.AddComponent<PointLightComponent>();
                    if (ImGui::MenuItem("Directional Light"))
                        entity.AddComponent<DirectionalLightComponent>();
                    ImGui::EndPopup();
                }
            }
        }
        ImGui::End();

        // Material Panel; TODO: Merge with inspector later
        ImGui::Begin("Material Editor");
        Entity selectedEntity = mHierarchy->GetSelectedEntity();
        if (selectedEntity && selectedEntity.HasComponent<MeshComponent>())
        {
            static Uint selectedMatIndex = 0;
            Ref<Mesh>& mesh = selectedEntity.GetComponent<MeshComponent>().Mesh;
            if (mesh)
            {
                Vector<Ref<Material>>& materials = mesh->GetMaterials();
                if (ImGui::BeginTable("MatTable", 1))
                {
                    for (Uint i = 0; i < materials.size(); i++)
                    {
                        ImGuiTreeNodeFlags flags = ((i == selectedMatIndex) ? ImGuiTreeNodeFlags_Selected : 0);
                        flags |= ImGuiTreeNodeFlags_SpanFullWidth;

                        // TODO: remove std::to_string hack
                        bool open = ImGuiAux::TSelectable(fmt::format("{0} ({1})", materials[i]->GetName(), std::to_string(glm::abs(*(int*)&materials[i]))).c_str());

                        if (selectedMatIndex == i)
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32({0.1f, 0.1f, 0.1f, 1.0f}));

                        if (open)
                            selectedMatIndex = i;
                    }
                    ImGui::EndTable();
                }
                if (materials.size() <= selectedMatIndex)
                    selectedMatIndex = 0;

                Ref<Material>& material = materials[selectedMatIndex];
                if (ImGui::BeginTable("MatEditTable", 2, ImGuiTableFlags_Resizable) && material)
                {
                    ImGuiAux::TProperty<glm::vec3, ImGuiAux::CustomProprtyFlag::Color3>("Albedo", &material->Get<glm::vec3>("Material.Albedo"));
                    ImGuiAux::TProperty<float>("Metalness", &material->Get<float>("Material.Metalness"), 0.0f, 1.0f);
                    ImGuiAux::TProperty<float>("Roughness", &material->Get<float>("Material.Roughness"), 0.0f, 1.0f);
                    ImGuiAux::TProperty<bool>("UseNormalMap", &material->Get<bool>("Material.UseNormalMap"));
                    ImGui::Separator();
                    DrawMatTexControl("AlbedoMap", material);
                    DrawMatTexControl("NormalMap", material);
                    DrawMatTexControl("MetalnessMap", material);
                    DrawMatTexControl("RoughnessMap", material);
                    ImGui::EndTable();
                }
            }
        }

        ImGui::End();
    }

    void InspectorPanel::DrawComponents(Entity& entity)
    {
        if (entity.HasComponent<NameComponent>())
        {
            NameComponent& component = entity.GetComponent<NameComponent>();
            ImGui::PushItemWidth(-1);
            ImGui::InputText("##n@Me", &component.Name);
            ImGui::PopItemWidth();
        }

        if (entity.HasComponent<TransformComponent>())
        {
            TransformComponent& component = entity.GetComponent<TransformComponent>();
            DrawComponent<TransformComponent>(
                entity, "Transform", [&component]() {
                    ImGuiAux::TProperty<glm::vec3>("Position", &component.Position);
                    ImGuiAux::TProperty<glm::vec3>("Rotation", &component.Rotation);
                    ImGuiAux::TProperty<glm::vec3>("Scale", &component.Scale);
                },
                false);
        }

        if (entity.HasComponent<MeshComponent>())
        {
            MeshComponent& component = entity.GetComponent<MeshComponent>();
            DrawComponent<MeshComponent>(entity, "Mesh", [&component]() {
                const String meshPath = component.Mesh ? component.Mesh->GetPath().Str() : "";
                if (ImGuiAux::TButton("Path", meshPath.empty() ? "Open..." : meshPath.c_str()))
                {
                    String path = FileDialog::OpenFile("");
                    if (!path.empty())
                        component.Mesh = Ref<Mesh>::Create(path);
                }
            });
        }

        if (entity.HasComponent<CameraComponent>())
        {
            CameraComponent& component = entity.GetComponent<CameraComponent>();
            DrawComponent<CameraComponent>(entity, "Camera", [&component]() {
                RuntimeCamera& camera = component.Camera;
                ImGuiAux::TProperty<bool>("Primary", &component.Primary);

                const char* projectionTypeStrings[] = {"Perspective", "Orthographic"};
                const char* currentProjectionTypeString = projectionTypeStrings[static_cast<int>(camera.GetProjectionType())];

                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Projection");
                ImGui::TableNextColumn();
                ImGui::PushItemWidth(-1);
                if (ImGui::BeginCombo("##Projection", currentProjectionTypeString))
                {
                    for (int i = 0; i < 2; i++)
                    {
                        const bool isSelected = currentProjectionTypeString == projectionTypeStrings[i];
                        if (ImGui::Selectable(projectionTypeStrings[i], isSelected))
                        {
                            currentProjectionTypeString = projectionTypeStrings[i];
                            camera.SetProjectionType(static_cast<RuntimeCamera::ProjectionType>(i));
                        }
                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();

                if (camera.GetProjectionType() == RuntimeCamera::ProjectionType::Perspective)
                {
                    float verticalFOV = camera.GetPerspectiveVerticalFOV();
                    if (ImGuiAux::TProperty<float>("Vertical FOV", &verticalFOV)) // In degree
                        camera.SetPerspectiveVerticalFOV(verticalFOV);

                    float nearClip = camera.GetPerspectiveNearClip();
                    if (ImGuiAux::TProperty<float>("Near Clip", &nearClip))
                        camera.SetPerspectiveNearClip(nearClip);

                    float farClip = camera.GetPerspectiveFarClip();
                    if (ImGuiAux::TProperty<float>("Far Clip", &farClip))
                        camera.SetPerspectiveFarClip(farClip);
                }

                if (camera.GetProjectionType() == RuntimeCamera::ProjectionType::Orthographic)
                {
                    float orthoSize = camera.GetOrthographicSize();
                    if (ImGuiAux::TProperty<float>("Size", &orthoSize))
                        camera.SetOrthographicSize(orthoSize);

                    float nearClip = camera.GetOrthographicNearClip();
                    if (ImGuiAux::TProperty<float>("Near Clip", &nearClip))
                        camera.SetOrthographicNearClip(nearClip);

                    float farClip = camera.GetOrthographicFarClip();
                    if (ImGuiAux::TProperty<float>("Far Clip", &farClip))
                        camera.SetOrthographicFarClip(farClip);

                    ImGuiAux::TProperty<bool>("Fixed Aspect Ratio", &component.FixedAspectRatio);
                }
            });
        }

        if (entity.HasComponent<PointLightComponent>())
        {
            PointLightComponent& component = entity.GetComponent<PointLightComponent>();
            DrawComponent<PointLightComponent>(entity, "Point Light", [&component]() {
                ImGuiAux::TProperty<glm::vec3, ImGuiAux::CustomProprtyFlag::Color3>("Color", &component.Color);
                ImGuiAux::TProperty<float>("Intensity", &component.Intensity);
                ImGuiAux::TProperty<float>("Radius", &component.Radius);
                ImGuiAux::TProperty<float>("Falloff", &component.Falloff, 0.0f, 1.0f);
            });
        }

        if (entity.HasComponent<DirectionalLightComponent>())
        {
            DirectionalLightComponent& component = entity.GetComponent<DirectionalLightComponent>();
            DrawComponent<DirectionalLightComponent>(entity, "Directional Light", [&component]() {
                ImGuiAux::TProperty<glm::vec3, ImGuiAux::CustomProprtyFlag::Color3>("Color", &component.Color);
                ImGuiAux::TProperty<float>("Intensity", &component.Intensity);
                ImGuiAux::TProperty<float>("Size", &component.Size);
            });
        }
    }

} // namespace Surge
