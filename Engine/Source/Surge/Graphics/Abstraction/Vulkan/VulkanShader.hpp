// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/Shader/Shader.hpp"
#include "Surge/Graphics/Shader/ShaderReflector.hpp"
#include <volk.h>

namespace Surge
{
    class SURGE_API VulkanShader : public Shader
    {
    public:
        VulkanShader(const Path& path);
        virtual ~VulkanShader() override;

        virtual void Load(const HashMap<ShaderType, bool>& compileStages = {}) override;
        virtual void Reload() override;
        virtual UUID AddReloadCallback(const std::function<void()> callback) override;
        virtual void RemoveReloadCallback(const UUID& id) override;
        virtual const ShaderReflectionData& GetReflectionData() const override { return mReflectionData; }
        virtual const Vector<SPIRVHandle>& GetSPIRVs() const override { return mShaderSPIRVs; }
        virtual const Path& GetPath() const override { return mPath; }
        virtual const HashMap<ShaderType, String>& GetSources() const override { return mShaderSources; }
        virtual const HashCode& GetHash(const ShaderType& type) const override { return mHashCodes.at(type); }
        virtual const HashMap<ShaderType, HashCode>& GetHashCodes() const override { return mHashCodes; }
        virtual const ShaderType& GetTypes() const override { return mTypesBit; };

        // Vulkan Specific (Used by different Pipelines)
        const HashMap<ShaderType, VkShaderModule>& GetVulkanShaderModules() const { return mVkShaderModules; }
        const std::map<Uint, VkDescriptorSetLayout>& GetDescriptorSetLayouts() const { return mDescriptorSetLayouts; }
        const HashMap<String, VkPushConstantRange>& GetPushConstantRanges() const { return mPushConstants; }

        void ParseShader();
        void Compile(const HashMap<ShaderType, bool>& compileStages);
        void Clear();
        void CreateVulkanDescriptorSetLayouts();
        void CreateVulkanPushConstantRanges();

    private:
        Path mPath;
        bool mCreatedDescriptorSetLayouts; // We want to make sure that the DescriptorSetLayouts doesn't get recreated when the shader is reloaded
        HashMap<ShaderType, HashCode> mHashCodes;
        HashMap<ShaderType, String> mShaderSources;
        Vector<SPIRVHandle> mShaderSPIRVs {};

        HashMap<ShaderType, VkShaderModule> mVkShaderModules;
        std::map<Uint, VkDescriptorSetLayout> mDescriptorSetLayouts;

        HashMap<String, VkPushConstantRange> mPushConstants;
        ShaderType mTypesBit;

        ShaderReflectionData mReflectionData;
        HashMap<UUID, std::function<void()>> mCallbacks;
    };
} // namespace Surge