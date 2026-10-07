// Copyright (c) - SurgeTechnologies - All rights reserved
#include "UILayout.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Graphics/HighLevel/Font.hpp"
#include "Surge/Graphics/HighLevel/Texture2D.hpp"

namespace Surge::UI
{
    WidgetDesc* WidgetDesc::Find(UUID id)
    {
        if(ID == id)
            return this;

        for(WidgetDesc& child : Children)
        {
            if(WidgetDesc* found = child.Find(id))
                return found;
        }
        return nullptr;
    }

    WidgetDesc* WidgetDesc::FindParentOf(UUID id)
    {
        for(WidgetDesc& child : Children)
        {
            if(child.ID == id)
                return this;
            if(WidgetDesc* found = child.FindParentOf(id))
                return found;
        }
        return nullptr;
    }

    WidgetDesc WidgetDesc::CreateRoot()
    {
        WidgetDesc root;
        root.Name = "Canvas";
        root.Type = WidgetType::BASE_WIDGET;
        root.AnchorMin = { 0.0f, 0.0f };
        root.AnchorMax = { 1.0f, 1.0f };
        root.Pivot = { 0.0f, 0.0f };
        root.Size = { 0.0f, 0.0f };
        return root;
    }

    WidgetDesc WidgetDesc::CreateDefault(WidgetType type, const String& name, AssetID defaultFont)
    {
        WidgetDesc desc;
        desc.Name = name;
        desc.Type = type;

        switch(type)
        {
            case WidgetType::BASE_WIDGET:
                desc.Size = { 400.0f, 300.0f };
                break;
            case WidgetType::IMAGE:
                desc.Size = { 256.0f, 256.0f };
                break;
            case WidgetType::TEXT:
                desc.Size = { 400.0f, 80.0f };
                desc.Text = "New Text";
                desc.Font = defaultFont;
                desc.FontSize = 36.0f;
                break;
            case WidgetType::BUTTON:
                desc.Size = { 300.0f, 80.0f };
                desc.Text = "Button";
                desc.Font = defaultFont;
                desc.FontSize = 32.0f;
                desc.NormalColor = { 0.20f, 0.20f, 0.23f, 1.0f };
                desc.HoverColor = { 0.30f, 0.30f, 0.35f, 1.0f };
                desc.PressedColor = { 0.12f, 0.12f, 0.14f, 1.0f };
                break;
            case WidgetType::IMAGE_BUTTON:
                desc.Size = { 160.0f, 160.0f };
                desc.NormalColor = { 1.0f, 1.0f, 1.0f, 1.0f };
                desc.HoverColor = { 0.85f, 0.85f, 0.85f, 1.0f };
                desc.PressedColor = { 0.65f, 0.65f, 0.65f, 1.0f };
                break;
        }
        return desc;
    }

    const char* WidgetTypeToString(WidgetType type)
    {
        switch(type)
        {
            case WidgetType::BASE_WIDGET:  return "PANEL";
            case WidgetType::IMAGE:        return "IMAGE";
            case WidgetType::TEXT:         return "TEXT";
            case WidgetType::BUTTON:       return "BUTTON";
            case WidgetType::IMAGE_BUTTON: return "IMAGE_BUTTON";
        }
        return "PANEL";
    }

    const char* WidgetTypeToDisplayName(WidgetType type)
    {
        switch(type)
        {
            case WidgetType::BASE_WIDGET:  return "Panel";
            case WidgetType::IMAGE:        return "Image";
            case WidgetType::TEXT:         return "Text";
            case WidgetType::BUTTON:       return "Button";
            case WidgetType::IMAGE_BUTTON: return "Image Button";
        }
        return "Panel";
    }

    WidgetType WidgetTypeFromString(const String& str)
    {
        if(str == "IMAGE")        return WidgetType::IMAGE;
        if(str == "TEXT")         return WidgetType::TEXT;
        if(str == "BUTTON")       return WidgetType::BUTTON;
        if(str == "IMAGE_BUTTON") return WidgetType::IMAGE_BUTTON;
        return WidgetType::BASE_WIDGET;
    }

    Ref<Font> LayoutResourceCache::GetFont(AssetID id)
    {
        if(!id)
            return nullptr;

        auto it = mAssets.find(id.Get());
        if(it != mAssets.end())
            return it->second ? it->second.As<Font>() : nullptr;

        AssetManager* am = Core::GetAssetManager();
        Ref<Font> font = (am->GetMetadata(id).Type == AssetType::FONT) ? am->Load<Font>(id) : nullptr;
        mAssets[id.Get()] = font; // Cache misses too, avoids retrying a broken asset every rebuild
        return font;
    }

    Ref<Asset> LayoutResourceCache::GetTexture(AssetID id, ImageHandle& outImage)
    {
        outImage = ImageHandle::Invalid();
        if(!id)
            return nullptr;

        Ref<Asset> asset;
        auto it = mAssets.find(id.Get());
        if(it != mAssets.end())
            asset = it->second;
        else
        {
            AssetManager* am = Core::GetAssetManager();
            if(am->GetMetadata(id).Type == AssetType::TEXTURE2D)
                asset = am->Load<Texture2D>(id);
            mAssets[id.Get()] = asset;
        }

        if(asset)
            outImage = asset.As<Texture2D>()->GetRHIImage();
        return asset;
    }

    Ref<Widget> Instantiate(const WidgetDesc& desc, LayoutResourceCache& cache, std::unordered_map<uint64_t, Widget*>* outWidgetMap)
    {
        Ref<Widget> widget;
        switch(desc.Type)
        {
            case WidgetType::BASE_WIDGET:
                widget = Ref<Widget>::Create();
                break;

            case WidgetType::IMAGE:
            {
                Ref<Image> image = Ref<Image>::Create();
                ImageHandle handle;
                Ref<Asset> texture = cache.GetTexture(desc.Texture, handle);
                image->SetTexture(handle, texture);
                widget = image;
                break;
            }

            case WidgetType::TEXT:
            {
                Ref<Text> text = Ref<Text>::Create(desc.Text, cache.GetFont(desc.Font));
                text->SetFontSize(desc.FontSize);
                text->SetTextAlignment(desc.HAlign);
                text->SetTextVAlignment(desc.VAlign);
                text->SetWordWrap(desc.WordWrap);
                widget = text;
                break;
            }

            case WidgetType::BUTTON:
            {
                ImageHandle handle;
                Ref<Asset> texture = cache.GetTexture(desc.Texture, handle);
                Ref<Button> button = Ref<Button>::Create(desc.Text, cache.GetFont(desc.Font), handle);
                button->SetTexture(handle, texture);
                button->NormalColor = desc.NormalColor;
                button->HoverColor = desc.HoverColor;
                button->PressedColor = desc.PressedColor;
                button->SetTextAlignment(desc.HAlign, desc.TextPadding);
                button->SetTextVAlignment(desc.VAlign, desc.TextPadding);
                button->GetTextWidget()->SetFontSize(desc.FontSize);
                button->GetTextWidget()->SetColor(desc.TextColor);
                widget = button;
                break;
            }

            case WidgetType::IMAGE_BUTTON:
            {
                ImageHandle handle;
                Ref<Asset> texture = cache.GetTexture(desc.Texture, handle);
                Ref<ImageButton> button = Ref<ImageButton>::Create(handle);
                button->SetTexture(handle, texture);
                button->NormalColor = desc.NormalColor;
                button->HoverColor = desc.HoverColor;
                button->PressedColor = desc.PressedColor;
                widget = button;
                break;
            }
        }

        widget->SetName(desc.Name);
        widget->SetVisible(desc.Visible);
        widget->SetAnchorMin(desc.AnchorMin.x, desc.AnchorMin.y);
        widget->SetAnchorMax(desc.AnchorMax.x, desc.AnchorMax.y);
        widget->SetPivot(desc.Pivot.x, desc.Pivot.y);
        widget->SetOffset(desc.Offset.x, desc.Offset.y);
        widget->SetSize(desc.Size.x, desc.Size.y);
        widget->SetColor(desc.Color);

        if(outWidgetMap)
            (*outWidgetMap)[desc.ID.Get()] = widget.get();

        for(const WidgetDesc& child : desc.Children)
            widget->AddChild(Instantiate(child, cache, outWidgetMap));

        return widget;
    }
}

namespace Surge
{
    Ref<UI::Widget> UILayout::Instantiate() const
    {
        UI::LayoutResourceCache cache;
        return UI::Instantiate(Root, cache);
    }
}
