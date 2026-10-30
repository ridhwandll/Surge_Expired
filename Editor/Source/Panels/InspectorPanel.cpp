// Copyright (c) - SurgeTechnologies - All rights reserved
#include "InspectorPanel.hpp"
#include "Surge/Core/Core.hpp"

#include "Surge/ECS/Components/Components.hpp"
#include "Surge/ECS/Components/ScriptComponent.hpp"

#include "Surge/Graphics/HighLevel/Mesh.hpp"
#include "Surge/Graphics/HighLevel/Font.hpp"

#include "Surge/Asset/AssetManager.hpp"

#include "Surge/ScriptEngine/ScriptAsset.hpp"
#include "Surge/Audio/Audio.hpp"
#include "Surge/Asset/Format/Mesh/MeshFormat.hpp"
#include "Asset/SourceIO/SceneSourceIO.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"

#include "Editor.hpp"
#include "MaterialEditorPanel.hpp"
#include "ContentBrowserPanel.hpp"
#include "UIEditorPanel.hpp"
#include "ParticleEditorPanel.hpp"
#include "Surge/Graphics/Particles/ParticleEffect.hpp"
#include "Surge/Graphics/UISystem/UILayout.hpp"
#include "Utility/EditorUI.hpp"
#include "Gizmos/GizmoIcons.hpp"
#include "StatusBar/EditorStatusBar.hpp"
#include "Scripting/ScriptFields.hpp"
#include "Surge/ScriptEngine/ScriptEngine.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <imgui_internal.h>
#include <json/json.hpp>
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Audio/AudioEngine.hpp"
#include <algorithm>

namespace Surge
{
    namespace
    {
        namespace Colors = EditorUI::Colors;
        using EditorUI::ToU32;
        using EditorUI::WithAlpha;
        constexpr float HEADER_GRADIENT_WIDTH = 0.45f; // Fraction of the header the component color fades over
        constexpr float HEADER_GRADIENT_ALPHA = 0.8f;  // Opacity at the right edge
        constexpr float OUTLINE_THICKNESS = 1.5f;

        struct ComponentStyle
        {
            GizmoIcon Icon;
            ImU32 Color;
        };


        template <typename T>
        ComponentStyle StyleOf()
        {
            if constexpr(std::is_same_v<T, TransformComponent>)              return { GizmoIcon::TRANSFORM, ToU32(Colors::Component::Transform) };
            else if constexpr(std::is_same_v<T, CameraComponent>)            return { GizmoIcon::CAMERA, ToU32(Colors::Component::Camera) };
            else if constexpr(std::is_same_v<T, SpriteRendererComponent>)    return { GizmoIcon::SPRITE, ToU32(Colors::Component::Sprite) };
            else if constexpr(std::is_same_v<T, MeshComponent>)              return { GizmoIcon::MESH, ToU32(Colors::Component::Mesh) };
            else if constexpr(std::is_same_v<T, LightComponent>)             return { GizmoIcon::POINT_LIGHT, ToU32(Colors::Component::Light) };
            else if constexpr(std::is_same_v<T, EnvironmentComponent>)       return { GizmoIcon::ENVIRONMENT, ToU32(Colors::Component::Environment) };
            else if constexpr(std::is_same_v<T, RendererSettingsComponent>)  return { GizmoIcon::CAMERA, ToU32(Colors::Component::RendererSettings) };
            else if constexpr(std::is_same_v<T, TextComponent>)              return { GizmoIcon::TEXT, ToU32(Colors::Component::Text) };
            else if constexpr(std::is_same_v<T, RigidbodyComponent>)         return { GizmoIcon::RIGIDBODY, ToU32(Colors::Component::Physics) };
            else if constexpr(std::is_same_v<T, ScriptComponent>)            return { GizmoIcon::SCRIPT, ToU32(Colors::Component::Script) };
            else if constexpr(std::is_same_v<T, UICanvasComponent>)          return { GizmoIcon::UI_CANVAS, ToU32(Colors::Component::UICanvas) };
            else if constexpr(std::is_same_v<T, AudioSourceComponent>)       return { GizmoIcon::AUDIO_SOURCE, ToU32(Colors::Component::Audio) };
            else if constexpr(std::is_same_v<T, AudioListenerComponent>)     return { GizmoIcon::AUDIO_LISTENER, ToU32(Colors::Component::Audio) };
            else if constexpr(std::is_same_v<T, ParticleSystemComponent>)    return { GizmoIcon::PARTICLES, ToU32(Colors::Component::Particles) };
            else if constexpr(std::is_same_v<T, AnimatorComponent>)          return { GizmoIcon::PLAY, ToU32(Colors::Component::Animator) };
            else                                                             return { GizmoIcon::COLLIDER, ToU32(Colors::Component::Collider) }; // Colliders
        }

        template <typename T>
        bool HasComponentOf(Entity e) { return e.HasComponent<T>(); }

        // Components a user sees in the Inspector (ID/Name/Relationship are always there and not shown as components)
        template <typename... Ts>
        int CountComponents(Entity e) { return (static_cast<int>(e.HasComponent<Ts>()) + ...); }

        int CountVisibleComponents(Entity e)
        {
            return CountComponents<TransformComponent, CameraComponent, SpriteRendererComponent, MeshComponent, LightComponent, EnvironmentComponent, RendererSettingsComponent,
                                   RigidbodyComponent, BoxColliderComponent, SphereColliderComponent, CapsuleColliderComponent, CylinderColliderComponent,
                                   ConvexColliderComponent, MeshColliderComponent, ScriptComponent, TextComponent, UICanvasComponent,
                                   AudioListenerComponent, AudioSourceComponent, ParticleSystemComponent, AnimatorComponent>(e);
        }

        // ------------------------------------------------------------------ Component clipboard
        // Held as serialized JSON (the format undo uses): no asset Refs kept alive in a static, runtime handles never get copied

        struct ComponentClipboard
        {
            String Key; // Reflection class name, the component's key in SceneSourceIO::EntityToJson()
            String DisplayName;
            String Json;
            bool (*Has)(Entity) = nullptr;

            bool IsValid() const { return !Json.empty(); }
        };
        ComponentClipboard sComponentClipboard;

        template <typename T>
        const char* ComponentKey() { return SurgeReflect::GetReflection<T>()->GetName().c_str(); }

        template <typename T>
        void CopyComponent(Entity entity, const char* displayName)
        {
            const nlohmann::json state = nlohmann::json::parse(SceneSourceIO::EntityToJson(entity), nullptr, false);
            const char* key = ComponentKey<T>();
            if(state.is_discarded() || !state.contains(key))
                return;
            sComponentClipboard = { key, displayName, state[key].dump(), &HasComponentOf<T> };
        }

        // Applies at frame end: deserializing may add the component, nothing may hold component references then.
        // The selection tracker sees the change like any other edit (undoable)
        void PasteComponent(Entity entity)
        {
            Core::AddFrameEndCallback([entity, clipboard = sComponentClipboard]() {
                if(!entity)
                    return;
                nlohmann::json state = nlohmann::json::parse(SceneSourceIO::EntityToJson(entity), nullptr, false);
                nlohmann::json value = nlohmann::json::parse(clipboard.Json, nullptr, false);
                if(state.is_discarded() || value.is_discarded())
                    return;
                state[clipboard.Key] = std::move(value);
                SceneSourceIO::EntityFromJson(state.dump(), entity);
            });
        }

        // Script-driven components own live Lua instances, everything else can be reset to its defaults in edit mode
        template <typename T>
        constexpr bool IsResettable_v = !std::is_same_v<T, ScriptComponent> && !std::is_same_v<T, UICanvasComponent>;

        template <typename T>
        void ResetComponent(T& component)
        {
            if constexpr(std::is_same_v<T, SpriteRendererComponent>)
                component = SpriteRendererComponent(glm::vec4(1.0f));
            else
                component = T {};

            if constexpr(std::is_same_v<T, TransformComponent>)
                component.MarkDirty();
        }

        template <typename, typename = void>
        constexpr bool HasActiveProperty_v = false;
        template <typename T>
        constexpr bool HasActiveProperty_v<T, std::void_t<decltype(std::declval<T>().Active)>> = true;

        // ------------------------------------------------------------------ Property widgets

        template <typename T>
        bool DrawAssetDropSlot(const char* label, Ref<Asset>& assetPtr, AssetType expectedType, const char* dropHint)
        {
            bool modified = false;
            AssetManager* am = Core::GetAssetManager();
            const bool hasAsset = static_cast<bool>(assetPtr);

            ImGui::PushID(label); // Several slots per component (UI Canvas: Layout + Script), each needs its own button IDs
            EditorUI::TLabel(label);

            String buttonText = dropHint;
            if(hasAsset)
                buttonText = Filesystem::GetFilenameWithExt(am->GetMetadata(assetPtr->GetID()).RelativePath);

            // Highlight while a matching asset is dragged over the Inspector
            bool acceptsDrag = false;
            if(const ImGuiPayload* drag = ImGui::GetDragDropPayload(); drag && drag->IsDataType(CONTENT_BROWSER_PAYLOAD))
                acceptsDrag = am->GetMetadata(*static_cast<const AssetID*>(drag->Data)).Type == expectedType;

            EditorUI::ScopedBoldFont font;
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, OUTLINE_THICKNESS);
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));

            const float clearWidth = hasAsset ? ImGui::GetFrameHeight() : 0.0f;
            const float spacing = hasAsset ? 4.0f : 0.0f;
            const float slotWidth = ImGui::GetContentRegionAvail().x - clearWidth - spacing;

            const ImVec4 border = acceptsDrag ? Colors::Accent : (hasAsset ? Colors::Tone30 : Colors::ErrorMuted);
            ImGui::PushStyleColor(ImGuiCol_Border, border);
            ImGui::PushStyleColor(ImGuiCol_Button, Colors::Tone20);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Colors::Tone07);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Colors::Tone11);
            ImGui::PushStyleColor(ImGuiCol_Text, hasAsset ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : Colors::ErrorSoft);

            if(ImGui::Button(buttonText.c_str(), ImVec2(slotWidth, 0.0f)) && hasAsset)
            {
                Editor* editor = static_cast<Editor*>(Core::GetClient());
                editor->GetPanelManager().GetPanel<ContentBrowserPanel>()->SetSelectedAsset(assetPtr->GetID());
            }
            ImGui::PopStyleColor(5);
            EditorUI::DelayedToolTip(hasAsset ? "Click: show in the Content Browser\nDrop another asset here to replace it" : "Drop an asset from the Content Browser");

            if(ImGui::BeginDragDropTarget())
            {
                if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_PAYLOAD))
                {
                    SG_ASSERT(payload->DataSize == sizeof(AssetID), "Payload size mismatch!");
                    const AssetID droppedAssetID = *static_cast<const AssetID*>(payload->Data);
                    if(am->GetMetadata(droppedAssetID).Type == expectedType)
                    {
                        assetPtr = am->Load<T>(droppedAssetID);
                        modified = true;
                    }
                    else
                        EditorStatusBar::Get().Notify(Severity::Warn, std::format("{} expects a {} asset", label, SurgeReflect::EnumToString(expectedType)));
                }
                ImGui::EndDragDropTarget();
            }

            if(hasAsset)
            {
                ImGui::SameLine(0.0f, spacing);
                ImGui::PushStyleColor(ImGuiCol_Button, Colors::Tone20);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Colors::ErrorDark);
                ImGui::PushStyleColor(ImGuiCol_Border, Colors::Tone30);
                ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));
                if(ImGui::Button("x", ImVec2(clearWidth, 0.0f)))
                {
                    assetPtr = nullptr;
                    modified = true;
                }
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(3);
                EditorUI::DelayedToolTip("Clear");
            }

            ImGui::PopStyleVar(3);
            ImGui::PopID();
            return modified;
        }

        // ------------------------------------------------------------------ Component header + options

        template <typename XComponent>
        bool DrawComponentMenu(Entity entity, XComponent& component, const char* name, bool removable)
        {
            const bool playing = static_cast<Editor*>(Core::GetClient())->IsPlaying();
            bool remove = false;

            EditorUI::MenuHeader(name);

            if constexpr(std::is_same_v<XComponent, TransformComponent>)
            {
                if(EditorUI::StyledMenuItem("Reset"))
                    ResetComponent(component);
                if(EditorUI::StyledMenuItem("Reset Position"))
                {
                    component.Position = glm::vec3(0.0f);
                    component.MarkDirty();
                }
                if(EditorUI::StyledMenuItem("Reset Rotation"))
                {
                    component.Rotation = glm::vec3(0.0f);
                    component.MarkDirty();
                }
                if(EditorUI::StyledMenuItem("Reset Scale"))
                {
                    component.Scale = glm::vec3(1.0f);
                    component.MarkDirty();
                }
                EditorUI::StyledSeparator();
            }
            else if constexpr(IsResettable_v<XComponent>)
            {
                if(EditorUI::StyledMenuItem("Reset", nullptr, nullptr, !playing))
                    ResetComponent(component);
                EditorUI::StyledSeparator();
            }

            if(EditorUI::StyledMenuItem("Copy"))
                CopyComponent<XComponent>(entity, name);
            const bool canPaste = !playing && sComponentClipboard.IsValid() && sComponentClipboard.Key == ComponentKey<XComponent>();
            if(EditorUI::StyledMenuItem("Paste Values", nullptr, nullptr, canPaste))
                PasteComponent(entity);

            if(removable)
            {
                EditorUI::StyledSeparator();
                ImGui::PushStyleColor(ImGuiCol_Text, Colors::Error);
                if(EditorUI::StyledMenuItem("Remove Component"))
                    remove = true;
                ImGui::PopStyleColor();
            }
            return remove;
        }

        template <typename XComponent, bool IsRemoveable = true, typename Func>
        void DrawComponent(Entity& entity, const char* name, Func&& function)
        {
            const int64_t hash = SurgeReflect::GetReflection<XComponent>()->GetHash();
            ImGui::PushID(static_cast<int>(hash));

            XComponent& component = entity.GetComponent<XComponent>();
            ComponentStyle style = StyleOf<XComponent>();
            if constexpr(std::is_same_v<XComponent, LightComponent>)
                style.Icon = component.Type == LightType::DIRECTIONAL ? GizmoIcon::DIRECTIONAL_LIGHT : GizmoIcon::POINT_LIGHT;

            bool active = true;
            if constexpr(HasActiveProperty_v<XComponent>)
                active = component.Active;

            // Header: framed tree node (keeps the open state per component type), everything else is drawn on top of it
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 5.0f));

            if constexpr(HasActiveProperty_v<XComponent>)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 5.0f));
                ImGui::Checkbox("##Active", &component.Active);
                EditorUI::DelayedToolTip(component.Active ? "Enabled" : "Disabled");
                ImGui::PopStyleVar();
                ImGui::SameLine(0.0f, 8.0f);
            }

            const float labelX = ImGui::GetCursorScreenPos().x + ImGui::GetTreeNodeToLabelSpacing();
            const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth |
                                             ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_FramePadding;
            const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(hash), flags, "%s", ""); // Label drawn below, with the icon
            const bool openMenuFromHeader = ImGui::IsItemClicked(ImGuiMouseButton_Right);
            const ImVec2 headerMin = ImGui::GetItemRectMin();
            const ImVec2 headerMax = ImGui::GetItemRectMax();

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const float fontSize = ImGui::GetFontSize();
            const float centerY = (headerMin.y + headerMax.y) * 0.5f;
            const float headerHeight = headerMax.y - headerMin.y;

            // Component color fading out from the right edge, glyph and name
            {
                const ImU32 gradientColor = active ? style.Color : ToU32(Colors::Tone43);
                const ImU32 left = WithAlpha(gradientColor, 0.0f);
                const ImU32 right = WithAlpha(gradientColor, HEADER_GRADIENT_ALPHA);
                EditorUI::AddHorizontalGradient(drawList, ImVec2(headerMax.x - (headerMax.x - headerMin.x) * HEADER_GRADIENT_WIDTH, headerMin.y), headerMax, left, right);
            }
            GizmoIcons::DrawGlyph(drawList, ImVec2(labelX + fontSize * 0.5f, centerY), fontSize * 0.9f, style.Icon, active ? style.Color : ToU32(Colors::Tone43), ImGui::GetColorU32(ImGuiCol_Header));
            {
                EditorUI::ScopedBoldFont font;
                const ImU32 textColor = ImGui::GetColorU32(active ? ImGuiCol_Text : ImGuiCol_TextDisabled);
                drawList->AddText(ImVec2(labelX + fontSize + 7.0f, centerY - fontSize * 0.5f), textColor, name);
            }

            // Right side: active toggle, options
            bool openMenu = openMenuFromHeader;
            float rightX = headerMax.x - 2.0f;
            {
                const float buttonSize = headerHeight - 4.0f;
                rightX -= buttonSize;
                ImGui::SetCursorScreenPos(ImVec2(rightX, headerMin.y + 2.0f));
                if(EditorUI::KebabButton("##Options", buttonSize))
                    openMenu = true;
            }
            ImGui::SetCursorScreenPos(ImVec2(headerMin.x, headerMax.y + ImGui::GetStyle().ItemSpacing.y));
            ImGui::PopStyleVar();

            if(openMenu)
                ImGui::OpenPopup("##ComponentMenu");

            bool removeComponent = false;
            EditorUI::StyledPopupVars::Push();
            if(ImGui::BeginPopup("##ComponentMenu"))
            {
                removeComponent = DrawComponentMenu(entity, component, name, IsRemoveable);
                EditorUI::EndStyledPopup();
            }
            else
                EditorUI::StyledPopupVars::Pop();

            if(open)
            {
                ImGui::Dummy(ImVec2(0.0f, 1.0f));
                ImGui::Indent(6.0f);
                const float labelWidth = glm::clamp(ImGui::GetContentRegionAvail().x * 0.38f, 90.0f, 170.0f);
                if(ImGui::BeginTable("##ComponentTable", 2, ImGuiTableFlags_SizingFixedFit))
                {
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, labelWidth);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    if(!active)
                        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.6f); // Still editable, just reads as off
                    function();
                    if(!active)
                        ImGui::PopStyleVar();

                    ImGui::EndTable();
                }
                ImGui::Unindent(6.0f);
                ImGui::Dummy(ImVec2(0.0f, 6.0f));
                ImGui::TreePop();
            }
            else
                ImGui::Dummy(ImVec2(0.0f, 2.0f));

            if constexpr(IsRemoveable)
            {
                if(removeComponent)
                    Core::AddFrameEndCallback([entity]() mutable {
                        if(entity && entity.HasComponent<XComponent>())
                            entity.RemoveComponent<XComponent>();
                    });
            }

            ImGui::PopID();
        }

        // ------------------------------------------------------------------ Add Component catalog

        struct AddableComponent
        {
            const char* Name;
            const char* Category;
            ComponentStyle Style;
            bool (*Has)(Entity);
            void (*Add)(Entity);
            const char* (*MissingRequirement)(Entity); // nullptr = always addable
        };

        template <typename T>
        void AddDefault(Entity e) { e.AddComponent<T>(); }

        const char* RequiresMesh(Entity e) { return e.HasComponent<MeshComponent>() ? nullptr : "Requires a Mesh component"; }

        const AddableComponent sAddableComponents[] = {
            { "Camera", "RENDERING", StyleOf<CameraComponent>(), &HasComponentOf<CameraComponent>, &AddDefault<CameraComponent>, nullptr },
            { "Sprite Renderer", "RENDERING", StyleOf<SpriteRendererComponent>(), &HasComponentOf<SpriteRendererComponent>,
              [](Entity e) { e.AddComponent<SpriteRendererComponent>(glm::vec4(1.0f)); }, nullptr },
            { "Mesh", "RENDERING", StyleOf<MeshComponent>(), &HasComponentOf<MeshComponent>, &AddDefault<MeshComponent>, nullptr },
            { "Light", "RENDERING", StyleOf<LightComponent>(), &HasComponentOf<LightComponent>, &AddDefault<LightComponent>, nullptr },
            { "Environment", "RENDERING", StyleOf<EnvironmentComponent>(), &HasComponentOf<EnvironmentComponent>, &AddDefault<EnvironmentComponent>, nullptr },
            { "Renderer Settings", "RENDERING", StyleOf<RendererSettingsComponent>(), &HasComponentOf<RendererSettingsComponent>, &AddDefault<RendererSettingsComponent>, nullptr },
            { "Text", "RENDERING", StyleOf<TextComponent>(), &HasComponentOf<TextComponent>, &AddDefault<TextComponent>, nullptr },
            { "Particle System", "RENDERING", StyleOf<ParticleSystemComponent>(), &HasComponentOf<ParticleSystemComponent>, &AddDefault<ParticleSystemComponent>, nullptr },
            { "Animator", "RENDERING", StyleOf<AnimatorComponent>(), &HasComponentOf<AnimatorComponent>, &AddDefault<AnimatorComponent>, &RequiresMesh },

            { "Rigidbody", "PHYSICS", StyleOf<RigidbodyComponent>(), &HasComponentOf<RigidbodyComponent>, &AddDefault<RigidbodyComponent>, nullptr },
            { "Box Collider", "PHYSICS", StyleOf<BoxColliderComponent>(), &HasComponentOf<BoxColliderComponent>, &AddDefault<BoxColliderComponent>, nullptr },
            { "Sphere Collider", "PHYSICS", StyleOf<SphereColliderComponent>(), &HasComponentOf<SphereColliderComponent>, &AddDefault<SphereColliderComponent>, nullptr },
            { "Capsule Collider", "PHYSICS", StyleOf<CapsuleColliderComponent>(), &HasComponentOf<CapsuleColliderComponent>, &AddDefault<CapsuleColliderComponent>, nullptr },
            { "Cylinder Collider", "PHYSICS", StyleOf<CylinderColliderComponent>(), &HasComponentOf<CylinderColliderComponent>, &AddDefault<CylinderColliderComponent>, nullptr },
            { "Convex Collider", "PHYSICS", StyleOf<ConvexColliderComponent>(), &HasComponentOf<ConvexColliderComponent>, &AddDefault<ConvexColliderComponent>, &RequiresMesh },
            { "Mesh Collider", "PHYSICS", StyleOf<MeshColliderComponent>(), &HasComponentOf<MeshColliderComponent>, &AddDefault<MeshColliderComponent>, &RequiresMesh },

            { "Script", "SCRIPTING & UI", StyleOf<ScriptComponent>(), &HasComponentOf<ScriptComponent>,
              [](Entity e) { e.AddComponent<ScriptComponent>().Scripts.emplace_back(); }, nullptr }, // One empty slot to drop a script on
            { "UI Canvas", "SCRIPTING & UI", StyleOf<UICanvasComponent>(), &HasComponentOf<UICanvasComponent>, &AddDefault<UICanvasComponent>, nullptr },

            { "Audio Source", "AUDIO", StyleOf<AudioSourceComponent>(), &HasComponentOf<AudioSourceComponent>, &AddDefault<AudioSourceComponent>, nullptr },
            { "Audio Listener", "AUDIO", StyleOf<AudioListenerComponent>(), &HasComponentOf<AudioListenerComponent>, &AddDefault<AudioListenerComponent>, nullptr },
        };

        // One row of the Add Component list: glyph, name, and why it is unavailable. Returns true when clicked
        bool AddComponentRow(const char* id, const ComponentStyle& style, const char* label, const char* note, bool enabled, bool highlighted)
        {
            const float height = ImGui::GetFrameHeight();
            ImGui::BeginDisabled(!enabled);
            const bool clicked = ImGui::Selectable(id, highlighted, ImGuiSelectableFlags_None, ImVec2(0.0f, height));
            ImGui::EndDisabled();

            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const float fontSize = ImGui::GetFontSize();
            const float centerY = (min.y + max.y) * 0.5f;

            GizmoIcons::DrawGlyph(drawList, ImVec2(min.x + 4.0f + fontSize * 0.5f, centerY), fontSize * 0.85f, style.Icon, enabled ? style.Color : ToU32(Colors::Tone43), ImGui::GetColorU32(ImGuiCol_PopupBg));
            drawList->AddText(ImVec2(min.x + fontSize + 12.0f, centerY - fontSize * 0.5f), ImGui::GetColorU32(enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled), label);
            if(note)
            {
                const float noteWidth = ImGui::CalcTextSize(note).x;
                drawList->AddText(ImVec2(max.x - noteWidth - 4.0f, centerY - fontSize * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled), note);
            }
            return clicked;
        }

        bool ContainsCaseInsensitive(std::string_view text, std::string_view lowerNeedle)
        {
            if(lowerNeedle.empty())
                return true;
            auto it = std::search(text.begin(), text.end(), lowerNeedle.begin(), lowerNeedle.end(),
                                  [](char a, char b) { return ::tolower(static_cast<unsigned char>(a)) == b; });
            return it != text.end();
        }

        // ------------------------------------------------------------------ Mesh materials

        String sMaterialFilter; // Shown once a mesh has more slots than fit comfortably
        constexpr Uint MATERIAL_FILTER_THRESHOLD = 6;

        // An overridden slot's glTF material is released when the mesh loads: rebuilt from the cooked mesh before the override goes
        void ResetMaterialOverride(Ref<Mesh> mesh, Uint slot)
        {
            AssetManager* am = Core::GetAssetManager();
            if(!mesh->GetMaterials()[slot])
            {
                CookedFile file;
                MeshSpecification spec;
                bool read = CookedFileIO::Read(am->GetSidecarPath(mesh->GetID()), AssetType::MESH, MeshBinary::VERSION, file) == CookedFileError::NONE;
                if(read)
                {
                    Rapt0R::Reader payload = file.GetPayload();
                    read = MeshBinary::Read(am->GetContext(), payload, spec);
                }
                if(!read || slot >= spec.Materials.size() || !spec.Materials[slot])
                {
                    EditorStatusBar::Get().Notify(Severity::Error, "Could not read the mesh's own material, re-cook the mesh and try again");
                    return;
                }
                //(Rid) spec.Materials[slot] is the gLTF material which was originally assigned to this slot
                mesh->SetGLTFMaterial(slot, spec.Materials[slot]);
            }
            mesh->ClearMaterialOverride(slot);
            AssetDatabase::Get().Save(mesh->GetID()); // Its .import, then a re-cook
        }

        // Albedo map tinted by the albedo color (what the surface mostly looks like), the plain color without a map
        void DrawMaterialSwatch(Material* material, const ImVec2& min, float size)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 max(min.x + size, min.y + size);
            if(!material)
            {
                drawList->AddRectFilled(min, max, ImGui::GetColorU32(Colors::Tone20), 3.0f);
                drawList->AddRect(min, max, ToU32(Colors::ErrorMuted), 3.0f, 0, OUTLINE_THICKNESS);
                return;
            }

            const glm::vec3 albedo = glm::clamp(material->Get<glm::vec3>("Albedo"), glm::vec3(0.0f), glm::vec3(1.0f));
            const ImU32 tint = ImGui::GetColorU32(ImVec4(albedo.x, albedo.y, albedo.z, 1.0f));
            const Ref<Texture2D>& albedoMap = material->GetTexture("AlbedoMap");
            if(albedoMap && material->Get<int>("UseAlbedoMap"))
            {
                const ImTextureID image = static_cast<ImTextureID>(Core::GetRenderer()->GetRHI()->GetImGuiImage(albedoMap->GetRHIImage()));
                drawList->AddImageRounded(image, min, max, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), tint, 3.0f);
            }
            else
                drawList->AddRectFilled(min, max, tint, 3.0f);
            drawList->AddRect(min, max, ImGui::GetColorU32(Colors::Tone30), 3.0f, 0, 1.0f);
        }

        void DrawMaterialSlots(Ref<Mesh> mesh)
        {
            AssetManager* am = Core::GetAssetManager();
            Editor* editor = static_cast<Editor*>(Core::GetClient());
            const Uint slotCount = static_cast<Uint>(mesh->GetMaterials().size());
            const bool builtIn = HasFlag(am->GetMetadata(mesh->GetID()).Flags, AssetFlags::MEMORY);

            const String title = slotCount == 1 ? String("Material") : std::format("Materials ({})", slotCount);
            EditorUI::TSeparator(title.c_str());
            if(builtIn)
                EditorUI::THint("Built-in mesh: overrides change every entity using it and are not saved with the project", Colors::Warning, 14.0f, "Hint");

            Vector<Uint> submeshCounts(slotCount, 0);
            for(const Submesh& submesh : mesh->GetSubmeshes())
            {
                if(submesh.MaterialIndex < slotCount)
                    submeshCounts[submesh.MaterialIndex]++;
            }

            String filter;
            if(slotCount > MATERIAL_FILTER_THRESHOLD)
            {
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputTextWithHint("##MaterialFilter", "Filter by material name", &sMaterialFilter);
                filter = sMaterialFilter;
                std::transform(filter.begin(), filter.end(), filter.begin(), [](char c) { return static_cast<char>(::tolower(static_cast<unsigned char>(c))); });
            }

            // Highlight every slot while a material is dragged over the Inspector
            bool acceptsDrag = false;
            if(const ImGuiPayload* drag = ImGui::GetDragDropPayload(); drag && drag->IsDataType(CONTENT_BROWSER_PAYLOAD))
                acceptsDrag = am->GetMetadata(*static_cast<const AssetID*>(drag->Data)).Type == AssetType::MATERIAL;

            int resetSlot = -1;
            for(Uint i = 0; i < slotCount; i++)
            {
                Ref<Material> material = mesh->GetMaterialAtIndex(i);
                if(!filter.empty() && (!material || !ContainsCaseInsensitive(material->GetName(), filter)))
                    continue;

                const bool overridden = mesh->HasOverride(i);
                ImGui::PushID(static_cast<int>(i));

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(Colors::TextMuted, "Slot %u", i);
                EditorUI::DelayedToolTip(std::format("Used by {} submesh{}", submeshCounts[i], submeshCounts[i] == 1 ? "" : "es").c_str());
                ImGui::TableNextColumn();

                const float frameHeight = ImGui::GetFrameHeight();
                constexpr float spacing = 4.0f;

                DrawMaterialSwatch(material.Raw(), ImGui::GetCursorScreenPos(), frameHeight);
                ImGui::Dummy(ImVec2(frameHeight, frameHeight));
                ImGui::SameLine(0.0f, spacing);

                // Name on the left, where the material comes from on the right
                const float buttonWidth = ImGui::GetContentRegionAvail().x - (overridden ? frameHeight + spacing : 0.0f);
                const ImVec4 border = acceptsDrag ? Colors::Accent : (material ? Colors::Tone30 : Colors::ErrorMuted);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, OUTLINE_THICKNESS);
                ImGui::PushStyleColor(ImGuiCol_Border, border);
                ImGui::PushStyleColor(ImGuiCol_Button, Colors::Tone20);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Colors::Tone07);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, Colors::Tone11);
                const bool clicked = ImGui::Button("##Material", ImVec2(std::max(buttonWidth, frameHeight), 0.0f));
                ImGui::PopStyleColor(4);
                ImGui::PopStyleVar(2);

                const ImVec2 buttonMin = ImGui::GetItemRectMin();
                const ImVec2 buttonMax = ImGui::GetItemRectMax();
                {
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    const char* tag = !material ? "" : (overridden ? "OVERRIDE" : (material->GetID() ? "ASSET" : "glTF"));
                    const ImU32 tagColor = overridden ? ImGui::GetColorU32(Colors::Accent) : ImGui::GetColorU32(Colors::TextMuted);
                    const float tagWidth = ImGui::CalcTextSize(tag).x;
                    const float textY = (buttonMin.y + buttonMax.y - ImGui::GetTextLineHeight()) * 0.5f;
                    const float padding = ImGui::GetStyle().FramePadding.x;

                    drawList->AddText(ImVec2(buttonMax.x - padding - tagWidth, textY), tagColor, tag);

                    drawList->PushClipRect(buttonMin, ImVec2(buttonMax.x - padding - tagWidth - 8.0f, buttonMax.y), true);
                    {
                        EditorUI::ScopedBoldFont bold;
                        const char* name = material ? material->GetName().c_str() : "Missing material";
                        const ImU32 nameColor = material ? ImGui::GetColorU32(ImGuiCol_Text) : ToU32(Colors::ErrorSoft);
                        drawList->AddText(ImVec2(buttonMin.x + padding, (buttonMin.y + buttonMax.y - ImGui::GetTextLineHeight()) * 0.5f), nameColor, name);
                    }
                    drawList->PopClipRect();
                }

                if(clicked && material)
                    editor->GetPanelManager().GetPanel<MaterialEditorPanel>()->OpenMaterial(material, mesh, i);
                EditorUI::DelayedToolTip(material ? "Click: edit in the Material Editor, right click: more\nDrop a MATERIAL here to override this slot for every entity using this mesh"
                                                  : "Drop a MATERIAL here to fill this slot");

                if(ImGui::BeginDragDropTarget())
                {
                    if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_PAYLOAD))
                    {
                        SG_ASSERT(payload->DataSize == sizeof(AssetID), "Payload size mismatch!");
                        const AssetID dropped = *static_cast<const AssetID*>(payload->Data);
                        if(am->GetMetadata(dropped).Type == AssetType::MATERIAL)
                        {
                            if(Ref<Material> newMaterial = am->Load<Material>(dropped))
                            {
                                mesh->SetMaterialOverride(i, newMaterial);
                                AssetDatabase::Get().Save(mesh->GetID()); // Only the mesh (its .import + a re-cook): saving the material would also write its unsaved Material Editor edits
                            }
                        }
                        else
                            EditorStatusBar::Get().Notify(Severity::Warn, "Mesh slots take MATERIAL assets");
                    }
                    ImGui::EndDragDropTarget();
                }

                if(EditorUI::BeginStyledPopupContextItem("##SlotMenu"))
                {
                    if(EditorUI::StyledMenuItem("Edit in Material Editor", nullptr, nullptr, static_cast<bool>(material)))
                        editor->GetPanelManager().GetPanel<MaterialEditorPanel>()->OpenMaterial(material, mesh, i);
                    if(EditorUI::StyledMenuItem("Show in Content Browser", nullptr, nullptr, material && material->GetID()))
                        editor->GetPanelManager().GetPanel<ContentBrowserPanel>()->SetSelectedAsset(material->GetID());
                    EditorUI::StyledSeparator();
                    if(EditorUI::StyledMenuItem("Reset to Mesh Material", nullptr, nullptr, overridden))
                        resetSlot = static_cast<int>(i);
                    EditorUI::EndStyledPopup();
                }

                if(overridden)
                {
                    ImGui::SameLine(0.0f, spacing);
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, OUTLINE_THICKNESS);
                    ImGui::PushStyleColor(ImGuiCol_Button, Colors::Tone20);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Colors::ErrorDark);
                    ImGui::PushStyleColor(ImGuiCol_Border, Colors::Tone30);
                    if(ImGui::Button("x", ImVec2(frameHeight, frameHeight)))
                        resetSlot = static_cast<int>(i);
                    ImGui::PopStyleColor(3);
                    ImGui::PopStyleVar(2);
                    EditorUI::DelayedToolTip("Remove the override: back to the mesh's own material");
                }

                ImGui::PopID();
            }

            if(resetSlot >= 0)
                ResetMaterialOverride(mesh, static_cast<Uint>(resetSlot));
        }
    } // namespace

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
            if(!entity)
            {
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + glm::max(ImGui::GetContentRegionAvail().y * 0.35f, 8.0f));
                {
                    EditorUI::ScopedBoldFont font;
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                    EditorUI::TextCentered("Nothing selected");
                    ImGui::PopStyleColor();
                }
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                EditorUI::TextCentered("Select an entity in the Hierarchy or a Scene view");
                ImGui::PopStyleColor();
            }
            else
            {
                DrawEntityHeader(entity);
                DrawComponents(entity);
                DrawAddComponentButton(entity);
            }
        }
        ImGui::End();
    }

    void InspectorPanel::DrawEntityHeader(Entity& entity)
    {
        const GizmoIcons::EntityIcon icon = GizmoIcons::GetEntityIcon(entity);
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Type glyph
        const float badgeSize = 44.0f;
        const ImVec2 start = ImGui::GetCursorScreenPos();
        const ImVec2 badgeMax(start.x + badgeSize, start.y + badgeSize);
        GizmoIcons::DrawGlyph(drawList, ImVec2(start.x + badgeSize * 0.5f, start.y + badgeSize * 0.5f), badgeSize * 0.7f, icon.Icon, icon.Color, ImGui::GetColorU32(ImGuiCol_WindowBg));

        // Name, ID and component 
        ImGui::SetCursorScreenPos(ImVec2(badgeMax.x + 8.0f, start.y));
        ImGui::BeginGroup();
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 4.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
            ImGui::SetNextItemWidth(-FLT_MIN);
            {
                EditorUI::ScopedBoldFont font;
                ImGui::InputTextWithHint("##EntityName", "Name", &entity.GetComponent<NameComponent>().Name);
            }
            ImGui::PopStyleVar(2);

            const uint64_t id = entity.GetComponent<IDComponent>().ID.Get();
            ImGui::TextDisabled("ID %llu", static_cast<unsigned long long>(id));
            if(ImGui::IsItemHovered())
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                EditorUI::DelayedToolTip("Click to copy");
            }
            if(ImGui::IsItemClicked())
            {
                ImGui::SetClipboardText(std::to_string(id).c_str());
                EditorStatusBar::Get().Notify(Severity::Info, "Entity ID copied to the clipboard", 2.0f);
            }

            const int count = CountVisibleComponents(entity);
            ImGui::SameLine();
            ImGui::TextDisabled("|  %d component%s", count, count == 1 ? "" : "s");
        }
        ImGui::EndGroup();

        // Below whichever is taller
        ImGui::SetCursorScreenPos(ImVec2(start.x, glm::max(badgeMax.y, ImGui::GetItemRectMax().y) + 6.0f));

        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
    }

    void InspectorPanel::DrawAddComponentButton(Entity& entity)
    {
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        const float width = glm::min(ImGui::GetContentRegionAvail().x, 240.0f);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - width) * 0.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 7.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, Colors::Tone20);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Colors::Tone07);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Colors::Tone11);
        bool openPopup = false;
        {
            EditorUI::ScopedBoldFont font;
            openPopup = ImGui::Button("+  Add Component", ImVec2(width, 0.0f));
        }
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);

        if(openPopup)
        {
            mAddComponentSearch.clear();
            ImGui::OpenPopup("##AddComponent");
        }

        // Below the button, as wide as it
        const ImVec2 buttonMin = ImGui::GetItemRectMin();
        ImGui::SetNextWindowPos(ImVec2(buttonMin.x, ImGui::GetItemRectMax().y + 2.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f), ImVec2(glm::max(width, 260.0f), 420.0f));

        EditorUI::StyledPopupVars::Push();
        if(!ImGui::BeginPopup("##AddComponent"))
        {
            EditorUI::StyledPopupVars::Pop();
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            return;
        }

        if(ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-FLT_MIN);
        const bool enterPressed = ImGui::InputTextWithHint("##Search", "Search components...", &mAddComponentSearch, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SetNavCursorVisible(false);

        String needle = mAddComponentSearch;
        std::transform(needle.begin(), needle.end(), needle.begin(), [](char c) { return static_cast<char>(::tolower(static_cast<unsigned char>(c))); });
        const bool searching = !needle.empty();

        // The first addable match is highlighted and taken by Enter
        const AddableComponent* firstMatch = nullptr;
        for(const AddableComponent& item : sAddableComponents)
        {
            if(ContainsCaseInsensitive(item.Name, needle) && !item.Has(entity) && !(item.MissingRequirement && item.MissingRequirement(entity)))
            {
                firstMatch = &item;
                break;
            }
        }

        const AddableComponent* toAdd = nullptr;

        // A copied component this entity does not have yet
        if(sComponentClipboard.IsValid() && sComponentClipboard.Has && !sComponentClipboard.Has(entity) && !static_cast<Editor*>(Core::GetClient())->IsPlaying())
        {
            const String label = std::format("Paste {}", sComponentClipboard.DisplayName);
            if(ContainsCaseInsensitive(label, needle))
            {
                EditorUI::StyledSeparator();
                if(AddComponentRow("##PasteComponent", ComponentStyle { GizmoIcon::ENTITY, ToU32(Colors::Accent) }, label.c_str(), "CLIPBOARD", true, false))
                    PasteComponent(entity);
            }
        }

        const char* category = nullptr;
        bool anyShown = false;
        for(const AddableComponent& item : sAddableComponents)
        {
            if(!ContainsCaseInsensitive(item.Name, needle))
                continue;

            if(!searching && (!category || strcmp(category, item.Category) != 0))
            {
                EditorUI::ScopedBoldFont font;
                category = item.Category;
                ImGui::TextColored(Colors::Hint, "%s", item.Category);
                EditorUI::StyledSeparator();
            }
            else if(searching && !anyShown)
                EditorUI::StyledSeparator();

            anyShown = true;

            const bool has = item.Has(entity);
            const char* missing = (!has && item.MissingRequirement) ? item.MissingRequirement(entity) : nullptr;
            ImGui::PushID(item.Name);
            if(AddComponentRow("##Item", item.Style, item.Name, has ? "ADDED" : nullptr, !has && !missing, &item == firstMatch && searching))
                toAdd = &item;
            if(missing)
                EditorUI::ItemTooltip(missing);
            ImGui::PopID();
        }

        if(!anyShown)
            ImGui::TextDisabled("No component matches \"%s\"", mAddComponentSearch.c_str());

        if(enterPressed && firstMatch)
            toAdd = firstMatch;
        if(toAdd)
        {
            toAdd->Add(entity);
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
        EditorUI::StyledPopupVars::Pop();
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
    }

    void InspectorPanel::DrawComponents(Entity& entity)
    {
        if(entity.HasComponent<TransformComponent>())
        {
            TransformComponent& component = entity.GetComponent<TransformComponent>();
            DrawComponent<TransformComponent, false>(entity, "Transform", [&component, this]() {
                if(EditorUI::TVec3Control("Position", component.Position, 0.0f, 0.05f))
                    component.MarkDirty();
                if(EditorUI::TVec3Control("Rotation", component.Rotation, 0.0f, 0.5f))
                    component.MarkDirty();
                if(EditorUI::TVec3Control("Scale", component.Scale, 1.0f, 0.01f, &mUniformScale))
                    component.MarkDirty();
            });
        }

        if(entity.HasComponent<CameraComponent>())
        {
            CameraComponent& component = entity.GetComponent<CameraComponent>();
            DrawComponent<CameraComponent>(entity, "Camera", [&component]() {
                RuntimeCamera& camera = component.Camera;
                EditorUI::TProperty<bool>("Primary", &component.Primary);

                RuntimeCamera::ProjectionType projection = camera.GetProjectionType();
                constexpr auto projectionTypeStrings = std::array { "PERSPECTIVE", "ORTHOGRAPHIC" };
                if(EditorUI::TComboBox("Projection", projection, projectionTypeStrings))
                    camera.SetProjectionType(projection);

                if(camera.GetProjectionType() == RuntimeCamera::ProjectionType::Perspective)
                {
                    float verticalFOV = camera.GetPerspectiveVerticalFOV();
                    if(EditorUI::TProperty<float>("Vertical FOV", &verticalFOV)) // In degree
                        camera.SetPerspectiveVerticalFOV(verticalFOV);

                    float nearClip = camera.GetPerspectiveNearClip();
                    if(EditorUI::TProperty<float>("Near Clip", &nearClip))
                        camera.SetPerspectiveNearClip(nearClip);

                    float farClip = camera.GetPerspectiveFarClip();
                    if(EditorUI::TProperty<float>("Far Clip", &farClip))
                        camera.SetPerspectiveFarClip(farClip);
                }
                else
                {
                    float orthoSize = camera.GetOrthographicSize();
                    if(EditorUI::TProperty<float>("Size", &orthoSize))
                        camera.SetOrthographicSize(orthoSize);

                    float nearClip = camera.GetOrthographicNearClip();
                    if(EditorUI::TProperty<float>("Near Clip", &nearClip))
                        camera.SetOrthographicNearClip(nearClip);

                    float farClip = camera.GetOrthographicFarClip();
                    if(EditorUI::TProperty<float>("Far Clip", &farClip))
                        camera.SetOrthographicFarClip(farClip);
                }

                EditorUI::TSeparator("Aspect");
                EditorUI::TProperty<bool>("Fixed Aspect Ratio", &component.FixedAspectRatio);
                if(component.FixedAspectRatio)
                {
                    glm::vec2 aspectRatio = camera.GetAspectRatio();
                    if(EditorUI::TProperty<glm::vec2>("Aspect Ratio", &aspectRatio))
                        camera.SetAspectRatio(aspectRatio);
                }
            });
        }

        if(entity.HasComponent<SpriteRendererComponent>())
        {
            SpriteRendererComponent& component = entity.GetComponent<SpriteRendererComponent>();
            DrawComponent<SpriteRendererComponent>(entity, "Sprite Renderer", [&component]() {
                DrawAssetDropSlot<Texture2D>("Texture", component.TextureAsset, AssetType::TEXTURE2D, "None (Texture2D)");
                EditorUI::TProperty<glm::vec4, EditorUI::CustomPropertyFlag::Color4>("Color", &component.Color);
                EditorUI::TProperty<bool>("Billboard", &component.Billboard);
            });
        }

        if(entity.HasComponent<MeshComponent>())
        {
            MeshComponent& component = entity.GetComponent<MeshComponent>();
            DrawComponent<MeshComponent>(entity, "Mesh", [&component]() {
                DrawAssetDropSlot<Mesh>("Mesh", component.MeshAsset, AssetType::MESH, "None (Mesh)");
                if(!component.MeshAsset)
                    return;

                EditorUI::TProperty<bool>("Drop Shadow", &component.DropShadow);
                DrawMaterialSlots(component.MeshAsset.As<Mesh>());
            });
        }

        if(entity.HasComponent<LightComponent>())
        {
            LightComponent& component = entity.GetComponent<LightComponent>();
            DrawComponent<LightComponent>(entity, "Light", [&component]() {
                constexpr auto lightTypeStrings = std::array { "DIRECTIONAL", "POINT" };
                EditorUI::TComboBox("Type", component.Type, lightTypeStrings);
                EditorUI::TProperty<glm::vec3, EditorUI::CustomPropertyFlag::Color3>("Color", &component.Color);
                EditorUI::TProperty<float>("Intensity", &component.Intensity, 0.0f, 1000.0f);

                if(component.Type == LightType::POINT)
                {
                    EditorUI::TProperty<float>("Radius", &component.Radius, 0.0f, 10000.0f);
                    EditorUI::TSlider<float>("Falloff", &component.Falloff, 0.1f, 2.0f);
                }
                else
                    EditorUI::THint("Direction follows the entity's rotation", Colors::TextMuted, 14.0f, "Hint");
            });
        }

        if(entity.HasComponent<EnvironmentComponent>())
        {
            EnvironmentComponent& component = entity.GetComponent<EnvironmentComponent>();
            DrawComponent<EnvironmentComponent>(entity, "Environment", [&component]() {
                EditorUI::TSeparator("Sun Position");
                EditorUI::TProperty<float>("Azimuth", &component.Azimuth);
                EditorUI::TProperty<float>("Elevation", &component.Elevation);

                EditorUI::TSeparator("Sky");
                EditorUI::TProperty<bool>("Sun Disk", &component.EnableSunDisk);
                EditorUI::TSlider<float>("Turbidity", &component.Turbidity, 1.5f, 10.0f, "%.2f");
                EditorUI::TSlider<float>("Exposure", &component.Exposure, 0.001f, 0.1f, "%.4f");
                EditorUI::TSlider<float>("Sun Intensity", &component.SunIntensity, 0.01f, 1.0f, "%.2f");

                EditorUI::TSeparator("Ambient (Fast GI)");
                EditorUI::TProperty<glm::vec3, EditorUI::CustomPropertyFlag::Color3>("Sky", &component.SkyAmbient);
                EditorUI::TProperty<glm::vec3, EditorUI::CustomPropertyFlag::Color3>("Horizon", &component.HorizonAmbient);
                EditorUI::TProperty<glm::vec3, EditorUI::CustomPropertyFlag::Color3>("Ground", &component.GroundAmbient);
            });
        }

        if(entity.HasComponent<RendererSettingsComponent>())
        {
            RendererSettingsComponent& component = entity.GetComponent<RendererSettingsComponent>();
            DrawComponent<RendererSettingsComponent>(entity, "Renderer Settings", [&component, &entity, this]() {
                mRendererSettings.Draw(entity, component);
            });
        }

        if(entity.HasComponent<RigidbodyComponent>())
        {
            RigidbodyComponent& component = entity.GetComponent<RigidbodyComponent>();
            DrawComponent<RigidbodyComponent>(entity, "Rigidbody", [&entity, &component]() {

                if (entity.GetScene()->IsRunning())
                    EditorUI::THint("Editing Rigidbody fields in the Inspector during Play does not update the live body (they are read at creation). Lua's RigidbodyC.Layer is the exception.", Colors::TextMuted, 14.0f, "Hint");

                constexpr auto rbTypeStrings = std::array { "STATIC", "DYNAMIC", "KINEMATIC" };
                EditorUI::TComboBox("Type", component.Type, rbTypeStrings);

                // Named layers only (Physics panel); a body on a layer that was renamed away still shows its index
                const PhysicsSettings& physicsSettings = static_cast<Editor*>(Core::GetClient())->GetPhysicsSettings();
                const String& layerName = physicsSettings.LayerNames[PhysicsSettings::ClampLayer(component.Layer)];
                const String preview = layerName.empty() ? std::format("Layer {}", component.Layer) : std::format("{} [{}]", layerName, component.Layer);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Layer");
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1.0f);
                if(ImGui::BeginCombo("##Layer", preview.c_str()))
                {
                    for(int i = 0; i < PhysicsSettings::MAX_LAYERS; i++)
                    {
                        if(physicsSettings.LayerNames[i].empty())
                            continue;

                        ImGui::PushID(i);
                        if(ImGui::Selectable(std::format("{} [{}]", physicsSettings.LayerNames[i], i).c_str(), component.Layer == i))
                            component.Layer = i;
                        ImGui::PopID();
                    }
                    ImGui::EndCombo();
                }

                if(component.Type != RigidbodyType::STATIC)
                    EditorUI::TProperty<float>("Mass", &component.Mass, 0.0f, 100000.0f);

                EditorUI::TSeparator("Simulation");
                EditorUI::TProperty<bool>("Interpolate", &component.Interpolate);
                EditorUI::TProperty<bool>("Use Gravity", &component.UseGravity);
                EditorUI::TProperty<bool>("Is Sensor", &component.IsSensor);
                EditorUI::TProperty<bool>("Continuous (CCD)", &component.ContinuousCollision);

                EditorUI::TSeparator("Surface");
                EditorUI::TProperty<float>("Friction", &component.Friction, 0.0f, 10.0f);
                EditorUI::TProperty<float>("Bounciness", &component.Bounciness, 0.0f, 1.0f);

                if(component.Type != RigidbodyType::STATIC)
                {
                    EditorUI::TSeparator("Damping");
                    EditorUI::TProperty<float>("Linear", &component.LinearDamping, 0.0f, 100.0f);
                    EditorUI::TProperty<float>("Angular", &component.AngularDamping, 0.0f, 100.0f);

                    EditorUI::TSeparator("Freeze Rotation");
                    EditorUI::TProperty<bool>("X Axis", &component.FreezeRotationX);
                    EditorUI::TProperty<bool>("Y Axis", &component.FreezeRotationY);
                    EditorUI::TProperty<bool>("Z Axis", &component.FreezeRotationZ);
                }
            });
        }

        if(entity.HasComponent<BoxColliderComponent>())
        {
            BoxColliderComponent& component = entity.GetComponent<BoxColliderComponent>();
            DrawComponent<BoxColliderComponent>(entity, "Box Collider", [&component]() {
                EditorUI::TProperty<glm::vec3>("Half Extents", &component.HalfExtents, 0.0f, 10000.0f);
                EditorUI::TProperty<bool>("Show Collider", &component.ShowCollider);
            });
        }

        if(entity.HasComponent<SphereColliderComponent>())
        {
            SphereColliderComponent& component = entity.GetComponent<SphereColliderComponent>();
            DrawComponent<SphereColliderComponent>(entity, "Sphere Collider", [&component]() {
                EditorUI::TProperty<float>("Radius", &component.Radius, 0.0f, 10000.0f);
                EditorUI::TProperty<bool>("Show Collider", &component.ShowCollider);
            });
        }

        if(entity.HasComponent<CapsuleColliderComponent>())
        {
            CapsuleColliderComponent& component = entity.GetComponent<CapsuleColliderComponent>();
            DrawComponent<CapsuleColliderComponent>(entity, "Capsule Collider", [&component]() {
                EditorUI::TProperty<float>("Radius", &component.Radius, 0.0f, 10000.0f);
                EditorUI::TProperty<float>("Height", &component.Height, 0.0f, 10000.0f);
                EditorUI::TProperty<bool>("Show Collider", &component.ShowCollider);
            });
        }

        if(entity.HasComponent<CylinderColliderComponent>())
        {
            CylinderColliderComponent& component = entity.GetComponent<CylinderColliderComponent>();
            DrawComponent<CylinderColliderComponent>(entity, "Cylinder Collider", [&component]() {
                EditorUI::TProperty<float>("Radius", &component.Radius, 0.0f, 10000.0f);
                EditorUI::TProperty<float>("Height", &component.Height, 0.0f, 10000.0f);
                EditorUI::TProperty<bool>("Show Collider", &component.ShowCollider);
            });
        }

        if(entity.HasComponent<ConvexColliderComponent>())
        {
            ConvexColliderComponent& component = entity.GetComponent<ConvexColliderComponent>();
            DrawComponent<ConvexColliderComponent>(entity, "Convex Collider", [&component, &entity]() {
                if(entity.HasComponent<MeshComponent>())
                {
                    if(EditorUI::TProperty<glm::vec3>("Local Offset", &component.LocalOffset))
                        component.IsDirty = true;
                    if(EditorUI::TProperty<glm::vec3>("Local Rotation", &component.LocalRotation))
                        component.IsDirty = true;
                }
                else
                    EditorUI::THint("The hull is built from the Mesh component, add one to this entity", Colors::Warning, 14.0f, "Hint");
                EditorUI::TProperty<bool>("Show Collider", &component.ShowCollider);
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
                        EditorUI::TProperty<glm::vec3>("Local Offset", &component.LocalOffset);
                        EditorUI::TProperty<glm::vec3>("Local Rotation", &component.LocalRotation);
                    }
                    EditorUI::TProperty<bool>("Show Collider", &component.ShowCollider);
                    EditorUI::THint("Generated from the mesh triangles, expensive to visualize: turn Show Collider off when done", Colors::TextMuted, 14.0f, "Hint");
                }
                else
                    EditorUI::THint("Invalid setup: needs a STATIC or KINEMATIC Rigidbody on this entity", Colors::Error, 14.0f, "Hint");
            });
        }

        if(entity.HasComponent<ScriptComponent>())
        {
            ScriptComponent& component = entity.GetComponent<ScriptComponent>();
            DrawComponent<ScriptComponent>(entity, "Scripts", [&component, &entity]() {
                const bool playing = static_cast<Editor*>(Core::GetClient())->IsPlaying();
                int removeIndex = -1;
                for(size_t i = 0; i < component.Scripts.size(); i++)
                {
                    ScriptEntry& entry = component.Scripts[i];
                    ImGui::PushID(static_cast<int>(i));

                    const String title = entry.ScriptAsset ? ScriptEngine::GetScriptName(entry.ScriptAsset) : String("Empty slot");
                    EditorUI::TSeparator(title.c_str());

                    // Enabled toggles OnEnable/OnDisable while playing; scripts are only added/removed in edit mode
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted("Enabled");
                    ImGui::TableNextColumn();
                    ImGui::Checkbox("##Active", &entry.Active);
                    if(!playing)
                    {
                        const float removeWidth = ImGui::CalcTextSize("Remove").x + ImGui::GetStyle().FramePadding.x * 2.0f;
                        ImGui::SameLine(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - removeWidth);
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Colors::ErrorDark);
                        if(ImGui::Button("Remove"))
                            removeIndex = static_cast<int>(i);
                        ImGui::PopStyleColor();
                    }

                    DrawAssetDropSlot<Script>("Script", entry.ScriptAsset, AssetType::SCRIPT, "None (Script)");
                    const bool live = playing && entry.IsInstantiated;
                    ScriptFields::DrawInspector(entry.ScriptAsset, entry.Fields, live ? &entry.Instance : nullptr, entity.GetScene());
                    ImGui::PopID();
                }
                if(removeIndex >= 0)
                    component.Scripts.erase(component.Scripts.begin() + removeIndex);

                if(!playing)
                {
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    if(ImGui::Button("+ Add Script", ImVec2(-FLT_MIN, 0.0f)))
                        component.Scripts.emplace_back();
                    EditorUI::ItemTooltip("Adds an empty slot, or drop a SCRIPT from the Content Browser here");
                    if(ImGui::BeginDragDropTarget())
                    {
                        if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_PAYLOAD))
                        {
                            AssetManager* am = Core::GetAssetManager();
                            const AssetID dropped = *static_cast<const AssetID*>(payload->Data);
                            if(am->GetMetadata(dropped).Type == AssetType::SCRIPT)
                                component.Scripts.emplace_back().ScriptAsset = am->Load<Script>(dropped);
                            else
                                EditorStatusBar::Get().Notify(Severity::Warn, "Only SCRIPT assets can be added to a Script component");
                        }
                        ImGui::EndDragDropTarget();
                    }
                }
            });
        }

        if(entity.HasComponent<TextComponent>())
        {
            TextComponent& component = entity.GetComponent<TextComponent>();
            DrawComponent<TextComponent>(entity, "Text", [&component]() {
                DrawAssetDropSlot<Font>("Font", component.FontAsset, AssetType::FONT, "None (Font)");
                if(!component.FontAsset)
                {
                    EditorUI::THint("Drop a FONT to edit the text", Colors::TextMuted, 14.0f, "Hint");
                    return;
                }

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Text");
                ImGui::TableNextColumn();
                ImGui::InputTextMultiline("##Text", &component.Text, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 3.5f));
                EditorUI::TProperty<glm::vec4, EditorUI::CustomPropertyFlag::Color4>("Color", &component.Color);
                EditorUI::TProperty<float>("Max Width", &component.MaxWidth, 0.0f, 100000.0f);

                EditorUI::TSeparator("Layout");
                EditorUI::TProperty<float>("Letter Spacing", &component.LetterSpacing);
                EditorUI::TProperty<float>("Line Spacing", &component.LineSpacing);
                constexpr auto alignTypeStrings = std::array { "LEFT", "CENTER", "RIGHT" };
                EditorUI::TComboBox("Horizontal Align", component.Alignment, alignTypeStrings);
                constexpr auto verticalAlignTypeStrings = std::array { "TOP", "CENTER", "BASELINE", "BOTTOM" };
                EditorUI::TComboBox("Vertical Align", component.VerticalAlignment, verticalAlignTypeStrings);

                EditorUI::TSeparator("Style");
                EditorUI::TProperty<bool>("Underline", &component.Underline);
                EditorUI::TProperty<bool>("Italic", &component.Italic);
                EditorUI::TProperty<bool>("Billboard", &component.Billboard);

                EditorUI::TSeparator("Shadow");
                ImGui::PushID("Shadow");
                EditorUI::TProperty<bool>("Enabled", &component.ShadowEnabled);
                if(component.ShadowEnabled)
                {
                    EditorUI::TProperty<glm::vec4, EditorUI::CustomPropertyFlag::Color4>("Color", &component.ShadowColor);
                    EditorUI::TProperty<glm::vec2>("Offset", &component.ShadowOffset);
                }
                ImGui::PopID();
            });
        }

        if(entity.HasComponent<ParticleSystemComponent>())
        {
            ParticleSystemComponent& component = entity.GetComponent<ParticleSystemComponent>();
            DrawComponent<ParticleSystemComponent>(entity, "Particle System", [&component, &entity]() {
                DrawAssetDropSlot<ParticleEffect>("Effect", component.Effect, AssetType::PARTICLE_EFFECT, "None (Particle Effect)");

                ImGui::TableNextColumn();
                ImGui::TableNextColumn();
                ParticleEditorPanel* particleEditor = static_cast<Editor*>(Core::GetClient())->GetPanelManager().GetPanel<ParticleEditorPanel>();
                if(component.Effect)
                {
                    if(ImGui::Button("Edit Effect", ImVec2(-FLT_MIN, 0.0f)))
                        particleEditor->OpenEffect(component.Effect->GetID());
                }
                else if(ImGui::Button("Create New Effect", ImVec2(-FLT_MIN, 0.0f)))
                {
                    // Assets/Effects/<EntityName>.spfx, assigned and opened in the Particle Editor right away
                    AssetManager* am = Core::GetAssetManager();
                    const String name = entity.HasComponent<NameComponent>() ? entity.GetComponent<NameComponent>().Name : String("ParticleEffect");
                    const AssetID effectID = ParticleEditorPanel::CreateEffectAsset(std::filesystem::path(am->GetAssetsDirectory()) / "Effects", name);
                    if(effectID)
                    {
                        component.Effect = am->Load<ParticleEffect>(effectID);
                        particleEditor->OpenEffect(effectID);
                    }
                }

                // Edit mode previews the effect in the Scene views, Play mode drives the running one
                ParticleRuntime& runtime = component.Runtime;
                EditorUI::TSeparator("Playback");
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();
                {
                    const float buttonSize = ImGui::GetFrameHeight();
                    const ImVec4 glyphColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                    ImGui::BeginDisabled(!component.Effect);
                    if(GizmoIcons::IconButton("##PlayPause", runtime.IsPlaying() ? GizmoIcon::PAUSE : GizmoIcon::PLAY, ImVec2(buttonSize, buttonSize), glyphColor))
                    {
                        if(runtime.IsPlaying())
                            runtime.Pause();
                        else
                            runtime.Play();
                    }
                    EditorUI::DelayedToolTip(runtime.IsPlaying() ? "Pause" : "Play");
                    ImGui::SameLine(0.0f, 2.0f);
                    if(GizmoIcons::IconButton("##Restart", GizmoIcon::STEP, ImVec2(buttonSize, buttonSize), glyphColor))
                        runtime.Restart();
                    EditorUI::DelayedToolTip("Restart");
                    ImGui::SameLine(0.0f, 2.0f);
                    if(GizmoIcons::IconButton("##Stop", GizmoIcon::STOP, ImVec2(buttonSize, buttonSize), glyphColor))
                        runtime.Stop(true);
                    EditorUI::DelayedToolTip("Stop and clear");
                    ImGui::EndDisabled();

                    ImGui::SameLine();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextDisabled("%.2f s  |  %u particles", runtime.GetTime(), runtime.GetParticleCount());
                }

                EditorUI::TSeparator("Settings");
                EditorUI::TProperty<bool>("Auto Play", &component.AutoPlay);
                EditorUI::TProperty<bool>("Auto Destroy", &component.AutoDestroy);
                EditorUI::TSlider<float>("Simulation Speed", &component.SimulationSpeed, 0.0f, 4.0f, "%.2f");
                EditorUI::TProperty<float>("Emission Multiplier", &component.EmissionMultiplier, 0.0f, 100.0f);
                EditorUI::TProperty<int>("Seed", &component.Seed);
                constexpr auto cullingModeStrings = std::array { "ALWAYS_SIMULATE", "PAUSE_OFFSCREEN", "PAUSE_AND_CATCH_UP" };
                EditorUI::TComboBox("Culling", component.Culling, cullingModeStrings);
                EditorUI::THint("Seed 0 = a different sequence every play. Culling: what the effect does outside the camera (Player only)", Colors::TextMuted, 14.0f, "Hint");
            });
        }

        if(entity.HasComponent<AnimatorComponent>())
        {
            AnimatorComponent& component = entity.GetComponent<AnimatorComponent>();
            DrawComponent<AnimatorComponent>(entity, "Animator", [&component, &entity]() {
                const MeshComponent* meshComponent = entity.TryGetComponent<MeshComponent>();
                const Ref<Mesh> mesh = meshComponent && meshComponent->MeshAsset ? meshComponent->MeshAsset.As<Mesh>() : nullptr;
                if(!mesh || !mesh->IsSkinned())
                {
                    EditorUI::THint("Needs a Mesh component with an animated glTF (a skin or animated nodes). Re-import the model if it was imported before animation support.", Colors::TextMuted, 14.0f, "Hint");
                    return;
                }

                AnimatorRuntime& runtime = component.Runtime;
                const bool running = entity.GetScene()->IsRunning();

                // Clip: every clip the runtime knows (the mesh's + the clip sources'), filled once it updated
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Clip");
                ImGui::TableNextColumn();
                ImGui::PushItemWidth(-1);
                if(ImGui::BeginCombo("##Clip", component.Clip.empty() ? "None (rest pose)" : component.Clip.c_str()))
                {
                    if(ImGui::Selectable("None (rest pose)", component.Clip.empty()))
                        component.Clip.clear();
                    for(Uint i = 0; i < runtime.GetClipCount(); i++)
                    {
                        const String& name = runtime.GetClipNameAt(i);
                        ImGui::PushID(static_cast<int>(i));
                        if(ImGui::Selectable(name.c_str(), name == component.Clip))
                            component.Clip = name;
                        ImGui::PopID();
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();

                // Edit mode previews in the Scene views (not saved), Play mode drives the running animator
                EditorUI::TSeparator("Playback");
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();
                {
                    const float buttonSize = ImGui::GetFrameHeight();
                    const ImVec4 glyphColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                    const bool playing = running ? !runtime.IsPaused() : runtime.IsPreviewing();
                    ImGui::BeginDisabled(runtime.GetClipName().empty());
                    if(GizmoIcons::IconButton("##PlayPause", playing ? GizmoIcon::PAUSE : GizmoIcon::PLAY, ImVec2(buttonSize, buttonSize), glyphColor))
                    {
                        if(running)
                            runtime.SetPaused(playing);
                        else
                            runtime.SetPreview(!playing);
                    }
                    EditorUI::DelayedToolTip(playing ? "Pause" : (running ? "Resume" : "Preview"));
                    ImGui::SameLine(0.0f, 2.0f);
                    if(GizmoIcons::IconButton("##Restart", GizmoIcon::STEP, ImVec2(buttonSize, buttonSize), glyphColor))
                        runtime.SetTime(0.0f);
                    EditorUI::DelayedToolTip("Back to the first frame");
                    ImGui::EndDisabled();

                    ImGui::SameLine();
                    ImGui::AlignTextToFramePadding();
                    if(runtime.GetClipName().empty())
                        ImGui::TextDisabled("Rest pose");
                    else
                        ImGui::TextDisabled("%s  |  %.2f / %.2f s", runtime.GetClipName().c_str(), runtime.GetTime(), runtime.GetDuration());
                }
                if(!runtime.GetClipName().empty())
                {
                    float time = runtime.GetTime();
                    if(EditorUI::TSlider<float>("Time", &time, 0.0f, runtime.GetDuration(), "%.2f s"))
                        runtime.SetTime(time);
                }

                EditorUI::TSeparator("Settings");
                EditorUI::TProperty<bool>("Play On Awake", &component.PlayOnAwake);
                EditorUI::TProperty<bool>("Loop", &component.Loop);
                EditorUI::TSlider<float>("Speed", &component.Speed, -2.0f, 3.0f, "%.2f");
                EditorUI::TProperty<bool>("Animate Offscreen", &component.AnimateOffscreen);

                EditorUI::TSeparator("Clip Sources");
                DrawAssetDropSlot<Mesh>("Source 1", component.ClipSource1, AssetType::MESH, "None (Mesh)");
                DrawAssetDropSlot<Mesh>("Source 2", component.ClipSource2, AssetType::MESH, "None (Mesh)");
                DrawAssetDropSlot<Mesh>("Source 3", component.ClipSource3, AssetType::MESH, "None (Mesh)");
                DrawAssetDropSlot<Mesh>("Source 4", component.ClipSource4, AssetType::MESH, "None (Mesh)");

                const Skeleton& skeleton = mesh->GetSkeleton();
                EditorUI::TString("Skeleton", "%u nodes, %u joints, %u clips (%u own)", skeleton.GetNodeCount(), skeleton.GetPaletteSize(), runtime.GetClipCount(), static_cast<Uint>(mesh->GetClips().size()));
                EditorUI::THint("Clip sources: other animated glTFs whose joint names match (one animation per file, Mixamo style). Animate Offscreen: Player only, otherwise only the time advances outside the camera", Colors::TextMuted, 14.0f, "Hint");
            });
        }

        if(entity.HasComponent<UICanvasComponent>())
        {
            UICanvasComponent& component = entity.GetComponent<UICanvasComponent>();
            DrawComponent<UICanvasComponent>(entity, "UI Canvas", [&component, &entity]() {
                EditorUI::TProperty<bool>("Show Canvas", &component.ShowCanvas);
                EditorUI::TProperty<int>("Sort Order", &component.SortOrder, -1000.0f, 1000.0f);
                DrawAssetDropSlot<UILayout>("Layout", component.Layout, AssetType::UI_LAYOUT, "None (UI Layout)");

                ImGui::TableNextColumn();
                ImGui::TableNextColumn();
                UIEditorPanel* uiEditor = static_cast<Editor*>(Core::GetClient())->GetPanelManager().GetPanel<UIEditorPanel>();
                if(component.Layout)
                {
                    if(ImGui::Button("Edit Layout", ImVec2(-FLT_MIN, 0.0f)))
                        uiEditor->OpenLayout(component.Layout->GetID());
                }
                else if(ImGui::Button("Create New Layout", ImVec2(-FLT_MIN, 0.0f)))
                {
                    // Assets/UI/<EntityName>.sui, assigned and opened in the UI Editor right away
                    AssetManager* am = Core::GetAssetManager();
                    const String name = entity.HasComponent<NameComponent>() ? entity.GetComponent<NameComponent>().Name : String("UICanvas");
                    const AssetID layoutID = UIEditorPanel::CreateLayoutAsset(std::filesystem::path(am->GetAssetsDirectory()) / "UI", name);
                    if(layoutID)
                    {
                        component.Layout = am->Load<UILayout>(layoutID);
                        uiEditor->OpenLayout(layoutID);
                    }
                }

                EditorUI::TSeparator("Script");
                DrawAssetDropSlot<Script>("Script", component.ScriptAsset, AssetType::SCRIPT, "None (Script)");
                const bool live = static_cast<Editor*>(Core::GetClient())->IsPlaying() && component.IsInstantiated;
                ScriptFields::DrawInspector(component.ScriptAsset, component.Fields, live ? &component.Instance : nullptr, entity.GetScene());
            });
        }

        if(entity.HasComponent<AudioSourceComponent>())
        {
            AudioSourceComponent& component = entity.GetComponent<AudioSourceComponent>();
            DrawComponent<AudioSourceComponent>(entity, "Audio Source", [&component]() {
                AudioEngine* audioEngine = Core::GetAudioEngine();

                DrawAssetDropSlot<Audio>("Audio Clip", component.AudioClip, AssetType::AUDIO, "None (Audio)");
                if(!component.AudioClip)
                    return;

                if(EditorUI::TSlider<float>("Volume", &component.Volume, 0.0f, 2.0f, "%.2f") && component.RuntimeID)
                    audioEngine->SetVolume(component.RuntimeID, component.Volume);
                if(EditorUI::TSlider<float>("Pitch", &component.Pitch, 0.1f, 3.0f, "%.2f") && component.RuntimeID)
                    audioEngine->SetPitch(component.RuntimeID, component.Pitch);
                if(EditorUI::TProperty<bool>("Loop", &component.Loop) && component.RuntimeID)
                    audioEngine->SetLooping(component.RuntimeID, component.Loop);
                EditorUI::TProperty<bool>("Play On Awake", &component.PlayOnAwake);
                EditorUI::TProperty<bool>("Streaming", &component.IsStreaming);

                EditorUI::TSeparator("3D Sound");
                EditorUI::TProperty<bool>("Spatialized", &component.IsSpatialized);
                if(component.IsSpatialized)
                {
                    constexpr auto attenuationModelStrings = std::array { "NONE", "INVERSE_DISTANCE", "LINEAR_DISTANCE", "EXPONENTIAL_DISTANCE" };
                    EditorUI::TComboBox("Attenuation", component.Attenuation, attenuationModelStrings);

                    if(EditorUI::TProperty<float>("Min Distance", &component.MinDistance, 0.0f, 100000.0f) && component.RuntimeID)
                        audioEngine->SetMinDistance(component.RuntimeID, component.MinDistance);
                    if(EditorUI::TProperty<float>("Max Distance", &component.MaxDistance, 0.0f, 100000.0f) && component.RuntimeID)
                        audioEngine->SetMaxDistance(component.RuntimeID, component.MaxDistance);
                }
                EditorUI::THint("Music and long ambience: use OGG with Streaming on. That keeps about 1 MB per minute in memory and no PCM.\n\n"
                        "Short SFX : leave Streaming off.They decode once and every source shares the PCM, so there’s no decoding while playing.\n"
                        "Avoid WAV for music.The encoded WAV already sits in RAM, and a non - streamed WAV adds its full PCM copy on top.", Colors::TextMuted, 14.0f, "Hint");
            });
        }

        if(entity.HasComponent<AudioListenerComponent>())
        {
            DrawComponent<AudioListenerComponent>(entity, "Audio Listener", []() {
                EditorUI::THint("Hears the scene's 3D sounds from this entity. Keep one active listener per scene", Colors::TextMuted, 14.0f, "Hint");
            });
        }
    }

} // namespace Surge
