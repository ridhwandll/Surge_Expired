// Copyright (c) - SurgeTechnologies - All rights reserved
#include "ScriptAsset.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Time/Clock.hpp"
#include "ScriptEngine.hpp"

namespace Surge
{

    Script::Script(Vector<Byte>&& bytecode)
        : mBytecode(std::move(bytecode))
    {}

    void Script::CreateEnvironment(sol::environment* outEnv, sol::protected_function* outOnCreate,
                           sol::protected_function* outOnUpdate,
                           sol::protected_function* outOnDestroy,
                           sol::protected_function* outOnCollisionEnter)
    {
        SG_ASSERT(!mBytecode.empty(), "Script bytecode is empty. Cannot create environment!");

        ScriptEngine* scriptEngine = Core::GetScriptEngine();
        sol::state_view* luaState = static_cast<sol::state_view*>(scriptEngine->GetSOLState());
        // TODO: move this to ScriptEngine and make it a function that returns a sol::environment thingy
        *outEnv = sol::environment(*luaState, sol::create, luaState->globals());
        std::string_view bytecodeView(reinterpret_cast<const char*>(mBytecode.data()), mBytecode.size());
        auto result = luaState->safe_script(bytecodeView, *outEnv, sol::script_pass_on_error);

        if(result.valid())
        {
            *outOnCreate = (*outEnv)["OnCreate"];
            *outOnUpdate = (*outEnv)["OnUpdate"];
            *outOnDestroy = (*outEnv)["OnDestroy"];
            *outOnCollisionEnter = (*outEnv)["OnCollisionEnter"];
        }
        else
        {
            sol::error err = result;
            Log<Severity::Error>("[Script::CreateEnvironment] Script Error: {}", err.what());
        }
    }

    void Script::ExecuteOnCreate(Entity e, sol::protected_function& func)
    {
        if(func.valid())
        {
            // We Use std::move() to force sol2 to copy the Entity into Lua's memory instead of capturing a reference to the local stack variable
            auto createResult = func(std::move(e));
            if(!createResult.valid())
            {
                sol::error err = createResult;
                Log<Severity::Error>("Lua OnCreate Error: {}", err.what());
            }
        }
    }

    void Script::ExecuteOnUpdate(Entity e, sol::protected_function& func)
    {
        if(func.valid())
        {
            float dt = Core::GetClock().GetSeconds();
            auto updateResult = func(std::move(e), dt);
            if(!updateResult.valid())
            {
                sol::error err = updateResult;
                Log<Severity::Error>("Lua OnUpdate Error: {}", err.what());
            }
        }
    }

    void Script::ExecuteOnDestroy(Entity e, sol::protected_function& func)
    {
        if(func.valid())
        {
            auto destroyResult = func(std::move(e));
            if(!destroyResult.valid())
            {
                sol::error err = destroyResult;
                Log<Severity::Error>("Lua OnDestroy Error: {}", err.what());
            }
        }
    }

    void Script::ExecuteOnCollisionEnter(Entity e, Entity other, sol::protected_function& func)
    {
        if (func.valid())
        {
            auto collisionResult = func(std::move(e), std::move(other));
            if (!collisionResult.valid())
            {
                sol::error err = collisionResult;
                Log<Severity::Error>("Lua OnCollisionEnter Error: {}", err.what());
            }
        }
    }

    Ref<Script> Script::Create(Vector<Byte>&& bytecode)
    {
        return Ref<Script>::Create(std::move(bytecode));
    }
}