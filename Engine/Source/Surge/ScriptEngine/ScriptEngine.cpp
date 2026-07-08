// Copyright (c) - SurgeTechnologies - All rights reserved
#include "ScriptEngine.hpp"
#include "Surge/Core/Defines.hpp"
#include "Surge/ScriptEngine/Lua.hpp"

#include "Bindings/MathBinding.hpp"
#include "Bindings/InputBinding.hpp"
#include "Bindings/LogBinding.hpp"
#include "Bindings/ECSBindings.hpp"
#include "Bindings/UIBindings.hpp"

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
            sol::lib::os,
            sol::lib::package,
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
        ScriptBinding::BindInput(sLua);
        ScriptBinding::BindMath(sLua);
        ScriptBinding::BindEntity(sLua);
        ScriptBinding::BindComponents(sLua);
        ScriptBinding::BindUIWidgets(sLua);

        Log<Severity::Info>("ScriptEngine Initialized: {}", LUA_VERSION);
    }

    void ScriptEngine::Shutdown()
    {
        SG_ASSERT(sLua, "ScriptEngine not initialized or already shutdown!");
        delete sLua;
        sLua = nullptr;
    }

    static int BytecodeWriter(lua_State*, const void* p, size_t sz, void* ud)
    {
        auto* bytecode = static_cast<Vector<Byte>*>(ud);
        const auto* data = static_cast<const Byte*>(p);
        bytecode->insert(bytecode->end(), data, data + sz);
        return 0;
    }

    Vector<Byte> ScriptEngine::Compile(const String& source, const String& chunkName)
    {
        lua_State* L = luaL_newstate();
        if(luaL_loadbuffer(L, source.c_str(), source.length(), chunkName.c_str()) != LUA_OK)
        {
            String errorMsg = lua_tostring(L, -1);
            Log<Severity::Error>("[ScriptEngine] Compile Error: {}", errorMsg);
            lua_close(L);
            return {};
        }

        Vector<Byte> bytecode;
        int stripDebugInfo = 0; // 1 = Strips out variable names and line numbers to save memory
        lua_dump(L, BytecodeWriter, &bytecode, stripDebugInfo);

        lua_close(L);
        return bytecode;
    }

    void* ScriptEngine::GetSOLState() const
    {
        return sLua;
    }
}
