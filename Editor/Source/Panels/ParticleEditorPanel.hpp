// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Panels/IPanel.hpp"
#include "Gizmos/GizmoCanvas.hpp"
#include "Surge/ECS/Components/Components.hpp"
#include "Surge/Graphics/Particles/ParticleEffect.hpp"
#include "Surge/Graphics/Renderer/RenderView.hpp"
#include <imgui.h>
#include <filesystem>

namespace Surge
{
    /*
    * Particle Editor: edits ParticleEffect (.spfx) assets
    *   - Emitter list (add, duplicate, delete, reorder, enable) + the modules of the selected emitter
    *   - Live preview in its own isolated Renderer view (orbit: left drag, pan: right/middle drag, zoom: wheel)
    *   - Preview gizmos: emitter shapes, emit direction, collision planes, culling bounds and a move/rotate/scale manipulator
    *     for the selected emitter's shape (Q/W/E/R/T while the preview is hovered, F frames the emitter)
    *   - Camera drags capture the mouse: hidden cursor pinned where the drag started, the window and other widgets ignore it
    *   - Fullscreen (toolbar button, F11 while hovered, Esc leaves): covers the Editor window like a fullscreen viewport
    *   - Edits apply to the loaded asset right away: every scene instance updates live, Save (Ctrl+S) writes the file
    *   - Undo/Redo (Ctrl+Z / Ctrl+Y) with JSON snapshots, a drag or typing session is one step
    */
    class ParticleEditorPanel : public IPanel
    {
    public:
        ParticleEditorPanel() = default;
        virtual ~ParticleEditorPanel() override = default;

        virtual void Init(void* panelInitArgs) override;
        virtual void OnEvent([[maybe_unused]] Event& e) override {}
        virtual void Render(bool* show) override;
        virtual void Shutdown() override;

        void OpenEffect(AssetID effectID); // Asks before discarding unsaved changes
        void CloseEffect();                // Asks before discarding unsaved changes
        bool OwnsShortcuts() const { return mOwnsShortcuts; }
        void SetFullscreen(bool fullscreen);

        // Creates a new .spfx file (unique name inside absoluteDirectory) with one default emitter, returns INVALID on failure
        static AssetID CreateEffectAsset(const std::filesystem::path& absoluteDirectory, const String& baseName = "NewParticleEffect");

        static PanelCode GetStaticCode() { return PanelCode::ParticleEditor; }

    private:
        // Document
        void Save();
        void Revert(); // Reloads the file, drops the unsaved edits (they are live in the loaded asset)
        void MarkChanged(bool layoutChanged = false);
        void CommitUndoIfIdle();
        void Undo();
        void Redo();
        void RestoreState(const String& json);
        void HandleShortcuts();

        // UI
        void DrawEmptyState();
        void DrawToolbar();
        void DrawPreview(float height);
        void DrawEmitterList();
        void DrawEmitter(ParticleEmitterDesc& emitter);
        bool BeginModule(const char* name, bool* enabled, bool defaultOpen = false);
        void EndModule();

        // Emitter list edits (sub-emitter indices follow)
        void AddEmitter();
        void DuplicateEmitter(int index);
        void DeleteEmitter(int index);
        void MoveEmitter(int index, int direction);

        // Preview
        void UpdatePreview();
        glm::mat4 GetPreviewView() const;
        glm::mat4 GetPreviewProjection(float aspect) const;
        void DrawPreviewGrid(const ImVec2& min, const ImVec2& size, const glm::mat4& viewProjection);
        void HandlePreviewInput(bool hovered);

        // Preview gizmos
        void DrawPreviewGizmos(const ImVec2& min, const ImVec2& size, const glm::mat4& view, const glm::mat4& projection);
        void ManipulateSelectedShape(const ImVec2& min, const ImVec2& size, const glm::mat4& view, const glm::mat4& projection);
        void DrawPreviewToolbar(const ImVec2& min);
        void FrameSelectedEmitter();
        ParticleEmitterDesc* GetSelectedEmitter();

    private:
        PanelCode mCode = PanelCode::ParticleEditor;
        Ref<ParticleEffect> mEffect;
        int mSelectedEmitter = 0;
        bool mUnsaved = false;
        bool mDocumentChanged = false;
        bool mLayoutChanged = false;
        bool mOwnsShortcuts = false;
        bool mFocusWindow = false;

        // Fullscreen, like EditorViewport: back to the dock node it left
        bool mFullscreen = false;
        bool mRestoreDockAfterFullscreen = false;
        ImGuiID mDockID = 0;
        ImGuiID mDockBeforeFullscreen = 0;
        ImVec2 mWindowPos = { 0.0f, 0.0f }; // Restored when it was floating
        ImVec2 mWindowSize = { 0.0f, 0.0f };

        // Undo: JSON of the effect (ParticleEffectSourceIO)
        static constexpr size_t MAX_UNDO_STEPS = 64;
        Vector<String> mUndoStack;
        Vector<String> mRedoStack;
        String mCommitted;

        // Preview
        RenderViewID mView = INVALID_RENDER_VIEW;
        ParticleSystemComponent mPreview;
        bool mPreviewVisible = false; // Drawn this frame (the view only renders while it is on screen)
        bool mPreviewPaused = false;
        bool mLoopPreview = true;
        bool mMoveEmitter = false;  // Moves the preview emitter on a circle (world space trails, rate over distance)
        bool mShowGrid = true;
        bool mOrbiting = false;   // The preview's input item is active: a camera drag owns the mouse
        ImVec2 mDragAnchor = { 0.0f, 0.0f }; // The hidden cursor stays here during a camera drag
        float mPreviewSpeed = 1.0f;
        float mRestartTimer = 0.0f;
        float mMoveTime = 0.0f;
        float mYaw = 0.6f;
        float mPitch = 0.35f;
        float mDistance = 8.0f;
        glm::vec3 mTarget = glm::vec3(0.0f, 1.0f, 0.0f);
        ImVec2 mPreviewSize = { 0.0f, 0.0f };
        glm::mat4 mPreviewTransform = glm::mat4(1.0f); // Emitter transform of the last update (Move circles it)

        // Preview gizmos
        GizmoCanvas mCanvas;
        int mGizmoOperation = -1; // ImGuizmo::OPERATION, -1 = none (select only)
        bool mGizmoWorldSpace = false;
        bool mGizmoActive = false; // Hovered or dragged, the camera leaves the mouse to it
        bool mShowShapes = true;
        bool mShowDirection = true;
        bool mShowCollision = true;
        bool mShowBounds = false;
    };
} // namespace Surge
