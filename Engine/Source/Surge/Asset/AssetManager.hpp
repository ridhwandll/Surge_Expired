// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/MemoryBlock.hpp"
#include "Serializer/IAssetSerializer.hpp"
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
        AssetManager();
        ~AssetManager();

        void Initialize(const Path& assetDirectory);
        void Shutdown();

        // Create
        // Instantiates a new asset of type T, saves it to disk at the given path, and registers it.
        // @param relativePath  Path where the new asset should be saved.
        // @param args          Arguments to forward to the asset's Create method
        // @return              Stable AssetID of the newly created asset
        template<IsAssetConcept T, typename... Args>
        Ref<T> Create(const String& relativePath, Args&&... args)
        {
            if(GetIDFromPath(relativePath).IsValid())
            {
                Log<Severity::Warn>("[AssetManager] Create: Asset already exists at path '{}'", relativePath);
                return nullptr;
            }
            AssetID newID = UUID();

            AssetMetadata meta;
            meta.ID = newID;
            meta.Type = T::GetStaticType();
            meta.RelativePath = relativePath;
            meta.Flags = AssetFlags::VALID | AssetFlags::LOADED;
            mAssetRegistry[newID] = meta;

            Ref<T> newAsset = T::Create(std::forward<Args>(args)...);
            newAsset->mID = newID;
            mLoadedAssets[newID] = newAsset;

            Save(meta.ID);

            Log<Severity::Info>("[AssetManager] Created & Saved new asset: '{}'", relativePath);
            return newAsset;
        }

        // Import
        // Registers a source file into the registry. Returns the existing ID if already imported
        // @param str     Path relative to the assets directory e.g. "Textures/Surge.png" OR memoryStr if created from memory e.g. DefaultMeshes::CUBE
        // @param type    Explicit asset type, must match intended usage
        // @return        Stable AssetID, or UUID::INVALID on failure
        AssetID Import(const String& str, AssetType type);

        // Load<T>
        // Returns a cached Ref<T> immediately if already loaded, performs a full synchronous load (CPU + GPU) otherwise
        // @param id    AssetID of the requested asset
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
        // Removes the live Ref from the cache if no other objects are referencing it
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
        void UnregisterAsset(AssetID id) { mAssetRegistry.erase(id); SerializeRegistry(); }

        // GetIDFromPath
        // Returns the AssetID associated with a given relative path.
        // This is a linear search, so avoid using this in performance critical code.
        // It's best to cache the AssetID after the first lookup if you need to reference an asset by path multiple times
        // @param relativePath    Path to the asset relative to the assets directory
        // @return                AssetID of the asset, or UUID::INVALID if not found
        AssetID GetIDFromPath(const String& relativePath);

        // UpdateAssetPath
        // Updates the relative path of an already registered asset. This is used when renaming/moving source files on disk.
        // @param id                 AssetID of the asset to update
        // @param newRelativePath    New path relative to the assets directory
        void UpdateAssetPath(AssetID id, const String& newRelativePath);

        const AssetMetadata& GetMetadata(AssetID id);
        const String& GetAssetsDirectory() { return sAssetsDirectory; }
        const std::unordered_map<AssetID, AssetMetadata>& GetRegistryMap() { return mAssetRegistry; }
        size_t GetAssetRefCount(AssetID id)
        {
            auto it = mLoadedAssets.find(id);
            if(it != mLoadedAssets.end())
                return it->second->GetRefCount();
            return 0;
        }

        // GetSidecarPath
        // Returns the absolute/relative path to the sidecar file for a given asset file. The sidecar file is where the cooked runtime data is stored for an asset
        // @param filePath    Relative/Absolute, but must include the file extension. The returned sidecar path will just have the sidecar extension
        // @param type        AssetType of the asset
        String GetSidecarPath(const String& filePath, AssetType type);

        // Registry
        void SerializeRegistry();
        bool DeserializeRegistry();
    private:
        Ref<Asset> LoadAsset(AssetID id);

        // Utilities
    public:
        String GetAbsolutePath(const String& relativePath) { return sAssetsDirectory + '/' + relativePath; }
    private:
        String sAssetsDirectory;
        std::unordered_map<AssetID, AssetMetadata> mAssetRegistry;
        std::unordered_map<AssetID, Ref<Asset>> mLoadedAssets;
        std::unordered_map<AssetType, Scope<AssetSerializer>> mSerializers;
        bool mInitialized = false;

        static constexpr const char* kRegistryFilename = "AssetRegistry.surge";
        static constexpr const char* kRegistryDelimiter = "|";
    };

} // namespace Surge
