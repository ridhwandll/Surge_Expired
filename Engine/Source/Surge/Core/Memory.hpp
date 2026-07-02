// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include <type_traits>
// #include <atomic> //Using compiler intrinsics to bypass this header include

#if defined(_MSC_VER) && !defined(__clang__)
// (Rid) We forward declare intrinsics directly to bypass <intrin.h> bloat
// If this ever fails to compile/link in future then comment out the 3 lines below and uncomment the 4th line
extern "C" long _InterlockedIncrement(long volatile* Addend);
extern "C" long _InterlockedDecrement(long volatile* Addend);
extern "C" long _InterlockedExchange(long volatile* Target, long Value);
extern "C" long _InterlockedExchangeAdd(long volatile* Target, long Value);
//#include <intrin.h>
#endif

namespace Surge
{
    class RefCounted
    {
    public:
        virtual ~RefCounted() = default;

        inline void IncRefCount() const
        {
#if defined(__clang__) || defined(__GNUC__)
            __atomic_fetch_add(&mRefCount, 1, __ATOMIC_RELAXED);
#elif defined(_MSC_VER)
            _InterlockedIncrement(reinterpret_cast<volatile long*>(&mRefCount));
#endif
        }

        inline bool DecRefCount() const
        {
#if defined(__clang__) || defined(__GNUC__)
            return __atomic_fetch_sub(&mRefCount, 1, __ATOMIC_ACQ_REL) == 1;
#elif defined(_MSC_VER)
            return _InterlockedDecrement(reinterpret_cast<volatile long*>(&mRefCount)) == 0;
#endif
        }

        inline void ZeroRefCount() const
        {
#if defined(__clang__) || defined(__GNUC__)
            __atomic_store_n(&mRefCount, 0, __ATOMIC_RELEASE);
#elif defined(_MSC_VER)
            _InterlockedExchange(reinterpret_cast<volatile long*>(&mRefCount), 0);
#endif
        }

        inline uint32_t GetRefCount() const
        {
#if defined(__clang__) || defined(__GNUC__)
            return __atomic_load_n(&mRefCount, __ATOMIC_RELAXED);
#elif defined(_MSC_VER)
            return static_cast<uint32_t>(_InterlockedExchangeAdd(reinterpret_cast<volatile long*>(&mRefCount), 0)); //Not tested! Is this good?
#endif
        }

    private:
        mutable uint32_t mRefCount = 0;
    };

    template <typename T>
    class Ref
    {
    public:
        Ref()
            : mInstance(nullptr)
        {}

        Ref(std::nullptr_t)
            : mInstance(nullptr)
        {}

        Ref(T* instance)
            : mInstance(instance)
        {
            static_assert(std::is_base_of<RefCounted, T>::value, "Class is not RefCounted!");
            IncRef();
        }

        // COPY
        Ref(const Ref<T>& other)
            : mInstance(other.mInstance)
        {
            IncRef();
        }

        // TEMPLATED COPY
        template <typename Ts>
        Ref(const Ref<Ts>& other)
            : mInstance(static_cast<T*>(other.mInstance))
        {
            IncRef();
        }

        // MOVE
        Ref(Ref<T>&& other) noexcept
            : mInstance(other.mInstance)
        {
            other.mInstance = nullptr;
        }

        // TEMPLATED MOVE
        template <typename Ts>
        Ref(Ref<Ts>&& other) noexcept
            : mInstance(static_cast<T*>(other.mInstance))
        {
            other.mInstance = nullptr;
        }

        Ref& operator=(std::nullptr_t)
        {
            DecRef();
            mInstance = nullptr;
            return *this;
        }

        // COPY ASSIGNMENT
        Ref& operator=(const Ref<T>& other)
        {
            if(this == &other)
                return *this;

            other.IncRef();
            DecRef();
            mInstance = other.mInstance;
            return *this;
        }

        // TEMPLATED COPY ASSIGNMENT
        template <typename Ts>
        Ref& operator=(const Ref<Ts>& other)
        {
            other.IncRef();
            DecRef();
            mInstance = static_cast<T*>(other.mInstance);
            return *this;
        }

        // STANDARD MOVE ASSIGNMENT
        Ref& operator=(Ref<T>&& other) noexcept
        {
            if(this == &other)
                return *this;

            T* oldInstance = mInstance;
            mInstance = other.mInstance;
            other.mInstance = nullptr;

            if(oldInstance && oldInstance->DecRefCount())
                delete oldInstance;

            return *this;
        }

        // TEMPLATED MOVE ASSIGNMENT
        template <typename Ts>
        Ref& operator=(Ref<Ts>&& other) noexcept
        {
            T* oldInstance = mInstance;
            mInstance = static_cast<T*>(other.mInstance);
            other.mInstance = nullptr;

            if(oldInstance && oldInstance->DecRefCount())
                delete oldInstance;

            return *this;
        }

        explicit operator bool() const { return mInstance != nullptr; }

        T* operator->() { return mInstance; }
        const T* operator->() const { return mInstance; }

        T& operator*() { return *mInstance; }
        const T& operator*() const { return *mInstance; }

        [[nodiscard]] T* Raw() { return mInstance; }
        [[nodiscard]] const T* Raw() const { return mInstance; }

        // --- STL COMPATIBILITY ---
        [[nodiscard]] T* get() { return mInstance; }
        [[nodiscard]] const T* get() const { return mInstance; }

        // Potentially dangerous, use with caution. This will forcibly destroy the managed object regardless of
        // current ref count which can lead to dangling pointers if other Refs are still referencing it
        void ForceDestroy()
        {
            if(mInstance)
            {
                mInstance->ZeroRefCount();
                delete mInstance;
                mInstance = nullptr;
            }
        }

        void Reset(T* instance = nullptr)
        {
            DecRef();
            mInstance = instance;
            IncRef();
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

        ~Ref()
        {
            DecRef();
        }

    private:
        void IncRef() const
        {
            if(mInstance)
                mInstance->IncRefCount();
        }

        void DecRef() const
        {
            if(mInstance && mInstance->DecRefCount())
                delete mInstance;
        }

        template <typename U>
        friend class Ref;

    private:
        T* mInstance;
    };

} // namespace Surge