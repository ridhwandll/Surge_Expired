// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Core/Scope.hpp"
#include "Asset/Cookers/AssetCooker.hpp"
#include "Asset/ImportSettings.hpp"
#include <unordered_map>

namespace Surge
{
    // The authoring side of the asset pipeline (Editor only): the project's sources, the text registry (Assets/AssetRegistry.surge),
    // import settings, cooking and saving. It's the only code that writes into a project's Assets folder. The engine's AssetManager
    // only reads cooked files: this keeps its in-memory registry in sync (Register/SetPath/Unregister) and its cache fresh (Invalidate,
    // the cook hook installed by Initialize)
    class AssetDatabase
    {
    public:
        static AssetDatabase& Get();

        void Initialize(AssetManager* assetManager); // Registers the cookers, installs the cook hook
        void Shutdown();                             // Writes the text registry of the open project

        // Points the AssetManager at a project's Assets folder: text registry -> AssetManager::Register
        void OpenProject(const Path& assetsDirectory);

        String GetAbsolutePath(const String& relativePath) const;
        AssetManager* GetAssetManager() const { return mAssetManager; }

        // Text registry (AssetRegistry.surge), written after every change by the functions below unless saveRegistry is false (batches)
        void SaveRegistry() const;
        // Cooked registry of every registered asset (exported games read it instead of the text one)
        bool WriteCookedRegistry(const Path& path) const;

        // Import
        // Registers a source file (relative to the Assets folder) or a built-in "Engine://" asset. Returns the existing ID if already imported
        // @return  Stable AssetID, or UUID::INVALID on failure
        AssetID Import(const String& relativePath, AssetType type, bool saveRegistry = true);

        // Create
        // Instantiates a new asset of type T at the given path, saves its source and cooked file, and registers it
        template<IsAssetConcept T, typename... Args>
        Ref<T> Create(const String& relativePath, Args&&... args)
        {
            if(mAssetManager->GetIDFromPath(relativePath).IsValid())
            {
                Log<Severity::Warn>("[AssetDatabase] Create: Asset already exists at path '{}'", relativePath);
                return nullptr;
            }

            AssetMetadata meta;
            meta.ID = AssetID();
            meta.Type = T::GetStaticType();
            meta.RelativePath = relativePath;
            meta.Flags = AssetFlags::VALID;
            if(!mAssetManager->Register(meta))
                return nullptr;

            Ref<T> asset = T::Create(std::forward<Args>(args)...);
            mAssetManager->Adopt(meta.ID, asset);
            Save(meta.ID);
            SaveRegistry();

            Log<Severity::Info>("[AssetDatabase] Created & Saved new asset: {}", relativePath);
            return asset;
        }

        // Save
        // Writes a loaded asset's source (or its import settings) and its cooked file with a fresh header. Scene, ParticleEffect, UILayout and
        // Material write their source + the cooked file from memory, Mesh writes its material overrides to its .import and re-cooks
        // @return  false if the asset is not loaded, can't be saved (Texture, Audio, Font, Script) or a write failed
        bool Save(AssetID id);

        // SetPath
        // Its source was renamed/moved. Scripts are re-cooked (their chunk name is their path)
        bool SetPath(AssetID id, const String& newRelativePath, bool saveRegistry = true);
        void Unregister(AssetID id, bool saveRegistry = true);

        // Cooking
        void RegisterCooker(Scope<AssetCooker> cooker);
        bool IsCookable(AssetType type) const { return mCookers.contains(type); }
        bool NeedsCook(AssetID id);
        CookResult Cook(AssetID id); // Then the next Load reads the new file (AssetManager::Invalidate)
        void CookAllOutdated();      // Before an export

        // Import settings of a registered source (defaults when it has no .import)
        ImportSettings GetImportSettings(AssetID id) const;
        bool SetImportSettings(AssetID id, const ImportSettings& settings); // Re-cooks the asset

        // Called by Load through the cook hook: cooks a stale asset, true when a cached one must be read again
        bool EnsureCooked(AssetID id, const AssetMetadata& metadata, bool cached);

        // Write time (seconds) of a file, 0 when it doesn't exist
        static uint64_t GetFileModTime(const Path& path);

    private:
        const AssetCooker* GetCooker(AssetType type) const;
        CookedHeader BuildHeader(const AssetMetadata& metadata, const AssetCooker* cooker) const;
        uint64_t GetInputModTime(const AssetMetadata& metadata) const;
        uint64_t HashInputs(const AssetMetadata& metadata, const AssetCooker* cooker) const;
        CookResult CookInternal(AssetID id);
        bool WriteCookedFromMemory(AssetID id, const AssetMetadata& metadata, const Asset& asset);

        void LoadRegistry();

    private:
        AssetManager* mAssetManager = nullptr;
        std::unordered_map<AssetType, Scope<AssetCooker>> mCookers;
        std::unordered_map<AssetID, uint64_t> mVerifiedModTimes; // Input write time when the cooked file was last known fresh, the cheap check of cached loads
        bool mProjectOpen = false;
    };
}
