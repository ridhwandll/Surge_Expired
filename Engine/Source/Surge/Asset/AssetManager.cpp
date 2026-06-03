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
    std::unordered_map<uint64_t, AssetID>        AssetManager::sPathIndex;

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
        sPathIndex.clear();
        Log<Severity::Info>("[AssetManager] Shutdown");
    }

    // Import
    AssetID AssetManager::Import(const String& relativePath, AssetType type)
    {
        SG_ASSERT(type != AssetType::NONE, "[AssetManager] Cannot import with AssetType::NONE");
        SG_ASSERT(!relativePath.empty(), "[AssetManager] Cannot import with an empty path!");

        if(relativePath.starts_with(SURGE_MEMORY_ASSET_PREFIX))
            return ImportFromMemory(relativePath, type);

        // Return the existing ID if already registered
        const uint64_t pathHash = HashPath(relativePath);
        auto indexIt = sPathIndex.find(pathHash);
        if(indexIt != sPathIndex.end())
        {
            Log<Severity::Trace>("[AssetManager] Import: '{}' is already registered (ID: {})!", relativePath.c_str(), indexIt->second.Get());
            return indexIt->second;
        }

        const String absPath = GetAbsolutePath(relativePath);

        // Validate the source file exists on disk
        if(!std::ifstream(absPath).good())
        {
            Log<Severity::Warn>("[AssetManager] Import failed: file not found: '{}'!", absPath.c_str());
            return UUID::INVALID;
        }

        // Retrieve or generate a stable UUID via the .surgeasset sidecar
        // The sidecar guarantees the same ID survives renames/re-imports
        const AssetID id = ReadOrCreateSidecar(absPath, type);
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
        sPathIndex[pathHash] = id;

        Log<Severity::Info>("[AssetManager] Imported '{}' | ID: {} | Type: {}", relativePath.c_str(), id.Get(), SurgeReflect::EnumToString(type).data());

        return id;
    }

    // No Sidecars, no file system validations, memoryStr is directly passed to LoadInternal(no paths)
    AssetID AssetManager::ImportFromMemory(const String& memoryStr, AssetType type)
    {
        SG_ASSERT(type != AssetType::NONE, "[AssetManager] Cannot import with AssetType::NONE");
        SG_ASSERT(!memoryStr.empty(), "[AssetManager] Cannot import with an empty memoryStr!");

        // Idempotent: return the existing ID if already registered
        const uint64_t pathHash = HashPath(memoryStr);
        auto indexIt = sPathIndex.find(pathHash);
        if(indexIt != sPathIndex.end())
        {
            Log<Severity::Warn>("[AssetManager] Import: '{}' is already registered (ID: {})!", memoryStr.c_str(), indexIt->second.Get());
            return indexIt->second;
        }

        AssetMetadata meta;
        meta.ID = AssetID();
        meta.Type = type;
        meta.Flags = AssetFlags::VALID | AssetFlags::MEMORY;
        meta.RelativePath = memoryStr;

        sPathIndex[pathHash] = meta.ID;
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
        auto it = sPathIndex.find(HashPath(relativePath));
        return it != sPathIndex.end() ? it->second : UUID(uint64_t(UUID::INVALID));
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
        sPathIndex.clear();

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
            sPathIndex[HashPath(relPath)] = id;
            ++count;
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

    // Sidecar helpers
    //
    // Format (Hero.png.surgeasset):
    //   UUID=<decimal_u64>
    //   Type=Texture2D
    AssetID AssetManager::ReadOrCreateSidecar(const String& absAssetPath, AssetType type)
    {
        const String sidecarPath = GetSidecarPath(absAssetPath);

        AssetID existingID = AssetID::INVALID;
        AssetType existingType = AssetType::NONE;

        if(ReadSidecar(sidecarPath, existingID, existingType))
        {
            if(existingType != type)
            {
                Log<Severity::Warn>("[AssetManager] Sidecar type mismatch for '{}': stored={} requested={}. " "Re-import with the correct type if this is wrong.",
                    absAssetPath.c_str(),
                    SurgeReflect::EnumToString(existingType).data(),
                    SurgeReflect::EnumToString(type).data());
            }
            return existingID; // Stable ID from disk wins
        }

        // No sidecar yet: generate a fresh UUID and persist it
        const AssetID freshID;
        if(!WriteSidecar(sidecarPath, freshID, type))
            return UUID::INVALID;

        return freshID;
    }

    bool AssetManager::WriteSidecar(const String& sidecarPath, const AssetID& id, AssetType type)
    {
        std::ofstream file(sidecarPath, std::ios::out | std::ios::trunc);
        if(!file.is_open())
        {
            Log<Severity::Error>("[AssetManager] WriteSidecar: Failed to open '{}'.", sidecarPath.c_str());
            return false;
        }

        file << "UUID=" << id.Get() << '\n';
        file << "Type=" << SurgeReflect::EnumToString(type).data() << '\n';
        return true;
    }

    bool AssetManager::ReadSidecar(const String& sidecarPath, AssetID& outID, AssetType& outType)
    {
        std::ifstream file(sidecarPath);
        if(!file.is_open())
            return false;

        uint64_t rawID = 0;
        AssetType type = AssetType::NONE;
        String line;

        while(std::getline(file, line))
        {
            // "UUID=<value>"
            if(line.compare(0, 5, "UUID=") == 0)
            {
                char* end = nullptr;
                errno = 0;
                rawID = strtoull(line.c_str() + 5, &end, 10);
                if(end == line.c_str() + 5 || errno == ERANGE) rawID = 0;
            }
            // "Type=<TypeString>"
            else if(line.compare(0, 5, "Type=") == 0)
            {
                type = AssetTypeFromString(line.c_str() + 5);
            }
        }

        if(rawID == 0 || type == AssetType::NONE)
            return false;

        outID = UUID(rawID);
        outType = type;
        return true;
    }

    uint64_t AssetManager::HashPath(const String& relativePath)
    {
        // FNV-1a 64-bit: fast, low-collision, deterministic.
        // Path separators are normalised (\\ → /) for cross-platform consistency.
        constexpr uint64_t kOffsetBasis = 14695981039346656037ULL;
        constexpr uint64_t kPrime = 1099511628211ULL;

        uint64_t hash = kOffsetBasis;
        for(const char c : relativePath)
        {
            const char n = (c == '\\') ? '/' : c;
            hash ^= static_cast<uint64_t>(n);
            hash *= kPrime;
        }
        return hash;
    }

} // namespace Surge