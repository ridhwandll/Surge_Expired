// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Serializer/IAssetSerializer.hpp"
#include "Surge/Graphics/Particles/ParticleEffect.hpp"

namespace Surge
{
    // .spfx files are human readable JSON (diffable, mergeable), read directly at runtime like .sui layouts, no cooking step
    class ParticleEffectSerializer : public AssetSerializer
    {
    public:
        virtual ~ParticleEffectSerializer() = default;

        virtual void Initialize() override;
        virtual bool Serialize(Ref<Asset> asset) const override;
        virtual Ref<Asset> Deserialize(const AssetMetadata& metadata) const override;
        virtual void Shutdown() override;

        // Used by the Editor for undo snapshots. FromJson leaves the effect untouched when the json is invalid
        static String ToJson(const ParticleEffect& effect);
        static bool FromJson(AssetManager& assetManager, const String& json, ParticleEffect& outEffect); // assetManager loads the textures
    };
}
