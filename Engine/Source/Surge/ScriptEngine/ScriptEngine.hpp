// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"

namespace Surge
{
    class ScriptEngine
    {
    public:
        void Initialize();
        void Shutdown();

        // Compile
        // Takes raw Lua source code and returns compiled binary bytecode
        // @param source:    The raw Lua source code as a string
        // @return           A vector of bytes containing the compiled Lua bytecode
        Vector<Byte> Compile(const String& source, const String& chunkName);

        // Returns sol::state_view*, use with caution and cast to sol::state_view* when needed. This is to avoid exposing sol directly in the header
        void* GetSOLState() const;
    };
}