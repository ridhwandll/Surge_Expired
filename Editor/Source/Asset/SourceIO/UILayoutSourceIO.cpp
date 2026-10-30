// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Asset/SourceIO/UILayoutSourceIO.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include <json/json.hpp>
#include <array>
#include <algorithm>

namespace Surge
{
    static constexpr int UI_LAYOUT_VERSION = 2; // 2: image parts (Background/Fill/Handle), text overflow, new widget types

    static nlohmann::json Vec2ToJson(const glm::vec2& v) { return nlohmann::json::array({ v.x, v.y }); }
    static nlohmann::json Vec4ToJson(const glm::vec4& v) { return nlohmann::json::array({ v.x, v.y, v.z, v.w }); }

    static glm::vec2 JsonToVec2(const nlohmann::json& j, const char* key, const glm::vec2& fallback)
    {
        if(!j.contains(key) || !j[key].is_array() || j[key].size() != 2)
            return fallback;
        return { j[key][0].get<float>(), j[key][1].get<float>() };
    }

    static glm::vec4 JsonToVec4(const nlohmann::json& j, const char* key, const glm::vec4& fallback)
    {
        if(!j.contains(key) || !j[key].is_array() || j[key].size() != 4)
            return fallback;
        return { j[key][0].get<float>(), j[key][1].get<float>(), j[key][2].get<float>(), j[key][3].get<float>() };
    }

    static const char* HAlignToString(TextAlignment a)
    {
        switch(a)
        {
            case TextAlignment::LEFT:   return "LEFT";
            case TextAlignment::CENTER: return "CENTER";
            case TextAlignment::RIGHT:  return "RIGHT";
        }
        return "CENTER";
    }

    static TextAlignment HAlignFromString(const String& s)
    {
        if(s == "LEFT")  return TextAlignment::LEFT;
        if(s == "RIGHT") return TextAlignment::RIGHT;
        return TextAlignment::CENTER;
    }

    static const char* VAlignToString(TextVerticalAlignment a)
    {
        switch(a)
        {
            case TextVerticalAlignment::TOP:      return "TOP";
            case TextVerticalAlignment::CENTER:   return "CENTER";
            case TextVerticalAlignment::BOTTOM:   return "BOTTOM";
            case TextVerticalAlignment::BASELINE: return "BASELINE";
        }
        return "CENTER";
    }

    static TextVerticalAlignment VAlignFromString(const String& s)
    {
        if(s == "TOP")      return TextVerticalAlignment::TOP;
        if(s == "BOTTOM")   return TextVerticalAlignment::BOTTOM;
        if(s == "BASELINE") return TextVerticalAlignment::BASELINE;
        return TextVerticalAlignment::CENTER;
    }

    template <typename E, size_t N>
    static const char* EnumName(E value, const std::array<const char*, N>& names)
    {
        const size_t index = static_cast<size_t>(value);
        return index < N ? names[index] : names[0];
    }

    template <typename E, size_t N>
    static E EnumFromJson(const nlohmann::json& j, const char* key, E fallback, const std::array<const char*, N>& names)
    {
        if(!j.contains(key) || !j[key].is_string())
            return fallback;

        const String value = j[key].get<String>();
        for(size_t i = 0; i < N; i++)
        {
            if(value == names[i])
                return static_cast<E>(i);
        }
        return fallback;
    }

    // Same order as the enums
    static constexpr std::array<const char*, 3> sImageModeNames = { "SIMPLE", "SLICED", "FILLED" };
    static constexpr std::array<const char*, 4> sFillOriginNames = { "LEFT", "RIGHT", "TOP", "BOTTOM" };
    static constexpr std::array<const char*, 3> sOverflowNames = { "WRAP", "TRUNCATE", "NO_WRAP" };
    static constexpr std::array<const char*, 2> sToggleStyleNames = { "CHECKBOX", "SWITCH" };
    static constexpr std::array<const char*, 2> sJoystickModeNames = { "FIXED", "FLOATING" };
    static constexpr std::array<const char*, 3> sLayoutDirectionNames = { "HORIZONTAL", "VERTICAL", "GRID" };
    static constexpr std::array<const char*, 6> sContentTypeNames = { "STANDARD", "INTEGER", "DECIMAL", "EMAIL", "PHONE", "PASSWORD" };

    static nlohmann::json PartToJson(const UI::ImagePartDesc& part)
    {
        nlohmann::json j;
        j["Texture"] = part.Texture.Get();
        j["Color"] = Vec4ToJson(part.Color);
        j["Mode"] = EnumName(part.Mode, sImageModeNames);
        j["SliceBorder"] = Vec4ToJson(part.SliceBorder);
        j["SliceScale"] = part.SliceScale;
        j["FillCenter"] = part.FillCenter;
        j["CornerRadius"] = part.CornerRadius;
        j["BorderWidth"] = part.BorderWidth;
        j["BorderColor"] = Vec4ToJson(part.BorderColor);
        return j;
    }

    static UI::ImagePartDesc PartFromJson(const nlohmann::json& parent, const char* key)
    {
        UI::ImagePartDesc part;
        if(!parent.contains(key) || !parent[key].is_object())
            return part;

        const nlohmann::json& j = parent[key];
        part.Texture = j.value("Texture", static_cast<uint64_t>(UUID::INVALID));
        part.Color = JsonToVec4(j, "Color", part.Color);
        part.Mode = EnumFromJson(j, "Mode", part.Mode, sImageModeNames);
        part.SliceBorder = JsonToVec4(j, "SliceBorder", part.SliceBorder);
        part.SliceScale = j.value("SliceScale", part.SliceScale);
        part.FillCenter = j.value("FillCenter", part.FillCenter);
        part.CornerRadius = j.value("CornerRadius", part.CornerRadius);
        part.BorderWidth = j.value("BorderWidth", part.BorderWidth);
        part.BorderColor = JsonToVec4(j, "BorderColor", part.BorderColor);
        return part;
    }

    static nlohmann::json WidgetToJson(const UI::WidgetDesc& w)
    {
        using UI::WidgetType;
        auto is = [&](std::initializer_list<WidgetType> types) { return std::find(types.begin(), types.end(), w.Type) != types.end(); };

        nlohmann::json j;
        j["ID"] = w.ID.Get();
        j["Name"] = w.Name;
        j["Type"] = UI::WidgetTypeToString(w.Type);
        j["Visible"] = w.Visible;
        j["AnchorMin"] = Vec2ToJson(w.AnchorMin);
        j["AnchorMax"] = Vec2ToJson(w.AnchorMax);
        j["Pivot"] = Vec2ToJson(w.Pivot);
        j["Offset"] = Vec2ToJson(w.Offset);
        j["Size"] = Vec2ToJson(w.Size);
        j["Color"] = Vec4ToJson(w.Color);

        // Only write what the type uses, keeps the files readable
        if(is({ WidgetType::IMAGE, WidgetType::BUTTON, WidgetType::IMAGE_BUTTON, WidgetType::PROGRESS_BAR, WidgetType::TOGGLE, WidgetType::SLIDER, WidgetType::JOYSTICK,
                WidgetType::INPUT_FIELD, WidgetType::DROPDOWN, WidgetType::MODAL, WidgetType::TAB_VIEW }))
            j["Background"] = PartToJson(w.Background);
        if(is({ WidgetType::PROGRESS_BAR, WidgetType::TOGGLE, WidgetType::SLIDER, WidgetType::DROPDOWN, WidgetType::TAB_VIEW }))
            j["Fill"] = PartToJson(w.Fill);
        if(is({ WidgetType::TOGGLE, WidgetType::SLIDER, WidgetType::JOYSTICK }))
            j["Handle"] = PartToJson(w.Handle);

        if(is({ WidgetType::IMAGE, WidgetType::IMAGE_BUTTON }))
            j["PreserveAspect"] = w.PreserveAspect;
        if(is({ WidgetType::IMAGE, WidgetType::PROGRESS_BAR, WidgetType::SLIDER }))
            j["Origin"] = EnumName(w.Origin, sFillOriginNames);
        if(w.Type == WidgetType::IMAGE)
            j["FillAmount"] = w.FillAmount;

        if(is({ WidgetType::TEXT, WidgetType::BUTTON, WidgetType::TOGGLE, WidgetType::INPUT_FIELD, WidgetType::DROPDOWN, WidgetType::TAB_VIEW }))
        {
            j["Text"] = w.Text;
            j["Font"] = w.Font.Get();
            j["FontSize"] = w.FontSize;
            j["TextColor"] = Vec4ToJson(w.TextColor);
            j["TextPadding"] = w.TextPadding;
        }
        if(is({ WidgetType::TEXT, WidgetType::BUTTON }))
        {
            j["TextAlignment"] = HAlignToString(w.HAlign);
            j["TextVAlignment"] = VAlignToString(w.VAlign);
        }
        if(w.Type == WidgetType::TEXT)
            j["Overflow"] = EnumName(w.Overflow, sOverflowNames);

        if(is({ WidgetType::BUTTON, WidgetType::IMAGE_BUTTON, WidgetType::DROPDOWN }))
        {
            j["NormalColor"] = Vec4ToJson(w.NormalColor);
            j["HoverColor"] = Vec4ToJson(w.HoverColor);
            j["PressedColor"] = Vec4ToJson(w.PressedColor);
        }
        if(is({ WidgetType::INPUT_FIELD, WidgetType::DROPDOWN, WidgetType::TAB_VIEW }))
            j["HighlightColor"] = Vec4ToJson(w.HighlightColor);

        if(is({ WidgetType::PROGRESS_BAR, WidgetType::SLIDER }))
        {
            j["MinValue"] = w.MinValue;
            j["MaxValue"] = w.MaxValue;
            j["Value"] = w.Value;
        }
        if(w.Type == WidgetType::PROGRESS_BAR)
            j["FillPadding"] = w.FillPadding;
        if(w.Type == WidgetType::SLIDER)
        {
            j["WholeNumbers"] = w.WholeNumbers;
            j["TrackThickness"] = w.TrackThickness;
        }
        if(is({ WidgetType::SLIDER, WidgetType::JOYSTICK }))
            j["HandleSize"] = w.HandleSize;

        if(w.Type == WidgetType::TOGGLE)
        {
            j["IsOn"] = w.IsOn;
            j["Group"] = w.Group;
            j["AllowSwitchOff"] = w.AllowSwitchOff;
            j["ToggleStyle"] = EnumName(w.ToggleType, sToggleStyleNames);
        }

        if(w.Type == WidgetType::JOYSTICK)
        {
            j["StickMode"] = EnumName(w.StickMode, sJoystickModeNames);
            j["BaseSize"] = w.BaseSize;
            j["DeadZone"] = w.DeadZone;
            j["HandleRange"] = w.HandleRange;
            j["HideWhenIdle"] = w.HideWhenIdle;
        }

        if(is({ WidgetType::SCROLL_VIEW, WidgetType::LIST_VIEW }))
        {
            j["ScrollHorizontal"] = w.ScrollHorizontal;
            j["ScrollVertical"] = w.ScrollVertical;
            j["DecelerationRate"] = w.DecelerationRate;
            j["ShowScrollbars"] = w.ShowScrollbars;
            j["ScrollbarColor"] = Vec4ToJson(w.ScrollbarColor);
        }

        if(is({ WidgetType::LAYOUT_GROUP, WidgetType::LIST_VIEW }))
        {
            j["Padding"] = Vec4ToJson(w.Padding);
            j["Spacing"] = Vec2ToJson(w.Spacing);
            j["Columns"] = w.Columns;
        }
        if(w.Type == WidgetType::LAYOUT_GROUP)
        {
            j["Direction"] = EnumName(w.Direction, sLayoutDirectionNames);
            j["CellSize"] = Vec2ToJson(w.CellSize);
            j["ChildAlignment"] = Vec2ToJson(w.ChildAlignment);
            j["ControlChildWidth"] = w.ControlChildWidth;
            j["ControlChildHeight"] = w.ControlChildHeight;
            j["FitContent"] = w.FitContent;
        }
        if(w.Type == WidgetType::LIST_VIEW)
        {
            j["ItemSize"] = Vec2ToJson(w.ItemSize);
            j["PreviewItemCount"] = w.PreviewItemCount;
        }

        if(is({ WidgetType::INPUT_FIELD, WidgetType::DROPDOWN }))
            j["Placeholder"] = w.Placeholder;
        if(w.Type == WidgetType::INPUT_FIELD)
        {
            j["ContentType"] = EnumName(w.ContentType, sContentTypeNames);
            j["MultiLine"] = w.MultiLine;
            j["MaxLength"] = w.MaxLength;
            j["PlaceholderColor"] = Vec4ToJson(w.PlaceholderColor);
            j["CaretColor"] = Vec4ToJson(w.CaretColor);
            j["SelectionColor"] = Vec4ToJson(w.SelectionColor);
        }

        if(w.Type == WidgetType::DROPDOWN)
        {
            j["Options"] = w.Options;
            j["SelectedIndex"] = w.SelectedIndex;
            j["MaxVisibleItems"] = w.MaxVisibleItems;
        }

        if(w.Type == WidgetType::MODAL)
        {
            j["BackdropColor"] = Vec4ToJson(w.BackdropColor);
            j["CloseOnBackdrop"] = w.CloseOnBackdrop;
            j["CloseOnBack"] = w.CloseOnBack;
        }

        if(w.Type == WidgetType::TAB_VIEW)
        {
            j["ActiveTab"] = w.ActiveTab;
            j["TabsAtBottom"] = w.TabsAtBottom;
            j["TabBarHeight"] = w.TabBarHeight;
        }

        if(w.Type == WidgetType::SPINNER)
        {
            j["DotCount"] = w.DotCount;
            j["DotSize"] = w.DotSize;
            j["Speed"] = w.Speed;
        }

        if(w.Type == WidgetType::SAFE_AREA)
            j["SafeEdges"] = w.SafeEdges;

        nlohmann::json& children = j["Children"];
        children = nlohmann::json::array();
        for(const UI::WidgetDesc& child : w.Children)
            children.push_back(WidgetToJson(child));

        return j;
    }

    static UI::WidgetDesc WidgetFromJson(const nlohmann::json& j)
    {
        const UI::WidgetDesc defaults;
        UI::WidgetDesc w;
        w.ID = j.value("ID", UUID().Get());
        w.Name = j.value("Name", String());
        w.Type = UI::WidgetTypeFromString(j.value("Type", String("PANEL")));
        w.Visible = j.value("Visible", true);
        w.AnchorMin = JsonToVec2(j, "AnchorMin", defaults.AnchorMin);
        w.AnchorMax = JsonToVec2(j, "AnchorMax", defaults.AnchorMax);
        w.Pivot = JsonToVec2(j, "Pivot", defaults.Pivot);
        w.Offset = JsonToVec2(j, "Offset", defaults.Offset);
        w.Size = JsonToVec2(j, "Size", defaults.Size);
        w.Color = JsonToVec4(j, "Color", defaults.Color);

        w.Background = PartFromJson(j, "Background");
        w.Fill = PartFromJson(j, "Fill");
        w.Handle = PartFromJson(j, "Handle");
        if(!j.contains("Background") && j.contains("Texture")) // Version 1: image/button texture at the top level
            w.Background.Texture = j.value("Texture", static_cast<uint64_t>(UUID::INVALID));

        w.FillAmount = j.value("FillAmount", defaults.FillAmount);
        w.Origin = EnumFromJson(j, "Origin", defaults.Origin, sFillOriginNames);
        w.PreserveAspect = j.value("PreserveAspect", defaults.PreserveAspect);

        w.Text = j.value("Text", String());
        w.Font = j.value("Font", static_cast<uint64_t>(UUID::INVALID));
        w.FontSize = j.value("FontSize", defaults.FontSize);
        w.HAlign = HAlignFromString(j.value("TextAlignment", String("CENTER")));
        w.VAlign = VAlignFromString(j.value("TextVAlignment", String("CENTER")));
        if(j.contains("Overflow"))
            w.Overflow = EnumFromJson(j, "Overflow", defaults.Overflow, sOverflowNames);
        else
            w.Overflow = j.value("WordWrap", true) ? UI::TextOverflow::WRAP : UI::TextOverflow::NO_WRAP; // Version 1
        w.TextColor = JsonToVec4(j, "TextColor", defaults.TextColor);
        w.TextPadding = j.value("TextPadding", defaults.TextPadding);

        w.NormalColor = JsonToVec4(j, "NormalColor", defaults.NormalColor);
        w.HoverColor = JsonToVec4(j, "HoverColor", defaults.HoverColor);
        w.PressedColor = JsonToVec4(j, "PressedColor", defaults.PressedColor);
        w.HighlightColor = JsonToVec4(j, "HighlightColor", defaults.HighlightColor);

        w.MinValue = j.value("MinValue", defaults.MinValue);
        w.MaxValue = j.value("MaxValue", defaults.MaxValue);
        w.Value = j.value("Value", defaults.Value);
        w.WholeNumbers = j.value("WholeNumbers", defaults.WholeNumbers);
        w.FillPadding = j.value("FillPadding", defaults.FillPadding);

        w.IsOn = j.value("IsOn", defaults.IsOn);
        w.Group = j.value("Group", String());
        w.AllowSwitchOff = j.value("AllowSwitchOff", defaults.AllowSwitchOff);
        w.ToggleType = EnumFromJson(j, "ToggleStyle", defaults.ToggleType, sToggleStyleNames);

        w.HandleSize = j.value("HandleSize", defaults.HandleSize);
        w.TrackThickness = j.value("TrackThickness", defaults.TrackThickness);
        w.StickMode = EnumFromJson(j, "StickMode", defaults.StickMode, sJoystickModeNames);
        w.BaseSize = j.value("BaseSize", defaults.BaseSize);
        w.DeadZone = j.value("DeadZone", defaults.DeadZone);
        w.HandleRange = j.value("HandleRange", defaults.HandleRange);
        w.HideWhenIdle = j.value("HideWhenIdle", defaults.HideWhenIdle);

        w.ScrollHorizontal = j.value("ScrollHorizontal", defaults.ScrollHorizontal);
        w.ScrollVertical = j.value("ScrollVertical", defaults.ScrollVertical);
        w.DecelerationRate = j.value("DecelerationRate", defaults.DecelerationRate);
        w.ShowScrollbars = j.value("ShowScrollbars", defaults.ShowScrollbars);
        w.ScrollbarColor = JsonToVec4(j, "ScrollbarColor", defaults.ScrollbarColor);

        w.Direction = EnumFromJson(j, "Direction", defaults.Direction, sLayoutDirectionNames);
        w.Padding = JsonToVec4(j, "Padding", defaults.Padding);
        w.Spacing = JsonToVec2(j, "Spacing", defaults.Spacing);
        w.CellSize = JsonToVec2(j, "CellSize", defaults.CellSize);
        w.Columns = j.value("Columns", defaults.Columns);
        w.ChildAlignment = JsonToVec2(j, "ChildAlignment", defaults.ChildAlignment);
        w.ControlChildWidth = j.value("ControlChildWidth", defaults.ControlChildWidth);
        w.ControlChildHeight = j.value("ControlChildHeight", defaults.ControlChildHeight);
        w.FitContent = j.value("FitContent", defaults.FitContent);
        w.ItemSize = JsonToVec2(j, "ItemSize", defaults.ItemSize);
        w.PreviewItemCount = j.value("PreviewItemCount", defaults.PreviewItemCount);

        w.Placeholder = j.value("Placeholder", String());
        w.ContentType = EnumFromJson(j, "ContentType", defaults.ContentType, sContentTypeNames);
        w.MultiLine = j.value("MultiLine", defaults.MultiLine);
        w.MaxLength = j.value("MaxLength", defaults.MaxLength);
        w.PlaceholderColor = JsonToVec4(j, "PlaceholderColor", defaults.PlaceholderColor);
        w.CaretColor = JsonToVec4(j, "CaretColor", defaults.CaretColor);
        w.SelectionColor = JsonToVec4(j, "SelectionColor", defaults.SelectionColor);

        if(j.contains("Options") && j["Options"].is_array())
        {
            for(const nlohmann::json& option : j["Options"])
            {
                if(option.is_string())
                    w.Options.push_back(option.get<String>());
            }
        }
        w.SelectedIndex = j.value("SelectedIndex", defaults.SelectedIndex);
        w.MaxVisibleItems = j.value("MaxVisibleItems", defaults.MaxVisibleItems);

        w.BackdropColor = JsonToVec4(j, "BackdropColor", defaults.BackdropColor);
        w.CloseOnBackdrop = j.value("CloseOnBackdrop", defaults.CloseOnBackdrop);
        w.CloseOnBack = j.value("CloseOnBack", defaults.CloseOnBack);

        w.ActiveTab = j.value("ActiveTab", defaults.ActiveTab);
        w.TabsAtBottom = j.value("TabsAtBottom", defaults.TabsAtBottom);
        w.TabBarHeight = j.value("TabBarHeight", defaults.TabBarHeight);

        w.DotCount = j.value("DotCount", defaults.DotCount);
        w.DotSize = j.value("DotSize", defaults.DotSize);
        w.Speed = j.value("Speed", defaults.Speed);
        w.SafeEdges = j.value("SafeEdges", defaults.SafeEdges);

        if(j.contains("Children") && j["Children"].is_array())
        {
            for(const nlohmann::json& child : j["Children"])
                w.Children.push_back(WidgetFromJson(child));
        }
        return w;
    }

    String UILayoutSourceIO::ToJson(const UILayout& layout)
    {
        nlohmann::json j;
        j["Version"] = UI_LAYOUT_VERSION;
        j["Scaler"]["ReferenceResolution"] = Vec2ToJson(layout.Scaler.ReferenceResolution);
        j["Scaler"]["MatchWidthOrHeight"] = layout.Scaler.MatchWidthOrHeight;
        j["Root"] = WidgetToJson(layout.Root);
        return j.dump(4);
    }

    bool UILayoutSourceIO::FromJson(const String& json, UILayout& outLayout)
    {
        nlohmann::json j = nlohmann::json::parse(json, nullptr, false);
        if(j.is_discarded() || !j.is_object())
            return false;

        if(j.contains("Scaler"))
        {
            const nlohmann::json& scaler = j["Scaler"];
            outLayout.Scaler.ReferenceResolution = JsonToVec2(scaler, "ReferenceResolution", { 1920.0f, 1080.0f });
            outLayout.Scaler.MatchWidthOrHeight = scaler.value("MatchWidthOrHeight", 1.0f);
        }

        outLayout.Root = j.contains("Root") ? WidgetFromJson(j["Root"]) : UI::WidgetDesc::CreateRoot();
        return true;
    }

    bool UILayoutSourceIO::Write(const UILayout& layout, const Path& path)
    {
        if(!Filesystem::WriteTextFile(path, ToJson(layout)))
        {
            Log<Severity::Error>("[UILayoutSourceIO] Failed to write {}", path.string());
            return false;
        }
        return true;
    }

    bool UILayoutSourceIO::Read(const Path& path, UILayout& outLayout)
    {
        String json;
        return Filesystem::ReadTextFile(path, json) && FromJson(json, outLayout);
    }
}
