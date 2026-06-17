// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Asset.hpp"
#include "Surge/ScriptEngine/Lua.hpp"

namespace Surge
{
    class Script : public Asset
    {
    public:
        Script(Vector<Byte>&& bytecode);

        SURGE_ASSET_TYPE(AssetType::SCRIPT);

        void SetBytecode(Vector<Byte>&& code) { mBytecode = std::move(code); }
        const Vector<Byte>& GetBytecode() const { return mBytecode; }

        void CreateEnvironment();
        void ExecuteOnCreate();
        void ExecuteOnUpdate();
        void ExecuteOnDestroy();

        static Ref<Script> Create(Vector<Byte>&& bytecode);
    private:
        Vector<Byte> mBytecode;

        sol::environment mEnv;
        sol::protected_function mOnCreate;
        sol::protected_function mOnUpdate;
        sol::protected_function mOnDestroy;

        friend class ScriptSerializer;
    };
}