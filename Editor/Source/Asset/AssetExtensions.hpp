// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Asset.hpp"

namespace Surge
{
    // Source file extensions: Editor only, the engine knows assets by their cooked files
    inline AssetType AssetTypeFromExtension(const char* str)
    {
        if(strcmp(str, ".png") == 0 || strcmp(str, ".jpg") == 0 || strcmp(str, ".jpeg") == 0) return AssetType::TEXTURE2D;
        if(strcmp(str, ".glb") == 0 || strcmp(str, ".gltf") == 0) return AssetType::MESH;
        if(strcmp(str, ".smat") == 0) return AssetType::MATERIAL;
        if(strcmp(str, ".srg") == 0) return AssetType::SCENE;
        if(strcmp(str, ".lua") == 0) return AssetType::SCRIPT;
        if(strcmp(str, ".ttf") == 0 || strcmp(str, ".otf") == 0) return AssetType::FONT;
        if(strcmp(str, ".wav") == 0 || strcmp(str, ".mp3") == 0 || strcmp(str, ".ogg") == 0) return AssetType::AUDIO;
        if(strcmp(str, ".sui") == 0) return AssetType::UI_LAYOUT;
        if(strcmp(str, ".spfx") == 0) return AssetType::PARTICLE_EFFECT;
        return AssetType::NONE;
    }

    inline const char* GetExtensionFromAssetType(AssetType type)
    {
        switch(type)
        {
            case AssetType::MATERIAL: return ".smat";
            case AssetType::SCENE: return ".srg";
            case AssetType::SCRIPT: return ".lua";
            case AssetType::FONT: return ".ttf";
            case AssetType::UI_LAYOUT: return ".sui";
            case AssetType::PARTICLE_EFFECT: return ".spfx";
            default:
                SG_ASSERT_INTERNAL("GetExtensionFromAssetType: Invalid asset type");
                return "";
        }
    }

} // namespace Surge
