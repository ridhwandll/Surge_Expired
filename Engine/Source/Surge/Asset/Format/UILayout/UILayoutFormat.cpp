// Copyright (c) - SurgeTechnologies - All rights reserved
#include "UILayoutFormat.hpp"
#include "Surge/Asset/Format/BinarySerialization.hpp"
#include "Surge/Graphics/UISystem/UILayout.hpp"

namespace Surge::UI
{
// Release builds of the Windows toolchain (no iterator debugging aka SURGE_DEBUG, so the std containers have their shipping size): a desc struct that
// changes size breaks the build here. Update the field lists below, bump UILayoutBinary::VERSION, then update the size
#if defined(SURGE_PLATFORM_WINDOWS)
#ifndef SURGE_DEBUG
    static_assert(sizeof(WidgetDesc) == 856, "UI::WidgetDesc changed: update the UILayoutBinary field lists, bump VERSION, then this size");
    static_assert(sizeof(ImagePartDesc) == 80, "UI::ImagePartDesc changed: update the UILayoutBinary field lists, bump VERSION, then this size");
    static_assert(sizeof(CanvasScaler) == 12, "UI::CanvasScaler changed: update the UILayoutBinary field lists, bump VERSION, then this size");
#endif
#endif

    SURGE_RAPTOR_FIELDS(ImagePartDesc, v.Texture, v.Color, v.Mode, v.SliceBorder, v.SliceScale, v.FillCenter, v.CornerRadius, v.BorderWidth, v.BorderColor)
    SURGE_RAPTOR_FIELDS(CanvasScaler, v.ReferenceResolution, v.MatchWidthOrHeight)

    // Every field of the flat desc (defaults included) but the children, which UILayoutBinary writes after it (depth checked on read).
    // Fonts/textures stay AssetIDs, resolved when the layout is instantiated
    template <typename Desc, typename Fn>
    static void VisitWidgetFields(Desc& w, Fn&& fields)
    {
        fields(w.ID, w.Name, w.Type, w.Visible);
        fields(w.AnchorMin, w.AnchorMax, w.Pivot, w.Offset, w.Size, w.Color);
        fields(w.Background, w.Fill, w.Handle);
        fields(w.FillAmount, w.Origin, w.PreserveAspect);
        fields(w.Text, w.Font, w.FontSize, w.HAlign, w.VAlign, w.Overflow, w.TextColor, w.TextPadding);
        fields(w.NormalColor, w.HoverColor, w.PressedColor, w.HighlightColor);
        fields(w.MinValue, w.MaxValue, w.Value, w.WholeNumbers, w.FillPadding);
        fields(w.IsOn, w.Group, w.AllowSwitchOff, w.ToggleType);
        fields(w.HandleSize, w.TrackThickness, w.StickMode, w.BaseSize, w.DeadZone, w.HandleRange, w.HideWhenIdle);
        fields(w.ScrollHorizontal, w.ScrollVertical, w.DecelerationRate, w.ShowScrollbars, w.ScrollbarColor);
        fields(w.Direction, w.Padding, w.Spacing, w.CellSize, w.Columns, w.ChildAlignment, w.ControlChildWidth, w.ControlChildHeight, w.FitContent);
        fields(w.ItemSize, w.PreviewItemCount);
        fields(w.Placeholder, w.ContentType, w.MultiLine, w.MaxLength, w.PlaceholderColor, w.CaretColor, w.SelectionColor);
        fields(w.Options, w.SelectedIndex, w.MaxVisibleItems);
        fields(w.BackdropColor, w.CloseOnBackdrop, w.CloseOnBack);
        fields(w.ActiveTab, w.TabsAtBottom, w.TabBarHeight);
        fields(w.DotCount, w.DotSize, w.Speed);
        fields(w.SafeEdges);
    }

    static void Rapt0RSerialize(Rapt0R::Writer& writer, const WidgetDesc& desc)
    {
        VisitWidgetFields(desc, [&writer](const auto&... values) { writer.Write(values...); });
    }

    static void Rapt0RDeserialize(Rapt0R::Reader& reader, WidgetDesc& desc)
    {
        VisitWidgetFields(desc, [&reader](auto&... values) { reader.Read(values...); });
    }
}

namespace Surge::UILayoutBinary
{
    static constexpr Uint MAX_DEPTH = 64;     // Deeper trees are treated as corrupted data

    static void WriteWidget(Rapt0R::Writer& writer, const UI::WidgetDesc& w)
    {
        writer.Write(w, static_cast<Uint>(w.Children.size()));
        for(const UI::WidgetDesc& child : w.Children)
            WriteWidget(writer, child);
    }

    // Every enum a widget saves, against its last enumerator (update it when the enum grows)
    static bool HasValidEnums(const UI::WidgetDesc& w)
    {
        auto ValidPart = [](const UI::ImagePartDesc& part) { return IsEnumInRange(part.Mode, UI::ImageMode::FILLED); };

        return IsEnumInRange(w.Type, UI::WidgetType::SAFE_AREA) && ValidPart(w.Background) && ValidPart(w.Fill) && ValidPart(w.Handle) &&
               IsEnumInRange(w.Origin, UI::FillOrigin::BOTTOM) && IsEnumInRange(w.HAlign, TextAlignment::RIGHT) &&
               IsEnumInRange(w.VAlign, TextVerticalAlignment::BOTTOM) && IsEnumInRange(w.Overflow, UI::TextOverflow::NO_WRAP) &&
               IsEnumInRange(w.ToggleType, UI::ToggleStyle::SWITCH) && IsEnumInRange(w.StickMode, UI::JoystickMode::FLOATING) &&
               IsEnumInRange(w.Direction, UI::LayoutDirection::GRID) && IsEnumInRange(w.ContentType, UI::InputContentType::PASSWORD);
    }

    static void ReadWidget(Rapt0R::Reader& reader, UI::WidgetDesc& w, Uint depth)
    {
        if(depth > MAX_DEPTH)
        {
            reader.SetError(Rapt0R::ErrorCode::CORRUPT_DATA);
            return;
        }

        Uint childCount = 0;
        if(!reader.Read(w, childCount))
            return;
        if(!HasValidEnums(w))
        {
            reader.SetError(Rapt0R::ErrorCode::CORRUPT_DATA);
            return;
        }
        if(!reader.EnsureElements(childCount))
            return;

        w.Children.resize(childCount);
        for(Uint i = 0; i < childCount && !reader.HasError(); i++)
            ReadWidget(reader, w.Children[i], depth + 1);
    }

    void Write(Rapt0R::Writer& writer, const UILayout& layout)
    {
        writer.Write(layout.Scaler);
        WriteWidget(writer, layout.Root);
    }

    bool Read(Rapt0R::Reader& reader, UILayout& outLayout)
    {
        UI::CanvasScaler scaler;
        UI::WidgetDesc root;
        reader.Read(scaler);
        ReadWidget(reader, root, 0);
        if(reader.HasError())
            return false;

        outLayout.Scaler = scaler;
        outLayout.Root = std::move(root);
        return true;
    }
}

namespace Surge
{
    Ref<Asset> UILayoutFormat::Read(CookedFile& file, const AssetMetadata& metadata, [[maybe_unused]] const AssetContext& context) const
    {
        Ref<UILayout> layout = UILayout::Create();
        Rapt0R::Reader payload = file.GetPayload();
        if(!UILayoutBinary::Read(payload, *layout))
        {
            Log<Severity::Error>("[UILayoutFormat] {} is truncated or corrupted!", metadata.RelativePath);
            return nullptr;
        }
        return layout.As<Asset>();
    }

    bool UILayoutFormat::Write(Rapt0R::Writer& payload, const Asset& asset) const
    {
        UILayoutBinary::Write(payload, static_cast<const UILayout&>(asset));
        return !payload.HasError();
    }
}
