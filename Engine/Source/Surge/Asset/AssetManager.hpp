// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Asset.hpp"
#include "Surge/Asset/AssetMetadata.hpp"
#include "Surge/Core/Path.hpp"
#include <unordered_map>

namespace Surge
{
    template<typename T>
    concept IsAssetConcept = std::is_base_of_v<Surge::Asset, T> && !std::is_same_v<Surge::Asset, T>;

    class AssetManager
    {
    public:
        static void Initialize(const Path& assetDirectory);
        static void Shutdown();

        // Import
        // Registers a source file into the registry. Returns the existing ID if already imported
        // @param str     Path relative to the assets directory e.g. "Textures/Surge.png" OR memoryStr if created from memory e.g. DefaultMeshes::CUBE
        // @param type    Explicit asset type, must match intended usage
        // @return        Stable AssetID, or UUID::INVALID on failure
        static AssetID Import(const String& str, AssetType type);

        // ImportLive
        // Registers an already-live asset, stamps its ID, adds it to the loaded cache. Use this when the asset is created in memory before it exists on disk
        // Serialize the asset to disk first before calling this method, then use the same relativePath in relativePath parameter
        // @param relativePath    Path to which you serialized this asset in before calling this method
        // @param type            Explicit asset type, must match intended usage
        // @return                Stable AssetID, or UUID::INVALID on failure
        static AssetID ImportLive(const String& relativePath, AssetType type, Ref<Asset> asset);

        // Load<T>
        // Returns a cached Ref<T> immediately if already loaded, performs a full synchronous load (CPU + GPU) otherwise
        // @param id    AssetID of the requested mesh
        // @return      Ref<T> of the requested underlying concrete asset
        template<IsAssetConcept T>
        static Ref<T> Load(AssetID id)
        {
            return LoadAsset(id).As<T>();
        }

        // Unload
        // Removes the live Ref from the cache. The actual GPU memory is freed once all external Refs (held by scene objects etc.) drop out of scope
        // @param id    AssetID of the asset to be removed
        // @return      true if unload was successful, false otherwise
        static bool Unload(AssetID id);

        // IsLoaded
        // Checks if the asset is currently loaded in memory
        // @param id    AssetID of the asset to check
        // @return      true if the asset is loaded, false otherwise
        static bool IsLoaded(AssetID id);

        // IsRegistered
        // Checks if the asset is registered in the registry (i.e. has been imported)
        // @param id    AssetID of the asset to check
        // @return      true if the asset is registered, false otherwise
        static bool IsRegistered(AssetID id);

        // Save
        // Saves the asset to disk
        // @param id    AssetID of the asset to save
        static void Save(AssetID id);

        // UnregisterAsset
        // Removes the asset from the registry
        // @param id    AssetID of the asset to unregister
        static void UnregisterAsset(AssetID id) { sAssetRegistry.erase(id); }

        // GetIDFromPath
        // Returns the AssetID associated with a given relative path.
        // This is a linear search, so avoid using this in performance critical code.
        // It's best to cache the AssetID after the first lookup if you need to reference an asset by path multiple times
        // @param relativePath    Path to the asset relative to the assets directory
        // @return                AssetID of the asset, or UUID::INVALID if not found
        static AssetID GetIDFromPath(const String& relativePath);

        static const AssetMetadata& GetMetadata(AssetID id);
        static const String& GetAssetsDirectory() { return sAssetsDirectory; }
        static const std::unordered_map<AssetID, AssetMetadata>& GetRegistryMap() { return sAssetRegistry; }
        static size_t GetAssetRefCount(AssetID id)
        {
            auto it = sLoadedAssets.find(id);
            if(it != sLoadedAssets.end())
                return it->second->GetRefCount();
            return 0;
        }

        // Registry
        static void SerializeRegistry();
        static bool DeserializeRegistry();
    private:
        static Ref<Asset> LoadAsset(AssetID id);
        static Ref<Asset> LoadInternal(const AssetMetadata& metadata);
        static void SaveInternal(const AssetMetadata& meta, const Ref<Asset>& asset);

        // Utilities
    public:
        static String GetAbsolutePath(const String& relativePath) { return sAssetsDirectory + '/' + relativePath; }
    private:
        static String sAssetsDirectory;
        static std::unordered_map<AssetID, AssetMetadata> sAssetRegistry;
        static std::unordered_map<AssetID, Ref<Asset>> sLoadedAssets;
        static bool sInitialized;

        static constexpr const char* kRegistryFilename = "AssetRegistry.surge";
        static constexpr const char* kRegistryDelimiter = "|";
    };

} // namespace Surge
