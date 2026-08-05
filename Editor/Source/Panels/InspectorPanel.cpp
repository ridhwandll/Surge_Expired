// Copyright (c) - SurgeTechnologies - All rights reserved
#include "InspectorPanel.hpp"
#include "Surge/Graphics/HighLevel/Mesh.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/ECS/Components.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Graphics/HighLevel/Font.hpp"
#include "Surge/ScriptEngine/ScriptAsset.hpp"
#include "Surge/Audio/Audio.hpp"

#include "Editor.hpp"
#include "MaterialEditorPanel.hpp"
#include "ContentBrowserPanel.hpp"
#include "Utility/ImGuiAux.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <imgui_internal.h>
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Utility/Platform.hpp"
#include "Surge/Audio/AudioEngine.hpp"

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

    template <typename T>
    static bool DrawAssetDropSlot(const char* label, Ref<Asset>& assetPtr, AssetType expectedType, const char* dropHint)
    {
        bool modified = false;
        AssetManager* am = Core::GetAssetManager();
        bool hasAsset = (bool)assetPtr;

        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::TableNextColumn();

        String buttonText = dropHint;
        if(hasAsset)
        {
            const String& path = am->GetMetadata(assetPtr->GetID()).RelativePath;
            buttonText = Filesystem::GetFilenameWithExt(path).c_str();
        }

        ImFont* boldFont = ImGui::GetIO().Fonts->Fonts[1];
        ImGui::PushFont(boldFont);

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
        float fullWidth = ImGui::GetContentRegionAvail().x;

        float removeButtonWidth = hasAsset ? ImGui::CalcTextSize("REMOVE").x + ImGui::GetStyle().FramePadding.x * 2.0f : 0.0f;
        float spacing = hasAsset ? ImGui::GetStyle().ItemSpacing.x : 0.0f;
        float assetButtonWidth = fullWidth - removeButtonWidth - spacing;

        ImVec4 btnNormalCol, btnHoverCol, btnActiveCol, borderCol, textCol;
        if(hasAsset)
        {
            btnNormalCol = ImGuiAux::Colors::LightGreen;
            btnHoverCol = ImGuiAux::Colors::Iron;
            btnActiveCol = ImGuiAux::Colors::Titanium;
            borderCol = ImGuiAux::Colors::ExtraDark;
            textCol = ImGuiAux::Colors::ExtraDark;
        }
        else
        {
            float time = (float)ImGui::GetTime();
            // Map sine wave (-1 to 1) to a 0.0 to 1.0 range. Speed multiplier: 3.5f
            float pulse = (sinf(time * 3.5f) * 0.5f) + 0.5f;

            ImVec4 baseColor = ImGuiAux::Colors::ExtraDark;
            ImVec4 glowColor = ImGuiAux::Colors::Red;
            btnNormalCol = ImLerp(baseColor, glowColor, pulse);

            btnHoverCol = btnNormalCol;
            btnActiveCol = ImVec4(0.09f, 0.09f, 0.09f, 1.0f);
            borderCol = ImGuiAux::Colors::Red;
            textCol = ImGuiAux::Colors::White;
        }

        ImGui::PushStyleColor(ImGuiCol_Button, btnNormalCol);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btnHoverCol);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, btnActiveCol);
        ImGui::PushStyleColor(ImGuiCol_Border, borderCol);
        ImGui::PushStyleColor(ImGuiCol_Text, textCol);
        if(ImGui::Button(buttonText.c_str(), ImVec2(assetButtonWidth, 0)) && hasAsset)
        {
            Editor* editor = static_cast<Editor*>(Core::GetClient());
            editor->GetPanelManager().GetPanel<ContentBrowserPanel>()->SetSelectedAsset(assetPtr->GetID());
        }
        if(!hasAsset)
            ImGuiAux::DelayedToolTip("Drop asset from Content Browser");

        // Drag and Drop Logic
        if(ImGui::BeginDragDropTarget())
        {
            if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_PAYLOAD))
            {
                SG_ASSERT(payload->DataSize == sizeof(AssetID), "Payload size mismatch!");
                AssetID droppedAssetID = *(const AssetID*)payload->Data;
                AssetMetadata meta = am->GetMetadata(droppedAssetID);

                if(meta.Type == expectedType)
                {
                    assetPtr = am->Load<T>(droppedAssetID);
                    modified = true;
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::PopStyleColor(5);

        // Draw Remove [REMOVE] Button
        if(hasAsset)
        {
            ImGui::SameLine();
            if(ImGui::Button("REMOVE", ImVec2(removeButtonWidth, 0)))
            {
                assetPtr = nullptr;
                modified = true;
            }
        }

        // Cleanup Styles & Fonts
        ImGui::PopStyleVar(2);
        ImGui::PopFont();

        return modified;
    }

    template <typename, typename = void>
    constexpr bool HasActiveProperty_v = false;
    template <typename T>
    constexpr bool HasActiveProperty_v<T, std::void_t<decltype(std::declval<T>().Active)>> = true;

    template <typename XComponent, bool IsRemoveable = true, typename Func>
    static void DrawComponent(Entity& entity, const String& name, Func&& function)
    {
        const int64_t& hash = SurgeReflect::GetReflection<XComponent>()->GetHash();
        ImGui::PushID(static_cast<int>(hash));

        ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2 { 4, 4 });
        const float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;

        ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_FramePadding;

        const bool open = ImGui::TreeNodeEx((void*)hash, treeNodeFlags, "%s", name.c_str());

        XComponent& component = entity.GetComponent<XComponent>();
        bool removeComponent = false;

        const float optionsBtnSize = lineHeight;
        const float checkboxSize = ImGui::GetFrameHeight();
        const float spacing = 0;

        const float optionsPos = contentRegionAvailable.x - optionsBtnSize * 0.5f;
        const float checkboxPos = optionsPos - checkboxSize - spacing;

        if constexpr (HasActiveProperty_v<XComponent>)
        {
            // If it's not removeable, we don't have the options button, so move checkbox to the far right
            ImGui::SameLine(IsRemoveable ? checkboxPos : optionsPos);
            ImGui::Checkbox("##ActiveToggle", &component.Active);
        }

        if constexpr (IsRemoveable)
        {
            ImGui::SameLine(optionsPos);
            if(ImGui::Button(".../", ImVec2 { lineHeight, lineHeight }))
                ImGui::OpenPopup("ComponentSettings");

            ImGuiAux::StyledPopupVars::Push();
            if(ImGui::BeginPopup("ComponentSettings"))
            {
                ImFont* boldFont = ImGui::GetIO().Fonts->Fonts[1];
                ImGui::PushFont(boldFont);
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "OPTIONS");
                ImGui::PopFont();
                ImGuiAux::StyledSeparator();

                if(ImGuiAux::StyledMenuItem("Remove Component"))
                    removeComponent = true;

                ImGuiAux::EndStyledPopup();
            }
            else
                ImGuiAux::StyledPopupVars::Pop();
        }
        ImGui::PopStyleVar();

        if(open)
        {
            ImGui::Dummy(ImVec2(0.0f, 2.0f));
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

        if constexpr (IsRemoveable)
        {
            if(removeComponent)
                Core::AddFrameEndCallback([entity]() mutable { entity.RemoveComponent<XComponent>(); });
        }

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

                ImGuiAux::StyledPopupVars::Push();
                if(ImGui::BeginPopup("AddComponentPopup"))
                {
                    ImFont* boldFont = ImGui::GetIO().Fonts->Fonts[1];

                    ImGui::PushFont(boldFont);
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "RENDERING");
                    ImGui::PopFont();
                    ImGuiAux::StyledSeparator();

                    if(ImGuiAux::StyledMenuItem("Camera Component") && !entity.HasComponent<CameraComponent>())
                        entity.AddComponent<CameraComponent>();
                    if(ImGuiAux::StyledMenuItem("Sprite Renderer Component") && !entity.HasComponent<SpriteRendererComponent>())
                        entity.AddComponent<SpriteRendererComponent>(ImGuiAux::Colors::ThemeColor1);
                    if(ImGuiAux::StyledMenuItem("Mesh Component") && !entity.HasComponent<MeshComponent>())
                        entity.AddComponent<MeshComponent>();
                    if(ImGuiAux::StyledMenuItem("Light Component") && !entity.HasComponent<LightComponent>())
                        entity.AddComponent<LightComponent>();
                    if(ImGuiAux::StyledMenuItem("Environment Component") && !entity.HasComponent<EnvironmentComponent>())
                        entity.AddComponent<EnvironmentComponent>();
                    if(ImGuiAux::StyledMenuItem("Text Component") && !entity.HasComponent<TextComponent>())
                        entity.AddComponent<TextComponent>();

                    ImGuiAux::StyledSeparator();
                    ImGui::PushFont(boldFont);
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "PHYSICS");
                    ImGui::PopFont();
                    ImGuiAux::StyledSeparator();

                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
                    if(ImGui::BeginMenu("Colliders & Dynamics"))
                    {
                        if(ImGuiAux::StyledMenuItem("Rigidbody Component") && !entity.HasComponent<RigidbodyComponent>())
                            entity.AddComponent<RigidbodyComponent>();
                        if(ImGuiAux::StyledMenuItem("Box Collider") && !entity.HasComponent<BoxColliderComponent>())
                            entity.AddComponent<BoxColliderComponent>();
                        if(ImGuiAux::StyledMenuItem("Sphere Collider") && !entity.HasComponent<SphereColliderComponent>())
                            entity.AddComponent<SphereColliderComponent>();
                        if(ImGuiAux::StyledMenuItem("Capsule Collider") && !entity.HasComponent<CapsuleColliderComponent>())
                            entity.AddComponent<CapsuleColliderComponent>();
                        if(ImGuiAux::StyledMenuItem("Cylinder Collider") && !entity.HasComponent<CylinderColliderComponent>())
                            entity.AddComponent<CylinderColliderComponent>();
                        if(ImGuiAux::StyledMenuItem("Convex Collider") && !entity.HasComponent<ConvexColliderComponent>() && entity.HasComponent<MeshComponent>())
                            entity.AddComponent<ConvexColliderComponent>();
                        if(ImGuiAux::StyledMenuItem("Mesh Collider") && !entity.HasComponent<MeshColliderComponent>())
                            entity.AddComponent<MeshColliderComponent>();

                        ImGui::EndMenu();
                    }
                    ImGui::PopStyleVar();

                    ImGuiAux::StyledSeparator();
                    ImGui::PushFont(boldFont);
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "SCRIPTING");
                    ImGui::PopFont();
                    ImGuiAux::StyledSeparator();

                    if(ImGuiAux::StyledMenuItem("Script Component") && !entity.HasComponent<ScriptComponent>())
                        entity.AddComponent<ScriptComponent>();

                    if(ImGuiAux::StyledMenuItem("UICanvas Component"))
                    {
                        if(!entity.HasComponent<UICanvasComponent>())
                            entity.AddComponent<UICanvasComponent>();
                        else
                            Platform::ErrorMessageBox("Entity already has a UICanvas Component");
                    }
                    ImGui::PushFont(boldFont);
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "AUDIO");
                    ImGui::PopFont();
                    ImGuiAux::StyledSeparator();
                    if(ImGuiAux::StyledMenuItem("Audio Source Component") && !entity.HasComponent<AudioSourceComponent>())
                        entity.AddComponent<AudioSourceComponent>();
                    if(ImGuiAux::StyledMenuItem("Audio Listener Component") && !entity.HasComponent<AudioListenerComponent>())
                        entity.AddComponent<AudioListenerComponent>();

                    ImGuiAux::EndStyledPopup();
                }
                else
                {
                    ImGuiAux::StyledPopupVars::Pop();
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
            DrawComponent<TransformComponent, false>(
                entity, "Transform", [&component]() {
                    if(DrawVec3Control("Position", component.Position))
                        component.MarkDirty();
                    if(DrawVec3Control("Rotation", component.Rotation))
                        component.MarkDirty();
                    if(DrawVec3Control("Scale", component.Scale, 1.0f))
                        component.MarkDirty();

                });
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

                ImGuiAux::TProperty<bool>("Fixed Aspect Ratio", &component.FixedAspectRatio);

                if(component.FixedAspectRatio)
                {
                    glm::vec2 aspectRatio = camera.GetAspectRatio();
                    if(ImGuiAux::TProperty<glm::vec2>("Aspect Ratio", &aspectRatio))
                        camera.SetAspectRatio(aspectRatio);
                }

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
                ImGuiAux::TProperty<bool>("Billboard", &component.Billboard);
                DrawAssetDropSlot<Texture2D>("Texture", component.TextureAsset, AssetType::TEXTURE2D, "Drop TEXTURE2D");
            });
        }

        if(entity.HasComponent<MeshComponent>())
        {
            MeshComponent& component = entity.GetComponent<MeshComponent>();
            DrawComponent<MeshComponent>(entity, "Mesh Component", [&component]() {

                DrawAssetDropSlot<Mesh>("Mesh", component.MeshAsset, AssetType::MESH, "Drop MESH");

                if(component.MeshAsset)
                {
                    ImGuiAux::TProperty<bool>("Drop Shadow", &component.DropShadow);

                    Ref<Mesh> mesh = component.MeshAsset;
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
                            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
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
                            ImGui::PopStyleVar();

                            if(ImGui::BeginDragDropTarget())
                            {
                                if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_PAYLOAD))
                                {
                                    AssetManager* am = Core::GetAssetManager();
                                    SG_ASSERT(payload->DataSize == sizeof(AssetID), "Payload size mismatch!");
                                    AssetID droppedAssetID = *(const AssetID*)payload->Data;
                                    AssetMetadata meta = am->GetMetadata(droppedAssetID);

                                    if(meta.Type == AssetType::MATERIAL)
                                    {
                                        mesh->SetMaterialOverride(i, am->Load<Material>(droppedAssetID));
                                        am->Save(meta.ID);
                                        am->Save(component.MeshAsset->GetID());
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

                constexpr auto lightTypeStrings = std::array { "DIRECTIONAL", "POINT" };
                ImGuiAux::TComboBox("TYPE", component.Type, lightTypeStrings);
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

                constexpr auto rbTypeStrings = std::array { "STATIC", "DYNAMIC", "KINEMATIC" };
                ImGuiAux::TComboBox("TYPE", component.Type, rbTypeStrings);

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
                DrawAssetDropSlot<Script>("Script", component.ScriptAsset, AssetType::SCRIPT, "Drop SCRIPT");
            });
        }
        if(entity.HasComponent<TextComponent>())
        {
            TextComponent& component = entity.GetComponent<TextComponent>();
            DrawComponent<TextComponent>(entity, "Text Component", [&component]() {

                DrawAssetDropSlot<Font>("Font", component.FontAsset, AssetType::FONT, "Drop FONT");

                if(component.FontAsset)
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
                    constexpr auto alignTypeStrings = std::array { "LEFT", "CENTER", "RIGHT" };
                    ImGuiAux::TComboBox("Horizontal Alignment", component.Alignment, alignTypeStrings);
                    constexpr auto verticalAlignTypeStrings = std::array { "TOP", "CENTER", "BASELINE", "BOTTOM" };
                    ImGuiAux::TComboBox("Vertical Alignment", component.VerticalAlignment, verticalAlignTypeStrings);
                    ImGui::PopItemWidth();
                    ImGuiAux::TProperty<bool>("Underline", &component.Underline);
                    ImGuiAux::TProperty<bool>("Italic", &component.Italic);
                    ImGuiAux::TProperty<bool>("Billboard", &component.Billboard);

                    ImGuiAux::TSperator("Shadow");
                    ImGui::PushID("Shadow");
                    ImGuiAux::TProperty<bool>("Enabled", &component.ShadowEnabled);
                    ImGuiAux::TProperty<glm::vec4, ImGuiAux::CustomProprtyFlag::Color4>("Color", &component.ShadowColor);
                    ImGuiAux::TProperty<glm::vec2>("Offset", &component.ShadowOffset);
                    ImGui::PopID();
                }
            });
        }

        if(entity.HasComponent<UICanvasComponent>())
        {
            UICanvasComponent& component = entity.GetComponent<UICanvasComponent>();
            DrawComponent<UICanvasComponent>(entity, "UI Canvas Component", [&component]() {
                ImGuiAux::TProperty<bool>("Show Canvas", &component.ShowCanvas);
                DrawAssetDropSlot<Script>("Script", component.ScriptAsset, AssetType::SCRIPT, "Drop UI SCRIPT");
            });
        }
        if(entity.HasComponent<AudioListenerComponent>())
        {
            DrawComponent<AudioListenerComponent>(entity, "Audio Listener Component", []() {});
        }
        if(entity.HasComponent<AudioSourceComponent>())
        {
            AudioSourceComponent& component = entity.GetComponent<AudioSourceComponent>();
            DrawComponent<AudioSourceComponent>(entity, "Audio Source Component", [&component]() {
                AudioEngine* audioEngine = Core::GetAudioEngine();

                DrawAssetDropSlot<Audio>("Audio Clip", component.AudioClip, AssetType::AUDIO, "Drop Audio Clip");

                if(component.AudioClip)
                {
                    ImGuiAux::TProperty<bool>("Streaming", &component.IsStreaming);
                    ImGuiAux::TProperty<bool>("Play On Awake", &component.PlayOnAwake);

                    ImGuiAux::TProperty<bool>("Spatialized", &component.IsSpatialized);
                    if (component.IsSpatialized)
                    {
                        constexpr auto attenuationModelStrings = std::array { "NONE", "INVERSE_DISTANCE", "LINEAR_DISTANCE", "EXPONENTIAL_DISTANCE" };
                        ImGuiAux::TComboBox("Attenuation", component.Attenuation, attenuationModelStrings);

                        if(ImGuiAux::TProperty<float>("Min Distance", &component.MinDistance) && component.RuntimeID)
                            audioEngine->SetMinDistance(component.RuntimeID, component.MinDistance);

                        if(ImGuiAux::TProperty<float>("Max Distance", &component.MaxDistance) && component.RuntimeID)
                            audioEngine->SetMaxDistance(component.RuntimeID, component.MaxDistance);
                    }

                    if (ImGuiAux::TProperty<bool>("Loop", &component.Loop) && component.RuntimeID)
                        audioEngine->SetLooping(component.RuntimeID, component.Loop);

                    if (ImGuiAux::TSlider<float>("Volume", &component.Volume, 0.0f, 2.0f, "%.1f") && component.RuntimeID)
                        audioEngine->SetVolume(component.RuntimeID, component.Volume);

                    if (ImGuiAux::TSlider<float>("Pitch", &component.Pitch, 0.1f, 3.0f, "%.1f") && component.RuntimeID)
                        audioEngine->SetPitch(component.RuntimeID, component.Pitch);

                }
            });
        }
    }

} // namespace Surge