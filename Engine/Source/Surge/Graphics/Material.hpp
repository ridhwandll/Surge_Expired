// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Buffer.hpp"
#include "Surge/Core/String.hpp"
#include "Surge/Graphics/Shader/Shader.hpp"
#include "Surge/Graphics/Shader/ReflectionData.hpp"
#include "Surge/Graphics/Interface/UniformBuffer.hpp"
#include "Surge/Graphics/Interface/Texture.hpp"
#include "Surge/Graphics/Interface/RenderCommandBuffer.hpp"
#include "Surge/Graphics/Interface/GraphicsPipeline.hpp"

namespace Surge
{
    class SURGE_API Material : public RefCounted
    {
    public:
        Material() = default;
        virtual ~Material() = default;

        virtual void UpdateForRendering() = 0;
        virtual void Bind(const Ref<RenderCommandBuffer>& cmdBuffer, const Ref<GraphicsPipeline>& gfxPipeline) const = 0;
        virtual void Load() = 0;
        virtual void Release() = 0;

        // Removed support for Setting/Getting textures: TODO: Fix in Renderer2026
        template <typename T>
        FORCEINLINE void Set(const String& name, const T& data)
        {
            const ShaderBufferMember* member = mShaderBuffer.GetMember(name);
            SG_ASSERT_NOMSG(member);
            SG_ASSERT(sizeof(data) == member->Size, "The size of the shader member and the size of the input data doesn't match!");
            mBufferMemory.Write((Byte*)&data, sizeof(data), member->MemoryOffset);
        }

        template <typename T>
        FORCEINLINE auto& Get(const String& name)
        {
            const ShaderBufferMember* member = mShaderBuffer.GetMember(name);
            SG_ASSERT_NOMSG(member);
            return mBufferMemory.Read<T>(member->MemoryOffset);
        }

        void RemoveTexture(const String& name);

        const String& GetName() const { return mName; }
        const ShaderBuffer& GetShaderBuffer() const { return mShaderBuffer; }
        static Ref<Material> Create(const String& shaderName, const String& materialName);
        static Ref<Texture2D> mDummyTexture;

    protected:
        Ref<Shader> mShader;
        String mName;

        // Buffer
        Buffer mBufferMemory;
        ShaderBuffer mShaderBuffer;
        Ref<UniformBuffer> mUniformBuffer;

        // Textures
        //   Binding - Res
        HashMap<Uint, ShaderResource> mShaderResources;
        HashMap<Uint, Ref<Texture2D>> mTextures;

        Vector<Pair<Uint, Texture2D*>> mUpdatePendingTextures;

        UUID mShaderReloadID;
    };

} // namespace Surge
