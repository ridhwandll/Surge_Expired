// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include <map>

namespace Surge
{
	enum class ShaderType
	{
		NONE = 0,
		VERTEX = BIT(0),
		FRAGMENT = BIT(1),
		COMPUTE = BIT(2)
	};
	MAKE_BIT_ENUM(ShaderType, uint16_t)

	struct SPIRVHandle
	{
		Vector<Uint> SPIRV;
		ShaderType Type;
	};

	enum class ShaderDataType
	{
		NONE,
		INT,
		UINT,
		FLOAT,
		FLOAT2,
		FLOAT3,
		FLOAT4,
		MAT2,
		MAT4,
		MAT3,
		BOOL,
		STRUCT
	};

	// Represents Textures
	struct ShaderResource
	{
		enum class Usage
		{
			SAMPLED,
			STORAGE
		};

		Uint Set = 0;
		Uint Binding = 0;
		Uint ArraySize = 1;
		String Name;
		ShaderResource::Usage ShaderUsage;
		ShaderType ShaderStages{}; // Specifies what shader stages the resource is being used for
	};

	struct ShaderStageInput
	{
		String Name = "None";
		Uint Size = 0;
		Uint Offset = 0;
		ShaderDataType DataType = ShaderDataType::NONE;
	};

	// Represents a ConstantBuffer Member
	struct ShaderBufferMember
	{
		String Name;
		Uint MemoryOffset = 0;
		ShaderDataType DataType = ShaderDataType::NONE;
		Uint Size = 0;
	};

	// Represents a ConstantBuffer
	struct ShaderBuffer
	{
		enum class SURGE_API Usage
		{
			STORAGE,
			UNIFORM
		};

		Uint Set = 0;
		Uint Binding = 0;
		String BufferName;
		Uint Size = 0;
		Vector<ShaderBufferMember> Members = {};
		ShaderBuffer::Usage ShaderUsage;
		ShaderType ShaderStages{}; // Specifies what shader stages the buffer is being used for

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
		ShaderType ShaderStages{}; // Specifies what shader stages the buffer is being used for
	};

	class ShaderReflectionData
	{
	public:
		ShaderReflectionData() = default;
		~ShaderReflectionData() = default;

		const ShaderBuffer& GetBuffer(const String& name) const;
		const Vector<ShaderBuffer>& GetBuffers() const { return mShaderBuffers; }
		const Vector<ShaderPushConstant> GetPushConstantBuffers() const { return mPushConstants; }
		const ShaderBufferMember& GetBufferMember(const ShaderBuffer& buffer, const String& memberName) const;
		const Vector<ShaderResource>& GetResources() const { return mShaderResources; }
		const HashMap<ShaderType, std::map<Uint, ShaderStageInput>>& GetStageInputs() const { return mStageInputs; }
		const Vector<Uint> GetDescriptorSets() const { return mDescriptorSets; }

		void LogAll();
	private:
		void SetShaderName(const String& name) { mShaderName = name; }
		void PushResource(const ShaderResource& res) { mShaderResources.push_back(res); }
		void PushStageInput(const ShaderStageInput& input, const ShaderType& stage, Uint location) { mStageInputs[stage][location] = input; }
		void PushBuffer(const ShaderBuffer& buffer)
		{
			SG_ASSERT(!buffer.BufferName.empty() || buffer.Members.size() != 0 || buffer.Size != 0, "ShaderBuffer is invalid!");
			mShaderBuffers.push_back(buffer);
		}
		void PushBufferPushConstant(const ShaderPushConstant& pushConstant) { mPushConstants.push_back(pushConstant); }
		void ClearRepeatedMembers();
		void CalculateDescriptorSetCount();

	private:
		String mShaderName;
		Vector<ShaderResource> mShaderResources{};
		Vector<ShaderBuffer> mShaderBuffers{};
		Vector<ShaderPushConstant> mPushConstants;

		// NOTE(AC3R): Keeping track of how many descriptor set we will need for the descriptor layout
		Vector<Uint> mDescriptorSets;

		// Stage inputs, per shader stage
		HashMap<ShaderType, std::map<Uint /*location*/, ShaderStageInput /*Data*/>> mStageInputs{};
		friend class ShaderReflector;
	};

    class ShaderReflector
    {
    public:
        ShaderReflector() = default;
        ShaderReflectionData Reflect(const String& shaderName, const Vector<SPIRVHandle>& spirvHandles);
    };

} // namespace Surge
