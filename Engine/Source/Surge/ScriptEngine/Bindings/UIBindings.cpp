// Copyright (c) - SurgeTechnologies - All rights reserved
#include "UIBindings.hpp"
#include "Surge/ScriptEngine/Lua.hpp"

#include "Surge/Core/Core.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/UISystem/UIManager.hpp"
#include "Surge/Graphics/UISystem/UIWidgets.hpp"
#include "Surge/ECS/Components/ScriptComponent.hpp"
#include "BindingUtils.hpp"

namespace sol
{
    template <typename T>
    struct unique_usertype_traits<Surge::Ref<T>>
    {
        typedef T type;
        typedef Surge::Ref<T> actual_type;
        static const bool is_shared = true;
        static bool is_null(const actual_type& ptr) { return !ptr; }
        static type* get(const actual_type& ptr) { return const_cast<type*>(ptr.get()); }
    };
}

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
            case UI::WidgetType::BASE_WIDGET:  break;
        }
        return sol::make_object(lua, Ref<UI::Widget>(widget));
    }

    class LuaEventCallback : public UI::IEventCallback
    {
    public:
        LuaEventCallback(sol::protected_function func, UI::Widget* self)
            : mFunc(std::move(func)), mSelf(self) {}

        void Invoke() override
        {
            if(mFunc.valid())
            {
                sol::protected_function_result result = mFunc(ToTypedLuaObject(mFunc.lua_state(), mSelf));
                if(!result.valid())
                {
                    sol::error err = result;
                    Log<Severity::Error>("Lua UI Callback Error: {}", err.what());
                }
            }
        }
    private:
        sol::protected_function mFunc;
        UI::Widget* mSelf;
    };

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
                                     "OnClick",      [](UI::Widget& w, sol::protected_function f) { w.SetOnClick(Ref<LuaEventCallback>::Create(f, &w)); },
                                     "OnHoverEnter", [](UI::Widget& w, sol::protected_function f) { w.SetOnHoverEnter(Ref<LuaEventCallback>::Create(f, &w)); },
                                     "OnHoverExit",  [](UI::Widget& w, sol::protected_function f) { w.SetOnHoverExit(Ref<LuaEventCallback>::Create(f, &w)); },
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
                                        AssetManager* am = Core::GetAssetManager();
                                        AssetID id = am->GetIDFromPath(textureRelPath);
                                        Ref<UI::Image> image = Ref<UI::Image>::Create();
                                        if(id)
                                        {
                                            Ref<Texture2D> texture = am->Load<Texture2D>(id);
                                            if(texture) image->SetTexture(texture->GetRHIImage(), texture);
                                            else        Log<Severity::Warn>("[UIBindings.cpp] Lua: UIImage: Failed to load texture at path {}", textureRelPath);
                                        }
                                        return image;
                                    }),
                                    sol::base_classes, sol::bases<UI::Widget>()
        );

        lua.new_usertype<UI::Text>("UIText",
                                   sol::factories([](const String& text, const String& fontRelPath) {
                                       AssetManager* am = Core::GetAssetManager();
                                       AssetID id = am->GetIDFromPath(fontRelPath);
                                       if(id)
                                           return Ref<UI::Text>::Create(text, id);

                                       Log<Severity::Warn>("[UIBindings.cpp] Lua: UIText: Failed to load font at path {}", fontRelPath);
                                       return Ref<UI::Text>::Create(text, AssetID::INVALID);
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
                                   )
        );

        lua.new_usertype<UI::Button>("UIButton",
                                     sol::factories([](const String& text, const String& fontRelPath, const String& textureRelPath) {

                                             AssetID fontID = AssetID::INVALID;
                                             ImageHandle textureId = ImageHandle::Invalid();

                                             AssetManager* am = Core::GetAssetManager();
                                             {
                                                 AssetID id = am->GetIDFromPath(textureRelPath);
                                                 if(id)
                                                 {
                                                     Ref<Texture2D> texture = am->Load<Texture2D>(id);
                                                     if(texture)
                                                         textureId = texture->GetRHIImage();
                                                     else
                                                         Log<Severity::Warn>("[UIBindings.cpp] Lua: UIButton: Failed to load texture at path {}", textureRelPath);
                                                 }
                                             }
                                             {
                                                 AssetID id = am->GetIDFromPath(fontRelPath);
                                                 if(id)
                                                     fontID = id;
                                                 else
                                                     Log<Severity::Warn>("[UIBindings.cpp] Lua: UIButton: Failed to load font at path {}", fontRelPath);
                                             }
                                             return Ref<UI::Button>::Create(text, fontID, textureId);
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
                                              AssetManager* am = Core::GetAssetManager();
                                              Ref<UI::ImageButton> button = Ref<UI::ImageButton>::Create();
                                              AssetID id = am->GetIDFromPath(textureRelPath);
                                              if(id)
                                              {
                                                  Ref<Texture2D> texture = am->Load<Texture2D>(id);
                                                  if(texture) button->SetTexture(texture->GetRHIImage(), texture);
                                                  else        Log<Severity::Warn>("[UIBindings.cpp] Lua: UIImageButton: Failed to load texture at path {}", textureRelPath);
                                              }
                                              return button;
                                          }),
                                          sol::base_classes, sol::bases<UI::Widget, UI::Image>(),
                                          "NormalColor", BIND_PROP(UI::ImageButton, NormalColor),
                                          "HoverColor", BIND_PROP(UI::ImageButton, HoverColor),
                                          "PressedColor", BIND_PROP(UI::ImageButton, PressedColor)
        );
    }
}