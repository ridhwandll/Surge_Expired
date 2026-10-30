// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Asset.hpp"
#include "Surge/Graphics/UISystem/UIWidgets.hpp"
#include "Surge/Graphics/UISystem/UIManager.hpp"
#include "Surge/Graphics/UISystem/UIInputField.hpp"
#include <unordered_map>

namespace Surge
{
    class Font;

    namespace UI
    {
        // Look of one drawn part of a widget (Image, button background, slider handle...), see UI::ImageStyle
        struct ImagePartDesc
        {
            AssetID Texture = UUID::INVALID;
            glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
            ImageMode Mode = ImageMode::SIMPLE;
            glm::vec4 SliceBorder = { 0.0f, 0.0f, 0.0f, 0.0f }; // left, top, right, bottom (texture pixels)
            float SliceScale = 1.0f;
            bool FillCenter = true;
            float CornerRadius = 0.0f; // UI units
            float BorderWidth = 0.0f;  // UI units
            glm::vec4 BorderColor = { 0.0f, 0.0f, 0.0f, 1.0f };
        };

        // Plain data description of a widget, what the UI Editor edits and .sui files store
        // UI::Instantiate() turns it into a live UI::Widget hierarchy. Fields are shared between types where they mean the same thing
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
            glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f }; // Tint for Image/Button, text color for Text, dot color for Spinner, overall tint otherwise

            // Parts: Background = image/button/field/track/box/base/bar/panel, Fill = fill/check/popup/active tab, Handle = slider handle/switch knob/stick
            ImagePartDesc Background;
            ImagePartDesc Fill;
            ImagePartDesc Handle;

            // IMAGE (Background.Mode == FILLED), PROGRESS_BAR, SLIDER (direction)
            float FillAmount = 1.0f;
            FillOrigin Origin = FillOrigin::LEFT;
            bool PreserveAspect = false;

            // TEXT, BUTTON, TOGGLE (label), INPUT_FIELD, DROPDOWN, TAB_VIEW
            String Text;
            AssetID Font = UUID::INVALID;
            float FontSize = 32.0f;
            TextAlignment HAlign = TextAlignment::CENTER;
            TextVerticalAlignment VAlign = TextVerticalAlignment::CENTER;
            TextOverflow Overflow = TextOverflow::WRAP;       // TEXT only, wraps/truncates at the rect width
            glm::vec4 TextColor = { 1.0f, 1.0f, 1.0f, 1.0f }; // Label/field/tab text color
            float TextPadding = 15.0f;                         // BUTTON label inset, INPUT_FIELD/DROPDOWN text inset

            // BUTTON, IMAGE_BUTTON (DROPDOWN: HoverColor = list item hover)
            glm::vec4 NormalColor = { 1.0f, 1.0f, 1.0f, 1.0f };
            glm::vec4 HoverColor = { 0.8f, 0.8f, 0.8f, 1.0f };
            glm::vec4 PressedColor = { 0.5f, 0.5f, 0.5f, 1.0f };
            glm::vec4 HighlightColor = { 0.25f, 0.6f, 1.0f, 1.0f }; // INPUT_FIELD focus border, DROPDOWN selected item, TAB_VIEW active text

            // PROGRESS_BAR, SLIDER
            float MinValue = 0.0f;
            float MaxValue = 1.0f;
            float Value = 0.5f;
            bool WholeNumbers = false; // SLIDER
            float FillPadding = 0.0f;  // PROGRESS_BAR

            // TOGGLE
            bool IsOn = false;
            String Group;
            bool AllowSwitchOff = false;
            ToggleStyle ToggleType = ToggleStyle::CHECKBOX;

            // SLIDER, JOYSTICK
            float HandleSize = 40.0f;   // UI units (JOYSTICK: 0 = 45% of the base)
            float TrackThickness = 12.0f;
            JoystickMode StickMode = JoystickMode::FIXED;
            float BaseSize = 0.0f;
            float DeadZone = 0.1f;
            float HandleRange = 1.0f;
            bool HideWhenIdle = false;

            // SCROLL_VIEW, LIST_VIEW
            bool ScrollHorizontal = false;
            bool ScrollVertical = true;
            float DecelerationRate = 0.135f;
            bool ShowScrollbars = true;
            glm::vec4 ScrollbarColor = { 1.0f, 1.0f, 1.0f, 0.45f };

            // LAYOUT_GROUP (LIST_VIEW: Padding, Spacing, Columns)
            LayoutDirection Direction = LayoutDirection::VERTICAL;
            glm::vec4 Padding = { 0.0f, 0.0f, 0.0f, 0.0f };
            glm::vec2 Spacing = { 10.0f, 10.0f };
            glm::vec2 CellSize = { 100.0f, 100.0f };
            int Columns = 0;
            glm::vec2 ChildAlignment = { 0.0f, 0.0f };
            bool ControlChildWidth = false;
            bool ControlChildHeight = false;
            bool FitContent = false;

            // LIST_VIEW: first child = item template
            glm::vec2 ItemSize = { 0.0f, 0.0f };
            int PreviewItemCount = 3;

            // INPUT_FIELD (Text = initial text), DROPDOWN (Placeholder = shown with no selection)
            String Placeholder;
            InputContentType ContentType = InputContentType::STANDARD;
            bool MultiLine = false;
            int MaxLength = 0;
            glm::vec4 PlaceholderColor = { 1.0f, 1.0f, 1.0f, 0.35f };
            glm::vec4 CaretColor = { 1.0f, 1.0f, 1.0f, 1.0f };
            glm::vec4 SelectionColor = { 0.25f, 0.6f, 1.0f, 0.45f };

            // DROPDOWN
            Vector<String> Options;
            int SelectedIndex = -1;
            int MaxVisibleItems = 5;

            // MODAL
            glm::vec4 BackdropColor = { 0.0f, 0.0f, 0.0f, 0.6f };
            bool CloseOnBackdrop = true;
            bool CloseOnBack = true;

            // TAB_VIEW: one page per child, the child name is the tab label
            int ActiveTab = 0;
            bool TabsAtBottom = false;
            float TabBarHeight = 90.0f;

            // SPINNER
            int DotCount = 10;
            float DotSize = 0.2f;
            float Speed = 1.0f;

            // SAFE_AREA
            int SafeEdges = 15; // UI::SafeArea::Edge bits

            Vector<WidgetDesc> Children;

            WidgetDesc* Find(UUID id);
            WidgetDesc* FindParentOf(UUID id);
            static WidgetDesc CreateDefault(WidgetType type, const String& name, AssetID defaultFont = UUID::INVALID);
            static WidgetDesc CreateRoot();
        };

        const char* WidgetTypeToString(WidgetType type);       // "BUTTON"
        const char* WidgetTypeToDisplayName(WidgetType type);  // "Button"
        WidgetType WidgetTypeFromString(const String& str);
        bool IsContainerType(WidgetType type);                 // Children are content (new widgets go inside it in the UI Editor)

        // Caches fonts/textures while instantiating, AssetManager::Load can be expensive (stamp checks in the Editor)
        class LayoutResourceCache
        {
        public:
            Ref<Font> GetFont(AssetID id); // No font: the engine UI font (Renderer::GetEngineFont)
            Ref<Asset> GetTexture(AssetID id, ImageHandle& outImage);
            void Clear() { mAssets.clear(); }

        private:
            std::unordered_map<uint64_t, Ref<Asset>> mAssets;
        };

        // Builds a live widget hierarchy. outWidgetMap (optional) maps WidgetDesc::ID -> created widget
        // designPreview: UI Editor preview (modals drawn in place, list views show placeholder items, popups never open)
        Ref<Widget> Instantiate(const WidgetDesc& desc, LayoutResourceCache& cache, std::unordered_map<uint64_t, Widget*>* outWidgetMap = nullptr, bool designPreview = false);
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
