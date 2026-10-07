// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Asset.hpp"
#include "Surge/Graphics/UISystem/UIWidgets.hpp"
#include "Surge/Graphics/UISystem/UIManager.hpp"
#include <unordered_map>

namespace Surge
{
    class Font;

    namespace UI
    {
        // Plain data description of a widget, what the UI Editor edits and .sui files store
        // UI::Instantiate() turns it into a live UI::Widget hierarchy
        struct WidgetDesc
        {
            UUID ID;
            String Name;
            WidgetType Type = WidgetType::BASE_WIDGET;
            bool Visible = true;

            // Rect transform (see UI::Widget)
            glm::vec2 AnchorMin = { 0.5f, 0.5f };
            glm::vec2 AnchorMax = { 0.5f, 0.5f };
            glm::vec2 Pivot = { 0.5f, 0.5f };
            glm::vec2 Offset = { 0.0f, 0.0f };
            glm::vec2 Size = { 200.0f, 100.0f };
            glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f }; // Tint for Image, text color for Text

            // IMAGE, BUTTON, IMAGE_BUTTON
            AssetID Texture = UUID::INVALID;

            // TEXT, BUTTON
            String Text;
            AssetID Font = UUID::INVALID;
            float FontSize = 32.0f;
            TextAlignment HAlign = TextAlignment::CENTER;
            TextVerticalAlignment VAlign = TextVerticalAlignment::CENTER;
            bool WordWrap = true;                          // TEXT only, wraps at the rect width
            glm::vec4 TextColor = { 1.0f, 1.0f, 1.0f, 1.0f }; // BUTTON label color
            float TextPadding = 15.0f;                     // BUTTON label inset for LEFT/RIGHT/TOP/BOTTOM alignment

            // BUTTON, IMAGE_BUTTON
            glm::vec4 NormalColor = { 1.0f, 1.0f, 1.0f, 1.0f };
            glm::vec4 HoverColor = { 0.8f, 0.8f, 0.8f, 1.0f };
            glm::vec4 PressedColor = { 0.5f, 0.5f, 0.5f, 1.0f };

            Vector<WidgetDesc> Children;

            WidgetDesc* Find(UUID id);
            WidgetDesc* FindParentOf(UUID id);
            static WidgetDesc CreateDefault(WidgetType type, const String& name, AssetID defaultFont = UUID::INVALID);
            static WidgetDesc CreateRoot();
        };

        const char* WidgetTypeToString(WidgetType type);       // "BUTTON"
        const char* WidgetTypeToDisplayName(WidgetType type);  // "Button"
        WidgetType WidgetTypeFromString(const String& str);

        // Caches fonts/textures while instantiating, AssetManager::Load can be expensive (stamp checks in the Editor)
        class LayoutResourceCache
        {
        public:
            Ref<Font> GetFont(AssetID id);
            Ref<Asset> GetTexture(AssetID id, ImageHandle& outImage);
            void Clear() { mAssets.clear(); }

        private:
            std::unordered_map<uint64_t, Ref<Asset>> mAssets;
        };

        // Builds a live widget hierarchy. outWidgetMap (optional) maps WidgetDesc::ID -> created widget
        Ref<Widget> Instantiate(const WidgetDesc& desc, LayoutResourceCache& cache, std::unordered_map<uint64_t, Widget*>* outWidgetMap = nullptr);
    }

    // UI Layout asset (.sui, human readable JSON, not cooked, loaded directly like scenes)
    class UILayout final : public Asset
    {
    public:
        UILayout() { Root = UI::WidgetDesc::CreateRoot(); }
        SURGE_ASSET_TYPE(AssetType::UI_LAYOUT);
        static Ref<UILayout> Create() { return Ref<UILayout>::Create(); }

        Ref<UI::Widget> Instantiate() const;

    public:
        UI::WidgetDesc Root;
        UI::CanvasScaler Scaler;
    };
}
