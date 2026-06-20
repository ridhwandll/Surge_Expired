// Copyright (c) - SurgeTechnologies - All rights reserved
#include "InspectorPanel.hpp"
#include "Surge/Graphics/HighLevel/Mesh.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/ECS/Components.hpp"
#include "Utility/ImGuiAux.hpp"
#include "Surge/Core/Core.hpp"

#include "Editor.hpp"
#include "MaterialEditorPanel.hpp"
#include "ContentBrowserPanel.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <imgui_internal.h>
#include "Surge/Utility/Filesystem.hpp"

namespace Surge
{
    template <typename XComponent, typename Func>
    static void DrawComponent(Entity& entity, const String& name, Func&& function, bool isRemoveable = true)
    {
        const int64_t& hash = SurgeReflect::GetReflection<XComponent>()->GetHash();
        ImGui::PushID(static_cast<int>(hash));


        bool open = ImGuiAux::PropertyGridHeader(name);

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
                    if (ImGuiAux::Button(reinterpret_cast<const char*>("REMOVE")))
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

    void InspectorPanel::Init([[maybe_unused]] void* panelInitArgs)
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

                if (ImGuiAux::Button("Add Component", {ImGui::GetWindowWidth() - 15, 0.0f}))
                    ImGui::OpenPopup("AddComponentPopup");

                if (ImGui::BeginPopup("AddComponentPopup"))
                {
                    if (ImGui::MenuItem("Camera") && !entity.HasComponent<CameraComponent>())
                        entity.AddComponent<CameraComponent>();
                    if (ImGui::MenuItem("Sprite Renderer") && !entity.HasComponent<SpriteRendererComponent>())
                        entity.AddComponent<SpriteRendererComponent>(ImGuiAux::Colors::ThemeColor1);
                    if(ImGui::MenuItem("Mesh Component") && !entity.HasComponent<MeshComponent>())
                        entity.AddComponent<MeshComponent>();
                    if (ImGui::MenuItem("Light") && !entity.HasComponent<LightComponent>())
                        entity.AddComponent<LightComponent>();
                    if (ImGui::MenuItem("Environment") && !entity.HasComponent<EnvironmentComponent>())
                        entity.AddComponent<EnvironmentComponent>();
                    if(ImGui::BeginMenu("Physics"))
                    {
                        if(ImGui::MenuItem("Rigidbody") && !entity.HasComponent<RigidbodyComponent>())
                            entity.AddComponent<RigidbodyComponent>();
                        if(ImGui::MenuItem("Box Collider") && !entity.HasComponent<BoxColliderComponent>())
                            entity.AddComponent<BoxColliderComponent>();
                        if(ImGui::MenuItem("Sphere Collider") && !entity.HasComponent<SphereColliderComponent>())
                            entity.AddComponent<SphereColliderComponent>();
                        if(ImGui::MenuItem("Capsule Collider") && !entity.HasComponent<CapsuleColliderComponent>())
                            entity.AddComponent<CapsuleColliderComponent>();
                        if(ImGui::MenuItem("Cylinder Collider") && !entity.HasComponent<CylinderColliderComponent>())
                            entity.AddComponent<CylinderColliderComponent>();
                        if(ImGui::MenuItem("Convex Collider") && !entity.HasComponent<ConvexColliderComponent>() && entity.HasComponent<MeshComponent>())
                            entity.AddComponent<ConvexColliderComponent>();
                        if(ImGui::MenuItem("Mesh Collider") && !entity.HasComponent<MeshColliderComponent>())
                            entity.AddComponent<MeshColliderComponent>();

                        ImGui::EndPopup();
                    }
                    if(ImGui::MenuItem("Script") && !entity.HasComponent<ScriptComponent>())
                        entity.AddComponent<ScriptComponent>();

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
            ImGui::InputText("##nA@Me", &component.Name);
            ImGui::PopItemWidth();
        }

        if (entity.HasComponent<TransformComponent>())
        {
            TransformComponent& component = entity.GetComponent<TransformComponent>();
            DrawComponent<TransformComponent>(
                entity, "Transform", [&component]() {
                    if (ImGuiAux::TProperty<glm::vec3>("Position", &component.Position))
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

                AssetManager* am = Core::GetAssetManager();

                if (component.MeshID.IsValid())
                    ImGuiAux::TString("Asset Handle: ", "%llu", component.MeshID.Get());
                else
                    ImGui::TextUnformatted("Drop MESH from Content browser");

                if(ImGui::BeginDragDropTarget())
                {
                    if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_PAYLOAD))
                    {
                        SG_ASSERT(payload->DataSize == sizeof(AssetID), "Payload size mismatch!");
                        AssetID droppedAssetID = *(const AssetID*)payload->Data;
                        AssetMetadata meta = am->GetMetadata(droppedAssetID);
                        if(meta.Type == AssetType::MESH)
                            component.MeshID = droppedAssetID;
                    }
                    ImGui::EndDragDropTarget();
                }
                if(component.MeshID.IsValid())
                {
                    ImGuiAux::TProperty<bool>("Drop Shadow", &component.DropShadow);

                    Ref<Mesh> mesh = am->Load<Mesh>(component.MeshID);
                    if(!mesh)
                    {
                        ImGuiAux::ScopedBoldFont font;
                        ImGui::TextColored(ImGuiAux::Colors::Red, "MISSING");
                    }
                    else
                    {
                        for(size_t i = 0; i < mesh->GetMaterials().size(); i++)
                        {
                            ImGui::PushID(i);
                            Ref<Material> material = mesh->GetMaterialAtIndex(i);
                            if(material)
                            {
                                const String& matName = material->GetName();
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                ImGui::Text("Material [ %zu ]", i);

                                ImGui::TableSetColumnIndex(1);
                                ImGui::PushItemWidth(-FLT_MIN);

                                if(ImGui::Button(matName.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                                {
                                    auto* editor = static_cast<Editor*>(Core::GetClient());
                                    editor->GetPanelManager().GetPanel<MaterialEditorPanel>()->SetSelectedMaterial(material);
                                    ImGui::SetWindowFocus("Material Editor");
                                }
                                if(ImGui::BeginDragDropTarget())
                                {
                                    if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_PAYLOAD))
                                    {
                                        SG_ASSERT(payload->DataSize == sizeof(AssetID), "Payload size mismatch!");
                                        AssetID droppedAssetID = *(const AssetID*)payload->Data;
                                        AssetMetadata meta = am->GetMetadata(droppedAssetID);

                                        if(meta.Type == AssetType::MATERIAL)
                                        {
                                            mesh->SetMaterialOverride(i, am->Load<Material>(droppedAssetID));
                                            am->Save(meta.ID);
                                            am->Save(component.MeshID);
                                        }
                                    }
                                    ImGui::EndDragDropTarget();
                                }
                                ImGui::PopItemWidth();
                            }
                            else
                            {
                                ImGuiAux::ScopedBoldFont font;
                                ImGui::TextColored(ImGuiAux::Colors::Red, "MISSING MATERIAL [ %zu ]", i);
                            }
                            ImGui::PopID();
                        }
                    }
                }
                });
        }

        if (entity.HasComponent<LightComponent>())
        {
            LightComponent& component = entity.GetComponent<LightComponent>();
            DrawComponent<LightComponent>(entity, "Light", [&component]()
            {
                const char* lightTypeStrings[] = { "DIRECTIONAL", "POINT" };
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
                    ImGuiAux::TSlider<float>("Falloff", &component.Falloff, 0.1f, 2.0f);
                }
            });
        }
        if (entity.HasComponent<EnvironmentComponent>())
        {
            EnvironmentComponent& component = entity.GetComponent<EnvironmentComponent>();
            DrawComponent<EnvironmentComponent>(entity, "Environment", [&component]()
            {
                ImGuiAux::TSperator("Position");
                ImGuiAux::TProperty<float>("Azimuth (X)", &component.Azimuth);
                ImGuiAux::TProperty<float>("Elevation (Y)", &component.Elevation);
                ImGuiAux::TSperator("Visuals");
                ImGuiAux::TProperty<bool>("Sun Disk", &component.EnableSunDisk);
                ImGuiAux::TSlider<float>("Turbidity", &component.Turbidity, 1.5f, 10.0f, "%.2f");
                ImGuiAux::TSlider<float>("Exposure", &component.Exposure, 0.001, 0.1f, "%.4f");
                ImGuiAux::TSlider<float>("Sun Intensity", &component.SunIntensity, 0.01, 1.0f, "%.2f");
                ImGuiAux::TSperator("Global Illumination");
                ImGuiAux::TProperty<glm::vec3, ImGuiAux::CustomProprtyFlag::Color3>("SkyAmbient", &component.SkyAmbient);
                ImGuiAux::TProperty<glm::vec3, ImGuiAux::CustomProprtyFlag::Color3>("HorizonAmbient", &component.HorizonAmbient);
                ImGuiAux::TProperty<glm::vec3, ImGuiAux::CustomProprtyFlag::Color3>("GroundAmbient", &component.GroundAmbient);

            });
        }
        if(entity.HasComponent<RigidbodyComponent>())
        {
            RigidbodyComponent& component = entity.GetComponent<RigidbodyComponent>();
            DrawComponent<RigidbodyComponent>(entity, "Rigidbody", [&component]()
            {
                const char* rbTypeStrings[] = { "STATIC", "DYNAMIC", "KINEMATIC" };
                const char* currentRbTypeString = rbTypeStrings[static_cast<int>(component.Type)];

                ImGui::TableNextColumn();
                ImGui::TextUnformatted("TYPE");
                ImGui::TableNextColumn();
                ImGui::PushItemWidth(-1);
                if(ImGui::BeginCombo("##RBTYPE", currentRbTypeString))
                {
                    for(int i = 0; i < 3; i++)
                    {
                        const bool isSelected = (component.Type == static_cast<RigidbodyType>(i));
                        if(ImGui::Selectable(rbTypeStrings[i], isSelected))
                        {
                            component.Type = static_cast<RigidbodyType>(i);
                        }
                        if(isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();

                if(component.Type != RigidbodyType::STATIC)
                    ImGuiAux::TProperty<float>("Mass", &component.Mass);

                ImGuiAux::TSperator("Collision Settings");
                ImGuiAux::TProperty<bool>("Use Gravity", &component.UseGravity);
                ImGuiAux::TProperty<bool>("Is Sensor", &component.IsSensor);
                ImGuiAux::TProperty<bool>("Continuous Collision (CCD)", &component.ContinuousCollision);

                ImGuiAux::TSperator("Material Properties");
                ImGuiAux::TProperty<float>("Friction", &component.Friction);
                ImGuiAux::TProperty<float>("Bounciness", &component.Bounciness);

                if(component.Type != RigidbodyType::STATIC)
                {
                    ImGuiAux::TSperator("Damping");
                    ImGuiAux::TProperty<float>("Linear", &component.LinearDamping);
                    ImGuiAux::TProperty<float>("Angular", &component.AngularDamping);

                    ImGuiAux::TSperator("Freeze Rotation Axes");
                    ImGuiAux::TProperty<bool>("X", &component.FreezeRotationX);
                    ImGuiAux::TProperty<bool>("Y", &component.FreezeRotationY);
                    ImGuiAux::TProperty<bool>("Z", &component.FreezeRotationZ);
                }
            });
        }
        if(entity.HasComponent<BoxColliderComponent>())
        {
            BoxColliderComponent& component = entity.GetComponent<BoxColliderComponent>();
            DrawComponent<BoxColliderComponent>(entity, "Box Collider", [&component]()
            {
                ImGuiAux::TProperty<glm::vec3>("Half Extents", &component.HalfExtents);
                ImGuiAux::TProperty<bool>("Show Collider", &component.ShowCollider);
            });
        }
        if(entity.HasComponent<SphereColliderComponent>())
        {
            SphereColliderComponent& component = entity.GetComponent<SphereColliderComponent>();
            DrawComponent<SphereColliderComponent>(entity, "Sphere Collider", [&component]()
            {
                ImGuiAux::TProperty<float>("Radius", &component.Radius);
                ImGuiAux::TProperty<bool>("Show Collider", &component.ShowCollider);
            });
        }
        if(entity.HasComponent<CapsuleColliderComponent>())
        {
            CapsuleColliderComponent& component = entity.GetComponent<CapsuleColliderComponent>();
            DrawComponent<CapsuleColliderComponent>(entity, "Capsule Collider", [&component]()
            {
                ImGuiAux::TProperty<float>("Radius", &component.Radius);
                ImGuiAux::TProperty<float>("Height", &component.Height);
                ImGuiAux::TProperty<bool>("Show Collider", &component.ShowCollider);
            });
        }
        if(entity.HasComponent<CylinderColliderComponent>())
        {
            CylinderColliderComponent& component = entity.GetComponent<CylinderColliderComponent>();
            DrawComponent<CylinderColliderComponent>(entity, "Cylinder Collider", [&component]()
            {
                ImGuiAux::TProperty<float>("Radius", &component.Radius);
                ImGuiAux::TProperty<float>("Height", &component.Height);
                ImGuiAux::TProperty<bool>("Show Collider", &component.ShowCollider);
            });
        }
        if(entity.HasComponent<ConvexColliderComponent>())
        {
            ConvexColliderComponent& component = entity.GetComponent<ConvexColliderComponent>();
            DrawComponent<ConvexColliderComponent>(entity, "Convex Collider", [&component, &entity]()
            {
                if(entity.HasComponent<MeshComponent>())
                {
                    if (ImGuiAux::TProperty<glm::vec3>("Local Offset", &component.LocalOffset))
                        component.IsDirty = true;
                    if (ImGuiAux::TProperty<glm::vec3>("Local Rotation", &component.LocalRotation))
                        component.IsDirty = true;
                }
                ImGuiAux::TProperty<bool>("Show Collider", &component.ShowCollider);
            });
        }
        if(entity.HasComponent<MeshColliderComponent>())
        {
            MeshColliderComponent& component = entity.GetComponent<MeshColliderComponent>();
            DrawComponent<MeshColliderComponent>(entity, "Mesh Collider", [&component, &entity]()
            {
                if(entity.HasComponent<RigidbodyComponent>() && entity.GetComponent<RigidbodyComponent>().Type != RigidbodyType::DYNAMIC)
                {
                    if(entity.HasComponent<MeshComponent>())
                    {
                        ImGuiAux::TProperty<glm::vec3>("Local Offset", &component.LocalOffset);
                        ImGuiAux::TProperty<glm::vec3>("Local Rotation", &component.LocalRotation);
                    }
                    ImGuiAux::TSperator("Note");
                    ImGuiAux::TString("Show Collider", "Mesh Colliders are generated properly from the associated mesh, trust the Engine and pray it works, no visualization!");
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGuiAux::Colors::Red);
                    ImGuiAux::TString("ERROR", "Mesh Collider Components can/should NOT be used with DYNAMIC Rigidbodies! REMOVE this component or set the Rigidbody Type to STATIC or KINEMATIC!");
                    ImGui::PopStyleColor();
                }
            });
        }
        if(entity.HasComponent<ScriptComponent>())
        {
            ScriptComponent& component = entity.GetComponent<ScriptComponent>();
            DrawComponent<ScriptComponent>(entity, "Script Component", [&component]()
            {
                AssetManager* am = Core::GetAssetManager();

                String buttonText;
                bool hasScript = component.ScriptAsset.IsValid();
                if(hasScript)
                {
                    const String& scriptPath = am->GetMetadata(component.ScriptAsset).RelativePath;
                    buttonText = Filesystem::GetFilenameWithExt(scriptPath).c_str();
                }
                else
                    buttonText = "Drop SCRIPT from Content browser";

                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Script");
                ImGui::TableNextColumn();
                if(ImGuiAux::Button(buttonText.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0)) && hasScript)
                {
                    Editor* editor = static_cast<Editor*>(Core::GetClient());
                    editor->GetPanelManager().GetPanel<ContentBrowserPanel>()->SetSelectedAsset(component.ScriptAsset);
                }

                if(ImGui::BeginDragDropTarget())
                {
                    if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_PAYLOAD))
                    {
                        SG_ASSERT(payload->DataSize == sizeof(AssetID), "Payload size mismatch!");
                        AssetID droppedAssetID = *(const AssetID*)payload->Data;
                        AssetMetadata meta = am->GetMetadata(droppedAssetID);
                        if(meta.Type == AssetType::SCRIPT)
                            component.ScriptAsset = droppedAssetID;
                    }
                    ImGui::EndDragDropTarget();
                }
            });
        }
    }

} // namespace Surge
