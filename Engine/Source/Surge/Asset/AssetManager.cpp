// Copyright (c) - SurgeTechnologies - All rights reserved
#include "AssetManager.hpp"

#include "Serializer/Script/ScriptSerializer.hpp"
#include "Serializer/Material/MaterialSerializer.hpp"
#include "Serializer/Mesh/MeshSerializer.hpp"
#include "Serializer/Scene/SceneSerializer.hpp"
#include "Serializer/Texture2DSerializer.hpp"
#include "Serializer/Font/FontSerializer.hpp"
#include "Serializer/Audio/AudioSerializer.hpp"
#include "Serializer/UILayout/UILayoutSerializer.hpp"
#include "Serializer/ParticleEffect/ParticleEffectSerializer.hpp"

#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Core/Profiler.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>

namespace Surge
{
    static const AssetMetadata sNullMetadata {};

    AssetManager::AssetManager()
    {
        mSerializers[AssetType::SCENE] = CreateScope<SceneSerializer>();
        mSerializers[AssetType::TEXTURE2D] = CreateScope<Texture2DSerializer>();
        mSerializers[AssetType::MESH] = CreateScope<MeshSerializer>();
        mSerializers[AssetType::MATERIAL] = CreateScope<MaterialSerializer>();
        mSerializers[AssetType::SCRIPT] = CreateScope<ScriptSerializer>();
        mSerializers[AssetType::FONT] = CreateScope<FontSerializer>();
        mSerializers[AssetType::AUDIO] = CreateScope<AudioSerializer>();
        mSerializers[AssetType::UI_LAYOUT] = CreateScope<UILayoutSerializer>();
        mSerializers[AssetType::PARTICLE_EFFECT] = CreateScope<ParticleEffectSerializer>();

        for(auto& [type, serializer] : mSerializers)
            serializer->Initialize();
    }

    AssetManager::~AssetManager()
    {
        for(auto& [type, serializer] : mSerializers)
            serializer->Shutdown();
    }

    void AssetManager::Initialize(const Path& assetDirectory, const AssetContext& context)
    {
        SG_ASSERT(context.AssetManager_ == this, "[AssetManager] The context must point back to this AssetManager!");
        mContext = context;
        for(auto& [type, serializer] : mSerializers)
            serializer->SetContext(&mContext);

        sAssetsDirectory = assetDirectory.generic_string();
        DeserializeRegistry();
        Log<Severity::Info>("[AssetManager] Initialized. Directory: '{}' // {} asset(s) in registry", sAssetsDirectory, mAssetRegistry.size());

        // Ensure at initialization that all sidecar directories exist
        for (const auto& [type, serializer] : mSerializers)
            Filesystem::CreateOrEnsureDirectories(GetSidecarDirectory(type));

        mInitialized = true;
    }

    void AssetManager::Shutdown()
    {
        if (!mInitialized)
            return;

        SerializeRegistry();
        mLoadedAssets.clear();
        mAssetRegistry.clear();
        mPathToID.clear();
        Log<Severity::Info>("[AssetManager] Shutdown");
        mInitialized = false;
    }

    // Import
    AssetID AssetManager::Import(const String& relativePath, AssetType type)
    {
        SG_ASSERT(type != AssetType::NONE, "[AssetManager] Cannot import with AssetType::NONE");
        SG_ASSERT(!relativePath.empty(), "[AssetManager] Cannot import with an empty path!");

        {
            const AssetID existing = GetIDFromPath(relativePath);
            if(existing.IsValid())
            {
                //Log<Severity::Trace>("[AssetManager] Import: '{}' is already registered (ID: {})!", relativePath.c_str(), existing.Get());
                return existing;
            }
        }

        // If the asset is not created from memory, validate the source file exists on disk
        bool fromMemory = true;
        if(!relativePath.starts_with(SURGE_MEMORY_ASSET_PREFIX))
        {
            const String absPath = GetAbsolutePath(relativePath);
            if(!Filesystem::Exists(absPath))
            {
                Log<Severity::Warn>("[AssetManager] Import failed: file not found: {}!", absPath);
                return UUID::INVALID;
            }
            fromMemory = false;
        }

        AssetID id = AssetID();

        AssetMetadata meta;
        meta.ID = id;
        meta.Type = type;

        fromMemory ? meta.Flags = AssetFlags::VALID | AssetFlags::MEMORY : meta.Flags = AssetFlags::VALID;

        meta.RelativePath = relativePath;
        mAssetRegistry[id] = std::move(meta);
        mPathToID[relativePath] = id;

        Log<Severity::Info>("[AssetManager] Imported '{}' | ID: {} | Type: {}", relativePath, id.Get(), SurgeReflect::EnumToString(type).data());
        return id;
    }

    // Load (template in header points here, do not call directly)
    Ref<Asset> AssetManager::LoadAssetInternal(AssetID id, AssetMetadata* meta)
    {
        auto serIt = mSerializers.find(meta->Type);
        SG_ASSERT(serIt != mSerializers.end() && serIt->second, "[AssetManager] No serializer for type {}!", SurgeReflect::EnumToString(meta->Type).data());

        Ref<Asset> asset = serIt->second->Deserialize(*meta);
        if(!asset)
        {
            Log<Severity::Error>("[AssetManager] Load: Loader returned null for {}! Is the asset Missing?", meta->RelativePath);
            meta->Flags |= AssetFlags::MISSING;
            return nullptr;
        }

        asset->mID = id;
        meta->Flags |= AssetFlags::LOADED;
        meta->Flags &= ~AssetFlags::MISSING;

        mLoadedAssets[id] = asset;
        return asset;
    }

    AssetMetadata* AssetManager::GetMetadataForEdit(AssetID id)
    {
        auto it = mAssetRegistry.find(id);
        if(it != mAssetRegistry.end())
            return &it->second;

        return nullptr;
    }

        // Unload
    bool AssetManager::Unload(AssetID id)
    {
        auto cacheIt = mLoadedAssets.find(id);
        if(cacheIt == mLoadedAssets.end())
            return false;

        // Cannot unload if there are external Refs still alive
        if(cacheIt->second->GetRefCount() > 1)
            return false;

        mLoadedAssets.erase(cacheIt);

        // Just clear the Loaded flag
        auto metaIt = mAssetRegistry.find(id);
        if(metaIt != mAssetRegistry.end())
            metaIt->second.Flags &= ~AssetFlags::LOADED;

        return true;
    }

    Uint AssetManager::UnloadUnreferenced()
    {
        SURGE_PROFILE_FUNC("AssetManager::UnloadUnreferenced");
        Uint total = 0;
        Uint unloaded = 0;
        do
        {
            // Erasing destroys the asset, which only lowers the Ref count of the assets it held: they are picked up by the next pass
            unloaded = 0;
            for(auto it = mLoadedAssets.begin(); it != mLoadedAssets.end();)
            {
                if(it->second->GetRefCount() > 1)
                {
                    ++it;
                    continue;
                }

                if(AssetMetadata* meta = GetMetadataForEdit(it->first))
                    meta->Flags &= ~AssetFlags::LOADED;

                it = mLoadedAssets.erase(it);
                unloaded++;
            }
            total += unloaded;

        } while(unloaded > 0);

        if(total > 0)
            Log<Severity::Info>("[AssetManager] Unloaded {} unreferenced asset(s), {} still loaded", total, mLoadedAssets.size());
        return total;
    }

    bool AssetManager::IsLoaded(AssetID id)
    {
        return mLoadedAssets.find(id) != mLoadedAssets.end();
    }

    bool AssetManager::Save(AssetID id)
    {
        auto cacheIt = mLoadedAssets.find(id);
        if(cacheIt == mLoadedAssets.end())
        {
            Log<Severity::Error>("[AssetManager] Save: AssetID {} is not loaded!", id.Get());
            return false;
        }

        auto metaIt = mAssetRegistry.find(id);
        if(metaIt == mAssetRegistry.end())
        {
            Log<Severity::Error>("[AssetManager] Save: AssetID {} is not registered!", id.Get());
            return false;
        }

        auto serIt = mSerializers.find(metaIt->second.Type);
        if(serIt == mSerializers.end() || !serIt->second)
        {
            Log<Severity::Error>("[AssetManager] Save: No serializer for type {}!", SurgeReflect::EnumToString(metaIt->second.Type).data());
            return false;
        }
        return serIt->second->Serialize(cacheIt->second);
    }

    void AssetManager::UnregisterAsset(AssetID id)
    {
        auto it = mAssetRegistry.find(id);
        if(it == mAssetRegistry.end())
            return;

        auto pathIt = mPathToID.find(it->second.RelativePath);
        if(pathIt != mPathToID.end() && pathIt->second == id)
            mPathToID.erase(pathIt);

        mAssetRegistry.erase(it);
        SerializeRegistry();
    }

    bool AssetManager::IsRegistered(AssetID id)
    {
        return mAssetRegistry.find(id) != mAssetRegistry.end();
    }

    const AssetMetadata& AssetManager::GetMetadata(AssetID id)
    {
        auto it = mAssetRegistry.find(id);
        return it != mAssetRegistry.end() ? it->second : sNullMetadata;
    }

    String AssetManager::GetSidecarPath(AssetID id, AssetType type)
    {
        const AssetMetadata& meta = GetMetadata(id);

        AssetType t;
        type != AssetType::NONE ? t = type : t = meta.Type;

        String filename = std::format("{0}.r{1}", id.Get(), SurgeReflect::EnumToString(t).data());
        return GetSidecarDirectory(t) + "/" + filename;
    }

    String AssetManager::GetSidecarDirectory(AssetType type)
    {
        return sAssetsDirectory + "/Internal/" + SurgeReflect::EnumToString(type).data();
    }

    AssetID AssetManager::GetIDFromPath(const String& relativePath)
    {
        auto it = mPathToID.find(relativePath);
        if(it != mPathToID.end())
            return it->second;

#ifdef SURGE_PLATFORM_WINDOWS
        // Windows paths ignore case, the APK does not: a path that only matches with another case works on desktop but not on device.
        // Only misses pay for this scan (the same cost the lookup always had before the hash map)
        for(const auto& [path, id] : mPathToID)
        {
            if(path.size() == relativePath.size() && std::equal(path.begin(), path.end(), relativePath.begin(), [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); }))
            {
                Log<Severity::Warn>("[AssetManager] '{}' is not registered, but '{}' is: paths are case sensitive on Android", relativePath, path);
                break;
            }
        }
#endif
        return UUID::INVALID;
    }

    void AssetManager::UpdateAssetPath([[maybe_unused]] AssetID id, [[maybe_unused]] const String& newRelativePath, [[maybe_unused]] bool serialize)
    {
#ifdef SURGE_PLATFORM_ANDROID
        Log<Severity::Error>("[AssetManager] UpdateAssetPath is an Editor function. APK assets are readonly!");
#else
        auto it = mAssetRegistry.find(id);
        if(it == mAssetRegistry.end() || it->second.RelativePath == newRelativePath)
            return;

        auto takenIt = mPathToID.find(newRelativePath);
        if(takenIt != mPathToID.end() && takenIt->second != id)
        {
            Log<Severity::Error>("[AssetManager] UpdateAssetPath: '{}' already belongs to asset {}, {} keeps '{}'", newRelativePath, takenIt->second.Get(), id.Get(), it->second.RelativePath);
            return;
        }

        auto oldIt = mPathToID.find(it->second.RelativePath);
        if(oldIt != mPathToID.end() && oldIt->second == id)
            mPathToID.erase(oldIt);

        it->second.RelativePath = newRelativePath;
        mPathToID[newRelativePath] = id;
        if(serialize)
            SerializeRegistry();
#endif
    }

    // Registry
    // Format (AssetRegistry.surge):
    //    comment lines start with //
    //    <UUID>|<TypeString>|<Relative/path/to/asset.ext OR MemoryString>
    void AssetManager::SerializeRegistry()
    {
#ifdef SURGE_PLATFORM_ANDROID
        Log<Severity::Warn>("[AssetManager] SerializeRegistry skipped. APK assets are readonly!");
        return;
#else
        const String registryPath = sAssetsDirectory + '/' + kRegistryFilename;

        std::ofstream file(registryPath, std::ios::out | std::ios::trunc);
        if(!file.is_open())
        {
            Log<Severity::Error>("[AssetManager] SerializeRegistry: Failed to open '{}'.", registryPath);
            return;
        }

        file << "// Surge Asset Registry v1\n";
        file << "// Format: UUID|Type|RelativePath\n";

        for(const auto& [id, meta] : mAssetRegistry)
        {
            file << id.Get()
                << kRegistryDelimiter
                << SurgeReflect::EnumToString(meta.Type).data()
                << kRegistryDelimiter
                << meta.RelativePath
                << '\n';
        }

        Log<Severity::Info>("[AssetManager] Registry serialized ({} entries) -> '{}'.", mAssetRegistry.size(), registryPath.c_str());
#endif
    }

    bool AssetManager::DeserializeRegistry()
    {
        const String registryPath = sAssetsDirectory + '/' + kRegistryFilename;
        String fileContents;

        if(!Filesystem::ReadTextFile(registryPath, fileContents))
        {
            Log<Severity::Warn>("[AssetManager] Failed to open {}", registryPath);
            return false;
        }

        if (fileContents.empty())
        {
            Log<Severity::Warn>("[AssetManager] Registry file is empty: {}\nIs this intentional?", registryPath);
            return false;
        }

        mAssetRegistry.clear();
        mPathToID.clear();

        std::istringstream stream(fileContents);
        String line;
        Uint count = 0;

        while(std::getline(stream, line))
        {
            if(!line.empty() && line.back() == '\r')
                line.pop_back();

            // Ignore the lines starting with "//"
            if(line.empty() || (line.size() >= 2 && line[0] == '/' && line[1] == '/'))
                continue;

            // Parse: UUID|Type|RelativePath
            const size_t d1 = line.find(kRegistryDelimiter);
            const size_t d2 = line.find(kRegistryDelimiter, d1 + 1);

            if(d1 == String::npos || d2 == String::npos)
            {
                Log<Severity::Warn>("[AssetManager] DeserializeRegistry: Malformed line skipped!");
                continue;
            }

            const String uuidStr = line.substr(0, d1);
            char* endPtr = nullptr;
            errno = 0;
            const uint64_t rawID = strtoull(uuidStr.c_str(), &endPtr, 10);

            if(endPtr == uuidStr.c_str() || errno == ERANGE || rawID == 0)
                continue;

            const String typeStr = line.substr(d1 + 1, d2 - d1 - 1);
            const String relPath = line.substr(d2 + 1);
            const AssetID id(rawID);
            const AssetType type = AssetTypeFromString(typeStr.c_str());

            if(type == AssetType::NONE || relPath.empty())
                continue;

            AssetMetadata meta;
            meta.ID = id;
            meta.Type = type;
            meta.RelativePath = relPath;

            if(relPath.starts_with(SURGE_MEMORY_ASSET_PREFIX))
            {
                meta.Flags = AssetFlags::VALID | AssetFlags::MEMORY;
            }
            else
            {
                String absPath = GetAbsolutePath(relPath);
                String sidecarPath = GetSidecarPath(meta.ID, type);

                bool sourceExists = Filesystem::Exists(absPath);
                bool sidecarExists = Filesystem::Exists(sidecarPath);

                bool exists = sourceExists || sidecarExists;
                meta.Flags = exists ? AssetFlags::VALID : AssetFlags::MISSING;

                // Exported builds only ship the cooked files, a missing source alone is normal there
                if (!exists)
                    Log<Severity::Warn>("[AssetManager] DeserializeRegistry: Asset Source & Sidecar is missing for {}", relPath);
            }

            if(!mPathToID.try_emplace(relPath, id).second)
                Log<Severity::Warn>("[AssetManager] DeserializeRegistry: '{}' is registered twice, path lookups resolve to {}", relPath, mPathToID[relPath].Get());

            mAssetRegistry[id] = std::move(meta);
            count++;
        }

        Log<Severity::Info>("[AssetManager] Registry deserialized ({} entries).", count);
        return count > 0;
    }

} // namespace Surge