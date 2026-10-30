// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/UUID.hpp"
#include "Surge/Core/Memory.hpp"
#include <cstring>
#include <array>

#define SURGE_MEMORY_ASSET_PREFIX "Engine://"

#define SURGE_ASSET_TYPE(AssetEnum)                                                \
public:                                                                            \
    static  ::Surge::AssetType GetStaticType() { return AssetEnum; }               \
    virtual ::Surge::AssetType GetAssetType() const override { return AssetEnum; } 

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
        FONT,
        AUDIO,
        UI_LAYOUT,
        PARTICLE_EFFECT
    };

    inline constexpr auto sAssetTypeArray = std::array {
        AssetType::TEXTURE2D,
        AssetType::SPRITE,
        AssetType::MESH,
        AssetType::MATERIAL,
        AssetType::SCENE,
        AssetType::SCRIPT,
        AssetType::FONT,
        AssetType::AUDIO,
        AssetType::UI_LAYOUT,
        AssetType::PARTICLE_EFFECT
    };

    inline AssetType AssetTypeFromString(const char* str)
    {
        if(strcmp(str, "TEXTURE2D") == 0) return AssetType::TEXTURE2D;
        if(strcmp(str, "SPRITE") == 0) return AssetType::SPRITE;
        if(strcmp(str, "MESH") == 0) return AssetType::MESH;
        if(strcmp(str, "MATERIAL") == 0) return AssetType::MATERIAL;
        if(strcmp(str, "SCENE") == 0) return AssetType::SCENE;
        if(strcmp(str, "SCRIPT") == 0) return AssetType::SCRIPT;
        if(strcmp(str, "FONT") == 0) return AssetType::FONT;
        if(strcmp(str, "AUDIO") == 0) return AssetType::AUDIO;
        if(strcmp(str, "UI_LAYOUT") == 0) return AssetType::UI_LAYOUT;
        if(strcmp(str, "PARTICLE_EFFECT") == 0) return AssetType::PARTICLE_EFFECT;
        return AssetType::NONE;
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

} // namespace Surge