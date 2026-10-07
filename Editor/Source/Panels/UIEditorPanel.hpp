// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Panels/IPanel.hpp"
#include "Surge/Graphics/UISystem/UILayout.hpp"
#include <imgui.h>
#include <filesystem>
#include <unordered_map>

namespace Surge
{
    /*
    * UI Editor: visual editor for UILayout (.sui) assets
    *   - Hierarchy (add/duplicate/delete/rename, drag to re-parent, reorder) + Properties (rect transform, anchor presets, per widget type settings)
    *   - WYSIWYG: the layout being edited is rendered by the real UI renderer in the Viewport, widgets are selected/moved/resized right there
    *   - Undo/Redo (Ctrl+Z / Ctrl+Y), Save (Ctrl+S), Duplicate (Ctrl+D), Delete, arrow keys nudge (Shift = 10 units), Ctrl while dragging snaps
    * At runtime a UICanvasComponent instantiates the layout, its Lua script finds widgets by name: entity.UICanvasC:FindWidget("PlayButton")
    */
    class UIEditorPanel : public IPanel
    {
    public:
        UIEditorPanel() = default;
        virtual ~UIEditorPanel() override = default;

        virtual void Init(void* panelInitArgs) override;
        virtual void OnEvent(Event& e) override;
        virtual void Render(bool* show) override;
        virtual void Shutdown() override;

        void OpenLayout(AssetID layoutID); // Asks before discarding unsaved changes
        void CloseLayout();                // Asks before discarding unsaved changes
        bool HasOpenLayout() const { return (bool)mLayout; }
        AssetID GetOpenLayoutID() const { return mLayout ? mLayout->GetID() : AssetID(UUID::INVALID); }

        // Creates a new, empty .sui file (unique name inside absoluteDirectory) and registers it, returns INVALID on failure
        static AssetID CreateLayoutAsset(const std::filesystem::path& absoluteDirectory, const String& baseName = "NewUILayout");

        // ViewportPanel hook, call right after the viewport image (edit mode only)
        // Draws widget outlines and selection handles, handles click to select, drag to move and handle to resize
        // Returns true while UI editing owns the viewport mouse (the entity gizmo is skipped then)
        bool OnViewportGUI(const ImVec2& viewportMin, const ImVec2& viewportSize);
        bool IsViewportEditingEnabled() const { return mLayout && mEditInViewport; }
        bool OwnsShortcuts() const { return mOwnsShortcuts; } // Ctrl+Z & co. belong to the layout history this frame, not the scene's

    public:
        static PanelCode GetStaticCode() { return PanelCode::UIEditor; }

    private:
        struct Snapshot
        {
            UI::WidgetDesc Root;
            UI::CanvasScaler Scaler;
            UUID Selected = UUID::INVALID;
        };

        enum class DragMode { NONE, MOVE, RESIZE };
        enum HandleBits : int { HANDLE_LEFT = BIT(0), HANDLE_RIGHT = BIT(1), HANDLE_TOP = BIT(2), HANDLE_BOTTOM = BIT(3) };

        struct PendingAction
        {
            enum class Type { ADD_WIDGET, REMOVE_WIDGET, DUPLICATE_WIDGET, REPARENT_WIDGET, MOVE_WIDGET }; // (DELETE is a Windows macro)
            Type Op;
            UUID Target = UUID::INVALID;
            UUID NewParent = UUID::INVALID;
            UI::WidgetType NewWidgetType = UI::WidgetType::BASE_WIDGET;
            int Direction = 0;
        };

    private:
        // Document
        void LoadFromAsset();
        void Save();
        void MarkChanged() { mDocumentChanged = true; mUnsaved = true; mNeedsRebuild = true; }
        void CommitUndoIfIdle();
        void Undo();
        void Redo();
        Snapshot TakeSnapshot() const { return { mRoot, mScaler, mSelected }; }
        void RestoreSnapshot(const Snapshot& snapshot);

        // Preview
        void RebuildPreview();
        void UpdatePreviewLayout();
        glm::vec2 GetTargetResolution() const;
        bool GetWidgetRect(UUID id, glm::vec2& outMin, glm::vec2& outMax) const;         // Pixels (render target)
        bool GetParentRectUI(UUID id, glm::vec2& outMin, glm::vec2& outSize) const;      // UI units
        void SetRectFromBounds(UI::WidgetDesc& desc, const glm::vec2& rectMin, const glm::vec2& rectMax, const glm::vec2& parentMin, const glm::vec2& parentSize) const; // UI units

        // Panels
        void DrawEmptyState();
        void DrawToolbar();
        void DrawHierarchy();
        void DrawHierarchyNode(UI::WidgetDesc& desc, bool isRoot);
        void DrawProperties();
        void DrawCanvasSettings();
        void DrawRectTransform(UI::WidgetDesc& desc);
        void DrawAnchorPresets(UI::WidgetDesc& desc);
        void DrawWidgetSettings(UI::WidgetDesc& desc);
        void HandleShortcuts(bool focused);

        // Structural edits (deferred until the hierarchy is done iterating)
        void QueueAction(const PendingAction& action) { mPendingActions.push_back(action); }
        void ApplyPendingActions();
        String MakeUniqueName(const String& base) const;
        AssetID GetDefaultFont() const;

    private:
        PanelCode mCode;

        Ref<UILayout> mLayout;   // Asset being edited, only written on Save
        UI::WidgetDesc mRoot;    // Working copy
        UI::CanvasScaler mScaler;
        UUID mSelected = UUID::INVALID;
        bool mUnsaved = false;
        bool mFocusWindow = false;

        // Live preview (rendered by the UI renderer in the Viewport)
        Ref<UI::Widget> mPreviewRoot;
        std::unordered_map<uint64_t, UI::Widget*> mPreviewWidgets;
        UI::LayoutResourceCache mResourceCache;
        bool mNeedsRebuild = true;
        bool mEditInViewport = true;
        bool mShowAllOutlines = true;
        bool mSnapToGrid = false;
        float mGridSize = 10.0f;

        // Undo/Redo, mCommitted is the last state pushed, changes are coalesced until the mouse/keyboard interaction ends
        Vector<Snapshot> mUndoStack;
        Vector<Snapshot> mRedoStack;
        Snapshot mCommitted;
        bool mDocumentChanged = false;

        // Viewport interaction
        DragMode mDragMode = DragMode::NONE;
        int mDragHandles = 0;
        ImVec2 mDragStartMouse = { 0.0f, 0.0f };
        glm::vec2 mDragStartMin = { 0.0f, 0.0f }; // UI units
        glm::vec2 mDragStartMax = { 0.0f, 0.0f };
        glm::vec2 mDragParentMin = { 0.0f, 0.0f };
        glm::vec2 mDragParentSize = { 0.0f, 0.0f };
        bool mViewportHovered = false;
        bool mOwnsShortcuts = false;

        Vector<PendingAction> mPendingActions;
        UUID mRenaming = UUID::INVALID;
        String mRenameBuffer;
        bool mRenameFocus = false;
    };
} // namespace Surge
