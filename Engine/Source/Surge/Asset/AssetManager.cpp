// Copyright (c) - SurgeTechnologies - All rights reserved
#include "AssetManager.hpp"

#include "Serializer/Script/ScriptSerializer.hpp"
#include "Serializer/Material/MaterialSerializer.hpp"
#include "Serializer/Mesh/MeshSerializer.hpp"
#include "Serializer/SceneSerializer.hpp"
#include "Serializer/Texture2DSerializer.hpp"
#include "Serializer/Font/FontSerializer.hpp"

#include "Surge/Utility/Filesystem.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>

namespace Surge
{
    AssetManager::AssetManager()
    {
        mSerializers[AssetType::SCENE] = CreateScope<SceneSerializer>();
        mSerializers[AssetType::TEXTURE2D] = CreateScope<Texture2DSerializer>();
        mSerializers[AssetType::MESH] = CreateScope<MeshSerializer>();
        mSerializers[AssetType::MATERIAL] = CreateScope<MaterialSerializer>();
        mSerializers[AssetType::SCRIPT] = CreateScope<ScriptSerializer>();
        mSerializers[AssetType::FONT] = CreateScope<FontSerializer>();

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

        Log<Severity::Info>("[AssetManager] Imported '{}' | ID: {} | Type: {}", relativePath, id.Get(), SurgeReflect::EnumToString(type).data());
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

        if (mAssetLoadHook)
            mAssetLoadHook(id, meta);

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

        auto serIt = mSerializers.find(mAssetRegistry.at(id).Type);
        SG_ASSERT(serIt != mSerializers.end() && serIt->second, "[AssetManager] Save: No serializer found!'");
        serIt->second->Serialize(cacheIt->second);
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
        for(const auto& [id, meta] : mAssetRegistry)
        {
            if(meta.RelativePath == relativePath)
                return id;
        }
        return UUID::INVALID;
    }

    void AssetManager::UpdateAssetPath([[maybe_unused]] AssetID id, [[maybe_unused]] const String& newRelativePath)
    {
#ifdef SURGE_PLATFORM_ANDROID
        Log<Severity::Error>("[AssetManager] UpdateAssetPath is an Editor function. APK assets are readonly!");
#else
        auto it = mAssetRegistry.find(id);
        if(it != mAssetRegistry.end())
        {
            it->second.RelativePath = newRelativePath;
            SerializeRegistry();
        }
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
            Log<Severity::Warn>("[AssetManager] Failed to open '{}'", registryPath);
            return false;
        }

        mAssetRegistry.clear();

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

                if(!sourceExists)
                    Log<Severity::Warn>("[AssetManager] DeserializeRegistry: Source file missing for {}", relPath);
                if (!exists)
                    Log<Severity::Warn>("[AssetManager] DeserializeRegistry: Asset Source & Sidecar is missing for {}", relPath);
            }

            mAssetRegistry[id] = std::move(meta);
            count++;
        }

        Log<Severity::Info>("[AssetManager] Registry deserialized ({} entries).", count);
        return count > 0;
    }

} // namespace Surge