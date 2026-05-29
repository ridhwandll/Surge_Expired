// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Class.hpp"

namespace SurgeReflect
{
    class Registry
    {
    public:
        static void Initialize();
        static Registry* Get();
        static void Shutdown();

        ~Registry();
        Class* GetClass(const std::string& name);
        void RegisterReflectionClass(Class&& clazz);
        void RemoveClass(std::string name);
        Class* GetIfExists(const std::string& name);
        const auto& GetAllClasses() const { return mClasses; }

    private:
        std::unordered_map<std::string, Class*> mClasses;
    };

} // namespace SurgeReflect
