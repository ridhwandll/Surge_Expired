// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "AssetEnums.hpp"
#include <unordered_map>

namespace Surge
{
    inline static std::unordered_map<String, AssetType> sAssetExtensionMap =
    {
        { ".surge",   AssetType::SCENE            },
        { ".smat",    AssetType::MATERIAL         },
        { ".spmat",   AssetType::PHYSICS_MATERIAL },
        { ".gltf",    AssetType::MESH             },
        { ".glb",     AssetType::MESH             },
        { ".png",     AssetType::TEXTURE2D        },
        { ".jpg",     AssetType::TEXTURE2D        },
        { ".jpeg",    AssetType::TEXTURE2D        },
        { ".hdr",     AssetType::ENVIRONMENT_MAP  },
    };
}
