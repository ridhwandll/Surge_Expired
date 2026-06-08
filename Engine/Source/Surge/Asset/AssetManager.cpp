// Copyright (c) - SurgeTechnologies - All rights reserved
#include "AssetManager.hpp"
#include "Surge/ECS/Scene.hpp"

#include "Serializer/Texture2DSerializer.hpp"
#include "Serializer/MeshSerializer.hpp"
#include "Serializer/SceneSerializer.hpp"
#include "Serializer/MaterialSerializer.hpp"

#include <fstream>
#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace Surge
{
    AssetManager::AssetManager()
    {
        mSerializers[AssetType::SCENE] = CreateScope<SceneSerializer>();
        mSerializers[AssetType::TEXTURE2D] = CreateScope<Texture2DSerializer>();
        mSerializers[AssetType::MESH] = CreateScope<MeshSerializer>();
        mSerializers[AssetType::MATERIAL] = CreateScope<MaterialSerializer>();

        for(auto& [type, serializer] : mSerializers)
            serializer->Initialize();
    }

    AssetManager::~AssetManager()
    {
        for(auto& [type, serializer] : mSerializers)
            serializer->Shutdown();
    }

    void AssetManager::Initialize(const Path& assetDirectory)
    {
        sAssetsDirectory = assetDirectory.generic_string();
        DeserializeRegistry();
        Log<Severity::Info>("[AssetManager] Initialized. Directory: '{}' // {} asset(s) in registry", sAssetsDirectory, mAssetRegistry.size());
        mInitialized = true;
    }

    void AssetManager::Shutdown()
    {
        if (!mInitialized)
            return;

        SerializeRegistry();
        mLoadedAssets.clear();
        mAssetRegistry.clear();
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
            if(!std::ifstream(absPath).good())
            {
                Log<Severity::Warn>("[AssetManager] Import failed: file not found: '{}'!", absPath.c_str());
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

        Log<Severity::Info>("[AssetManager] Imported '{}' | ID: {} | Type: {}", relativePath.c_str(), id.Get(), SurgeReflect::EnumToString(type).data());
        return id;
    }

    // Load (template in header points here)
    Ref<Asset> AssetManager::LoadAsset(AssetID id)
    {
        // Already live in the cache
        {
            auto cacheIt = mLoadedAssets.find(id);
            if(cacheIt != mLoadedAssets.end())
                return cacheIt->second;
        }

        // Validate registry
        auto metaIt = mAssetRegistry.find(id);
        if(metaIt == mAssetRegistry.end())
        {
            Log<Severity::Error>("[AssetManager] Load: AssetID {} is not registered!", id.Get());
            return nullptr;
        }

        AssetMetadata& meta = metaIt->second;
        if(meta.IsMissing())
        {
            Log<Severity::Error>("[AssetManager] Load: Source file missing for '{}'!", meta.RelativePath);
            return nullptr;
        }

        auto serIt = mSerializers.find(meta.Type);
        SG_ASSERT(serIt != mSerializers.end() && serIt->second, "[AssetManager] No serializer for type '{}'!", SurgeReflect::EnumToString(meta.Type).data());
        Ref<Asset> asset = serIt->second->Deserialize(meta);
        if(!asset)
        {
            Log<Severity::Error>("[AssetManager] Load: Loader returned null for '{}'!", meta.RelativePath);
            meta.Flags |= AssetFlags::MISSING;
            return nullptr;
        }

        asset->mID = id; // Stamp the ID
        meta.Flags |= AssetFlags::LOADED;

        mLoadedAssets[id] = asset;
        return asset;
    }

    // Unload
    bool AssetManager::Unload(AssetID id)
    {
        auto cacheIt = mLoadedAssets.find(id);
        if(cacheIt == mLoadedAssets.end())
            return false;

        // Cannot unload if there are external Refs still alive
        if(cacheIt->second->GetRefCount() > 1)
        {
            Log<Severity::Warn>("[AssetManager] Unload: AssetID {} has external references, cannot unload!", id.Get());
            return false;
        }

        mLoadedAssets.erase(cacheIt);

        // Just clear the Loaded flag
        auto metaIt = mAssetRegistry.find(id);
        if(metaIt != mAssetRegistry.end())
            metaIt->second.Flags &= ~AssetFlags::LOADED;

        return true;
    }

    bool AssetManager::IsLoaded(AssetID id)
    {
        return mLoadedAssets.find(id) != mLoadedAssets.end();
    }

    void AssetManager::Save(AssetID id)
    {
        auto cacheIt = mLoadedAssets.find(id);
        SG_ASSERT(cacheIt != mLoadedAssets.end(), "[AssetManager] Save: AssetID {} is not loaded!", id.Get());
        SG_ASSERT(mAssetRegistry.find(id) != mAssetRegistry.end(), "[AssetManager] Save: AssetID {} is not registered!", id.Get());

        mSerializers[mAssetRegistry.at(id).Type]->Serialize(cacheIt->second);
    }

    bool AssetManager::IsRegistered(AssetID id)
    {
        return mAssetRegistry.find(id) != mAssetRegistry.end();
    }

    const AssetMetadata& AssetManager::GetMetadata(AssetID id)
    {
        static const AssetMetadata kNull {};

        auto it = mAssetRegistry.find(id);
        return it != mAssetRegistry.end() ? it->second : kNull;
    }

    AssetID AssetManager::GetIDFromPath(const String& relativePath)
    {
        for(const auto& [id, meta] : mAssetRegistry)
        {
            if(meta.RelativePath == relativePath)
                return id;
        }
        return UUID::INVALID;
    }

    void AssetManager::UpdateAssetPath(AssetID id, const String& newRelativePath)
    {
        auto it = mAssetRegistry.find(id);
        if(it != mAssetRegistry.end())
        {
            it->second.RelativePath = newRelativePath;
            SerializeRegistry();
        }
    }

    // Registry
    // Format (AssetRegistry.surge):
    //    comment lines start with //
    //    <UUID>|<TypeString>|<Relative/path/to/asset.ext OR MemoryString>
    void AssetManager::SerializeRegistry()
    {
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
    }

    bool AssetManager::DeserializeRegistry()
    {
        const String registryPath = sAssetsDirectory + '/' + kRegistryFilename;

        std::ifstream file(registryPath);
        if(!file.is_open())
            return false;

        mAssetRegistry.clear();

        String line;
        Uint count = 0;

        while(std::getline(file, line))
        {
            // Ignore the lines starting with "//"
            if(line.empty() || (line.size() >= 2 && line[0] == '/' && line[1] == '/'))
                continue;

            // Parse: UUID|Type|RelativePath
            const size_t d1 = line.find(kRegistryDelimiter);
            const size_t d2 = line.find(kRegistryDelimiter, d1 + 1);

            if(d1 == String::npos || d2 == String::npos)
            {
                Log<Severity::Warn>("[AssetManager] DeserializeRegistry: Malformed line skipped.");
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
                meta.Flags = AssetFlags::VALID | AssetFlags::MEMORY;
            else
            {
                const String absPath = GetAbsolutePath(relPath);
                const bool exists = std::ifstream(absPath).good();
                meta.Flags = exists ? AssetFlags::VALID : AssetFlags::MISSING;

                if(!exists)
                    Log<Severity::Warn>("[AssetManager] DeserializeRegistry: Source file missing for '{}'.", relPath.c_str());
            }

            mAssetRegistry[id] = std::move(meta);
            count++;
        }

        Log<Severity::Info>("[AssetManager] Registry deserialized ({} entries).", count);
        return count > 0;
    }

} // namespace Surge