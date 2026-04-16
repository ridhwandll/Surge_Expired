// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Graphics/Shader/Shader.hpp"
#include <map>

namespace Surge
{
    enum class SURGE_API ShaderType;

    // Represents Textures
    struct ShaderResource
    {
        enum class SURGE_API Usage
        {
            Sampled,
            Storage
        };

        Uint Set = 0;
        Uint Binding = 0;
        String Name;
        ShaderResource::Usage ShaderUsage;
        ShaderType ShaderStages {}; // Specify what shader stages the resource is being used for
    };

    struct ShaderStageInput
    {
        String Name = "None";
        Uint Size = 0;
        Uint Offset = 0;
        ShaderDataType DataType = ShaderDataType::None;
    };

    // Represents a ConstantBuffer Member
    struct ShaderBufferMember
    {
        String Name;
        Uint MemoryOffset = 0;
        ShaderDataType DataType = ShaderDataType::None;
        Uint Size = 0;
    };

    // Represents a ConstantBuffer
    struct ShaderBuffer
    {
        enum class SURGE_API Usage
        {
            Storage,
            Uniform
        };

        Uint Set = 0;
        Uint Binding = 0;
        String BufferName;
        Uint Size = 0;
        Vector<ShaderBufferMember> Members = {};
        ShaderBuffer::Usage ShaderUsage;
        ShaderType ShaderStages {}; // Specify what shader stages the buffer is being used for

        const ShaderBufferMember* GetMember(const String& name)
        {
            for (const ShaderBufferMember& member : Members)
            {
                if (member.Name == name)
                    return &member;
            }
            return nullptr;
        }
    };

    struct ShaderPushConstant
    {
        String BufferName;
        Uint Size = 0;
        ShaderType ShaderStages {}; // Specify what shader stages the buffer is being used for
    };

    class SURGE_API ShaderReflectionData
    {
    public:
        ShaderReflectionData() = default;
        ~ShaderReflectionData() = default;

        void PushResource(const ShaderResource& res) { mShaderResources.push_back(res); }
        void PushStageInput(const ShaderStageInput& input, const ShaderType& stage, Uint location) { mStageInputs[stage][location] = input; }
        void PushBuffer(const ShaderBuffer& buffer)
        {
            SG_ASSERT(!buffer.BufferName.empty() || buffer.Members.size() != 0 || buffer.Size != 0, "ShaderBuffer is invalid!");
            mShaderBuffers.push_back(buffer);
        }
        void PushBufferPushConstant(const ShaderPushConstant& pushConstant) { mPushConstants.push_back(pushConstant); }

        const ShaderBuffer& GetBuffer(const String& name) const;
        const Vector<ShaderBuffer>& GetBuffers() const { return mShaderBuffers; }
        const Vector<ShaderPushConstant> GetPushConstantBuffers() const { return mPushConstants; }
        const ShaderBufferMember& GetBufferMember(const ShaderBuffer& buffer, const String& memberName) const;
        const Vector<ShaderResource>& GetResources() const { return mShaderResources; }
        const HashMap<ShaderType, std::map<Uint, ShaderStageInput>>& GetStageInputs() const { return mStageInputs; }
        const Vector<Uint> GetDescriptorSets() const { return mDescriptorSets; }

    private:
        void ClearRepeatedMembers();
        void CalculateDescriptorSetCount();

    private:
        Vector<ShaderResource> mShaderResources {};
        Vector<ShaderBuffer> mShaderBuffers {};
        Vector<ShaderPushConstant> mPushConstants;

        // NOTE(AC3R): Keeping track of how many descriptor set we will need for the descriptor layout
        Vector<Uint> mDescriptorSets;

        // Stage inputs, per shader stage
        HashMap<ShaderType, std::map<Uint /*location*/, ShaderStageInput /*Data*/>> mStageInputs {};
        friend class SURGE_API ShaderReflector;
    };
} // namespace Surge