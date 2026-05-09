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
        static constexpr const char* ENGINE_SHADER_PATH = "Engine/Assets/Shaders";

    public:
        Shader() = default;
        ~Shader() = default;

		//Must be located at ENGINE_SHADER_PATH + "/" + name + ".shader"
        void Load(const String& name, ShaderType type);

		const ShaderReflectionData& GetReflectionData() const { return mReflectionData; }
		const Vector<SPIRVHandle>& GetSPIRVs() const { return mShaderSPIRVs; }
		const String& GetName() const { return mName; }
    private:
        void ParseShader();
		void Compile();
        String GetShaderCachePath(ShaderType type);
    private:
		String mName;
        HashMap<ShaderType, String> mShaderSources;
		Vector<SPIRVHandle> mShaderSPIRVs;
        ShaderType mTypesBit;
		ShaderReflectionData mReflectionData;
    };

} // namespace Surge