// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Path.hpp"
#include "Surge/Graphics/Particles/ParticleEffect.hpp"

namespace Surge
{
    class AssetManager;

    // .spfx files are human readable JSON (diffable, mergeable), the Editor's source of particle effects. Every build loads the cooked effect
    namespace ParticleEffectSourceIO
    {
        // Also the Particle Editor's undo snapshots. FromJson leaves the effect untouched when the json is invalid
        String ToJson(const ParticleEffect& effect);
        bool FromJson(AssetManager& assetManager, const String& json, ParticleEffect& outEffect); // assetManager loads the textures

        bool Write(const ParticleEffect& effect, const Path& path);
        bool Read(AssetManager& assetManager, const Path& path, ParticleEffect& outEffect);
    }
}
