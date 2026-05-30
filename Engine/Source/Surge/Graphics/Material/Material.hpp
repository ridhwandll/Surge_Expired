// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Memory.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Graphics/RHI/RHISettings.hpp"
#include "Surge/Graphics/RHI/RHIFrameContext.hpp"
#include "Surge/Graphics/RHI/RHIDescs.hpp"
#include "Surge/Graphics/Shader/Shader.hpp"
#include "Surge/Core/MemoryBlock.hpp"

namespace Surge
{
    class GraphicsRHI;
    class Material : public RefCounted
    {
    public:
        Material(const PipelineHandle& pipeline, const Shader& shader, const String& materialBufferName = "Material");
        ~Material();

        void SetName(const String& name) { mName = name; }
        const String& GetName() const { return mName; }

        template <typename T>
        void Set(const String& name, const T& value) const
        {
            static_assert(std::is_trivially_copyable_v<T>, "Material::Set can only be used with trivially copyable types!");

            const ShaderBufferMember* member = mRefletedBuffer.GetMember(name);
            SG_ASSERT(member, "Invalid shader member name!");
            SG_ASSERT(sizeof(T) <= member->Size, "The data type you are passing is larger than the allocated shader property block size!");

            mCPUData.Write((void*)&value, sizeof(T), member->MemoryOffset);
            MarkDirty();
        }

        template <typename T>
        const T& Get(const String& name) const
        {
            static_assert(std::is_trivially_copyable_v<T>, "Material::Get can only be used with trivially copyable types!");

            const ShaderBufferMember* member = mRefletedBuffer.GetMember(name);
            SG_ASSERT(member, "Invalid shader member name!");
            SG_ASSERT(sizeof(T) <= member->Size, "The data type requested is larger than the actual shader property block size!");

            return mCPUData.Read<T>(member->MemoryOffset);
        }

        void Bind(const FrameContext& ctx, PipelineHandle pipeline) const;

        // Uploads pending GPU data to the registry buffer
        void UpdateForRendering(const FrameContext& ctx) const;
        void MarkDirty() const
        {
            for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
                mIsDirty[i] = true;
        }
    private:
        String mName;
        ShaderBuffer mRefletedBuffer;
        Vector<ShaderResource> mReflectedResources;
        Uint mBufferBinding;

        BufferHandle mGPUBuffers[RHISettings::FRAMES_IN_FLIGHT];
        DescriptorSetSlot mMaterialDescriptorSlot;
        DescriptorSetHandle mDescriptorSet;

        mutable MemoryBlock mCPUData;
        mutable bool mIsDirty[RHISettings::FRAMES_IN_FLIGHT];
        GraphicsRHI* mRHI;
    };

} // namespace Surge
