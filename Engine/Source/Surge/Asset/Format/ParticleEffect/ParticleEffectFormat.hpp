// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Format/AssetFormat.hpp"

namespace Rapt0R
{
    class Reader;
}

namespace Surge
{
    class ParticleEffect;
    class AssetManager;

    // ParticleEffectBinary (.rPARTICLE_EFFECT) payload, cooked from the Editor's .spfx JSON source
    // EmitterCount  Uint
    // Per emitter   Name, Enabled, then every module field by field in declaration order (SURGE_RAPTOR_FIELDS lists in the .cpp), enums at
    //               their own size. Curves/gradients store their keys only, ParticleEffect::Bake() rebuilds the lookup tables after loading.
    //               Renderer.Texture last, as an AssetID
    namespace ParticleEffectBinary
    {
        // Bump when a module field is added/removed/reordered
        inline constexpr uint16_t VERSION = 1;

        void Write(Rapt0R::Writer& writer, const ParticleEffect& effect);
        bool Read(AssetManager& assetManager, Rapt0R::Reader& reader, ParticleEffect& outEffect); // Marks the effect changed (layout), which bakes it
    }

    class ParticleEffectFormat final : public AssetFormat
    {
    public:
        virtual AssetType GetType() const override { return AssetType::PARTICLE_EFFECT; }
        virtual uint16_t GetFormatVersion() const override { return ParticleEffectBinary::VERSION; }
        virtual Ref<Asset> Read(CookedFile& file, const AssetMetadata& metadata, const AssetContext& context) const override;
        virtual bool Write(Rapt0R::Writer& payload, const Asset& asset) const override;
    };

} // namespace Surge
