// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Memory.hpp"
#include "Surge/Core/String.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/AssetManager/AssetRegistry.hpp"
#include "Surge/AssetManager/AssetImporter/AssetLoader.hpp"

namespace Surge
{
    class AssetManager
    {
    public:
        AssetManager() = default;
        ~AssetManager() = default;

        // Initializes the AssetManager
        static void Init();

        // Shutdowns the AssetManager, clears all registry
        static void Shutdown();

        // Writes the registry cache to the file
        static void SerializeRegistry();

        // Loads the registry cache from the file
        static void DeserializeRegistry();

        // Returns the Absolute Path from the relative path stored in the metadata
        // Design choice: No one else will know the absolute path of the asset except the AssetManager
        //static String GetAbsolutePath(const AssetMetadata& metadata);
        //static String GetAbsolutePath(const String& path);

        // Returns the Relative Path from the given absolute path
        static String GetRelativePath(const String& absolutePath);

        // Imports a specific asset, doesn't load the data to the RAM
        static AssetHandle ImportAsset(const String& filepath);

        // Fetches the metadata of a specific asset
        static AssetMetadata& GetMetadata(AssetHandle handle);

        // Retrieves the AssetType from the given extension
        static AssetType GetAssetTypeFromExtension(const String& extension);

        // Checks if an asset Exists in Registry or not
        static bool ExistsInRegistry(const String& absPath);

        // Checks if an asset Exists in Loaded Registry or not
        static bool IsLoaded(const AssetHandle& handle);

        // Returns the pool for loaded assets
        static std::unordered_map<AssetHandle, Ref<Asset>>* GetLoadedAssetsRegistry() { return &sLoadedAssets; }

        // Returns the AssetRegistry
        static AssetRegistry* GetRegistry() { return &sAssetRegistry; }

        // Removes an asset from the registry
        static void RemoveAsset(AssetHandle assetHandle);

        // Returns AssetHandle of a given asset path
        static AssetHandle GetHandleFromPath(const String& assetPath);

        // Creates a brand NEW asset, loads it to RAM and writes it to the registry
        // relativePath is the path relative to the Assets Directory, and it will be stored in the registry as is, so make sure to give it in correct format
        // Example: "Assets/Textures/Texture.png" or "Assets/Scenes/Scene1.scene"
        // Always start with "Assets/" and use forward slashes, even on Windows
        template <typename T, typename... Args>
        static Ref<T> CreateNewAsset(String& relativePath, AssetType type, Args&&... args)
        {
            static_assert(std::is_base_of<Asset, T>::value, "Class must derive from Asset");

            // Create the Metadata for the Asset
            AssetMetadata metadata;
            metadata.Handle = AssetHandle();
            metadata.Path = relativePath; // Relative path is stored
            metadata.Type = type;
            metadata.IsDataLoaded = true;

            // Store the Asset with the metadata in the AssetRegistry
            sAssetRegistry[relativePath] = metadata;

            // Create the Actual Asset that will get used
            Ref<T> asset = T::Create(std::forward<Args>(args)...);

            // Fill in the data for Assets
            asset->SetHandle(metadata.Handle);
            asset->SetType(metadata.Type);
            asset ? asset->SetFlag(AssetFlag::VALID) : asset->SetFlag(AssetFlag::INVALID);

            // Store that in sLoadedAssets(technically in RAM) for quick access
            sLoadedAssets[metadata.Handle] = asset;

            SerializeRegistry();
            return asset;
        }

        // Returns the Asset, from the handle
        template<typename T>
        static Ref<T> GetAsset(AssetHandle assetHandle)
        {
            AssetMetadata& metadata = GetMetadata(assetHandle);

            Ref<Asset> asset = nullptr;
            if (!metadata.IsDataLoaded)
            {
                metadata.IsDataLoaded = AssetLoader::TryLoadData(metadata, asset);
                if (!metadata.IsDataLoaded)
                    return nullptr;

                sLoadedAssets[assetHandle] = asset;
            }
            else
                asset = sLoadedAssets[assetHandle];

            return asset.As<T>();
        }
    private:
        static std::unordered_map<AssetHandle, Ref<Asset>> sLoadedAssets;
        static AssetRegistry sAssetRegistry;
    };
}
