// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderProcedure/RenderProcedure.hpp"
#include "Surge/Core/Profiler.hpp"
#include "SurgeReflect/SurgeReflect.hpp"

namespace Surge
{
    class SURGE_API RenderProcedureManager
    {
    public:
        RenderProcedureManager() = default;
        ~RenderProcedureManager() = default;

        FORCEINLINE void Init(Scope<RendererData>& data)
        {
            SG_ASSERT_NOMSG(data);
            mRendererData = data.get();
        }

        template <typename... Procedures>
        FORCEINLINE void Sort()
        {
            (mProcOrder.push_back(SurgeReflect::GetReflection<Procedures>()->GetHash()), ...);
        }

        template <typename T> // TODO: Use C++20 "concept"s when we switch
        FORCEINLINE T* AddProcedure()
        {
            static_assert(std::is_base_of<RenderProcedure, T>::value, "Class must derive from RenderProcedure");

            T* procInstance = new T();
            procInstance->Init(mRendererData);
            const SurgeReflect::ClassHash& hash = GetProcHash<T>();

            mProcedures[hash] = {true, procInstance};
            return procInstance;
        }

        template <typename T> // TODO: Use C++20 "concept"s when we switch
        FORCEINLINE T* GetProcedure()
        {
            static_assert(std::is_base_of<RenderProcedure, T>::value, "Class must derive from RenderProcedure");
            const SurgeReflect::ClassHash& hash = GetProcHash<T>();

            auto itr = mProcedures.find(hash);
            if (itr != mProcedures.end())
                return static_cast<T*>(itr->second.Data2);

            return nullptr;
        }

        void UpdateAll();

        template <typename T>
        FORCEINLINE typename T::InternalData* GetRenderProcData()
        {
            static_assert(std::is_base_of<RenderProcedure, T>::value, "Class must derive from RenderProcedure");
            const SurgeReflect::ClassHash& hash = GetProcHash<T>();

            auto itr = mProcedures.find(hash);
            if (itr != mProcedures.end())
                return static_cast<typename T::InternalData*>(itr->second.Data2->GetInternalDataBlock());

            return nullptr;
        }

        template <typename T>
        void RestartProcedure()
        {
            //Core::AddFrameEndCallback([this]() {
            //    T* proc = GetProcedure<T>();
            //    SG_ASSERT(proc, "Attempted to restart invalid Procedure!");
            //
            //    proc->Shutdown();
            //    proc->Init(mRendererData);
            //});
        }

        FORCEINLINE void ResizeAll(Uint newWidth, Uint newHeight)
        {
            for (const SurgeReflect::ClassHash& hash : mProcOrder)
            {
                auto& [isActive, procedure] = mProcedures.at(hash);
                procedure->Resize(newWidth, newHeight);
            }
        }

        template <typename T>
        void SetProcecureActive(bool disable)
        {
            const SurgeReflect::ClassHash& providedHash = GetProcHash<T>();
            auto& [isActive, procedure] = mProcedures.at(providedHash);
            isActive = disable;
        }

        template <typename T>
        const bool& IsProcecureActive() const
        {
            const SurgeReflect::ClassHash& providedHash = GetProcHash<T>();
            const auto& [isActive, proc] = mProcedures.at(providedHash);
            return isActive;
        }

        FORCEINLINE void Shutdown()
        {
            for (const SurgeReflect::ClassHash& hash : mProcOrder)
            {
                auto& [isActive, procedure] = mProcedures.at(hash);
                procedure->Shutdown();
                delete procedure;
            }

            mProcOrder.clear();
            mProcedures.clear();
        }

    private:
        template <typename T>
        const SurgeReflect::ClassHash& GetProcHash() const
        {
            const SurgeReflect::Class* clazz = SurgeReflect::GetReflection<T>();
            SG_ASSERT(clazz, "No Reflection found!");
            const SurgeReflect::ClassHash& hash = clazz->GetHash();
            return hash;
        }

    private:
        RendererData* mRendererData;
        Vector<SurgeReflect::ClassHash> mProcOrder;
        HashMap<SurgeReflect::ClassHash, Pair<bool, RenderProcedure*>> mProcedures; // mapped as-> classHash - {isActive, proc}
    };

} // namespace Surge
