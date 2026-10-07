// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/UIEditorPanel.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/HighLevel/Texture2D.hpp"
#include "Surge/Utility/Filesystem.hpp"

#include "Editor.hpp"
#include "Panels/ContentBrowserPanel.hpp"
#include "Utility/ImGuiAux.hpp"

#include <imgui_stdlib.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>

#define UI_EDITOR_WIDGET_PAYLOAD "UI_EDITOR_WIDGET"

namespace Surge
{
    namespace
    {
        constexpr size_t MAX_UNDO_STEPS = 128;
        constexpr float HANDLE_HALF_SIZE = 4.5f;
        constexpr float PROPERTY_LABEL_WIDTH = 125.0f;

        constexpr ImU32 COLOR_OUTLINE = IM_COL32(255, 255, 255, 45);
        constexpr ImU32 COLOR_HOVER = IM_COL32(110, 185, 255, 220);
        constexpr ImU32 COLOR_SELECTED = IM_COL32(255, 168, 0, 255);
        constexpr ImU32 COLOR_PARENT = IM_COL32(255, 168, 0, 70);

        bool ContainsID(const UI::WidgetDesc& node, UUID id)
        {
            if(node.ID == id)
                return true;
            for(const UI::WidgetDesc& child : node.Children)
            {
                if(ContainsID(child, id))
                    return true;
            }
            return false;
        }

        void RegenerateIDs(UI::WidgetDesc& desc)
        {
            desc.ID = UUID();
            for(UI::WidgetDesc& child : desc.Children)
                RegenerateIDs(child);
        }

        void CollectNames(const UI::WidgetDesc& desc, Vector<String>& outNames)
        {
            outNames.push_back(desc.Name);
            for(const UI::WidgetDesc& child : desc.Children)
                CollectNames(child, outNames);
        }

        int CountName(const UI::WidgetDesc& desc, const String& name)
        {
            int count = desc.Name == name ? 1 : 0;
            for(const UI::WidgetDesc& child : desc.Children)
                count += CountName(child, name);
            return count;
        }

        bool IsStretched(const UI::WidgetDesc& desc)
        {
            return desc.AnchorMin.x != desc.AnchorMax.x || desc.AnchorMin.y != desc.AnchorMax.y;
        }

        float SnapValue(float value, float grid)
        {
            return grid > 0.0f ? std::round(value / grid) * grid : value;
        }

        // Property table helpers (label column + value column), all return true when the value changed
        void PropertyLabel(const char* label)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
        }

        bool PropertyVec2(const char* label, glm::vec2& value, float speed, float min = 0.0f, float max = 0.0f, const char* format = "%.1f")
        {
            ImGui::PushID(label);
            PropertyLabel(label);
            const bool changed = ImGui::DragFloat2("##v", glm::value_ptr(value), speed, min, max, format);
            ImGui::PopID();
            return changed;
        }

        bool PropertyFloat(const char* label, float& value, float speed, float min = 0.0f, float max = 0.0f, const char* format = "%.1f")
        {
            ImGui::PushID(label);
            PropertyLabel(label);
            const bool changed = ImGui::DragFloat("##v", &value, speed, min, max, format);
            ImGui::PopID();
            return changed;
        }

        bool PropertyColor(const char* label, glm::vec4& value)
        {
            ImGui::PushID(label);
            PropertyLabel(label);
            const bool changed = ImGui::ColorEdit4("##v", glm::value_ptr(value), ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::PopID();
            return changed;
        }

        bool PropertyBool(const char* label, bool& value)
        {
            ImGui::PushID(label);
            PropertyLabel(label);
            const bool changed = ImGui::Checkbox("##v", &value);
            ImGui::PopID();
            return changed;
        }

        bool PropertyText(const char* label, String& value, bool multiline)
        {
            ImGui::PushID(label);
            PropertyLabel(label);
            const bool changed = multiline ? ImGui::InputTextMultiline("##v", &value, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 3.5f)) : ImGui::InputText("##v", &value);
            ImGui::PopID();
            return changed;
        }

        template <typename E, size_t N>
        bool PropertyCombo(const char* label, E& value, const std::array<const char*, N>& names, const std::array<E, N>& values)
        {
            ImGui::PushID(label);
            PropertyLabel(label);
            bool changed = false;
            size_t current = 0;
            for(size_t i = 0; i < N; i++)
            {
                if(values[i] == value)
                    current = i;
            }

            if(ImGui::BeginCombo("##v", names[current]))
            {
                for(size_t i = 0; i < N; i++)
                {
                    if(ImGui::Selectable(names[i], i == current))
                    {
                        value = values[i];
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopID();
            return changed;
        }

        // Combo of every registered asset of the type, also accepts drops from the Content Browser
        bool PropertyAsset(const char* label, AssetID& id, AssetType type)
        {
            AssetManager* am = Core::GetAssetManager();
            bool changed = false;

            ImGui::PushID(label);
            PropertyLabel(label);

            String preview = "None";
            if(id)
            {
                const AssetMetadata& meta = am->GetMetadata(id);
                preview = meta.Type == type ? Filesystem::GetFilenameWithExt(meta.RelativePath) : String("Missing!");
            }

            if(ImGui::BeginCombo("##v", preview.c_str(), ImGuiComboFlags_HeightLarge))
            {
                if(ImGui::Selectable("None", !id))
                {
                    id = UUID::INVALID;
                    changed = true;
                }

                Vector<std::pair<String, AssetID>> entries;
                for(const auto& [assetID, meta] : am->GetRegistryMap())
                {
                    if(meta.Type == type && !HasFlag(meta.Flags, AssetFlags::MEMORY))
                        entries.emplace_back(meta.RelativePath, assetID);
                }
                std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

                for(const auto& [path, assetID] : entries)
                {
                    if(ImGui::Selectable(path.c_str(), assetID == id))
                    {
                        id = assetID;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }

            if(ImGui::BeginDragDropTarget())
            {
                if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_PAYLOAD))
                {
                    const AssetID dropped = *static_cast<const AssetID*>(payload->Data);
                    if(am->GetMetadata(dropped).Type == type)
                    {
                        id = dropped;
                        changed = true;
                    }
                    else
                        Log<Severity::Warn>("[UIEditor] Dropped asset is not a {}", SurgeReflect::EnumToString(type).data());
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::PopID();
            return changed;
        }

        bool BeginPropertyTable(const char* id)
        {
            if(!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX))
                return false;

            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, PROPERTY_LABEL_WIDTH);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            return true;
        }

        void SectionHeader(const char* title)
        {
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImGuiAux::ScopedBoldFont bold;
            ImGuiAux::ScopedColor color({ ImGuiCol_Separator }, ImGuiAux::Colors::ThemeColor1);
            ImGui::SeparatorText(title);
        }
    }

    // -----------------------------------------------------------------------------------------------
    // Lifetime
    // -----------------------------------------------------------------------------------------------

    void UIEditorPanel::Init(void*)
    {
        mCode = GetStaticCode();
    }

    void UIEditorPanel::OnEvent(Event&) {}

    void UIEditorPanel::Shutdown()
    {
        Core::GetRenderer()->GetUIManager().ClearEditorPreview();
        if(mPreviewRoot)
            mPreviewRoot->Destroy();
        mPreviewRoot = nullptr;
        mPreviewWidgets.clear();
        mLayout = nullptr;
    }

    AssetID UIEditorPanel::CreateLayoutAsset(const std::filesystem::path& absoluteDirectory, const String& baseName)
    {
        AssetManager* am = Core::GetAssetManager();
        Filesystem::CreateOrEnsureDirectories(absoluteDirectory);

        const char* extension = GetExtensionFromAssetType(AssetType::UI_LAYOUT);
        std::filesystem::path path = absoluteDirectory / (baseName + extension);
        for(int count = 1; std::filesystem::exists(path); count++)
            path = absoluteDirectory / std::format("{} ({}){}", baseName, count, extension);

        const String relativePath = Filesystem::GetRelativePath(path, am->GetAssetsDirectory()).generic_string();
        Ref<UILayout> layout = am->Create<UILayout>(relativePath);
        return layout ? layout->GetID() : AssetID(UUID::INVALID);
    }

    void UIEditorPanel::OpenLayout(AssetID layoutID)
    {
        Editor* editor = static_cast<Editor*>(Core::GetClient());
        editor->GetPanelManager().GetAllPanels()[mCode].Show = true;
        mFocusWindow = true;

        if(mLayout && mLayout->GetID() == layoutID)
            return;

        auto open = [this, layoutID]() {
            Ref<UILayout> layout = Core::GetAssetManager()->Load<UILayout>(layoutID);
            if(!layout)
            {
                Log<Severity::Error>("[UIEditor] Failed to load UI Layout {}", layoutID.Get());
                return;
            }
            mLayout = layout;
            LoadFromAsset();
        };

        if(mLayout && mUnsaved)
            ImGuiAux::ShowConfirmationBox("Unsaved UI Layout", "The open UI Layout has unsaved changes.\nDiscard them and open the other layout?", open);
        else
            open();
    }

    void UIEditorPanel::CloseLayout()
    {
        auto close = [this]() {
            Core::GetRenderer()->GetUIManager().ClearEditorPreview();
            if(mPreviewRoot)
                mPreviewRoot->Destroy();
            mPreviewRoot = nullptr;
            mPreviewWidgets.clear();
            mResourceCache.Clear();
            mUndoStack.clear();
            mRedoStack.clear();
            mLayout = nullptr;
            mUnsaved = false;
            mDocumentChanged = false;
            mDragMode = DragMode::NONE;
        };

        if(mUnsaved)
            ImGuiAux::ShowConfirmationBox("Unsaved UI Layout", "Discard unsaved changes?", close);
        else
            close();
    }

    void UIEditorPanel::LoadFromAsset()
    {
        mRoot = mLayout->Root;
        mScaler = mLayout->Scaler;
        mSelected = UUID::INVALID;
        mUndoStack.clear();
        mRedoStack.clear();
        mCommitted = TakeSnapshot();
        mDocumentChanged = false;
        mUnsaved = false;
        mDragMode = DragMode::NONE;
        mResourceCache.Clear();
        mNeedsRebuild = true;
    }

    void UIEditorPanel::Save()
    {
        if(!mLayout)
            return;

        mLayout->Root = mRoot;
        mLayout->Scaler = mScaler;
        Core::GetAssetManager()->Save(mLayout->GetID());
        mUnsaved = false;
        Log<Severity::Info>("[UIEditor] Saved {}", Core::GetAssetManager()->GetMetadata(mLayout->GetID()).RelativePath);
    }

    // -----------------------------------------------------------------------------------------------
    // Undo/Redo
    // -----------------------------------------------------------------------------------------------

    void UIEditorPanel::CommitUndoIfIdle()
    {
        if(!mLayout || !mDocumentChanged)
            return;

        // Coalesce: a whole drag/typing session becomes a single undo step
        if(mDragMode != DragMode::NONE || ImGui::IsAnyItemActive() || ImGui::IsMouseDown(ImGuiMouseButton_Left))
            return;

        mUndoStack.push_back(std::move(mCommitted));
        if(mUndoStack.size() > MAX_UNDO_STEPS)
            mUndoStack.erase(mUndoStack.begin());

        mRedoStack.clear();
        mCommitted = TakeSnapshot();
        mDocumentChanged = false;
    }

    void UIEditorPanel::Undo()
    {
        if(mDocumentChanged) // Pending edit becomes its own step first
        {
            mUndoStack.push_back(std::move(mCommitted));
            mCommitted = TakeSnapshot();
            mDocumentChanged = false;
        }

        if(mUndoStack.empty())
            return;

        mRedoStack.push_back(TakeSnapshot());
        Snapshot snapshot = std::move(mUndoStack.back());
        mUndoStack.pop_back();
        RestoreSnapshot(snapshot);
        mCommitted = std::move(snapshot);
    }

    void UIEditorPanel::Redo()
    {
        if(mRedoStack.empty())
            return;

        mUndoStack.push_back(TakeSnapshot());
        Snapshot snapshot = std::move(mRedoStack.back());
        mRedoStack.pop_back();
        RestoreSnapshot(snapshot);
        mCommitted = std::move(snapshot);
    }

    void UIEditorPanel::RestoreSnapshot(const Snapshot& snapshot)
    {
        mRoot = snapshot.Root;
        mScaler = snapshot.Scaler;
        mSelected = mRoot.Find(snapshot.Selected) ? snapshot.Selected : UUID(UUID::INVALID);
        mDocumentChanged = false;
        mUnsaved = true;
        mNeedsRebuild = true;
        mDragMode = DragMode::NONE;
    }

    // -----------------------------------------------------------------------------------------------
    // Preview
    // -----------------------------------------------------------------------------------------------

    glm::vec2 UIEditorPanel::GetTargetResolution() const
    {
        const glm::vec2& target = Core::GetRenderer()->GetUIManager().GetTargetResolution();
        return (target.x > 1.0f && target.y > 1.0f) ? target : mScaler.ReferenceResolution;
    }

    void UIEditorPanel::RebuildPreview()
    {
        if(mPreviewRoot)
            mPreviewRoot->Destroy();

        mPreviewWidgets.clear();
        mPreviewRoot = UI::Instantiate(mRoot, mResourceCache, &mPreviewWidgets);
        mNeedsRebuild = false;
        UpdatePreviewLayout();

        // Re-register right away, the renderer would otherwise draw the destroyed tree for a frame
        Editor* editor = static_cast<Editor*>(Core::GetClient());
        UI::Manager& uiManager = Core::GetRenderer()->GetUIManager();
        if(mEditInViewport && !editor->IsPlaying())
            uiManager.SetEditorPreview(mPreviewRoot, mScaler);
        else
            uiManager.ClearEditorPreview();
    }

    void UIEditorPanel::UpdatePreviewLayout()
    {
        if(!mPreviewRoot)
            return;

        const glm::vec2 target = GetTargetResolution();
        mPreviewRoot->LayoutAsRoot(target, mScaler.ComputeScale(target));
    }

    bool UIEditorPanel::GetWidgetRect(UUID id, glm::vec2& outMin, glm::vec2& outMax) const
    {
        auto it = mPreviewWidgets.find(id.Get());
        if(it == mPreviewWidgets.end())
            return false;

        glm::vec2 pos, size;
        it->second->GetGlobalBounds(pos, size);
        outMin = pos;
        outMax = pos + size;
        return true;
    }

    bool UIEditorPanel::GetParentRectUI(UUID id, glm::vec2& outMin, glm::vec2& outSize) const
    {
        const UI::WidgetDesc* parent = const_cast<UI::WidgetDesc&>(mRoot).FindParentOf(id);
        if(!parent)
            return false;

        const glm::vec2 target = GetTargetResolution();
        const float scale = mScaler.ComputeScale(target);

        glm::vec2 min, max;
        if(!GetWidgetRect(parent->ID, min, max))
            return false;

        outMin = min / scale;
        outSize = (max - min) / scale;
        return true;
    }

    void UIEditorPanel::SetRectFromBounds(UI::WidgetDesc& desc, const glm::vec2& rectMin, const glm::vec2& rectMax, const glm::vec2& parentMin, const glm::vec2& parentSize) const
    {
        // Inverse of UI::Widget::UpdateLayout, in UI units
        const glm::vec2 span = desc.AnchorMax - desc.AnchorMin;
        const glm::vec2 size = rectMax - rectMin;
        desc.Size = size - parentSize * span;

        const glm::vec2 pivotPoint = rectMin + size * desc.Pivot;
        desc.Offset = pivotPoint - (parentMin + parentSize * (desc.AnchorMin + span * desc.Pivot));
    }

    // -----------------------------------------------------------------------------------------------
    // Panel
    // -----------------------------------------------------------------------------------------------

    void UIEditorPanel::Render(bool* show)
    {
        Editor* editor = static_cast<Editor*>(Core::GetClient());
        UI::Manager& uiManager = Core::GetRenderer()->GetUIManager();
        mOwnsShortcuts = false;

        // Keep the viewport preview alive even while this window is hidden/docked away
        if(mLayout)
        {
            if(mNeedsRebuild)
                RebuildPreview();
            UpdatePreviewLayout();

            if(mEditInViewport && !editor->IsPlaying())
                uiManager.SetEditorPreview(mPreviewRoot, mScaler);
            else
                uiManager.ClearEditorPreview();
        }
        else
            uiManager.ClearEditorPreview();

        if(!*show)
        {
            CommitUndoIfIdle();
            return;
        }

        if(mFocusWindow)
        {
            ImGui::SetNextWindowFocus();
            mFocusWindow = false;
        }

        if(ImGui::Begin(PanelCodeToString(mCode), show))
        {
            const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
            mOwnsShortcuts = mLayout && (focused || mViewportHovered);

            if(!mLayout)
                DrawEmptyState();
            else
            {
                DrawToolbar();
                ImGui::Separator();

                if(ImGui::BeginTable("##UIEditorSplit", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV, ImVec2(0.0f, ImGui::GetContentRegionAvail().y)))
                {
                    ImGui::TableSetupColumn("Hierarchy", ImGuiTableColumnFlags_WidthStretch, 0.4f);
                    ImGui::TableSetupColumn("Properties", ImGuiTableColumnFlags_WidthStretch, 0.6f);
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    if(ImGui::BeginChild("##UIHierarchy"))
                        DrawHierarchy();
                    ImGui::EndChild();

                    ImGui::TableSetColumnIndex(1);
                    if(ImGui::BeginChild("##UIProperties"))
                        DrawProperties();
                    ImGui::EndChild();

                    ImGui::EndTable();
                }

                ApplyPendingActions();
                HandleShortcuts(focused || mViewportHovered);
            }
        }
        ImGui::End();

        CommitUndoIfIdle();
    }

    void UIEditorPanel::DrawEmptyState()
    {
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        {
            ImGuiAux::ScopedBoldFont bold(20.0f);
            ImGuiAux::TextCentered("No UI Layout open");
        }
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::TextWrapped("Double click a .sui file in the Content Browser, drop one on the button below, or create a new layout. "
                           "Add a UI Canvas Component to an entity and assign the layout to show it in game.");
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        ImGuiAux::ScopedBoldFont bold;
        if(ImGui::Button("Create New UI Layout", ImVec2(-FLT_MIN, 40.0f)))
        {
            const String assetsDir = Core::GetAssetManager()->GetAssetsDirectory();
            const AssetID id = CreateLayoutAsset(std::filesystem::path(assetsDir) / "UI");
            if(id)
                OpenLayout(id);
        }

        if(ImGui::BeginDragDropTarget())
        {
            if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_PAYLOAD))
            {
                const AssetID dropped = *static_cast<const AssetID*>(payload->Data);
                if(Core::GetAssetManager()->GetMetadata(dropped).Type == AssetType::UI_LAYOUT)
                    OpenLayout(dropped);
            }
            ImGui::EndDragDropTarget();
        }
    }

    void UIEditorPanel::DrawToolbar()
    {
        Editor* editor = static_cast<Editor*>(Core::GetClient());
        const String fileName = Filesystem::GetFilenameWithExt(Core::GetAssetManager()->GetMetadata(mLayout->GetID()).RelativePath);

        {
            ImGuiAux::ScopedBoldFont bold(18.0f);
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1.0f), "%s%s", fileName.c_str(), mUnsaved ? " *" : "");
        }

        ImGui::SameLine();
        if(ImGui::Button("Save"))
            Save();
        ImGuiAux::DelayedToolTip("Ctrl+S");

        ImGui::SameLine();
        ImGui::BeginDisabled(mUndoStack.empty() && !mDocumentChanged);
        if(ImGui::Button("Undo"))
            Undo();
        ImGui::EndDisabled();
        ImGuiAux::DelayedToolTip("Ctrl+Z");

        ImGui::SameLine();
        ImGui::BeginDisabled(mRedoStack.empty());
        if(ImGui::Button("Redo"))
            Redo();
        ImGui::EndDisabled();
        ImGuiAux::DelayedToolTip("Ctrl+Y / Ctrl+Shift+Z");

        ImGui::SameLine();
        if(ImGui::Button("Close"))
            CloseLayout();

        // Second row: creation + view options
        if(ImGui::Button("+ Add Widget"))
            ImGui::OpenPopup("##AddWidgetPopup");

        if(ImGui::BeginPopup("##AddWidgetPopup"))
        {
            // New widgets go into the selected Panel/Image (or next to the selected widget)
            UUID parent = mRoot.ID;
            if(const UI::WidgetDesc* selected = mRoot.Find(mSelected))
            {
                if(selected->Type == UI::WidgetType::BASE_WIDGET || selected->Type == UI::WidgetType::IMAGE)
                    parent = selected->ID;
                else if(const UI::WidgetDesc* selectedParent = mRoot.FindParentOf(mSelected))
                    parent = selectedParent->ID;
            }

            for(UI::WidgetType type : { UI::WidgetType::BASE_WIDGET, UI::WidgetType::IMAGE, UI::WidgetType::TEXT, UI::WidgetType::BUTTON, UI::WidgetType::IMAGE_BUTTON })
            {
                if(ImGui::MenuItem(UI::WidgetTypeToDisplayName(type)))
                    QueueAction({ .Op = PendingAction::Type::ADD_WIDGET, .NewParent = parent, .NewWidgetType = type });
            }
            ImGui::EndPopup();
        }

        const bool canEditSelection = mSelected && mSelected != mRoot.ID && mRoot.Find(mSelected);
        ImGui::SameLine();
        ImGui::BeginDisabled(!canEditSelection);
        if(ImGui::Button("Duplicate"))
            QueueAction({ .Op = PendingAction::Type::DUPLICATE_WIDGET, .Target = mSelected });
        ImGuiAux::DelayedToolTip("Ctrl+D");
        ImGui::SameLine();
        if(ImGui::Button("Delete"))
            QueueAction({ .Op = PendingAction::Type::REMOVE_WIDGET, .Target = mSelected });
        ImGuiAux::DelayedToolTip("Delete");
        ImGui::SameLine();
        if(ImGui::ArrowButton("##MoveUp", ImGuiDir_Up))
            QueueAction({ .Op = PendingAction::Type::MOVE_WIDGET, .Target = mSelected, .Direction = -1 });
        ImGuiAux::DelayedToolTip("Move up (drawn earlier, behind its siblings)");
        ImGui::SameLine();
        if(ImGui::ArrowButton("##MoveDown", ImGuiDir_Down))
            QueueAction({ .Op = PendingAction::Type::MOVE_WIDGET, .Target = mSelected, .Direction = 1 });
        ImGuiAux::DelayedToolTip("Move down (drawn later, on top of its siblings)");
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        if(ImGui::Checkbox("Edit in Viewport", &mEditInViewport))
            mNeedsRebuild = true;
        ImGuiAux::DelayedToolTip("Show the layout in the Viewport and select/move/resize widgets there");

        ImGui::SameLine();
        ImGui::Checkbox("Outlines", &mShowAllOutlines);
        ImGui::SameLine();
        ImGui::Checkbox("Snap", &mSnapToGrid);
        ImGuiAux::DelayedToolTip("Snap offsets and sizes to the grid while dragging (hold Ctrl to toggle temporarily)");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60.0f);
        ImGui::DragFloat("##Grid", &mGridSize, 0.5f, 1.0f, 200.0f, "%.0f");

        if(editor->IsPlaying())
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Playing: the game uses the last saved version of this layout");
    }

    // -----------------------------------------------------------------------------------------------
    // Hierarchy
    // -----------------------------------------------------------------------------------------------

    void UIEditorPanel::DrawHierarchy()
    {
        DrawHierarchyNode(mRoot, true);

        // Empty area: deselect + add to root
        ImGui::Dummy(ImGui::GetContentRegionAvail());
        if(ImGui::IsItemClicked(ImGuiMouseButton_Left))
            mSelected = UUID::INVALID;

        if(ImGui::BeginPopupContextItem("##HierarchyBackground", ImGuiPopupFlags_MouseButtonRight))
        {
            for(UI::WidgetType type : { UI::WidgetType::BASE_WIDGET, UI::WidgetType::IMAGE, UI::WidgetType::TEXT, UI::WidgetType::BUTTON, UI::WidgetType::IMAGE_BUTTON })
            {
                if(ImGui::MenuItem(std::format("Add {}", UI::WidgetTypeToDisplayName(type)).c_str()))
                    QueueAction({ .Op = PendingAction::Type::ADD_WIDGET, .NewParent = mRoot.ID, .NewWidgetType = type });
            }
            ImGui::EndPopup();
        }

        if(ImGui::BeginDragDropTarget()) // Drop on empty space: move to the canvas root
        {
            if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(UI_EDITOR_WIDGET_PAYLOAD))
                QueueAction({ .Op = PendingAction::Type::REPARENT_WIDGET, .Target = UUID(*static_cast<const uint64_t*>(payload->Data)), .NewParent = mRoot.ID });
            ImGui::EndDragDropTarget();
        }
    }

    void UIEditorPanel::DrawHierarchyNode(UI::WidgetDesc& desc, bool isRoot)
    {
        ImGui::PushID(reinterpret_cast<const void*>(static_cast<uintptr_t>(desc.ID.Get())));

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_DefaultOpen;
        if(desc.Children.empty())
            flags |= ImGuiTreeNodeFlags_Leaf;
        if(desc.ID == mSelected)
            flags |= ImGuiTreeNodeFlags_Selected;

        bool open = false;
        if(mRenaming == desc.ID)
        {
            ImGui::SetNextItemWidth(-FLT_MIN);
            if(mRenameFocus)
            {
                ImGui::SetKeyboardFocusHere();
                mRenameFocus = false;
            }
            const bool commit = ImGui::InputText("##Rename", &mRenameBuffer, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            if(commit || ImGui::IsItemDeactivated())
            {
                if(!mRenameBuffer.empty() && mRenameBuffer != desc.Name)
                {
                    desc.Name = mRenameBuffer;
                    MarkChanged();
                }
                mRenaming = UUID::INVALID;
            }
            open = !desc.Children.empty();
            if(open)
                ImGui::TreePush("##RenameChildren");
        }
        else
        {
            const String label = desc.Name.empty() ? std::format("<{}>", UI::WidgetTypeToDisplayName(desc.Type)) : desc.Name;
            if(!desc.Visible)
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            open = ImGui::TreeNodeEx("##Node", flags, "%s", label.c_str());
            if(!desc.Visible)
                ImGui::PopStyleColor();

            if(ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
                mSelected = desc.ID;

            if(!isRoot && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                mRenaming = desc.ID;
                mRenameBuffer = desc.Name;
                mRenameFocus = true;
            }

            // Drag to re-parent
            if(!isRoot && ImGui::BeginDragDropSource())
            {
                const uint64_t id = desc.ID.Get();
                ImGui::SetDragDropPayload(UI_EDITOR_WIDGET_PAYLOAD, &id, sizeof(uint64_t));
                ImGui::Text("%s", label.c_str());
                ImGui::EndDragDropSource();
            }

            if(ImGui::BeginDragDropTarget())
            {
                if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(UI_EDITOR_WIDGET_PAYLOAD))
                    QueueAction({ .Op = PendingAction::Type::REPARENT_WIDGET, .Target = UUID(*static_cast<const uint64_t*>(payload->Data)), .NewParent = desc.ID });
                ImGui::EndDragDropTarget();
            }

            // Context menu
            if(ImGui::BeginPopupContextItem("##NodeContext"))
            {
                mSelected = desc.ID;
                if(ImGui::BeginMenu("Add Child"))
                {
                    for(UI::WidgetType type : { UI::WidgetType::BASE_WIDGET, UI::WidgetType::IMAGE, UI::WidgetType::TEXT, UI::WidgetType::BUTTON, UI::WidgetType::IMAGE_BUTTON })
                    {
                        if(ImGui::MenuItem(UI::WidgetTypeToDisplayName(type)))
                            QueueAction({ .Op = PendingAction::Type::ADD_WIDGET, .NewParent = desc.ID, .NewWidgetType = type });
                    }
                    ImGui::EndMenu();
                }

                if(!isRoot)
                {
                    if(ImGui::MenuItem("Rename", "Double Click"))
                    {
                        mRenaming = desc.ID;
                        mRenameBuffer = desc.Name;
                        mRenameFocus = true;
                    }
                    if(ImGui::MenuItem("Duplicate", "Ctrl+D"))
                        QueueAction({ .Op = PendingAction::Type::DUPLICATE_WIDGET, .Target = desc.ID });
                    if(ImGui::MenuItem("Move Up"))
                        QueueAction({ .Op = PendingAction::Type::MOVE_WIDGET, .Target = desc.ID, .Direction = -1 });
                    if(ImGui::MenuItem("Move Down"))
                        QueueAction({ .Op = PendingAction::Type::MOVE_WIDGET, .Target = desc.ID, .Direction = 1 });
                    ImGui::Separator();
                    if(ImGui::MenuItem("Delete", "Del"))
                        QueueAction({ .Op = PendingAction::Type::REMOVE_WIDGET, .Target = desc.ID });
                }
                ImGui::EndPopup();
            }

            // Type tag
            ImGui::SameLine();
            ImGui::TextDisabled("%s", UI::WidgetTypeToDisplayName(desc.Type));
        }

        if(open)
        {
            for(UI::WidgetDesc& child : desc.Children)
                DrawHierarchyNode(child, false);
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    String UIEditorPanel::MakeUniqueName(const String& base) const
    {
        Vector<String> names;
        CollectNames(mRoot, names);
        if(std::find(names.begin(), names.end(), base) == names.end())
            return base;

        for(int i = 1;; i++)
        {
            String candidate = std::format("{} ({})", base, i);
            if(std::find(names.begin(), names.end(), candidate) == names.end())
                return candidate;
        }
    }

    AssetID UIEditorPanel::GetDefaultFont() const
    {
        AssetManager* am = Core::GetAssetManager();
        AssetID best = UUID::INVALID;
        String bestPath;
        for(const auto& [id, meta] : am->GetRegistryMap())
        {
            if(meta.Type != AssetType::FONT || HasFlag(meta.Flags, AssetFlags::MEMORY))
                continue;
            if(!best || meta.RelativePath < bestPath)
            {
                best = id;
                bestPath = meta.RelativePath;
            }
        }
        return best;
    }

    void UIEditorPanel::ApplyPendingActions()
    {
        if(mPendingActions.empty())
            return;

        // Pointers into the tree are re-fetched after every mutation, vectors may reallocate
        for(const PendingAction& action : mPendingActions)
        {
            switch(action.Op)
            {
                case PendingAction::Type::ADD_WIDGET:
                {
                    UI::WidgetDesc* parent = mRoot.Find(action.NewParent);
                    if(!parent)
                        parent = &mRoot;

                    UI::WidgetDesc widget = UI::WidgetDesc::CreateDefault(action.NewWidgetType, MakeUniqueName(UI::WidgetTypeToDisplayName(action.NewWidgetType)), GetDefaultFont());
                    mSelected = widget.ID;
                    parent->Children.push_back(std::move(widget));
                    MarkChanged();
                    break;
                }

                case PendingAction::Type::REMOVE_WIDGET:
                {
                    if(action.Target == mRoot.ID)
                        break;

                    UI::WidgetDesc* parent = mRoot.FindParentOf(action.Target);
                    if(!parent)
                        break;

                    auto it = std::find_if(parent->Children.begin(), parent->Children.end(), [&](const UI::WidgetDesc& c) { return c.ID == action.Target; });
                    if(ContainsID(*it, mSelected))
                        mSelected = parent->ID;
                    parent->Children.erase(it);
                    MarkChanged();
                    break;
                }

                case PendingAction::Type::DUPLICATE_WIDGET:
                {
                    UI::WidgetDesc* parent = mRoot.FindParentOf(action.Target);
                    if(!parent)
                        break;

                    auto it = std::find_if(parent->Children.begin(), parent->Children.end(), [&](const UI::WidgetDesc& c) { return c.ID == action.Target; });
                    UI::WidgetDesc copy = *it;
                    RegenerateIDs(copy);
                    copy.Name = MakeUniqueName(copy.Name);
                    mSelected = copy.ID;
                    parent->Children.insert(it + 1, std::move(copy));
                    MarkChanged();
                    break;
                }

                case PendingAction::Type::REPARENT_WIDGET:
                {
                    UI::WidgetDesc* target = mRoot.Find(action.Target);
                    UI::WidgetDesc* oldParent = mRoot.FindParentOf(action.Target);
                    if(!target || !oldParent || action.Target == mRoot.ID || ContainsID(*target, action.NewParent) || oldParent->ID == action.NewParent)
                        break;

                    // Keep the widget where it is on screen
                    const float scale = mScaler.ComputeScale(GetTargetResolution());
                    glm::vec2 rectMin, rectMax, parentMin, parentMax;
                    const bool haveRects = GetWidgetRect(action.Target, rectMin, rectMax) && GetWidgetRect(action.NewParent, parentMin, parentMax);

                    UI::WidgetDesc moved = *target;
                    oldParent->Children.erase(std::find_if(oldParent->Children.begin(), oldParent->Children.end(), [&](const UI::WidgetDesc& c) { return c.ID == action.Target; }));

                    if(haveRects)
                        SetRectFromBounds(moved, rectMin / scale, rectMax / scale, parentMin / scale, (parentMax - parentMin) / scale);

                    UI::WidgetDesc* newParent = mRoot.Find(action.NewParent);
                    (newParent ? newParent : &mRoot)->Children.push_back(std::move(moved));
                    mSelected = action.Target;
                    MarkChanged();
                    break;
                }

                case PendingAction::Type::MOVE_WIDGET:
                {
                    UI::WidgetDesc* parent = mRoot.FindParentOf(action.Target);
                    if(!parent)
                        break;

                    auto it = std::find_if(parent->Children.begin(), parent->Children.end(), [&](const UI::WidgetDesc& c) { return c.ID == action.Target; });
                    const int index = static_cast<int>(it - parent->Children.begin());
                    const int newIndex = index + action.Direction;
                    if(newIndex < 0 || newIndex >= static_cast<int>(parent->Children.size()))
                        break;

                    std::swap(parent->Children[index], parent->Children[newIndex]);
                    MarkChanged();
                    break;
                }
            }
        }
        mPendingActions.clear();
    }

    // -----------------------------------------------------------------------------------------------
    // Properties
    // -----------------------------------------------------------------------------------------------

    void UIEditorPanel::DrawProperties()
    {
        UI::WidgetDesc* desc = mRoot.Find(mSelected);
        if(!desc)
        {
            ImGui::TextDisabled("Select a widget in the hierarchy or in the Viewport");
            DrawCanvasSettings();
            return;
        }

        if(desc->ID == mRoot.ID)
        {
            DrawCanvasSettings();
            return;
        }

        {
            ImGuiAux::ScopedBoldFont bold(18.0f);
            ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1.0f), "%s", UI::WidgetTypeToDisplayName(desc->Type));
        }

        if(BeginPropertyTable("##General"))
        {
            if(PropertyText("Name", desc->Name, false))
                MarkChanged();
            if(PropertyBool("Visible", desc->Visible))
                MarkChanged();
            ImGui::EndTable();
        }

        if(desc->Name.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "Give the widget a name to find it from Lua");
        else if(CountName(mRoot, desc->Name) > 1)
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "Name is not unique, FindWidget returns the first match");

        SectionHeader("Rect Transform");
        DrawAnchorPresets(*desc);
        DrawRectTransform(*desc);

        SectionHeader(UI::WidgetTypeToDisplayName(desc->Type));
        DrawWidgetSettings(*desc);

        // Scripting hint
        if(!desc->Name.empty())
        {
            SectionHeader("Scripting");
            String snippet;
            switch(desc->Type)
            {
                case UI::WidgetType::BUTTON:
                case UI::WidgetType::IMAGE_BUTTON:
                    snippet = std::format("local widget = entity.UICanvasC:FindWidget(\"{}\")\nwidget:OnClick(function(self) Log(\"Clicked!\") end)", desc->Name);
                    break;
                case UI::WidgetType::TEXT:
                    snippet = std::format("local widget = entity.UICanvasC:FindWidget(\"{}\")\nwidget.Text = \"Hello\"", desc->Name);
                    break;
                default:
                    snippet = std::format("local widget = entity.UICanvasC:FindWidget(\"{}\")\nwidget.Visible = false", desc->Name);
                    break;
            }

            ImGui::InputTextMultiline("##Snippet", &snippet, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 3.2f), ImGuiInputTextFlags_ReadOnly);
            if(ImGui::Button("Copy Lua Snippet"))
                ImGui::SetClipboardText(snippet.c_str());
        }
    }

    void UIEditorPanel::DrawCanvasSettings()
    {
        SectionHeader("Canvas Scaler");
        ImGui::TextWrapped("The layout is designed at the reference resolution and scaled to the screen. "
                           "Match Height (1) keeps sizes relative to the screen height (landscape), Match Width (0) to the width (portrait).");

        if(BeginPropertyTable("##Canvas"))
        {
            PropertyLabel("Preset");
            struct ResolutionPreset { const char* Name; glm::vec2 Resolution; float Match; };
            constexpr ResolutionPreset presets[] = {
                { "1920 x 1080 (Landscape)", { 1920.0f, 1080.0f }, 1.0f },
                { "1080 x 1920 (Portrait)", { 1080.0f, 1920.0f }, 0.0f },
                { "2400 x 1080 (Phone Landscape)", { 2400.0f, 1080.0f }, 1.0f },
                { "1080 x 2400 (Phone Portrait)", { 1080.0f, 2400.0f }, 0.0f },
                { "1280 x 720", { 1280.0f, 720.0f }, 1.0f },
            };
            if(ImGui::BeginCombo("##Preset", "Choose..."))
            {
                for(const ResolutionPreset& preset : presets)
                {
                    if(ImGui::Selectable(preset.Name))
                    {
                        mScaler.ReferenceResolution = preset.Resolution;
                        mScaler.MatchWidthOrHeight = preset.Match;
                        MarkChanged();
                    }
                }
                ImGui::EndCombo();
            }

            if(PropertyVec2("Reference Res", mScaler.ReferenceResolution, 1.0f, 1.0f, 8192.0f, "%.0f"))
                MarkChanged();

            ImGui::PushID("Match");
            PropertyLabel("Match W/H");
            if(ImGui::SliderFloat("##v", &mScaler.MatchWidthOrHeight, 0.0f, 1.0f, mScaler.MatchWidthOrHeight < 0.5f ? "%.2f (Width)" : "%.2f (Height)"))
                MarkChanged();
            ImGui::PopID();
            ImGui::EndTable();
        }

        const glm::vec2 target = GetTargetResolution();
        ImGui::TextDisabled("Viewport: %.0f x %.0f  |  Scale: %.3f", target.x, target.y, mScaler.ComputeScale(target));
        ImGui::TextDisabled("Tip: use the Viewport aspect ratio dropdown to preview other devices");
    }

    void UIEditorPanel::DrawAnchorPresets(UI::WidgetDesc& desc)
    {
        // Unity style 4x4 grid: columns = left/center/right/stretch, rows = top/middle/bottom/stretch
        constexpr float anchorValues[4][2] = { { 0.0f, 0.0f }, { 0.5f, 0.5f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
        constexpr const char* columnNames[4] = { "Left", "Center", "Right", "Stretch" };
        constexpr const char* rowNames[4] = { "Top", "Middle", "Bottom", "Stretch" };
        constexpr float cellSize = 26.0f;
        constexpr float cellSpacing = 3.0f;

        ImGui::TextDisabled("Anchor Presets (Shift: also pivot, Alt: also position)");
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImGuiIO& io = ImGui::GetIO();

        for(int row = 0; row < 4; row++)
        {
            for(int column = 0; column < 4; column++)
            {
                if(column > 0)
                    ImGui::SameLine(0.0f, cellSpacing);

                const glm::vec2 anchorMin = { anchorValues[column][0], anchorValues[row][0] };
                const glm::vec2 anchorMax = { anchorValues[column][1], anchorValues[row][1] };
                const bool isCurrent = desc.AnchorMin == anchorMin && desc.AnchorMax == anchorMax;

                ImGui::PushID(row * 4 + column);
                const bool clicked = ImGui::InvisibleButton("##Anchor", ImVec2(cellSize, cellSize));
                const bool hovered = ImGui::IsItemHovered();
                if(hovered)
                    ImGui::SetTooltip("%s / %s", rowNames[row], columnNames[column]);

                // Icon: outer box = parent, inner marker = anchor (lines for stretched axes)
                const ImVec2 min = ImGui::GetItemRectMin();
                const ImVec2 max = ImGui::GetItemRectMax();
                drawList->AddRectFilled(min, max, isCurrent ? IM_COL32(255, 168, 0, 90) : (hovered ? IM_COL32(80, 80, 80, 255) : IM_COL32(45, 45, 45, 255)), 3.0f);
                const ImVec2 innerMin = { min.x + 5.0f, min.y + 5.0f };
                const ImVec2 innerMax = { max.x - 5.0f, max.y - 5.0f };
                drawList->AddRect(innerMin, innerMax, IM_COL32(150, 150, 150, 255));

                const ImU32 markerColor = isCurrent ? IM_COL32(255, 200, 80, 255) : IM_COL32(230, 230, 230, 255);
                const float innerW = innerMax.x - innerMin.x;
                const float innerH = innerMax.y - innerMin.y;
                const float x0 = innerMin.x + innerW * anchorMin.x, x1 = innerMin.x + innerW * anchorMax.x;
                const float y0 = innerMin.y + innerH * anchorMin.y, y1 = innerMin.y + innerH * anchorMax.y;
                if(column == 3 || row == 3)
                {
                    if(column == 3) drawList->AddLine({ x0, (y0 + y1) * 0.5f }, { x1, (y0 + y1) * 0.5f }, markerColor, 2.0f);
                    if(row == 3)    drawList->AddLine({ (x0 + x1) * 0.5f, y0 }, { (x0 + x1) * 0.5f, y1 }, markerColor, 2.0f);
                }
                else
                    drawList->AddRectFilled({ x0 - 2.5f, y0 - 2.5f }, { x0 + 2.5f, y0 + 2.5f }, markerColor);

                if(clicked)
                {
                    glm::vec2 rectMin, rectMax, parentMin, parentSize;
                    const float scale = mScaler.ComputeScale(GetTargetResolution());
                    const bool haveRects = GetWidgetRect(desc.ID, rectMin, rectMax) && GetParentRectUI(desc.ID, parentMin, parentSize);

                    desc.AnchorMin = anchorMin;
                    desc.AnchorMax = anchorMax;
                    if(io.KeyShift)
                        desc.Pivot = { column == 3 ? 0.5f : anchorMin.x, row == 3 ? 0.5f : anchorMin.y };

                    if(io.KeyAlt)
                    {
                        // Snap onto the anchor: no offset, stretched axes fill the parent
                        desc.Offset = { 0.0f, 0.0f };
                        if(column == 3) desc.Size.x = 0.0f;
                        if(row == 3)    desc.Size.y = 0.0f;
                    }
                    else if(haveRects)
                        SetRectFromBounds(desc, rectMin / scale, rectMax / scale, parentMin, parentSize); // Keep the widget where it is

                    MarkChanged();
                }
                ImGui::PopID();
            }
        }
    }

    void UIEditorPanel::DrawRectTransform(UI::WidgetDesc& desc)
    {
        if(!BeginPropertyTable("##RectTransform"))
            return;

        bool changed = false;
        changed |= PropertyVec2("Anchor Min", desc.AnchorMin, 0.005f, 0.0f, 1.0f, "%.3f");
        changed |= PropertyVec2("Anchor Max", desc.AnchorMax, 0.005f, 0.0f, 1.0f, "%.3f");
        changed |= PropertyVec2("Pivot", desc.Pivot, 0.005f, 0.0f, 1.0f, "%.3f");
        changed |= PropertyVec2("Offset", desc.Offset, 1.0f);
        changed |= PropertyVec2(IsStretched(desc) ? "Size (+Stretch)" : "Size", desc.Size, 1.0f);
        ImGui::EndTable();

        if(changed)
            MarkChanged();

        if(IsStretched(desc))
            ImGui::TextDisabled("Stretched axes: Size is added to the parent size (negative = inset)");
    }

    void UIEditorPanel::DrawWidgetSettings(UI::WidgetDesc& desc)
    {
        constexpr std::array<const char*, 3> hAlignNames = { "Left", "Center", "Right" };
        constexpr std::array<TextAlignment, 3> hAlignValues = { TextAlignment::LEFT, TextAlignment::CENTER, TextAlignment::RIGHT };
        constexpr std::array<const char*, 3> vAlignNames = { "Top", "Center", "Bottom" };
        constexpr std::array<TextVerticalAlignment, 3> vAlignValues = { TextVerticalAlignment::TOP, TextVerticalAlignment::CENTER, TextVerticalAlignment::BOTTOM };

        if(desc.Type == UI::WidgetType::BASE_WIDGET)
        {
            ImGui::TextDisabled("Empty container, draws nothing. Use it to group and anchor widgets.");
            return;
        }

        bool changed = false;
        bool setNativeSize = false;
        if(BeginPropertyTable("##WidgetSettings"))
        {
            const bool hasText = desc.Type == UI::WidgetType::TEXT || desc.Type == UI::WidgetType::BUTTON;
            const bool hasTexture = desc.Type == UI::WidgetType::IMAGE || desc.Type == UI::WidgetType::BUTTON || desc.Type == UI::WidgetType::IMAGE_BUTTON;
            const bool hasStates = desc.Type == UI::WidgetType::BUTTON || desc.Type == UI::WidgetType::IMAGE_BUTTON;

            if(hasText)
            {
                changed |= PropertyText(desc.Type == UI::WidgetType::BUTTON ? "Label" : "Text", desc.Text, desc.Type == UI::WidgetType::TEXT);
                changed |= PropertyAsset("Font", desc.Font, AssetType::FONT);
                changed |= PropertyFloat("Font Size", desc.FontSize, 0.25f, 1.0f, 512.0f);
                changed |= PropertyCombo("Align", desc.HAlign, hAlignNames, hAlignValues);
                changed |= PropertyCombo("Vertical Align", desc.VAlign, vAlignNames, vAlignValues);
                if(desc.Type == UI::WidgetType::TEXT)
                {
                    changed |= PropertyColor("Color", desc.Color);
                    changed |= PropertyBool("Word Wrap", desc.WordWrap);
                }
                else
                {
                    changed |= PropertyColor("Label Color", desc.TextColor);
                    changed |= PropertyFloat("Label Padding", desc.TextPadding, 0.25f, 0.0f, 500.0f);
                }
            }

            if(hasTexture)
            {
                changed |= PropertyAsset("Texture", desc.Texture, AssetType::TEXTURE2D);
                if(desc.Texture)
                {
                    PropertyLabel("");
                    setNativeSize = ImGui::Button("Set Native Size", ImVec2(-FLT_MIN, 0.0f));
                }
            }

            if(desc.Type == UI::WidgetType::IMAGE)
                changed |= PropertyColor("Tint", desc.Color);

            if(hasStates)
            {
                changed |= PropertyColor("Normal", desc.NormalColor);
                changed |= PropertyColor("Hover", desc.HoverColor);
                changed |= PropertyColor("Pressed", desc.PressedColor);
            }
            ImGui::EndTable();
        }

        if(setNativeSize)
        {
            ImageHandle image;
            mResourceCache.GetTexture(desc.Texture, image);
            if(!image.IsNull())
            {
                const ImageDesc& imageDesc = Core::GetRenderer()->GetRHI()->GetDesc(image);
                desc.Size = { static_cast<float>(imageDesc.Width), static_cast<float>(imageDesc.Height) };
                changed = true;
            }
        }

        if((desc.Type == UI::WidgetType::TEXT || desc.Type == UI::WidgetType::BUTTON) && !desc.Font)
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "No font assigned, the text is invisible. Import a .ttf and pick it above.");

        if(changed)
            MarkChanged();
    }

    // -----------------------------------------------------------------------------------------------
    // Shortcuts
    // -----------------------------------------------------------------------------------------------

    void UIEditorPanel::HandleShortcuts(bool focused)
    {
        const ImGuiIO& io = ImGui::GetIO();
        if(!focused || io.WantTextInput || mRenaming)
            return;

        if(io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
            Save();
        if(io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z))
            Undo();
        if(io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Y) || (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z))))
            Redo();

        const bool hasEditableSelection = mSelected && mSelected != mRoot.ID && mRoot.Find(mSelected);
        if(!hasEditableSelection)
            return;

        if(io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false))
            QueueAction({ .Op = PendingAction::Type::DUPLICATE_WIDGET, .Target = mSelected });
        if(ImGui::IsKeyPressed(ImGuiKey_Delete, false))
            QueueAction({ .Op = PendingAction::Type::REMOVE_WIDGET, .Target = mSelected });

        // Nudge
        glm::vec2 nudge = { 0.0f, 0.0f };
        if(ImGui::IsKeyPressed(ImGuiKey_LeftArrow))  nudge.x -= 1.0f;
        if(ImGui::IsKeyPressed(ImGuiKey_RightArrow)) nudge.x += 1.0f;
        if(ImGui::IsKeyPressed(ImGuiKey_UpArrow))    nudge.y -= 1.0f;
        if(ImGui::IsKeyPressed(ImGuiKey_DownArrow))  nudge.y += 1.0f;
        if(nudge.x != 0.0f || nudge.y != 0.0f)
        {
            mRoot.Find(mSelected)->Offset += nudge * (io.KeyShift ? 10.0f : 1.0f);
            MarkChanged();
        }

        ApplyPendingActions();
    }

    // -----------------------------------------------------------------------------------------------
    // Viewport
    // -----------------------------------------------------------------------------------------------

    bool UIEditorPanel::OnViewportGUI(const ImVec2& viewportMin, const ImVec2& viewportSize)
    {
        mViewportHovered = false;
        Editor* editor = static_cast<Editor*>(Core::GetClient());
        if(!IsViewportEditingEnabled() || editor->IsPlaying() || viewportSize.x < 1.0f || viewportSize.y < 1.0f)
            return false;

        if(mNeedsRebuild)
            RebuildPreview();
        UpdatePreviewLayout();
        if(!mPreviewRoot)
            return false;

        const ImGuiIO& io = ImGui::GetIO();
        const glm::vec2 target = GetTargetResolution();
        const float scale = mScaler.ComputeScale(target);
        const glm::vec2 toScreen = { viewportSize.x / target.x, viewportSize.y / target.y };
        auto ToScreen = [&](const glm::vec2& p) { return ImVec2(viewportMin.x + p.x * toScreen.x, viewportMin.y + p.y * toScreen.y); };

        // Input capture over the image, later items (the aspect ratio combo) still win the hover
        const ImVec2 cursorBackup = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(viewportMin);
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("##UIEditorViewport", viewportSize, ImGuiButtonFlags_MouseButtonLeft);
        const bool hovered = ImGui::IsItemHovered();
        const bool activated = ImGui::IsItemActivated();
        const bool active = ImGui::IsItemActive();
        ImGui::SetCursorScreenPos(cursorBackup);
        mViewportHovered = hovered;

        // Everything drawable, in draw order (later = on top)
        struct Item { UUID ID; glm::vec2 Min, Max; };
        Vector<Item> items;
        auto collect = [&](auto&& self, const UI::WidgetDesc& desc, bool visible) -> void {
            for(const UI::WidgetDesc& child : desc.Children)
            {
                const bool childVisible = visible && child.Visible;
                glm::vec2 min, max;
                if(childVisible && GetWidgetRect(child.ID, min, max))
                    items.push_back({ child.ID, min, max });
                self(self, child, childVisible);
            }
        };
        collect(collect, mRoot, mRoot.Visible);

        const glm::vec2 mouse = { (io.MousePos.x - viewportMin.x) / toScreen.x, (io.MousePos.y - viewportMin.y) / toScreen.y }; // Target pixels

        UUID hoveredWidget = UUID::INVALID;
        if(hovered)
        {
            for(auto it = items.rbegin(); it != items.rend(); ++it)
            {
                if(mouse.x >= it->Min.x && mouse.x <= it->Max.x && mouse.y >= it->Min.y && mouse.y <= it->Max.y)
                {
                    hoveredWidget = it->ID;
                    break;
                }
            }
        }

        // Selection handles (screen space)
        glm::vec2 selMin, selMax;
        const bool hasSelection = mSelected != mRoot.ID && GetWidgetRect(mSelected, selMin, selMax);
        struct Handle { int Bits; ImVec2 Pos; };
        Handle handles[8];
        int handleUnderMouse = 0;
        if(hasSelection)
        {
            const ImVec2 a = ToScreen(selMin);
            const ImVec2 b = ToScreen(selMax);
            const float midX = (a.x + b.x) * 0.5f;
            const float midY = (a.y + b.y) * 0.5f;
            handles[0] = { HANDLE_LEFT | HANDLE_TOP, a };
            handles[1] = { HANDLE_TOP, { midX, a.y } };
            handles[2] = { HANDLE_RIGHT | HANDLE_TOP, { b.x, a.y } };
            handles[3] = { HANDLE_RIGHT, { b.x, midY } };
            handles[4] = { HANDLE_RIGHT | HANDLE_BOTTOM, b };
            handles[5] = { HANDLE_BOTTOM, { midX, b.y } };
            handles[6] = { HANDLE_LEFT | HANDLE_BOTTOM, { a.x, b.y } };
            handles[7] = { HANDLE_LEFT, { a.x, midY } };

            if(hovered || active)
            {
                for(const Handle& handle : handles)
                {
                    if(std::abs(io.MousePos.x - handle.Pos.x) <= HANDLE_HALF_SIZE + 2.0f && std::abs(io.MousePos.y - handle.Pos.y) <= HANDLE_HALF_SIZE + 2.0f)
                    {
                        handleUnderMouse = handle.Bits;
                        break;
                    }
                }
            }
        }

        // Begin interaction (Alt + LMB belongs to the editor camera)
        if(activated && !io.KeyAlt)
        {
            if(handleUnderMouse)
            {
                mDragMode = DragMode::RESIZE;
                mDragHandles = handleUnderMouse;
            }
            else if(hoveredWidget)
            {
                mSelected = hoveredWidget;
                mDragMode = DragMode::MOVE;
            }
            else
            {
                mSelected = UUID::INVALID;
                mDragMode = DragMode::NONE;
            }

            glm::vec2 rectMin, rectMax;
            if(mDragMode != DragMode::NONE && GetWidgetRect(mSelected, rectMin, rectMax) && GetParentRectUI(mSelected, mDragParentMin, mDragParentSize))
            {
                mDragStartMin = rectMin / scale;
                mDragStartMax = rectMax / scale;
                mDragStartMouse = io.MousePos;
            }
            else
                mDragMode = DragMode::NONE;
        }

        // Drag
        if(mDragMode != DragMode::NONE)
        {
            UI::WidgetDesc* desc = mRoot.Find(mSelected);
            if(!active || !desc)
                mDragMode = DragMode::NONE;
            else
            {
                const glm::vec2 delta = { (io.MousePos.x - mDragStartMouse.x) / toScreen.x / scale, (io.MousePos.y - mDragStartMouse.y) / toScreen.y / scale };
                if(delta.x != 0.0f || delta.y != 0.0f)
                {
                    glm::vec2 newMin = mDragStartMin;
                    glm::vec2 newMax = mDragStartMax;
                    if(mDragMode == DragMode::MOVE)
                    {
                        newMin += delta;
                        newMax += delta;
                    }
                    else
                    {
                        constexpr float minSize = 1.0f;
                        if(mDragHandles & HANDLE_LEFT)   newMin.x = std::min(newMin.x + delta.x, newMax.x - minSize);
                        if(mDragHandles & HANDLE_RIGHT)  newMax.x = std::max(newMax.x + delta.x, newMin.x + minSize);
                        if(mDragHandles & HANDLE_TOP)    newMin.y = std::min(newMin.y + delta.y, newMax.y - minSize);
                        if(mDragHandles & HANDLE_BOTTOM) newMax.y = std::max(newMax.y + delta.y, newMin.y + minSize);
                    }

                    const glm::vec2 oldOffset = desc->Offset;
                    const glm::vec2 oldSize = desc->Size;
                    SetRectFromBounds(*desc, newMin, newMax, mDragParentMin, mDragParentSize);

                    if(mSnapToGrid != io.KeyCtrl) // Ctrl flips the snap toggle
                    {
                        desc->Offset = { SnapValue(desc->Offset.x, mGridSize), SnapValue(desc->Offset.y, mGridSize) };
                        if(mDragMode == DragMode::RESIZE)
                            desc->Size = { SnapValue(desc->Size.x, mGridSize), SnapValue(desc->Size.y, mGridSize) };
                    }

                    if(desc->Offset != oldOffset || desc->Size != oldSize)
                        MarkChanged();
                }
            }
        }

        // Draw
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->PushClipRect(viewportMin, { viewportMin.x + viewportSize.x, viewportMin.y + viewportSize.y }, true);

        if(mShowAllOutlines)
        {
            for(const Item& item : items)
                drawList->AddRect(ToScreen(item.Min), ToScreen(item.Max), COLOR_OUTLINE);
        }

        if(hoveredWidget && hoveredWidget != mSelected && mDragMode == DragMode::NONE)
        {
            glm::vec2 min, max;
            if(GetWidgetRect(hoveredWidget, min, max))
                drawList->AddRect(ToScreen(min), ToScreen(max), COLOR_HOVER, 0.0f, 0, 1.5f);
        }

        if(hasSelection)
        {
            const UI::WidgetDesc* desc = mRoot.Find(mSelected);
            glm::vec2 parentMinUI, parentSizeUI;
            if(desc && GetParentRectUI(mSelected, parentMinUI, parentSizeUI))
            {
                // Parent rect + anchors (the 4 "petals" like Unity)
                const glm::vec2 parentMin = parentMinUI * scale;
                const glm::vec2 parentMax = (parentMinUI + parentSizeUI) * scale;
                drawList->AddRect(ToScreen(parentMin), ToScreen(parentMax), COLOR_PARENT, 0.0f, 0, 1.0f);

                for(int i = 0; i < 4; i++)
                {
                    const glm::vec2 anchor = { (i & 1) ? desc->AnchorMax.x : desc->AnchorMin.x, (i & 2) ? desc->AnchorMax.y : desc->AnchorMin.y };
                    const ImVec2 p = ToScreen(parentMin + (parentMax - parentMin) * anchor);
                    const float dx = (i & 1) ? 6.0f : -6.0f;
                    const float dy = (i & 2) ? 6.0f : -6.0f;
                    drawList->AddTriangleFilled(p, { p.x + dx, p.y + dy * 0.4f }, { p.x + dx * 0.4f, p.y + dy }, IM_COL32(240, 240, 240, 230));
                }
            }

            drawList->AddRect(ToScreen(selMin), ToScreen(selMax), COLOR_SELECTED, 0.0f, 0, 2.0f);

            if(desc)
            {
                const ImVec2 pivot = ToScreen(selMin + (selMax - selMin) * desc->Pivot);
                drawList->AddCircle(pivot, 5.0f, IM_COL32(110, 185, 255, 255), 16, 2.0f);
            }

            for(const Handle& handle : handles)
            {
                const ImVec2 hMin = { handle.Pos.x - HANDLE_HALF_SIZE, handle.Pos.y - HANDLE_HALF_SIZE };
                const ImVec2 hMax = { handle.Pos.x + HANDLE_HALF_SIZE, handle.Pos.y + HANDLE_HALF_SIZE };
                const bool highlighted = handle.Bits == handleUnderMouse || (mDragMode == DragMode::RESIZE && handle.Bits == mDragHandles);
                drawList->AddRectFilled(hMin, hMax, highlighted ? COLOR_SELECTED : IM_COL32(255, 255, 255, 255));
                drawList->AddRect(hMin, hMax, IM_COL32(20, 20, 20, 255));
            }

            // Size readout (UI units)
            const glm::vec2 sizeUI = (selMax - selMin) / scale;
            const String sizeText = std::format("{:.0f} x {:.0f}", sizeUI.x, sizeUI.y);
            const ImVec2 textPos = { ToScreen(selMin).x, ToScreen(selMax).y + 6.0f };
            const ImVec2 textSize = ImGui::CalcTextSize(sizeText.c_str());
            drawList->AddRectFilled({ textPos.x - 3.0f, textPos.y - 1.0f }, { textPos.x + textSize.x + 3.0f, textPos.y + textSize.y + 1.0f }, IM_COL32(15, 15, 15, 200), 3.0f);
            drawList->AddText(textPos, IM_COL32(230, 230, 230, 255), sizeText.c_str());
        }

        drawList->PopClipRect();

        // Cursor feedback
        const int cursorBits = mDragMode == DragMode::RESIZE ? mDragHandles : handleUnderMouse;
        if(cursorBits)
        {
            const bool horizontal = cursorBits & (HANDLE_LEFT | HANDLE_RIGHT);
            const bool vertical = cursorBits & (HANDLE_TOP | HANDLE_BOTTOM);
            if(horizontal && vertical)
                ImGui::SetMouseCursor(((cursorBits & HANDLE_LEFT) != 0) == ((cursorBits & HANDLE_TOP) != 0) ? ImGuiMouseCursor_ResizeNWSE : ImGuiMouseCursor_ResizeNESW);
            else
                ImGui::SetMouseCursor(horizontal ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
        }
        else if(mDragMode == DragMode::MOVE || (hoveredWidget && hoveredWidget == mSelected))
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);

        return true;
    }
} // namespace Surge
