// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include <memory>

namespace Surge
{
    template <typename T>
    using Scope = std::unique_ptr<T>; //TODO: Have a dedicated Scope Class
 
    template <typename T, typename... Args>
    constexpr Scope<T> CreateScope(Args&&... args)
    {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }
}
