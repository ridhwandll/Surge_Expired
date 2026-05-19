// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Graphics/Shader/Shader.hpp"

namespace Surge
{
    class ShaderManager
    {
    public:
        ShaderManager() = default;
        ~ShaderManager() = default;
        SURGE_DISABLE_COPY(ShaderManager);

        void Initialize(const String& baseShaderPath);

        // Shader name in baseShaderPath directory, with extension, e.g. "Renderer3D.glsl"
        void Load(const String& shaderName);
        void Shutdown();

        const Shader& Get(const String& shaderName) const; //Name with extension
        const Vector<Shader>& GetAllShaders() const { return mShaders; }

    private:
        String mBaseShaderPath;
        Vector<Shader> mShaders;
        Shader mDummyShader {};
    };

} // namespace Surge
