// Copyright (c) - SurgeTechnologies - All rights reserved
#include "AssetManager.hpp"

#include "Format/Audio/AudioFormat.hpp"
#include "Format/Font/FontFormat.hpp"
#include "Format/Material/MaterialFormat.hpp"
#include "Format/Mesh/MeshFormat.hpp"
#include "Format/ParticleEffect/ParticleEffectFormat.hpp"
#include "Format/Registry/RegistryFormat.hpp"
#include "Format/Scene/SceneFormat.hpp"
#include "Format/Script/ScriptFormat.hpp"
#include "Format/Texture2D/Texture2DFormat.hpp"
#include "Format/UILayout/UILayoutFormat.hpp"

#include "Surge/Core/Profiler.hpp"

#include <algorithm>
#include <cctype>

namespace Surge
{
    static const AssetMetadata sNullMetadata {};

    AssetManager::AssetManager()
    {
        Scope<AssetFormat> formats[] = { CreateScope<SceneFormat>(), CreateScope<Texture2DFormat>(), CreateScope<MeshFormat>(), CreateScope<MaterialFormat>(),
                                         CreateScope<ScriptFormat>(), CreateScope<FontFormat>(), CreateScope<AudioFormat>(), CreateScope<UILayoutFormat>(),
                                         CreateScope<ParticleEffectFormat>() };
        for(Scope<AssetFormat>& format : formats)
        {
            const AssetType type = format->GetType();
            mFormats[type] = std::move(format);
        }
    }

    AssetManager::~AssetManager() = default;

    void AssetManager::Initialize(const Path& assetDirectory, const AssetContext& context)
    {
        SG_ASSERT(context.AssetManager_ == this, "[AssetManager] The context must point back to this AssetManager!");
        mContext = context;
        mAssetsDirectory = assetDirectory.generic_string();
        LoadCookedRegistry();
        Log<Severity::Info>("[AssetManager] Initialized. Directory: '{}' // {} asset(s) in registry", mAssetsDirectory, mAssetRegistry.size());
        mInitialized = true;
    }

    void AssetManager::Shutdown()
    {
        if(!mInitialized)
            return;

        Clear();
        Log<Severity::Info>("[AssetManager] Shutdown");
        mInitialized = false;
    }

    void AssetManager::LoadCookedRegistry()
    {
        // Exported games ship it (the export writes it), the Editor registers the project from its text registry instead
        Vector<AssetMetadata> entries;
        if(!RegistryBinary::Read(Path(mAssetsDirectory) / RegistryBinary::FILE_NAME, entries))
            return;

        mAssetRegistry.reserve(entries.size());
        mPathToID.reserve(entries.size());
        for(AssetMetadata& entry : entries)
        {
            // The export only lists assets whose cooked file it shipped: nothing to check on disk
            entry.Flags = entry.RelativePath.starts_with(SURGE_MEMORY_ASSET_PREFIX) ? AssetFlags::VALID | AssetFlags::MEMORY : AssetFlags::VALID;
            Register(entry);
        }
    }

    // Load (template in header points here, do not call directly)
    Ref<Asset> AssetManager::LoadAssetInternal(AssetID id, AssetMetadata* meta)
    {
        const AssetFormat* format = GetFormat(meta->Type);
        if(!format)
        {
            Log<Severity::Error>("[AssetManager] Load: No format for type {}!", SurgeReflect::EnumToString(meta->Type).data());
            return nullptr;
        }

        Ref<Asset> asset;
        if(HasFlag(meta->Flags, AssetFlags::MEMORY))
            asset = format->CreateBuiltin(*meta, mContext);
        else
        {
            const String path = GetSidecarPath(id);
            CookedFile file;
            const CookedFileError error = CookedFileIO::Read(path, meta->Type, format->GetFormatVersion(), file);
            if(error == CookedFileError::NONE)
                asset = format->Read(file, *meta, mContext);
            else
                Log<Severity::Error>("[AssetManager] Can't load {} ({}): {}. Re-cook it in the Editor (or export the game again)", meta->RelativePath, path, CookedFileErrorToString(error));
        }

        if(!asset)
        {
            meta->Flags |= AssetFlags::MISSING;
            meta->Flags &= ~AssetFlags::LOADED;
            return nullptr;
        }

        asset->mID = id;
        meta->Flags |= AssetFlags::LOADED;
        meta->Flags &= ~AssetFlags::MISSING;

        mLoadedAssets[id] = asset;
        return asset;
    }

    AssetID AssetManager::GetBuiltin(const String& name, AssetType type)
    {
        if(!name.starts_with(SURGE_MEMORY_ASSET_PREFIX))
            return AssetID::INVALID;

        const AssetID existing = GetIDFromPath(name);
        if(existing.IsValid())
            return existing;

        AssetMetadata meta;
        meta.ID = AssetID();
        meta.Type = type;
        meta.Flags = AssetFlags::VALID | AssetFlags::MEMORY;
        meta.RelativePath = name;
        Register(meta);
        return meta.ID;
    }

    AssetMetadata* AssetManager::GetMetadataForEdit(AssetID id)
    {
        auto it = mAssetRegistry.find(id);
        return it != mAssetRegistry.end() ? &it->second : nullptr;
    }

    const AssetFormat* AssetManager::GetFormat(AssetType type) const
    {
        auto it = mFormats.find(type);
        return it != mFormats.end() ? it->second.get() : nullptr;
    }

    bool AssetManager::Unload(AssetID id)
    {
        auto cacheIt = mLoadedAssets.find(id);
        if(cacheIt == mLoadedAssets.end())
            return false;

        // Cannot unload if there are external Refs still alive
        if(cacheIt->second->GetRefCount() > 1)
            return false;

        mLoadedAssets.erase(cacheIt);
        if(AssetMetadata* meta = GetMetadataForEdit(id))
            meta->Flags &= ~AssetFlags::LOADED;
        return true;
    }

    Uint AssetManager::UnloadUnreferenced()
    {
        SURGE_PROFILE_FUNC("AssetManager::UnloadUnreferenced");
        Uint total = 0;
        Uint unloaded = 0;
        do
        {
            // Erasing destroys the asset, which only lowers the Ref count of the assets it held: they are picked up by the next pass
            unloaded = 0;
            for(auto it = mLoadedAssets.begin(); it != mLoadedAssets.end();)
            {
                if(it->second->GetRefCount() > 1)
                {
                    ++it;
                    continue;
                }

                if(AssetMetadata* meta = GetMetadataForEdit(it->first))
                    meta->Flags &= ~AssetFlags::LOADED;

                it = mLoadedAssets.erase(it);
                unloaded++;
            }
            total += unloaded;

        } while(unloaded > 0);

        if(total > 0)
            Log<Severity::Info>("[AssetManager] Unloaded {} unreferenced asset(s), {} still loaded", total, mLoadedAssets.size());
        return total;
    }

    bool AssetManager::IsLoaded(AssetID id) const
    {
        return mLoadedAssets.find(id) != mLoadedAssets.end();
    }

    bool AssetManager::IsRegistered(AssetID id) const
    {
        return mAssetRegistry.find(id) != mAssetRegistry.end();
    }

    const AssetMetadata& AssetManager::GetMetadata(AssetID id) const
    {
        auto it = mAssetRegistry.find(id);
        return it != mAssetRegistry.end() ? it->second : sNullMetadata;
    }

    String AssetManager::GetSidecarPath(AssetID id, AssetType type) const
    {
        const AssetType t = type != AssetType::NONE ? type : GetMetadata(id).Type;
        return std::format("{0}/{1}.r{2}", GetSidecarDirectory(t), id.Get(), SurgeReflect::EnumToString(t).data());
    }

    String AssetManager::GetSidecarDirectory(AssetType type) const
    {
        return mAssetsDirectory + "/Internal/" + SurgeReflect::EnumToString(type).data();
    }

    AssetID AssetManager::GetIDFromPath(const String& relativePath) const
    {
        auto it = mPathToID.find(relativePath);
        if(it != mPathToID.end())
            return it->second;

#ifdef SURGE_PLATFORM_WINDOWS
        // Windows paths ignore case, the APK does not: a path that only matches with another case works on desktop but not on device.
        // Only misses pay for this scan
        for(const auto& [path, id] : mPathToID)
        {
            if(path.size() == relativePath.size() && std::equal(path.begin(), path.end(), relativePath.begin(), [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); }))
            {
                Log<Severity::Warn>("[AssetManager] '{}' is not registered, but '{}' is: paths are case sensitive on Android", relativePath, path);
                break;
            }
        }
#endif
        return UUID::INVALID;
    }

    bool AssetManager::Register(const AssetMetadata& metadata)
    {
        if(!metadata.ID.IsValid() || metadata.Type == AssetType::NONE || metadata.RelativePath.empty())
            return false;

        if(mAssetRegistry.contains(metadata.ID))
        {
            Log<Severity::Warn>("[AssetManager] Register: ID {} is registered already ({})", metadata.ID.Get(), mAssetRegistry[metadata.ID].RelativePath);
            return false;
        }
        if(!mPathToID.try_emplace(metadata.RelativePath, metadata.ID).second)
        {
            Log<Severity::Warn>("[AssetManager] Register: '{}' is registered twice, path lookups resolve to {}", metadata.RelativePath, mPathToID[metadata.RelativePath].Get());
            return false;
        }

        mAssetRegistry[metadata.ID] = metadata;
        return true;
    }

    void AssetManager::Unregister(AssetID id)
    {
        auto it = mAssetRegistry.find(id);
        if(it == mAssetRegistry.end())
            return;

        auto pathIt = mPathToID.find(it->second.RelativePath);
        if(pathIt != mPathToID.end() && pathIt->second == id)
            mPathToID.erase(pathIt);

        Unload(id); // Only succeeds when nothing references it anymore
        mAssetRegistry.erase(it);
    }

    bool AssetManager::SetPath(AssetID id, const String& newRelativePath)
    {
        auto it = mAssetRegistry.find(id);
        if(it == mAssetRegistry.end())
            return false;
        if(it->second.RelativePath == newRelativePath)
            return true;

        auto takenIt = mPathToID.find(newRelativePath);
        if(takenIt != mPathToID.end() && takenIt->second != id)
        {
            Log<Severity::Error>("[AssetManager] SetPath: '{}' already belongs to asset {}, {} keeps '{}'", newRelativePath, takenIt->second.Get(), id.Get(), it->second.RelativePath);
            return false;
        }

        auto oldIt = mPathToID.find(it->second.RelativePath);
        if(oldIt != mPathToID.end() && oldIt->second == id)
            mPathToID.erase(oldIt);

        it->second.RelativePath = newRelativePath;
        mPathToID[newRelativePath] = id;
        return true;
    }

    void AssetManager::Adopt(AssetID id, Ref<Asset> asset)
    {
        AssetMetadata* meta = GetMetadataForEdit(id);
        SG_ASSERT(meta && asset && asset->GetAssetType() == meta->Type, "[AssetManager] Adopt: the asset must be registered with its type");
        if(!meta || !asset)
            return;

        asset->mID = id;
        mLoadedAssets[id] = asset;
        meta->Flags |= AssetFlags::LOADED;
        meta->Flags &= ~AssetFlags::MISSING;
    }

    void AssetManager::Invalidate(AssetID id)
    {
        if(mLoadedAssets.erase(id) > 0)
        {
            if(AssetMetadata* meta = GetMetadataForEdit(id))
                meta->Flags &= ~AssetFlags::LOADED;
        }
        if(AssetMetadata* meta = GetMetadataForEdit(id))
            meta->Flags &= ~AssetFlags::MISSING;
    }

    void AssetManager::Clear()
    {
        mLoadedAssets.clear();
        mAssetRegistry.clear();
        mPathToID.clear();
    }

} // namespace Surge
