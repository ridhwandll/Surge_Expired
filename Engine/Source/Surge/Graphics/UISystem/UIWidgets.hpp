// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Memory.hpp"
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Vector.hpp"
#include "Surge/Core/String.hpp"
#include "Surge/Core/Profiler.hpp"
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

    enum class WidgetType
    {
        BASE_WIDGET, // Empty container ("Panel" in the UI Editor), draws nothing itself
        IMAGE,
        TEXT,
        BUTTON,
        IMAGE_BUTTON,
    };

    // Where a widget hierarchy writes its draw commands, Order keeps sprites and text interleaved in hierarchy order
    struct DrawList
    {
        Vector<QuadSubmitCmd>& Sprites;
        Vector<TextSubmitCmd>& Texts;
        Vector<UIDrawItem>& Order;

        void AddSprite(const QuadSubmitCmd& cmd) { Order.push_back({ false, static_cast<Uint>(Sprites.size()) }); Sprites.push_back(cmd); }
        void AddText(const TextSubmitCmd& cmd) { Order.push_back({ true, static_cast<Uint>(Texts.size()) }); Texts.push_back(cmd); }
    };

    /*
    * Rect transform (all values in UI units, multiplied by the canvas scale to get pixels):
    *   AnchorMin/AnchorMax : Normalized position inside the parent rect. Equal = point anchor, different = the widget stretches with the parent on that axis
    *   Pivot               : Normalized point of the widget that sits on the anchor (0,0 = top left, 1,1 = bottom right)
    *   Offset              : Position of the pivot relative to the anchor
    *   Size                : Size of the widget. On a stretched axis it is added to the stretched size (negative values inset the widget)
    * Same model as Unity's RectTransform, with a point anchor it reduces to: pos = parentPos + parentSize * anchor + offset - size * pivot
    */
    class Widget : public RefCounted
    {
    public:
        virtual ~Widget() = default;

        // Hierarchy
        Vector<Ref<Widget>>& GetChildren() { return mChildren; }
        const Vector<Ref<Widget>>& GetChildren() const { return mChildren; }
        void AddChild(Ref<Widget> child);   // Re-parents the child if it already has a parent, adding the same child twice is a no-op
        void RemoveChild(Widget* child);
        Widget* GetParent() const { return mParent; }
        Widget* FindChild(const String& name, bool recursive = true); // Depth first, nullptr if not found

        // Identity/State
        void SetName(const String& name) { mName = name; }
        const String& GetName() const { return mName; }
        void SetVisible(bool visible) { mVisible = visible; }
        bool IsVisible() const { return mVisible; }
        void SetInteractable(bool interactable) { mIsInteractable = interactable; }
        bool IsInteractable() const { return mIsInteractable; }

        // Rect transform
        void SetAnchor(float x, float y) { mAnchorMin = mAnchorMax = { x, y }; } // Point anchor
        void SetAnchorMin(float x, float y) { mAnchorMin = { x, y }; }
        void SetAnchorMax(float x, float y) { mAnchorMax = { x, y }; }
        void SetPivot(float x, float y) { mPivot = { x, y }; }
        void SetOffset(float x, float y) { mLocalOffset = { x, y }; }
        void SetSize(float w, float h) { mSize = { w, h }; }
        void SetColor(const glm::vec4& color) { mColor = color; }

        glm::vec2 GetAnchor() const { return mAnchorMin; }
        glm::vec2 GetAnchorMin() const { return mAnchorMin; }
        glm::vec2 GetAnchorMax() const { return mAnchorMax; }
        glm::vec2 GetPivot() const { return mPivot; }
        glm::vec2 GetOffset() const { return mLocalOffset; }
        glm::vec2 GetSize() const { return mSize; }
        glm::vec4 GetColor() const { return mColor; }

        // Layout, recomputed once per frame by UI::Manager (cached so hit testing and drawing don't walk up the hierarchy)
        void LayoutAsRoot(const glm::vec2& screenSize, float scale);  // Root always covers the whole canvas
        void UpdateLayout(const glm::vec2& parentPos, const glm::vec2& parentSize, float scale);
        void GetGlobalBounds(glm::vec2& outPos, glm::vec2& outSize) const { outPos = mGlobalPos; outSize = mGlobalSize; } // Pixels, (0,0) = top left of the viewport
        float GetLayoutScale() const { return mLayoutScale; }

        // Events
        void SetOnClick(Ref<IEventCallback> callback) { mOnClick = callback; mIsInteractable = true; }
        void SetOnHoverEnter(Ref<IEventCallback> callback) { mOnHoverEnter = callback; mIsInteractable = true; }
        void SetOnHoverExit(Ref<IEventCallback> callback) { mOnHoverExit = callback; mIsInteractable = true; }
        bool IsHovered() const { return mIsHovered; }
        bool IsPressed() const { return mIsPressed; }

        virtual void GenerateDrawCommands(DrawList& drawList)
        {
            SURGE_PROFILE_FUNC("Widget::GenerateDrawCommands");
            GenerateChildDrawCommands(drawList);
        }

        // Recursive Hit Test, invisible widgets (and their children) are skipped
        virtual Widget* HitTest(float x, float y);

        // Breaks the widget <-> callback <-> Lua closure reference cycles, call before dropping a hierarchy
        virtual void Destroy();

        virtual WidgetType GetType() const { return WidgetType::BASE_WIDGET; }
        virtual void OnMouseEnter() { mIsHovered = true; if(mOnHoverEnter) mOnHoverEnter->Invoke(); }
        virtual void OnMouseExit() { mIsHovered = false; mIsPressed = false; if(mOnHoverExit) mOnHoverExit->Invoke(); }
        virtual void OnMouseDown() { mIsPressed = true; }
        virtual void OnMouseUp() { if(mIsPressed && mOnClick) mOnClick->Invoke(); mIsPressed = false; }
        void CancelPress() { mIsPressed = false; } // Released somewhere else, no click

    protected:
        void GenerateChildDrawCommands(DrawList& drawList)
        {
            for(auto& child : mChildren)
            {
                if(child->IsVisible())
                    child->GenerateDrawCommands(drawList);
            }
        }

    protected:
        String mName;

        glm::vec2 mAnchorMin { 0.0f, 0.0f };
        glm::vec2 mAnchorMax { 0.0f, 0.0f };
        glm::vec2 mPivot { 0.0f, 0.0f };
        glm::vec2 mLocalOffset { 0.0f, 0.0f };
        glm::vec2 mSize { 100.0f, 100.0f };
        glm::vec4 mColor = { 1.0f, 1.0f, 1.0f, 1.0f };

        // Cached layout (pixels)
        glm::vec2 mGlobalPos { 0.0f, 0.0f };
        glm::vec2 mGlobalSize { 0.0f, 0.0f };
        float mLayoutScale = 1.0f;

        Widget* mParent = nullptr;
        Vector<Ref<Widget>> mChildren;

        bool mVisible = true;
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

        // textureAsset keeps the owning Texture2D alive for as long as the widget references its RHI image
        void SetTexture(ImageHandle textureId, Ref<Asset> textureAsset = nullptr) { mTextureID = textureId; mTextureAsset = textureAsset; }
        ImageHandle GetTexture() const { return mTextureID; }

        virtual WidgetType GetType() const override { return WidgetType::IMAGE; }
        virtual void GenerateDrawCommands(DrawList& drawList) override
        {
            glm::mat4 transform = glm::mat4(1.0f);
            const float centerX = mGlobalPos.x + (mGlobalSize.x * 0.5f);
            const float centerY = mGlobalPos.y + (mGlobalSize.y * 0.5f);
            transform = glm::translate(transform, glm::vec3(centerX, centerY, 0.0f));
            transform = glm::scale(transform, glm::vec3(mGlobalSize.x, mGlobalSize.y, 1.0f));

            QuadSubmitCmd cmd;
            cmd.Transform = transform;
            cmd.Color = mColor;
            cmd.Texture = mTextureID;
            cmd.Billboard = false;
            drawList.AddSprite(cmd);

            GenerateChildDrawCommands(drawList);
        }
    public:
        ImageHandle mTextureID;
        Ref<Asset> mTextureAsset;
    };

    /*
    * Text is laid out inside its rect: TextAlignment picks left/center/right edge, TextVerticalAlignment the top/center/bottom edge.
    * With the default size of (0, 0) the rect collapses to a point, so the text is aligned around its anchor point (the original behaviour),
    * a non zero width enables word wrapping at that width (WordWrap)
    */
    class Text final : public Widget
    {
    public:
        Text(const String& text, AssetID fontAsset);
        Text(const String& text, Ref<Font> font);

        virtual WidgetType GetType() const override { return WidgetType::TEXT; }
        virtual void GenerateDrawCommands(DrawList& drawList) override;

        void SetText(const String& text) { mText = text; }
        void SetFont(Ref<Font> font) { mFontAsset = font; }
        void SetFontSize(float size) { mFontSize = size; }
        void SetTextAlignment(TextAlignment alignment) { mTextAlignment = alignment; }
        void SetTextVAlignment(TextVerticalAlignment alignment) { mTextVerticalAlignment = alignment; }
        void SetWordWrap(bool wrap) { mWordWrap = wrap; }

        Ref<Font> GetFontAsset() const { return mFontAsset; }
        String& GetTextBuffer() { return mText; }
        float GetFontSize() const { return mFontSize; }
        TextAlignment GetTextAlignment() const { return mTextAlignment; }
        TextVerticalAlignment GetTextVAlignment() const { return mTextVerticalAlignment; }
        bool GetWordWrap() const { return mWordWrap; }

    protected:
        String mText;
        TextAlignment mTextAlignment = TextAlignment::LEFT;
        TextVerticalAlignment mTextVerticalAlignment = TextVerticalAlignment::CENTER;
        float mFontSize = 18.0f;
        bool mWordWrap = true;
        Ref<Font> mFontAsset;
    };

    class Button final : public Image
    {
    public:
        // Pass the background texture, the string, and the font
        Button(const String& text, AssetID fontAsset, ImageHandle textureId = ImageHandle::Invalid())
            : Image(textureId)
        {
            Init(Ref<Text>::Create(text, fontAsset));
        }

        Button(const String& text, Ref<Font> font, ImageHandle textureId = ImageHandle::Invalid())
            : Image(textureId)
        {
            Init(Ref<Text>::Create(text, font));
        }

        void Destroy() override
        {
            if(mTextWidget)
            {
                mTextWidget->Destroy();
                mTextWidget = nullptr;
            }
            UI::Widget::Destroy(); // Call base class destroy to clear mChildren
        }

        Ref<Text> GetTextWidget() { return mTextWidget; }
        void SetText(const String& text) { if(mTextWidget) mTextWidget->SetText(text); }

        // The label is a zero sized text anchored inside the button, so it never wraps and stays aligned when the button is resized
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
        virtual WidgetType GetType() const override { return WidgetType::BUTTON; }
        virtual void GenerateDrawCommands(DrawList& drawList) override
        {
            if(mIsPressed)      mColor = PressedColor;
            else if(mIsHovered) mColor = HoverColor;
            else                mColor = NormalColor;

            // Calls GenerateDrawCommands on mTextWidget(child) recursively so it draws on top!
            Image::GenerateDrawCommands(drawList);
        }

    public:
        glm::vec4 NormalColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 HoverColor = { 0.8f, 0.8f, 0.8f, 1.0f };
        glm::vec4 PressedColor = { 0.5f, 0.5f, 0.5f, 1.0f };

    private:
        void Init(Ref<Text> textWidget)
        {
            mIsInteractable = true;
            SetSize(200.0f, 50.0f);

            mTextWidget = textWidget;
            mTextWidget->SetFontSize(18.0f);
            mTextWidget->SetSize(0.0f, 0.0f);
            AddChild(mTextWidget);

            SetTextAlignment(TextAlignment::CENTER);
            SetTextVAlignment(TextVerticalAlignment::CENTER);
        }

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

        virtual WidgetType GetType() const  override{ return WidgetType::IMAGE_BUTTON; }
        virtual void GenerateDrawCommands(DrawList& drawList) override
        {
            if(mIsPressed)      mColor = PressedColor;
            else if(mIsHovered) mColor = HoverColor;
            else                mColor = NormalColor;

            Image::GenerateDrawCommands(drawList);
        }

    public:
        glm::vec4 NormalColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 HoverColor = { 0.8f, 0.8f, 0.8f, 1.0f };
        glm::vec4 PressedColor = { 0.5f, 0.5f, 0.5f, 1.0f };
    };

}
