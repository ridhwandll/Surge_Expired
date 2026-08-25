// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Vector.hpp"
#include "Surge/Asset/Asset.hpp"
#include "Surge/ScriptEngine/Lua.hpp"

namespace Surge
{
    class Entity;
    class Script final : public Asset
    {
    public:
        Script(Vector<Byte>&& bytecode);

        SURGE_ASSET_TYPE(AssetType::SCRIPT);

        void SetBytecode(Vector<Byte>&& code) { mBytecode = std::move(code); }
        const Vector<Byte>& GetBytecode() const { return mBytecode; }

        void CreateEnvironment();
        void ExecuteOnCreate(Entity e);
        void ExecuteOnUpdate(Entity e);
        void ExecuteOnDestroy(Entity e);
        void ExecuteOnCollisionEnter(Entity e, Entity other);

        static Ref<Script> Create(Vector<Byte>&& bytecode);
    private:
        Vector<Byte> mBytecode;

        sol::environment mEnv;
        sol::protected_function mOnCreate;
        sol::protected_function mOnUpdate;
        sol::protected_function mOnDestroy;

        sol::protected_function mOnCollisionEnter;

        friend class ScriptSerializer;
    };
}