// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "SurgeReflect/Type.hpp"
#include "SurgeReflect/TypeTraits.hpp"
#include <string>

namespace SurgeReflect
{
    class Variable
    {
    public:
        Variable() = default;
        Variable(const std::string& name, AccessModifier accessModifier)
            : mName(name), mAccessModifier(accessModifier) {}

        const std::string& GetName() const { return mName; }
        const AccessModifier& GetAccessModifier() const { return mAccessModifier; }
        uint64_t GetSize() const { return mSize; }
        uint64_t GetOffset() const { return mOffset; }
        const Type& GetType() const { return mType; }

    private:
        template <auto Var> 
        void Initialize()
        {
            using Traits = TypeTraits::VariableTraits<decltype(Var)>; //Example: decltype(&TransformComponent::Position) = glm::vec3 TransformComponent::*
            mSize = sizeof(typename Traits::Type);
            mOffset = ComputeOffset(Var);
            mType.Initialize<typename Traits::Type>();
        }

        template <typename T, typename C>
        static uint64_t ComputeOffset(T C::* member)
        {
            alignas(C) char buf[sizeof(C)] = {}; // Temporary memory on stack to compute the offset of the member within the class
            auto* obj = reinterpret_cast<C*>(buf);
            return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&(obj->*member)) - reinterpret_cast<uintptr_t>(obj));
        }

    private:
        std::string mName;
        AccessModifier mAccessModifier;
        uint64_t mSize = 0;
        uint64_t mOffset = 0;
        Type mType;

        friend class Class;
    };

} // namespace SurgeReflect