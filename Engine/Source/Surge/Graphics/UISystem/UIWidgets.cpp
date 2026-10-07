// Copyright (c) - SurgeTechnologies - All rights reserved
#include "UIWidgets.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Asset/AssetManager.hpp"

namespace Surge
{
    void UI::Widget::AddChild(Ref<Widget> child)
    {
        if(!child || child.get() == this)
            return;

        if(child->mParent == this)
            return; // Already ours, adding twice would draw and hit test it twice

        if(child->mParent)
            child->mParent->RemoveChild(child.get());

        child->mParent = this;
        mChildren.push_back(child);
    }

    void UI::Widget::RemoveChild(Widget* child)
    {
        for(auto it = mChildren.begin(); it != mChildren.end(); ++it)
        {
            if(it->get() == child)
            {
                child->mParent = nullptr;
                mChildren.erase(it); // May release the last reference to child, don't touch it after this
                return;
            }
        }
    }

    UI::Widget* UI::Widget::FindChild(const String& name, bool recursive)
    {
        for(auto& child : mChildren)
        {
            if(child->mName == name)
                return child.get();
        }

        if(recursive)
        {
            for(auto& child : mChildren)
            {
                if(Widget* found = child->FindChild(name, true))
                    return found;
            }
        }
        return nullptr;
    }

    void UI::Widget::LayoutAsRoot(const glm::vec2& screenSize, float scale)
    {
        mLayoutScale = scale;
        mGlobalPos = { 0.0f, 0.0f };
        mGlobalSize = screenSize;

        for(auto& child : mChildren)
            child->UpdateLayout(mGlobalPos, mGlobalSize, mLayoutScale);
    }

    void UI::Widget::UpdateLayout(const glm::vec2& parentPos, const glm::vec2& parentSize, float scale)
    {
        mLayoutScale = scale;

        const glm::vec2 anchorSpan = mAnchorMax - mAnchorMin;
        mGlobalSize = glm::max(parentSize * anchorSpan + mSize * scale, glm::vec2(0.0f));

        const glm::vec2 pivotPoint = parentPos + parentSize * (mAnchorMin + anchorSpan * mPivot) + mLocalOffset * scale;
        mGlobalPos = pivotPoint - mGlobalSize * mPivot;

        for(auto& child : mChildren)
            child->UpdateLayout(mGlobalPos, mGlobalSize, mLayoutScale);
    }

    UI::Widget* UI::Widget::HitTest(float x, float y)
    {
        if(!mVisible)
            return nullptr;

        // Children first (they draw on top of parents)
        for(auto it = mChildren.rbegin(); it != mChildren.rend(); ++it)
        {
            Widget* hit = (*it)->HitTest(x, y);
            if(hit)
                return hit;
        }

        if(!mIsInteractable)
            return nullptr;

        if(x >= mGlobalPos.x && x <= mGlobalPos.x + mGlobalSize.x && y >= mGlobalPos.y && y <= mGlobalPos.y + mGlobalSize.y)
            return this;

        return nullptr;
    }

    void UI::Widget::Destroy()
    {
        mOnClick = nullptr;
        mOnHoverEnter = nullptr;
        mOnHoverExit = nullptr;

        for(auto& child : mChildren)
        {
            child->Destroy();
            child->mParent = nullptr;
        }

        mChildren.clear();
        mParent = nullptr;
    }

    void UI::Text::GenerateDrawCommands(DrawList& drawList)
    {
        const float fontSize = mFontSize * mLayoutScale;

        // Text origin on the rect edge picked by the alignment (with a zero sized rect this is the anchor point itself)
        glm::vec2 origin = mGlobalPos;
        if(mTextAlignment == TextAlignment::CENTER)     origin.x += mGlobalSize.x * 0.5f;
        else if(mTextAlignment == TextAlignment::RIGHT) origin.x += mGlobalSize.x;

        if(mTextVerticalAlignment == TextVerticalAlignment::CENTER)
            origin.y += mGlobalSize.y * 0.5f;
        else if(mTextVerticalAlignment == TextVerticalAlignment::BOTTOM || mTextVerticalAlignment == TextVerticalAlignment::BASELINE)
            origin.y += mGlobalSize.y;

        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, glm::vec3(origin.x, origin.y, 0.0f));
        transform = glm::scale(transform, glm::vec3(fontSize, fontSize, 1.0f));

        TextSubmitCmd cmd {};
        cmd.Transform = transform;
        cmd.Text = mText;
        cmd.Color = mColor;
        cmd.FontAsset = mFontAsset.Raw();
        cmd.Billboard = false;
        cmd.Alignment = mTextAlignment;
        cmd.VerticalAlignment = mTextVerticalAlignment;

        // Glyph layout happens in font (em) units, the transform scales them by fontSize, so the pixel width must be converted
        cmd.MaxWidth = (mWordWrap && mGlobalSize.x > 0.0f && fontSize > 0.0f) ? mGlobalSize.x / fontSize : 0.0f;

        drawList.AddText(cmd);
        GenerateChildDrawCommands(drawList);
    }

    UI::Text::Text(const String& text, AssetID fontAsset)
        : mText(text)
    {
        mSize = { 0.0f, 0.0f };
        if(fontAsset)
            mFontAsset = Core::GetAssetManager()->Load<Font>(fontAsset);
    }

    UI::Text::Text(const String& text, Ref<Font> font)
        : mText(text), mFontAsset(font)
    {
        mSize = { 0.0f, 0.0f };
    }

}
