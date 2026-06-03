// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Asset.hpp"
#include "Surge/Asset/AssetMetadata.hpp"
#include <unordered_map>

namespace Surge
{
    // =========================================================================
    // Surge Engine AssetManager
    //
    // Central registry and cache for all engine assets
    //
    // Lifecycle
    // ---------
    //    Import  : Assigns a stable AssetID, writes a .surgeasset sidecar
    //    Load<T> : Synchronous; Blocks calling thread until CPU + GPU resources are ready
    //    Unload  : Removes from the live cache(sLoadedAssets); GPU memory frees when last Ref drops (at Asset destructor)
    //
    // Registry persistence
    // --------------------
    //    Editor  : SerializeRegistry() on save
    //    Runtime : DeserializeRegistry() on Initialize() no sidecars needed
    //    Format  : "Assets/AssetRegistry.surge" one entry per line
    // =========================================================================

    template<typename T>
    concept IsAssetConcept = std::is_base_of_v<Surge::Asset, T> && !std::is_same_v<Surge::Asset, T>;

    class AssetManager
    {
    public:
        static void Initialize(const String& assetDirectory);
        static void Shutdown();

        // -------------------------------------------------------------------------
        // Import (editor-side operation)
        //
        // Registers a source file into the registry. Creates a .surgeasset sidecar alongside the file to guarantee a stable AssetID across renames
        // Returns the existing ID if already imported
        //
        // @param relativePath  Path relative to the assets directory.
        //                      e.g.  "Textures/Hero.png"
        // @param type          Explicit asset type, must match intended usage
        //                      (Texture2D vs Sprite requires raw pixels; decide here)
        // @return              Stable AssetID, or UUID::INVALID on failure.
        // -------------------------------------------------------------------------
        static AssetID Import(const String& relativePath, AssetType type);

        // -------------------------------------------------------------------------
        // Load<T>  (Synchronous, main thread)
        // Returns a cached Ref<T> immediately if already loaded
        // Performs a full synchronous load (CPU + GPU) otherwise
        // -------------------------------------------------------------------------
        template<IsAssetConcept T>
        static Ref<T> Load(AssetID id)
        {
            return LoadAsset(id).As<T>();
        }
        // -------------------------------------------------------------------------
        // Unload
        // Removes the live Ref from the cache. The actual GPU memory is freed once all external Refs (held by scene objects etc.) drop out of scope
        // -------------------------------------------------------------------------
        static bool Unload(AssetID id);

        static bool IsLoaded(AssetID id);
        static bool IsRegistered(AssetID id);
        static const AssetMetadata& GetMetadata(AssetID id);
        static AssetID GetIDFromPath(const String& relativePath);

        static const String& GetAssetsDirectory() { return sAssetsDirectory; }

        // Registry
        static void SerializeRegistry();
        static bool DeserializeRegistry();

    private:
        static AssetID ImportFromMemory(const String& memoryStr, AssetType type);
        static Ref<Asset> LoadAsset(AssetID id);
        static Ref<Asset> LoadInternal(const AssetMetadata& metadata);

        // Utilities
        static String GetAbsolutePath(const String& relativePath) { return sAssetsDirectory + '/' + relativePath; }
    private:
        static String sAssetsDirectory;
        static std::unordered_map<AssetID, AssetMetadata> sAssetRegistry;
        static std::unordered_map<AssetID, Ref<Asset>> sLoadedAssets;

        static constexpr const char* kRegistryFilename = "AssetRegistry.surge";
        static constexpr char kRegistryDelimiter = '|';
    };

} // namespace Surge
