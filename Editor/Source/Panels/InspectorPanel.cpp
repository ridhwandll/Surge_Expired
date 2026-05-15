// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/InspectorPanel.hpp"
#include "Surge/ECS/Components.hpp"
#include "Utility/ImGuiAux.hpp"
#include "Surge/Utility/FileDialogs.hpp"
#include "Surge/Core/Core.hpp"
#include <imgui.h>
#include <imgui_internal.h>
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
            if (ImGui::BeginTable("##ComponentTable", 2, ImGuiTableFlags_Resizable))
            {
				if (isRemoveable)
				{
                    ImGui::TableNextColumn();
					ImGui::TextUnformatted("Settings");
                    ImGui::TableNextColumn();
					if (ImGui::Button(reinterpret_cast<const char*>("REMOVE")))
						remvove = true;
				}
                function();
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
        if (remvove)
            Surge::Core::AddFrameEndCallback([entity]() mutable { entity.RemoveComponent<XComponent>(); });

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
					if (ImGui::MenuItem("Sprite Renderer"))
						entity.AddComponent<SpriteRendererComponent>(ImGuiAux::Colors::ThemeColor);
					if (ImGui::MenuItem("Mesh Component"))
						entity.AddComponent<MeshComponent>();
					if (ImGui::MenuItem("Light"))
						entity.AddComponent<LightComponent>();
                    ImGui::EndPopup();
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
            //ImGui::InputText("##n@Me", &component.Name);
            ImGui::PopItemWidth();
        }

        if (entity.HasComponent<TransformComponent>())
        {
            TransformComponent& component = entity.GetComponent<TransformComponent>();
            DrawComponent<TransformComponent>(
                entity, "Transform", [&component]() {
                    if (ImGuiAux::TProperty<glm::vec3>("Position", &component.Position));
                        component.MarkDirty();
                    if (ImGuiAux::TProperty<glm::vec3>("Rotation", &component.Rotation))
                        component.MarkDirty();
                    if (ImGuiAux::TProperty<glm::vec3>("Scale", &component.Scale))
                        component.MarkDirty();
                },
                false);
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

		if (entity.HasComponent<SpriteRendererComponent>())
		{
            SpriteRendererComponent& component = entity.GetComponent<SpriteRendererComponent>();
			DrawComponent<SpriteRendererComponent>(entity, "Sprite Renderer", [&component]() {
				ImGuiAux::TProperty<glm::vec4, ImGuiAux::CustomProprtyFlag::Color4>("Color", &component.Color);
				});
		}

		if (entity.HasComponent<MeshComponent>())
		{
            MeshComponent& component = entity.GetComponent<MeshComponent>();
			DrawComponent<MeshComponent>(entity, "Mesh Component", [&component]() {
                String kek = "TODO: Implement Asset Manager";
				ImGuiAux::TProperty<String>("AssetHandle: ", &kek);
				});
		}

        if (entity.HasComponent<LightComponent>())
        {
            LightComponent& component = entity.GetComponent<LightComponent>();
            DrawComponent<LightComponent>(entity, "Light", [&component]()
            {
				const char* lightTypeStrings[] = { "POINT", "DIRECTIONAL" };
				const char* currentLightTypeString = lightTypeStrings[static_cast<int>(component.Type)];
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("TYPE");
				ImGui::TableNextColumn();
				ImGui::PushItemWidth(-1);
				if (ImGui::BeginCombo("##TYPE", currentLightTypeString))
				{
					for (int i = 0; i < 2; i++)
					{
						const bool isSelected = currentLightTypeString == lightTypeStrings[i];
						if (ImGui::Selectable(lightTypeStrings[i], isSelected))
						{
							currentLightTypeString = lightTypeStrings[i];
							component.Type = static_cast<LightType>(i);
						}
						if (isSelected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				ImGui::PopItemWidth();

                ImGuiAux::TProperty<glm::vec3, ImGuiAux::CustomProprtyFlag::Color3>("Color", &component.Color);
                ImGuiAux::TProperty<float>("Intensity", &component.Intensity);

				if (component.Type == LightType::POINT)
                {
                    ImGuiAux::TProperty<float>("Radius", &component.Radius);
                    ImGuiAux::TSlider<float>("Falloff", component.Falloff, 0.1f, 2.0f);
                }
            });
        }
    }

} // namespace Surge
