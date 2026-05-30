// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Shader/ShaderManager.hpp"

namespace Surge
{
    void ShaderManager::Initialize(const String& baseShaderPath)
    {
        mBaseShaderPath = baseShaderPath;
        // TODO: Cache shader
    }

    void ShaderManager::Load(const String& shaderName)
    {
        mShaders.emplace_back(Shader());
        mShaders.back().Load(std::format("{0}/{1}", mBaseShaderPath, shaderName), ShaderType::VERTEX | ShaderType::FRAGMENT);
    }

    void ShaderManager::Shutdown()
    {
    }

    const Shader& ShaderManager::Get(const String& shaderName) const
    {
        for(const Shader& shader : mShaders)
        {
            if(shader.GetName() == shaderName)
                return shader;
        }
        SG_ASSERT_INTERNAL("Shader not found!");
        return mDummyShader;
    }

} // namespace Surge
