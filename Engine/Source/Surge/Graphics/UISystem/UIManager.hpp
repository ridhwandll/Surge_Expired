// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "UIWidgets.hpp"
#include "Surge/Core/Profiler.hpp"

namespace Surge
{
    class Renderer;
}

namespace Surge::UI
{
    // Canvas scaling, same as Unity's CanvasScaler "Scale With Screen Size"
    // The canvas is designed at ReferenceResolution, at runtime UI units are scaled by the screen/reference ratio of the matched axis
    struct CanvasScaler
    {
        glm::vec2 ReferenceResolution = { 1920.0f, 1080.0f };
        float MatchWidthOrHeight = 1.0f; // 0 = match width, 1 = match height (default, portrait games usually want 0)

        float ComputeScale(const glm::vec2& screenSize) const;
        bool operator==(const CanvasScaler&) const = default;
    };

    class Manager
    {
    public:
        // Owner of a root set with SetUIRoot() from a script that is not a UICanvasComponent script
        static constexpr uint64_t GLOBAL_CANVAS_OWNER = 0;

    public:
        void Initialize() {}
        void Shutdown() { ClearCanvases(); ClearEditorPreview(); }

        // Canvases, one per owner (the UICanvasComponent entity UUID), drawn in SortOrder (higher = on top, receives input first)
        // A canvas has an optional layout root (UILayout asset) and an optional script root (SetUIRoot from its Lua script, drawn above the layout)
        void SetCanvasLayoutRoot(uint64_t owner, Ref<Widget> root);
        void SetCanvasScriptRoot(uint64_t owner, Ref<Widget> root);
        void SetCanvasProperties(uint64_t owner, int sortOrder, const CanvasScaler& scaler, bool visible);
        void RemoveCanvas(uint64_t owner);
        void ClearCanvases();
        Widget* FindWidget(uint64_t owner, const String& name); // Searches the layout root first, then the script root

        // Lua SetUIRoot(): attaches to the canvas whose script is currently running, or the global canvas
        void SetRoot(Ref<Widget> root) { SetCanvasScriptRoot(mScriptContextOwner, root); }
        void ClearRoot() { SetCanvasScriptRoot(mScriptContextOwner, nullptr); }

        // Scene wraps every UICanvasComponent script call with these
        void BeginCanvasScript(uint64_t owner) { mScriptContextOwner = owner; }
        void EndCanvasScript() { mScriptContextOwner = GLOBAL_CANVAS_OWNER; }

        // Editor only: a layout being edited, drawn on top of everything, never receives input
        void SetEditorPreview(Ref<Widget> root, const CanvasScaler& scaler);
        void ClearEditorPreview();

        // SetViewportBounds
        // Sets the viewport offset and size for the UI system. This is used to convert raw OS window coordinates to the UI coordinate system.
        // @param xOffset:    X offset of the viewport in raw OS window coordinates
        // @param yOffset:    Y offset of the viewport in raw OS window coordinates
        // @param width:      Width of the viewport in raw OS window coordinates
        // @param height:     Height of the viewport in raw OS window coordinates
        void SetViewportBounds(float xOffset, float yOffset, float width, float height)
        {
            mViewportOffset = { xOffset, yOffset };
            mViewportSize = { width, height };
        }

        // Resolution the UI was last laid out at (render target pixels)
        const glm::vec2& GetTargetResolution() const { return mTargetResolution; }

        // Inverse of ScreenToUI: render target pixels -> raw OS window coordinates (e.g. to point a tutorial arrow at a widget)
        glm::vec2 UIToScreen(const glm::vec2& uiPos) const { return mViewportOffset + uiPos / mTargetResolution * mViewportSize; }
        size_t GetCanvasCount() const { return mCanvases.size(); }

    private:
        struct Canvas
        {
            uint64_t Owner = GLOBAL_CANVAS_OWNER;
            Ref<Widget> LayoutRoot;
            Ref<Widget> ScriptRoot;
            int SortOrder = 0;
            CanvasScaler Scaler;
            bool Visible = true;
        };

        Canvas* FindCanvas(uint64_t owner);
        Canvas& GetOrCreateCanvas(uint64_t owner);
        void SortCanvases();
        void ReleaseRoot(Ref<Widget>& root); // Drops input trackers pointing into the tree, then destroys it
        Widget* HitTest(float x, float y);

        // ScreenToUI
        // Converts raw OS window coordinates to the UIs coordinate system (0,0) at top-left of the viewport, and (width,height) at bottom-right of the viewport
        glm::vec2 ScreenToUI(float screenX, float screenY) const
        {
            const float localX = screenX - mViewportOffset.x;
            const float localY = screenY - mViewportOffset.y;
            const float normalizedX = localX / mViewportSize.x;
            const float normalizedY = localY / mViewportSize.y;

            return { normalizedX * mTargetResolution.x, normalizedY * mTargetResolution.y };
        }

        bool IsInsideViewport(const glm::vec2& uiPos) const
        {
            return uiPos.x >= 0.0f && uiPos.x <= mTargetResolution.x && uiPos.y >= 0.0f && uiPos.y <= mTargetResolution.y;
        }

        // Called by the Renderer
        void ExtractRenderData(FrameBlackboard& blackboard);
        void ProcessMouseMove(float rawX, float rawY);
        bool ProcessMouseButton(float rawX, float rawY, bool isDown);

    private:
        Vector<Canvas> mCanvases; // Sorted by SortOrder
        uint64_t mScriptContextOwner = GLOBAL_CANVAS_OWNER;

        Ref<Widget> mEditorPreviewRoot;
        CanvasScaler mEditorPreviewScaler;

        // Trackers, Refs so a widget removed while hovered/pressed can never dangle
        Ref<Widget> mHoveredWidget;
        Ref<Widget> mPressedWidget;
        bool mUIVisible = true;

        glm::vec2 mViewportOffset = { 0.0f, 0.0f };
        glm::vec2 mViewportSize = { 1920.0f, 1080.0f };
        glm::vec2 mTargetResolution = { 1920.0f, 1080.0f };

        friend class Surge::Renderer;
    };

}
