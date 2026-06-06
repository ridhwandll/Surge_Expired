// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "../Core/MemoryBlock.hpp"
#include "Surge/Asset/Asset.hpp"
#include "Surge/Asset/AssetMetadata.hpp"
#include "Surge/Core/Path.hpp"
#include "SurgeReflect/Enum.hpp"
#include <unordered_map>

namespace Surge
{
    template<typename T>
    concept IsAssetConcept = std::is_base_of_v<Surge::Asset, T> && !std::is_same_v<Surge::Asset, T>
        && requires { { T::GetStaticType() } -> std::same_as<AssetType>; };

    class AssetManager
    {
    public:
        void Initialize(const Path& assetDirectory);
        void Shutdown();

        // Import
        // Registers a source file into the registry. Returns the existing ID if already imported
        // @param str     Path relative to the assets directory e.g. "Textures/Surge.png" OR memoryStr if created from memory e.g. DefaultMeshes::CUBE
        // @param type    Explicit asset type, must match intended usage
        // @return        Stable AssetID, or UUID::INVALID on failure
        AssetID Import(const String& str, AssetType type);

        // ImportLive
        // Registers an already-live asset, stamps its ID, adds it to the loaded cache. Use this when the asset is created in memory before it exists on disk
        // Serialize the asset to disk first before calling this method, then use the same relativePath in relativePath parameter
        // @param relativePath    Path to which you serialized this asset in before calling this method
        // @param type            Explicit asset type, must match intended usage
        // @return                Stable AssetID, or UUID::INVALID on failure
        AssetID ImportLive(const String& relativePath, AssetType type, Ref<Asset> asset);

        // Load<T>
        // Returns a cached Ref<T> immediately if already loaded, performs a full synchronous load (CPU + GPU) otherwise
        // @param id    AssetID of the requested mesh
        // @return      Ref<T> of the requested underlying concrete asset
        template<IsAssetConcept T>
        Ref<T> Load(AssetID id)
        {
            const AssetMetadata& metadata = GetMetadata(id);

            if(HasFlag(metadata.Flags, AssetFlags::MISSING))
            {
                Log<Severity::Error>("[AssetManager] Load: asset is missing for ID {}!", id.Get());
                return nullptr;
            }
            if(metadata.Type != T::GetStaticType())
            {
                Log<Severity::Error>("[AssetManager] Load: type mismatch for ID {}! " "Requested '{}' but registry says '{}'.", id.Get(), SurgeReflect::EnumToString(T::GetStaticType()).data(), SurgeReflect::EnumToString(metadata.Type).data());
                return nullptr;
            }


            Ref<Asset> asset = LoadAsset(id);
            SG_ASSERT(!asset || asset->GetAssetType() == T::GetStaticType(), "[AssetManager] LoadInternal returned wrong type, loader bug!");
            return asset.As<T>();
        }

        // Unload
        // Removes the live Ref from the cache. The actual GPU memory is freed once all external Refs (held by scene objects etc.) drop out of scope
        // @param id    AssetID of the asset to be removed
        // @return      true if unload was successful, false otherwise
        bool Unload(AssetID id);

        // IsLoaded
        // Checks if the asset is currently loaded in memory
        // @param id    AssetID of the asset to check
        // @return      true if the asset is loaded, false otherwise
        bool IsLoaded(AssetID id);

        // IsRegistered
        // Checks if the asset is registered in the registry (i.e. has been imported)
        // @param id    AssetID of the asset to check
        // @return      true if the asset is registered, false otherwise
        bool IsRegistered(AssetID id);

        // Save
        // Saves the asset to disk
        // @param id    AssetID of the asset to save
        void Save(AssetID id);

        // UnregisterAsset
        // Removes the asset from the registry
        // @param id    AssetID of the asset to unregister
        void UnregisterAsset(AssetID id) { sAssetRegistry.erase(id); }

        // GetIDFromPath
        // Returns the AssetID associated with a given relative path.
        // This is a linear search, so avoid using this in performance critical code.
        // It's best to cache the AssetID after the first lookup if you need to reference an asset by path multiple times
        // @param relativePath    Path to the asset relative to the assets directory
        // @return                AssetID of the asset, or UUID::INVALID if not found
        AssetID GetIDFromPath(const String& relativePath);

        const AssetMetadata& GetMetadata(AssetID id);
        const String& GetAssetsDirectory() { return sAssetsDirectory; }
        const std::unordered_map<AssetID, AssetMetadata>& GetRegistryMap() { return sAssetRegistry; }
        size_t GetAssetRefCount(AssetID id)
        {
            auto it = sLoadedAssets.find(id);
            if(it != sLoadedAssets.end())
                return it->second->GetRefCount();
            return 0;
        }

        // Registry
        void SerializeRegistry();
        bool DeserializeRegistry();
    private:
        Ref<Asset> LoadAsset(AssetID id);
        Ref<Asset> LoadInternal(const AssetMetadata& metadata);
        void SaveInternal(const AssetMetadata& meta, const Ref<Asset>& asset);

        // Utilities
    public:
        String GetAbsolutePath(const String& relativePath) { return sAssetsDirectory + '/' + relativePath; }
    private:
        String sAssetsDirectory;
        std::unordered_map<AssetID, AssetMetadata> sAssetRegistry;
        std::unordered_map<AssetID, Ref<Asset>> sLoadedAssets;
        bool sInitialized;

        static constexpr const char* kRegistryFilename = "AssetRegistry.surge";
        static constexpr const char* kRegistryDelimiter = "|";
    };

} // namespace Surge
