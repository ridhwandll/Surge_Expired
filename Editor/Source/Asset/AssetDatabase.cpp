// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Asset/AssetDatabase.hpp"
#include "Asset/Cookers/AudioCooker.hpp"
#include "Asset/Cookers/FontCooker.hpp"
#include "Asset/Cookers/MaterialCooker.hpp"
#include "Asset/Cookers/MeshCooker.hpp"
#include "Asset/Cookers/ParticleEffectCooker.hpp"
#include "Asset/Cookers/SceneCooker.hpp"
#include "Asset/Cookers/ScriptCooker.hpp"
#include "Asset/Cookers/Texture2DCooker.hpp"
#include "Asset/Cookers/UILayoutCooker.hpp"
#include "Asset/SourceIO/MaterialSourceWriter.hpp"
#include "Asset/SourceIO/ParticleEffectSourceIO.hpp"
#include "Asset/SourceIO/SceneSourceIO.hpp"
#include "Asset/SourceIO/UILayoutSourceIO.hpp"

#include "Surge/Asset/Format/BinarySerialization.hpp"
#include "Surge/Asset/Format/Registry/RegistryFormat.hpp"
#include "Surge/ECS/Scene.hpp"
#include "Surge/Graphics/HighLevel/Mesh.hpp"
#include "Surge/Graphics/Particles/ParticleEffect.hpp"
#include "Surge/Graphics/UISystem/UILayout.hpp"
#include "Surge/Utility/Filesystem.hpp"

#include <chrono>
#include <fstream>
#include <sstream>

namespace Surge
{
    static constexpr const char* REGISTRY_FILENAME = "AssetRegistry.surge";
    static constexpr const char* REGISTRY_DELIMITER = "|";

    class EditorCookHook final : public AssetCookHook
    {
    public:
        virtual bool OnLoad(AssetID id, const AssetMetadata& metadata, bool cached) override
        {
            return AssetDatabase::Get().EnsureCooked(id, metadata, cached);
        }
    };

    AssetDatabase& AssetDatabase::Get()
    {
        static AssetDatabase sInstance;
        return sInstance;
    }

    void AssetDatabase::Initialize(AssetManager* assetManager)
    {
        mAssetManager = assetManager;
        RegisterCooker(CreateScope<Texture2DCooker>());
        RegisterCooker(CreateScope<MaterialCooker>());
        RegisterCooker(CreateScope<MeshCooker>());
        RegisterCooker(CreateScope<ScriptCooker>());
        RegisterCooker(CreateScope<FontCooker>());
        RegisterCooker(CreateScope<AudioCooker>());
        RegisterCooker(CreateScope<SceneCooker>());
        RegisterCooker(CreateScope<ParticleEffectCooker>());
        RegisterCooker(CreateScope<UILayoutCooker>());
        mAssetManager->SetCookHook(CreateScope<EditorCookHook>());
    }

    void AssetDatabase::Shutdown()
    {
        if(mProjectOpen)
            SaveRegistry();
        mProjectOpen = false;
        mCookers.clear();
        mVerifiedModTimes.clear();
    }

    void AssetDatabase::OpenProject(const Path& assetsDirectory)
    {
        const AssetContext context = mAssetManager->GetContext(); // Set up by Core, stays the same across projects
        if(mProjectOpen)
            SaveRegistry();

        mAssetManager->Shutdown();
        mAssetManager->Initialize(assetsDirectory, context);
        mVerifiedModTimes.clear();
        LoadRegistry();
        mProjectOpen = true;
    }

    String AssetDatabase::GetAbsolutePath(const String& relativePath) const
    {
        return mAssetManager->GetAssetsDirectory() + '/' + relativePath;
    }

    //////////////////////////////////////////////////////////////////////////
    // Registry
    //////////////////////////////////////////////////////////////////////////

    // Format (AssetRegistry.surge):
    //    comment lines start with //
    //    <UUID>|<TypeString>|<Relative/path/to/asset.ext OR MemoryString>
    void AssetDatabase::SaveRegistry() const
    {
        const String registryPath = GetAbsolutePath(REGISTRY_FILENAME);
        std::ofstream file(registryPath, std::ios::out | std::ios::trunc);
        if(!file.is_open())
        {
            Log<Severity::Error>("[AssetDatabase] SaveRegistry: Failed to open '{}'.", registryPath);
            return;
        }

        file << "// Surge Asset Registry v1\n";
        file << "// Format: UUID|Type|RelativePath\n";
        for(const auto& [id, meta] : mAssetManager->GetRegistryMap())
            file << id.Get() << REGISTRY_DELIMITER << SurgeReflect::EnumToString(meta.Type).data() << REGISTRY_DELIMITER << meta.RelativePath << '\n';

        Log<Severity::Info>("[AssetDatabase] Registry saved ({} entries) -> '{}'.", mAssetManager->GetRegistryMap().size(), registryPath);
    }

    void AssetDatabase::LoadRegistry()
    {
        const String registryPath = GetAbsolutePath(REGISTRY_FILENAME);
        String fileContents;
        if(!Filesystem::ReadTextFile(registryPath, fileContents) || fileContents.empty())
        {
            Log<Severity::Warn>("[AssetDatabase] No asset registry at {} (a new project?)", registryPath);
            return;
        }

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
            const size_t d1 = line.find(REGISTRY_DELIMITER);
            const size_t d2 = line.find(REGISTRY_DELIMITER, d1 + 1);
            if(d1 == String::npos || d2 == String::npos)
            {
                Log<Severity::Warn>("[AssetDatabase] LoadRegistry: Malformed line skipped!");
                continue;
            }

            const String uuidStr = line.substr(0, d1);
            char* endPtr = nullptr;
            errno = 0;
            const uint64_t rawID = strtoull(uuidStr.c_str(), &endPtr, 10);
            if(endPtr == uuidStr.c_str() || errno == ERANGE || rawID == 0)
                continue;

            AssetMetadata meta;
            meta.ID = AssetID(rawID);
            meta.Type = AssetTypeFromString(line.substr(d1 + 1, d2 - d1 - 1).c_str());
            meta.RelativePath = line.substr(d2 + 1);
            if(meta.Type == AssetType::NONE || meta.RelativePath.empty())
                continue;

            if(meta.RelativePath.starts_with(SURGE_MEMORY_ASSET_PREFIX))
                meta.Flags = AssetFlags::VALID | AssetFlags::MEMORY;
            else
            {
                // Without its source the cooked file can still load, until something needs a re-cook
                const bool exists = Filesystem::Exists(GetAbsolutePath(meta.RelativePath)) || Filesystem::Exists(mAssetManager->GetSidecarPath(meta.ID, meta.Type));
                meta.Flags = exists ? AssetFlags::VALID : AssetFlags::MISSING;
                if(!exists)
                    Log<Severity::Warn>("[AssetDatabase] LoadRegistry: Asset Source & Sidecar is missing for {}", meta.RelativePath);
            }

            if(mAssetManager->Register(meta))
                count++;
        }
        Log<Severity::Info>("[AssetDatabase] Registry loaded ({} entries).", count);
    }

    bool AssetDatabase::WriteCookedRegistry(const Path& path) const
    {
        return RegistryBinary::Write(path, mAssetManager->GetRegistryMap());
    }

    //////////////////////////////////////////////////////////////////////////
    // Authoring
    //////////////////////////////////////////////////////////////////////////

    AssetID AssetDatabase::Import(const String& relativePath, AssetType type, bool saveRegistry)
    {
        SG_ASSERT(type != AssetType::NONE, "[AssetDatabase] Cannot import with AssetType::NONE");
        if(relativePath.empty())
            return UUID::INVALID;

        // Built-ins are registered like files: scenes store their ID
        if(relativePath.starts_with(SURGE_MEMORY_ASSET_PREFIX))
        {
            const bool isNew = !mAssetManager->GetIDFromPath(relativePath).IsValid();
            const AssetID id = mAssetManager->GetBuiltin(relativePath, type);
            if(isNew && id && saveRegistry && mProjectOpen)
                SaveRegistry();
            return id;
        }

        const AssetID existing = mAssetManager->GetIDFromPath(relativePath);
        if(existing.IsValid())
            return existing;

        const String absPath = GetAbsolutePath(relativePath);
        if(!Filesystem::Exists(absPath))
        {
            Log<Severity::Warn>("[AssetDatabase] Import failed: file not found: {}!", absPath);
            return UUID::INVALID;
        }

        AssetMetadata meta;
        meta.ID = AssetID();
        meta.Type = type;
        meta.Flags = AssetFlags::VALID;
        meta.RelativePath = relativePath;
        if(!mAssetManager->Register(meta))
            return UUID::INVALID;

        if(saveRegistry)
            SaveRegistry();
        Log<Severity::Info>("[AssetDatabase] Imported '{}' | ID: {} | Type: {}", relativePath, meta.ID.Get(), SurgeReflect::EnumToString(type).data());
        return meta.ID;
    }

    bool AssetDatabase::Save(AssetID id)
    {
        const AssetMetadata metadata = mAssetManager->GetMetadata(id);
        if(!metadata.IsValid() || !mAssetManager->IsLoaded(id))
        {
            Log<Severity::Error>("[AssetDatabase] Save: AssetID {} is not loaded!", id.Get());
            return false;
        }

        const Path sourcePath = GetAbsolutePath(metadata.RelativePath);
        switch(metadata.Type)
        {
            case AssetType::SCENE:
            {
                Ref<Scene> scene = mAssetManager->GetLoaded<Scene>(id);
                return SceneSourceIO::Write(*scene, sourcePath) && WriteCookedFromMemory(id, metadata, *scene);
            }
            case AssetType::PARTICLE_EFFECT:
            {
                Ref<ParticleEffect> effect = mAssetManager->GetLoaded<ParticleEffect>(id);
                return ParticleEffectSourceIO::Write(*effect, sourcePath) && WriteCookedFromMemory(id, metadata, *effect);
            }
            case AssetType::UI_LAYOUT:
            {
                Ref<UILayout> layout = mAssetManager->GetLoaded<UILayout>(id);
                return UILayoutSourceIO::Write(*layout, sourcePath) && WriteCookedFromMemory(id, metadata, *layout);
            }
            case AssetType::MATERIAL:
            {
                Ref<Material> material = mAssetManager->GetLoaded<Material>(id);
                return MaterialSourceWriter::Write(material, sourcePath.generic_string()) && WriteCookedFromMemory(id, metadata, *material);
            }
            case AssetType::MESH:
            {
                // Overrides are the model's import settings: written there, then baked into the cooked mesh. The live mesh already shows them
                Ref<Mesh> mesh = mAssetManager->GetLoaded<Mesh>(id);
                ImportSettings settings = GetImportSettings(id);
                settings.MaterialOverrides.clear();
                const Vector<Ref<Material>>& overrides = mesh->GetMaterialOverrides();
                for(Uint slot = 0; slot < static_cast<Uint>(overrides.size()); slot++)
                {
                    if(overrides[slot] && overrides[slot]->GetID().IsValid())
                        settings.MaterialOverrides[slot] = overrides[slot]->GetID();
                }
                return ImportSettings::Save(sourcePath, settings) && CookInternal(id).Success;
            }
            default:
                Log<Severity::Warn>("[AssetDatabase] Save: {} assets are cooked from their source file only ({})", SurgeReflect::EnumToString(metadata.Type).data(), metadata.RelativePath);
                return false;
        }
    }

    bool AssetDatabase::WriteCookedFromMemory(AssetID id, const AssetMetadata& metadata, const Asset& asset)
    {
        const AssetFormat* format = mAssetManager->GetFormat(metadata.Type);
        Rapt0R::Writer payload(64 * 1024);
        if(!format || !format->Write(payload, asset) || payload.HasError())
        {
            Log<Severity::Error>("[AssetDatabase] Could not build the cooked data of {}", metadata.RelativePath);
            return false;
        }

        // The source was just written: the header's input time and hash match it, so the next Load doesn't re-cook
        const CookedHeader header = BuildHeader(metadata, GetCooker(metadata.Type));
        const String sidecarPath = mAssetManager->GetSidecarPath(id);
        if(!CookedFileIO::Write(sidecarPath, header, format->GetFormatVersion(), payload.WrittenSpan()))
        {
            Log<Severity::Error>("[AssetDatabase] Failed to write {}", sidecarPath);
            return false;
        }
        mVerifiedModTimes[id] = header.SourceModTime;
        return true;
    }

    bool AssetDatabase::SetPath(AssetID id, const String& newRelativePath, bool saveRegistry)
    {
        if(!mAssetManager->SetPath(id, newRelativePath))
            return false;

        mVerifiedModTimes.erase(id);
        if(mAssetManager->GetMetadata(id).Type == AssetType::SCRIPT)
            Cook(id);
        if(saveRegistry)
            SaveRegistry();
        return true;
    }

    void AssetDatabase::Unregister(AssetID id, bool saveRegistry)
    {
        mAssetManager->Unregister(id);
        mVerifiedModTimes.erase(id);
        if(saveRegistry)
            SaveRegistry();
    }

    ImportSettings AssetDatabase::GetImportSettings(AssetID id) const
    {
        const AssetMetadata& metadata = mAssetManager->GetMetadata(id);
        if(!metadata.IsValid() || HasFlag(metadata.Flags, AssetFlags::MEMORY))
            return {};
        return ImportSettings::Load(GetAbsolutePath(metadata.RelativePath));
    }

    bool AssetDatabase::SetImportSettings(AssetID id, const ImportSettings& settings)
    {
        const AssetMetadata& metadata = mAssetManager->GetMetadata(id);
        if(!metadata.IsValid() || HasFlag(metadata.Flags, AssetFlags::MEMORY))
            return false;
        return ImportSettings::Save(GetAbsolutePath(metadata.RelativePath), settings) && Cook(id).Success;
    }

    //////////////////////////////////////////////////////////////////////////
    // Cooking
    //////////////////////////////////////////////////////////////////////////

    uint64_t AssetDatabase::GetFileModTime(const Path& path)
    {
        std::error_code ec;
        const auto time = std::filesystem::last_write_time(path, ec);
        if(ec)
            return 0;
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count());
    }

    static void HashBytes(uint64_t& hash, const Byte* data, size_t size) // FNV-1a
    {
        for(size_t i = 0; i < size; i++)
            hash = (hash ^ data[i]) * 1099511628211ull;
    }

    void AssetDatabase::RegisterCooker(Scope<AssetCooker> cooker)
    {
        const AssetType type = cooker->GetAssetType();
        mCookers[type] = std::move(cooker);
    }

    const AssetCooker* AssetDatabase::GetCooker(AssetType type) const
    {
        auto it = mCookers.find(type);
        return it != mCookers.end() ? it->second.get() : nullptr;
    }

    uint64_t AssetDatabase::GetInputModTime(const AssetMetadata& metadata) const
    {
        const Path sourcePath = GetAbsolutePath(metadata.RelativePath);
        return std::max(GetFileModTime(sourcePath), GetFileModTime(ImportSettings::GetPath(sourcePath)));
    }

    uint64_t AssetDatabase::HashInputs(const AssetMetadata& metadata, const AssetCooker* cooker) const
    {
        uint64_t hash = 14695981039346656037ull;
        const Path sourcePath = GetAbsolutePath(metadata.RelativePath);
        Vector<Byte> bytes;
        if(Filesystem::ReadBinaryFile(sourcePath, bytes))
            HashBytes(hash, bytes.data(), bytes.size());

        const Path importPath = ImportSettings::GetPath(sourcePath);
        if(Filesystem::Exists(importPath) && Filesystem::ReadBinaryFile(importPath, bytes))
            HashBytes(hash, bytes.data(), bytes.size());

        if(cooker)
        {
            const String extras = cooker->GetExtraInputs(metadata);
            HashBytes(hash, reinterpret_cast<const Byte*>(extras.data()), extras.size());
        }
        return hash;
    }

    CookedHeader AssetDatabase::BuildHeader(const AssetMetadata& metadata, const AssetCooker* cooker) const
    {
        CookedHeader header;
        header.Type = metadata.Type;
        header.CookerVersion = cooker ? cooker->GetCookerVersion() : 0;
        header.SourceModTime = GetInputModTime(metadata);
        header.InputHash = HashInputs(metadata, cooker);
        return header;
    }

    bool AssetDatabase::NeedsCook(AssetID id)
    {
        const AssetMetadata& metadata = mAssetManager->GetMetadata(id);
        const AssetCooker* cooker = GetCooker(metadata.Type);
        const AssetFormat* format = mAssetManager->GetFormat(metadata.Type);
        if(!metadata.IsValid() || HasFlag(metadata.Flags, AssetFlags::MEMORY) || !cooker || !format)
            return false;

        const String sidecarPath = mAssetManager->GetSidecarPath(id);
        CookedHeader header;
        if(!CookedFileIO::ReadHeader(sidecarPath, header) || header.Type != metadata.Type || header.FormatVersion != format->GetFormatVersion() ||
           header.CookerVersion != cooker->GetCookerVersion())
            return true;

        // Fast path: no input was written since the cook
        const uint64_t modTime = GetInputModTime(metadata);
        if(modTime == header.SourceModTime)
            return false;

        // New write times, same content (a fresh clone, a branch switch): only the header's time is updated
        if(HashInputs(metadata, cooker) != header.InputHash)
        {
            Log<Severity::Debug>("[AssetDatabase] Asset {} needs cook, source modified!", metadata.RelativePath);
            return true;
        }

        header.SourceModTime = modTime;
        std::fstream file(sidecarPath, std::ios::in | std::ios::out | std::ios::binary);
        if(file.is_open())
            file.write(reinterpret_cast<const char*>(&header), sizeof(CookedHeader));
        return false;
    }

    CookResult AssetDatabase::CookInternal(AssetID id)
    {
        const AssetMetadata metadata = mAssetManager->GetMetadata(id);
        const AssetCooker* cooker = GetCooker(metadata.Type);
        CookResult result;
        if(!metadata.IsValid() || HasFlag(metadata.Flags, AssetFlags::MEMORY) || !cooker)
            return result;

        CookContext context;
        context.ID = id;
        context.Metadata = metadata;
        context.SourcePath = GetAbsolutePath(metadata.RelativePath);
        context.OutputPath = mAssetManager->GetSidecarPath(id);
        context.Settings = ImportSettings::Load(context.SourcePath);
        context.Header = BuildHeader(metadata, cooker);

        result.OutputPath = context.OutputPath.generic_string();
        Filesystem::CreateOrEnsureDirectories(context.OutputPath.parent_path()); // Cookers write temporary files next to their output
        result.Success = cooker->Cook(context);
        if(!result.Success)
        {
            Log<Severity::Error>("[AssetDatabase] Cook failed: '{}'", metadata.RelativePath);
            return result;
        }

        result.InputMegaBytes = Filesystem::FileSizeInMB(context.SourcePath);
        result.OutputMegaBytes = Filesystem::FileSizeInMB(context.OutputPath);
        mVerifiedModTimes[id] = context.Header.SourceModTime;
        return result;
    }

    CookResult AssetDatabase::Cook(AssetID id)
    {
        const CookResult result = CookInternal(id);
        if(result.Success)
            mAssetManager->Invalidate(id);
        return result;
    }

    void AssetDatabase::CookAllOutdated()
    {
        Vector<AssetID> ids;
        ids.reserve(mAssetManager->GetRegistryMap().size());
        for(const auto& [id, meta] : mAssetManager->GetRegistryMap())
            ids.push_back(id); // Cooking a mesh can register its textures, the map must not be walked while it happens

        Uint cooked = 0, skipped = 0, failed = 0;
        for(AssetID id : ids)
        {
            if(!NeedsCook(id))
            {
                skipped++;
                continue;
            }
            Cook(id).Success ? cooked++ : failed++;
        }
        Log<Severity::Info>("[AssetDatabase] Cooked: {} | Already up to date: {} | Failed: {}", cooked, skipped, failed);
    }

    bool AssetDatabase::EnsureCooked(AssetID id, const AssetMetadata& metadata, bool cached)
    {
        if(HasFlag(metadata.Flags, AssetFlags::MEMORY) || !GetCooker(metadata.Type))
            return false;

        // Cached and verified: one write time check of the source and its .import
        if(cached)
        {
            auto it = mVerifiedModTimes.find(id);
            if(it != mVerifiedModTimes.end() && it->second == GetInputModTime(metadata))
                return false;
        }

        if(!NeedsCook(id))
        {
            mVerifiedModTimes[id] = GetInputModTime(metadata);
            return false;
        }

        Log<Severity::Trace>("[AssetDatabase] Cooked file outdated/missing on Load, cooking now: {}", metadata.RelativePath);
        CookInternal(id);
        return true; // Read the asset from disk again after cooking
    }
}
