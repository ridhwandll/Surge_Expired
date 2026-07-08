// Copyright (c) - SurgeTechnologies - All rights reserved
#include "UIBindings.hpp"
#include "Surge/ScriptEngine/Lua.hpp"

#include "Surge/Core/Core.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/UISystem/UIManager.hpp"
#include "Surge/Graphics/UISystem/UIWidgets.hpp"

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

#define BIND_PROP(COMP, PROP) \
        sol::property( \
            [](COMP& c) -> decltype(COMP::PROP) { return c.PROP; }, \
            [](COMP& c, const decltype(COMP::PROP)& val) { c.PROP = val; } \
        )

namespace Surge::ScriptBinding
{
    class LuaEventCallback : public UI::IEventCallback
    {
    public:
        LuaEventCallback(sol::protected_function func)
            : mFunc(std::move(func)) {}

        void Invoke() override
        {
            if(mFunc.valid())
            {
                auto result = mFunc();
                if(!result.valid())
                {
                    sol::error err = result;
                    Log<Severity::Error>("Lua UI Callback Error: {}", err.what());
                }
            }
        }
    private:
        sol::protected_function mFunc;
    };

    void BindUIWidgets(void* luaState)
    {
        sol::state_view& lua = *static_cast<sol::state_view*>(luaState);

        lua.set_function("SetUIRoot", [](UI::Widget* root) {
            UI::Manager & uiManager = Core::GetRenderer()->GetUIManager();
            root ? uiManager.SetRoot(Ref<UI::Widget>(root)) : uiManager.ClearRoot();
        });

        //sol::factories enables this in Lua: `local widget = UIWidget.new()`
        lua.new_usertype<UI::Widget>("UIWidget", sol::factories([]() { return Ref<UI::Widget>::Create(); }),
                                     "AddChild", [](UI::Widget& parent, UI::Widget* child) {
                                         if(child)
                                             parent.AddChild(Ref<UI::Widget>(child));
                                     },
                                     "OnClick",      [](UI::Widget& w, sol::protected_function f) { w.SetOnClick(Ref<LuaEventCallback>::Create(f)); },
                                     "OnHoverEnter", [](UI::Widget& w, sol::protected_function f) { w.SetOnHoverEnter(Ref<LuaEventCallback>::Create(f)); },
                                     "OnHoverExit",  [](UI::Widget& w, sol::protected_function f) { w.SetOnHoverExit(Ref<LuaEventCallback>::Create(f)); },
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
                                        ImageHandle handle = ImageHandle::Invalid();
                                        if(id)
                                        {
                                            Ref<Texture2D> texture = am->Load<Texture2D>(id);
                                            if(texture) handle = texture->GetRHIImage();
                                            else        Log<Severity::Warn>("[UIBindings] Lua: UIImage: Failed to load texture at path {}", textureRelPath);
                                        }
                                        return Ref<UI::Image>::Create(handle);
                                    }),
                                    sol::base_classes, sol::bases<UI::Widget>()
        );

        lua.new_usertype<UI::Text>("UIText",
                                   sol::factories([](const String& text, const String& fontRelPath) {
                                       AssetManager* am = Core::GetAssetManager();
                                       AssetID id = am->GetIDFromPath(fontRelPath);
                                       if(id)
                                           return Ref<UI::Text>::Create(text, id);

                                       Log<Severity::Warn>("[UIBindings] Lua: UIText: Failed to load font at path {}", fontRelPath);
                                       return Ref<UI::Text>::Create(text, AssetID::INVALID);
                                   }),
                                   sol::base_classes, sol::bases<UI::Widget>(),
                                   "Text", sol::property(
                                       [](UI::Text& t) -> String& { return t.GetTextBuffer(); },
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
                                                         Log<Severity::Warn>("[UIBindings] Lua: UIButton: Failed to load texture at path {}", textureRelPath);
                                                 }
                                             }
                                             {
                                                 AssetID id = am->GetIDFromPath(fontRelPath);
                                                 if(id)
                                                     fontID = id;
                                                 else
                                                     Log<Severity::Warn>("[UIBindings] Lua: UIButton: Failed to load font at path {}", fontRelPath);
                                             }
                                             return Ref<UI::Button>::Create(text, fontID, textureId);
                                     }),
                                     sol::base_classes, sol::bases<UI::Widget, UI::Image>(),
                                     "NormalColor", BIND_PROP(UI::Button, NormalColor),
                                     "HoverColor", BIND_PROP(UI::Button, HoverColor),
                                     "PressedColor", BIND_PROP(UI::Button, PressedColor)
        );
    }
}