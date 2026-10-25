// Copyright (c) - SurgeTechnologies - All rights reserved
#include "UIManager.hpp"
#include "UIInputField.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Time/Clock.hpp"
#include "Surge/Core/Input/Input.hpp"
#include "Surge/Utility/Platform.hpp"
#include <algorithm>
#include <cmath>

namespace Surge::UI
{
    static bool IsInTree(const Widget* widget, const Widget* root)
    {
        const Widget* w = widget;
        while(w)
        {
            if(w == root)
                return true;

            w = w->GetParent();
        }
        return false;
    }

    float CanvasScaler::ComputeScale(const glm::vec2& screenSize) const
    {
        if(screenSize.x <= 0.0f || screenSize.y <= 0.0f || ReferenceResolution.x <= 0.0f || ReferenceResolution.y <= 0.0f)
            return 1.0f;

        // Interpolate in log space so 2x wider + 0.5x taller matched 50/50 gives 1.0 (Unity)
        const float logWidth = std::log2(screenSize.x / ReferenceResolution.x);
        const float logHeight = std::log2(screenSize.y / ReferenceResolution.y);
        const float match = glm::clamp(MatchWidthOrHeight, 0.0f, 1.0f);
        return std::exp2(glm::mix(logWidth, logHeight, match));
    }

    Manager::Canvas* Manager::FindCanvas(uint64_t owner)
    {
        for(Canvas& canvas : mCanvases)
        {
            if(canvas.Owner == owner)
                return &canvas;
        }
        return nullptr;
    }

    Manager::Canvas& Manager::GetOrCreateCanvas(uint64_t owner)
    {
        if(Canvas* canvas = FindCanvas(owner))
            return *canvas;

        Canvas canvas;
        canvas.Owner = owner;
        mCanvases.push_back(canvas);
        SortCanvases();
        return *FindCanvas(owner);
    }

    void Manager::SortCanvases()
    {
        std::stable_sort(mCanvases.begin(), mCanvases.end(), [](const Canvas& a, const Canvas& b) { return a.SortOrder < b.SortOrder; });
    }

    void Manager::ReleaseRoot(Ref<Widget>& root)
    {
        if(!root)
            return;

        if(mHoveredWidget && IsInTree(mHoveredWidget.get(), root.get()))
            mHoveredWidget = nullptr;

        for(PointerPress& press : mPointerPresses)
        {
            if(press.PressedWidget && IsInTree(press.PressedWidget.get(), root.get()))
                press.PressedWidget = nullptr;
        }

        // Silently: no blur/close callbacks while a tree is torn down, but the keyboard must not stay open
        if(mFocused && IsInTree(mFocused.get(), root.get()))
        {
            mFocused = nullptr;
            Platform::HideSoftKeyboard();
        }
        std::erase_if(mOverlays, [&](const Ref<Widget>& overlay) { return IsInTree(overlay.get(), root.get()); });

        root->Destroy();
        root = nullptr;
    }

    void Manager::SetCanvasLayoutRoot(uint64_t owner, Ref<Widget> root)
    {
        SG_ASSERT(!root || root->GetParent() == nullptr, "Root widget must not have a parent!");

        Canvas& canvas = GetOrCreateCanvas(owner);
        if(canvas.LayoutRoot.get() != root.get())
            ReleaseRoot(canvas.LayoutRoot);

        canvas.LayoutRoot = root;
    }

    void Manager::SetCanvasScriptRoot(uint64_t owner, Ref<Widget> root)
    {
        SG_ASSERT(!root || root->GetParent() == nullptr, "Root widget must not have a parent!");

        Canvas& canvas = GetOrCreateCanvas(owner);
        if(canvas.ScriptRoot.get() != root.get())
            ReleaseRoot(canvas.ScriptRoot);

        canvas.ScriptRoot = root;
    }

    void Manager::SetCanvasProperties(uint64_t owner, int sortOrder, const CanvasScaler& scaler, bool visible)
    {
        Canvas& canvas = GetOrCreateCanvas(owner);
        canvas.Scaler = scaler;
        canvas.Visible = visible;
        if(canvas.SortOrder != sortOrder)
        {
            canvas.SortOrder = sortOrder;
            SortCanvases();
        }
    }

    void Manager::RemoveCanvas(uint64_t owner)
    {
        for(auto it = mCanvases.begin(); it != mCanvases.end(); ++it)
        {
            if(it->Owner == owner)
            {
                ReleaseRoot(it->LayoutRoot);
                ReleaseRoot(it->ScriptRoot);
                mCanvases.erase(it);
                return;
            }
        }
    }

    void Manager::ClearCanvases()
    {
        SURGE_PROFILE_FUNC("Surge::UI::Manager::ClearCanvases");

        // No OnHoverExit/OnBlur here: this runs during teardown, Lua callbacks must not fire anymore
        mHoveredWidget = nullptr;
        mPointerPresses.clear();
        if(mFocused)
        {
            mFocused = nullptr;
            Platform::HideSoftKeyboard();
        }
        mOverlays.clear();
        mKeyboardShift = 0.0f;

        for(Canvas& canvas : mCanvases)
        {
            ReleaseRoot(canvas.LayoutRoot);
            ReleaseRoot(canvas.ScriptRoot);
        }
        mCanvases.clear();
    }

    Widget* Manager::FindWidget(uint64_t owner, const String& name)
    {
        Canvas* canvas = FindCanvas(owner);
        if(!canvas)
            return nullptr;

        for(Widget* root : { canvas->LayoutRoot.get(), canvas->ScriptRoot.get() })
        {
            if(!root)
                continue;

            if(root->GetName() == name)
                return root;
            if(Widget* found = root->FindChild(name, true))
                return found;
        }
        return nullptr;
    }

    void Manager::SetEditorPreview(Ref<Widget> root, const CanvasScaler& scaler)
    {
        mEditorPreviewRoot = root;
        mEditorPreviewScaler = scaler;
    }

    void Manager::ClearEditorPreview()
    {
        mEditorPreviewRoot = nullptr; // Owned by the UI Editor, it destroys the tree itself
    }

    //////////////////////
    // Overlays + focus //
    //////////////////////

    void Manager::OpenOverlay(Ref<Widget> widget)
    {
        if(!widget)
            return;

        CloseOverlay(widget.get()); // Re-opening moves it to the top
        mOverlays.push_back(widget);
    }

    void Manager::CloseOverlay(Widget* widget)
    {
        std::erase_if(mOverlays, [widget](const Ref<Widget>& overlay) { return overlay.get() == widget; });
    }

    void Manager::SetFocus(Widget* widget)
    {
        if(widget == mFocused.get())
            return;

        Ref<Widget> previous = mFocused;
        mFocused = widget ? Ref<Widget>(widget) : nullptr;
        mFocusTime = GetFrameInfo().Time;
        mKeyboardWasVisible = false;

        // Blur first: switching fields hides then shows the keyboard, the platform keeps the last request
        if(previous)
            previous->OnBlur();
        if(mFocused)
            mFocused->OnFocus();
    }

    // (Rid) Collect the focusable widgets in the tree rooted at 'widget' into 'outWidgets' (NavKey Tab)
    static void CollectFocusable(Widget* widget, Vector<Widget*>& outWidgets)
    {
        if(!widget->IsVisible())
            return;
        if(widget->IsFocusable())
            outWidgets.push_back(widget);
        for(auto& child : widget->GetChildren())
            CollectFocusable(child.get(), outWidgets);
    }

    void Manager::FocusNext(bool backwards)
    {
        if(!mFocused)
            return;

        Widget* root = mFocused.get();
        while(root->GetParent())
            root = root->GetParent();

        Vector<Widget*> focusable; // Only on Tab presses
        CollectFocusable(root, focusable);

        auto it = std::find(focusable.begin(), focusable.end(), mFocused.get());
        if(focusable.size() < 2 || it == focusable.end())
            return;

        const int count = static_cast<int>(focusable.size());
        const int index = static_cast<int>(it - focusable.begin());
        SetFocus(focusable[(index + (backwards ? count - 1 : 1)) % count]);
    }

    ///////////
    // Frame //
    ///////////

    void Manager::UpdateFrameInfo()
    {
        FrameInfo& frame = GetFrameInfo();
        Clock& clock = Core::GetClock();
        frame.Time = clock.GetLife();
        frame.DeltaTime = glm::min(clock.GetSeconds(), 0.1f); // A hitch must not fling scroll views across the list
        frame.ScreenSize = mTargetResolution;

        // Platform insets are window pixels, the UI target can be a different size (Editor game view)
        if(mSimulateSafeArea)
            frame.SafeInsets = mSimulatedSafeArea * glm::vec4(mTargetResolution, mTargetResolution);
        else
        {
            const glm::vec2 toUI = mViewportSize.x > 0.0f && mViewportSize.y > 0.0f ? mTargetResolution / mViewportSize : glm::vec2(1.0f);
            frame.SafeInsets = Platform::GetSafeAreaInsets() * glm::vec4(toUI, toUI);
        }
    }

    void Manager::PollTextInput()
    {
        if(!mFocused || mFocused->GetType() != WidgetType::INPUT_FIELD)
            return;

        InputField* field = static_cast<InputField*>(mFocused.get());
        Platform::TextInputState state;
        if(Platform::PollTextInputState(state))
            field->ApplyPlatformTextState(state.Text, state.SelectionStart, state.SelectionEnd);
        if(Platform::PollTextInputSubmit() && mFocused.get() == field)
            field->Submit();

        // The user closed the keyboard (Back, swipe): leave the field. A short grace period covers the hide/show of a focus switch
        const bool visible = Platform::IsSoftKeyboardVisible();
        if(mFocused && mKeyboardWasVisible && !visible && GetFrameInfo().Time - mFocusTime > 0.5f)
            SetFocus(nullptr);
        else
            mKeyboardWasVisible = visible || mKeyboardWasVisible;
    }

    void Manager::ExtractRenderData(FrameBlackboard& blackboard)
    {
        SURGE_PROFILE_FUNC("Surge::UI::Manager::ExtractRenderData");
        mUIVisible = blackboard.ShowUI;
        mTargetResolution = { blackboard.ScreenWidth, blackboard.ScreenHeight };
        UpdateFrameInfo();
        PollTextInput();

        if(!mUIVisible)
            return;

        // Keyboard avoidance: how far the focused field's bottom sits under the top of the soft keyboard
        const float dt = GetFrameInfo().DeltaTime;
        const float keyboardHeight = mViewportSize.y > 0.0f ? Platform::GetSoftKeyboardHeight() * (mTargetResolution.y / mViewportSize.y) : 0.0f;
        float targetShift = 0.0f;

        DrawList drawList = { .Sprites = blackboard.UISpriteList, .Texts = blackboard.UITextList, .Order = blackboard.UIDrawOrder, .ClipRects = blackboard.UIClipRects };
        auto generateRootDrawCommands = [&](Widget* root, const CanvasScaler& scaler) {
            if(!root || !root->IsVisible())
                return;

            root->LayoutAsRoot(mTargetResolution, scaler.ComputeScale(mTargetResolution));
            if(mFocused && keyboardHeight > 0.0f && IsInTree(mFocused.get(), root))
            {
                auto [pos, size] = mFocused->GetGlobalBounds();

                const float margin = 16.0f * scaler.ComputeScale(mTargetResolution);
                targetShift = glm::clamp(pos.y + size.y + margin - (mTargetResolution.y - keyboardHeight), 0.0f, pos.y); // Never push the field off the top
                if(mKeyboardShift > 0.0f)
                    root->TranslateLayout({ 0.0f, -mKeyboardShift });
            }
            drawList.CurrentClipIndex = UI_NO_CLIP;
            root->GenerateDrawCommands(drawList);
        };

        for(Canvas& canvas : mCanvases)
        {
            if(!canvas.Visible)
                continue;

            generateRootDrawCommands(canvas.LayoutRoot.get(), canvas.Scaler);
            generateRootDrawCommands(canvas.ScriptRoot.get(), canvas.Scaler);
        }
        mKeyboardShift = glm::mix(mKeyboardShift, targetShift, glm::min(dt * 12.0f, 1.0f)); // Eased, the keyboard slides in too

        // Overlays above every canvas (a callback could close one, index loop)
        for(size_t i = 0; i < mOverlays.size(); i++)
        {
            Ref<Widget> overlay = mOverlays[i];
            drawList.CurrentClipIndex = UI_NO_CLIP;
            overlay->GenerateOverlayDrawCommands(drawList);
        }

        drawList.CurrentClipIndex = UI_NO_CLIP;
        generateRootDrawCommands(mEditorPreviewRoot.get(), mEditorPreviewScaler);

        drawList.CurrentClipIndex = UI_NO_CLIP;
        generateRootDrawCommands(mDebugRoot.get(), mDebugScaler);
    }

    ///////////
    // Input //
    ///////////

    Widget* Manager::HitTest(float x, float y)
    {
        // Overlays first (topmost last opened), a modal one swallows everything below it
        for(auto it = mOverlays.rbegin(); it != mOverlays.rend(); ++it)
        {
            if(Widget* hit = (*it)->OverlayHitTest(x, y))
                return hit;

            if((*it)->IsOverlayModal())
                return nullptr;
        }

        // Top most canvas first, script root is drawn above the layout root
        for(auto it = mCanvases.rbegin(); it != mCanvases.rend(); ++it)
        {
            if(!it->Visible)
                continue;

            for(Widget* root : { it->ScriptRoot.get(), it->LayoutRoot.get() })
            {
                if(!root)
                    continue;
                if(Widget* hit = root->HitTest(x, y))
                    return hit;
            }
        }
        return nullptr;
    }

    void Manager::ProcessMouseMove(float rawX, float rawY)
    {
        SURGE_PROFILE_FUNC("Surge::UI::Manager::ProcessMouseMove");

        const glm::vec2 uiPos = ScreenToUI(rawX, rawY);
        Widget* hit = (mUIVisible && IsInsideViewport(uiPos)) ? HitTest(uiPos.x, uiPos.y) : nullptr;

        // If the mouse moved over a different widget than last frame
        if(hit != mHoveredWidget.get())
        {
            if(mHoveredWidget)
                mHoveredWidget->OnMouseExit();

            mHoveredWidget = Ref<Widget>(hit);

            if(mHoveredWidget)
                mHoveredWidget->OnMouseEnter();
        }
    }

    bool Manager::ProcessPointerDown(int64_t pointerID, float rawX, float rawY)
    {
        SURGE_PROFILE_FUNC("Surge::UI::Manager::ProcessPointerDown");

        const glm::vec2 uiPos = ScreenToUI(rawX, rawY);
        // Ignore presses that happen completely outside the game viewport (they still end text editing)
        if(!mUIVisible || !IsInsideViewport(uiPos))
        {
            SetFocus(nullptr);
            return false;
        }

        // A tap outside an open list (dropdown) closes it, and does nothing else
        if(!mOverlays.empty())
        {
            Ref<Widget> top = mOverlays.back();
            if(!top->IsOverlayModal() && !top->OverlayHitTest(uiPos.x, uiPos.y))
            {
                top->OnOverlayDismiss();
                CloseOverlay(top.get());
                return true;
            }
        }

        Widget* hit = HitTest(uiPos.x, uiPos.y);

        // A press on anything but a text field ends text editing (another field takes the focus on release, keeping the keyboard up)
        if(mFocused && hit != mFocused.get() && !(hit && hit->IsFocusable()))
            SetFocus(nullptr);

        if(!hit)
            return false;

        // Sticks/sliders follow one finger: a second finger on the same widget is swallowed
        if(hit->WantsDrag())
        {
            for(const PointerPress& other : mPointerPresses)
            {
                if(other.PointerID != pointerID && other.PressedWidget.get() == hit)
                {
                    mPointerPresses.push_back({ pointerID, nullptr, uiPos });
                    return true;
                }
            }
        }

        for(PointerPress& press : mPointerPresses)
        {
            if(press.PointerID == pointerID) // Missed release of this pointer
            {
                if(press.PressedWidget)
                    press.PressedWidget->CancelPress();
                press.PressedWidget = Ref<Widget>(hit);
                press.StartPos = uiPos;
                hit->OnPointerDown(uiPos);
                return true;
            }
        }

        mPointerPresses.push_back({ pointerID, Ref<Widget>(hit), uiPos });
        hit->OnPointerDown(uiPos);
        return true;
    }

    void Manager::ProcessPointerMove(int64_t pointerID, float rawX, float rawY)
    {
        for(PointerPress& press : mPointerPresses)
        {
            if(press.PointerID != pointerID || !press.PressedWidget)
                continue;

            const glm::vec2 uiPos = ScreenToUI(rawX, rawY);
            if(press.PressedWidget->WantsDrag())
            {
                press.PressedWidget->OnPointerDrag(uiPos);
                return;
            }

            // Past the threshold, the closest ancestor that scrolls along the drag takes the press over (the child gets no click)
            const glm::vec2 delta = uiPos - press.StartPos;
            const float threshold = glm::max(8.0f, 0.01f * glm::min(mTargetResolution.x, mTargetResolution.y));
            if(glm::length(delta) < threshold)
                return;

            for(Widget* ancestor = press.PressedWidget->GetParent(); ancestor; ancestor = ancestor->GetParent())
            {
                if(ancestor->CanTakeOverDrag(delta))
                {
                    press.PressedWidget->CancelPress();
                    press.PressedWidget = Ref<Widget>(ancestor);
                    ancestor->OnPointerDown(press.StartPos); // From where the finger first landed, the content follows it 1:1
                    ancestor->OnPointerDrag(uiPos);
                    break;
                }
            }
            return;
        }
    }

    bool Manager::ProcessPointerUp(int64_t pointerID, float rawX, float rawY, bool canceled)
    {
        SURGE_PROFILE_FUNC("Surge::UI::Manager::ProcessPointerUp");

        // Release must always be handled, even outside the viewport, otherwise the pressed widget stays stuck in the pressed state
        for(auto it = mPointerPresses.begin(); it != mPointerPresses.end(); ++it)
        {
            if(it->PointerID != pointerID)
                continue;

            Ref<Widget> pressed = it->PressedWidget;
            mPointerPresses.erase(it);
            if(!pressed)
                return true;

            // Another finger still holds the same widget: keep it pressed, that finger decides about the click
            for(const PointerPress& other : mPointerPresses)
            {
                if(other.PressedWidget.get() == pressed.get())
                    return true;
            }

            const glm::vec2 uiPos = ScreenToUI(rawX, rawY);
            const bool inside = !canceled && mUIVisible && IsInsideViewport(uiPos);
            Widget* hit = inside ? HitTest(uiPos.x, uiPos.y) : nullptr;
            if(hit == pressed.get())
                pressed->OnPointerUp(uiPos);
            else
                pressed->CancelPress(); // Hover exit was already reported by ProcessMouseMove, don't fire it twice
            return true;
        }
        return false;
    }

    bool Manager::ProcessScroll(float rawX, float rawY, float wheelDelta)
    {
        const glm::vec2 uiPos = ScreenToUI(rawX, rawY);
        if(!mUIVisible || !IsInsideViewport(uiPos) || wheelDelta == 0.0f)
            return false;

        // The wheel bubbles up from the widget under the cursor to the first one that scrolls
        Widget* widget = HitTest(uiPos.x, uiPos.y);
        while(widget)
        {
            if(widget->OnScroll(wheelDelta))
                return true;

            widget = widget->GetParent();
        }
        return false;
    }

    bool Manager::ProcessKeyPressed(Key key)
    {
        const bool ctrl = Input::IsKeyPressed(Key::LeftControl) || Input::IsKeyPressed(Key::RightControl);
        const bool shift = Input::IsKeyPressed(Key::LeftShift) || Input::IsKeyPressed(Key::RightShift);

        if(mFocused)
        {
            if(key == Key::Escape) // Also Android Back
            {
                SetFocus(nullptr);
                return true;
            }
            if(key == Key::Tab)
            {
                FocusNext(shift);
                return true;
            }
            return mFocused->OnKeyPressed(key, ctrl, shift);
        }

        // Back / Escape closes the topmost popup first
        if(key == Key::Escape && !mOverlays.empty())
        {
            Ref<Widget> top = mOverlays.back();
            top->OnOverlayDismiss();
            return true;
        }
        return false;
    }

    void Manager::ProcessTextInput(Uint codepoint)
    {
        if(mFocused)
            mFocused->OnTextInput(codepoint);
    }

    Widget* Manager::WidgetAt(float rawX, float rawY)
    {
        const glm::vec2 uiPos = ScreenToUI(rawX, rawY);
        if(!mUIVisible || !IsInsideViewport(uiPos))
            return nullptr;

        return HitTest(uiPos.x, uiPos.y);
    }

    Widget* Manager::WidgetAt(uint64_t owner, float rawX, float rawY)
    {
        Canvas* canvas = FindCanvas(owner);
        const glm::vec2 uiPos = ScreenToUI(rawX, rawY);
        if(!canvas || !canvas->Visible || !mUIVisible || !IsInsideViewport(uiPos))
            return nullptr;

        for(Widget* root : { canvas->ScriptRoot.get(), canvas->LayoutRoot.get() })
        {
            if(!root)
                continue;
            if(Widget* hit = root->HitTest(uiPos.x, uiPos.y))
                return hit;
        }
        return nullptr;
    }

    glm::vec2 Manager::ScreenToCanvas(uint64_t owner, float rawX, float rawY)
    {
        Canvas* canvas = FindCanvas(owner);
        const float scale = canvas ? GetCanvasScale(*canvas) : 1.0f;
        return ScreenToUI(rawX, rawY) / (scale > 0.0f ? scale : 1.0f);
    }

    glm::vec2 Manager::GetCanvasSize(uint64_t owner)
    {
        Canvas* canvas = FindCanvas(owner);
        const float scale = canvas ? GetCanvasScale(*canvas) : 1.0f;
        return mTargetResolution / (scale > 0.0f ? scale : 1.0f);
    }
}
