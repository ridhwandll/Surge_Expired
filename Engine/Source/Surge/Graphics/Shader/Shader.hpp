// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "ShaderReflector.hpp"
#include <functional>

namespace Surge
{
    class ShaderReflectionData;
    class Shader
    {
    public:
        Shader() = default;
        ~Shader() = default;

        void Load(const String& glslPath, ShaderType type);

        const ShaderReflectionData& GetReflectionData() const { return mReflectionData; }
        const Vector<SPIRVHandle>& GetSPIRVs() const { return mShaderSPIRVs; }
        const String& GetPath() const { return mPath; }
        const String& GetName() const { return mReflectionData.GetShaderName(); }
    private:
        void ParseShader();
        void Compile();
        String GetShaderCachePath(ShaderType type);
    private:
        String mPath;
        HashMap<ShaderType, String> mShaderSources;
        Vector<SPIRVHandle> mShaderSPIRVs;
        ShaderType mTypesBit;
        ShaderReflectionData mReflectionData;
    };

} // namespace Surge