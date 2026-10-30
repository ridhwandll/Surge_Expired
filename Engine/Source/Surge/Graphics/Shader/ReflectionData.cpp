// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Shader/ReflectionData.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanUtils.hpp"
#include "Surge/Asset/Format/BinarySerialization.hpp"
#include <SurgeReflect/Enum.hpp>

namespace Surge
{
    // We hate the warnings
    static ShaderBuffer sDummyBuffer = ShaderBuffer();
    static ShaderBufferMember sDummyBufferMember = ShaderBufferMember();

    /////
    // ShaderReflectionData
    /////

    const ShaderBuffer& ShaderReflectionData::GetBuffer(const String& name) const
    {
        for (const ShaderBuffer& buffer : mShaderBuffers)
            if (buffer.BufferName == name)
                return buffer;

        SG_ASSERT_INTERNAL("ShaderBuffer with name {0} doesn't exist in shader!", name);
        return sDummyBuffer;
    }

    const ShaderBufferMember& ShaderReflectionData::GetBufferMember(const ShaderBuffer& buffer, const String& memberName) const
    {
        for (const ShaderBufferMember& member : buffer.Members)
            if (member.Name == memberName)
                return member;

        SG_ASSERT_INTERNAL("ShaderBufferMember with name {0} doesn't exist in {1} buffer!", memberName, buffer.BufferName);
        return sDummyBufferMember;
    }

    void ShaderReflectionData::LogAll() const
    {
        String header = std::format("------------------ {} ------------------", mShaderName);
        Log<Severity::Debug>(header);
        for (const auto& [stage, inputs] : mStageInputs)
        {
            for (const auto& [location, input] : inputs)
            {
                Log<Severity::Info>("Stage Input: {0}, Location: {1}, Type: {2}, Stage: {3}", input.Name, location, SurgeReflect::EnumToString(input.DataType), VulkanUtils::ShaderTypeToString(stage));
            }
        }
        for(const ShaderBuffer& buffer : mShaderBuffers)
        {
            Log<Severity::Info>("Buffer: {0}, Set: {1}, Binding: {2}, Size: {3}, Usage: {4}, Stages: {5}", buffer.BufferName, buffer.Set, buffer.Binding, buffer.Size,
                buffer.ShaderUsage == ShaderBuffer::Usage::STORAGE ? "Storage" : "Uniform", VulkanUtils::ShaderTypeToString(buffer.ShaderStages));
            for (const ShaderBufferMember& member : buffer.Members)
            {
                Log<Severity::Info>("\tMember: {0}, Offset: {1}, Size: {2}, Type: {3}", member.Name, member.MemoryOffset, member.Size, SurgeReflect::EnumToString(member.DataType));
            }
        }
        for (const ShaderResource& res : mShaderResources)
        {
            Log<Severity::Info>("Resource: {0}, Set: {1}, Binding: {2}, Usage: {3}, Stages: {4}", res.Name, res.Set, res.Binding,
                res.ShaderUsage == ShaderResource::Usage::SAMPLED ? "Sampled" : "Storage", VulkanUtils::ShaderTypeToString(res.ShaderStages));
        }
        for (const ShaderPushConstant& pushConstant : mPushConstants)
        {
            Log<Severity::Info>("Push Constant: {0}, Size: {1}, Stages: {2}", pushConstant.BufferName, pushConstant.Size, VulkanUtils::ShaderTypeToString(pushConstant.ShaderStages));
        }


        String footer(header.length(), '-');		
        Log<Severity::Debug>(footer);
    }

    void ShaderReflectionData::ClearRepeatedMembers()
    {
        // Just a simple O(n^2) algorithm to find the repeated members

        // For buffers
        for (Uint i = 0; i < mShaderBuffers.size(); i++)
        {
            for (Uint j = i + 1; j < mShaderBuffers.size(); j++)
            {
                ShaderBuffer& bufferData1 = mShaderBuffers[i];
                ShaderBuffer& bufferData2 = mShaderBuffers[j];

                // Check if the binding and set of the buffers is the same
                if (bufferData1.Set == bufferData2.Set && bufferData1.Binding == bufferData2.Binding)
                {
                    // If the binding and set of the buffer is the same, then we add the shaderstages from the second buffer into
                    // the first one (so like we combine them), and then erase the second buffer so we have only one combined
                    bufferData1.ShaderStages |= bufferData2.ShaderStages;

                    // Deleting the second copy of the same member
                    mShaderBuffers.erase(mShaderBuffers.begin() + j);
                }
            }
        }

        // For textures
        for (Uint i = 0; i < mShaderResources.size(); i++)
        {
            for (Uint j = i + 1; j < mShaderResources.size(); j++)
            {
                auto& textureData1 = mShaderResources[i];
                auto& textureData2 = mShaderResources[j];

                // Check if the binding and set of the textures is the same
                if (textureData1.Set == textureData2.Set && textureData1.Binding == textureData2.Binding)
                {
                    // If the binding and set of the texture is the same, then we add the shaderstages from the second texture into
                    // the first one (so like we combine them), and then erase the second texture so we have only one combined
                    textureData1.ShaderStages |= textureData2.ShaderStages;

                    // Deleting the second copy of the same member
                    mShaderResources.erase(mShaderResources.begin() + j);
                }
            }
        }

        // For Push Constants
        for (Uint i = 0; i < mPushConstants.size(); i++)
        {
            for (Uint j = i + 1; j < mPushConstants.size(); j++)
            {
                auto& pushConstant1 = mPushConstants[i];
                auto& pushConstant2 = mPushConstants[j];

                // Check if the size and the name of the push constants are the same
                if (pushConstant1.BufferName == pushConstant2.BufferName && pushConstant1.Size == pushConstant2.Size)
                {
                    // If the size and the name of the push constants are the same, then we add the shaderstages from the second push
                    // constant buffer into the first one (so like we combine them), and then erase the second pushconstant buffer so
                    // we have only one combined |Explanation 100|
                    pushConstant1.ShaderStages |= pushConstant2.ShaderStages;

                    // Deleting the second copy of the same member
                    mPushConstants.erase(mPushConstants.begin() + j);
                }
            }
        }
    }

    SURGE_RAPTOR_FIELDS(ShaderResource, v.Set, v.Binding, v.ArraySize, v.Name, v.ShaderUsage, v.ShaderStages)
    SURGE_RAPTOR_FIELDS(ShaderBufferMember, v.Name, v.MemoryOffset, v.DataType, v.Size)
    SURGE_RAPTOR_FIELDS(ShaderBuffer, v.Set, v.Binding, v.BufferName, v.Size, v.ShaderUsage, v.ShaderStages, v.Members)
    SURGE_RAPTOR_FIELDS(ShaderPushConstant, v.BufferName, v.Size, v.ShaderStages)
    SURGE_RAPTOR_FIELDS(ShaderStageInput, v.Name, v.Size, v.Offset, v.DataType)

    static constexpr ShaderType ALL_SHADER_STAGES = static_cast<ShaderType>(static_cast<Uint>(ShaderType::VERTEX) | static_cast<Uint>(ShaderType::FRAGMENT) | static_cast<Uint>(ShaderType::COMPUTE));

    void ShaderReflectionData::Rapt0RSerialize(Rapt0R::Writer& writer) const
    {
        writer.Write(mShaderName, mShaderResources, mShaderBuffers, mPushConstants, mStageInputs);
    }

    void ShaderReflectionData::Rapt0RDeserialize(Rapt0R::Reader& reader)
    {
        *this = ShaderReflectionData();
        if(!reader.Read(mShaderName, mShaderResources, mShaderBuffers, mPushConstants, mStageInputs))
            return;

        // Enums are range checked (stages against the full mask), a stale/corrupt file must not produce out of range values
        bool valid = true;
        for(const ShaderResource& res : mShaderResources)
            valid = valid && IsEnumInRange(res.ShaderUsage, ShaderResource::Usage::STORAGE) && IsEnumInRange(res.ShaderStages, ALL_SHADER_STAGES);
        for(const ShaderBuffer& buffer : mShaderBuffers)
        {
            valid = valid && IsEnumInRange(buffer.ShaderUsage, ShaderBuffer::Usage::UNIFORM) && IsEnumInRange(buffer.ShaderStages, ALL_SHADER_STAGES);
            for(const ShaderBufferMember& member : buffer.Members)
                valid = valid && IsEnumInRange(member.DataType, ShaderDataType::STRUCT);
        }
        for(const ShaderPushConstant& pushConstant : mPushConstants)
            valid = valid && IsEnumInRange(pushConstant.ShaderStages, ALL_SHADER_STAGES);
        for(const auto& [stage, inputs] : mStageInputs)
        {
            valid = valid && IsEnumInRange(stage, ALL_SHADER_STAGES);
            for(const auto& [location, input] : inputs)
                valid = valid && IsEnumInRange(input.DataType, ShaderDataType::STRUCT);
        }

        if(!valid)
            reader.SetError(Rapt0R::ErrorCode::CORRUPT_DATA);
    }
} // namespace Surge
