// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "UIWidgets.hpp"

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
        void Shutdown() { ClearCanvases(); ClearEditorPreview(); mDebugRoot = nullptr; }

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

        // Engine debug UI (performance overlay): drawn above everything, never hit tested (input goes through to the game),
        // untouched by ClearCanvases() so it survives scene changes. nullptr = nothing drawn
        void SetDebugRoot(Ref<Widget> root) { mDebugRoot = root; }

        // Editor only: fake device safe area (fractions of the UI target: left, top, right, bottom), replaces the platform insets
        void SetSimulatedSafeArea(const glm::vec4& fractions, bool enabled) { mSimulatedSafeArea = fractions; mSimulateSafeArea = enabled; }

        // Overlays: drawn after every canvas in open order and hit first (Dropdown lists, Modals). Widgets open/close themselves
        void OpenOverlay(Ref<Widget> widget);
        void CloseOverlay(Widget* widget);
        bool HasOverlays() const { return !mOverlays.empty(); }

        // Keyboard focus (InputField). The focused widget receives key/text events, a press elsewhere or Escape/Back blurs it
        void SetFocus(Widget* widget);
        Widget* GetFocused() { return mFocused.get(); }

        // SetViewportBounds
        // Sets the viewport offset and size for the UI system. This is used to convert raw OS window coordinates to the UI coordinate system.
        // @param xOffset:    X offset of the viewport in raw OS window coordinates
        // @param yOffset:    Y offset of the viewport in raw OS window coordinates
        // @param width:      Width of the viewport in raw OS window coordinates
        // @param height:     Height of the viewport in raw OS window coordinates
        void SetViewportBounds(float xOffset, float yOffset, float width, float height)
        {
            SG_ASSERT(mViewportSize.x > 0.0f && mViewportSize.y > 0.0f, "Invalid viewport size");

            mViewportOffset = { xOffset, yOffset };
            mViewportSize = { width, height };
        }

        // Resolution the UI was last laid out at (render target pixels)
        const glm::vec2& GetTargetResolution() const { return mTargetResolution; }

        // Render target pixels -> raw OS window coordinates (e.g. to point a tutorial arrow at a widget)
        glm::vec2 UIToScreen(const glm::vec2& uiPos) const { return mViewportOffset + uiPos / mTargetResolution * mViewportSize; }
        size_t GetCanvasCount() const { return mCanvases.size(); }

        // Pointer queries, positions in raw OS window coordinates (the space of mouse/touch events and Input::GetTouch)
        Widget* WidgetAt(float rawX, float rawY);                 // Topmost interactable widget of all visible canvases, nullptr if none
        Widget* WidgetAt(uint64_t owner, float rawX, float rawY); // Same, restricted to one canvas
        bool IsPointerOverUI(float rawX, float rawY) { return WidgetAt(rawX, rawY) != nullptr; }
        glm::vec2 ScreenToCanvas(uint64_t owner, float rawX, float rawY); // Canvas units, the units Offset/Size are authored in
        glm::vec2 GetCanvasSize(uint64_t owner);                          // Visible canvas area in canvas units (width depends on the aspect)

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
        void ReleaseRoot(Ref<Widget>& root); // Drops input trackers, focus and overlays pointing into the tree, then destroys it
        Widget* HitTest(float x, float y);
        void UpdateFrameInfo();
        void PollTextInput();
        void FocusNext(bool backwards); // Tab: next focusable widget of the focused widget's tree

        // ScreenToUI
        // Converts raw OS window coordinates to the UIs coordinate system (0,0) at top-left of the viewport, and (width,height) at bottom-right of the viewport
        glm::vec2 ScreenToUI(float screenX, float screenY) const
        {
            const float localX = screenX - mViewportOffset.x;
            const float localY = screenY - mViewportOffset.y;
            const float normalizedX = localX / mViewportSize.x;
            const float normalizedY = localY / mViewportSize.y;

            glm::vec2 result = { normalizedX * mTargetResolution.x, normalizedY * mTargetResolution.y };
            return result;
        }

        bool IsInsideViewport(const glm::vec2& uiPos) const
        {
            return uiPos.x >= 0.0f && uiPos.x <= mTargetResolution.x && uiPos.y >= 0.0f && uiPos.y <= mTargetResolution.y;
        }

        // Called by the Renderer
        void ExtractRenderData(FrameBlackboard& blackboard);
        void ProcessMouseMove(float rawX, float rawY);
        bool ProcessMouseButton(float rawX, float rawY, bool isDown) { return isDown ? ProcessPointerDown(MOUSE_POINTER_ID, rawX, rawY) : ProcessPointerUp(MOUSE_POINTER_ID, rawX, rawY, false); }
        bool ProcessScroll(float rawX, float rawY, float wheelDelta);
        bool ProcessKeyPressed(Key key);
        void ProcessTextInput(Uint codepoint);

        // Every pointer (mouse, each touch) presses its own widget: holding one button never blocks another finger
        static constexpr int64_t MOUSE_POINTER_ID = -1;
        bool ProcessPointerDown(int64_t pointerID, float rawX, float rawY);
        void ProcessPointerMove(int64_t pointerID, float rawX, float rawY);
        bool ProcessPointerUp(int64_t pointerID, float rawX, float rawY, bool canceled);
        float GetCanvasScale(const Canvas& canvas) const { return canvas.Scaler.ComputeScale(mTargetResolution); }

    private:
        Vector<Canvas> mCanvases; // Sorted by SortOrder
        uint64_t mScriptContextOwner = GLOBAL_CANVAS_OWNER;

        Ref<Widget> mEditorPreviewRoot;
        CanvasScaler mEditorPreviewScaler;
        glm::vec4 mSimulatedSafeArea = { 0.0f, 0.0f, 0.0f, 0.0f };
        bool mSimulateSafeArea = false;

        Vector<Ref<Widget>> mOverlays; // Open order, last = on top

        Ref<Widget> mDebugRoot;
        CanvasScaler mDebugScaler = { { 1280.0f, 720.0f }, 0.5f };

        // Trackers, Refs so a widget removed while hovered/pressed can never dangle
        struct PointerPress
        {
            int64_t PointerID;
            Ref<Widget> PressedWidget;
            glm::vec2 StartPos; // UI pixels, where the press began (drag threshold, scroll hand-off)
        };
        Ref<Widget> mHoveredWidget;
        Vector<PointerPress> mPointerPresses;
        bool mUIVisible = true;

        // Focus + soft keyboard
        Ref<Widget> mFocused;
        float mFocusTime = 0.0f;
        bool mKeyboardWasVisible = false;
        float mKeyboardShift = 0.0f; // Pixels the focused field's canvas is moved up so the keyboard doesn't cover the field

        glm::vec2 mViewportOffset = { 0.0f, 0.0f };
        glm::vec2 mViewportSize = { 1920.0f, 1080.0f };
        glm::vec2 mTargetResolution = { 1920.0f, 1080.0f };

        friend class Surge::Renderer;
    };

}
