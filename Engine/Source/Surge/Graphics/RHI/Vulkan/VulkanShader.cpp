// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/Vulkan/VulkanShader.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHIDevice.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanDiagnostics.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanUtils.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include <shaderc/shaderc.hpp>
#include <filesystem>

#ifdef SURGE_DEBUG
#define SHADER_LOG(...) Log<Severity::Debug>(__VA_ARGS__);
#else
#define SHADER_LOG(...)
#endif

namespace Surge
{
    VulkanShader::VulkanShader(const Path& path)
        : mPath(path), mCreatedDescriptorSetLayouts(false)
    {
        ParseShader();
    }

    VulkanShader::~VulkanShader()
    {
        SG_ASSERT(mCallbacks.empty(), "Callbacks must be empty! Did you forgot to call 'RemoveReloadCallback(id);' somewhere?");
        Clear();

        VkDevice device = RHI::gVulkanRHIDevice.GetLogicalDevice();
        for (auto& descriptorSetLayout : mDescriptorSetLayouts)
        {
            if (descriptorSetLayout.second)
                vkDestroyDescriptorSetLayout(device, descriptorSetLayout.second, nullptr);
        }
        mDescriptorSetLayouts.clear();
        mPushConstants.clear();
    }

    void VulkanShader::Load(const HashMap<ShaderType, bool>& compileStages)
    {
        SCOPED_TIMER("Shader({0}) Compilation", Filesystem::GetNameWithExtension(mPath));
        Clear();
        ParseShader();
        Compile(compileStages);

        if (!mCreatedDescriptorSetLayouts)
        {
            CreateVulkanDescriptorSetLayouts();
            CreateVulkanPushConstantRanges();
        }
    }

    void VulkanShader::Reload()
    {
        Load();
        for (const auto& [id, callback] : mCallbacks)
            callback();
    }

    Surge::UUID VulkanShader::AddReloadCallback(const std::function<void()> callback)
    {
        UUID id = UUID();
        mCallbacks[id] = callback;
        return id;
    }

    void VulkanShader::RemoveReloadCallback(const UUID& id)
    {
        auto itr = mCallbacks.find(id);
        if (itr != mCallbacks.end())
        {
            mCallbacks.erase(id);
            return;
        }
        SG_ASSERT_INTERNAL("Invalid UUID!");
    }

    void VulkanShader::Compile(const HashMap<ShaderType, bool>& compileStages)
    {
        VkDevice device = RHI::gVulkanRHIDevice.GetLogicalDevice();

        shaderc::Compiler compiler;
        shaderc::CompileOptions options;
        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);

        for (auto&& [stage, source] : mShaderSources)
        {
            SPIRVHandle spirvHandle;
            spirvHandle.Type = stage;
            bool compile = true;

            if (!compileStages.empty())
            {
                auto itr = compileStages.find(stage);
                if (itr != compileStages.end())
                    compile = compileStages.at(stage);
            }

            if (compile)
            {
                shaderc::CompilationResult result = compiler.CompileGlslToSpv(source, VulkanUtils::ShadercShaderKindFromSurgeShaderType(stage), mPath, options);
                if (result.GetCompilationStatus() != shaderc_compilation_status_success)
                {
                    Log<Severity::Error>("{0} Shader compilation failure!", VulkanUtils::ShaderTypeToString(stage));
                    Log<Severity::Error>("{0} Error(s): \n{1}", result.GetNumErrors(), result.GetErrorMessage());
                    SG_ASSERT_INTERNAL("Shader Compilation failure!");
                }
                else
                    spirvHandle.SPIRV = Vector<Uint>(result.cbegin(), result.cend());
            }
            else
            {
                String name = fmt::format("{0}.{1}.spv", Filesystem::GetNameWithExtension(mPath), ShaderTypeToString(stage));
                String path = fmt::format("{0}/{1}", SHADER_CACHE_PATH, name);
                spirvHandle.SPIRV = Filesystem::ReadFile<Vector<Uint>>(path);
            }
            SG_ASSERT(!spirvHandle.SPIRV.empty(), "Invalid SPIRV!");

            VkShaderModuleCreateInfo createInfo {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            createInfo.codeSize = spirvHandle.SPIRV.size() * sizeof(Uint);
            createInfo.pCode = spirvHandle.SPIRV.data();
            VK_CALL(vkCreateShaderModule(device, &createInfo, nullptr, &mVkShaderModules[stage]));
            SET_VK_OBJECT_DEBUGNAME(mVkShaderModules.at(stage), VK_OBJECT_TYPE_SHADER_MODULE, "Vulkan Shader");
            mShaderSPIRVs.push_back(spirvHandle);
        }
        if (mShaderSources.empty())
        {
            for (auto& dir : std::filesystem::directory_iterator(SHADER_CACHE_PATH))
            {
                String chachedShaderPath = dir.path().string();
                if (dir.path().extension() != ".spv")
                    continue;

                String shaderStageString = std::filesystem::path(Filesystem::RemoveExtension(chachedShaderPath)).extension().string().substr(1, std::string::npos);
                ShaderType shaderStage = VulkanUtils::ShaderTypeFromString(shaderStageString + "]");

                String cacheName = fmt::format("{0}.{1}.spv", Filesystem::GetNameWithExtension(mPath), ShaderTypeToString(shaderStage));
                String cachePath = fmt::format("{0}/{1}", SHADER_CACHE_PATH, cacheName);
                std::replace(chachedShaderPath.begin(), chachedShaderPath.end(), '\\', '/');

                if (cachePath != chachedShaderPath)
                    continue;

                SPIRVHandle spirvHandle;
                spirvHandle.SPIRV = Filesystem::ReadFile<Vector<Uint>>(chachedShaderPath);
                spirvHandle.Type = shaderStage;
                mShaderSPIRVs.push_back(spirvHandle);

                VkShaderModuleCreateInfo createInfo {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
                createInfo.codeSize = spirvHandle.SPIRV.size() * sizeof(Uint);
                createInfo.pCode = spirvHandle.SPIRV.data();
                VK_CALL(vkCreateShaderModule(device, &createInfo, nullptr, &mVkShaderModules[shaderStage]));
                SET_VK_OBJECT_DEBUGNAME(mVkShaderModules.at(shaderStage), VK_OBJECT_TYPE_SHADER_MODULE, "Vulkan Shader");
            }
        }
        ShaderReflector reflector;
        mReflectionData = reflector.Reflect(mShaderSPIRVs);
    }

    void VulkanShader::Clear()
    {
        VkDevice device = RHI::gVulkanRHIDevice.GetLogicalDevice();

        mShaderSources.clear();
        mShaderSPIRVs.clear();

        for (auto&& [stage, source] : mVkShaderModules)
        {
            if (mVkShaderModules[stage])
                vkDestroyShaderModule(device, mVkShaderModules[stage], nullptr);
        }

        mVkShaderModules.clear();
    }

    void VulkanShader::CreateVulkanDescriptorSetLayouts()
    {
        VkDevice device = RHI::gVulkanRHIDevice.GetLogicalDevice();

        const Vector<Uint>& descriptorSetCount = mReflectionData.GetDescriptorSets();

        for (const Uint& descriptorSet : descriptorSetCount)
        {
            Vector<VkDescriptorSetLayoutBinding> layoutBindings;
            for (const ShaderBuffer& buffer : mReflectionData.GetBuffers())
            {
                if (buffer.Set != descriptorSet)
                    continue;

                VkDescriptorSetLayoutBinding& layoutBinding = layoutBindings.emplace_back();
                layoutBinding.binding = buffer.Binding;
                layoutBinding.descriptorCount = 1;
                layoutBinding.descriptorType = VulkanUtils::ShaderBufferTypeToVulkan(buffer.ShaderUsage);
                layoutBinding.stageFlags = VulkanUtils::GetShaderStagesFlagsFromShaderTypes(buffer.ShaderStages) | VK_SHADER_STAGE_ALL;
            }

            for (const ShaderResource& texture : mReflectionData.GetResources())
            {
                if (texture.Set != descriptorSet)
                    continue;

                VkDescriptorSetLayoutBinding& LayoutBinding = layoutBindings.emplace_back();
                LayoutBinding.binding = texture.Binding;
                LayoutBinding.descriptorCount = 1;
                LayoutBinding.descriptorType = VulkanUtils::ShaderImageUsageToVulkan(texture.ShaderUsage);
                LayoutBinding.stageFlags = VulkanUtils::GetShaderStagesFlagsFromShaderTypes(texture.ShaderStages) | VK_SHADER_STAGE_ALL;
            }

            VkDescriptorSetLayoutCreateInfo layoutInfo {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            layoutInfo.flags = 0;
            layoutInfo.bindingCount = static_cast<Uint>(layoutBindings.size());
            layoutInfo.pBindings = layoutBindings.data();

            VK_CALL(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &mDescriptorSetLayouts[descriptorSet]));
        }
        mCreatedDescriptorSetLayouts = true;
    }

    void VulkanShader::CreateVulkanPushConstantRanges()
    {
        for (const ShaderPushConstant& pushConstant : mReflectionData.GetPushConstantBuffers())
        {
            VkPushConstantRange& pushConstantRange = mPushConstants[pushConstant.BufferName];
            pushConstantRange.offset = 0;
            pushConstantRange.size = pushConstant.Size;
            pushConstantRange.stageFlags = VulkanUtils::GetShaderStagesFlagsFromShaderTypes(pushConstant.ShaderStages);
        }
    }

    void VulkanShader::ParseShader()
    {
        String source = Filesystem::ReadFile<String>(mPath);

        const char* typeToken = "[SurgeShader:";
        size_t typeTokenLength = strlen(typeToken);
        size_t pos = source.find(typeToken, 0);
        while (pos != std::string::npos)
        {
            size_t eol = source.find_first_of("\r\n", pos);
            size_t begin = pos + typeTokenLength + 1;
            String type = source.substr(begin, eol - begin);
            size_t nextLinePos = source.find_first_not_of("\r\n", eol);

            ShaderType shaderType = VulkanUtils::ShaderTypeFromString(type);
            SG_ASSERT((int)shaderType, "Invalid shader type!");
            pos = source.find(typeToken, nextLinePos);
            mShaderSources[shaderType] = (pos == std::string::npos) ? source.substr(nextLinePos) : source.substr(nextLinePos, pos - nextLinePos);
            mHashCodes[shaderType] = Hash().Generate<String>(mShaderSources.at(shaderType));
            mTypesBit |= shaderType;
        }
    }
} // namespace Surge
