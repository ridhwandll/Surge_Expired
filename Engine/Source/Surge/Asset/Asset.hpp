// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/UUID.hpp"
#include "Surge/Core/Memory.hpp"
#include <cstring>
#include <array>

#define SURGE_MEMORY_ASSET_PREFIX "Engine://"

namespace Surge
{
    using AssetID = UUID;

    // Register all asset types here
    enum class AssetType : uint8_t
    {
        NONE = 0,
        TEXTURE2D,
        SPRITE,
        MESH,
        MATERIAL,
        SCENE,
        SCRIPT,
    };

    inline constexpr auto sAssetTypeArray = std::array {
        AssetType::TEXTURE2D,
        AssetType::SPRITE,
        AssetType::MESH,
        AssetType::MATERIAL,
        AssetType::SCENE,
        AssetType::SCRIPT
    };

    inline AssetType AssetTypeFromString(const char* str)
    {
        if(strcmp(str, "TEXTURE2D") == 0) return AssetType::TEXTURE2D;
        if(strcmp(str, "SPRITE") == 0) return AssetType::SPRITE;
        if(strcmp(str, "MESH") == 0) return AssetType::MESH;
        if(strcmp(str, "MATERIAL") == 0) return AssetType::MATERIAL;
        if(strcmp(str, "SCENE") == 0) return AssetType::SCENE;
        if(strcmp(str, "SCRIPT") == 0) return AssetType::SCRIPT;
        return AssetType::NONE;
    }

    inline AssetType AssetTypeFromExtension(const char* str)
    {
        if(strcmp(str, ".png") == 0 || strcmp(str, ".jpg") == 0 || strcmp(str, ".jpeg") == 0) return AssetType::TEXTURE2D;
        if(strcmp(str, ".glb") == 0 || strcmp(str, ".gltf") == 0) return AssetType::MESH;
        if(strcmp(str, ".smat") == 0) return AssetType::MATERIAL;
        if(strcmp(str, ".srg") == 0) return AssetType::SCENE;
        if(strcmp(str, ".lua") == 0) return AssetType::SCRIPT;
        return AssetType::NONE;
    }

    inline const char* GetExtensionFromAssetType(AssetType type)
    {
        switch(type)
        {
            case AssetType::MATERIAL: return ".smat";
            case AssetType::SCENE: return ".srg";
            case AssetType::SCRIPT: return ".lua";
            default:
                SG_ASSERT_INTERNAL("GetExtensionFromAssetType: Invalid asset type");
                return "";
        }
    }

    // Asset
    // Base class for all engine assets
    // Concrete types (Texture2D, Mesh, Material, Scene ...) SURGE_ASSET_TYPE macro. The AssetID is stamped by AssetManager after load;
    class Asset : public RefCounted
    {
    public:
        virtual ~Asset() = default;
        virtual AssetType GetAssetType() const = 0;

        AssetID GetID()   const { return mID; }
        bool IsValid() const { return mID.IsValid(); }

    protected:
        // Starts with an invalid ID; AssetManager stamps the real one after load.
        Asset()
            : mID(UUID::INVALID) {}

    private:
        AssetID mID;
        friend class AssetManager;
    };

#define SURGE_ASSET_TYPE(AssetEnum)                                      \
public:                                                                  \
    static AssetType GetStaticType() { return AssetEnum; }               \
    virtual AssetType GetAssetType() const override { return AssetEnum; }

} // namespace Surge