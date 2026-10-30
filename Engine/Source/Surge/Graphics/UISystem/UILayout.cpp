// Copyright (c) - SurgeTechnologies - All rights reserved
#include "UILayout.hpp"
#include "UIScrollView.hpp"
#include "UIPopups.hpp"
#include "UIInputField.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Graphics/HighLevel/Font.hpp"
#include "Surge/Graphics/HighLevel/Texture2D.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"

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

    static void MakeStretched(WidgetDesc& desc)
    {
        desc.AnchorMin = { 0.0f, 0.0f };
        desc.AnchorMax = { 1.0f, 1.0f };
        desc.Pivot = { 0.0f, 0.0f };
        desc.Offset = { 0.0f, 0.0f };
        desc.Size = { 0.0f, 0.0f };
    }

    static ImagePartDesc MakePart(const glm::vec4& color, float cornerRadius)
    {
        ImagePartDesc part;
        part.Color = color;
        part.CornerRadius = cornerRadius;
        return part;
    }

    WidgetDesc WidgetDesc::CreateDefault(WidgetType type, const String& name, AssetID defaultFont)
    {
        constexpr glm::vec4 accent = { 0.25f, 0.6f, 1.0f, 1.0f };
        constexpr glm::vec4 surface = { 0.2f, 0.21f, 0.25f, 1.0f };
        constexpr float round = 10000.0f; // Clamped to half the size in the shader: pill/circle

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
                desc.Background.CornerRadius = 12.0f;
                break;
            case WidgetType::IMAGE_BUTTON:
                desc.Size = { 160.0f, 160.0f };
                desc.NormalColor = { 1.0f, 1.0f, 1.0f, 1.0f };
                desc.HoverColor = { 0.85f, 0.85f, 0.85f, 1.0f };
                desc.PressedColor = { 0.65f, 0.65f, 0.65f, 1.0f };
                break;
            case WidgetType::PROGRESS_BAR:
                desc.Size = { 400.0f, 40.0f };
                desc.Background = MakePart({ 0.15f, 0.16f, 0.19f, 1.0f }, round);
                desc.Fill = MakePart({ 0.25f, 0.75f, 0.4f, 1.0f }, round);
                desc.Value = 0.6f;
                desc.FillPadding = 4.0f;
                break;
            case WidgetType::TOGGLE:
                desc.Size = { 320.0f, 60.0f };
                desc.Text = "Toggle";
                desc.Font = defaultFont;
                desc.FontSize = 30.0f;
                desc.Background = MakePart({ 0.22f, 0.23f, 0.27f, 1.0f }, 10.0f);
                desc.Fill = MakePart(accent, 5.0f);
                desc.Handle = MakePart({ 1.0f, 1.0f, 1.0f, 1.0f }, round);
                desc.IsOn = true;
                break;
            case WidgetType::SLIDER:
                desc.Size = { 400.0f, 50.0f };
                desc.Background = MakePart({ 0.22f, 0.23f, 0.27f, 1.0f }, round);
                desc.Fill = MakePart(accent, round);
                desc.Handle = MakePart({ 1.0f, 1.0f, 1.0f, 1.0f }, round);
                desc.HandleSize = 40.0f;
                desc.TrackThickness = 12.0f;
                break;
            case WidgetType::JOYSTICK:
                desc.Size = { 300.0f, 300.0f };
                desc.Background = MakePart({ 1.0f, 1.0f, 1.0f, 0.25f }, round);
                desc.Handle = MakePart({ 1.0f, 1.0f, 1.0f, 0.85f }, round);
                desc.HandleSize = 0.0f;
                break;
            case WidgetType::SCROLL_VIEW:
            {
                desc.Size = { 600.0f, 800.0f };
                WidgetDesc content = CreateDefault(WidgetType::LAYOUT_GROUP, "Content", defaultFont);
                content.AnchorMin = { 0.0f, 0.0f };
                content.AnchorMax = { 1.0f, 0.0f };
                content.Pivot = { 0.0f, 0.0f };
                content.Size = { 0.0f, 0.0f };
                content.ControlChildWidth = true;
                content.FitContent = true;
                desc.Children.push_back(std::move(content));
                break;
            }
            case WidgetType::LAYOUT_GROUP:
                desc.Size = { 400.0f, 300.0f };
                desc.Padding = { 10.0f, 10.0f, 10.0f, 10.0f };
                desc.Spacing = { 10.0f, 10.0f };
                break;
            case WidgetType::INPUT_FIELD:
                desc.Size = { 500.0f, 80.0f };
                desc.Placeholder = "Enter text...";
                desc.Font = defaultFont;
                desc.FontSize = 32.0f;
                desc.TextPadding = 16.0f;
                desc.Background = MakePart({ 0.1f, 0.11f, 0.13f, 1.0f }, 12.0f);
                desc.Background.BorderWidth = 2.0f;
                desc.Background.BorderColor = { 1.0f, 1.0f, 1.0f, 0.15f };
                desc.HighlightColor = accent;
                break;
            case WidgetType::DROPDOWN:
                desc.Size = { 400.0f, 80.0f };
                desc.Options = { "Option A", "Option B", "Option C" };
                desc.SelectedIndex = 0;
                desc.Placeholder = "Select...";
                desc.Font = defaultFont;
                desc.FontSize = 30.0f;
                desc.TextPadding = 20.0f;
                desc.Background = MakePart(surface, 12.0f);
                desc.Fill = MakePart({ 0.16f, 0.17f, 0.2f, 1.0f }, 12.0f);
                desc.HoverColor = { 1.0f, 1.0f, 1.0f, 0.08f };
                desc.HighlightColor = { 0.25f, 0.6f, 1.0f, 0.35f };
                break;
            case WidgetType::MODAL:
                desc.Size = { 700.0f, 500.0f };
                desc.Background = MakePart({ 0.14f, 0.15f, 0.18f, 1.0f }, 24.0f);
                break;
            case WidgetType::TAB_VIEW:
            {
                desc.Size = { 800.0f, 600.0f };
                desc.Font = defaultFont;
                desc.FontSize = 30.0f;
                desc.Background = MakePart({ 0.12f, 0.13f, 0.16f, 1.0f }, 0.0f);
                desc.Fill = MakePart(accent, 14.0f);
                desc.TextColor = { 0.75f, 0.77f, 0.82f, 1.0f };
                desc.HighlightColor = { 1.0f, 1.0f, 1.0f, 1.0f };
                for(const char* page : { "Tab 1", "Tab 2" })
                {
                    WidgetDesc child = CreateDefault(WidgetType::BASE_WIDGET, page, defaultFont);
                    MakeStretched(child);
                    desc.Children.push_back(std::move(child));
                }
                break;
            }
            case WidgetType::LIST_VIEW:
            {
                desc.Size = { 600.0f, 800.0f };
                desc.Padding = { 10.0f, 10.0f, 10.0f, 10.0f };
                desc.Spacing = { 10.0f, 10.0f };
                desc.Columns = 1;
                WidgetDesc item = CreateDefault(WidgetType::BUTTON, "Item", defaultFont);
                item.Size = { 0.0f, 100.0f };
                desc.Children.push_back(std::move(item));
                break;
            }
            case WidgetType::SPINNER:
                desc.Size = { 64.0f, 64.0f };
                break;
            case WidgetType::SAFE_AREA:
                MakeStretched(desc);
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
            case WidgetType::PROGRESS_BAR: return "PROGRESS_BAR";
            case WidgetType::TOGGLE:       return "TOGGLE";
            case WidgetType::SLIDER:       return "SLIDER";
            case WidgetType::JOYSTICK:     return "JOYSTICK";
            case WidgetType::SCROLL_VIEW:  return "SCROLL_VIEW";
            case WidgetType::LAYOUT_GROUP: return "LAYOUT_GROUP";
            case WidgetType::INPUT_FIELD:  return "INPUT_FIELD";
            case WidgetType::DROPDOWN:     return "DROPDOWN";
            case WidgetType::MODAL:        return "MODAL";
            case WidgetType::TAB_VIEW:     return "TAB_VIEW";
            case WidgetType::LIST_VIEW:    return "LIST_VIEW";
            case WidgetType::SPINNER:      return "SPINNER";
            case WidgetType::SAFE_AREA:    return "SAFE_AREA";
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
            case WidgetType::PROGRESS_BAR: return "Progress Bar";
            case WidgetType::TOGGLE:       return "Toggle";
            case WidgetType::SLIDER:       return "Slider";
            case WidgetType::JOYSTICK:     return "Joystick";
            case WidgetType::SCROLL_VIEW:  return "Scroll View";
            case WidgetType::LAYOUT_GROUP: return "Layout Group";
            case WidgetType::INPUT_FIELD:  return "Input Field";
            case WidgetType::DROPDOWN:     return "Dropdown";
            case WidgetType::MODAL:        return "Modal";
            case WidgetType::TAB_VIEW:     return "Tab View";
            case WidgetType::LIST_VIEW:    return "List View";
            case WidgetType::SPINNER:      return "Spinner";
            case WidgetType::SAFE_AREA:    return "Safe Area";
        }
        return "Panel";
    }

    WidgetType WidgetTypeFromString(const String& str)
    {
        constexpr WidgetType types[] = { WidgetType::IMAGE, WidgetType::TEXT, WidgetType::BUTTON, WidgetType::IMAGE_BUTTON, WidgetType::PROGRESS_BAR, WidgetType::TOGGLE,
                                         WidgetType::SLIDER, WidgetType::JOYSTICK, WidgetType::SCROLL_VIEW, WidgetType::LAYOUT_GROUP, WidgetType::INPUT_FIELD,
                                         WidgetType::DROPDOWN, WidgetType::MODAL, WidgetType::TAB_VIEW, WidgetType::LIST_VIEW, WidgetType::SPINNER, WidgetType::SAFE_AREA };
        for(WidgetType type : types)
        {
            if(str == WidgetTypeToString(type))
                return type;
        }
        return WidgetType::BASE_WIDGET;
    }

    bool IsContainerType(WidgetType type)
    {
        switch(type)
        {
            case WidgetType::BASE_WIDGET:
            case WidgetType::IMAGE:
            case WidgetType::SCROLL_VIEW:
            case WidgetType::LAYOUT_GROUP:
            case WidgetType::MODAL:
            case WidgetType::TAB_VIEW:
            case WidgetType::LIST_VIEW:
            case WidgetType::SAFE_AREA:
                return true;
            default:
                return false;
        }
    }

    Ref<Font> LayoutResourceCache::GetFont(AssetID id)
    {
        if(!id)
            return Core::GetRenderer()->GetEngineFont();

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

    static void ApplyPart(const ImagePartDesc& part, ImageStyle& style, LayoutResourceCache& cache)
    {
        ImageHandle image;
        Ref<Asset> texture = cache.GetTexture(part.Texture, image);
        style.SetTexture(image, texture);
        style.Mode = part.Mode;
        style.SliceBorder = part.SliceBorder;
        style.SliceScale = part.SliceScale;
        style.FillCenter = part.FillCenter;
        style.CornerRadius = part.CornerRadius;
        style.BorderWidth = part.BorderWidth;
        style.BorderColor = part.BorderColor;
    }

    Ref<Widget> Instantiate(const WidgetDesc& desc, LayoutResourceCache& cache, std::unordered_map<uint64_t, Widget*>* outWidgetMap, bool designPreview)
    {
        Ref<Widget> widget;
        bool instantiateChildren = true;
        switch(desc.Type)
        {
            case WidgetType::BASE_WIDGET:
                widget = Ref<Widget>::Create();
                break;

            case WidgetType::IMAGE:
            {
                Ref<Image> image = Ref<Image>::Create();
                ImageStyle& style = image->GetStyle();
                ApplyPart(desc.Background, style, cache);
                style.FillAmount = desc.FillAmount;
                style.Origin = desc.Origin;
                style.PreserveAspect = desc.PreserveAspect;
                widget = image;
                break;
            }

            case WidgetType::TEXT:
            {
                Ref<Text> text = Ref<Text>::Create(desc.Text, cache.GetFont(desc.Font));
                text->SetFontSize(desc.FontSize);
                text->SetTextAlignment(desc.HAlign);
                text->SetTextVAlignment(desc.VAlign);
                text->SetOverflow(desc.Overflow);
                widget = text;
                break;
            }

            case WidgetType::BUTTON:
            {
                Ref<Button> button = Ref<Button>::Create(desc.Text, cache.GetFont(desc.Font));
                ApplyPart(desc.Background, button->GetStyle(), cache);
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
                Ref<ImageButton> button = Ref<ImageButton>::Create();
                ApplyPart(desc.Background, button->GetStyle(), cache);
                button->GetStyle().PreserveAspect = desc.PreserveAspect;
                button->NormalColor = desc.NormalColor;
                button->HoverColor = desc.HoverColor;
                button->PressedColor = desc.PressedColor;
                widget = button;
                break;
            }

            case WidgetType::PROGRESS_BAR:
            {
                Ref<ProgressBar> bar = Ref<ProgressBar>::Create();
                ApplyPart(desc.Background, bar->Background, cache);
                ApplyPart(desc.Fill, bar->Fill, cache);
                bar->BackgroundColor = desc.Background.Color;
                bar->FillColor = desc.Fill.Color;
                bar->Min = desc.MinValue;
                bar->Max = desc.MaxValue;
                bar->Direction = desc.Origin;
                bar->FillPadding = desc.FillPadding;
                bar->SetValue(desc.Value);
                widget = bar;
                break;
            }

            case WidgetType::TOGGLE:
            {
                Ref<Toggle> toggle = Ref<Toggle>::Create(desc.Text, cache.GetFont(desc.Font));
                ApplyPart(desc.Background, toggle->Box, cache);
                ApplyPart(desc.Fill, toggle->Check, cache);
                ApplyPart(desc.Handle, toggle->Knob, cache);
                toggle->BoxColor = desc.Background.Color;
                toggle->CheckColor = desc.Fill.Color;
                toggle->KnobColor = desc.Handle.Color;
                toggle->Style = desc.ToggleType;
                toggle->Group = desc.Group;
                toggle->AllowSwitchOff = desc.AllowSwitchOff;
                toggle->GetLabel()->SetFontSize(desc.FontSize);
                toggle->GetLabel()->SetColor(desc.TextColor);
                toggle->SetOn(desc.IsOn, false);
                widget = toggle;
                break;
            }

            case WidgetType::SLIDER:
            {
                Ref<Slider> slider = Ref<Slider>::Create();
                ApplyPart(desc.Background, slider->Track, cache);
                ApplyPart(desc.Fill, slider->Fill, cache);
                ApplyPart(desc.Handle, slider->Handle, cache);
                slider->TrackColor = desc.Background.Color;
                slider->FillColor = desc.Fill.Color;
                slider->HandleColor = desc.Handle.Color;
                slider->Min = desc.MinValue;
                slider->Max = desc.MaxValue;
                slider->WholeNumbers = desc.WholeNumbers;
                slider->Direction = desc.Origin;
                slider->HandleSize = desc.HandleSize;
                slider->TrackThickness = desc.TrackThickness;
                slider->SetValue(desc.Value, false);
                widget = slider;
                break;
            }

            case WidgetType::JOYSTICK:
            {
                Ref<Joystick> stick = Ref<Joystick>::Create();
                ApplyPart(desc.Background, stick->Base, cache);
                ApplyPart(desc.Handle, stick->Knob, cache);
                stick->BaseColor = desc.Background.Color;
                stick->KnobColor = desc.Handle.Color;
                stick->Mode = desc.StickMode;
                stick->BaseSize = desc.BaseSize;
                stick->HandleSize = desc.HandleSize;
                stick->DeadZone = desc.DeadZone;
                stick->HandleRange = desc.HandleRange;
                stick->HideWhenIdle = desc.HideWhenIdle && !designPreview;
                widget = stick;
                break;
            }

            case WidgetType::SCROLL_VIEW:
            case WidgetType::LIST_VIEW:
            {
                Ref<ScrollView> scroll;
                if(desc.Type == WidgetType::LIST_VIEW)
                {
                    Ref<ListView> list = Ref<ListView>::Create();
                    list->ItemSize = desc.ItemSize;
                    list->Spacing = desc.Spacing;
                    list->Padding = desc.Padding;
                    list->Columns = glm::max(desc.Columns, 1);
                    list->SetDesignPreview(designPreview);
                    if(!desc.Children.empty())
                    {
                        list->SetItemTemplate(desc.Children[0]);
                        if(designPreview)
                        {
                            // Placeholder items: the first one maps to the template so it can be selected/resized in the viewport
                            list->AddChild(Instantiate(desc.Children[0], cache, outWidgetMap, true));
                            for(int i = 1; i < glm::max(desc.PreviewItemCount, 1); i++)
                                list->AddChild(Instantiate(desc.Children[0], cache, nullptr, true));
                        }
                    }
                    instantiateChildren = false; // Items are created by the list
                    scroll = list;
                }
                else
                    scroll = Ref<ScrollView>::Create();

                scroll->Horizontal = desc.ScrollHorizontal;
                scroll->Vertical = desc.ScrollVertical;
                scroll->DecelerationRate = desc.DecelerationRate;
                scroll->ShowScrollbars = desc.ShowScrollbars;
                scroll->ScrollbarColor = desc.ScrollbarColor;
                widget = scroll;
                break;
            }

            case WidgetType::LAYOUT_GROUP:
            {
                Ref<LayoutGroup> group = Ref<LayoutGroup>::Create();
                group->Direction = desc.Direction;
                group->Padding = desc.Padding;
                group->Spacing = desc.Spacing;
                group->CellSize = desc.CellSize;
                group->Columns = desc.Columns;
                group->ChildAlignment = desc.ChildAlignment;
                group->ControlChildWidth = desc.ControlChildWidth;
                group->ControlChildHeight = desc.ControlChildHeight;
                group->FitContent = desc.FitContent;
                widget = group;
                break;
            }

            case WidgetType::INPUT_FIELD:
            {
                Ref<InputField> field = Ref<InputField>::Create(cache.GetFont(desc.Font));
                ApplyPart(desc.Background, field->Background, cache);
                field->BackgroundColor = desc.Background.Color;
                field->FocusedBorderColor = desc.HighlightColor;
                field->Placeholder = desc.Placeholder;
                field->ContentType = desc.ContentType;
                field->MultiLine = desc.MultiLine;
                field->MaxLength = desc.MaxLength;
                field->FontSize = desc.FontSize;
                field->TextPadding = desc.TextPadding;
                field->TextColor = desc.TextColor;
                field->PlaceholderColor = desc.PlaceholderColor;
                field->CaretColor = desc.CaretColor;
                field->SelectionColor = desc.SelectionColor;
                field->SetText(desc.Text);
                widget = field;
                break;
            }

            case WidgetType::DROPDOWN:
            {
                Ref<Dropdown> dropdown = Ref<Dropdown>::Create(cache.GetFont(desc.Font));
                ApplyPart(desc.Background, dropdown->Background, cache);
                ApplyPart(desc.Fill, dropdown->PopupStyle, cache);
                dropdown->BackgroundColor = desc.Background.Color;
                dropdown->PopupColor = desc.Fill.Color;
                dropdown->TextColor = desc.TextColor;
                dropdown->ItemHoverColor = desc.HoverColor;
                dropdown->SelectedItemColor = desc.HighlightColor;
                dropdown->FontSize = desc.FontSize;
                dropdown->TextPadding = desc.TextPadding;
                dropdown->MaxVisibleItems = desc.MaxVisibleItems;
                dropdown->Placeholder = desc.Placeholder;
                dropdown->SetOptions(desc.Options);
                dropdown->SetSelectedIndex(desc.SelectedIndex, false);
                widget = dropdown;
                break;
            }

            case WidgetType::MODAL:
            {
                Ref<Modal> modal = Ref<Modal>::Create();
                ApplyPart(desc.Background, modal->Panel, cache);
                modal->PanelColor = desc.Background.Color;
                modal->BackdropColor = desc.BackdropColor;
                modal->CloseOnBackdrop = desc.CloseOnBackdrop;
                modal->CloseOnBack = desc.CloseOnBack;
                modal->SetDesignPreview(designPreview);
                widget = modal;
                break;
            }

            case WidgetType::TAB_VIEW:
            {
                Ref<TabView> tabs = Ref<TabView>::Create();
                ApplyPart(desc.Background, tabs->Bar, cache);
                ApplyPart(desc.Fill, tabs->ActiveTabStyle, cache);
                tabs->BarColor = desc.Background.Color;
                tabs->ActiveTabColor = desc.Fill.Color;
                tabs->TextColor = desc.TextColor;
                tabs->ActiveTextColor = desc.HighlightColor;
                tabs->TextFont = cache.GetFont(desc.Font);
                tabs->FontSize = desc.FontSize;
                tabs->TabsAtBottom = desc.TabsAtBottom;
                tabs->TabBarHeight = desc.TabBarHeight;
                widget = tabs;
                break;
            }

            case WidgetType::SPINNER:
            {
                Ref<Spinner> spinner = Ref<Spinner>::Create();
                spinner->DotCount = desc.DotCount;
                spinner->DotSize = desc.DotSize;
                spinner->Speed = desc.Speed;
                widget = spinner;
                break;
            }

            case WidgetType::SAFE_AREA:
            {
                Ref<SafeArea> safeArea = Ref<SafeArea>::Create();
                safeArea->Edges = desc.SafeEdges;
                widget = safeArea;
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

        if(instantiateChildren)
        {
            for(const WidgetDesc& child : desc.Children)
                widget->AddChild(Instantiate(child, cache, outWidgetMap, designPreview));
        }

        // Pages exist now, the active one can be picked
        if(desc.Type == WidgetType::TAB_VIEW)
            static_cast<TabView*>(widget.get())->SetActiveTab(desc.ActiveTab, false);

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
