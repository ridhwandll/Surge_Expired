// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include <string>
#include <functional>
#include "SurgeReflect/Utility.hpp"
#include "TypeTraits.hpp"

namespace SurgeReflect
{
    enum class AccessModifier
    {
        Public,
        Private,
        Protected
    };

    inline std::string AccessModifierToString(AccessModifier mod)
    {
        switch (mod)
        {
            case AccessModifier::Public: return "Public";
            case AccessModifier::Private: return "Private";
            case AccessModifier::Protected: return "Protected";
        }
        return "ERROR";
    }

    class Type
    {
    public:
        Type() = default;
        ~Type() = default;

        template <typename T>
        bool EqualTo() const
        {
            const std::string givenTypeName = std::string(TypeTraits::GetTypeName<T>());
            const int64_t givenTypeHash = Utility::GenerateStringHash(givenTypeName);

            bool result = mHashCode == givenTypeHash;
            return result;
        }

        const std::string& GetFullName() const { return mFullName; }
        const int64_t& GetHashCode() const { return mHashCode; }
        const bool& IsEnum() const { return mIsEnum; }
        const bool& IsClass() const { return mIsClass; }
        const bool& IsUnion() const { return mIsUnion; }
        const bool& IsPrimitive() const { return mIsPrimitive; }

        template <typename T>
        void Initialize()
        {
            mIsEnum = TypeTraits::IsTypeEnum<T>;
            mIsClass = TypeTraits::IsTypeClass<T>;
            mIsUnion = TypeTraits::IsTypeUnion<T>;
            mIsPrimitive = TypeTraits::IsTypePrimitive<T>;

            mFullName = std::string(TypeTraits::GetTypeName<T>());
            mHashCode = Utility::GenerateStringHash(mFullName);
        }

    private:
        std::string mFullName;
        int64_t mHashCode;

        bool mIsEnum = false;
        bool mIsClass = false;
        bool mIsUnion = false;
        bool mIsPrimitive = false;
    };

    template <class T>
    Type GetType()
    {
        Type type;
        type.Initialize<T>();
        return type;
    }

} // namespace SurgeReflect