// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/SceneHierarchyPanel.hpp"
#include "Asset/AssetDatabase.hpp"
#include "Asset/AssetExtensions.hpp"
#include "Surge/Core/Core.hpp"

#include "Surge/Asset/Asset.hpp"
#include "Surge/ECS/Components/Components.hpp"
#include "Surge/ECS/Components/ScriptComponent.hpp"

#include "Utility/EditorUI.hpp"
#include "History/SceneCommands.hpp"
#include "Gizmos/GizmoIcons.hpp"
#include "Panels/ContentBrowserPanel.hpp"
#include "Editor.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Audio/Audio.hpp"
#include "Surge/Graphics/HighLevel/DefaultMeshes.hpp"
#include "Surge/Graphics/HighLevel/Font.hpp"
#include "Surge/Graphics/HighLevel/Mesh.hpp"
#include "Surge/Graphics/UISystem/UILayout.hpp"
#include "Surge/Graphics/Particles/ParticleEffect.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <utility>

namespace Surge
{
    namespace
    {
        namespace Colors = EditorUI::Colors;
        using EditorUI::WithAlpha;
        constexpr ImU32 DROP_INDICATOR = EditorUI::ToU32(Colors::Accent);
        constexpr float ROW_GRADIENT_WIDTH = 0.2f;  // Fraction of the row the type color fades over
        constexpr float ROW_GRADIENT_ALPHA = 0.22f; // Opacity at the left edge

        using GizmoIcons::EntityIcon;
        using GizmoIcons::GetEntityIcon;

        // "t:<keyword>" search tokens, a token matches every keyword it is a prefix of ("t:cam" -> camera)
        struct TypeFilter
        {
            const char* Keyword;
            bool (*Matches)(Entity);
        };

        const TypeFilter sTypeFilters[] = {
            { "camera", [](Entity e) { return e.HasComponent<CameraComponent>(); } },
            { "light", [](Entity e) { return e.HasComponent<LightComponent>(); } },
            { "mesh", [](Entity e) { return e.HasComponent<MeshComponent>(); } },
            { "sprite", [](Entity e) { return e.HasComponent<SpriteRendererComponent>(); } },
            { "text", [](Entity e) { return e.HasComponent<TextComponent>(); } },
            { "audio", [](Entity e) { return e.HasComponent<AudioSourceComponent>() || e.HasComponent<AudioListenerComponent>(); } },
            { "script", [](Entity e) { return e.HasComponent<ScriptComponent>(); } },
            { "canvas", [](Entity e) { return e.HasComponent<UICanvasComponent>(); } },
            { "ui", [](Entity e) { return e.HasComponent<UICanvasComponent>(); } },
            { "environment", [](Entity e) { return e.HasComponent<EnvironmentComponent>(); } },
            { "rendersettings", [](Entity e) { return e.HasComponent<RendererSettingsComponent>(); } },
            { "particles", [](Entity e) { return e.HasComponent<ParticleSystemComponent>(); } },
            { "animator", [](Entity e) { return e.HasComponent<AnimatorComponent>(); } },
            { "rigidbody", [](Entity e) { return e.HasComponent<RigidbodyComponent>(); } },
            { "collider", [](Entity e) {
                  return e.HasComponent<BoxColliderComponent>() || e.HasComponent<SphereColliderComponent>() || e.HasComponent<CapsuleColliderComponent>() ||
                         e.HasComponent<CylinderColliderComponent>() || e.HasComponent<ConvexColliderComponent>() || e.HasComponent<MeshColliderComponent>();
              } },
        };

        void ToLower(String& str)
        {
            std::transform(str.begin(), str.end(), str.begin(), [](char c) { return static_cast<char>(::tolower(static_cast<unsigned char>(c))); });
        }

        bool IsCreatableAsset(AssetID id)
        {
            switch(Core::GetAssetManager()->GetMetadata(id).Type)
            {
                case AssetType::MESH:
                case AssetType::TEXTURE2D:
                case AssetType::AUDIO:
                case AssetType::FONT:
                case AssetType::UI_LAYOUT:
                case AssetType::PARTICLE_EFFECT: return true;
                default: return false;
            }
        }

        // Selecting brings a docked Inspector tab to the front without taking the keyboard focus away from the Hierarchy (arrow keys, F2)
        void ShowInspectorTab()
        {
            ImGuiWindow* inspector = ImGui::FindWindowByName(PanelCodeToString(PanelCode::Inspector));
            ImGuiWindow* hierarchy = ImGui::GetCurrentWindow()->RootWindow;
            if(inspector && inspector->DockNode && inspector->DockNode->TabBar && inspector->DockNode != hierarchy->DockNode)
                inspector->DockNode->TabBar->NextSelectedTabId = inspector->TabId;
        }
    } // namespace

    void SceneHierarchyPanel::Init(void*)
    {
        mCode = GetStaticCode();
        mSceneContext = nullptr;
        mSelectedEntity = {};
    }

    void SceneHierarchyPanel::OnEvent(Event&)
    {
        // Delete/Duplicate/Copy/Paste shortcuts live in Editor::HandleShortcuts (they go through the undo history)
    }

    void SceneHierarchyPanel::Render(bool* show)
    {
        if(!*show || !mSceneContext)
            return;

        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, WithAlpha(Colors::Ink, 0.06f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, WithAlpha(Colors::Ink, 0.1f));

        mHierarchyFocused = false;
        mHierarchyHovered = false;
        if(ImGui::Begin(PanelCodeToString(mCode), show))
        {
            Editor* editor = static_cast<Editor*>(Core::GetClient());
            mHierarchyHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
            mHierarchyFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

            if(!mRenamingEntity || mRenamingEntity != mSelectedEntity)
                mRenamingEntity = {}; // Selection moved on, or the entity is gone

            const bool typing = ImGui::GetIO().WantTextInput;
            if((mHierarchyFocused || mHierarchyHovered) && !typing)
            {
                if(mSelectedEntity && !mRenamingEntity && ImGui::IsKeyPressed(ImGuiKey_F2, false))
                    StartRename(mSelectedEntity);
                if(ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F, false))
                    mFocusSearch = true;
            }

            DrawToolbar();
            UpdateSearch();
            const bool searching = IsSearching();

            mRoots.clear();
            mSceneContext->GetRegistry().each([&](entt::entity entity) {
                const RelationshipComponent* rel = mSceneContext->GetRegistry().try_get<RelationshipComponent>(entity);
                if(rel && rel->Parent == entt::null && (!searching || mSearchVisible.contains(entity)))
                    mRoots.push_back(entity);
            });

            // The tree scrolls on its own, the toolbar stays. Arrow keys drive the selection, not ImGui's nav cursor
            mVisibleRows.clear();
            if(ImGui::BeginChild("##HierarchyTree", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_NoNavInputs))
            {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.0f, 0.0f));

                // Search results keep their own open state, clearing the search brings the tree back as it was
                ImGui::PushID(searching ? "Search" : "Tree");
                if(ImGui::BeginTable("##HierarchyTable", 1, ImGuiTableFlags_None))
                {
                    for(entt::entity root : mRoots)
                    {
                        if(mSceneContext->GetRegistry().valid(root))
                            DrawEntityNode(Entity(root, mSceneContext), false);
                    }
                    ImGui::EndTable();
                }
                ImGui::PopID();
                ImGui::PopStyleVar(3);

                if(mRoots.empty())
                {
                    ImGui::Spacing();
                    ImGui::PushTextWrapPos(0.0f);
                    if(searching)
                        ImGui::TextDisabled("No entity matches \"%s\"", mSearchBuffer.c_str());
                    else
                        ImGui::TextDisabled("Seems like this scene is as empty as your brain\nPress + or right click here to create an entity");
                    ImGui::PopTextWrapPos();
                }

                // Empty space below the tree: deselect, context menu, drop to move to the root level / create from an asset
                const ImVec2 emptySpace = ImGui::GetContentRegionAvail();
                if(emptySpace.y > 0.0f && emptySpace.x > 0.0f)
                {
                    ImGui::InvisibleButton("##EmptySpace", emptySpace);
                    if(ImGui::IsItemClicked(ImGuiMouseButton_Left))
                        Select({}, false);
                    if(ImGui::IsItemClicked(ImGuiMouseButton_Right))
                        mOpenSpacePopup = true;
                    AcceptDrop({}, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), 0.0f, false);
                }
            }
            ImGui::EndChild();

            // State requested for this frame was consumed by the rows, what the code below asks for applies next frame
            mRevealSelection = false;
            mOpenRequests.clear();
            mSetAllOpen = -1;
            mExpandSubtree = std::exchange(mNextExpandSubtree, entt::null);

            // Popups live in the panel's ID scope, the ADD button and the tree's empty space open the same ones
            if(mOpenCreatePopup)
                ImGui::OpenPopup("##CreateEntity");
            if(mOpenSpacePopup)
                ImGui::OpenPopup("##HierarchySpace");
            mOpenCreatePopup = mOpenSpacePopup = false;

            EditorUI::StyledPopupVars::Push();
            if(ImGui::BeginPopup("##CreateEntity"))
            {
                DrawCreateMenu({});
                EditorUI::EndStyledPopup();
            }
            else
                EditorUI::StyledPopupVars::Pop();

            EditorUI::StyledPopupVars::Push();
            if(ImGui::BeginPopup("##HierarchySpace"))
            {
                if(EditorUI::StyledMenuItem("Paste", "Ctrl+V", nullptr, editor->HasEntityInClipboard() && !editor->IsPlaying()))
                    Core::AddFrameEndCallback([editor]() { editor->PasteEntity(); });
                EditorUI::StyledSeparator();

                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
                if(ImGui::BeginMenu("Create"))
                {
                    DrawCreateMenu({});
                    ImGui::EndMenu();
                }
                ImGui::PopStyleVar();
                EditorUI::StyledSeparator();

                if(EditorUI::StyledMenuItem("Expand All"))
                    mSetAllOpen = 1;
                if(EditorUI::StyledMenuItem("Collapse All"))
                    mSetAllOpen = 0;
                EditorUI::EndStyledPopup();
            }
            else
                EditorUI::StyledPopupVars::Pop();

            HandleKeyboardNavigation();
            ApplyPendingEdits();
        }
        ImGui::End();
        ImGui::PopStyleColor(2);
    }

    void SceneHierarchyPanel::DrawToolbar()
    {
        Editor* editor = static_cast<Editor*>(Core::GetClient());

        // Scene name, play state and entity count
        {
            const String title = std::format("{}{}", mSceneContext->GetName(), GetExtensionFromAssetType(AssetType::SCENE));
            const float lineStart = ImGui::GetCursorPosX();
            const float right = lineStart + ImGui::GetContentRegionAvail().x;
            {
                EditorUI::ScopedBoldFont font;
                ImGui::TextUnformatted(title.c_str());
            }

            const String count = std::format("{} {}", mSceneContext->GetRegistry().alive(), mSceneContext->GetRegistry().alive() == 1 ? "entity" : "entities");
            const char* playState = editor->IsPlaying() ? "PLAYING" : nullptr;
            const float countWidth = ImGui::CalcTextSize(count.c_str()).x;
            const float playWidth = playState ? ImGui::CalcTextSize(playState).x + ImGui::GetStyle().ItemSpacing.x : 0.0f;
            ImGui::SameLine(glm::max(right - countWidth - playWidth, lineStart + ImGui::GetItemRectSize().x + 8.0f));
            if(playState)
            {
                ImGui::TextColored(Colors::Accent, "%s", playState);
                ImGui::SameLine();
            }
            ImGui::TextDisabled("%s", count.c_str());
        }

        // Search + ADD
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
        {
            const float addButtonSize = ImGui::GetFrameHeight();

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - addButtonSize - ImGui::GetStyle().ItemSpacing.x);
            if(mFocusSearch)
            {
                ImGui::SetKeyboardFocusHere();
                mFocusSearch = false;
            }
            ImGui::InputTextWithHint("##Search", "Search...  (t:mesh, t:light)", &mSearchBuffer, ImGuiInputTextFlags_EscapeClearsAll);
            EditorUI::DelayedToolTip("...");

            ImGui::SameLine();
            if(GizmoIcons::IconButton("##Add", GizmoIcon::ADD, ImVec2(addButtonSize, addButtonSize), Colors::Text))
                mOpenCreatePopup = true;
            EditorUI::DelayedToolTip("Create entity");
        }
        ImGui::PopStyleVar();
        ImGui::Spacing();
    }

    void SceneHierarchyPanel::DrawCreateMenu(Entity parent)
    {
        auto item = [&](const char* label, CreateKind kind, const char* defaultMesh = nullptr) {
            if(EditorUI::StyledMenuItem(label))
                mPendingCreate = { .Kind = kind, .Parent = parent, .DefaultMesh = defaultMesh, .Asset = {} };
        };

        item("Empty Entity", CreateKind::EMPTY);

        EditorUI::StyledSeparator();
        EditorUI::MenuHeader("RENDERING");
        item("Camera", CreateKind::CAMERA);
        item("Sprite Renderer", CreateKind::SPRITE);
        item("Particle System", CreateKind::PARTICLE_SYSTEM);

        EditorUI::StyledSeparator();
        EditorUI::MenuHeader("3D PRIMITIVES");
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
        if(ImGui::BeginMenu("Meshes"))
        {
            item("Empty Mesh", CreateKind::MESH);
            item("Cube", CreateKind::MESH, DefaultMesh::CUBE);
            item("Sphere", CreateKind::MESH, DefaultMesh::SPHERE);
            item("Bean", CreateKind::MESH, DefaultMesh::BEAN);
            item("Cone", CreateKind::MESH, DefaultMesh::CONE);
            item("Cylinder", CreateKind::MESH, DefaultMesh::CYLINDER);
            item("Torus", CreateKind::MESH, DefaultMesh::TORUS);
            item("Plane", CreateKind::MESH, DefaultMesh::PLANE);
            ImGui::EndMenu();
        }
        ImGui::PopStyleVar();

        EditorUI::StyledSeparator();
        EditorUI::MenuHeader("WORLD & UI");
        item("Environment", CreateKind::ENVIRONMENT);
        item("Directional Light", CreateKind::DIRECTIONAL_LIGHT);
        item("Point Light", CreateKind::POINT_LIGHT);
        item("Text", CreateKind::TEXT);
        item("UI Canvas", CreateKind::UI_CANVAS);
        item("Audio Source", CreateKind::AUDIO_SOURCE);
        item("Audio Listener", CreateKind::AUDIO_LISTENER);
    }

    void SceneHierarchyPanel::DrawEntityNode(Entity e, bool forceOpen)
    {
        const entt::entity handle = e.Raw();
        const RelationshipComponent& rel = e.GetComponent<RelationshipComponent>();
        const bool searching = IsSearching();

        // Children shown in this mode: all of them, or only those leading to a search match
        bool hasChildren = false;
        for(entt::entity child = static_cast<entt::entity>(rel.FirstChild); child != entt::null && !hasChildren;
            child = static_cast<entt::entity>(mSceneContext->GetRegistry().get<RelationshipComponent>(child).NextSibling))
            hasChildren = !searching || mSearchVisible.contains(child);

        const bool isSelected = mSelectedEntity == e;
        const bool isRenaming = mRenamingEntity == e;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_DrawLinesFull;
        if(isSelected)
            flags |= ImGuiTreeNodeFlags_Selected;
        if(!hasChildren)
            flags |= ImGuiTreeNodeFlags_Leaf;
        if(isRenaming)
            flags |= ImGuiTreeNodeFlags_AllowOverlap; // The rename field sits on top of the row

        // Last call wins: search default < reveal < explicit requests < expand/collapse all
        if(hasChildren)
        {
            if(searching && mSearchChanged)
                ImGui::SetNextItemOpen(true);
            if(mRevealSelection && !isSelected && mSelectedEntity && SceneEditUtils::IsDescendantOf(mSelectedEntity, e))
                ImGui::SetNextItemOpen(true);
            for(const auto& [entity, open] : mOpenRequests)
            {
                if(entity == handle)
                    ImGui::SetNextItemOpen(open);
            }
            forceOpen = forceOpen || handle == mExpandSubtree;
            if(forceOpen || mSetAllOpen >= 0)
                ImGui::SetNextItemOpen(forceOpen || mSetAllOpen == 1);
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        const float labelX = ImGui::GetCursorScreenPos().x + ImGui::GetTreeNodeToLabelSpacing();
        ImGui::PushStyleColor(ImGuiCol_Header, Colors::Selection);
        if(isSelected)
        {
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, Colors::Selection);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, Colors::Selection);
        }
        const bool opened = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uint64_t>(static_cast<Uint>(handle))), flags, "%s", ""); // Label drawn below, with the icon
        ImGui::PopStyleColor(isSelected ? 3 : 1);

        // Read before the drag & drop source opens its tooltip window.
        // Selected on release, not on press: dragging a row onto an Inspector field must not switch the Inspector to the dragged entity
        if(ImGui::IsItemClicked(ImGuiMouseButton_Left))
            mPressedEntity = ImGui::IsItemToggledOpen() ? entt::null : handle;
        bool clicked = false;
        if(mPressedEntity == handle && ImGui::IsItemDeactivated())
        {
            clicked = !ImGui::IsMouseDragPastThreshold(ImGuiMouseButton_Left);
            mPressedEntity = entt::null;
        }
        const bool doubleClicked = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        const ImVec2 rowMin = ImGui::GetItemRectMin();
        const ImVec2 rowMax = ImGui::GetItemRectMax();

        mVisibleRows.push_back({ handle, static_cast<entt::entity>(rel.Parent), hasChildren, opened });

        if(isSelected && mRevealSelection)
            ImGui::ScrollToItem(ImGuiScrollFlags_KeepVisibleEdgeY);

        DrawRowContents(e, rowMin, rowMax, labelX, isSelected, isRenaming);

        if(!isRenaming && ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload(IMGUI_ENTITY_PAYLOAD, &handle, sizeof(entt::entity));
            ImGui::Text("Move %s", e.GetComponent<NameComponent>().Name.c_str());
            ImGui::EndDragDropSource();
        }
        AcceptDrop(e, rowMin, rowMax, labelX, opened && hasChildren);

        if(clicked)
        {
            Select(e, false);
            ShowInspectorTab();
        }
        if(doubleClicked && !isRenaming)
            static_cast<Editor*>(Core::GetClient())->FocusEntity(e);

        DrawEntityContextMenu(e);

        if(isRenaming)
            DrawRenameField(e, ImVec2(labelX + ImGui::GetFontSize() + 1.0f, rowMin.y));

        if(opened)
        {
            for(entt::entity child = static_cast<entt::entity>(e.GetComponent<RelationshipComponent>().FirstChild); child != entt::null;)
            {
                // Next sibling read first: nothing below relinks, but keep the walk independent of the child's row
                const entt::entity next = static_cast<entt::entity>(mSceneContext->GetRegistry().get<RelationshipComponent>(child).NextSibling);
                if(!searching || mSearchVisible.contains(child))
                    DrawEntityNode(Entity(child, mSceneContext), forceOpen);
                child = next;
            }
            ImGui::TreePop();
        }
    }

    void SceneHierarchyPanel::DrawRowContents(Entity e, const ImVec2& rowMin, const ImVec2& rowMax, float labelX, bool isSelected, bool isRenaming)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float fontSize = ImGui::GetFontSize();
        const float centerY = (rowMin.y + rowMax.y) * 0.5f;

        const EntityIcon icon = GetEntityIcon(e);

        // Row background: the type color fading out from the left edge (instead of alternating table row colors)
        {
            const float gradientEnd = rowMin.x + (rowMax.x - rowMin.x) * ROW_GRADIENT_WIDTH;
            const ImU32 left = WithAlpha(icon.Color, ROW_GRADIENT_ALPHA);
            const ImU32 right = WithAlpha(icon.Color, 0.0f);
            EditorUI::AddHorizontalGradient(drawList, rowMin, ImVec2(gradientEnd, rowMax.y), left, right);
        }

        // Type icon
        const ImU32 background = isSelected ? ImGui::GetColorU32(Colors::Selection) : ImGui::GetColorU32(ImGuiCol_WindowBg);
        GizmoIcons::DrawGlyph(drawList, ImVec2(labelX + fontSize * 0.5f, centerY), fontSize * 0.86f, icon.Icon, isSelected ? EditorUI::ToU32(Colors::Tone96) : icon.Color, background);

        // Script indicator on the right edge
        float textClipX = rowMax.x - 4.0f;
        if(const ScriptComponent* scripts = e.TryGetComponent<ScriptComponent>(); scripts && !scripts->Scripts.empty())
        {
            const ImVec2 center(rowMax.x - fontSize * 0.5f - 6.0f, centerY);
            GizmoIcons::DrawGlyph(drawList, center, fontSize * 0.72f, GizmoIcon::SCRIPT, isSelected ? EditorUI::ToU32(Colors::Tone96) : ImGui::GetColorU32(ImGuiCol_TextDisabled), background);
            textClipX = center.x - fontSize * 0.5f - 4.0f;

            const ImVec2 half(fontSize * 0.5f, fontSize * 0.5f);
            if(ImGui::IsMouseHoveringRect(ImVec2(center.x - half.x, center.y - half.y), ImVec2(center.x + half.x, center.y + half.y)) && !ImGui::GetDragDropPayload())
                ImGui::SetTooltip("%zu script%s", scripts->Scripts.size(), scripts->Scripts.size() == 1 ? "" : "s");
        }

        if(isRenaming)
            return; // The rename field takes the label's place

        const String& name = e.GetComponent<NameComponent>().Name;
        const ImVec2 textPos(labelX + fontSize + 5.0f, centerY - fontSize * 0.5f);
        const ImVec4 clip(rowMin.x, rowMin.y, textClipX, rowMax.y);

        ImFont* font = isSelected ? EditorUI::BoldFont() : ImGui::GetFont();
        ImU32 textColor = isSelected ? EditorUI::ToU32(Colors::Tone96) : ImGui::GetColorU32(ImGuiCol_Text);

        if(IsSearching())
        {
            const auto it = mSearchVisible.find(e.Raw());
            const bool isMatch = it != mSearchVisible.end() && it->second;
            if(!isMatch && !isSelected)
                textColor = ImGui::GetColorU32(ImGuiCol_TextDisabled); // Only here to show where a match lives

            // Highlight the matched part of the name (lower casing keeps the byte offsets)
            if(isMatch && !mSearchName.empty())
            {
                String lowered = name;
                ToLower(lowered);
                const size_t start = lowered.find(mSearchName);
                if(start != String::npos)
                {
                    const float before = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, name.c_str(), name.c_str() + start).x;
                    const float width = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, name.c_str() + start, name.c_str() + start + mSearchName.size()).x;
                    drawList->AddRectFilled(ImVec2(textPos.x + before - 1.0f, rowMin.y + 3.0f), ImVec2(textPos.x + before + width + 1.0f, rowMax.y - 3.0f), EditorUI::ToU32(WithAlpha(Colors::Accent, 90.0f / 255.0f)), 2.0f);
                }
            }
        }

        drawList->AddText(font, fontSize, textPos, textColor, name.c_str(), nullptr, 0.0f, &clip);
    }

    void SceneHierarchyPanel::DrawEntityContextMenu(Entity e)
    {
        if(!EditorUI::BeginStyledPopupContextItem())
            return;

        Editor* editor = static_cast<Editor*>(Core::GetClient());
        const RelationshipComponent& rel = e.GetComponent<RelationshipComponent>();

        EditorUI::MenuHeader(e.GetComponent<NameComponent>().Name.c_str());

        if(EditorUI::StyledMenuItem("Rename", "F2"))
        {
            editor->SelectEntity(e); // Renames are recorded through the selection tracker
            StartRename(e);
        }
        if(EditorUI::StyledMenuItem("Focus", "F"))
            editor->FocusEntity(e);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
        if(ImGui::BeginMenu("Create Child"))
        {
            DrawCreateMenu(e);
            ImGui::EndMenu();
        }
        ImGui::PopStyleVar();

        if(rel.FirstChild != entt::null && EditorUI::StyledMenuItem("Expand All Children"))
            mNextExpandSubtree = e.Raw();
        EditorUI::StyledSeparator();

        if(rel.Parent != entt::null)
        {
            if(EditorUI::StyledMenuItem("Unparent"))
                mPendingMove = { .Target = e, .Parent = {}, .InsertAfter = {}, .KeepOrder = true };
            EditorUI::StyledSeparator();
        }

        // Creating entities while the tree is being drawn is not safe, these run at the end of the frame
        if(EditorUI::StyledMenuItem("Copy", "Ctrl+C"))
            editor->CopyEntity(e);
        if(EditorUI::StyledMenuItem("Paste", "Ctrl+V", nullptr, editor->HasEntityInClipboard() && !editor->IsPlaying()))
        {
            Entity target = e;
            Core::AddFrameEndCallback([editor, target]() {
                editor->SelectEntity(target); // Pastes right after the clicked entity
                editor->PasteEntity();
            });
        }
        if(EditorUI::StyledMenuItem("Duplicate", "Ctrl+D"))
            Core::AddFrameEndCallback([editor, e]() { editor->DuplicateEntity(e); });
        if(EditorUI::StyledMenuItem("Delete", "Del"))
            editor->DeleteEntity(e);
        EditorUI::EndStyledPopup();
    }

    void SceneHierarchyPanel::AcceptDrop(Entity target, const ImVec2& rowMin, const ImVec2& rowMax, float labelX, bool targetOpen)
    {
        if(!ImGui::BeginDragDropTarget())
            return;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(IMGUI_ENTITY_PAYLOAD, ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
        {
            Entity dragged(*static_cast<const entt::entity*>(payload->Data), mSceneContext);
            if(dragged && dragged != target && !(target && SceneEditUtils::IsDescendantOf(target, dragged)))
            {
                if(!target)
                {
                    // Root level: roots have no sibling order (registry order), so this only unparents
                    if(SceneEditUtils::GetParent(dragged))
                    {
                        drawList->AddRect(rowMin, rowMax, DROP_INDICATOR, 3.0f, ImDrawFlags_None, 2.0f);
                        if(payload->IsDelivery())
                            mPendingMove = { .Target = dragged, .Parent = {}, .InsertAfter = {}, .KeepOrder = true };
                    }
                }
                else
                {
                    // Upper/lower quarter: next to the target among its siblings (children only), the rest: into it
                    enum class Zone { BEFORE, INSIDE, AFTER };
                    const Entity targetParent = SceneEditUtils::GetParent(target);
                    const float t = (ImGui::GetMousePos().y - rowMin.y) / glm::max(rowMax.y - rowMin.y, 1.0f);
                    Zone zone = Zone::INSIDE;
                    if(targetParent && t < 0.25f)
                        zone = Zone::BEFORE;
                    else if((targetParent || targetOpen) && t > 0.75f)
                        zone = Zone::AFTER;

                    const float lineStart = labelX + (zone == Zone::AFTER && targetOpen ? ImGui::GetStyle().IndentSpacing : 0.0f);
                    switch(zone)
                    {
                        case Zone::BEFORE: drawList->AddLine(ImVec2(lineStart, rowMin.y + 1.0f), ImVec2(rowMax.x, rowMin.y + 1.0f), DROP_INDICATOR, 2.0f); break;
                        case Zone::AFTER: drawList->AddLine(ImVec2(lineStart, rowMax.y - 1.0f), ImVec2(rowMax.x, rowMax.y - 1.0f), DROP_INDICATOR, 2.0f); break;
                        case Zone::INSIDE: drawList->AddRect(rowMin, rowMax, DROP_INDICATOR, 3.0f, ImDrawFlags_None, 2.0f); break;
                    }

                    if(payload->IsDelivery())
                    {
                        switch(zone)
                        {
                            case Zone::BEFORE: mPendingMove = { .Target = dragged, .Parent = targetParent, .InsertAfter = SceneEditUtils::GetPreviousSibling(target), .KeepOrder = false }; break;
                            // Below an open node reads as "its first child", like the rows suggest
                            case Zone::AFTER: mPendingMove = targetOpen ? MoveRequest { .Target = dragged, .Parent = target, .InsertAfter = {}, .KeepOrder = false } : MoveRequest { .Target = dragged, .Parent = targetParent, .InsertAfter = target, .KeepOrder = false }; break;
                            case Zone::INSIDE: mPendingMove = { .Target = dragged, .Parent = target, .InsertAfter = {}, .KeepOrder = true }; break;
                        }
                    }
                }
            }
        }

        // Assets from the Content Browser create an entity using them (as a child of the row they are dropped on)
        const ImGuiPayload* peek = ImGui::GetDragDropPayload();
        if(peek && peek->IsDataType(CONTENT_BROWSER_PAYLOAD) && IsCreatableAsset(*static_cast<const AssetID*>(peek->Data)))
        {
            if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_PAYLOAD, ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
                mPendingCreate = { .Kind = CreateKind::FROM_ASSET, .Parent = target, .DefaultMesh = nullptr, .Asset = *static_cast<const AssetID*>(payload->Data) };
            else if(ImGui::IsDragDropActive())
                drawList->AddRect(rowMin, rowMax, DROP_INDICATOR, 3.0f, ImDrawFlags_None, 2.0f);
        }

        ImGui::EndDragDropTarget();
    }

    void SceneHierarchyPanel::HandleKeyboardNavigation()
    {
        if(!mHierarchyFocused || ImGui::GetIO().WantTextInput || mRenamingEntity || !mSelectedEntity)
            return;

        const entt::entity selected = mSelectedEntity.Raw();
        const auto it = std::find_if(mVisibleRows.begin(), mVisibleRows.end(), [selected](const VisibleRow& row) { return row.Handle == selected; });
        if(it == mVisibleRows.end())
            return;

        const size_t index = static_cast<size_t>(it - mVisibleRows.begin());
        const VisibleRow& row = *it;
        auto selectRow = [this](size_t i) { Select(Entity(mVisibleRows[i].Handle, mSceneContext), true); };

        if(ImGui::IsKeyPressed(ImGuiKey_UpArrow) && index > 0)
            selectRow(index - 1);
        else if(ImGui::IsKeyPressed(ImGuiKey_DownArrow) && index + 1 < mVisibleRows.size())
            selectRow(index + 1);
        else if(ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
        {
            if(row.HasChildren && row.Open)
                RequestOpen(row.Handle, false);
            else if(row.Parent != entt::null)
                Select(Entity(row.Parent, mSceneContext), true);
        }
        else if(ImGui::IsKeyPressed(ImGuiKey_RightArrow) && row.HasChildren)
        {
            if(!row.Open)
                RequestOpen(row.Handle, true);
            else if(index + 1 < mVisibleRows.size())
                selectRow(index + 1); // First child
        }
    }

    void SceneHierarchyPanel::ApplyPendingEdits()
    {
        Editor* editor = static_cast<Editor*>(Core::GetClient());

        if(mPendingMove.Target)
        {
            MoveRequest move = mPendingMove;
            mPendingMove = {};
            if(move.KeepOrder)
                editor->ReparentEntity(move.Target, move.Parent);
            else
                editor->MoveEntity(move.Target, move.Parent, move.InsertAfter);

            if(move.Parent)
                RequestOpen(move.Parent.Raw(), true); // Keep the moved entity in sight
        }

        if(mPendingCreate.Kind == CreateKind::NONE)
            return;

        const CreateRequest request = mPendingCreate;
        mPendingCreate = {};

        AssetManager* assetManager = Core::GetAssetManager();
        Entity entity;
        switch(request.Kind)
        {
            case CreateKind::NONE: return;
            case CreateKind::EMPTY: mSceneContext->CreateEntity(entity, "Entity"); break;
            case CreateKind::CAMERA:
                mSceneContext->CreateEntity(entity, "Camera");
                entity.AddComponent<CameraComponent>();
                break;
            case CreateKind::SPRITE:
                mSceneContext->CreateEntity(entity, "Sprite");
                entity.AddComponent<SpriteRendererComponent>(glm::vec4 { 1.0f, 1.0f, 1.0f, 1.0f });
                break;
            case CreateKind::MESH:
            {
                mSceneContext->CreateEntity(entity, "Mesh");
                MeshComponent& meshComponent = entity.AddComponent<MeshComponent>();
                if(request.DefaultMesh)
                    meshComponent.MeshAsset = assetManager->Load<Mesh>(AssetDatabase::Get().Import(request.DefaultMesh, AssetType::MESH));
                break;
            }
            case CreateKind::ENVIRONMENT:
                mSceneContext->CreateEntity(entity, "Environment");
                entity.AddComponent<EnvironmentComponent>();
                break;
            case CreateKind::DIRECTIONAL_LIGHT:
                mSceneContext->CreateEntity(entity, "Directional Light");
                entity.AddComponent<LightComponent>().Type = LightType::DIRECTIONAL;
                break;
            case CreateKind::POINT_LIGHT:
                mSceneContext->CreateEntity(entity, "Point Light");
                entity.AddComponent<LightComponent>().Type = LightType::POINT;
                break;
            case CreateKind::TEXT:
                mSceneContext->CreateEntity(entity, "Text");
                entity.AddComponent<TextComponent>();
                break;
            case CreateKind::UI_CANVAS:
                mSceneContext->CreateEntity(entity, "Canvas");
                entity.AddComponent<UICanvasComponent>();
                break;
            case CreateKind::AUDIO_SOURCE:
                mSceneContext->CreateEntity(entity, "AudioSource");
                entity.AddComponent<AudioSourceComponent>();
                break;
            case CreateKind::AUDIO_LISTENER:
                mSceneContext->CreateEntity(entity, "AudioListener");
                entity.AddComponent<AudioListenerComponent>();
                break;
            case CreateKind::PARTICLE_SYSTEM:
                mSceneContext->CreateEntity(entity, "Particle System");
                entity.AddComponent<ParticleSystemComponent>();
                break;
            case CreateKind::FROM_ASSET:
            {
                const AssetMetadata& meta = assetManager->GetMetadata(request.Asset);
                mSceneContext->CreateEntity(entity, Path(meta.RelativePath).stem().string());
                switch(meta.Type)
                {
                    case AssetType::MESH: entity.AddComponent<MeshComponent>().MeshAsset = assetManager->Load<Mesh>(request.Asset); break;
                    case AssetType::TEXTURE2D: entity.AddComponent<SpriteRendererComponent>(glm::vec4 { 1.0f, 1.0f, 1.0f, 1.0f }).TextureAsset = assetManager->Load<Texture2D>(request.Asset); break;
                    case AssetType::AUDIO: entity.AddComponent<AudioSourceComponent>().AudioClip = assetManager->Load<Audio>(request.Asset); break;
                    case AssetType::UI_LAYOUT: entity.AddComponent<UICanvasComponent>().Layout = assetManager->Load<UILayout>(request.Asset); break;
                    case AssetType::PARTICLE_EFFECT: entity.AddComponent<ParticleSystemComponent>().Effect = assetManager->Load<ParticleEffect>(request.Asset); break;
                    case AssetType::FONT:
                    {
                        TextComponent& text = entity.AddComponent<TextComponent>();
                        text.Text = "Text";
                        text.FontAsset = assetManager->Load<Font>(request.Asset);
                        break;
                    }
                    default: break;
                }
                break;
            }
        }

        if(request.Parent)
            mSceneContext->SetParent(entity, request.Parent);
        editor->OnEntityCreated(entity); // Records "Create" (with its place in the hierarchy), selects and reveals it
    }

    void SceneHierarchyPanel::Select(Entity e, bool reveal)
    {
        mSelectedEntity = e;
        mRevealSelection = reveal;
    }

    void SceneHierarchyPanel::UpdateSearch()
    {
        mSearchChanged = mSearchBuffer != mLastSearchBuffer;
        if(mSearchChanged)
            mLastSearchBuffer = mSearchBuffer;

        // Split "name words t:type t:type"
        mSearchName.clear();
        mSearchTypes.clear();
        size_t start = 0;
        while(start < mSearchBuffer.size())
        {
            const size_t end = std::min(mSearchBuffer.find(' ', start), mSearchBuffer.size());
            String token = mSearchBuffer.substr(start, end - start);
            ToLower(token);
            if(token.size() > 2 && token.starts_with("t:"))
                mSearchTypes.push_back(token.substr(2));
            else if(!token.empty() && token != "t:")
                mSearchName += mSearchName.empty() ? token : " " + token;
            start = end + 1;
        }

        mSearchVisible.clear();
        if(!IsSearching())
            return;

        entt::registry& registry = mSceneContext->GetRegistry();
        String lowered;
        registry.each([&](entt::entity entity) {
            const NameComponent* name = registry.try_get<NameComponent>(entity);
            if(!name || !registry.all_of<RelationshipComponent>(entity))
                return;

            lowered = name->Name;
            ToLower(lowered);
            if(lowered.find(mSearchName) == String::npos || !MatchesTypeFilters(Entity(entity, mSceneContext)))
                return;

            mSearchVisible[entity] = true;

            // Ancestors lead to the match. An ancestor already in the map had its own ancestors added back then
            for(entt::entity parent = static_cast<entt::entity>(registry.get<RelationshipComponent>(entity).Parent); parent != entt::null;
                parent = static_cast<entt::entity>(registry.get<RelationshipComponent>(parent).Parent))
            {
                if(!mSearchVisible.try_emplace(parent, false).second)
                    break;
            }
        });
    }

    bool SceneHierarchyPanel::MatchesTypeFilters(Entity e) const
    {
        for(const String& type : mSearchTypes)
        {
            bool matched = false;
            for(const TypeFilter& filter : sTypeFilters)
            {
                if(std::string_view(filter.Keyword).starts_with(type) && filter.Matches(e))
                {
                    matched = true;
                    break;
                }
            }
            if(!matched)
                return false;
        }
        return true;
    }

    void SceneHierarchyPanel::StartRename(Entity entity)
    {
        if(!entity)
            return;

        mRenamingEntity = entity;
        mRenameBuffer = entity.GetComponent<NameComponent>().Name;
        mFocusRename = true;
        mRenameGraceFrames = 3;
    }

    void SceneHierarchyPanel::DrawRenameField(Entity& e, const ImVec2& position)
    {
        ImGui::SetCursorScreenPos(position);
        if(mFocusRename)
        {
            ImGui::SetKeyboardFocusHere();
            mFocusRename = false;
        }

        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##RenameEntity", &mRenameBuffer, ImGuiInputTextFlags_AutoSelectAll);

        // Enter or clicking elsewhere applies, Esc cancels
        if(ImGui::IsItemDeactivated())
        {
            if(!ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !mRenameBuffer.empty())
                e.GetComponent<NameComponent>().Name = mRenameBuffer;
            mRenamingEntity = {};
        }
        else if(!ImGui::IsItemActive() && --mRenameGraceFrames < 0)
            mRenamingEntity = {}; // Never got the keyboard focus
    }

    void SceneHierarchyPanel::Shutdown()
    {
    }

} // namespace Surge
