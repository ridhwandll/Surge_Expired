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

        void CreateEnvironment(sol::environment* outEnv, sol::protected_function* outOnCreate,
                                                         sol::protected_function* outOnUpdate,
                                                         sol::protected_function* outOnDestroy,
                                                         sol::protected_function* outOnCollisionEnter);

        void ExecuteOnCreate(Entity e, sol::protected_function& func);
        void ExecuteOnUpdate(Entity e, sol::protected_function& func);
        void ExecuteOnDestroy(Entity e, sol::protected_function& func);
        void ExecuteOnCollisionEnter(Entity e, Entity other, sol::protected_function& func);

        static Ref<Script> Create(Vector<Byte>&& bytecode);
    private:
        Vector<Byte> mBytecode;
        friend class ScriptSerializer;
    };
}