// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Material.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Graphics/RHI/RHI.hpp"
#include "Texture2D.hpp"
#include <json/json.hpp>
#include <fstream>
#include "SurgeReflect/Enum.hpp"
#include "Surge/Utility/Filesystem.hpp"

namespace Surge
{
    Material::Material(const PipelineHandle& pipeline, const Shader& shader, const String& materialBufferName)
    {
        mShaderName = shader.GetName();
        Initialize(pipeline, shader, materialBufferName);
    }

    Material::Material(const String& path)
    {
        std::ifstream file(path);
        SG_ASSERT(file.is_open(), "[Material] Failed to open: '{}'", path.c_str());

        const nlohmann::json j = nlohmann::json::parse(file, nullptr, false);
        SG_ASSERT(!j.is_discarded(), "[Material] Failed to parse JSON: '{}'", path.c_str());

        mName = j.value("Name", "Unnamed");

        const String shaderName = j.value("Shader", "");
        SG_ASSERT(!shaderName.empty(), "[Material] No shader specified in: '{}'", path.c_str());

        Renderer* renderer = Core::GetRenderer();
        PipelineHandle pipeline = renderer->GetRenderGraphBlackBoard().MaterialPipeline;
        const Shader& shader = renderer->GetShaderManager().Get(shaderName);
        SG_ASSERT(pipeline.IsValid(), "[Material] Pipeline not found for shader: '{}'", shaderName.c_str());

        Initialize(pipeline, shader, "Material");

        if(j.contains("Properties"))
        {
            for(const auto& [propName, propValue] : j["Properties"].items())
            {
                const ShaderBufferMember* member = mRefletedBuffer.GetMember(propName);
                if(!member)
                {
                    Log<Severity::Warn>("[Material] Property '{}' not in reflection, skipping", propName.c_str());
                    continue;
                }

                if(propValue.is_number())
                {
                    const float val = propValue.get<float>();
                    mCPUData.Write((void*)&val, sizeof(float), member->MemoryOffset);
                }
                else if(propValue.is_array())
                {
                    switch(propValue.size())
                    {
                        case 2:
                        {
                            const glm::vec2 val = { propValue[0].get<float>(), propValue[1].get<float>() };
                            mCPUData.Write((void*)&val, sizeof(glm::vec2), member->MemoryOffset);
                            break;
                        }
                        case 3:
                        {
                            const glm::vec3 val = { propValue[0].get<float>(), propValue[1].get<float>(), propValue[2].get<float>() };
                            mCPUData.Write((void*)&val, sizeof(glm::vec3), member->MemoryOffset);
                            break;
                        }
                        case 4:
                        {
                            const glm::vec4 val = { propValue[0].get<float>(), propValue[1].get<float>(), propValue[2].get<float>(), propValue[3].get<float>() };
                            mCPUData.Write((void*)&val, sizeof(glm::vec4), member->MemoryOffset);
                            break;
                        }
                        default:
                            Log<Severity::Warn>("[Material] Unsupported array size {} for '{}', skipping", propValue.size(), propName.c_str());
                            break;
                    }
                }
            }
        }

        if(j.contains("Textures"))
        {
            for(const auto& [texName, texIDVal] : j["Textures"].items())
            {
                const AssetID id(texIDVal.get<uint64_t>());
                Ref<Texture2D> texture = Core::GetAssetManager()->Load<Texture2D>(id);
                if(texture)
                    SetTexture(texName, texture);
                else
                    Log<Severity::Warn>("[Material] Texture '{}' (ID: {}) failed to load!", texName.c_str(), id.Get());
            }
        }

        MarkDirty();
    }

    Material::~Material()
    {
        //mRHI->WaitIdle();

        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            mRHI->DestroyBuffer(mGPUBuffers[i]);

        for(const auto& [name, tex] : mTextures)
        {
            if(tex.Data1) // If we have a Ref<Texture2D>, we own the texture and should destroy it. If we only have an ImageHandle, we don't own it and shouldn't destroy it
                mRHI->DestroyImage(tex.Data2);
        }

        mRHI->DestroyDescriptorSet(mDescriptorSet);
    }

    void Material::Bind(const FrameContext& ctx, PipelineHandle pipeline) const
    {
        UpdateForRendering(ctx);
        mRHI->CmdBindDescriptorSet(ctx, pipeline, mDescriptorSet, mMaterialDescriptorSlot);
    }

    #undef FindResource // FUCK ASS WIN32
    void Material::SetTexture(const String& name, const Ref<Texture2D>& texture)
    {
        SG_ASSERT(texture, "Material::SetTexture: texture is null!");

        const ShaderResource* resource = FindResource(name);
        SG_ASSERT(resource, "Material::SetTexture: '{}' is not a reflected texture name!", name.c_str());

        mTextures[name] = Pair<Ref<Texture2D>, ImageHandle>(texture, texture->GetRHIImage());
        MarkDirty();
    }

    void Material::SetTexture(const String& name, ImageHandle handle)
    {

        const ShaderResource* resource = FindResource(name);
        SG_ASSERT(resource, "Material::SetTexture: '{}' is not a reflected texture name!", name.c_str());

        mTextures[name] = Pair<Ref<Texture2D>, ImageHandle>(nullptr, handle);
        MarkDirty();
    }

    void Material::UpdateForRendering(const FrameContext& ctx) const
    {
        if(!mIsDirty[ctx.FrameIndex])
            return;

        Vector<DescriptorWrite> writes;
        writes.reserve(mTextures.size() + 1);

        DescriptorWrite bufferWrite = {};
        bufferWrite.Binding = mBufferBinding;
        bufferWrite.Type = DescriptorType::UNIFORM_BUFFER;
        bufferWrite.Buffer = mGPUBuffers[ctx.FrameIndex];
        bufferWrite.BufferRange = mRefletedBuffer.Size;
        writes.push_back(bufferWrite);

        // Textures driven by reflection, no hardcoded bindings
        for(const auto& [name, tex] : mTextures)
        {
            const ShaderResource* resource = FindResource(name);
            SG_ASSERT(resource, "Material::UpdateForRendering: '{}' is not a reflected texture name!", name.c_str());

            DescriptorWrite write = {};
            write.Binding = resource->Binding;
            write.Type = DescriptorType::TEXTURE;
            write.Texture = tex.Data2; // tex.Data2 is always a valid ImageHandle, even if we don't own the texture. If we have a Ref<Texture2D>, we get the ImageHandle from it. If we only have an ImageHandle, we use that directly
            write.Sampler = Core::GetRenderer()->GetDefaultSampler();
            writes.push_back(write);
        }

        mRHI->UploadBuffer(mGPUBuffers[ctx.FrameIndex], mCPUData.As<void>(), mCPUData.GetSize());
        mRHI->UpdateDescriptorSet(mDescriptorSet, writes.data(), (Uint)writes.size(), ctx.FrameIndex);
        mIsDirty[ctx.FrameIndex] = false;
    }

    Ref<Material> Material::Create(const PipelineHandle& pipeline, const Shader& shader, const String& name)
    {
        return Ref<Material>::Create(pipeline, shader, name);
    }

    Ref<Material> Material::Create(const String& path)
    {
        return Ref<Material>::Create(path);
    }

    void Material::Initialize(const PipelineHandle& pipeline, const Shader& shader, const String& materialBufferName)
    {
        mRHI = Core::GetRenderer()->GetRHI().get();
        mRefletedBuffer = shader.GetReflectionData().GetBuffer(materialBufferName);
        mReflectedResources = shader.GetReflectionData().GetResources();

        mBufferBinding = mRefletedBuffer.Binding;
        mCPUData.Allocate(mRefletedBuffer.Size);
        mMaterialDescriptorSlot = static_cast<DescriptorSetSlot>(mRefletedBuffer.Set);

        mDescriptorSet = mRHI->CreateDescriptorSet(pipeline, mMaterialDescriptorSlot, DescriptorUpdateFrequency::DYNAMIC, materialBufferName.c_str());

        BufferDesc bufferDesc = {};
        bufferDesc.Size = mRefletedBuffer.Size;
        bufferDesc.InitialData = nullptr;
        bufferDesc.Usage = BufferUsage::UNIFORM;
        bufferDesc.HostVisible = true;
        bufferDesc.DebugName = materialBufferName;

        // (RID) Instead of creating RHISettings::FRAMES_IN_FLIGHT amount of VkBuffer for each material, we could have a
        // global giant VkBuffer for each shader buffer block, and suballocate from that for each material. This would
        // reduce memory fragmentation and the number of buffers we have to manage, but it would add complexity to buffer
        // management and synchronization. For now we will stick with the simpler approach of one buffer per material per frame

        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            mGPUBuffers[i] = mRHI->CreateBuffer(bufferDesc);

        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
        {
            DescriptorWrite write = {};
            write.Binding = mBufferBinding;
            write.Buffer = mGPUBuffers[i];
            write.Type = DescriptorType::UNIFORM_BUFFER;
            write.BufferRange = mRefletedBuffer.Size;
            mRHI->UpdateDescriptorSet(mDescriptorSet, &write, 1, i);
        }
        MarkDirty();

    }

    void Material::Serialize(const String& path)
    {
        nlohmann::json j;
        j["Name"] = mName;
        j["Shader"] = mShaderName;
        nlohmann::json& props = j["Properties"];
        for(const ShaderBufferMember& member : mRefletedBuffer.Members)
        {
            switch(member.DataType)
            {
                case ShaderDataType::FLOAT:
                {
                    props[member.Name] = mCPUData.Read<float>(member.MemoryOffset);
                    break;
                }
                case ShaderDataType::INT:
                {
                    props[member.Name] = mCPUData.Read<int>(member.MemoryOffset);
                    break;
                }
                case ShaderDataType::FLOAT2:
                {
                    const glm::vec2 v = mCPUData.Read<glm::vec2>(member.MemoryOffset);
                    props[member.Name] = { v.x, v.y };
                    break;
                }
                case ShaderDataType::FLOAT3:
                {
                    const glm::vec3 v = mCPUData.Read<glm::vec3>(member.MemoryOffset);
                    props[member.Name] = { v.x, v.y, v.z };
                    break;
                }
                case ShaderDataType::FLOAT4:
                {
                    const glm::vec4 v = mCPUData.Read<glm::vec4>(member.MemoryOffset);
                    props[member.Name] = { v.x, v.y, v.z, v.w };
                    break;
                }
                default:
                    Log<Severity::Warn>("[Material] Skipping property '{}' unsupported type {}.", member.Name, SurgeReflect::EnumToString(member.DataType).data());
                    break;
            }
        }

        nlohmann::json& textures = j["Textures"];
        for(const auto& [name, tex] : mTextures)
        {
            if(tex.Data1) // Ref<Texture2D> owned, has a valid AssetID
                textures[name] = tex.Data1->GetID().Get();
        }

        std::ofstream file(path, std::ios::out | std::ios::trunc);
        SG_ASSERT(file.is_open(), "[Material] Failed to write: '{}'", path);
        file << j.dump(4);
        file.close();
    }

    void Material::Deserialize(const String& path)
    {

    }

    const Surge::ShaderResource* Material::FindResource(const String& name) const
    {
        for(const ShaderResource& resource : mReflectedResources)
        {
            if(resource.Name == name)
                return &resource;
        }
        return nullptr;
    }

}
