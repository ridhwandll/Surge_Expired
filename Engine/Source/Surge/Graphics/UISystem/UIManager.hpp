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
    class Manager
    {
    public:
        static float DPI_SCALE;
    public:
        void Initialize() { DPI_SCALE = 1.0f; }
        void Shutdown() { ClearRoot(); }

        void SetRoot(Ref<Widget> root)
        {
            SG_ASSERT(!root || root->GetParent() == nullptr, "Root widget must not have a parent!");

            if(mRootWidget)
                ClearRoot();

            mRootWidget = root;
        }
        void ClearRoot();

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
            //Log<Severity::Trace>("Viewport offset: ({}, {}) Size: ({}, {})", mViewportOffset.x, mViewportOffset.y, mViewportSize.x, mViewportSize.y);
        }

    private:
        // ScreenToUI
        // Converts raw OS window coordinates to the UIs coordinate system (0,0) at top-left of the viewport, and (width,height) at bottom-right of the viewport
        // @param screenX:    Raw OS window X coordinate
        // @param screenY:    Raw OS window Y coordinate
        // @return            glm::vec2: UI coordinates
        glm::vec2 ScreenToUI(float screenX, float screenY)
        {
            const float localX = screenX - mViewportOffset.x;
            const float localY = screenY - mViewportOffset.y;
            const float normalizedX = localX / mViewportSize.x;
            const float normalizedY = localY / mViewportSize.y;

            return { normalizedX * mTargetResolution.x, normalizedY * mTargetResolution.y };
        }

        // ExtractRenderData
        // Extracts render data from the UI hierarchy into the Frameblackboard for rendering
        // @param blackboard: The frame blackboard containing render information
        void ExtractRenderData(FrameBlackboard& blackboard)
        {
            SURGE_PROFILE_FUNC("Surgte::UI::Manager::ExtractRenderData");
            if (!blackboard.ShowUI)
                return;

            mTargetResolution = { blackboard.ScreenWidth, blackboard.ScreenHeight };

            if(mRootWidget)
            {
                constexpr float REFERENCE_HEIGHT = 1080.0f;
                UI::Manager::DPI_SCALE = mTargetResolution.y / REFERENCE_HEIGHT;
                mRootWidget->SetSize(mTargetResolution.x / UI::Manager::DPI_SCALE, mTargetResolution.y / UI::Manager::DPI_SCALE);
                mRootWidget->GenerateDrawCommands(blackboard);
            }
        }

        void ProcessMouseMove(float rawX, float rawY)
        {
            SURGE_PROFILE_FUNC("Surgte::UI::Manager::ProcessMouseMove");
            if(!mRootWidget)
                return;

            glm::vec2 uiPos = ScreenToUI(rawX, rawY);

            if(uiPos.x < 0.0f || uiPos.x > mTargetResolution.x || uiPos.y < 0.0f || uiPos.y > mTargetResolution.y)
            {
                if(mHoveredWidget)
                {
                    mHoveredWidget->OnMouseExit();
                    mHoveredWidget = nullptr;
                }
                return;
            }

            Widget* hit = mRootWidget->HitTest(uiPos.x, uiPos.y);

            // If the mouse moved over a different widget than last frame
            if(hit != mHoveredWidget)
            {
                if(mHoveredWidget)
                    mHoveredWidget->OnMouseExit();

                mHoveredWidget = hit;

                if(mHoveredWidget)
                    mHoveredWidget->OnMouseEnter();
            }
        }

        bool ProcessMouseButton(float rawX, float rawY, bool isDown)
        {
            SURGE_PROFILE_FUNC("Surgte::UI::Manager::ProcessMouseButton");
            if(!mRootWidget)
                return false;

            glm::vec2 uiPos = ScreenToUI(rawX, rawY);

            // Ignore clicks that happen completely outside the game viewport
            if(uiPos.x < 0.0f || uiPos.x > mTargetResolution.x || uiPos.y < 0.0f || uiPos.y > mTargetResolution.y)
                return false;

            if(isDown)
            {
                Widget* hit = mRootWidget->HitTest(uiPos.x, uiPos.y);
                if(hit)
                {
                    mPressedWidget = hit;
                    mPressedWidget->OnMouseDown();
                    return true;
                }
            }
            else
            {
                if(mPressedWidget)
                {
                    Widget* hit = mRootWidget->HitTest(uiPos.x, uiPos.y);
                    if(hit == mPressedWidget)
                        mPressedWidget->OnMouseUp();
                    else
                        mPressedWidget->OnMouseExit();

                    mPressedWidget = nullptr;
                    return true;
                }
            }
            return false;
        }

    private:
        Ref<Widget> mRootWidget;

        // Trackers
        Widget* mHoveredWidget = nullptr;
        Widget* mPressedWidget = nullptr;

        glm::vec2 mViewportOffset = { 0.0f, 0.0f };
        glm::vec2 mViewportSize = { 1920.0f, 1080.0f };
        glm::vec2 mTargetResolution = { 1920.0f, 1080.0f };

        friend class Surge::Renderer;
    };

}