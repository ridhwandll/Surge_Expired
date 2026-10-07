// Copyright (c) - SurgeTechnologies - All rights reserved
#include "UILayoutSerializer.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include <json/json.hpp>

namespace Surge
{
    static constexpr int UI_LAYOUT_VERSION = 1;

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

    static nlohmann::json WidgetToJson(const UI::WidgetDesc& w)
    {
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
        const bool hasTexture = w.Type == UI::WidgetType::IMAGE || w.Type == UI::WidgetType::BUTTON || w.Type == UI::WidgetType::IMAGE_BUTTON;
        const bool hasText = w.Type == UI::WidgetType::TEXT || w.Type == UI::WidgetType::BUTTON;
        const bool hasStates = w.Type == UI::WidgetType::BUTTON || w.Type == UI::WidgetType::IMAGE_BUTTON;

        if(hasTexture)
            j["Texture"] = w.Texture.Get();

        if(hasText)
        {
            j["Text"] = w.Text;
            j["Font"] = w.Font.Get();
            j["FontSize"] = w.FontSize;
            j["TextAlignment"] = HAlignToString(w.HAlign);
            j["TextVAlignment"] = VAlignToString(w.VAlign);
            j["WordWrap"] = w.WordWrap;
            j["TextColor"] = Vec4ToJson(w.TextColor);
            j["TextPadding"] = w.TextPadding;
        }

        if(hasStates)
        {
            j["NormalColor"] = Vec4ToJson(w.NormalColor);
            j["HoverColor"] = Vec4ToJson(w.HoverColor);
            j["PressedColor"] = Vec4ToJson(w.PressedColor);
        }

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
        w.Texture = j.value("Texture", static_cast<uint64_t>(UUID::INVALID));
        w.Text = j.value("Text", String());
        w.Font = j.value("Font", static_cast<uint64_t>(UUID::INVALID));
        w.FontSize = j.value("FontSize", defaults.FontSize);
        w.HAlign = HAlignFromString(j.value("TextAlignment", String("CENTER")));
        w.VAlign = VAlignFromString(j.value("TextVAlignment", String("CENTER")));
        w.WordWrap = j.value("WordWrap", defaults.WordWrap);
        w.TextColor = JsonToVec4(j, "TextColor", defaults.TextColor);
        w.TextPadding = j.value("TextPadding", defaults.TextPadding);
        w.NormalColor = JsonToVec4(j, "NormalColor", defaults.NormalColor);
        w.HoverColor = JsonToVec4(j, "HoverColor", defaults.HoverColor);
        w.PressedColor = JsonToVec4(j, "PressedColor", defaults.PressedColor);

        if(j.contains("Children") && j["Children"].is_array())
        {
            for(const nlohmann::json& child : j["Children"])
                w.Children.push_back(WidgetFromJson(child));
        }
        return w;
    }

    String UILayoutSerializer::ToJson(const UILayout& layout)
    {
        nlohmann::json j;
        j["Version"] = UI_LAYOUT_VERSION;
        j["Scaler"]["ReferenceResolution"] = Vec2ToJson(layout.Scaler.ReferenceResolution);
        j["Scaler"]["MatchWidthOrHeight"] = layout.Scaler.MatchWidthOrHeight;
        j["Root"] = WidgetToJson(layout.Root);
        return j.dump(4);
    }

    bool UILayoutSerializer::FromJson(const String& json, UILayout& outLayout)
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

    void UILayoutSerializer::Initialize()
    {
        mSerializerType = AssetType::UI_LAYOUT;
    }

    bool UILayoutSerializer::Serialize(Ref<Asset> asset) const
    {
#ifdef SURGE_PLATFORM_ANDROID
        Log<Severity::Error>("[UILayoutSerializer] Serialize is unsupported on Android runtime. APK assets are readonly!");
        return false;
#else
        AssetManager* am = Core::GetAssetManager();
        const AssetMetadata& metadata = am->GetMetadata(asset->GetID());
        const String absolutePath = am->GetAbsolutePath(metadata.RelativePath);

        if(!Filesystem::WriteTextFile(absolutePath, ToJson(*asset.As<UILayout>())))
        {
            Log<Severity::Error>("[UILayoutSerializer] Failed to write {}", absolutePath);
            return false;
        }
        return true;
#endif
    }

    Ref<Asset> UILayoutSerializer::Deserialize(const AssetMetadata& metadata) const
    {
        AssetManager* am = Core::GetAssetManager();
        const String absolutePath = am->GetAbsolutePath(metadata.RelativePath);

        String json;
        if(!Filesystem::ReadTextFile(absolutePath, json))
        {
            Log<Severity::Error>("[UILayoutSerializer] Failed to read {}", absolutePath);
            return nullptr;
        }

        Ref<UILayout> layout = UILayout::Create();
        if(!FromJson(json, *layout))
        {
            Log<Severity::Error>("[UILayoutSerializer] Failed to parse {}", absolutePath);
            return nullptr;
        }
        return layout;
    }

    void UILayoutSerializer::Shutdown()
    {
        Log<Severity::Info>("[UILayoutSerializer] Shutdown");
    }
}
