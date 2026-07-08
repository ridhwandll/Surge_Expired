// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Scope.hpp"
#include "Serializer/IAssetSerializer.hpp"
#include "Surge/Asset/Asset.hpp"
#include "Surge/Asset/AssetMetadata.hpp"
#include "Surge/Core/Path.hpp"
#include "SurgeReflect/Enum.hpp"
#include <unordered_map>

namespace Surge
{
    template<typename T>
    concept IsAssetConcept = std::is_base_of_v<Surge::Asset, T> && !std::is_same_v<Surge::Asset, T> && requires { { T::GetStaticType() } -> std::same_as<AssetType>; };

    class AssetManager;
    class AssetLoadCallback
    {
    public:
        virtual ~AssetLoadCallback() = default;
    protected:
        // OnAssetLoad
        // Called internally when an asset is requested to be loaded. Return true to force a reload(skip cache), false to use the cached version if available.
        // @param id    AssetID of the asset being loaded
        // @param meta  AssetMetadata of the asset being loaded
        // @return      true to force a reload, false to use cached version if available
        virtual bool OnAssetLoad(AssetID id, AssetMetadata& meta) = 0;
        friend class AssetManager;
    };

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

            Log<Severity::Info>("[AssetManager] Created & Saved new asset: {}", relativePath);
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
        // DO NOT CALL Load<T> every frame!
        // @param id    AssetID of the requested asset
        // @return      Ref<T> of the requested underlying concrete asset
        template<IsAssetConcept T>
        Ref<T> Load(AssetID id)
        {
            auto cacheIt = mLoadedAssets.find(id);
            bool isCached = cacheIt != mLoadedAssets.end();
            bool shouldReload = false;

            AssetMetadata* metadata = nullptr;
            if(mLoadCallback)
            {
                metadata = GetMetadataForEdit(id);
                if(!metadata)
                {
                    Log<Severity::Error>("[AssetManager] Load: Invalid AssetID {}!", id.Get());
                    return nullptr;
                }

                // Trigger user callback (In Editor this reads the binary file header to see if it is uptodate, Cooks the asset if needed)
                shouldReload = mLoadCallback->OnAssetLoad(id, *metadata);
            }

            // FAST PATH
            if(isCached && !shouldReload)
            {
                SG_ASSERT(cacheIt->second->GetAssetType() == T::GetStaticType(), "[AssetManager] Load: Cached asset type mismatch!");
                return cacheIt->second.As<T>();
            }

            // SLOW PATH
            if(!metadata)
                metadata = GetMetadataForEdit(id); // Fetch if we are in Release mode (no hook)

            if(!metadata)
            {
                Log<Severity::Error>("[AssetManager] Load: No metadata found for AssetID {}!", id.Get());
                return nullptr;
            }

            if(HasFlag(metadata->Flags, AssetFlags::MISSING))
            {
                Log<Severity::Error>("[AssetManager] Load: Asset is marked as MISSING for ID {}!", id.Get());
                return nullptr;
            }

            if(metadata->Type != T::GetStaticType())
            {
                Log<Severity::Error>("[AssetManager] Load: Type mismatch for ID {}! Requested {} but registry says {}!", id.Get(), SurgeReflect::EnumToString(T::GetStaticType()).data(), SurgeReflect::EnumToString(metadata->Type).data());
                return nullptr;
            }
            Ref<Asset> asset = LoadAssetInternal(id, metadata);
            SG_ASSERT(!asset || asset->GetAssetType() == T::GetStaticType(), "[AssetManager] LoadInternal returned wrong type, loader bug!");
            return asset ? asset.As<T>() : nullptr;
        }

// TODO: Make a faster version of Load
//         Ref<T> Get(AssetID id)
//         {
//             auto cacheIt = mLoadedAssets.find(id);
//             if(cacheIt != mLoadedAssets.end())
//                 return cacheIt->second.As<T>();
//             return nullptr;
//         }

        // Unload
        // Removes the live Ref from the cache if no other objects are referencing it i.e. Ref count = 1, otherwise does nothing since it's still in use 
        // somewhere else in the program Note that the asset will still be registered and can be loaded again later, this just removes it from memory
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

        // GetAssetRefCount
        // Returns the current reference count of the asset, asset ref count = 1 means only the AssetManager is referencing it, and it can be safely unloaded if desired
        // @param id    AssetID of the asset to check
        // @return      Reference count of the asset, or 0 if the asset is not loaded
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
        // @param id          AssetID of the asset
        // @param type        [Internal] Used for internal purposes, do NOT specify
        String GetSidecarPath(AssetID id, AssetType type = AssetType::NONE);

        // AddAssetLoadCallback
        // Use this to add a callback function that will be called every time an asset is loaded
        // @param hook An overriden class of AssetLoadCallback that implements the OnAssetLoad method.
        //             The method will be called with the AssetID and AssetMetadata of the asset being loaded, and should return true if the asset should be 
        //             reloaded from disk or false if it should not be reloaded
        void AddAssetLoadCallback(Scope<AssetLoadCallback>&& hook) { mLoadCallback = std::move(hook); }

        // Registry
        void SerializeRegistry();
        bool DeserializeRegistry();

        // Utilities
        String GetAbsolutePath(const String& relativePath) const { return sAssetsDirectory + '/' + relativePath; }
    private:
        Ref<Asset> LoadAssetInternal(AssetID id, AssetMetadata* meta);
        AssetMetadata* GetMetadataForEdit(AssetID id);
        String GetSidecarDirectory(AssetType type);

    private:
        String sAssetsDirectory;
        std::unordered_map<AssetID, AssetMetadata> mAssetRegistry;
        std::unordered_map<AssetID, Ref<Asset>> mLoadedAssets;
        std::unordered_map<AssetType, Scope<AssetSerializer>> mSerializers;
        Scope<AssetLoadCallback> mLoadCallback = nullptr;

        bool mInitialized = false;

        static constexpr const char* kRegistryFilename = "AssetRegistry.surge";
        static constexpr const char* kRegistryDelimiter = "|";
    };

} // namespace Surge
