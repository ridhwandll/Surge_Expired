// Copyright (c) - SurgeTechnologies - All rights reserved
#include "AssetManager.hpp"
#include "Texture2D.hpp"
#include "Mesh.hpp"
#include "SurgeReflect/Enum.hpp"

#include <fstream>
#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace Surge
{
    String                                       AssetManager::sAssetsDirectory;
    std::unordered_map<AssetID, AssetMetadata>   AssetManager::sAssetRegistry;
    std::unordered_map<AssetID, Ref<Asset>>      AssetManager::sLoadedAssets;

    void AssetManager::Initialize(const String& assetDirectory)
    {
        sAssetsDirectory = assetDirectory;
        DeserializeRegistry();
        Log<Severity::Info>("[AssetManager] Initialized. Directory: '{}' // {} asset(s) in registry", sAssetsDirectory, sAssetRegistry.size());
    }

    void AssetManager::Shutdown()
    {
        SerializeRegistry();

        sLoadedAssets.clear();
        sAssetRegistry.clear();
        Log<Severity::Info>("[AssetManager] Shutdown");
    }

    // Import
    AssetID AssetManager::Import(const String& relativePath, AssetType type)
    {
        SG_ASSERT(type != AssetType::NONE, "[AssetManager] Cannot import with AssetType::NONE");
        SG_ASSERT(!relativePath.empty(), "[AssetManager] Cannot import with an empty path!");

        if(relativePath.starts_with(SURGE_MEMORY_ASSET_PREFIX))
            return ImportFromMemory(relativePath, type);

        {
            const AssetID existing = GetIDFromPath(relativePath);
            if(existing.IsValid())
            {
                Log<Severity::Trace>("[AssetManager] Import: '{}' is already registered (ID: {})!", relativePath.c_str(), existing.Get());
                return existing;
            }
        }

        const String absPath = GetAbsolutePath(relativePath);

        // Validate the source file exists on disk
        if(!std::ifstream(absPath).good())
        {
            Log<Severity::Warn>("[AssetManager] Import failed: file not found: '{}'!", absPath.c_str());
            return UUID::INVALID;
        }

        const AssetID id = AssetID();
        if(!id.IsValid())
        {
            Log<Severity::Error>("[AssetManager] Import failed: could not read/create sidecar for: '{}'.", absPath.c_str());
            return UUID::INVALID;
        }

        AssetMetadata meta;
        meta.ID = id;
        meta.Type = type;
        meta.Flags = AssetFlags::VALID;
        meta.RelativePath = relativePath;
        sAssetRegistry[id] = std::move(meta);

        Log<Severity::Info>("[AssetManager] Imported '{}' | ID: {} | Type: {}", relativePath.c_str(), id.Get(), SurgeReflect::EnumToString(type).data());

        return id;
    }

    // No Sidecars, no file system validations, memoryStr is directly passed to LoadInternal(no paths)
    AssetID AssetManager::ImportFromMemory(const String& memoryStr, AssetType type)
    {
        SG_ASSERT(type != AssetType::NONE, "[AssetManager] Cannot import with AssetType::NONE");
        SG_ASSERT(!memoryStr.empty(), "[AssetManager] Cannot import with an empty memoryStr!");

        {
            const AssetID existing = GetIDFromPath(memoryStr);
            if(existing.IsValid())
            {
                Log<Severity::Trace>("[AssetManager] Import: '{}' is already registered (ID: {})!", memoryStr.c_str(), existing.Get());
                return existing;
            }
        }

        AssetMetadata meta;
        meta.ID = AssetID();
        meta.Type = type;
        meta.Flags = AssetFlags::VALID | AssetFlags::MEMORY;
        meta.RelativePath = memoryStr;
        sAssetRegistry[meta.ID] = std::move(meta);

        Log<Severity::Info>("[AssetManager] Imported '{}' | ID: {} | Type: {}", memoryStr.c_str(), meta.ID.Get(), SurgeReflect::EnumToString(type).data());

        return meta.ID;
    }

    // Load (template in header points here)
    Ref<Asset> AssetManager::LoadAsset(AssetID id)
    {
        // Already live in the cache
        {
            auto cacheIt = sLoadedAssets.find(id);
            if(cacheIt != sLoadedAssets.end())
                return cacheIt->second;
        }

        // Validate registry
        auto metaIt = sAssetRegistry.find(id);
        if(metaIt == sAssetRegistry.end())
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

        Ref<Asset> asset = LoadInternal(meta);
        if(!asset)
        {
            Log<Severity::Error>("[AssetManager] Load: Loader returned null for '{}'!", meta.RelativePath);
            meta.Flags |= AssetFlags::MISSING;
            return nullptr;
        }

        asset->mID = id; // Stamp the ID
        meta.Flags |= AssetFlags::LOADED;

        sLoadedAssets[id] = asset;
        return asset;
    }

    // Unload
    bool AssetManager::Unload(AssetID id)
    {
        auto cacheIt = sLoadedAssets.find(id);
        if(cacheIt == sLoadedAssets.end())
            return false;

        sLoadedAssets.erase(cacheIt);

        // Just clear the Loaded flag
        auto metaIt = sAssetRegistry.find(id);
        if(metaIt != sAssetRegistry.end())
            metaIt->second.Flags &= ~AssetFlags::LOADED;

        return true;
    }

    bool AssetManager::IsLoaded(AssetID id)
    {
        return sLoadedAssets.find(id) != sLoadedAssets.end();
    }

    bool AssetManager::IsRegistered(AssetID id)
    {
        return sAssetRegistry.find(id) != sAssetRegistry.end();
    }

    const AssetMetadata& AssetManager::GetMetadata(AssetID id)
    {
        static const AssetMetadata kNull {};

        auto it = sAssetRegistry.find(id);
        return it != sAssetRegistry.end() ? it->second : kNull;
    }

    AssetID AssetManager::GetIDFromPath(const String& relativePath)
    {
        for(const auto& [id, meta] : sAssetRegistry)
        {
            if(meta.RelativePath == relativePath)
                return id;
        }
        return UUID::INVALID;
    }

    // Registry
    //
    // Format (AssetRegistry.surge):
    //    // comment lines start with //
    //    <uuid>|<TypeString>|<relative/path/to/asset.ext>
    void AssetManager::SerializeRegistry()
    {
        const String registryPath = sAssetsDirectory + '/' + kRegistryFilename;

        std::ofstream file(registryPath, std::ios::out | std::ios::trunc);
        if(!file.is_open())
        {
            Log<Severity::Error>("[AssetManager] SerializeRegistry: Failed to open '{}'.",
                registryPath.c_str());
            return;
        }

        file << "// Surge Asset Registry v1\n";
        file << "// Format: UUID|Type|RelativePath\n";

        for(const auto& [id, meta] : sAssetRegistry)
        {
            file << id.Get()
                << kRegistryDelimiter
                << SurgeReflect::EnumToString(meta.Type).data()
                << kRegistryDelimiter
                << meta.RelativePath
                << '\n';
        }

        Log<Severity::Info>("[AssetManager] Registry serialized ({} entries) -> '{}'.", sAssetRegistry.size(), registryPath.c_str());
    }

    bool AssetManager::DeserializeRegistry()
    {
        const String registryPath = sAssetsDirectory + '/' + kRegistryFilename;

        std::ifstream file(registryPath);
        if(!file.is_open())
            return false;

        sAssetRegistry.clear();

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

            sAssetRegistry[id] = std::move(meta);
            count++;
        }

        Log<Severity::Info>("[AssetManager] Registry deserialized ({} entries).", count);
        return count > 0;
    }

    Ref<Asset> AssetManager::LoadInternal(const AssetMetadata& metadata)
    {
        String fullPath;

        if(HasFlag(metadata.Flags, AssetFlags::MEMORY))
            fullPath = metadata.RelativePath; //metadata.RelativePath contains the memoryStr
        else
            fullPath = GetAbsolutePath(metadata.RelativePath);

        switch(metadata.Type)
        {
            case AssetType::MESH:      return Mesh::Create(fullPath);
            case AssetType::TEXTURE2D: return Texture2D::Create(fullPath);
            case AssetType::SCENE:
            {
                // TODO: return Serializer::Deserialize(fullPath);
                SG_ASSERT_INTERNAL("[AssetManager] Scene loader not yet connected!");
                return nullptr;
            }
            case AssetType::SPRITE:
            {
                SG_ASSERT_INTERNAL("[AssetManager] Sprite loader not yet connected!");
                return nullptr;
            }
            default:
            {
                SG_ASSERT_INTERNAL("[AssetManager] LoadInternal: Unknown AssetType!");
                return nullptr;
            }
        }
    }

} // namespace Surge