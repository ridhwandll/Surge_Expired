// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/UUID.hpp"
#include "Surge/Core/Memory.hpp"
#include <cstring>

#define SURGE_MEMORY_ASSET_PREFIX "Engine://"

namespace Surge
{
    using AssetID = UUID;

    enum class AssetType : uint8_t
    {
        NONE = 0,
        TEXTURE2D,
        SPRITE,
        MESH,
        SCENE,
    };

    inline AssetType AssetTypeFromString(const char* str)
    {
        if(strcmp(str, "TEXTURE2D") == 0) return AssetType::TEXTURE2D;
        if(strcmp(str, "SPRITE") == 0) return AssetType::SPRITE;
        if(strcmp(str, "MESH") == 0) return AssetType::MESH;
        if(strcmp(str, "SCENE") == 0) return AssetType::SCENE;
        return AssetType::NONE;
    }

    // -------------------------------------------------------------------------
    // Asset: base class for all engine assets
    //
    // Concrete types (Texture2D, Mesh, ..) derive from this and implement GetAssetType(). The AssetID is stamped by AssetManager after load;
    // -------------------------------------------------------------------------
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