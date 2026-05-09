// Copyright (c) - SurgeTechnologies - All rights reserved
#include "ShaderReflector.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanUtils.hpp"
#include <SPIRV-Cross/spirv_glsl.hpp>


namespace Surge
{
	static ShaderDataType SPVTypeToShaderDataType(const spirv_cross::SPIRType& spvType)
	{
		if (spvType.basetype == spirv_cross::SPIRType::Float && spvType.vecsize == 1 && spvType.columns == 1)
			return ShaderDataType::FLOAT;
		if (spvType.basetype == spirv_cross::SPIRType::Float && spvType.vecsize == 2 && spvType.columns == 1)
			return ShaderDataType::FLOAT2;
		if (spvType.basetype == spirv_cross::SPIRType::Float && spvType.vecsize == 3 && spvType.columns == 1)
			return ShaderDataType::FLOAT3;
		if (spvType.basetype == spirv_cross::SPIRType::Float && spvType.vecsize == 4 && spvType.columns == 1)
			return ShaderDataType::FLOAT4;
		if (spvType.basetype == spirv_cross::SPIRType::Float && spvType.vecsize == 2 && spvType.columns == 2)
			return ShaderDataType::MAT2;
		if (spvType.basetype == spirv_cross::SPIRType::Float && spvType.vecsize == 3 && spvType.columns == 3)
			return ShaderDataType::MAT3;
		if (spvType.basetype == spirv_cross::SPIRType::Float && spvType.vecsize == 4 && spvType.columns == 4)
			return ShaderDataType::MAT4;
		if (spvType.basetype == spirv_cross::SPIRType::UInt && spvType.vecsize == 1 && spvType.columns == 1)
			return ShaderDataType::UINT;
		if (spvType.basetype == spirv_cross::SPIRType::Int && spvType.vecsize == 1 && spvType.columns == 1)
			return ShaderDataType::INT;
		if (spvType.basetype == spirv_cross::SPIRType::Boolean && spvType.vecsize == 1 && spvType.columns == 1)
			return ShaderDataType::BOOL;
		if (spvType.basetype == spirv_cross::SPIRType::Struct)
			return ShaderDataType::STRUCT;

		SG_ASSERT_INTERNAL("No spirv_cross::SPIRType matches Surge::ShaderDataType!");
		return ShaderDataType::NONE;
	}

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

	void ShaderReflectionData::LogAll()
	{
		String header = std::format("------------------ {} ------------------", mShaderName);
		Log<Severity::Debug>(header);
		for (const auto& [stage, inputs] : mStageInputs)
		{
			for (const auto& [location, input] : inputs)
			{
				Log<Severity::Info>("Stage Input: {0}, Location: {1}, Type: {2}, Stage: {3}", input.Name, location, VulkanUtils::ShaderDataTypeToString(input.DataType), VulkanUtils::ShaderTypeToString(stage));
			}
		}
		for(const ShaderBuffer& buffer : mShaderBuffers)
		{
			Log<Severity::Info>("Buffer: {0}, Set: {1}, Binding: {2}, Size: {3}, Usage: {4}, Stages: {5}", buffer.BufferName, buffer.Set, buffer.Binding, buffer.Size,
				buffer.ShaderUsage == ShaderBuffer::Usage::Storage ? "Storage" : "Uniform", VulkanUtils::ShaderTypeToString(buffer.ShaderStages));
			for (const ShaderBufferMember& member : buffer.Members)
			{
				Log<Severity::Info>("\tMember: {0}, Offset: {1}, Size: {2}, Type: {3}", member.Name, member.MemoryOffset, member.Size, VulkanUtils::ShaderDataTypeToString(member.DataType));
			}
		}
		for (const ShaderResource& res : mShaderResources)
		{
			Log<Severity::Info>("Resource: {0}, Set: {1}, Binding: {2}, Usage: {3}, Stages: {4}", res.Name, res.Set, res.Binding,
				res.ShaderUsage == ShaderResource::Usage::Sampled ? "Sampled" : "Storage", VulkanUtils::ShaderTypeToString(res.ShaderStages));
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

	void ShaderReflectionData::CalculateDescriptorSetCount()
	{
		// Adding all the sets used in the shader needed to make the amount of descriptor layout/sets
		for (const ShaderBuffer& buffer : mShaderBuffers)
		{
			// Check if the number of the set is already mentioned in the vector
			if (std::find(mDescriptorSets.begin(), mDescriptorSets.end(), buffer.Set) == mDescriptorSets.end())
				mDescriptorSets.push_back(buffer.Set);
		}

		for (const ShaderResource& res : mShaderResources)
		{
			// Check if the number of the set is already mentioned in the vector
			if (std::find(mDescriptorSets.begin(), mDescriptorSets.end(), res.Set) == mDescriptorSets.end())
				mDescriptorSets.push_back(res.Set);
		}
	}

	////
	// ShaderReflector
	////

    ShaderReflectionData ShaderReflector::Reflect(const String& shaderName, const Vector<SPIRVHandle>& spirvHandles)
    {
        ShaderReflectionData result;
		result.SetShaderName(shaderName);
        for (auto& handle : spirvHandles)
        {
            spirv_cross::Compiler compiler(handle.SPIRV);
            spirv_cross::ShaderResources resources = compiler.get_shader_resources();

            // Fetch the sampled textures
            for (const spirv_cross::Resource& resource : resources.sampled_images)
            {
                ShaderResource res;
                res.Binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
                res.Set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
                res.Name = resource.name;
                res.ShaderStages |= handle.Type;
                res.ShaderUsage = ShaderResource::Usage::Sampled;
                result.PushResource(res);
            }

            // Fetch the storage textures
            for (const spirv_cross::Resource& resource : resources.storage_images)
            {
                ShaderResource res;
                res.Binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
                res.Set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
                res.Name = resource.name;
                res.ShaderStages |= handle.Type;
                res.ShaderUsage = ShaderResource::Usage::Storage;
                result.PushResource(res);
            }

            // Fetch all the Uniform/Constant buffers
            for (const spirv_cross::Resource& resource : resources.uniform_buffers)
            {
                ShaderBuffer buffer;
                const spirv_cross::SPIRType& bufferType = compiler.get_type(resource.base_type_id);

                buffer.Size = static_cast<Uint>(compiler.get_declared_struct_size(bufferType));
                buffer.Set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
                buffer.Binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
                buffer.BufferName = resource.name;
                buffer.ShaderStages |= handle.Type;
                buffer.ShaderUsage = ShaderBuffer::Usage::Uniform;

                for (Uint i = 0; i < bufferType.member_types.size(); i++)
                {
                    const spirv_cross::SPIRType& spvType = compiler.get_type(bufferType.member_types[i]);

                    ShaderBufferMember bufferMember;
                    bufferMember.Name = buffer.BufferName + '.' + compiler.get_member_name(bufferType.self, i);
                    bufferMember.MemoryOffset = compiler.type_struct_member_offset(bufferType, i); // In bytes
                    bufferMember.DataType = SPVTypeToShaderDataType(spvType);
                    bufferMember.Size = VulkanUtils::ShaderDataTypeSize(bufferMember.DataType);
                    buffer.Members.emplace_back(bufferMember);
                }
                result.PushBuffer(buffer);
            }

            // Fetch all the Storage buffers
            for (const spirv_cross::Resource& resource : resources.storage_buffers)
            {
                ShaderBuffer buffer;
                const spirv_cross::SPIRType& bufferType = compiler.get_type(resource.base_type_id);

                buffer.Size = static_cast<Uint>(compiler.get_declared_struct_size(bufferType));
                buffer.Set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
                buffer.Binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
                buffer.BufferName = resource.name;
                buffer.ShaderStages |= handle.Type;
                buffer.ShaderUsage = ShaderBuffer::Usage::Storage;

                for (Uint i = 0; i < bufferType.member_types.size(); i++)
                {
                    const spirv_cross::SPIRType& spvType = compiler.get_type(bufferType.member_types[i]);

                    ShaderBufferMember bufferMember;
                    bufferMember.Name = buffer.BufferName + '.' + compiler.get_member_name(bufferType.self, i);
                    bufferMember.MemoryOffset = compiler.type_struct_member_offset(bufferType, i); // In bytes
                    bufferMember.DataType = SPVTypeToShaderDataType(spvType);
                    bufferMember.Size = VulkanUtils::ShaderDataTypeSize(bufferMember.DataType);
                    buffer.Members.emplace_back(bufferMember);
                }

                result.PushBuffer(buffer);
            }

            // Fetch the StageInputs
            for (const spirv_cross::Resource& resource : resources.stage_inputs)
            {
                ShaderStageInput stageInput;

                const spirv_cross::SPIRType& spvType = compiler.get_type(resource.base_type_id);
                Uint location = compiler.get_decoration(resource.id, spv::DecorationLocation);
                stageInput.Name = resource.name;
                stageInput.DataType = SPVTypeToShaderDataType(spvType);
                stageInput.Size = stageInput.DataType == ShaderDataType::STRUCT ? 0 : VulkanUtils::ShaderDataTypeSize(stageInput.DataType);
                stageInput.Offset = 0; // temporary, calculated later

                result.PushStageInput(stageInput, handle.Type, location);
            }

            // Calculating the offsets after the locations are sorted

            Uint elementOffset = 0;
            auto itr = result.mStageInputs.find(handle.Type);
            if (itr != result.mStageInputs.end())
            {
                for (auto& [location, stageInput] : result.mStageInputs.at(handle.Type))
                {
                    stageInput.Offset = elementOffset;
                    elementOffset += stageInput.Size;
                }
            }

            // Fetch Push Constants
            for (const spirv_cross::Resource& resource : resources.push_constant_buffers)
            {
                ShaderPushConstant pushConstant;
                const spirv_cross::SPIRType& bufferType = compiler.get_type(resource.base_type_id);

                pushConstant.BufferName = resource.name;
                pushConstant.Size = static_cast<Uint>(compiler.get_declared_struct_size(bufferType));
                pushConstant.ShaderStages |= handle.Type;
                result.PushBufferPushConstant(pushConstant);
            }
        }

        result.ClearRepeatedMembers();
        result.CalculateDescriptorSetCount();
        return result;
    }

} // namespace Surge