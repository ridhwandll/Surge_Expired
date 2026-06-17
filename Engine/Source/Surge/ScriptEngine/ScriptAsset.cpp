// Copyright (c) - SurgeTechnologies - All rights reserved
#include "ScriptAsset.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Time/Clock.hpp"
#include "ScriptEngine.hpp"

namespace Surge
{
    Script::Script(Vector<Byte>&& bytecode)
        : mBytecode(std::move(bytecode))
    {

    }

    void Script::CreateEnvironment()
    {
        SG_ASSERT(!mBytecode.empty(), "Script bytecode is empty. Cannot create environment!");

        ScriptEngine* scriptEngine = Core::GetScriptEngine();
        sol::state_view* luaState = static_cast<sol::state_view*>(scriptEngine->GetSOLState());

        mEnv = sol::environment(*luaState, sol::create, luaState->globals());
        std::string_view bytecodeView(reinterpret_cast<const char*>(mBytecode.data()), mBytecode.size());
        auto result = luaState->safe_script(bytecodeView, mEnv, sol::script_pass_on_error);

        if(result.valid())
        {
            mOnCreate = mEnv["OnCreate"];
            mOnUpdate = mEnv["OnUpdate"];
            mOnDestroy = mEnv["OnDestroy"];
        }
        else
        {
            sol::error err = result;
            Log<Severity::Error>("[Script::CreateEnvironment] Script Error: {}", err.what());
        }
    }

    void Script::ExecuteOnCreate()
    {
        if(mOnCreate.valid())
        {
            auto createResult = mOnCreate();
            if(!createResult.valid())
            {
                sol::error err = createResult;
                Log<Severity::Error>("Lua OnCreate Error: {}", err.what());
            }
        }
    }

    void Script::ExecuteOnUpdate()
    {
        if(mOnUpdate.valid())
        {
            float dt = Core::GetClock().GetSeconds();
            auto updateResult = mOnUpdate(dt);
            if(!updateResult.valid())
            {
                sol::error err = updateResult;
                Log<Severity::Error>("Lua OnUpdate Error: {}", err.what());
            }
        }
    }

    void Script::ExecuteOnDestroy()
    {
        if(mOnDestroy.valid())
        {
            auto destroyResult = mOnDestroy();
            if(!destroyResult.valid())
            {
                sol::error err = destroyResult;
                Log<Severity::Error>("Lua OnDestroy Error: {}", err.what());
            }
        }
    }

    Ref<Script> Script::Create(Vector<Byte>&& bytecode)
    {
        return Ref<Script>::Create(std::move(bytecode));
    }
}