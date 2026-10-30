// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/UIEditorPanel.hpp"
#include "Asset/AssetDatabase.hpp"
#include "Asset/AssetExtensions.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/HighLevel/Texture2D.hpp"
#include "Surge/Utility/Filesystem.hpp"

#include "Editor.hpp"
#include "Panels/ContentBrowserPanel.hpp"
#include "Utility/EditorUI.hpp"

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

        namespace Colors = EditorUI::Colors;
        using EditorUI::ToU32;
        using EditorUI::WithAlpha;
        constexpr ImU32 COLOR_OUTLINE = ToU32(WithAlpha(Colors::White, 0.18f));
        constexpr ImU32 COLOR_HOVER = ToU32(WithAlpha(Colors::Blue, 0.86f));
        constexpr ImU32 COLOR_SELECTED = ToU32(Colors::Accent);
        constexpr ImU32 COLOR_PARENT = ToU32(WithAlpha(Colors::Accent, 0.27f));

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

        // Combo of every registered asset of the type, also accepts drops from the Content Browser
        bool PropertyAsset(const char* label, AssetID& id, AssetType type, const char* noneLabel = "None")
        {
            AssetManager* am = Core::GetAssetManager();
            bool changed = false;

            ImGui::PushID(label);
            EditorUI::TLabel(label);

            String preview = noneLabel;
            if(id)
            {
                const AssetMetadata& meta = am->GetMetadata(id);
                preview = meta.Type == type ? Filesystem::GetFilenameWithExt(meta.RelativePath) : String("Missing!");
            }

            if(ImGui::BeginCombo("##v", preview.c_str(), ImGuiComboFlags_HeightLarge))
            {
                if(ImGui::Selectable(noneLabel, !id))
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

        // One drawn part of a widget (texture, color, 9-slice, rounded corners, border), as rows of the current property table
        bool PropertyImagePart(const char* label, UI::ImagePartDesc& part, bool showColor, bool allowFilled, bool* outSetNativeSize)
        {
            constexpr std::array<const char*, 3> modeNames = { "Simple", "Sliced (9-slice)", "Filled" };
            constexpr std::array<UI::ImageMode, 3> modeValues = { UI::ImageMode::SIMPLE, UI::ImageMode::SLICED, UI::ImageMode::FILLED };
            constexpr std::array<const char*, 2> partModeNames = { "Simple", "Sliced (9-slice)" };
            constexpr std::array<UI::ImageMode, 2> partModeValues = { UI::ImageMode::SIMPLE, UI::ImageMode::SLICED };

            ImGui::PushID(label);
            ImGui::TableNextColumn();
            ImGui::TextColored(Colors::Accent, "%s", label);
            ImGui::TableNextColumn();

            bool changed = PropertyAsset("Texture", part.Texture, AssetType::TEXTURE2D);
            if(part.Texture && outSetNativeSize)
            {
                EditorUI::TLabel("");
                if(ImGui::Button("Set Native Size", ImVec2(-FLT_MIN, 0.0f)))
                    *outSetNativeSize = true;
            }
            if(showColor)
                changed |= EditorUI::TColorEdit4("Color", glm::value_ptr(part.Color));

            changed |= allowFilled ? EditorUI::TComboValues("Mode", part.Mode, modeNames, modeValues) : EditorUI::TComboValues("Mode", part.Mode, partModeNames, partModeValues);
            if(part.Mode == UI::ImageMode::SLICED)
            {
                changed |= EditorUI::TDragVec("Slice Border", part.SliceBorder, 0.5f, 0.0f, 4096.0f, "%.0f");
                EditorUI::DelayedToolTip("Left, top, right, bottom in texture pixels: corners keep their size, edges and center stretch");
                changed |= EditorUI::TDragFloat("Slice Scale", &part.SliceScale, 0.01f, 0.0f, 100.0f, "%.2f");
                changed |= EditorUI::TCheckbox("Fill Center", &part.FillCenter);
            }
            else
            {
                changed |= EditorUI::TDragFloat("Corner Radius", &part.CornerRadius, 0.25f, 0.0f, 10000.0f, "%.1f");
                EditorUI::DelayedToolTip("UI units. Half the height (or more) makes a pill / circle");
            }
            changed |= EditorUI::TDragFloat("Border Width", &part.BorderWidth, 0.1f, 0.0f, 500.0f, "%.1f");
            if(part.BorderWidth > 0.0f)
                changed |= EditorUI::TColorEdit4("Border Color", glm::value_ptr(part.BorderColor));

            ImGui::PopID();
            return changed;
        }
    } // namespace

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
        Ref<UILayout> layout = AssetDatabase::Get().Create<UILayout>(relativePath);
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
            EditorUI::ShowConfirmationBox("Unsaved UI Layout", "The open UI Layout has unsaved changes.\nDiscard them and open the other layout?", open);
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
            EditorUI::ShowConfirmationBox("Unsaved UI Layout", "Discard unsaved changes?", close);
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
        AssetDatabase::Get().Save(mLayout->GetID());
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
        mPreviewRoot = UI::Instantiate(mRoot, mResourceCache, &mPreviewWidgets, true); // Design preview: modals in place, placeholder list items
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

        auto [pos, size] = it->second->GetGlobalBounds();
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
            EditorUI::ScopedBoldFont bold(20.0f);
            EditorUI::TextCentered("No UI Layout open");
        }
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::TextWrapped("Double click a .sui file in the Content Browser, drop one on the button below, or create a new layout. "
                           "Add a UI Canvas Component to an entity and assign the layout to show it in game.");
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        EditorUI::ScopedBoldFont bold;
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
            EditorUI::ScopedBoldFont bold(18.0f);
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(Colors::Accent, "%s%s", fileName.c_str(), mUnsaved ? " *" : "");
        }

        ImGui::SameLine();
        if(ImGui::Button("Save"))
            Save();
        EditorUI::DelayedToolTip("Ctrl+S");

        ImGui::SameLine();
        ImGui::BeginDisabled(mUndoStack.empty() && !mDocumentChanged);
        if(ImGui::Button("Undo"))
            Undo();
        ImGui::EndDisabled();
        EditorUI::DelayedToolTip("Ctrl+Z");

        ImGui::SameLine();
        ImGui::BeginDisabled(mRedoStack.empty());
        if(ImGui::Button("Redo"))
            Redo();
        ImGui::EndDisabled();
        EditorUI::DelayedToolTip("Ctrl+Y / Ctrl+Shift+Z");

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
                if(UI::IsContainerType(selected->Type))
                    parent = selected->ID;
                else if(const UI::WidgetDesc* selectedParent = mRoot.FindParentOf(mSelected))
                    parent = selectedParent->ID;
            }

            DrawAddWidgetMenuItems(parent, false);
            ImGui::EndPopup();
        }

        const bool canEditSelection = mSelected && mSelected != mRoot.ID && mRoot.Find(mSelected);
        ImGui::SameLine();
        ImGui::BeginDisabled(!canEditSelection);
        if(ImGui::Button("Duplicate"))
            QueueAction({ .Op = PendingAction::Type::DUPLICATE_WIDGET, .Target = mSelected });
        EditorUI::DelayedToolTip("Ctrl+D");
        ImGui::SameLine();
        if(ImGui::Button("Delete"))
            QueueAction({ .Op = PendingAction::Type::REMOVE_WIDGET, .Target = mSelected });
        EditorUI::DelayedToolTip("Delete");
        ImGui::SameLine();
        if(ImGui::ArrowButton("##MoveUp", ImGuiDir_Up))
            QueueAction({ .Op = PendingAction::Type::MOVE_WIDGET, .Target = mSelected, .Direction = -1 });
        EditorUI::DelayedToolTip("Move up (drawn earlier, behind its siblings)");
        ImGui::SameLine();
        if(ImGui::ArrowButton("##MoveDown", ImGuiDir_Down))
            QueueAction({ .Op = PendingAction::Type::MOVE_WIDGET, .Target = mSelected, .Direction = 1 });
        EditorUI::DelayedToolTip("Move down (drawn later, on top of its siblings)");
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        if(ImGui::Checkbox("Edit in Viewport", &mEditInViewport))
            mNeedsRebuild = true;
        EditorUI::DelayedToolTip("Show the layout in the Viewport and select/move/resize widgets there");

        ImGui::SameLine();
        ImGui::Checkbox("Outlines", &mShowAllOutlines);
        ImGui::SameLine();
        ImGui::Checkbox("Snap", &mSnapToGrid);
        EditorUI::DelayedToolTip("Snap offsets and sizes to the grid while dragging (hold Ctrl to toggle temporarily)");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60.0f);
        ImGui::DragFloat("##Grid", &mGridSize, 0.5f, 1.0f, 200.0f, "%.0f");

        if(editor->IsPlaying())
            ImGui::TextColored(Colors::Error, "Playing: the game uses the last saved version of this layout");
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
            DrawAddWidgetMenuItems(mRoot.ID, true);
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
                    DrawAddWidgetMenuItems(desc.ID, false);
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
            EditorUI::ScopedBoldFont bold(18.0f);
            ImGui::TextColored(Colors::Accent, "%s", UI::WidgetTypeToDisplayName(desc->Type));
        }

        if(EditorUI::BeginPropertyTable("##General", PROPERTY_LABEL_WIDTH, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX))
        {
            if(EditorUI::TInputText("Name", desc->Name, false))
                MarkChanged();
            if(EditorUI::TCheckbox("Visible", &desc->Visible))
                MarkChanged();
            ImGui::EndTable();
        }

        if(desc->Name.empty())
            ImGui::TextColored(Colors::Warning, "Give the widget a name to find it from Lua");
        else if(CountName(mRoot, desc->Name) > 1)
            ImGui::TextColored(Colors::Warning, "Name is not unique, FindWidget returns the first match");

        EditorUI::SeparatorTitle("Rect Transform", 4.0f);
        DrawAnchorPresets(*desc);
        DrawRectTransform(*desc);

        EditorUI::SeparatorTitle(UI::WidgetTypeToDisplayName(desc->Type), 4.0f);
        DrawWidgetSettings(*desc);

        // Scripting hint
        if(!desc->Name.empty())
        {
            EditorUI::SeparatorTitle("Scripting", 4.0f);
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
                case UI::WidgetType::TOGGLE:
                    snippet = std::format("local widget = entity.UICanvasC:FindWidget(\"{}\")\nwidget:OnValueChanged(function(self) Log(tostring(self.IsOn)) end)", desc->Name);
                    break;
                case UI::WidgetType::SLIDER:
                    snippet = std::format("local widget = entity.UICanvasC:FindWidget(\"{}\")\nwidget:OnValueChanged(function(self) Log(tostring(self.Value)) end)", desc->Name);
                    break;
                case UI::WidgetType::PROGRESS_BAR:
                    snippet = std::format("local widget = entity.UICanvasC:FindWidget(\"{}\")\nwidget.Value = 0.75", desc->Name);
                    break;
                case UI::WidgetType::JOYSTICK:
                    snippet = std::format("local widget = entity.UICanvasC:FindWidget(\"{}\")\n-- In OnUpdate: local dir = widget.Direction (Vec2, +Y up)", desc->Name);
                    break;
                case UI::WidgetType::INPUT_FIELD:
                    snippet = std::format("local widget = entity.UICanvasC:FindWidget(\"{}\")\nwidget:OnSubmit(function(self) Log(self.Text) end)", desc->Name);
                    break;
                case UI::WidgetType::DROPDOWN:
                    snippet = std::format("local widget = entity.UICanvasC:FindWidget(\"{}\")\nwidget:OnValueChanged(function(self) Log(self.SelectedText) end)", desc->Name);
                    break;
                case UI::WidgetType::MODAL:
                    snippet = std::format("local widget = entity.UICanvasC:FindWidget(\"{}\")\nwidget:Show()", desc->Name);
                    break;
                case UI::WidgetType::TAB_VIEW:
                    snippet = std::format("local widget = entity.UICanvasC:FindWidget(\"{}\")\nwidget:OnValueChanged(function(self) Log(tostring(self.ActiveTab)) end)", desc->Name);
                    break;
                case UI::WidgetType::LIST_VIEW:
                    snippet = std::format("local widget = entity.UICanvasC:FindWidget(\"{}\")\nwidget:OnBindItem(function(item, index) item:GetText().Text = \"Item \" .. index end)\nwidget.ItemCount = 100", desc->Name);
                    break;
                case UI::WidgetType::SCROLL_VIEW:
                    snippet = std::format("local widget = entity.UICanvasC:FindWidget(\"{}\")\nwidget.ScrollPosition = Math.Vec2.new(0, 0) -- top", desc->Name);
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
        EditorUI::SeparatorTitle("Canvas Scaler", 4.0f);
        ImGui::TextWrapped("The layout is designed at the reference resolution and scaled to the screen. "
                           "Match Height (1) keeps sizes relative to the screen height (landscape), Match Width (0) to the width (portrait).");

        if(EditorUI::BeginPropertyTable("##Canvas", PROPERTY_LABEL_WIDTH, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX))
        {
            EditorUI::TLabel("Preset");
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

            if(EditorUI::TDragVec("Reference Res", mScaler.ReferenceResolution, 1.0f, 1.0f, 8192.0f, "%.0f"))
                MarkChanged();

            ImGui::PushID("Match");
            EditorUI::TLabel("Match W/H");
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
                drawList->AddRectFilled(min, max, isCurrent ? ToU32(WithAlpha(Colors::Accent, 0.35f)) : (hovered ? ToU32(Colors::Tone30) : ToU32(Colors::Tone18)), 2.0f);
                const ImVec2 innerMin = { min.x + 5.0f, min.y + 5.0f };
                const ImVec2 innerMax = { max.x - 5.0f, max.y - 5.0f };
                drawList->AddRect(innerMin, innerMax, ToU32(Colors::Tone60));

                const ImU32 markerColor = isCurrent ? ToU32(Colors::Highlight) : ToU32(Colors::Tone90);
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
        if(!EditorUI::BeginPropertyTable("##RectTransform", PROPERTY_LABEL_WIDTH, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX))
            return;

        bool changed = false;
        changed |= EditorUI::TDragVec("Anchor Min", desc.AnchorMin, 0.005f, 0.0f, 1.0f, "%.3f");
        changed |= EditorUI::TDragVec("Anchor Max", desc.AnchorMax, 0.005f, 0.0f, 1.0f, "%.3f");
        changed |= EditorUI::TDragVec("Pivot", desc.Pivot, 0.005f, 0.0f, 1.0f, "%.3f");
        changed |= EditorUI::TDragVec("Offset", desc.Offset, 1.0f, 0.0f, 0.0f, "%.1f");
        changed |= EditorUI::TDragVec(IsStretched(desc) ? "Size (+Stretch)" : "Size", desc.Size, 1.0f, 0.0f, 0.0f, "%.1f");
        ImGui::EndTable();

        if(changed)
            MarkChanged();

        if(IsStretched(desc))
            ImGui::TextDisabled("Stretched axes: Size is added to the parent size (negative = inset)");
    }

    void UIEditorPanel::DrawAddWidgetMenuItems(UUID parent, bool addPrefix)
    {
        constexpr UI::WidgetType basicTypes[] = { UI::WidgetType::BASE_WIDGET, UI::WidgetType::IMAGE, UI::WidgetType::TEXT, UI::WidgetType::BUTTON, UI::WidgetType::IMAGE_BUTTON };
        constexpr UI::WidgetType controlTypes[] = { UI::WidgetType::TOGGLE, UI::WidgetType::SLIDER, UI::WidgetType::PROGRESS_BAR, UI::WidgetType::INPUT_FIELD,
                                                    UI::WidgetType::DROPDOWN, UI::WidgetType::JOYSTICK, UI::WidgetType::SPINNER };
        constexpr UI::WidgetType containerTypes[] = { UI::WidgetType::SCROLL_VIEW, UI::WidgetType::LIST_VIEW, UI::WidgetType::LAYOUT_GROUP, UI::WidgetType::TAB_VIEW,
                                                      UI::WidgetType::MODAL, UI::WidgetType::SAFE_AREA };

        auto item = [&](UI::WidgetType type) {
            const String label = addPrefix ? std::format("Add {}", UI::WidgetTypeToDisplayName(type)) : String(UI::WidgetTypeToDisplayName(type));
            if(ImGui::MenuItem(label.c_str()))
                QueueAction({ .Op = PendingAction::Type::ADD_WIDGET, .NewParent = parent, .NewWidgetType = type });
        };

        for(UI::WidgetType type : basicTypes)
            item(type);
        if(ImGui::BeginMenu("Controls"))
        {
            for(UI::WidgetType type : controlTypes)
                item(type);
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("Containers"))
        {
            for(UI::WidgetType type : containerTypes)
                item(type);
            ImGui::EndMenu();
        }
    }

    void UIEditorPanel::DrawWidgetSettings(UI::WidgetDesc& desc)
    {
        constexpr std::array<const char*, 3> hAlignNames = { "Left", "Center", "Right" };
        constexpr std::array<TextAlignment, 3> hAlignValues = { TextAlignment::LEFT, TextAlignment::CENTER, TextAlignment::RIGHT };
        constexpr std::array<const char*, 3> vAlignNames = { "Top", "Center", "Bottom" };
        constexpr std::array<TextVerticalAlignment, 3> vAlignValues = { TextVerticalAlignment::TOP, TextVerticalAlignment::CENTER, TextVerticalAlignment::BOTTOM };
        constexpr std::array<const char*, 3> overflowNames = { "Wrap", "Truncate (...)", "No Wrap" };
        constexpr std::array<UI::TextOverflow, 3> overflowValues = { UI::TextOverflow::WRAP, UI::TextOverflow::TRUNCATE, UI::TextOverflow::NO_WRAP };
        constexpr std::array<const char*, 4> originNames = { "Left", "Right", "Top", "Bottom" };
        constexpr std::array<UI::FillOrigin, 4> originValues = { UI::FillOrigin::LEFT, UI::FillOrigin::RIGHT, UI::FillOrigin::TOP, UI::FillOrigin::BOTTOM };
        constexpr std::array<const char*, 2> toggleNames = { "Checkbox", "Switch" };
        constexpr std::array<UI::ToggleStyle, 2> toggleValues = { UI::ToggleStyle::CHECKBOX, UI::ToggleStyle::SWITCH };
        constexpr std::array<const char*, 2> stickNames = { "Fixed", "Floating" };
        constexpr std::array<UI::JoystickMode, 2> stickValues = { UI::JoystickMode::FIXED, UI::JoystickMode::FLOATING };
        constexpr std::array<const char*, 3> directionNames = { "Horizontal", "Vertical", "Grid" };
        constexpr std::array<UI::LayoutDirection, 3> directionValues = { UI::LayoutDirection::HORIZONTAL, UI::LayoutDirection::VERTICAL, UI::LayoutDirection::GRID };
        constexpr std::array<const char*, 6> contentNames = { "Standard", "Integer", "Decimal", "Email", "Phone", "Password" };
        constexpr std::array<UI::InputContentType, 6> contentValues = { UI::InputContentType::STANDARD, UI::InputContentType::INTEGER, UI::InputContentType::DECIMAL,
                                                                       UI::InputContentType::EMAIL, UI::InputContentType::PHONE, UI::InputContentType::PASSWORD };

        if(desc.Type == UI::WidgetType::BASE_WIDGET)
        {
            ImGui::TextDisabled("Empty container, draws nothing. Use it to group and anchor widgets.");
            return;
        }

        bool changed = false;
        bool setNativeSize = false;

        // Shared groups
        auto textProperties = [&](const char* textLabel, bool multiline) {
            if(textLabel)
                changed |= EditorUI::TInputText(textLabel, desc.Text, multiline);
            changed |= PropertyAsset("Font", desc.Font, AssetType::FONT, "Engine Font");
            changed |= EditorUI::TDragFloat("Font Size", &desc.FontSize, 0.25f, 1.0f, 512.0f, "%.1f");
        };
        auto valueProperties = [&]() {
            changed |= EditorUI::TDragFloat("Min", &desc.MinValue, 0.05f, 0.0f, 0.0f, "%.2f");
            changed |= EditorUI::TDragFloat("Max", &desc.MaxValue, 0.05f, 0.0f, 0.0f, "%.2f");
            changed |= EditorUI::TSliderFloat("Value", &desc.Value, glm::min(desc.MinValue, desc.MaxValue), glm::max(desc.MinValue, desc.MaxValue));
            changed |= EditorUI::TComboValues("Direction", desc.Origin, originNames, originValues);
        };
        auto stateColors = [&]() {
            changed |= EditorUI::TColorEdit4("Normal", glm::value_ptr(desc.NormalColor));
            changed |= EditorUI::TColorEdit4("Hover", glm::value_ptr(desc.HoverColor));
            changed |= EditorUI::TColorEdit4("Pressed", glm::value_ptr(desc.PressedColor));
        };

        if(EditorUI::BeginPropertyTable("##WidgetSettings", PROPERTY_LABEL_WIDTH, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX))
        {
            switch(desc.Type)
            {
                case UI::WidgetType::IMAGE:
                    changed |= EditorUI::TColorEdit4("Tint", glm::value_ptr(desc.Color));
                    changed |= PropertyImagePart("Image", desc.Background, false, true, &setNativeSize);
                    changed |= EditorUI::TCheckbox("Preserve Aspect", &desc.PreserveAspect);
                    if(desc.Background.Mode == UI::ImageMode::FILLED)
                    {
                        changed |= EditorUI::TSliderFloat("Fill Amount", &desc.FillAmount, 0.0f, 1.0f);
                        changed |= EditorUI::TComboValues("Fill Origin", desc.Origin, originNames, originValues);
                    }
                    break;

                case UI::WidgetType::TEXT:
                    textProperties("Text", true);
                    changed |= EditorUI::TComboValues("Align", desc.HAlign, hAlignNames, hAlignValues);
                    changed |= EditorUI::TComboValues("Vertical Align", desc.VAlign, vAlignNames, vAlignValues);
                    changed |= EditorUI::TColorEdit4("Color", glm::value_ptr(desc.Color));
                    changed |= EditorUI::TComboValues("Overflow", desc.Overflow, overflowNames, overflowValues);
                    break;

                case UI::WidgetType::BUTTON:
                    textProperties("Label", false);
                    changed |= EditorUI::TComboValues("Align", desc.HAlign, hAlignNames, hAlignValues);
                    changed |= EditorUI::TComboValues("Vertical Align", desc.VAlign, vAlignNames, vAlignValues);
                    changed |= EditorUI::TColorEdit4("Label Color", glm::value_ptr(desc.TextColor));
                    changed |= EditorUI::TDragFloat("Label Padding", &desc.TextPadding, 0.25f, 0.0f, 500.0f, "%.1f");
                    stateColors();
                    changed |= PropertyImagePart("Background", desc.Background, false, false, &setNativeSize);
                    break;

                case UI::WidgetType::IMAGE_BUTTON:
                    stateColors();
                    changed |= EditorUI::TCheckbox("Preserve Aspect", &desc.PreserveAspect);
                    changed |= PropertyImagePart("Image", desc.Background, false, false, &setNativeSize);
                    break;

                case UI::WidgetType::PROGRESS_BAR:
                    valueProperties();
                    changed |= EditorUI::TDragFloat("Fill Padding", &desc.FillPadding, 0.25f, 0.0f, 500.0f, "%.1f");
                    changed |= PropertyImagePart("Background", desc.Background, true, false, nullptr);
                    changed |= PropertyImagePart("Fill", desc.Fill, true, false, nullptr);
                    break;

                case UI::WidgetType::TOGGLE:
                    changed |= EditorUI::TComboValues("Style", desc.ToggleType, toggleNames, toggleValues);
                    changed |= EditorUI::TCheckbox("Is On", &desc.IsOn);
                    changed |= EditorUI::TInputText("Group", desc.Group, false);
                    if(!desc.Group.empty())
                        changed |= EditorUI::TCheckbox("Allow Switch Off", &desc.AllowSwitchOff);
                    textProperties("Label", false);
                    changed |= EditorUI::TColorEdit4("Label Color", glm::value_ptr(desc.TextColor));
                    changed |= PropertyImagePart(desc.ToggleType == UI::ToggleStyle::SWITCH ? "Track (Off)" : "Box", desc.Background, true, false, nullptr);
                    changed |= PropertyImagePart(desc.ToggleType == UI::ToggleStyle::SWITCH ? "Track (On)" : "Check Mark", desc.Fill, true, false, nullptr);
                    if(desc.ToggleType == UI::ToggleStyle::SWITCH)
                        changed |= PropertyImagePart("Knob", desc.Handle, true, false, nullptr);
                    break;

                case UI::WidgetType::SLIDER:
                    valueProperties();
                    changed |= EditorUI::TCheckbox("Whole Numbers", &desc.WholeNumbers);
                    changed |= EditorUI::TDragFloat("Handle Size", &desc.HandleSize, 0.25f, 0.0f, 1000.0f, "%.1f");
                    changed |= EditorUI::TDragFloat("Track Thickness", &desc.TrackThickness, 0.25f, 0.0f, 1000.0f, "%.1f");
                    changed |= PropertyImagePart("Track", desc.Background, true, false, nullptr);
                    changed |= PropertyImagePart("Fill", desc.Fill, true, false, nullptr);
                    changed |= PropertyImagePart("Handle", desc.Handle, true, false, nullptr);
                    break;

                case UI::WidgetType::JOYSTICK:
                    changed |= EditorUI::TComboValues("Mode", desc.StickMode, stickNames, stickValues);
                    changed |= EditorUI::TDragFloat("Base Size", &desc.BaseSize, 0.5f, 0.0f, 4000.0f, "%.1f");
                    changed |= EditorUI::TDragFloat("Handle Size", &desc.HandleSize, 0.5f, 0.0f, 4000.0f, "%.1f");
                    changed |= EditorUI::TSliderFloat("Dead Zone", &desc.DeadZone, 0.0f, 0.95f);
                    changed |= EditorUI::TDragFloat("Handle Range", &desc.HandleRange, 0.01f, 0.0f, 4.0f, "%.2f");
                    changed |= EditorUI::TCheckbox("Hide When Idle", &desc.HideWhenIdle);
                    changed |= PropertyImagePart("Base", desc.Background, true, false, nullptr);
                    changed |= PropertyImagePart("Knob", desc.Handle, true, false, nullptr);
                    break;

                case UI::WidgetType::SCROLL_VIEW:
                case UI::WidgetType::LIST_VIEW:
                    changed |= EditorUI::TCheckbox("Scroll Vertical", &desc.ScrollVertical);
                    changed |= EditorUI::TCheckbox("Scroll Horizontal", &desc.ScrollHorizontal);
                    changed |= EditorUI::TSliderFloat("Deceleration", &desc.DecelerationRate, 0.0f, 0.99f);
                    changed |= EditorUI::TCheckbox("Show Scrollbars", &desc.ShowScrollbars);
                    if(desc.ShowScrollbars)
                        changed |= EditorUI::TColorEdit4("Scrollbar Color", glm::value_ptr(desc.ScrollbarColor));
                    if(desc.Type == UI::WidgetType::LIST_VIEW)
                    {
                        changed |= EditorUI::TDragVec("Item Size", desc.ItemSize, 0.5f, 0.0f, 8192.0f, "%.1f");
                        changed |= EditorUI::TDragVec("Spacing", desc.Spacing, 0.25f, 0.0f, 0.0f, "%.1f");
                        changed |= EditorUI::TDragVec("Padding", desc.Padding, 0.25f, 0.0f, 0.0f, "%.1f");
                        changed |= EditorUI::TDragInt("Columns", &desc.Columns, 0.1f, 1, 64);
                        changed |= EditorUI::TDragInt("Preview Items", &desc.PreviewItemCount, 0.1f, 1, 50);
                    }
                    break;

                case UI::WidgetType::LAYOUT_GROUP:
                    changed |= EditorUI::TComboValues("Direction", desc.Direction, directionNames, directionValues);
                    changed |= EditorUI::TDragVec("Padding", desc.Padding, 0.25f, 0.0f, 0.0f, "%.1f");
                    changed |= EditorUI::TDragVec("Spacing", desc.Spacing, 0.25f, 0.0f, 0.0f, "%.1f");
                    if(desc.Direction == UI::LayoutDirection::GRID)
                    {
                        changed |= EditorUI::TDragVec("Cell Size", desc.CellSize, 0.5f, 1.0f, 8192.0f, "%.1f");
                        changed |= EditorUI::TDragInt("Columns", &desc.Columns, 0.1f, 0, 64);
                    }
                    changed |= EditorUI::TDragVec("Child Alignment", desc.ChildAlignment, 0.01f, 0.0f, 1.0f, "%.2f");
                    if(desc.Direction != UI::LayoutDirection::GRID)
                    {
                        changed |= EditorUI::TCheckbox("Control Width", &desc.ControlChildWidth);
                        changed |= EditorUI::TCheckbox("Control Height", &desc.ControlChildHeight);
                    }
                    changed |= EditorUI::TCheckbox("Fit Content", &desc.FitContent);
                    break;

                case UI::WidgetType::INPUT_FIELD:
                    textProperties("Text", desc.MultiLine);
                    changed |= EditorUI::TInputText("Placeholder", desc.Placeholder, false);
                    changed |= EditorUI::TComboValues("Content Type", desc.ContentType, contentNames, contentValues);
                    changed |= EditorUI::TCheckbox("Multi Line", &desc.MultiLine);
                    changed |= EditorUI::TDragInt("Max Length", &desc.MaxLength, 0.1f, 0, 100000);
                    changed |= EditorUI::TDragFloat("Text Padding", &desc.TextPadding, 0.25f, 0.0f, 500.0f, "%.1f");
                    changed |= EditorUI::TColorEdit4("Text Color", glm::value_ptr(desc.TextColor));
                    changed |= EditorUI::TColorEdit4("Placeholder Color", glm::value_ptr(desc.PlaceholderColor));
                    changed |= EditorUI::TColorEdit4("Caret Color", glm::value_ptr(desc.CaretColor));
                    changed |= EditorUI::TColorEdit4("Selection Color", glm::value_ptr(desc.SelectionColor));
                    changed |= EditorUI::TColorEdit4("Focus Border", glm::value_ptr(desc.HighlightColor));
                    changed |= PropertyImagePart("Background", desc.Background, true, false, nullptr);
                    break;

                case UI::WidgetType::DROPDOWN:
                {
                    String options;
                    for(size_t i = 0; i < desc.Options.size(); i++)
                        options += (i > 0 ? "\n" : "") + desc.Options[i];
                    if(EditorUI::TInputText("Options (1 per line)", options, true))
                    {
                        desc.Options.clear();
                        size_t start = 0;
                        while(start <= options.size())
                        {
                            const size_t end = options.find('\n', start);
                            const String line = options.substr(start, end == String::npos ? String::npos : end - start);
                            if(!line.empty())
                                desc.Options.push_back(line);
                            if(end == String::npos)
                                break;
                            start = end + 1;
                        }
                        changed = true;
                    }
                    changed |= EditorUI::TDragInt("Selected (-1 none)", &desc.SelectedIndex, 0.1f, -1, glm::max(static_cast<int>(desc.Options.size()) - 1, -1));
                    changed |= EditorUI::TInputText("Placeholder", desc.Placeholder, false);
                    textProperties(nullptr, false);
                    changed |= EditorUI::TColorEdit4("Text Color", glm::value_ptr(desc.TextColor));
                    changed |= EditorUI::TDragFloat("Text Padding", &desc.TextPadding, 0.25f, 0.0f, 500.0f, "%.1f");
                    changed |= EditorUI::TDragInt("Max Visible Items", &desc.MaxVisibleItems, 0.1f, 1, 50);
                    changed |= EditorUI::TColorEdit4("Item Hover", glm::value_ptr(desc.HoverColor));
                    changed |= EditorUI::TColorEdit4("Selected Item", glm::value_ptr(desc.HighlightColor));
                    changed |= PropertyImagePart("Field", desc.Background, true, false, nullptr);
                    changed |= PropertyImagePart("List", desc.Fill, true, false, nullptr);
                    break;
                }

                case UI::WidgetType::MODAL:
                    changed |= EditorUI::TColorEdit4("Backdrop", glm::value_ptr(desc.BackdropColor));
                    changed |= EditorUI::TCheckbox("Close On Backdrop", &desc.CloseOnBackdrop);
                    changed |= EditorUI::TCheckbox("Close On Back", &desc.CloseOnBack);
                    changed |= PropertyImagePart("Panel", desc.Background, true, false, nullptr);
                    break;

                case UI::WidgetType::TAB_VIEW:
                    changed |= EditorUI::TDragInt("Active Tab", &desc.ActiveTab, 0.1f, 0, glm::max(static_cast<int>(desc.Children.size()) - 1, 0));
                    changed |= EditorUI::TCheckbox("Tabs At Bottom", &desc.TabsAtBottom);
                    changed |= EditorUI::TDragFloat("Tab Bar Height", &desc.TabBarHeight, 0.5f, 0.0f, 2000.0f, "%.1f");
                    textProperties(nullptr, false);
                    changed |= EditorUI::TColorEdit4("Text Color", glm::value_ptr(desc.TextColor));
                    changed |= EditorUI::TColorEdit4("Active Text", glm::value_ptr(desc.HighlightColor));
                    changed |= PropertyImagePart("Bar", desc.Background, true, false, nullptr);
                    changed |= PropertyImagePart("Active Tab", desc.Fill, true, false, nullptr);
                    break;

                case UI::WidgetType::SPINNER:
                    changed |= EditorUI::TColorEdit4("Color", glm::value_ptr(desc.Color));
                    changed |= EditorUI::TDragInt("Dots", &desc.DotCount, 0.1f, 3, 24);
                    changed |= EditorUI::TSliderFloat("Dot Size", &desc.DotSize, 0.02f, 0.5f);
                    changed |= EditorUI::TDragFloat("Speed (rev/s)", &desc.Speed, 0.01f, 0.0f, 10.0f, "%.2f");
                    break;

                case UI::WidgetType::SAFE_AREA:
                {
                    constexpr std::array<const char*, 4> edgeNames = { "Left Edge", "Top Edge", "Right Edge", "Bottom Edge" };
                    for(int i = 0; i < 4; i++)
                    {
                        bool enabled = (desc.SafeEdges & (1 << i)) != 0;
                        if(EditorUI::TCheckbox(edgeNames[i], &enabled))
                        {
                            desc.SafeEdges = enabled ? (desc.SafeEdges | (1 << i)) : (desc.SafeEdges & ~(1 << i));
                            changed = true;
                        }
                    }
                    break;
                }

                case UI::WidgetType::BASE_WIDGET:
                    break;
            }
            ImGui::EndTable();
        }

        // Type notes
        switch(desc.Type)
        {
            case UI::WidgetType::LIST_VIEW:
                ImGui::TextWrapped("The first child is the item template. At runtime only the visible items exist; fill them from Lua with OnBindItem(item, index) and set ItemCount.");
                if(desc.Children.size() > 1)
                    ImGui::TextColored(Colors::Warning, "Only the first child is used, the others are ignored");
                break;
            case UI::WidgetType::TAB_VIEW:
                ImGui::TextWrapped("Each child is a page, its name is the tab label. Set Active Tab to edit another page.");
                break;
            case UI::WidgetType::MODAL:
                ImGui::TextWrapped("Hidden until Show() is called from Lua; drawn above every canvas with a dimmed backdrop. Shown in place here for editing.");
                break;
            case UI::WidgetType::SCROLL_VIEW:
                ImGui::TextWrapped("Children are clipped to the view and scrolled. Put a Layout Group with Fit Content inside to stack items.");
                break;
            case UI::WidgetType::SAFE_AREA:
                ImGui::TextWrapped("Keeps its children clear of the notch and system bars. Preview with the Game view's Safe Area menu.");
                break;
            default:
                break;
        }

        if(setNativeSize)
        {
            ImageHandle image;
            mResourceCache.GetTexture(desc.Background.Texture, image);
            if(!image.IsNull())
            {
                const ImageDesc& imageDesc = Core::GetRenderer()->GetRHI()->GetDesc(image);
                desc.Size = { static_cast<float>(imageDesc.Width), static_cast<float>(imageDesc.Height) };
                changed = true;
            }
        }

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
            for(size_t i = 0; i < desc.Children.size(); i++)
            {
                const UI::WidgetDesc& child = desc.Children[i];
                const bool inactivePage = desc.Type == UI::WidgetType::TAB_VIEW && static_cast<int>(i) != glm::clamp(desc.ActiveTab, 0, static_cast<int>(desc.Children.size()) - 1);
                const bool childVisible = visible && child.Visible && !inactivePage;
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
                    drawList->AddTriangleFilled(p, { p.x + dx, p.y + dy * 0.4f }, { p.x + dx * 0.4f, p.y + dy }, ToU32(WithAlpha(Colors::Viewport::Text, 0.9f)));
                }
            }

            drawList->AddRect(ToScreen(selMin), ToScreen(selMax), COLOR_SELECTED, 0.0f, 0, 2.0f);

            if(desc)
            {
                const ImVec2 pivot = ToScreen(selMin + (selMax - selMin) * desc->Pivot);
                drawList->AddCircle(pivot, 5.0f, ToU32(Colors::Blue), 16, 2.0f);
            }

            for(const Handle& handle : handles)
            {
                const ImVec2 hMin = { handle.Pos.x - HANDLE_HALF_SIZE, handle.Pos.y - HANDLE_HALF_SIZE };
                const ImVec2 hMax = { handle.Pos.x + HANDLE_HALF_SIZE, handle.Pos.y + HANDLE_HALF_SIZE };
                const bool highlighted = handle.Bits == handleUnderMouse || (mDragMode == DragMode::RESIZE && handle.Bits == mDragHandles);
                drawList->AddRectFilled(hMin, hMax, highlighted ? COLOR_SELECTED : ToU32(Colors::White));
                drawList->AddRect(hMin, hMax, ToU32(Colors::Viewport::Shade));
            }

            // Size readout (UI units)
            const glm::vec2 sizeUI = (selMax - selMin) / scale;
            const String sizeText = std::format("{:.0f} x {:.0f}", sizeUI.x, sizeUI.y);
            const ImVec2 textPos = { ToScreen(selMin).x, ToScreen(selMax).y + 6.0f };
            const ImVec2 textSize = ImGui::CalcTextSize(sizeText.c_str());
            drawList->AddRectFilled({ textPos.x - 3.0f, textPos.y - 1.0f }, { textPos.x + textSize.x + 3.0f, textPos.y + textSize.y + 1.0f }, ToU32(WithAlpha(Colors::Viewport::Shade, 0.78f)), 3.0f);
            drawList->AddText(textPos, ToU32(Colors::Viewport::Text), sizeText.c_str());
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
