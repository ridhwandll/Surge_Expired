// Copyright (c) - SurgeTechnologies - All rights reserved
#include "ParticleEffectCooker.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Asset/Serializer/ParticleEffect/ParticleEffectSerializer.hpp"
#include "Surge/Utility/Filesystem.hpp"

namespace Surge
{
    CookResult ParticleEffectCooker::Cook(const String& sourceAbsPath, AssetID id) const
    {
        AssetManager* am = Core::GetAssetManager();
        const String sidecarPath = am->GetSidecarPath(id, AssetType::PARTICLE_EFFECT);

        CookResult result;
        result.OutputPath = sidecarPath;

        String json;
        Ref<ParticleEffect> effect = ParticleEffect::Create();
        if(!Filesystem::ReadTextFile(sourceAbsPath, json) || !ParticleEffectSerializer::FromJson(*Core::GetAssetManager(), json, *effect))
        {
            Log<Severity::Error>("[ParticleEffectCooker] Failed to read particle effect: {}", sourceAbsPath);
            return result;
        }

        result.Success = ParticleEffectBinary::Write(sidecarPath, AssetStampWriter::Build(sourceAbsPath, GetCookerVersion()), *effect);
        result.InputMegaBytes = Filesystem::FileSizeInMB(sourceAbsPath);
        result.OutputMegaBytes = Filesystem::FileSizeInMB(sidecarPath);
        return result;
    }
}
