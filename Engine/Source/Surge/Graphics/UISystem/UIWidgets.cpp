// Copyright (c) - SurgeTechnologies - All rights reserved
#include "UIWidgets.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Asset/AssetManager.hpp"

namespace Surge
{
    // Recursive Math: Calculate absolute screen coordinates based on parent bounds
    void UI::Widget::GetGlobalBounds(glm::vec2& outPos, glm::vec2& outSize)
    {
        outSize = mSize * UI::Manager::DPI_SCALE;

        if(mParent)
        {
            glm::vec2 parentPos, parentSize;
            mParent->GetGlobalBounds(parentPos, parentSize);

            outPos.x = parentPos.x + (parentSize.x * mAnchor.x) + (mLocalOffset.x * UI::Manager::DPI_SCALE) - (outSize.x * mPivot.x);
            outPos.y = parentPos.y + (parentSize.y * mAnchor.y) + (mLocalOffset.y * UI::Manager::DPI_SCALE) - (outSize.y * mPivot.y);
            //Log<Severity::Debug>("Global bounds for WIDGET: ({}, {}), Size: ({}, {})", outPos.x, outPos.y, outSize.x, outSize.y);
        }
        else // Root Widget
        {
            outPos.x = (mLocalOffset.x * UI::Manager::DPI_SCALE) - (mSize.x * mPivot.x);
            outPos.y = (mLocalOffset.y * UI::Manager::DPI_SCALE) - (mSize.y * mPivot.y);
            //Log<Severity::Info>("Global bounds for ROOT: ({}, {}), Size: ({}, {})", outPos.x, outPos.y, outSize.x, outSize.y);
        }
    }

    void UI::Text::GenerateDrawCommands(FrameBlackboard& blackboard)
    {
        glm::vec2 globalPos, globalSize;
        GetGlobalBounds(globalPos, globalSize);

        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, glm::vec3(globalPos.x, globalPos.y, 0.0f));

        auto fontSize = mFontSize * UI::Manager::DPI_SCALE;
        transform = glm::scale(transform, glm::vec3(fontSize, fontSize, 1.0f));

        TextSubmitCmd cmd {};
        cmd.Transform = transform;
        cmd.Text = mText;
        cmd.Color = mColor;
        cmd.FontAsset = Core::GetAssetManager()->Load<Font>(mFontAsset).Raw();
        cmd.Billboard = false;
        cmd.Alignment = mTextAlignment;
        cmd.VerticalAlignment = mTextVerticalAlignment;
        cmd.MaxWidth = globalSize.x; // Use widgets width for auto-wrapping!

        blackboard.UITextList.push_back(cmd);
        Widget::GenerateDrawCommands(blackboard);
    }

    UI::Text::~Text()
    {
        if (Core::GetAssetManager()->Unload(mFontAsset))
            Log<Severity::Info>("UI::Text::~Text(): Font asset unloaded successfully.");
    }

    UI::Button::~Button()
    {
        if (Core::GetAssetManager()->Unload(mTextWidget->GetFontAssetID()))
            Log<Severity::Info>("UI::Button::~Button(): Font asset unloaded successfully.");
    }

}