// Copyright (c) - SurgeTechnologies - All rights reserved
#include "ParticleEffectCooker.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Asset/Format/BinarySerialization.hpp"
#include "Surge/Asset/Format/ParticleEffect/ParticleEffectFormat.hpp"
#include "Asset/SourceIO/ParticleEffectSourceIO.hpp"

namespace Surge
{
    bool ParticleEffectCooker::Cook(const CookContext& context) const
    {
        Ref<ParticleEffect> effect = ParticleEffect::Create();
        if(!ParticleEffectSourceIO::Read(*Core::GetAssetManager(), context.SourcePath, *effect))
        {
            Log<Severity::Error>("[ParticleEffectCooker] Failed to read particle effect: {}", context.SourcePath.string());
            return false;
        }

        Rapt0R::Writer payload(16 * 1024);
        ParticleEffectBinary::Write(payload, *effect);
        return context.Write(ParticleEffectBinary::VERSION, payload);
    }
}
