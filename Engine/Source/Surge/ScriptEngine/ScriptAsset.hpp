// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Vector.hpp"
#include "Surge/Asset/Asset.hpp"

namespace Surge
{
    // Cooked Lua script: bytecode only, instances are created by ScriptEngine::Instantiate(), modules by require()
    class Script final : public Asset
    {
    public:
        Script(Vector<Byte>&& bytecode);

        SURGE_ASSET_TYPE(AssetType::SCRIPT);

        void SetBytecode(Vector<Byte>&& code) { mBytecode = std::move(code); }
        const Vector<Byte>& GetBytecode() const { return mBytecode; }

        static Ref<Script> Create(Vector<Byte>&& bytecode);
    private:
        Vector<Byte> mBytecode;
    };
}
