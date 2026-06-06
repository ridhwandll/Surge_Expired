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
#include "Surge/Asset/Asset.hpp"
#include "Texture2D.hpp"

namespace Surge
{
    class GraphicsRHI;
    class Material : public Asset
    {
    public:
        Material(const String& path);
        Material(const PipelineHandle& pipeline, const Shader& shader, const String& materialBufferName = "Material");
        ~Material();
        SURGE_ASSET_TYPE(AssetType::MATERIAL)

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

        void SetTexture(const String& name, const Ref<Texture2D>& texture);
        void SetTexture(const String& name, ImageHandle handle);

        void Bind(const FrameContext& ctx, PipelineHandle pipeline) const;

        // Uploads pending GPU data to the registry buffer
        void UpdateForRendering(const FrameContext& ctx) const;
        void MarkDirty() const
        {
            for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
                mIsDirty[i] = true;
        }

        static Ref<Material> Create(const String& path);
        static Ref<Material> Create(const PipelineHandle& pipeline, const Shader& shader, const String& name = "DefaultMaterial");
    private:
        void Initialize(const PipelineHandle& pipeline, const Shader& shader, const String& materialBufferName);
        void Serialize(const String& path);
        void Deserialize(const String& path);
        const ShaderResource* FindResource(const String& name) const;
    private:
        String mName;
        String mShaderName;
        ShaderBuffer mRefletedBuffer;
        Vector<ShaderResource> mReflectedResources;
        Uint mBufferBinding;

        BufferHandle mGPUBuffers[RHISettings::FRAMES_IN_FLIGHT];
        DescriptorSetSlot mMaterialDescriptorSlot;
        DescriptorSetHandle mDescriptorSet;

        mutable MemoryBlock mCPUData;
        std::unordered_map<String, Pair<Ref<Texture2D>, ImageHandle>> mTextures;

        mutable bool mIsDirty[RHISettings::FRAMES_IN_FLIGHT];
        GraphicsRHI* mRHI;
    };

} // namespace Surge
