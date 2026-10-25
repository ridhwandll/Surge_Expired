// Copyright (c) - SurgeTechnologies - All rights reserved
#include "ScriptEngine.hpp"
#include "Surge/Core/Defines.hpp"
#include "Surge/ScriptEngine/Lua.hpp"
#include "Surge/ScriptEngine/ScriptAsset.hpp"

#include "Bindings/MathBinding.hpp"
#include "Bindings/InputBinding.hpp"
#include "Bindings/LogBinding.hpp"
#include "Bindings/ECSBindings.hpp"
#include "Bindings/UIBindings.hpp"
#include "Bindings/SceneBinding.hpp"
#include "Bindings/StorageBinding.hpp"
#include "Bindings/ParticleBindings.hpp"
#include "Bindings/AnimationBindings.hpp"
#include "Bindings/DiagnosticsBindings.hpp"
#include "Builtin/PerformanceOverlay.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Profiler.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/ECS/Scene.hpp"
#include "Surge/ECS/Components/ScriptComponent.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace Surge
{
    static sol::state_view* sLua = nullptr;
    struct LuaSessionData
    {
        sol::table BaselineGlobals;       // Every global that exists after Initialize(), the rest was added by scripts
        Vector<String> LoadedModuleNames; // package.loaded keys set through the asset searcher
        int ModuleCacheRef = LUA_NOREF;   // Registry table: canonical asset path -> module value
    };
    static LuaSessionData* sSession = nullptr;

    // Runtime side of the Properties declarations: plain values, so a script reads Properties.X like any table field.
    // The Editor swaps in recording versions of these to learn the types, ranges and tooltips (Editor/Source/Scripting/ScriptFields)
    static constexpr const char* PROPERTY_HELPERS = R"(
Property = {
    Float  = function(value) return value or 0.0 end,
    Int    = function(value) return value or 0 end,
    Bool   = function(value) return value or false end,
    String = function(value) return value or "" end,
    Vec2   = function(x, y) return Math.Vec2.new(x or 0, y or 0) end,
    Vec3   = function(x, y, z) return Math.Vec3.new(x or 0, y or 0, z or 0) end,
    Vec4   = function(x, y, z, w) return Math.Vec4.new(x or 0, y or 0, z or 0, w or 0) end,
    Color  = function(r, g, b, a) return Math.Vec4.new(r or 1, g or 1, b or 1, a or 1) end,
    Entity = function() return nil end,
    Asset  = function(kind, path) return path end,
    Enum   = function(value, options) return value or (options and options[1]) end,
    Array  = function(elementType, values) return values or {} end,
}
)";

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

    // Error handler of every protected call: appends the Lua traceback. Only runs when an error is raised
    static int LuaMessageHandler(lua_State* L)
    {
        const char* message = lua_tostring(L, 1); // (Rid) The error object is placed at index 1 (at the bottom of the stack)
        if(!message)
            message = lua_pushfstring(L, "(error object is a %s value)", luaL_typename(L, 1));
        luaL_traceback(L, L, message, 1);
        return 1;
    }

    // print(...) into the engine log, any value type (tostring semantics)
    static int LuaPrint(lua_State* L)
    {
        const int count = lua_gettop(L);
        luaL_Buffer buffer;
        luaL_buffinit(L, &buffer);
        for(int i = 1; i <= count; i++)
        {
            luaL_tolstring(L, i, nullptr);
            luaL_addvalue(&buffer);
            if(i < count)
                luaL_addchar(&buffer, '\t');
        }
        luaL_pushresult(&buffer);
        Log<Severity::Info>("[ScriptEngine] Lua: {}", lua_tostring(L, -1));
        return 0;
    }

    // require(): module loader made by the searcher, upvalues: chunk, environment, canonical path
    static int LuaLoadModule(lua_State* L)
    {
        lua_pushvalue(L, lua_upvalueindex(1));
        lua_pushvalue(L, 1);
        lua_pushvalue(L, 2);
        lua_call(L, 2, 1);
        if(lua_isnil(L, -1)) // No return value: the module's globals are the module
        {
            lua_pop(L, 1);
            lua_pushvalue(L, lua_upvalueindex(2));
        }

        // Cached by canonical path, so "Scripts.State" and "Scripts/State.lua" share one module
        lua_rawgeti(L, LUA_REGISTRYINDEX, sSession->ModuleCacheRef);
        lua_pushvalue(L, -2);
        lua_setfield(L, -2, lua_tostring(L, lua_upvalueindex(3)));
        lua_pop(L, 1);
        return 1;
    }

    static int LuaReturnCachedModule(lua_State* L)
    {
        lua_pushvalue(L, lua_upvalueindex(1));
        return 1;
    }

    struct ModuleLoaderAccess
    {
        // package.searchers[2]: finds the module among the script assets. Returns an error string (never raises), C++ objects live here.
        // require() adds the "\n\t" separator in front of each searcher message itself
        static int Searcher(lua_State* L)
        {
            const char* name = lua_tostring(L, 1);
            if(!name)
            {
                lua_pushliteral(L, "module name must be a string");
                return 1;
            }

            String path, error;
            const AssetID id = Core::GetScriptEngine()->ResolveModule(name, path, error);
            if(!id)
            {
                lua_pushstring(L, error.c_str());
                return 1;
            }
            sSession->LoadedModuleNames.emplace_back(name);

            lua_rawgeti(L, LUA_REGISTRYINDEX, sSession->ModuleCacheRef);
            lua_getfield(L, -1, path.c_str());
            if(!lua_isnil(L, -1)) // Module already loaded, return it
            {
                lua_pushcclosure(L, LuaReturnCachedModule, 1);
                lua_pushstring(L, path.c_str());
                return 2;
            }
            lua_pop(L, 2);

            Ref<Script> script = Core::GetAssetManager()->Load<Script>(id);
            if(!script || script->GetBytecode().empty())
            {
                lua_pushfstring(L, "script asset '%s' failed to load via 'require' (compile error?)", path.c_str());
                return 1;
            }

            const Vector<Byte>& bytecode = script->GetBytecode();
            const String chunkName = "@" + path;
            if(luaL_loadbufferx(L, reinterpret_cast<const char*>(bytecode.data()), bytecode.size(), chunkName.c_str(), "b") != LUA_OK)
                return 1; // The load error message is on top

            // Own environment: the module's globals stay inside it, reads fall back to _G
            lua_newtable(L);
            lua_newtable(L);
            lua_pushglobaltable(L);
            lua_setfield(L, -2, "__index");
            lua_setmetatable(L, -2);
            lua_pushvalue(L, -1);
            lua_setupvalue(L, -3, 1); // _ENV is the first upvalue of a main chunk
            lua_pushstring(L, path.c_str());
            lua_pushcclosure(L, LuaLoadModule, 3);
            lua_pushstring(L, path.c_str());
            return 2;
        }
    };

    void ScriptEngine::Initialize()
    {
        lua_State* L = lua_newstate(LuaAllocator, nullptr);
        SG_ASSERT(L, "Failed to create Lua state!");

        lua_atpanic(L, LuaPanic);
        sLua = new sol::state_view(L);
        sSession = new LuaSessionData();

        sLua->open_libraries(
            sol::lib::base,
            sol::lib::math,
            sol::lib::string,
            sol::lib::table,
            sol::lib::utf8,
            sol::lib::os,
            sol::lib::package,
            sol::lib::coroutine
        );

        // Before any function reference is made: sol reads the default handler when a protected_function is created
        lua_pushcfunction(L, LuaMessageHandler);
        sol::protected_function::set_default_handler(sol::object(L, -1));
        lua_pop(L, 1);

        lua_register(L, "print", LuaPrint);

        // Scripts get time, not the process: no exit, shell, environment or file access
        sol::table os = (*sLua)["os"];
        for(const char* unsafe : { "execute", "exit", "getenv", "remove", "rename", "tmpname", "setlocale" })
            os[unsafe] = sol::lua_nil;

        // (Rid) Lua require() modification: only the asset searcher is used, no C modules or all-in-one loader.
        // Lua's default searchers are #1 = preload, #2 = Lua, #3 = C, #4 = all-in-one
        sol::table package = (*sLua)["package"];
        package["path"] = "";
        package["cpath"] = "";
        package["loadlib"] = sol::lua_nil;
        lua_getglobal(L, "package");
        lua_getfield(L, -1, "searchers");
        lua_pushnil(L);
        lua_rawseti(L, -2, 4); // Removes searcher[4] (#4)
        lua_pushnil(L);
        lua_rawseti(L, -2, 3); // Removes searcher[3] (#3)
        lua_pushcfunction(L, &ModuleLoaderAccess::Searcher);
        lua_rawseti(L, -2, 2); // Change searcher[2] to ModuleLoaderAccess::Searcher (#2)
        lua_pop(L, 2);

        lua_newtable(L);
        sSession->ModuleCacheRef = luaL_ref(L, LUA_REGISTRYINDEX);

        // Incremental, default pause/stepmul/stepsize: generational minor collections are stop-the-world (40-64 ms hitches on an A53/A73 phone)
        lua_gc(L, LUA_GCINC, 0, 0, 0);

        ScriptBinding::BindLog(sLua);
        ScriptBinding::BindInput(sLua);
        ScriptBinding::BindMath(sLua);
        ScriptBinding::BindEntity(sLua);
        ScriptBinding::BindComponents(sLua);
        ScriptBinding::BindParticles(sLua);
        ScriptBinding::BindAnimation(sLua);
        ScriptBinding::BindUIWidgets(sLua);
        ScriptBinding::BindScene(sLua);
        ScriptBinding::BindStorage(sLua);
        ScriptBinding::BindDiagnostics(sLua);

        sLua->safe_script(PROPERTY_HELPERS, "=PropertyHelpers");
        PerformanceOverlay::Initialize(sLua);

        // Everything that exists now belongs to the engine, ResetSession() removes the rest
        sSession->BaselineGlobals = sLua->create_table();
        for(const auto& [key, value] : sLua->globals())
            sSession->BaselineGlobals.raw_set(key, true);

        Log<Severity::Info>("ScriptEngine Initialized: {}", LUA_VERSION);
    }

    void ScriptEngine::Shutdown()
    {
        SG_ASSERT(sLua, "ScriptEngine not initialized or already shutdown!");

        PerformanceOverlay::Shutdown(); // Holds Lua functions and Lua made widgets

        // TODO: Remove from here
        // UI widgets can hold Lua callbacks (sol references), they must be released while the Lua state is still alive
        Core::GetRenderer()->GetUIManager().ClearCanvases();

        delete sSession;
        sSession = nullptr;

        // sLua is a non owning view, the state itself has to be closed explicitly. Closing runs __gc on every userdata still
        // held by Lua (widgets, textures, fonts...) so their GPU resources are released before the Renderer shuts down
        lua_State* L = sLua->lua_state();
        delete sLua;
        sLua = nullptr;
        lua_close(L);
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

    //////////////////////////////////////////////////////////////////////////
    // Script instances
    //////////////////////////////////////////////////////////////////////////

    static sol::protected_function GetCallback(const sol::environment& env, const char* name)
    {
        sol::object callback = env.raw_get<sol::object>(name);
        return callback.get_type() == sol::type::function ? callback.as<sol::protected_function>() : sol::protected_function();
    }

    static bool IsFieldCompatible(const sol::object& current, ScriptFieldType type)
    {
        switch(type)
        {
            case ScriptFieldType::ASSET:  return current.get_type() == sol::type::lua_nil || current.get_type() == sol::type::string;
            case ScriptFieldType::ENUM:   return current.get_type() == sol::type::string;
            case ScriptFieldType::ARRAY:  return current.get_type() == sol::type::lua_nil || current.get_type() == sol::type::table;
            case ScriptFieldType::FLOAT:
            case ScriptFieldType::INT:
                return current.get_type() == sol::type::number;
            case ScriptFieldType::BOOL:   return current.get_type() == sol::type::boolean;
            case ScriptFieldType::STRING: return current.get_type() == sol::type::string;
            case ScriptFieldType::VEC2:   return current.is<glm::vec2>();
            case ScriptFieldType::VEC3:   return current.is<glm::vec3>();
            case ScriptFieldType::VEC4:
            case ScriptFieldType::COLOR:
                return current.is<glm::vec4>();
            case ScriptFieldType::ENTITY: return current.get_type() == sol::type::lua_nil || current.is<Entity>();
            default: return false;
        }
    }

    // Lua value of a saved field. Missing entities/assets become nil
    static sol::object MakeFieldValue(const String& name, ScriptFieldType type, ScriptFieldType elementType, const ScriptFieldValue& value, Scene* scene)
    {
        sol::state_view& lua = *sLua;
        switch(type)
        {
            case ScriptFieldType::FLOAT: return sol::make_object(lua, value.Number);
            case ScriptFieldType::INT:   return sol::make_object(lua, static_cast<lua_Integer>(std::llround(value.Number)));
            case ScriptFieldType::BOOL:  return sol::make_object(lua, value.Bool);
            case ScriptFieldType::STRING:
            case ScriptFieldType::ENUM:
                return sol::make_object(lua, value.Text);
            case ScriptFieldType::VEC2:  return sol::make_object(lua, glm::vec2(value.Vector));
            case ScriptFieldType::VEC3:  return sol::make_object(lua, glm::vec3(value.Vector));
            case ScriptFieldType::VEC4:
            case ScriptFieldType::COLOR:
                return sol::make_object(lua, value.Vector);
            case ScriptFieldType::ENTITY:
            {
                const Entity target = (scene && value.EntityID) ? scene->FindEntityByUUID(UUID(value.EntityID)) : Entity();
                if(target)
                    return sol::make_object(lua, target);
                if(value.EntityID)
                    Log<Severity::Warn>("[ScriptEngine] Property '{}' references an entity that is not in the scene anymore/invalid entity", name);
                return sol::object();
            }
            case ScriptFieldType::ASSET:
            {
                // By ID in the scene, by path in Lua: renaming or moving the asset never breaks the reference
                const AssetMetadata& meta = Core::GetAssetManager()->GetMetadata(AssetID(value.AssetID));
                if(value.AssetID && meta.IsValid())
                    return sol::make_object(lua, meta.RelativePath);
                if(value.AssetID)
                    Log<Severity::Warn>("[ScriptEngine] Property '{}' references an asset that is not registered anymore", name);
                return sol::object();
            }
            case ScriptFieldType::ARRAY:
            {
                sol::table array = lua.create_table(static_cast<int>(value.Elements.size()), 0);
                int index = 1;
                for(const ScriptFieldValue& element : value.Elements)
                {
                    sol::object item = MakeFieldValue(name, elementType, ScriptFieldType::FLOAT, element, scene);
                    if(item.valid()) // nil elements (missing entity/asset) are skipped, the array stays a sequence
                        array.raw_set(index++, item);
                }
                return array;
            }
            default:
                return sol::object();
        }
    }

    void ScriptEngine::SetFields(ScriptInstance& instance, const Vector<ScriptFieldOverride>& fields, Scene* scene)
    {
        if(fields.empty() || !instance.IsValid())
            return;

        const sol::object propertiesObject = instance.Env.raw_get<sol::object>("Properties");
        if(propertiesObject.get_type() != sol::type::table)
            return;

        sol::table properties = propertiesObject.as<sol::table>();
        for(const ScriptFieldOverride& field : fields)
        {
            const sol::object current = properties.raw_get<sol::object>(field.Name);
            if(!IsFieldCompatible(current, field.Type))
            {
                Log<Severity::Warn>("[ScriptEngine] Property '{}' is no longer a {} in the script, its Inspector value is ignored", field.Name, ScriptFieldTypeToString(field.Type));
                continue;
            }
            properties.raw_set(field.Name, MakeFieldValue(field.Name, field.Type, field.ElementType, field.Value, scene));
        }
    }

    bool ScriptEngine::Instantiate(const Script& script, ScriptInstance& instance, Entity owner, const Vector<ScriptFieldOverride>& fields)
    {
        SURGE_PROFILE_FUNC("ScriptEngine::Instantiate");
        instance = ScriptInstance();

        const Vector<Byte>& bytecode = script.GetBytecode();
        if(bytecode.empty())
        {
            Log<Severity::Error>("[ScriptEngine] Script has no bytecode, was it compiled?");
            return false;
        }

        sol::state_view& lua = *sLua;
        sol::load_result loaded = lua.load(std::string_view(reinterpret_cast<const char*>(bytecode.data()), bytecode.size()), "=script", sol::load_mode::binary);
        if(!loaded.valid())
        {
            sol::error err = loaded;
            Log<Severity::Error>("[ScriptEngine] Failed to load script: {}", err.what());
            return false;
        }

        sol::protected_function chunk = loaded;
        sol::environment env(lua, sol::create, lua.globals());
        sol::set_environment(env, chunk);
        sol::protected_function_result result = chunk();
        if(!result.valid())
        {
            sol::error err = result;
            Log<Severity::Error>("[ScriptEngine] Script Error: {}", err.what());
            return false;
        }

        instance.Env = env;
        instance.Self = sol::make_object(lua, owner);
        SetFields(instance, fields, owner.GetScene());

        instance.OnCreate = GetCallback(env, "OnCreate");
        instance.OnEnable = GetCallback(env, "OnEnable");
        instance.OnDisable = GetCallback(env, "OnDisable");
        instance.OnUpdate = GetCallback(env, "OnUpdate");
        instance.OnLateUpdate = GetCallback(env, "OnLateUpdate");
        instance.OnFixedUpdate = GetCallback(env, "OnFixedUpdate");
        instance.OnDestroy = GetCallback(env, "OnDestroy");
        instance.OnCollisionEnter = GetCallback(env, "OnCollisionEnter");
        instance.OnCollisionExit = GetCallback(env, "OnCollisionExit");
        instance.OnTriggerEnter = GetCallback(env, "OnTriggerEnter");
        instance.OnTriggerExit = GetCallback(env, "OnTriggerExit");
        return true;
    }

    template <typename... Args>
    static void Call(sol::protected_function& function, const char* callbackName, Args&&... args)
    {
        if(!function.valid())
            return;

        sol::protected_function_result result = function(std::forward<Args>(args)...);
        if(!result.valid())
        {
            sol::error err = result;
            Log<Severity::Error>("[ScriptEngine] {}: {}", callbackName, err.what());
        }
    }

    void ScriptEngine::CallOnCreate(ScriptInstance& instance)
    {
        Call(instance.OnCreate, "OnCreate", instance.Self);
    }

    void ScriptEngine::CallOnUpdate(ScriptInstance& instance, float deltaTime)
    {
        Call(instance.OnUpdate, "OnUpdate", instance.Self, deltaTime);
    }

    void ScriptEngine::CallOnDestroy(ScriptInstance& instance)
    {
        Call(instance.OnDestroy, "OnDestroy", instance.Self);
    }

    void ScriptEngine::CallOnEnable(ScriptInstance& instance)
    {
        Call(instance.OnEnable, "OnEnable", instance.Self);
    }

    void ScriptEngine::CallOnDisable(ScriptInstance& instance)
    {
        Call(instance.OnDisable, "OnDisable", instance.Self);
    }

    void ScriptEngine::CallOnLateUpdate(ScriptInstance& instance, float deltaTime)
    {
        Call(instance.OnLateUpdate, "OnLateUpdate", instance.Self, deltaTime);
    }

    void ScriptEngine::CallOnFixedUpdate(ScriptInstance& instance, float fixedDeltaTime)
    {
        Call(instance.OnFixedUpdate, "OnFixedUpdate", instance.Self, fixedDeltaTime);
    }

    void ScriptEngine::CallOnCollisionEnter(ScriptInstance& instance, Entity other)
    {
        Call(instance.OnCollisionEnter, "OnCollisionEnter", instance.Self, std::move(other));
    }

    void ScriptEngine::CallOnCollisionExit(ScriptInstance& instance, Entity other)
    {
        Call(instance.OnCollisionExit, "OnCollisionExit", instance.Self, std::move(other));
    }

    void ScriptEngine::CallOnTriggerEnter(ScriptInstance& instance, Entity other)
    {
        Call(instance.OnTriggerEnter, "OnTriggerEnter", instance.Self, std::move(other));
    }

    void ScriptEngine::CallOnTriggerExit(ScriptInstance& instance, Entity other)
    {
        Call(instance.OnTriggerExit, "OnTriggerExit", instance.Self, std::move(other));
    }

    void ScriptEngine::AdvanceTime(float deltaTime, float unscaledDeltaTime)
    {
        mDeltaTime = deltaTime;
        mUnscaledDeltaTime = unscaledDeltaTime;

        mTime += deltaTime;
        mUnscaledTime += unscaledDeltaTime;
        mFrameCount++;
    }

    String ScriptEngine::GetScriptName(const Ref<Asset>& scriptAsset)
    {
        if(!scriptAsset)
            return {};

        const String& path = Core::GetAssetManager()->GetMetadata(scriptAsset->GetID()).RelativePath;

        size_t slash = path.find_last_of("/\\");
        size_t start = (slash == String::npos) ? 0 : slash + 1;

        size_t dot = path.find_last_of('.');

        if(dot != String::npos && dot > start)
            return path.substr(start, dot - start);

        return path.substr(start); // No valid extension
    }

    //////////////////////////////////////////////////////////////////////////
    // Modules & sessions
    //////////////////////////////////////////////////////////////////////////

    static String ToLower(String str)
    {
        std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return str;
    }

    void ScriptEngine::RebuildModuleIndex()
    {
        mModuleIndex.clear();
        for(const auto& [id, meta] : Core::GetAssetManager()->GetRegistryMap())
        {
            if(meta.Type == AssetType::SCRIPT && !meta.IsMissing())
                mModuleIndex[ToLower(meta.RelativePath)] = id;
        }
    }

    AssetID ScriptEngine::ResolveModule(const String& name, String& outPath, String& outError)
    {
        // "Scripts.Game.State", "Scripts/Game/State" and "Scripts/Game/State.lua" all mean Assets/Scripts/Game/State.lua
        String key = name;
        std::replace(key.begin(), key.end(), '\\', '/');
        if(key.size() > 4 && ToLower(key.substr(key.size() - 4)) == ".lua")
            key.resize(key.size() - 4);
        if(key.find('/') == String::npos)
            std::replace(key.begin(), key.end(), '.', '/');
        const String wanted = ToLower(key + ".lua");

        AssetManager* am = Core::GetAssetManager();
        for(int attempt = 0; attempt < 2; attempt++) // The index is rebuilt once when a module is not found (scripts imported meanwhile)
        {
            if(attempt == 1 || mModuleIndex.empty())
                RebuildModuleIndex();

            auto it = mModuleIndex.find(wanted);
            if(it != mModuleIndex.end())
            {
                outPath = am->GetMetadata(it->second).RelativePath;
                return it->second;
            }
        }

        // Unique file name match anywhere in the project: require("State") finds Scripts/Game/State.lua
        AssetID match = AssetID::INVALID;
        Uint matchCount = 0;
        String candidates;
        const String suffix = "/" + wanted;
        for(const auto& [path, id] : mModuleIndex)
        {
            if(path.size() > suffix.size() && path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0)
            {
                candidates += (candidates.empty() ? "" : ", ") + am->GetMetadata(id).RelativePath;
                match = id;
                matchCount++;
            }
        }

        if(matchCount == 1)
        {
            outPath = am->GetMetadata(match).RelativePath;
            return match;
        }

        outError = candidates.empty() ? std::format("no script asset '{}.lua' in the project (paths are relative to the Assets folder)", key)
                                      : std::format("'{}' is ambiguous, use the full path: {}", name, candidates);
        return AssetID::INVALID;
    }

    void ScriptEngine::ResetSession()
    {
        SURGE_PROFILE_FUNC("ScriptEngine::ResetSession");
        lua_State* L = sLua->lua_state();

        sol::table loaded = (*sLua)["package"]["loaded"];
        for(const String& moduleName : sSession->LoadedModuleNames)
            loaded.raw_set(moduleName, sol::lua_nil);

        sSession->LoadedModuleNames.clear();

        lua_newtable(L);
        lua_rawseti(L, LUA_REGISTRYINDEX, sSession->ModuleCacheRef);

        Vector<sol::object> addedGlobals;
        sol::table globals = sLua->globals();
        for(const auto& [key, value] : globals)
        {
            if(sSession->BaselineGlobals.raw_get<sol::object>(key).get_type() == sol::type::lua_nil)
                addedGlobals.push_back(key);
        }
        for(const sol::object& key : addedGlobals)
            globals.raw_set(key, sol::lua_nil);

        mModuleIndex.clear(); // Scripts may have been added or renamed between sessions
        mDeltaTime = mUnscaledDeltaTime = 0.0f;
        mTime = mUnscaledTime = 0.0;
        mFrameCount = 0;
        lua_gc(L, LUA_GCCOLLECT, 0);

        if(!addedGlobals.empty())
            Log<Severity::Trace>("[ScriptEngine] Session reset, removed {} global(s) scripts added to _G", addedGlobals.size());
    }

    void* ScriptEngine::GetSOLState() const
    {
        return sLua;
    }
}
