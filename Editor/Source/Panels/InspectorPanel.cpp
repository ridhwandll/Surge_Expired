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
    static bool DrawVec3Control(const String& label, glm::vec3& values, float resetValue = 0.0f)
    {
        bool modified = false;
        ImGui::PushID(label.c_str());

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(label.c_str());
        ImGui::TableNextColumn();

        ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2 { 3.0f, 0.0f });

        float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
        ImVec2 buttonSize = { lineHeight, lineHeight };

        auto drawAxis = [&](const char* id, const char* btnLabel, float& val, ImVec4 textColor) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // Transparent
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.1f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.2f));
            ImGui::PushStyleColor(ImGuiCol_Text, textColor);

            if(ImGui::Button(btnLabel, buttonSize))
            {
                val = resetValue;
                modified = true;
            }
            ImGui::PopStyleColor(4);

            ImGui::SameLine(0, 0);
            if(ImGui::DragFloat(id, &val, 0.1f, 0.0f, 0.0f, "%.2f"))
                modified = true;
            ImGui::PopItemWidth();
        };

        drawAxis("##X", "X", values.x, ImVec4 { 0.85f, 0.35f, 0.35f, 1.0f });
        ImGui::SameLine();
        drawAxis("##Y", "Y", values.y, ImVec4 { 0.35f, 0.85f, 0.35f, 1.0f });
        ImGui::SameLine();
        drawAxis("##Z", "Z", values.z, ImVec4 { 0.35f, 0.55f, 0.85f, 1.0f });

        ImGui::PopStyleVar();
        ImGui::PopID();
        return modified;
    }

    template <typename XComponent, typename Func>
    static void DrawComponent(Entity& entity, const String& name, Func&& function, bool isRemoveable = true)
    {
        const int64_t& hash = SurgeReflect::GetReflection<XComponent>()->GetHash();
        ImGui::PushID(static_cast<int>(hash));

        ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2 { 4, 4 });
        float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;

        ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_FramePadding;

        bool open = ImGui::TreeNodeEx((void*)hash, treeNodeFlags, "%s", name.c_str());
        ImGui::PopStyleVar();

        bool removeComponent = false;
        if(isRemoveable)
        {
            // Right align the options gear/button
            ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
            if(ImGui::Button(".../", ImVec2 { lineHeight, lineHeight }))
                ImGui::OpenPopup("ComponentSettings");

            if(ImGui::BeginPopup("ComponentSettings"))
            {
                if(ImGui::MenuItem("Remove Component"))
                    removeComponent = true;
                ImGui::EndPopup();
            }
        }

        if(open)
        {
            ImGui::Dummy(ImVec2(0.0f, 2.0f));

            // SizingStretchProp keeps property columns consistently sized
            if(ImGui::BeginTable("##ComponentTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                function();

                ImGui::EndTable();
            }
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImGui::TreePop();
        }

        if(removeComponent)
            Surge::Core::AddFrameEndCallback([entity]() mutable { entity.RemoveComponent<XComponent>(); });

        ImGui::PopID();
    }

    void InspectorPanel::Init(void*)
    {
        mCode = GetStaticCode();
        mHierarchy = nullptr;
    }

    void InspectorPanel::Render(bool* show)
    {
        if(!*show)
            return;

        if(ImGui::Begin(PanelCodeToString(mCode), show))
        {
            Entity& entity = mHierarchy->GetSelectedEntity();
            if(entity)
            {
                DrawComponents(entity);

                ImGui::Dummy(ImVec2(0.0f, 10.0f));

                // Add Component button
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 6));
                float availWidth = ImGui::GetContentRegionAvail().x;
                float buttonWidth = availWidth > 200.0f ? 200.0f : availWidth;
                ImGui::SetCursorPosX((ImGui::GetWindowWidth() - buttonWidth) * 0.5f);

                if(ImGui::Button("Add Component", ImVec2(buttonWidth, 0.0f)))
                    ImGui::OpenPopup("AddComponentPopup");
                ImGui::PopStyleVar();

                if(ImGui::BeginPopup("AddComponentPopup"))
                {
                    if(ImGui::MenuItem("Camera Component") && !entity.HasComponent<CameraComponent>())
                        entity.AddComponent<CameraComponent>();
                    if(ImGui::MenuItem("Sprite Renderer  Component") && !entity.HasComponent<SpriteRendererComponent>())
                        entity.AddComponent<SpriteRendererComponent>(ImGuiAux::Colors::ThemeColor1);
                    if(ImGui::MenuItem("Mesh Component") && !entity.HasComponent<MeshComponent>())
                        entity.AddComponent<MeshComponent>();
                    if(ImGui::MenuItem("Light Components") && !entity.HasComponent<LightComponent>())
                        entity.AddComponent<LightComponent>();
                    if(ImGui::MenuItem("Environment Component") && !entity.HasComponent<EnvironmentComponent>())
                        entity.AddComponent<EnvironmentComponent>();
                    if(ImGui::MenuItem("Text Component") && !entity.HasComponent<TextComponent>())
                        entity.AddComponent<TextComponent>();

                    if(ImGui::BeginMenu("Physics"))
                    {
                        if(ImGui::MenuItem("Rigidbody Component") && !entity.HasComponent<RigidbodyComponent>())
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
                    if(ImGui::MenuItem("Script Component") && !entity.HasComponent<ScriptComponent>())
                        entity.AddComponent<ScriptComponent>();

                    ImGui::EndPopup();
                }
            }
        }
        ImGui::End();
    }

    void InspectorPanel::DrawComponents(Entity& entity)
    {
        if(entity.HasComponent<NameComponent>())
        {
            NameComponent& component = entity.GetComponent<NameComponent>();

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
            ImGui::PushItemWidth(-1);
            ImGui::InputText("##nA@Me", &component.Name);
            ImGui::PopItemWidth();
            ImGui::PopStyleVar();

            ImGui::Dummy(ImVec2(0, 4));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, 4));
        }

        if(entity.HasComponent<TransformComponent>())
        {
            TransformComponent& component = entity.GetComponent<TransformComponent>();
            DrawComponent<TransformComponent>(
                entity, "Transform", [&component]() {
                    if(DrawVec3Control("Position", component.Position))
                        component.MarkDirty();
                    if(DrawVec3Control("Rotation", component.Rotation))
                        component.MarkDirty();
                    if(DrawVec3Control("Scale", component.Scale, 1.0f))
                        component.MarkDirty();

                }, false);
        }

        if(entity.HasComponent<CameraComponent>())
        {
            CameraComponent& component = entity.GetComponent<CameraComponent>();
            DrawComponent<CameraComponent>(entity, "Camera", [&component]() {
                RuntimeCamera& camera = component.Camera;
                ImGuiAux::TProperty<bool>("Primary", &component.Primary);

                const char* projectionTypeStrings[] = { "PERSPECTIVE", "ORTHOGRAPHIC" };
                const char* currentProjectionTypeString = projectionTypeStrings[static_cast<int>(camera.GetProjectionType())];

                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Projection");
                ImGui::TableNextColumn();
                ImGui::PushItemWidth(-1);
                if(ImGui::BeginCombo("##Projection", currentProjectionTypeString))
                {
                    for(int i = 0; i < 2; i++)
                    {
                        const bool isSelected = currentProjectionTypeString == projectionTypeStrings[i];
                        if(ImGui::Selectable(projectionTypeStrings[i], isSelected))
                        {
                            currentProjectionTypeString = projectionTypeStrings[i];
                            camera.SetProjectionType(static_cast<RuntimeCamera::ProjectionType>(i));
                        }
                        if(isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();

                if(camera.GetProjectionType() == RuntimeCamera::ProjectionType::Perspective)
                {
                    float verticalFOV = camera.GetPerspectiveVerticalFOV();
                    if(ImGuiAux::TProperty<float>("Vertical FOV", &verticalFOV)) // In degree
                        camera.SetPerspectiveVerticalFOV(verticalFOV);

                    float nearClip = camera.GetPerspectiveNearClip();
                    if(ImGuiAux::TProperty<float>("Near Clip", &nearClip))
                        camera.SetPerspectiveNearClip(nearClip);

                    float farClip = camera.GetPerspectiveFarClip();
                    if(ImGuiAux::TProperty<float>("Far Clip", &farClip))
                        camera.SetPerspectiveFarClip(farClip);
                }

                if(camera.GetProjectionType() == RuntimeCamera::ProjectionType::Orthographic)
                {
                    float orthoSize = camera.GetOrthographicSize();
                    if(ImGuiAux::TProperty<float>("Size", &orthoSize))
                        camera.SetOrthographicSize(orthoSize);

                    float nearClip = camera.GetOrthographicNearClip();
                    if(ImGuiAux::TProperty<float>("Near Clip", &nearClip))
                        camera.SetOrthographicNearClip(nearClip);

                    float farClip = camera.GetOrthographicFarClip();
                    if(ImGuiAux::TProperty<float>("Far Clip", &farClip))
                        camera.SetOrthographicFarClip(farClip);

                    ImGuiAux::TProperty<bool>("Fixed Aspect Ratio", &component.FixedAspectRatio);
                }
            });
        }

        if(entity.HasComponent<SpriteRendererComponent>())
        {
            SpriteRendererComponent& component = entity.GetComponent<SpriteRendererComponent>();
            DrawComponent<SpriteRendererComponent>(entity, "Sprite Renderer", [&component]() {
                ImGuiAux::TProperty<glm::vec4, ImGuiAux::CustomProprtyFlag::Color4>("Color", &component.Color);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Texture");
                ImGui::TableNextColumn();

                const char* buttonText = component.Texture.IsValid() ? Core::GetAssetManager()->GetMetadata(component.Texture).RelativePath.c_str() : "Drop TEXTURE2D";
                ImGui::PushID(buttonText);

                float fullWidth = ImGui::GetContentRegionAvail().x;
                if(ImGuiAux::Button(buttonText, ImVec2(fullWidth * 0.7f, 0)))
                {
                    Editor* editor = static_cast<Editor*>(Core::GetClient());
                    editor->GetPanelManager().GetPanel<ContentBrowserPanel>()->SetSelectedAsset(component.Texture);
                }

                if(ImGui::BeginDragDropTarget())
                {
                    if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_PAYLOAD))
                    {
                        AssetManager* am = Core::GetAssetManager();
                        SG_ASSERT(payload->DataSize == sizeof(AssetID), "Payload size mismatch!");
                        AssetID droppedAssetID = *(const AssetID*)payload->Data;
                        AssetMetadata meta = am->GetMetadata(droppedAssetID);
                        if(meta.Type == AssetType::TEXTURE2D)
                            component.Texture = droppedAssetID;
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::SameLine();
                if(ImGui::Button("REMOVE", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                    component.Texture = AssetID::INVALID;
                ImGui::PopID();
            });
        }

        if(entity.HasComponent<MeshComponent>())
        {
            MeshComponent& component = entity.GetComponent<MeshComponent>();
            DrawComponent<MeshComponent>(entity, "Mesh Component", [&component]() {

                AssetManager* am = Core::GetAssetManager();

                if(component.MeshID.IsValid())
                    ImGuiAux::TString("Asset Handle: ", "%llu", component.MeshID.Get());
                else
                {
                    ImGui::TableNextColumn(); ImGui::TextUnformatted("Mesh ID");
                    ImGui::TableNextColumn(); ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Drop MESH");
                }

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
                        ImGui::TableNextColumn();
                        ImGui::TableNextColumn();
                        ImGui::TextColored(ImGuiAux::Colors::Red, "MISSING MESH");
                    }
                    else
                    {
                        ImGuiAux::TSperator("Materials");
                        for(size_t i = 0; i < mesh->GetMaterials().size(); i++)
                        {
                            ImGui::PushID(static_cast<int>(i));
                            Ref<Material> material = mesh->GetMaterialAtIndex(i);

                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text("Slot %zu", i);

                            ImGui::TableSetColumnIndex(1);
                            ImGui::PushItemWidth(-FLT_MIN);

                            if(material)
                            {
                                const String& matName = material->GetName();
                                if(ImGui::Button(matName.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                                {
                                    auto* editor = static_cast<Editor*>(Core::GetClient());
                                    editor->GetPanelManager().GetPanel<MaterialEditorPanel>()->SetSelectedMaterial(material);
                                    ImGui::SetWindowFocus("Material Editor");
                                }
                            }
                            else
                            {
                                ImGuiAux::ScopedBoldFont font;
                                ImGui::TextColored(ImGuiAux::Colors::Red, "MISSING MATERIAL");
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
                            ImGui::PopID();
                        }
                    }
                }
            });
        }

        if(entity.HasComponent<LightComponent>())
        {
            LightComponent& component = entity.GetComponent<LightComponent>();
            DrawComponent<LightComponent>(entity, "Light", [&component]() {
                const char* lightTypeStrings[] = { "DIRECTIONAL", "POINT" };
                const char* currentLightTypeString = lightTypeStrings[static_cast<int>(component.Type)];

                ImGui::TableNextColumn();
                ImGui::TextUnformatted("TYPE");
                ImGui::TableNextColumn();
                ImGui::PushItemWidth(-1);
                if(ImGui::BeginCombo("##TYPE", currentLightTypeString))
                {
                    for(int i = 0; i < 2; i++)
                    {
                        const bool isSelected = currentLightTypeString == lightTypeStrings[i];
                        if(ImGui::Selectable(lightTypeStrings[i], isSelected))
                        {
                            currentLightTypeString = lightTypeStrings[i];
                            component.Type = static_cast<LightType>(i);
                        }
                        if(isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();

                ImGuiAux::TProperty<glm::vec3, ImGuiAux::CustomProprtyFlag::Color3>("Color", &component.Color);
                ImGuiAux::TProperty<float>("Intensity", &component.Intensity);

                if(component.Type == LightType::POINT)
                {
                    ImGuiAux::TProperty<float>("Radius", &component.Radius);
                    ImGuiAux::TSlider<float>("Falloff", &component.Falloff, 0.1f, 2.0f);
                }
            });
        }

        if(entity.HasComponent<EnvironmentComponent>())
        {
            EnvironmentComponent& component = entity.GetComponent<EnvironmentComponent>();
            DrawComponent<EnvironmentComponent>(entity, "Environment", [&component]() {
                ImGuiAux::TSperator("Position");
                ImGuiAux::TProperty<float>("Azimuth (X)", &component.Azimuth);
                ImGuiAux::TProperty<float>("Elevation (Y)", &component.Elevation);

                ImGuiAux::TSperator("Visuals");
                ImGuiAux::TProperty<bool>("Sun Disk", &component.EnableSunDisk);
                ImGuiAux::TSlider<float>("Turbidity", &component.Turbidity, 1.5f, 10.0f, "%.2f");
                ImGuiAux::TSlider<float>("Exposure", &component.Exposure, 0.001, 0.1f, "%.4f");
                ImGuiAux::TSlider<float>("Sun Intensity", &component.SunIntensity, 0.01, 1.0f, "%.2f");

                ImGuiAux::TSperator("Fast GI");
                ImGuiAux::TProperty<glm::vec3, ImGuiAux::CustomProprtyFlag::Color3>("SkyAmbient", &component.SkyAmbient);
                ImGuiAux::TProperty<glm::vec3, ImGuiAux::CustomProprtyFlag::Color3>("HorizonAmbient", &component.HorizonAmbient);
                ImGuiAux::TProperty<glm::vec3, ImGuiAux::CustomProprtyFlag::Color3>("GroundAmbient", &component.GroundAmbient);
            });
        }

        if(entity.HasComponent<RigidbodyComponent>())
        {
            RigidbodyComponent& component = entity.GetComponent<RigidbodyComponent>();
            DrawComponent<RigidbodyComponent>(entity, "Rigidbody", [&component]() {
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
                            component.Type = static_cast<RigidbodyType>(i);
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

                    ImGuiAux::TSperator("Freeze Rotation");
                    ImGuiAux::TProperty<bool>("X Axis", &component.FreezeRotationX);
                    ImGuiAux::TProperty<bool>("Y Axis", &component.FreezeRotationY);
                    ImGuiAux::TProperty<bool>("Z Axis", &component.FreezeRotationZ);
                }
            });
        }

        if(entity.HasComponent<BoxColliderComponent>())
        {
            BoxColliderComponent& component = entity.GetComponent<BoxColliderComponent>();
            DrawComponent<BoxColliderComponent>(entity, "Box Collider", [&component]() {
                ImGuiAux::TProperty<glm::vec3>("Half Extents", &component.HalfExtents);
                ImGuiAux::TProperty<bool>("Show Collider", &component.ShowCollider);
            });
        }

        if(entity.HasComponent<SphereColliderComponent>())
        {
            SphereColliderComponent& component = entity.GetComponent<SphereColliderComponent>();
            DrawComponent<SphereColliderComponent>(entity, "Sphere Collider", [&component]() {
                ImGuiAux::TProperty<float>("Radius", &component.Radius);
                ImGuiAux::TProperty<bool>("Show Collider", &component.ShowCollider);
            });
        }

        if(entity.HasComponent<CapsuleColliderComponent>())
        {
            CapsuleColliderComponent& component = entity.GetComponent<CapsuleColliderComponent>();
            DrawComponent<CapsuleColliderComponent>(entity, "Capsule Collider", [&component]() {
                ImGuiAux::TProperty<float>("Radius", &component.Radius);
                ImGuiAux::TProperty<float>("Height", &component.Height);
                ImGuiAux::TProperty<bool>("Show Collider", &component.ShowCollider);
            });
        }

        if(entity.HasComponent<CylinderColliderComponent>())
        {
            CylinderColliderComponent& component = entity.GetComponent<CylinderColliderComponent>();
            DrawComponent<CylinderColliderComponent>(entity, "Cylinder Collider", [&component]() {
                ImGuiAux::TProperty<float>("Radius", &component.Radius);
                ImGuiAux::TProperty<float>("Height", &component.Height);
                ImGuiAux::TProperty<bool>("Show Collider", &component.ShowCollider);
            });
        }

        if(entity.HasComponent<ConvexColliderComponent>())
        {
            ConvexColliderComponent& component = entity.GetComponent<ConvexColliderComponent>();
            DrawComponent<ConvexColliderComponent>(entity, "Convex Collider", [&component, &entity]() {
                if(entity.HasComponent<MeshComponent>())
                {
                    if(ImGuiAux::TProperty<glm::vec3>("Local Offset", &component.LocalOffset))
                        component.IsDirty = true;
                    if(ImGuiAux::TProperty<glm::vec3>("Local Rotation", &component.LocalRotation))
                        component.IsDirty = true;
                }
                ImGuiAux::TProperty<bool>("Show Collider", &component.ShowCollider);
            });
        }

        if(entity.HasComponent<MeshColliderComponent>())
        {
            MeshColliderComponent& component = entity.GetComponent<MeshColliderComponent>();
            DrawComponent<MeshColliderComponent>(entity, "Mesh Collider", [&component, &entity]() {
                if(entity.HasComponent<RigidbodyComponent>() && entity.GetComponent<RigidbodyComponent>().Type != RigidbodyType::DYNAMIC)
                {
                    if(entity.HasComponent<MeshComponent>())
                    {
                        ImGuiAux::TProperty<glm::vec3>("Local Offset", &component.LocalOffset);
                        ImGuiAux::TProperty<glm::vec3>("Local Rotation", &component.LocalRotation);
                    }
                    ImGuiAux::TSperator("Notice");
                    ImGuiAux::TString("Info", "Mesh Colliders are generated implicitly from the mesh. Visualization is not supported, pray and hope that it works :)");
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGuiAux::Colors::Red);
                    ImGuiAux::TString("ERROR", "Invalid setup: Change Rigidbody to STATIC or KINEMATIC!");
                    ImGui::PopStyleColor();
                }
            });
        }

        if(entity.HasComponent<ScriptComponent>())
        {
            ScriptComponent& component = entity.GetComponent<ScriptComponent>();
            DrawComponent<ScriptComponent>(entity, "Script Component", [&component]() {
                AssetManager* am = Core::GetAssetManager();

                String buttonText;
                bool hasScript = component.ScriptAsset.IsValid();
                if(hasScript)
                {
                    const String& scriptPath = am->GetMetadata(component.ScriptAsset).RelativePath;
                    buttonText = Filesystem::GetFilenameWithExt(scriptPath).c_str();
                }
                else
                    buttonText = "Drop SCRIPT";

                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Script");
                ImGui::TableNextColumn();
                if(ImGui::Button(buttonText.c_str(), ImVec2(ImGui::GetContentRegionAvail().x / 1.3f, 0)) && hasScript)
                {
                    Editor* editor = static_cast<Editor*>(Core::GetClient());
                    editor->GetPanelManager().GetPanel<ContentBrowserPanel>()->SetSelectedAsset(component.ScriptAsset);
                }
                if(hasScript)
                {
                    ImGui::SameLine();
                    if(ImGuiAux::Button("REMOVE", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                        component.ScriptAsset = AssetID::INVALID;
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
        if(entity.HasComponent<TextComponent>())
        {
            TextComponent& component = entity.GetComponent<TextComponent>();
            DrawComponent<TextComponent>(entity, "Text Component", [&component]() {
                AssetManager* am = Core::GetAssetManager();

                String buttonText;
                bool hasFontAsset = component.FontAssetID.IsValid();
                if(hasFontAsset)
                {
                    const String& fontPath = am->GetMetadata(component.FontAssetID).RelativePath;
                    buttonText = Filesystem::GetFilenameWithExt(fontPath).c_str();
                }
                else
                    buttonText = "Drop FONT";

                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Font");
                ImGui::TableNextColumn();
                if(ImGui::Button(buttonText.c_str(), ImVec2(ImGui::GetContentRegionAvail().x / 1.5f, 0)) && hasFontAsset)
                {
                    Editor* editor = static_cast<Editor*>(Core::GetClient());
                    editor->GetPanelManager().GetPanel<ContentBrowserPanel>()->SetSelectedAsset(component.FontAssetID);
                }
                if(hasFontAsset)
                {
                    ImGui::SameLine();
                    if(ImGuiAux::Button("REMOVE", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                        component.FontAssetID = AssetID::INVALID;
                }

                if(ImGui::BeginDragDropTarget())
                {
                    if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_PAYLOAD))
                    {
                        SG_ASSERT(payload->DataSize == sizeof(AssetID), "Payload size mismatch!");
                        AssetID droppedAssetID = *(const AssetID*)payload->Data;
                        AssetMetadata meta = am->GetMetadata(droppedAssetID);
                        if(meta.Type == AssetType::FONT)
                            component.FontAssetID = droppedAssetID;
                    }
                    ImGui::EndDragDropTarget();
                }

                if(component.FontAssetID)
                {
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Text");
                    ImGui::TableNextColumn();
                    ImGui::PushItemWidth(-1);
                    ImGui::InputTextMultiline("##Text", &component.Text);
                    ImGui::PopItemWidth();
                    ImGuiAux::TProperty<glm::vec4, ImGuiAux::CustomProprtyFlag::Color4>("Color", &component.Color);
                    ImGuiAux::TProperty<float>("Max Width", &component.MaxWidth);

                    ImGuiAux::TSperator("Style");

                    ImGuiAux::TProperty<float>("Letter Spacing", &component.LetterSpacing);
                    ImGuiAux::TProperty<float>("Line Spacing", &component.LineSpacing);

                    const char* alignTypeStrings[] = { "LEFT", "CENTER", "RIGHT" };
                    const char* currentAlignTypeString = alignTypeStrings[static_cast<uint8_t>(component.Alignment)];

                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("ALIGNMENT");
                    ImGui::TableNextColumn();
                    ImGui::PushItemWidth(-1);
                    if(ImGui::BeginCombo("##ALIGNTYPE", currentAlignTypeString))
                    {
                        for(int i = 0; i < 3; i++)
                        {
                            const bool isSelected = (component.Alignment == static_cast<TextAlignment>(i));
                            if(ImGui::Selectable(alignTypeStrings[i], isSelected))
                                component.Alignment = static_cast<TextAlignment>(i);
                            if(isSelected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::PopItemWidth();
                    ImGuiAux::TProperty<bool>("Underline", &component.Underline);
                    ImGuiAux::TProperty<bool>("Italic", &component.Italic);

                    ImGuiAux::TSperator("Shadow");
                    ImGui::PushID("Shadow");
                    ImGuiAux::TProperty<bool>("Enabled", &component.ShadowEnabled);
                    ImGuiAux::TProperty<glm::vec4, ImGuiAux::CustomProprtyFlag::Color4>("Color", &component.ShadowColor);
                    ImGuiAux::TProperty<glm::vec2>("Offset", &component.ShadowOffset);
                    ImGui::PopID();
                }
            });
        }
    }

} // namespace Surge