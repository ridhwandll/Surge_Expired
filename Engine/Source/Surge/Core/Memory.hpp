// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"

namespace Surge
{
    class SURGE_API RefCounted
    {
    public:
        void IncRefCount() const { mRefCount++; }
        void DecRefCount() const { mRefCount--; }
        void ZeroRefCount() const { mRefCount = 0; }

        Uint GetRefCount() const { return mRefCount; }

    private:
        mutable Uint mRefCount = 0;
    };

    template <typename T>
    class SURGE_API Ref
    {
    public:
        Ref() : mInstance(nullptr) {}

        Ref(std::nullptr_t n) : mInstance(nullptr) {}

        Ref(T* instance) : mInstance(instance)
        {
            static_assert(std::is_base_of<RefCounted, T>::value, "Class is not RefCounted!");
            IncRef();
        }

        template <typename Ts>
        Ref(const Ref<Ts>& other)
        {
            mInstance = static_cast<T*>(other.mInstance);
            IncRef();
        }

        Ref(const Ref<T>& other) : mInstance(other.mInstance) { IncRef(); }

        template <typename Ts>
        Ref(Ref<Ts>&& other)
        {
            mInstance = static_cast<T*>(other.mInstance);
            other.mInstance = nullptr;
        }

        Ref& operator=(std::nullptr_t)
        {
            DecRef();
            mInstance = nullptr;
            return *this;
        }

        Ref& operator=(const Ref<T>& other)
        {
            other.IncRef();
            DecRef();

            mInstance = other.mInstance;
            return *this;
        }

        template <typename Ts>
        Ref& operator=(const Ref<Ts>& other)
        {
            other.IncRef();
            DecRef();

            mInstance = other.mInstance;
            return *this;
        }

        template <typename Ts>
        Ref& operator=(Ref<Ts>&& other)
        {
            DecRef();

            mInstance = other.mInstance;
            other.mInstance = nullptr;
            return *this;
        }

        operator bool() { return mInstance != nullptr; }
        operator bool() const { return mInstance != nullptr; }

        T* operator->() { return mInstance; }
        const T* operator->() const { return mInstance; }

        T& operator*() { return *mInstance; }
        const T& operator*() const { return *mInstance; }

        T* Raw() { return mInstance; }
        [[nodiscard]] const T* Raw() const { return mInstance; }

        void Release()
        {
            delete mInstance;
            mInstance->ZeroRefCount();
        }

        void Reset(T* instance = nullptr)
        {
            DecRef();
            mInstance = instance;
        }

        template <typename... Args>
        static Ref<T> Create(Args&&... args)
        {
            return Ref<T>(new T(std::forward<Args>(args)...));
        }

        template <typename Ts>
        Ref<Ts> As() const
        {
            return Ref<Ts>(*this);
        }

        ~Ref() { DecRef(); }

    private:
        void IncRef() const
        {
            if (mInstance)
                mInstance->IncRefCount();
        }

        void DecRef() const
        {
            if (mInstance)
            {
                mInstance->DecRefCount();
                if (mInstance->GetRefCount() == 0)
                    delete mInstance;
            }
        }

        friend class Ref;

    public:
        T* mInstance;
    };

} // namespace Surge
