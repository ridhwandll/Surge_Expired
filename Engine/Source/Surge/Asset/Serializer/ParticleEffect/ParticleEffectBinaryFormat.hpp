// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Asset/Serializer/AssetStamp.hpp"

namespace Surge
{
    class ParticleEffect;
    class AssetManager;
}

namespace Surge::ParticleEffectBinary
{
    // ParticleEffectBinary (.rPARTICLE_EFFECT), the only effect format the runtime loads (the .spfx JSON is the Editor's source)
    // AssetStamp
    // Header        Magic, Version, EmitterCount
    // Per emitter   Name, Enabled, then every module field by field in declaration order. Curves/gradients store their keys only,
    //               ParticleEffect::Bake() rebuilds the lookup tables after loading. Renderer.Texture is an AssetID
    // Little endian like every cooked format (x64, arm64)

    // Bump when a module field is added/removed/reordered (also the ParticleEffectCooker version, so the Editor re-cooks every effect)
    inline constexpr Uint VERSION = 1;

    bool Write(const String& path, const AssetStamp& stamp, const ParticleEffect& effect);
    bool Read(AssetManager& assetManager, const String& path, AssetStamp& outStamp, ParticleEffect& outEffect); // Marks the effect changed (layout), which bakes it
}
