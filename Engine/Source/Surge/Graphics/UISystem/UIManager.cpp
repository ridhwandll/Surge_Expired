// Copyright (c) - SurgeTechnologies - All rights reserved
#include "UIManager.hpp"

namespace Surge::UI
{
    float Manager::DPI_SCALE;

    void Manager::ClearRoot()
    {
        SURGE_PROFILE_FUNC("Surgte::UI::Manager::ClearRoot");
        if(mHoveredWidget)
        {
            mHoveredWidget->OnMouseExit();
            mHoveredWidget = nullptr;
        }

        if(mPressedWidget)
            mPressedWidget = nullptr;

        if (mRootWidget)
            mRootWidget = nullptr;
    }
}
