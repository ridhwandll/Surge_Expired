// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once

namespace Surge
{
    class ScriptEngine
    {
    public:
        void Initialize();
        void Shutdown();

        // Returns sol::state_view*, use with caution and cast to sol::state_view* when needed. This is to avoid exposing sol directly in the header
        void* GetSOLState() const;
    };
}