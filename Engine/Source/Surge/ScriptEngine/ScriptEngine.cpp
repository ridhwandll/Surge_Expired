// Copyright (c) - SurgeTechnologies - All rights reserved
#include "ScriptEngine.hpp"
#include "Surge/Core/Defines.hpp"
#include "Surge/ScriptEngine/Lua.hpp"
#include "Bindings/MathBinding.hpp"
#include "Bindings/LogBinding.hpp"

namespace Surge
{
    static sol::state_view* sLua = nullptr;

    static void* LuaAllocator(void* /*ud*/, void* ptr, size_t /*osize*/, size_t nsize)
    {
        if(nsize == 0)
        {
            ::free(ptr);
            return nullptr;
        }
        return ::realloc(ptr, nsize);
    }

    static int LuaPanic(lua_State* L)
    {
        const char* msg = lua_tostring(L, -1);
        Log<Severity::Fatal>("ScriptEngine Lua panic: unprotected error: {}", msg ? msg : "(no message)");
        SG_ASSERT_INTERNAL("Unrecoverable Lua panic!");
        return 0;  // Unreachable
    }

    void ScriptEngine::Initialize()
    {
        lua_State* L = lua_newstate(LuaAllocator, nullptr);
        SG_ASSERT(L, "Failed to create Lua state!");

        lua_atpanic(L, LuaPanic);
        sLua = new sol::state_view(L);

        sLua->open_libraries(
            sol::lib::base,
            sol::lib::math,
            sol::lib::string,
            sol::lib::table,
            sol::lib::coroutine
        );

        (*sLua)["print"] = [](sol::variadic_args args) {
            String output;
            for(auto arg : args)
            {
                output += arg.as<String>();
                output += "\t";
            }
            Log<Severity::Info>("[ScriptEngine] Lua: {}", output);
            };

        lua_gc(L, LUA_GCGEN, 100, 200);

        ScriptBinding::BindLog(sLua);
        ScriptBinding::BindMath(sLua);

        Log<Severity::Info>("ScriptEngine Initialized: {}", LUA_VERSION);

        // Test
        auto result = sLua->safe_script(R"(
        Log.Trace("Hello from LUA!")
        local rgb = Math.HexToRGB("#FF0000")
        Log.Trace("Red in RGB: " ..tostring(rgb))
        local hsv = Math.RGBToHSV(rgb)
        Log.Trace("Red in HSV: " ..tostring(hsv))
        )", sol::script_pass_on_error);

        if(!result.valid())
        {
            sol::error err = result;
            Log<Severity::Fatal>("[ScriptEngine] Math Test Suite Failed: {}", err.what());
            SG_ASSERT_INTERNAL("Lua Math Bindings are broken!");
        }
    }

    void ScriptEngine::Shutdown()
    {
        SG_ASSERT(sLua, "ScriptEngine not initialized or already shutdown!");
        delete sLua;
        sLua = nullptr;
    }

    void* ScriptEngine::GetSOLState() const
    {
        return sLua;
    }
}
