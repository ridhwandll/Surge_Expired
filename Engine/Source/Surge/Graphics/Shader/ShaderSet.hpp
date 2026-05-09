#if 0
// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/Shader/Shader.hpp"

// TODO: Temporary, we don't have an asset manager yet
#define TEMP_ASSET_PATH "Engine/Assets/Temp"
#define SHADER_CACHE_PATH "Engine/Assets/Temp/ShaderCache"
#define SHADER_HASH_CACHE_PATH "Engine/Assets/Temp/ShaderCache/ShaderHash.txt"

namespace Surge
{
    class SURGE_API ShaderSet
    {
    public:
        ShaderSet() = default;
        ~ShaderSet() = default;

        void Initialize(const String& baseShaderPath);
        void AddShader(const String& shaderName);
        void LoadAll();
        void Shutdown();

        Ref<Shader>& GetShader(const String& shaderName); // Name without extension
        Vector<Ref<Shader>>& GetAllShaders() { return mShaders; }

    private:
        HashCode GetHashCodeFromCache(const Ref<Shader>& shader, ShaderType type);
        void CacheRequiredSPIRVs(const Ref<Shader>& shader, const HashMap<ShaderType, bool>& stagesToCache);
        void WriteHashToFile(const Ref<Shader>& shader);
        String GetCachePath(const Path& shaderPath, const ShaderType& type) const;
        String GetCacheName(const Path& shaderPath, const ShaderType& type) const;

    private:
        String mBaseShaderPath;
        Vector<Ref<Shader>> mShaders;
        Ref<Shader> mDummyShader = nullptr;
    };

} // namespace Surge
#endif // 0
