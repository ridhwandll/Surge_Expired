// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"

namespace Surge
{
    enum class AssetFlag : uint16_t
    {
        NONE = 0,
        VALID   = BIT(0),
        MISSING = BIT(1),
        INVALID = BIT(2)
    };
    MAKE_BIT_ENUM(AssetFlag, uint16_t)

    enum class AssetType
    {
        NONE,
        SCENE,
        TEXTURE2D,
        MESH,
        ENVIRONMENT_MAP,
        MATERIAL,
        PHYSICS_MATERIAL
    };

    namespace Utils
    {
        inline AssetType AssetTypeFromString(const std::string& assetType)
        {
            if (assetType == "NONE")             return AssetType::NONE;
            if (assetType == "SCENE")            return AssetType::SCENE;
            if (assetType == "TEXTURE2D")        return AssetType::TEXTURE2D;
            if (assetType == "MESH")             return AssetType::MESH;
            if (assetType == "ENVIRONMENT_MAP")  return AssetType::ENVIRONMENT_MAP;
            if (assetType == "MATERIAL")         return AssetType::MATERIAL;
            if (assetType == "PHYSICS_MATERIAL") return AssetType::PHYSICS_MATERIAL;

            SG_ASSERT_INTERNAL("Unknown Asset Type");
            return AssetType::NONE;
        }

        inline const char* AssetTypeToString(AssetType assetType)
        {
            switch (assetType)
            {
                case AssetType::NONE:             return "NONE";
                case AssetType::SCENE:            return "SCENE";
                case AssetType::TEXTURE2D:        return "TEXTURE2D";
                case AssetType::MESH:             return "MESH";
                case AssetType::ENVIRONMENT_MAP:  return "ENVIRONMENT_MAP";
                case AssetType::MATERIAL:         return "MATERIAL";
                case AssetType::PHYSICS_MATERIAL: return "PHYSICS_MATERIAL";
            }

            SG_ASSERT_INTERNAL("Unknown Asset Type");
            return "NONE";
        }
    }
}
