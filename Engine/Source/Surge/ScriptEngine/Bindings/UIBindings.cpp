// Copyright (c) - SurgeTechnologies - All rights reserved
#include "UIBindings.hpp"
#include "Surge/ScriptEngine/Lua.hpp"

#include "Surge/Core/Core.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/UISystem/UIManager.hpp"
#include "Surge/Graphics/UISystem/UIWidgets.hpp"
#include "Surge/Graphics/UISystem/UIScrollView.hpp"
#include "Surge/Graphics/UISystem/UIPopups.hpp"
#include "Surge/Graphics/UISystem/UIInputField.hpp"
#include "Surge/ECS/Components/ScriptComponent.hpp"
#include "BindingUtils.hpp"

namespace Surge::ScriptBinding
{
    // Lua only sees the static type it was given, hand out the concrete type so e.g. button.NormalColor works on a FindWidget() result
    static sol::object ToTypedLuaObject(sol::state_view lua, UI::Widget* widget)
    {
        if(!widget)
            return sol::make_object(lua, sol::lua_nil);

        switch(widget->GetType())
        {
            case UI::WidgetType::BUTTON:       return sol::make_object(lua, Ref<UI::Button>(static_cast<UI::Button*>(widget)));
            case UI::WidgetType::IMAGE:        return sol::make_object(lua, Ref<UI::Image>(static_cast<UI::Image*>(widget)));
            case UI::WidgetType::TEXT:         return sol::make_object(lua, Ref<UI::Text>(static_cast<UI::Text*>(widget)));
            case UI::WidgetType::IMAGE_BUTTON: return sol::make_object(lua, Ref<UI::ImageButton>(static_cast<UI::ImageButton*>(widget)));
            case UI::WidgetType::PROGRESS_BAR: return sol::make_object(lua, Ref<UI::ProgressBar>(static_cast<UI::ProgressBar*>(widget)));
            case UI::WidgetType::TOGGLE:       return sol::make_object(lua, Ref<UI::Toggle>(static_cast<UI::Toggle*>(widget)));
            case UI::WidgetType::SLIDER:       return sol::make_object(lua, Ref<UI::Slider>(static_cast<UI::Slider*>(widget)));
            case UI::WidgetType::JOYSTICK:     return sol::make_object(lua, Ref<UI::Joystick>(static_cast<UI::Joystick*>(widget)));
            case UI::WidgetType::SCROLL_VIEW:  return sol::make_object(lua, Ref<UI::ScrollView>(static_cast<UI::ScrollView*>(widget)));
            case UI::WidgetType::LAYOUT_GROUP: return sol::make_object(lua, Ref<UI::LayoutGroup>(static_cast<UI::LayoutGroup*>(widget)));
            case UI::WidgetType::INPUT_FIELD:  return sol::make_object(lua, Ref<UI::InputField>(static_cast<UI::InputField*>(widget)));
            case UI::WidgetType::DROPDOWN:     return sol::make_object(lua, Ref<UI::Dropdown>(static_cast<UI::Dropdown*>(widget)));
            case UI::WidgetType::MODAL:        return sol::make_object(lua, Ref<UI::Modal>(static_cast<UI::Modal*>(widget)));
            case UI::WidgetType::TAB_VIEW:     return sol::make_object(lua, Ref<UI::TabView>(static_cast<UI::TabView*>(widget)));
            case UI::WidgetType::LIST_VIEW:    return sol::make_object(lua, Ref<UI::ListView>(static_cast<UI::ListView*>(widget)));
            case UI::WidgetType::SPINNER:      return sol::make_object(lua, Ref<UI::Spinner>(static_cast<UI::Spinner*>(widget)));
            case UI::WidgetType::SAFE_AREA:    return sol::make_object(lua, Ref<UI::SafeArea>(static_cast<UI::SafeArea*>(widget)));
            case UI::WidgetType::BASE_WIDGET:  break;
        }
        return sol::make_object(lua, Ref<UI::Widget>(widget));
    }

    static void ReportError(const sol::protected_function_result& result)
    {
        if(!result.valid())
        {
            sol::error err = result;
            Log<Severity::Error>("Lua UI Callback Error: {}", err.what());
        }
    }

    class LuaEventCallback : public UI::IEventCallback
    {
    public:
        LuaEventCallback(sol::protected_function func, UI::Widget* self)
            : mFunc(std::move(func)), mSelf(self) {}

        void Invoke() override
        {
            if(mFunc.valid())
                ReportError(mFunc(ToTypedLuaObject(mFunc.lua_state(), mSelf)));
        }
    private:
        sol::protected_function mFunc;
        UI::Widget* mSelf;
    };

    // ListView hooks: OnBindItem(item, index) with a 1 based index, OnCreateItem() returns a new item widget
    class LuaListCallback : public UI::IListCallback
    {
    public:
        LuaListCallback(sol::protected_function func)
            : mFunc(std::move(func)) {}

        void BindItem(UI::Widget* item, int index) override
        {
            if(mFunc.valid())
                ReportError(mFunc(ToTypedLuaObject(mFunc.lua_state(), item), index + 1));
        }

        Ref<UI::Widget> CreateItem() override
        {
            if(!mFunc.valid())
                return nullptr;

            sol::protected_function_result result = mFunc();
            if(!result.valid())
            {
                ReportError(result);
                return nullptr;
            }
            sol::optional<UI::Widget*> widget = result;
            return widget && *widget ? Ref<UI::Widget>(*widget) : nullptr;
        }
    private:
        sol::protected_function mFunc;
    };

    static Ref<Texture2D> LoadTexture(const String& relativePath, const char* who)
    {
        if(relativePath.empty())
            return nullptr;

        AssetManager* am = Core::GetAssetManager();
        const AssetID id = am->GetIDFromPath(relativePath);
        Ref<Texture2D> texture = id ? am->Load<Texture2D>(id) : nullptr;
        if(!texture)
            Log<Severity::Warn>("[UIBindings.cpp] Lua: {}: Failed to load texture at path {}", who, relativePath);
        return texture;
    }

    static Ref<Font> LoadFont(const String& relativePath, const char* who)
    {
        if(relativePath.empty())
            return Core::GetRenderer()->GetEngineFont();

        AssetManager* am = Core::GetAssetManager();
        const AssetID id = am->GetIDFromPath(relativePath);
        Ref<Font> font = id ? am->Load<Font>(id) : nullptr;
        if(!font)
            Log<Severity::Warn>("[UIBindings.cpp] Lua: {}: Failed to load font at path {}", who, relativePath);
        return font;
    }

    static void SetStyleTexture(UI::ImageStyle& style, const String& relativePath)
    {
        Ref<Texture2D> texture = LoadTexture(relativePath, "SetTexture");
        style.SetTexture(texture ? texture->GetRHIImage() : ImageHandle::Invalid(), texture);
    }

    void BindUIWidgets(void* luaState)
    {
        sol::state_view& lua = *static_cast<sol::state_view*>(luaState);

        // Attaches to the UICanvasComponent whose script is running (or a global canvas when called from a regular script)
        lua.set_function("SetUIRoot", [](UI::Widget* root) {
            UI::Manager & uiManager = Core::GetRenderer()->GetUIManager();
            root ? uiManager.SetRoot(Ref<UI::Widget>(root)) : uiManager.ClearRoot();
        });

        // entity.UICanvasC:FindWidget("PlayButton") -> typed widget (UIButton/UIText/...) from the canvas layout or script root, nil if not found
        sol::usertype<UICanvasComponent> canvasType = lua["UICanvasComponent"];
        canvasType.set_function("FindWidget", [](UICanvasComponent& canvas, const String& name, sol::this_state s) -> sol::object {
            UI::Widget* widget = canvas.RuntimeCanvasID ? Core::GetRenderer()->GetUIManager().FindWidget(canvas.RuntimeCanvasID, name) : nullptr;
            if(!widget)
                Log<Severity::Warn>("[UIBindings.cpp] Lua: UICanvasComponent:FindWidget: No widget named '{}'", name);
            return ToTypedLuaObject(s, widget);
        });

        // Pointer queries in raw window coordinates (Input.GetTouch(i).Position, Input.GetMousePosition())
        canvasType.set_function("WidgetAt", [](UICanvasComponent& canvas, float x, float y, sol::this_state s) -> sol::object {
            UI::Widget* widget = canvas.RuntimeCanvasID ? Core::GetRenderer()->GetUIManager().WidgetAt(canvas.RuntimeCanvasID, x, y) : nullptr;
            return ToTypedLuaObject(s, widget);
        });
        canvasType.set_function("ScreenToCanvas", [](UICanvasComponent& canvas, float x, float y) -> glm::vec2 {
            return Core::GetRenderer()->GetUIManager().ScreenToCanvas(canvas.RuntimeCanvasID, x, y);
        });
        canvasType.set_function("GetCanvasSize", [](UICanvasComponent& canvas) -> glm::vec2 {
            return Core::GetRenderer()->GetUIManager().GetCanvasSize(canvas.RuntimeCanvasID);
        });

        sol::table uiTable = lua["UI"].get_or_create<sol::table>();
        uiTable.set_function("IsPointerOverUI", [](float x, float y) { return Core::GetRenderer()->GetUIManager().IsPointerOverUI(x, y); });
        uiTable.set_function("WidgetAt", [](float x, float y, sol::this_state s) -> sol::object {
            return ToTypedLuaObject(s, Core::GetRenderer()->GetUIManager().WidgetAt(x, y));
        });
        uiTable.set_function("GetFocused", [](sol::this_state s) -> sol::object {
            return ToTypedLuaObject(s, Core::GetRenderer()->GetUIManager().GetFocused());
        });
        uiTable.set_function("ClearFocus", []() { Core::GetRenderer()->GetUIManager().SetFocus(nullptr); });
        uiTable.set_function("GetSafeAreaInsets", []() -> glm::vec4 { return UI::GetFrameInfo().SafeInsets; });

        lua.new_enum("UIImageMode", "SIMPLE", UI::ImageMode::SIMPLE, "SLICED", UI::ImageMode::SLICED, "FILLED", UI::ImageMode::FILLED);
        lua.new_enum("UIFillOrigin", "LEFT", UI::FillOrigin::LEFT, "RIGHT", UI::FillOrigin::RIGHT, "TOP", UI::FillOrigin::TOP, "BOTTOM", UI::FillOrigin::BOTTOM);
        lua.new_enum("UITextOverflow", "WRAP", UI::TextOverflow::WRAP, "TRUNCATE", UI::TextOverflow::TRUNCATE, "NO_WRAP", UI::TextOverflow::NO_WRAP);
        lua.new_enum("UIToggleStyle", "CHECKBOX", UI::ToggleStyle::CHECKBOX, "SWITCH", UI::ToggleStyle::SWITCH);
        lua.new_enum("UIJoystickMode", "FIXED", UI::JoystickMode::FIXED, "FLOATING", UI::JoystickMode::FLOATING);
        lua.new_enum("UILayoutDirection", "HORIZONTAL", UI::LayoutDirection::HORIZONTAL, "VERTICAL", UI::LayoutDirection::VERTICAL, "GRID", UI::LayoutDirection::GRID);
        lua.new_enum("UIContentType", "STANDARD", UI::InputContentType::STANDARD, "INTEGER", UI::InputContentType::INTEGER, "DECIMAL", UI::InputContentType::DECIMAL,
                     "EMAIL", UI::InputContentType::EMAIL, "PHONE", UI::InputContentType::PHONE, "PASSWORD", UI::InputContentType::PASSWORD);

        //sol::factories enables this in Lua: `local widget = UIWidget.new()`
        lua.new_usertype<UI::Widget>("UIWidget", sol::factories([]() { return Ref<UI::Widget>::Create(); }),
                                     "AddChild", [](UI::Widget& parent, UI::Widget* child) {
                                         if(child)
                                             parent.AddChild(Ref<UI::Widget>(child));
                                     },
                                     "RemoveChild", [](UI::Widget& parent, UI::Widget* child) {
                                         if(child)
                                             parent.RemoveChild(child);
                                     },
                                     "FindChild", [](UI::Widget& parent, const String& name, sol::this_state s) -> sol::object {
                                         return ToTypedLuaObject(s, parent.FindChild(name, true));
                                     },
                                     "Name", sol::property(
                                         [](UI::Widget& w) -> String { return w.GetName(); },
                                         [](UI::Widget& w, const String& val) { w.SetName(val); }
                                     ),
                                     "Visible", sol::property(
                                         [](UI::Widget& w) -> bool { return w.IsVisible(); },
                                         [](UI::Widget& w, bool val) { w.SetVisible(val); }
                                     ),
                                     "Interactable", sol::property(
                                         [](UI::Widget& w) -> bool { return w.IsInteractable(); },
                                         [](UI::Widget& w, bool val) { w.SetInteractable(val); }
                                     ),
                                     "AnchorMin", sol::property(
                                         [](UI::Widget& w) -> glm::vec2 { return w.GetAnchorMin(); },
                                         [](UI::Widget& w, const glm::vec2& val) { w.SetAnchorMin(val.x, val.y); }
                                     ),
                                     "AnchorMax", sol::property(
                                         [](UI::Widget& w) -> glm::vec2 { return w.GetAnchorMax(); },
                                         [](UI::Widget& w, const glm::vec2& val) { w.SetAnchorMax(val.x, val.y); }
                                     ),
                                     "OnClick",        [](UI::Widget& w, sol::protected_function f) { w.SetOnClick(Ref<LuaEventCallback>::Create(f, &w)); },
                                     "OnHoverEnter",   [](UI::Widget& w, sol::protected_function f) { w.SetOnHoverEnter(Ref<LuaEventCallback>::Create(f, &w)); },
                                     "OnHoverExit",    [](UI::Widget& w, sol::protected_function f) { w.SetOnHoverExit(Ref<LuaEventCallback>::Create(f, &w)); },
                                     "OnValueChanged", [](UI::Widget& w, sol::protected_function f) { w.SetOnValueChanged(Ref<LuaEventCallback>::Create(f, &w)); },
                                     "Anchor", sol::property(
                                         [](UI::Widget& w) -> glm::vec2 { return w.GetAnchor(); },
                                         [](UI::Widget& w, const glm::vec2& val) { w.SetAnchor(val.x, val.y); }
                                     ),
                                     "Pivot", sol::property(
                                         [](UI::Widget& w) -> glm::vec2 { return w.GetPivot(); },
                                         [](UI::Widget& w, const glm::vec2& val) { w.SetPivot(val.x, val.y); }
                                     ),
                                     "Offset", sol::property(
                                         [](UI::Widget& w) -> glm::vec2 { return w.GetOffset(); },
                                         [](UI::Widget& w, const glm::vec2& val) { w.SetOffset(val.x, val.y); }
                                     ),
                                     "Size", sol::property(
                                         [](UI::Widget& w) -> glm::vec2 { return w.GetSize(); },
                                         [](UI::Widget& w, const glm::vec2& val) { w.SetSize(val.x, val.y); }
                                     ),
                                     "Color", sol::property(
                                         [](UI::Widget& w) -> glm::vec4 { return w.GetColor(); },
                                         [](UI::Widget& w, const glm::vec4& val) { w.SetColor(val); }
                                     )
        );

        lua.new_usertype<UI::Image>("UIImage",
                                    sol::factories([](const String& textureRelPath) {
                                        Ref<UI::Image> image = Ref<UI::Image>::Create();
                                        SetStyleTexture(image->GetStyle(), textureRelPath);
                                        return image;
                                    }),
                                    sol::base_classes, sol::bases<UI::Widget>(),
                                    "SetTexture", [](UI::Image& image, const String& path) { SetStyleTexture(image.GetStyle(), path); },
                                    "ImageMode", sol::property([](UI::Image& i) { return i.GetStyle().Mode; }, [](UI::Image& i, UI::ImageMode v) { i.GetStyle().Mode = v; }),
                                    "FillAmount", sol::property([](UI::Image& i) { return i.GetStyle().FillAmount; }, [](UI::Image& i, float v) { i.GetStyle().FillAmount = v; }),
                                    "FillOrigin", sol::property([](UI::Image& i) { return i.GetStyle().Origin; }, [](UI::Image& i, UI::FillOrigin v) { i.GetStyle().Origin = v; }),
                                    "PreserveAspect", sol::property([](UI::Image& i) { return i.GetStyle().PreserveAspect; }, [](UI::Image& i, bool v) { i.GetStyle().PreserveAspect = v; }),
                                    "SliceBorder", sol::property([](UI::Image& i) { return i.GetStyle().SliceBorder; }, [](UI::Image& i, const glm::vec4& v) { i.GetStyle().SliceBorder = v; }),
                                    "SliceScale", sol::property([](UI::Image& i) { return i.GetStyle().SliceScale; }, [](UI::Image& i, float v) { i.GetStyle().SliceScale = v; }),
                                    "CornerRadius", sol::property([](UI::Image& i) { return i.GetStyle().CornerRadius; }, [](UI::Image& i, float v) { i.GetStyle().CornerRadius = v; }),
                                    "BorderWidth", sol::property([](UI::Image& i) { return i.GetStyle().BorderWidth; }, [](UI::Image& i, float v) { i.GetStyle().BorderWidth = v; }),
                                    "BorderColor", sol::property([](UI::Image& i) { return i.GetStyle().BorderColor; }, [](UI::Image& i, const glm::vec4& v) { i.GetStyle().BorderColor = v; })
        );

        lua.new_usertype<UI::Text>("UIText",
                                   sol::factories([](const String& text) {
                                       return Ref<UI::Text>::Create(text, Core::GetRenderer()->GetEngineFont());
                                   }, [](const String& text, const String& fontRelPath) {
                                       return Ref<UI::Text>::Create(text, LoadFont(fontRelPath, "UIText"));
                                   }),
                                   sol::base_classes, sol::bases<UI::Widget>(),
                                   "Text", sol::property(
                                       [](UI::Text& t) -> const String& { return t.GetTextBuffer(); },
                                       [](UI::Text& t, const String& val) { t.SetText(val); }
                                   ),
                                   "FontSize", sol::property(
                                       [](UI::Text& t) -> float { return t.GetFontSize(); },
                                       [](UI::Text& t, float val) { t.SetFontSize(val); }
                                   ),
                                   "TextAlignment", sol::property(
                                       [](UI::Text& t) -> TextAlignment { return t.GetTextAlignment(); },
                                       [](UI::Text& t, TextAlignment val) { t.SetTextAlignment(val); }
                                   ),
                                   "TextVAlignment", sol::property(
                                       [](UI::Text& t) -> TextVerticalAlignment { return t.GetTextVAlignment(); },
                                       [](UI::Text& t, TextVerticalAlignment val) { t.SetTextVAlignment(val); }
                                   ),
                                   "WordWrap", sol::property(
                                       [](UI::Text& t) -> bool { return t.GetWordWrap(); },
                                       [](UI::Text& t, bool val) { t.SetWordWrap(val); }
                                   ),
                                   "Overflow", sol::property(
                                       [](UI::Text& t) -> UI::TextOverflow { return t.GetOverflow(); },
                                       [](UI::Text& t, UI::TextOverflow val) { t.SetOverflow(val); }
                                   )
        );

        lua.new_usertype<UI::Button>("UIButton",
                                     sol::factories([](const String& text, const String& fontRelPath, const String& textureRelPath) {
                                         Ref<UI::Button> button = Ref<UI::Button>::Create(text, LoadFont(fontRelPath, "UIButton"));
                                         SetStyleTexture(button->GetStyle(), textureRelPath);
                                         return button;
                                     }),
                                     sol::base_classes, sol::bases<UI::Widget, UI::Image>(),
                                     "NormalColor", BIND_PROP(UI::Button, NormalColor),
                                     "HoverColor", BIND_PROP(UI::Button, HoverColor),
                                     "PressedColor", BIND_PROP(UI::Button, PressedColor),
                                     "GetText", [](UI::Button& btn) -> UI::Text* {
                                         return btn.GetTextWidget().get();
                                     }
        );

        lua.new_usertype<UI::ImageButton>("UIImageButton",
                                          sol::factories([](const String& textureRelPath) {
                                              Ref<UI::ImageButton> button = Ref<UI::ImageButton>::Create();
                                              SetStyleTexture(button->GetStyle(), textureRelPath);
                                              return button;
                                          }),
                                          sol::base_classes, sol::bases<UI::Widget, UI::Image>(),
                                          "NormalColor", BIND_PROP(UI::ImageButton, NormalColor),
                                          "HoverColor", BIND_PROP(UI::ImageButton, HoverColor),
                                          "PressedColor", BIND_PROP(UI::ImageButton, PressedColor)
        );

        lua.new_usertype<UI::ProgressBar>("UIProgressBar",
                                          sol::factories([]() { return Ref<UI::ProgressBar>::Create(); }),
                                          sol::base_classes, sol::bases<UI::Widget>(),
                                          "Value", sol::property(&UI::ProgressBar::GetValue, &UI::ProgressBar::SetValue),
                                          "NormalizedValue", sol::readonly_property(&UI::ProgressBar::GetNormalizedValue),
                                          "Min", BIND_PROP(UI::ProgressBar, Min),
                                          "Max", BIND_PROP(UI::ProgressBar, Max),
                                          "Direction", BIND_PROP(UI::ProgressBar, Direction),
                                          "FillPadding", BIND_PROP(UI::ProgressBar, FillPadding),
                                          "BackgroundColor", BIND_PROP(UI::ProgressBar, BackgroundColor),
                                          "FillColor", BIND_PROP(UI::ProgressBar, FillColor)
        );

        lua.new_usertype<UI::Toggle>("UIToggle",
                                     sol::factories([](const String& label, const String& fontRelPath) {
                                         return Ref<UI::Toggle>::Create(label, LoadFont(fontRelPath, "UIToggle"));
                                     }),
                                     sol::base_classes, sol::bases<UI::Widget>(),
                                     "IsOn", sol::property(&UI::Toggle::IsOn, [](UI::Toggle& t, bool on) { t.SetOn(on, true); }),
                                     "Group", BIND_PROP(UI::Toggle, Group),
                                     "AllowSwitchOff", BIND_PROP(UI::Toggle, AllowSwitchOff),
                                     "Style", BIND_PROP(UI::Toggle, Style),
                                     "BoxColor", BIND_PROP(UI::Toggle, BoxColor),
                                     "CheckColor", BIND_PROP(UI::Toggle, CheckColor),
                                     "KnobColor", BIND_PROP(UI::Toggle, KnobColor),
                                     "GetLabel", [](UI::Toggle& t) -> UI::Text* { return t.GetLabel().get(); }
        );

        lua.new_usertype<UI::Slider>("UISlider",
                                     sol::factories([]() { return Ref<UI::Slider>::Create(); }),
                                     sol::base_classes, sol::bases<UI::Widget>(),
                                     "Value", sol::property(&UI::Slider::GetValue, [](UI::Slider& s, float v) { s.SetValue(v, true); }),
                                     "NormalizedValue", sol::readonly_property(&UI::Slider::GetNormalizedValue),
                                     "Min", BIND_PROP(UI::Slider, Min),
                                     "Max", BIND_PROP(UI::Slider, Max),
                                     "WholeNumbers", BIND_PROP(UI::Slider, WholeNumbers),
                                     "Direction", BIND_PROP(UI::Slider, Direction),
                                     "HandleSize", BIND_PROP(UI::Slider, HandleSize),
                                     "TrackThickness", BIND_PROP(UI::Slider, TrackThickness),
                                     "TrackColor", BIND_PROP(UI::Slider, TrackColor),
                                     "FillColor", BIND_PROP(UI::Slider, FillColor),
                                     "HandleColor", BIND_PROP(UI::Slider, HandleColor)
        );

        lua.new_usertype<UI::Joystick>("UIJoystick",
                                       sol::factories([]() { return Ref<UI::Joystick>::Create(); }),
                                       sol::base_classes, sol::bases<UI::Widget>(),
                                       "Direction", sol::readonly_property(&UI::Joystick::GetDirection),
                                       "Magnitude", sol::readonly_property(&UI::Joystick::GetMagnitude),
                                       "IsActive", sol::readonly_property(&UI::Joystick::IsActive),
                                       "Mode", BIND_PROP(UI::Joystick, Mode),
                                       "BaseSize", BIND_PROP(UI::Joystick, BaseSize),
                                       "HandleSize", BIND_PROP(UI::Joystick, HandleSize),
                                       "DeadZone", BIND_PROP(UI::Joystick, DeadZone),
                                       "HandleRange", BIND_PROP(UI::Joystick, HandleRange),
                                       "HideWhenIdle", BIND_PROP(UI::Joystick, HideWhenIdle),
                                       "BaseColor", BIND_PROP(UI::Joystick, BaseColor),
                                       "KnobColor", BIND_PROP(UI::Joystick, KnobColor)
        );

        lua.new_usertype<UI::Spinner>("UISpinner",
                                      sol::factories([]() { return Ref<UI::Spinner>::Create(); }),
                                      sol::base_classes, sol::bases<UI::Widget>(),
                                      "DotCount", BIND_PROP(UI::Spinner, DotCount),
                                      "DotSize", BIND_PROP(UI::Spinner, DotSize),
                                      "Speed", BIND_PROP(UI::Spinner, Speed)
        );

        lua.new_usertype<UI::SafeArea>("UISafeArea",
                                       sol::factories([]() { return Ref<UI::SafeArea>::Create(); }),
                                       sol::base_classes, sol::bases<UI::Widget>(),
                                       "Edges", BIND_PROP(UI::SafeArea, Edges)
        );

        lua.new_usertype<UI::LayoutGroup>("UILayoutGroup",
                                          sol::factories([]() { return Ref<UI::LayoutGroup>::Create(); }),
                                          sol::base_classes, sol::bases<UI::Widget>(),
                                          "Direction", BIND_PROP(UI::LayoutGroup, Direction),
                                          "Padding", BIND_PROP(UI::LayoutGroup, Padding),
                                          "Spacing", BIND_PROP(UI::LayoutGroup, Spacing),
                                          "CellSize", BIND_PROP(UI::LayoutGroup, CellSize),
                                          "Columns", BIND_PROP(UI::LayoutGroup, Columns),
                                          "ChildAlignment", BIND_PROP(UI::LayoutGroup, ChildAlignment),
                                          "ControlChildWidth", BIND_PROP(UI::LayoutGroup, ControlChildWidth),
                                          "ControlChildHeight", BIND_PROP(UI::LayoutGroup, ControlChildHeight),
                                          "FitContent", BIND_PROP(UI::LayoutGroup, FitContent)
        );

        // Tabs and dropdown options are 1 based in Lua (0 = nothing selected)
        lua.new_usertype<UI::TabView>("UITabView",
                                      sol::factories([](const String& fontRelPath) {
                                          Ref<UI::TabView> tabs = Ref<UI::TabView>::Create();
                                          tabs->TextFont = LoadFont(fontRelPath, "UITabView");
                                          return tabs;
                                      }),
                                      sol::base_classes, sol::bases<UI::Widget>(),
                                      "ActiveTab", sol::property([](UI::TabView& t) { return t.GetActiveTab() + 1; }, [](UI::TabView& t, int index) { t.SetActiveTab(index - 1, true); }),
                                      "TabsAtBottom", BIND_PROP(UI::TabView, TabsAtBottom),
                                      "TabBarHeight", BIND_PROP(UI::TabView, TabBarHeight),
                                      "FontSize", BIND_PROP(UI::TabView, FontSize),
                                      "BarColor", BIND_PROP(UI::TabView, BarColor),
                                      "ActiveTabColor", BIND_PROP(UI::TabView, ActiveTabColor),
                                      "TextColor", BIND_PROP(UI::TabView, TextColor),
                                      "ActiveTextColor", BIND_PROP(UI::TabView, ActiveTextColor)
        );

        lua.new_usertype<UI::ScrollView>("UIScrollView",
                                         sol::factories([]() { return Ref<UI::ScrollView>::Create(); }),
                                         sol::base_classes, sol::bases<UI::Widget>(),
                                         "ScrollPosition", sol::property(&UI::ScrollView::GetScrollPosition, &UI::ScrollView::SetScrollPosition),
                                         "ScrollOffset", sol::readonly_property(&UI::ScrollView::GetScrollOffset),
                                         "ContentSize", sol::readonly_property(&UI::ScrollView::GetContentSize),
                                         "Horizontal", BIND_PROP(UI::ScrollView, Horizontal),
                                         "Vertical", BIND_PROP(UI::ScrollView, Vertical),
                                         "DecelerationRate", BIND_PROP(UI::ScrollView, DecelerationRate),
                                         "ShowScrollbars", BIND_PROP(UI::ScrollView, ShowScrollbars),
                                         "ScrollbarColor", BIND_PROP(UI::ScrollView, ScrollbarColor)
        );

        lua.new_usertype<UI::ListView>("UIListView",
                                       sol::factories([]() { return Ref<UI::ListView>::Create(); }),
                                       sol::base_classes, sol::bases<UI::Widget, UI::ScrollView>(),
                                       "ItemCount", sol::property(&UI::ListView::GetItemCount, &UI::ListView::SetItemCount),
                                       "Refresh", &UI::ListView::Refresh,
                                       "ScrollToIndex", [](UI::ListView& list, int index) { list.ScrollToIndex(index - 1); },
                                       "OnBindItem", [](UI::ListView& list, sol::protected_function f) { list.SetOnBindItem(Ref<LuaListCallback>::Create(f)); },
                                       "OnCreateItem", [](UI::ListView& list, sol::protected_function f) { list.SetOnCreateItem(Ref<LuaListCallback>::Create(f)); },
                                       "ItemSize", BIND_PROP(UI::ListView, ItemSize),
                                       "Spacing", BIND_PROP(UI::ListView, Spacing),
                                       "Padding", BIND_PROP(UI::ListView, Padding),
                                       "Columns", BIND_PROP(UI::ListView, Columns)
        );

        lua.new_usertype<UI::InputField>("UIInputField",
                                         sol::factories([](const String& fontRelPath) { return Ref<UI::InputField>::Create(LoadFont(fontRelPath, "UIInputField")); }),
                                         sol::base_classes, sol::bases<UI::Widget>(),
                                         "Text", sol::property([](UI::InputField& f) -> String { return f.GetText(); }, [](UI::InputField& f, const String& text) { f.SetText(text, true); }),
                                         "IsFocused", sol::readonly_property(&UI::InputField::IsFocused),
                                         "Focus", &UI::InputField::Focus,
                                         "Blur", &UI::InputField::Blur,
                                         "OnSubmit", [](UI::InputField& f, sol::protected_function fn) { f.SetOnSubmit(Ref<LuaEventCallback>::Create(fn, &f)); },
                                         "OnFocus", [](UI::InputField& f, sol::protected_function fn) { f.SetOnFocus(Ref<LuaEventCallback>::Create(fn, &f)); },
                                         "OnBlur", [](UI::InputField& f, sol::protected_function fn) { f.SetOnBlur(Ref<LuaEventCallback>::Create(fn, &f)); },
                                         "Placeholder", BIND_PROP(UI::InputField, Placeholder),
                                         "ContentType", BIND_PROP(UI::InputField, ContentType),
                                         "MultiLine", BIND_PROP(UI::InputField, MultiLine),
                                         "MaxLength", BIND_PROP(UI::InputField, MaxLength),
                                         "FontSize", BIND_PROP(UI::InputField, FontSize),
                                         "TextColor", BIND_PROP(UI::InputField, TextColor),
                                         "PlaceholderColor", BIND_PROP(UI::InputField, PlaceholderColor),
                                         "BackgroundColor", BIND_PROP(UI::InputField, BackgroundColor)
        );

        lua.new_usertype<UI::Dropdown>("UIDropdown",
                                       sol::factories([](const String& fontRelPath) { return Ref<UI::Dropdown>::Create(LoadFont(fontRelPath, "UIDropdown")); }),
                                       sol::base_classes, sol::bases<UI::Widget>(),
                                       "Options", sol::property(
                                           [](UI::Dropdown& d, sol::this_state s) {
                                               sol::state_view state(s);
                                               sol::table options = state.create_table();
                                               for(const String& option : d.GetOptions())
                                                   options.add(option);
                                               return options;
                                           },
                                           [](UI::Dropdown& d, sol::table table) {
                                               Vector<String> options;
                                               for(size_t i = 1; i <= table.size(); i++)
                                               {
                                                   sol::optional<String> option = table[i];
                                                   options.push_back(option ? *option : String());
                                               }
                                               d.SetOptions(options);
                                           }
                                       ),
                                       "SelectedIndex", sol::property([](UI::Dropdown& d) { return d.GetSelectedIndex() + 1; }, [](UI::Dropdown& d, int index) { d.SetSelectedIndex(index - 1, true); }),
                                       "SelectedText", sol::readonly_property(&UI::Dropdown::GetSelectedText),
                                       "IsOpen", sol::readonly_property(&UI::Dropdown::IsOpen),
                                       "Open", &UI::Dropdown::Open,
                                       "Close", &UI::Dropdown::Close,
                                       "Placeholder", BIND_PROP(UI::Dropdown, Placeholder),
                                       "MaxVisibleItems", BIND_PROP(UI::Dropdown, MaxVisibleItems),
                                       "FontSize", BIND_PROP(UI::Dropdown, FontSize),
                                       "TextColor", BIND_PROP(UI::Dropdown, TextColor),
                                       "BackgroundColor", BIND_PROP(UI::Dropdown, BackgroundColor)
        );

        lua.new_usertype<UI::Modal>("UIModal",
                                    sol::factories([]() { return Ref<UI::Modal>::Create(); }),
                                    sol::base_classes, sol::bases<UI::Widget>(),
                                    "Show", &UI::Modal::Show,
                                    "Hide", &UI::Modal::Hide,
                                    "IsOpen", sol::readonly_property(&UI::Modal::IsOpen),
                                    "OnClose", [](UI::Modal& m, sol::protected_function fn) { m.SetOnClose(Ref<LuaEventCallback>::Create(fn, &m)); },
                                    "BackdropColor", BIND_PROP(UI::Modal, BackdropColor),
                                    "CloseOnBackdrop", BIND_PROP(UI::Modal, CloseOnBackdrop),
                                    "CloseOnBack", BIND_PROP(UI::Modal, CloseOnBack),
                                    "PanelColor", BIND_PROP(UI::Modal, PanelColor)
        );
    }
}
