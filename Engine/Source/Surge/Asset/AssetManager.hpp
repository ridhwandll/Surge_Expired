// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Scope.hpp"
#include "Surge/Asset/Asset.hpp"
#include "Surge/Asset/AssetMetadata.hpp"
#include "Surge/Asset/AssetContext.hpp"
#include "Surge/Asset/Format/AssetFormat.hpp"
#include "Surge/Core/Path.hpp"
#include "SurgeReflect/Enum.hpp"
#include <unordered_map>

namespace Surge
{
    template<typename T>
    concept IsAssetConcept = std::is_base_of_v<Surge::Asset, T> && !std::is_same_v<Surge::Asset, T> && requires { { T::GetStaticType() } -> std::same_as<AssetType>; };

    // Installed by the Editor (AssetManager::SetCookHook), the runtime has none
    class AssetCookHook
    {
    public:
        virtual ~AssetCookHook() = default;

        // Called by Load before it returns an asset. Cooks the asset when its cooked file is missing or older than its source.
        // cached: the asset is loaded already. Return true when the cooked file was rewritten, a cached asset is then read again
        virtual bool OnLoad(AssetID id, const AssetMetadata& metadata, bool cached) = 0;
    };

    // The runtime side of the asset pipeline: reads cooked binary files (Internal/<TYPE>/<id>.r<TYPE>) through the AssetFormat of their type,
    // caches the live assets and resolves paths. It never writes a file and knows no source format: the Editor's AssetDatabase imports,
    // cooks and saves, and keeps this registry in sync through Register/Unregister/SetPath/Invalidate
    class AssetManager
    {
    public:
        AssetManager();
        ~AssetManager();

        // context.AssetManager_ is this AssetManager. Reads the cooked registry (Internal/Registry.rREG) when the directory has one (exported games)
        void Initialize(const Path& assetDirectory, const AssetContext& context);
        void Shutdown();

        // What the formats create assets with (Renderer, SceneContext), for code that reads cooked files itself
        const AssetContext& GetContext() const { return mContext; }

        // Load<T>
        // Returns a cached Ref<T> immediately if already loaded, performs a full synchronous load (CPU + GPU) otherwise
        // DO NOT CALL Load<T> every frame!
        // @param id    AssetID of the requested asset
        // @return      Ref<T> of the requested underlying concrete asset
        template<IsAssetConcept T>
        Ref<T> Load(AssetID id)
        {
            AssetMetadata* metadata = GetMetadataForEdit(id);
            if(!metadata)
            {
                Log<Severity::Error>("[AssetManager] Load: No metadata found for AssetID {}!", id.Get());
                return nullptr;
            }

            auto cacheIt = mLoadedAssets.find(id);
            bool reload = false;
            if(mCookHook)
            {
                // In the Editor this cooks the asset when its source changed. Cooking can load and register other assets (a scene's meshes,
                // a model's textures), which rehashes the cache and the registry
                const AssetMetadata copy = *metadata;
                reload = mCookHook->OnLoad(id, copy, cacheIt != mLoadedAssets.end());
                cacheIt = mLoadedAssets.find(id);
                metadata = GetMetadataForEdit(id);
                if(!metadata)
                    return nullptr;
            }

            // FAST PATH
            if(cacheIt != mLoadedAssets.end() && !reload)
            {
                if(cacheIt->second->GetAssetType() != T::GetStaticType())
                    return nullptr;
                return cacheIt->second.As<T>();
            }

            // SLOW PATH
            // The Player has nothing that could bring a MISSING asset back. In the Editor the cook hook may have fixed it, so always retry
            if(!mCookHook && HasFlag(metadata->Flags, AssetFlags::MISSING))
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
            SG_ASSERT(!asset || asset->GetAssetType() == T::GetStaticType(), "[AssetManager] LoadInternal returned wrong type, format bug!");
            return asset ? asset.As<T>() : nullptr;
        }

        // GetLoaded<T>
        // Cache lookup only: no loading, no cook hook. A reload replaces the cached asset, so this is always the newest version of an asset that is loaded
        // @param id    AssetID of the requested asset
        // @return      Ref<T> of the cached asset, null if it is not loaded or has another type
        template<IsAssetConcept T>
        Ref<T> GetLoaded(AssetID id) const
        {
            auto cacheIt = mLoadedAssets.find(id);
            if(cacheIt == mLoadedAssets.end() || cacheIt->second->GetAssetType() != T::GetStaticType())
                return nullptr;
            return cacheIt->second.As<T>();
        }

        // GetBuiltin
        // Registers (once) and returns the ID of an asset made in code, name is an "Engine://" path e.g. DefaultMesh::CUBE
        // @return      Stable AssetID, or UUID::INVALID if name is not a built-in path
        AssetID GetBuiltin(const String& name, AssetType type);

        // Unload
        // Removes the live Ref from the cache if no other objects are referencing it i.e. Ref count = 1, otherwise does nothing since it's still in use
        // somewhere else in the program. The asset stays registered and can be loaded again later, this just removes it from memory
        // @param id    AssetID of the asset to be removed
        // @return      true if unload was successful, false otherwise
        bool Unload(AssetID id);

        // UnloadUnreferenced
        // Drops every cached asset nothing else references (Ref count = 1), repeated until nothing changes since freeing a Material/Mesh/UILayout
        // releases the textures/fonts it held. Call it after the old scene is released on a scene change, assets the next scene still uses survive
        // @return      Number of assets unloaded
        Uint UnloadUnreferenced();

        bool IsLoaded(AssetID id) const;
        bool IsRegistered(AssetID id) const;

        // GetIDFromPath
        // Returns the AssetID associated with a given relative path (hash lookup, the match is exact and case sensitive like the APK)
        // @param relativePath    Path to the asset relative to the assets directory
        // @return                AssetID of the asset, or UUID::INVALID if not found
        AssetID GetIDFromPath(const String& relativePath) const;

        const AssetMetadata& GetMetadata(AssetID id) const;
        const String& GetAssetsDirectory() const { return mAssetsDirectory; }
        const std::unordered_map<AssetID, AssetMetadata>& GetRegistryMap() const { return mAssetRegistry; }
        const AssetFormat* GetFormat(AssetType type) const;

        // GetAssetRefCount
        // Returns the current reference count of the asset, asset ref count = 1 means only the AssetManager is referencing it, and it can be safely unloaded if desired
        size_t GetAssetRefCount(AssetID id) const
        {
            auto it = mLoadedAssets.find(id);
            return it != mLoadedAssets.end() ? it->second->GetRefCount() : 0;
        }

        // GetSidecarPath
        // Path of the cooked file of an asset: <assets directory>/Internal/<TYPE>/<id>.r<TYPE>
        // @param id          AssetID of the asset
        // @param type        The asset's type when it is not registered yet, NONE = the registry's
        String GetSidecarPath(AssetID id, AssetType type = AssetType::NONE) const;

        // Editor (AssetDatabase) side: in memory only, none of these touch the disk

        // Register
        // Adds an asset to the registry, refused (false) when its ID or path is registered already
        bool Register(const AssetMetadata& metadata);

        // Unregister
        // Removes the asset from the registry (and from the cache when nothing else references it)
        void Unregister(AssetID id);

        // SetPath
        // Moves a registered asset to another relative path (rename/move of its source), refused if another asset is registered there
        bool SetPath(AssetID id, const String& newRelativePath);

        // Adopt
        // Caches an asset created in memory (a new scene, material...) as the loaded version of a registered asset and stamps its ID
        void Adopt(AssetID id, Ref<Asset> asset);

        // Invalidate
        // Its cooked file changed on disk: the next Load reads it again. Holders of the old Ref keep using it until they load it again
        void Invalidate(AssetID id);

        // Clear
        // Empties the registry and the cache (the Editor opens another project)
        void Clear();

        void SetCookHook(Scope<AssetCookHook>&& hook) { mCookHook = std::move(hook); }

    private:
        Ref<Asset> LoadAssetInternal(AssetID id, AssetMetadata* meta);
        AssetMetadata* GetMetadataForEdit(AssetID id);
        String GetSidecarDirectory(AssetType type) const;
        void LoadCookedRegistry();

    private:
        String mAssetsDirectory;
        std::unordered_map<AssetID, AssetMetadata> mAssetRegistry;
        std::unordered_map<String, AssetID> mPathToID; // Mirrors mAssetRegistry's RelativePaths, only the methods below may change either
        std::unordered_map<AssetID, Ref<Asset>> mLoadedAssets;
        std::unordered_map<AssetType, Scope<AssetFormat>> mFormats;
        Scope<AssetCookHook> mCookHook = nullptr;
        AssetContext mContext;

        bool mInitialized = false;
    };

} // namespace Surge
