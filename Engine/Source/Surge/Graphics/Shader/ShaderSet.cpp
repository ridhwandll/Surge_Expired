#if 0
// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Shader/ShaderSet.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include <filesystem>
#include <json/json.hpp>

namespace Surge
{
    void ShaderSet::Initialize(const String& baseShaderPath)
    {
        mBaseShaderPath = baseShaderPath;
#ifdef SURGE_PLATFORM_WINDOWS
        std::filesystem::create_directory(TEMP_ASSET_PATH);
        std::filesystem::create_directory(SHADER_CACHE_PATH);
#endif
    }

    void ShaderSet::AddShader(const String& shaderName)
    {
        mShaders.push_back(Shader::Create(fmt::format("{0}/{1}", mBaseShaderPath, shaderName)));
    }

    Ref<Shader>& ShaderSet::GetShader(const String& shaderName)
    {
        for (Ref<Shader>& shader : mShaders)
        {
            String nameWithoutExtension = Filesystem::RemoveExtension(shader->GetPath());
            if (nameWithoutExtension == fmt::format("{0}/{1}", mBaseShaderPath, shaderName))
                return shader;
        }

        SG_ASSERT_INTERNAL("No shaders found with name: {0}", shaderName);
        return mDummyShader;
    }

    void ShaderSet::LoadAll()
    {
        SCOPED_TIMER("ShaderSet::LoadAll");
        for (Ref<Shader>& shader : mShaders)
        {
#ifdef SURGE_PLATFORM_WINDOWS
            bool saveHash = false;
            const HashMap<ShaderType, String>& sources = shader->GetSources();
            HashMap<ShaderType, bool> compileStages;

            // Find which shader source needs to be reloaded
            for (auto& source : sources)
            {
                const HashCode& currentHashCode = shader->GetHash(source.first);
                const HashCode cacheHashCode = GetHashCodeFromCache(shader, source.first);

                const String cachePath = GetCachePath(shader->GetPath(), source.first);
                const bool existsInCache = Filesystem::Exists(cachePath);

                // Recompile if:
                //-> Doesn't exist in cache |or| the hash codes are different
                if (!existsInCache || (existsInCache && cacheHashCode != currentHashCode))
                {
                    compileStages[source.first] = true;
                    saveHash = true;
                }
                else
                    compileStages[source.first] = false;
            }

            // Load only required shader stages
            shader->Load(compileStages);

            CacheRequiredSPIRVs(shader, compileStages);

            if (saveHash)
                WriteHashToFile(shader);
#elif defined(SURGE_PLATFORM_ANDROID)
            const HashMap<ShaderType, String>& sources = shader->GetSources();
            HashMap<ShaderType, bool> compileStages;

            // Load all & do not hash
            for (auto& source : sources)
            {
                compileStages[source.first] = true;
            }

            // Load only required shader stages
            shader->Load(compileStages);
#endif
        }
    }

    void ShaderSet::Shutdown()
    {
        mShaders.clear();
    }

    HashCode ShaderSet::GetHashCodeFromCache(const Ref<Shader>& shader, ShaderType type)
    {
        HashCode result = 0;
#ifdef SURGE_PLATFORM_WINDOWS
        String previousContents = Filesystem::ReadFile<String>(SHADER_HASH_CACHE_PATH);
        String name = GetCacheName(shader->GetPath(), type);

        nlohmann::json j = previousContents.empty() ? nlohmann::json() : nlohmann::json::parse(previousContents);

        if (j.contains(name))
            result = j[name];
#endif
        return result;
    }

    void ShaderSet::CacheRequiredSPIRVs(const Ref<Shader>& shader, const HashMap<ShaderType, bool>& stagesToCache)
    {
#ifdef SURGE_PLATFORM_WINDOWS
        for (auto& stage : stagesToCache)
        {
            if (!stage.second)
                continue;

            String path = GetCachePath(shader->GetPath(), stage.first);
            FILE* f = nullptr;
            f = fopen(path.c_str(), "wb");
            if (f)
            {
                SPIRVHandle spirvToCache;
                Vector<SPIRVHandle> spirvs = shader->GetSPIRVs();
                for (SPIRVHandle& spirvHandle : spirvs)
                {
                    if (spirvHandle.Type == stage.first)
                    {
                        spirvToCache = spirvHandle;
                        break;
                    }
                }

                fwrite(spirvToCache.SPIRV.data(), sizeof(Uint), spirvToCache.SPIRV.size(), f);
                fclose(f);
                Log<Severity::Info>("Cached Shader at: {0}", path);
            }
        }
#endif
    }

    void ShaderSet::WriteHashToFile(const Ref<Shader>& shader)
    {
#ifdef SURGE_PLATFORM_WINDOWS
        FILE* f = nullptr;

        // Create the file if it doesn't exist
        if (!Filesystem::Exists(SHADER_HASH_CACHE_PATH))
            Filesystem::CreateOrEnsureFile(SHADER_HASH_CACHE_PATH);

        // Load in the contents of the file, because we append to it later
        String previousContents = Filesystem::ReadFile<String>(SHADER_HASH_CACHE_PATH);

        // Create A JSON, from the previous file for writing the new contents
        nlohmann::json j = previousContents.empty() ? nlohmann::json() : nlohmann::json::parse(previousContents);

        // For each HashCodes, update it
        for (auto& element : shader->GetHashCodes())
        {
            String name = GetCacheName(shader->GetPath(), element.first);
            j[name] = element.second;
        }

        String result = j.dump(4);
        f = fopen(SHADER_HASH_CACHE_PATH, "w");
        if (f)
        {
            fwrite(result.c_str(), sizeof(char), result.size(), f);
            fclose(f);
        }
#endif
    }

    String ShaderSet::GetCachePath(const Path& shaderPath, const ShaderType& type) const
    {
        String path = fmt::format("{0}/{1}", SHADER_CACHE_PATH, GetCacheName(shaderPath, type));
        return path;
    }

    String ShaderSet::GetCacheName(const Path& shaderPath, const ShaderType& type) const
    {
        String name = fmt::format("{0}.{1}.spv", Filesystem::GetNameWithExtension(shaderPath), ShaderTypeToString(type));
        return name;
    }

} // namespace Surge
#endif