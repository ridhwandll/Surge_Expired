// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Abstraction/Vulkan/VulkanShader.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanDevice.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanDiagnostics.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanUtils.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Core.hpp"
#include <filesystem>

#ifdef SURGE_PLATFORM_WINDOWS
#include <shaderc/shaderc.hpp>
#elif defined(SURGE_PLATFORM_ANDROID)
#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#include <android/log.h>
#endif


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

        // DescriptorSetLayouts doesn't get recreated when the shader is reloaded, thus it is outside the Clear() function
        VulkanRenderContext* renderContext;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VkDevice device = renderContext->GetDevice()->GetLogicalDevice();
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

        // We want to make sure that the DescriptorSetLayouts doesn't get recreated when the shader is reloaded
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

#ifdef SURGE_PLATFORM_WINDOWS
    static shaderc_shader_kind ShadercShaderKindFromSurgeShaderType(const ShaderType& type)
    {
        switch (type)
        {
            case ShaderType::Vertex: return shaderc_glsl_vertex_shader;
            case ShaderType::Pixel: return shaderc_glsl_fragment_shader;
            case ShaderType::Compute: return shaderc_glsl_compute_shader;
            case ShaderType::None: SG_ASSERT_INTERNAL("ShaderType::None is invalid in this case!");
        }

        return static_cast<shaderc_shader_kind>(-1);
    }
#endif

    void VulkanShader::Compile(const HashMap<ShaderType, bool>& compileStages)
    {

        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VkDevice device = renderContext->GetDevice()->GetLogicalDevice();
#ifdef SURGE_PLATFORM_WINDOWS
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;
        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
        bool saveHash = false;

        // NOTE(Rid - AC3R) If we enable optimization, it removes the name :kekCry:
        // options.SetOptimizationLevel(shaderc_optimization_level_performance);

        for (auto&& [stage, source] : mShaderSources)
        {
            SPIRVHandle spirvHandle;
            spirvHandle.Type = stage;
            bool compile = true; // Default is "true", shader should be compiled if not specified in compileStages

            // If compileStages is not empty, then try to find the "compile" bool
            // This "compile" bool determines if the shader should be recompiled or not
            if (!compileStages.empty())
            {
                auto itr = compileStages.find(stage);
                if (itr != compileStages.end())
                    compile = compileStages.at(stage);
            }

            // Load or create the SPIRV
            if (compile)
            {
                // Compile, not present in cache
                shaderc::CompilationResult result = compiler.CompileGlslToSpv(source, ShadercShaderKindFromSurgeShaderType(stage), mPath, options);
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
                // Load from cache
                String name = fmt::format("{0}.{1}.spv", Filesystem::GetNameWithExtension(mPath), ShaderTypeToString(stage));
                String path = fmt::format("{0}/{1}", SHADER_CACHE_PATH, name);
                spirvHandle.SPIRV = Filesystem::ReadFile<Vector<Uint>>(path);
            }
            SG_ASSERT(!spirvHandle.SPIRV.empty(), "Invalid SPIRV!");

            // Create the VkShaderModule
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
                if (dir.path().extension() != ".spv") // Early exit if it's not a spirv file
                    continue;

                // Get the shader type from filename
                String shaderStageString = std::filesystem::path(Filesystem::RemoveExtension(chachedShaderPath)).extension().string().substr(1, std::string::npos);
                ShaderType shaderStage = VulkanUtils::ShaderTypeFromString(shaderStageString + "]"); //TODO: remove this hack

                // Get the correct cache name for this shader
                String cacheName = fmt::format("{0}.{1}.spv", Filesystem::GetNameWithExtension(mPath), ShaderTypeToString(shaderStage));
                String cachePath = fmt::format("{0}/{1}", SHADER_CACHE_PATH, cacheName);
                std::replace(chachedShaderPath.begin(), chachedShaderPath.end(), '\\', '/');

                // We are iterating the whole shader cache directory, so skip the other files
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
#elif defined(SURGE_PLATFORM_ANDROID)
        {
            auto options = Core::GetClient()->GeClientOptions();
            android_app* app = static_cast<android_app*>(options.AndroidApp);
            AAssetManager* assetManager = app->activity->assetManager;
            AAssetDir* assetDir = AAssetManager_openDir(assetManager, "Engine/Assets/Shaders"); // Path relative to assets folder
            const char* filename = nullptr;
            while ((filename = AAssetDir_getNextFileName(assetDir)) != nullptr)
            {
                LOGW(filename);
                std::string fileNameStr(filename);

                // 1. Filter for .spv extension
                if (fileNameStr.find(".spv") == std::string::npos)
                    continue;

                // 2. Open the asset
                std::string fullPath = "Engine/Assets/Shaders/" + fileNameStr;
                AAsset* asset = AAssetManager_open(assetManager, fullPath.c_str(), AASSET_MODE_BUFFER);
                if (!asset)
                    continue;

                // 3. Read the buffer
                size_t size = AAsset_getLength(asset);
                if (size % 4 != 0)
                {
                    //LOGE("Shader %s size is not a multiple of 4!", filename);
                    AAsset_close(asset);
                    continue;
                }

                Vector<Uint> spirvCode(size / sizeof(uint32_t));
                AAsset_read(asset, spirvCode.data(), size);

                AAsset_close(asset);

                String shaderStageString = std::filesystem::path(Filesystem::RemoveExtension(fullPath)).extension().string().substr(1, std::string::npos);
                ShaderType shaderStage = VulkanUtils::ShaderTypeFromString(shaderStageString + "]"); // TODO: remove this hack

                SPIRVHandle spirvHandle;
                spirvHandle.SPIRV = spirvCode;
                spirvHandle.Type = shaderStage;
                mShaderSPIRVs.push_back(spirvHandle);

                VkShaderModuleCreateInfo createInfo {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
                createInfo.codeSize = spirvHandle.SPIRV.size() * sizeof(Uint);
                createInfo.pCode = spirvHandle.SPIRV.data();
                VK_CALL(vkCreateShaderModule(device, &createInfo, nullptr, &mVkShaderModules[shaderStage]));
                SET_VK_OBJECT_DEBUGNAME(mVkShaderModules.at(shaderStage), VK_OBJECT_TYPE_SHADER_MODULE, "Vulkan Shader");
            }
        }
#endif
        ShaderReflector reflector;
        mReflectionData = reflector.Reflect(mShaderSPIRVs);
    }

    void VulkanShader::Clear()
    {
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VkDevice device = renderContext->GetDevice()->GetLogicalDevice();

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
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VkDevice device = renderContext->GetDevice()->GetLogicalDevice();

        // Iterate through all the sets and creating the layouts
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
                layoutBinding.descriptorCount = 1; // TODO: Need to add arrays
                layoutBinding.descriptorType = VulkanUtils::ShaderBufferTypeToVulkan(buffer.ShaderUsage);
                layoutBinding.stageFlags = VulkanUtils::GetShaderStagesFlagsFromShaderTypes(buffer.ShaderStages) | VK_SHADER_STAGE_ALL;
            }

            for (const ShaderResource& texture : mReflectionData.GetResources())
            {
                if (texture.Set != descriptorSet)
                    continue;

                VkDescriptorSetLayoutBinding& LayoutBinding = layoutBindings.emplace_back();
                LayoutBinding.binding = texture.Binding;
                LayoutBinding.descriptorCount = 1; // TODO: Need to add arrays
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
            //pushConstantRange.stageFlags = VulkanUtils::GetShaderStagesFlagsFromShaderTypes(pushConstant.ShaderStages);
            // TODO: We want to specify the stage flags correctly, but currently we have some issues with shader 
            // reflection that causes the stage flags to be incorrect on mobile, so for now we will just set it to all stages
            pushConstantRange.stageFlags = VK_SHADER_STAGE_ALL; 
          
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