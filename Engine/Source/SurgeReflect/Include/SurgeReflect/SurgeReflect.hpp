// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "SurgeReflect/SurgeReflectRegistry.hpp"
#include "SurgeReflect/Type.hpp"
#include "Surge/Core/Defines.hpp"

#define SURGE_REFLECTION_ENABLE                         \
private:                                                \
    struct ReflectionRegister                           \
    {                                                   \
        ReflectionRegister();                           \
        void CookClassData(SurgeReflect::Class& clazz); \
    };                                                  \
    inline static ReflectionRegister sReflectionRegister;

#define SURGE_REFLECT_CLASS_REGISTER_BEGIN(ClassName)                             \
    ClassName::ReflectionRegister::ReflectionRegister()                           \
    {                                                                             \
        const SurgeReflect::Class& clazz = SurgeReflect::Class(#ClassName);             \
        CookClassData(const_cast<SurgeReflect::Class&>(clazz));                                                     \
        SurgeReflect::Registry::Get()->RegisterReflectionClass(std::move(const_cast<SurgeReflect::Class&>(clazz))); \
    }                                                                             \
    void ClassName::ReflectionRegister::CookClassData(SurgeReflect::Class& clazz) \
    {                                                                             \
        clazz

// clang-format off
#define SURGE_REFLECT_CLASS_REGISTER_END(ClassName) ;}
// clang-format on

namespace SurgeReflect
{
    template <typename T>
    Class* GetReflection()
    {
        std::string className = std::string(TypeTraits::GetClassName<T>());
        Class* clazz = Registry::Get()->GetClass(className);
        if (!clazz->IsSetup())
        {
            SG_ASSERT_INTERNAL("The class is not registered/setup in reflection engine! Maybe you forgot to Register the class?");
            Registry::Get()->RemoveClass(className);
        }
        return clazz;
    }

    template <typename T>
    Class* GetReflectionFromRegistry(Registry* reg)
    {
        std::string className = std::string(TypeTraits::GetClassName<T>());
        Class* clazz = reg->GetClass(className);
        if (!clazz->IsSetup())
        {
            SG_ASSERT_INTERNAL("The class is not registered/setup in reflection engine! Maybe you forgot to Register the class?");
            reg->RemoveClass(className);
        }
        return clazz;
    }

    template <typename T>
    const Class* GetReflectionIfExists()
    {
        std::string className = std::string(TypeTraits::GetClassName<T>());
        const Class* clazz = Registry::Get()->GetIfExists(className);
        return clazz;
    }

} // namespace SurgeReflect
