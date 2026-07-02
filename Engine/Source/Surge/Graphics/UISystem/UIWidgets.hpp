// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Memory.hpp"
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Vector.hpp"
#include "Surge/Core/String.hpp"
#include "Surge/Graphics/RenderGraph/FrameBlackboard.hpp"

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace Surge::UI
{
    class IEventCallback : public RefCounted
    {
    public:
        virtual ~IEventCallback() = default;
        virtual void Invoke() = 0;
    };

    /*
    * (Rid) Example Usage of UI::IEventCallback
    *
    *   // LuaEventCallback is used in Surge/ScriptEngine/UIBindings.cpp to bind Lua functions to UI events.
    *      If you are in C++ land, you can just use std::function<void()> instead of sol::protected_function
    *   class LuaEventCallback : public UI::IEventCallback
    *   {
    *   public:
    *       LuaEventCallback(sol::protected_function func) // OR std::function&& of you are in C++ Land
    *           : mFunc(std::move(func)) {}
    *
    *       void Invoke() override
    *       {
    *           // if (mFunc) mFunc(); // If you are in C++ land, you can just call the function directly
    *
    *           if(mFunc.valid())
    *           {
    *               auto result = mFunc();
    *               if(!result.valid())
    *               {
    *                   sol::error err = result;
    *                   Log<Severity::Error>("Lua UI Callback Error: {}", err.what());
    *               }
    *           }
    *       }
    *   private:
    *       sol::protected_function mFunc; // OR std::function<void()> of you are in C++ Land
    *   };
    */

    class Widget : public RefCounted
    {
    public:
        virtual ~Widget() = default;

        void ClearAll();

        Vector<Ref<Widget>>& GetChildren() { return mChildren; }
        void AddChild(Ref<Widget> child)
        {
            child->mParent = this;
            mChildren.push_back(child);
        }

        void SetAnchor(float x, float y) { mAnchor = { x, y }; }
        void SetPivot(float x, float y) { mPivot = { x, y }; }
        void SetOffset(float x, float y) { mLocalOffset = { x, y }; }
        void SetSize(float w, float h) { mSize = { w, h }; }
        void SetColor(const glm::vec4& color) { mColor = color; }

        Widget* GetParent() { return mParent; }
        glm::vec2 GetAnchor() { return mAnchor; }
        glm::vec2 GetPivot() { return mPivot; }
        glm::vec2 GetOffset() { return mLocalOffset; }
        glm::vec2 GetSize() { return mSize; }
        glm::vec4 GetColor() { return mColor; }

        void GetGlobalBounds(glm::vec2& outPos, glm::vec2& outSize);

        void SetOnClick(Ref<IEventCallback> callback) { mOnClick = callback; mIsInteractable = true; }
        void SetOnHoverEnter(Ref<IEventCallback> callback) { mOnHoverEnter = callback; mIsInteractable = true; }
        void SetOnHoverExit(Ref<IEventCallback> callback) { mOnHoverExit = callback; mIsInteractable = true; }
        bool IsHovered() const { return mIsHovered; }
        bool IsPressed() const { return mIsPressed; }

        virtual void GenerateDrawCommands(FrameBlackboard& blackboard)
        {
            for(auto& child : mChildren)
                child->GenerateDrawCommands(blackboard);
        }

        // Recursive Hit Test
        virtual Widget* HitTest(float x, float y)
        {
            // Children first (they draw on top of parents)
            for(auto it = mChildren.rbegin(); it != mChildren.rend(); ++it)
            {
                Widget* hit = (*it)->HitTest(x, y);
                if(hit)
                    return hit;
            }

            if(!mIsInteractable)
                return nullptr;

            glm::vec2 globalPos, globalSize;
            GetGlobalBounds(globalPos, globalSize);

            if(x >= globalPos.x && x <= globalPos.x + globalSize.x && y >= globalPos.y && y <= globalPos.y + globalSize.y)
                return this;

            return nullptr;
        }

        virtual void OnMouseEnter() { mIsHovered = true; if(mOnHoverEnter) mOnHoverEnter->Invoke(); }
        virtual void OnMouseExit() { mIsHovered = false; mIsPressed = false; if(mOnHoverExit) mOnHoverExit->Invoke(); }
        virtual void OnMouseDown() { mIsPressed = true; }
        virtual void OnMouseUp() { if(mIsPressed && mOnClick) mOnClick->Invoke(); mIsPressed = false; }

    protected:
        glm::vec2 mAnchor { 0.0f, 0.0f };
        glm::vec2 mPivot { 0.0f, 0.0f };
        glm::vec2 mLocalOffset { 0.0f, 0.0f };
        glm::vec2 mSize { 100.0f, 100.0f };
        glm::vec4 mColor = { 1.0f, 1.0f, 1.0f, 1.0f };

        Widget* mParent = nullptr;
        Vector<Ref<Widget>> mChildren;

        bool mIsInteractable = false;
        bool mIsHovered = false;
        bool mIsPressed = false;

        Ref<IEventCallback> mOnClick;
        Ref<IEventCallback> mOnHoverEnter;
        Ref<IEventCallback> mOnHoverExit;
    };

    class Image : public Widget
    {
    public:
        Image(ImageHandle textureId = ImageHandle::Invalid())
            : mTextureID(textureId) {}

        virtual void GenerateDrawCommands(FrameBlackboard& blackboard) override
        {
            glm::vec2 globalPos, globalSize;
            GetGlobalBounds(globalPos, globalSize); // Calculate anchored screen position

            glm::mat4 transform = glm::mat4(1.0f);
            float centerX = globalPos.x + (globalSize.x * 0.5f);
            float centerY = globalPos.y + (globalSize.y * 0.5f);
            transform = glm::translate(transform, glm::vec3(centerX, centerY, 0.0f));
            transform = glm::scale(transform, glm::vec3(globalSize.x, globalSize.y, 1.0f));

            QuadSubmitCmd cmd;
            cmd.Transform = transform;
            cmd.Color = mColor;
            cmd.Texture = mTextureID;
            cmd.Billboard = false;
            blackboard.UISpriteList.push_back(cmd);

            Widget::GenerateDrawCommands(blackboard); // Recursive call
        }
    public:
        ImageHandle mTextureID;
    };

    class Text final : public Widget
    {
    public:
        Text(const String& text, AssetID fontAsset)
            : mText(text), mFontAsset(fontAsset) {}

        ~Text();

        virtual void GenerateDrawCommands(FrameBlackboard& blackboard) override;

        void SetText(const String& text) { mText = text; }
        void SetFontSize(float size) { mFontSize = size; }
        void SetTextAlignment(TextAlignment alignment) { mTextAlignment = alignment; }
        void SetTextVAlignment(TextVerticalAlignment alignment) { mTextVerticalAlignment = alignment; }

        AssetID GetFontAssetID() const { return mFontAsset; }
        String& GetTextBuffer() { return mText; }
        float GetFontSize() const { return mFontSize; }
        TextAlignment GetTextAlignment() const { return mTextAlignment; }
        TextVerticalAlignment GetTextVAlignment() const { return mTextVerticalAlignment; }

    protected:
        String mText;
        TextAlignment mTextAlignment = TextAlignment::LEFT;
        TextVerticalAlignment mTextVerticalAlignment = TextVerticalAlignment::CENTER;
        float mFontSize = 18.0f;
        AssetID mFontAsset;
    };

    class Button final : public Image
    {
    public:
        // Pass the background texture, the string, and the font
        Button(const String& text, AssetID fontAsset, ImageHandle textureId = ImageHandle::Invalid())
            : Image(textureId)
        {
            mIsInteractable = true;
            SetSize(200.0f, 50.0f);

            mTextWidget = Ref<Text>::Create(text, fontAsset);
            mTextWidget->SetFontSize(18.0f);
            AddChild(mTextWidget);

            SetTextAlignment(TextAlignment::CENTER);
            SetTextVAlignment(TextVerticalAlignment::CENTER);
        }

        ~Button();

        Ref<Text> GetTextWidget() { return mTextWidget; }
        void SetText(const String& text) { if(mTextWidget) mTextWidget->SetText(text); }

        void SetTextAlignment(TextAlignment align, float paddingX = 15.0f)
        {
            switch(align)
            {
                case TextAlignment::CENTER:
                    mTextWidget->SetAnchor(0.5f, mTextWidget->GetAnchor().y);
                    mTextWidget->SetOffset(0.0f, mTextWidget->GetOffset().y);
                    break;
                case TextAlignment::LEFT:
                    mTextWidget->SetAnchor(0.0f, mTextWidget->GetAnchor().y);
                    mTextWidget->SetOffset(paddingX, mTextWidget->GetOffset().y);
                    break;
                case TextAlignment::RIGHT:
                    mTextWidget->SetAnchor(1.0f, mTextWidget->GetAnchor().y);
                    mTextWidget->SetOffset(-paddingX, mTextWidget->GetOffset().y);
                    break;
            }
            mTextWidget->SetPivot(0.0f, 0.0f);
            mTextWidget->SetTextAlignment(align);
        }

        void SetTextVAlignment(TextVerticalAlignment alignment, float paddingY = 15.0f)
        {
            switch(alignment)
            {
                case TextVerticalAlignment::CENTER:
                    mTextWidget->SetAnchor(mTextWidget->GetAnchor().x, 0.5f);
                    mTextWidget->SetOffset(mTextWidget->GetOffset().x, 0.0f);
                    break;
                case TextVerticalAlignment::TOP:
                    mTextWidget->SetAnchor(mTextWidget->GetAnchor().x, 0.0f);
                    mTextWidget->SetOffset(mTextWidget->GetOffset().x, paddingY);
                    break;
                case TextVerticalAlignment::BOTTOM:
                case TextVerticalAlignment::BASELINE:
                    mTextWidget->SetAnchor(mTextWidget->GetAnchor().x, 1.0f);
                    mTextWidget->SetOffset(mTextWidget->GetOffset().x, -paddingY);
                    break;
            }
            mTextWidget->SetPivot(0.0f, 0.0f);
            mTextWidget->SetTextVAlignment(alignment);
        }

        virtual void GenerateDrawCommands(FrameBlackboard& blackboard) override
        {
            if(mIsPressed)      mColor = PressedColor;
            else if(mIsHovered) mColor = HoverColor;
            else                mColor = NormalColor;

            // Calls GenerateDrawCommands on mTextWidget(child) recursively so it draws on top!
            Image::GenerateDrawCommands(blackboard);
        }

    public:
        glm::vec4 NormalColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 HoverColor = { 0.8f, 0.8f, 0.8f, 1.0f };
        glm::vec4 PressedColor = { 0.5f, 0.5f, 0.5f, 1.0f };

    private:
        Ref<Text> mTextWidget;
    };

    class ImageButton final : public Image
    {
    public:
        ImageButton(const ImageHandle& textureId = ImageHandle::Invalid())
            : Image(textureId)
        {
            mIsInteractable = true;
            SetSize(200.0f, 50.0f);
        }

        virtual void GenerateDrawCommands(FrameBlackboard& blackboard) override
        {
            if(mIsPressed)      mColor = PressedColor;
            else if(mIsHovered) mColor = HoverColor;
            else                mColor = NormalColor;

            // Calls GenerateDrawCommands on mTextWidget(child) recursively so it draws on top!
            Image::GenerateDrawCommands(blackboard);
        }

    public:
        glm::vec4 NormalColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 HoverColor = { 0.8f, 0.8f, 0.8f, 1.0f };
        glm::vec4 PressedColor = { 0.5f, 0.5f, 0.5f, 1.0f };
    };
}
