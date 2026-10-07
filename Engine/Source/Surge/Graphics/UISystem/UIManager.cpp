// Copyright (c) - SurgeTechnologies - All rights reserved
#include "UIManager.hpp"
#include <algorithm>
#include <cmath>

namespace Surge::UI
{
    static bool IsInTree(const Widget* widget, const Widget* root)
    {
        for(const Widget* w = widget; w; w = w->GetParent())
        {
            if(w == root)
                return true;
        }
        return false;
    }

    float CanvasScaler::ComputeScale(const glm::vec2& screenSize) const
    {
        if(screenSize.x <= 0.0f || screenSize.y <= 0.0f || ReferenceResolution.x <= 0.0f || ReferenceResolution.y <= 0.0f)
            return 1.0f;

        // Interpolate in log space so 2x wider + 0.5x taller matched 50/50 gives 1.0 (Unity does the same)
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
        if(mPressedWidget && IsInTree(mPressedWidget.get(), root.get()))
            mPressedWidget = nullptr;

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

        // No OnHoverExit here: this runs during teardown, Lua callbacks must not fire anymore
        mHoveredWidget = nullptr;
        mPressedWidget = nullptr;

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

    void Manager::ExtractRenderData(FrameBlackboard& blackboard)
    {
        SURGE_PROFILE_FUNC("Surge::UI::Manager::ExtractRenderData");
        mUIVisible = blackboard.ShowUI;
        mTargetResolution = { blackboard.ScreenWidth, blackboard.ScreenHeight };

        if(!mUIVisible)
            return;

        DrawList drawList = { blackboard.UISpriteList, blackboard.UITextList, blackboard.UIDrawOrder };
        auto drawRoot = [&](Widget* root, const CanvasScaler& scaler) {
            if(!root || !root->IsVisible())
                return;

            root->LayoutAsRoot(mTargetResolution, scaler.ComputeScale(mTargetResolution));
            root->GenerateDrawCommands(drawList);
        };

        for(Canvas& canvas : mCanvases)
        {
            if(!canvas.Visible)
                continue;

            drawRoot(canvas.LayoutRoot.get(), canvas.Scaler);
            drawRoot(canvas.ScriptRoot.get(), canvas.Scaler);
        }

        drawRoot(mEditorPreviewRoot.get(), mEditorPreviewScaler);
    }

    Widget* Manager::HitTest(float x, float y)
    {
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

    bool Manager::ProcessMouseButton(float rawX, float rawY, bool isDown)
    {
        SURGE_PROFILE_FUNC("Surge::UI::Manager::ProcessMouseButton");

        const glm::vec2 uiPos = ScreenToUI(rawX, rawY);
        const bool inside = mUIVisible && IsInsideViewport(uiPos);

        if(isDown)
        {
            // Ignore clicks that happen completely outside the game viewport
            if(!inside)
                return false;

            Widget* hit = HitTest(uiPos.x, uiPos.y);
            if(!hit)
                return false;

            mPressedWidget = Ref<Widget>(hit);
            mPressedWidget->OnMouseDown();
            return true;
        }

        // Release must always be handled, even outside the viewport, otherwise the pressed widget stays stuck in the pressed state
        if(!mPressedWidget)
            return false;

        Widget* hit = inside ? HitTest(uiPos.x, uiPos.y) : nullptr;
        if(hit == mPressedWidget.get())
            mPressedWidget->OnMouseUp();
        else
            mPressedWidget->CancelPress(); // Hover exit was already reported by ProcessMouseMove, don't fire it twice

        mPressedWidget = nullptr;
        return true;
    }
}
