// Copyright (c) - SurgeTechnologies - All rights reserved
#include "AudioSerializer.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Audio/Audio.hpp"

namespace Surge
{
    void AudioSerializer::Initialize()
    {
        mSerializerType = AssetType::AUDIO;
    }

    bool AudioSerializer::Serialize([[maybe_unused]] Ref<Asset> asset) const
    {
        SG_ASSERT_INTERNAL("[AudioSerializer] Serialize: You can not serialize Audio assets yet");
        return false;
    }

    Ref<Asset> AudioSerializer::Deserialize(const AssetMetadata& metadata) const
    {
        AssetManager* am = Core::GetAssetManager();
        const String audioPath = am->GetSidecarPath(metadata.ID);

        Ref<Audio> audio = Ref<Audio>::Create(audioPath);
        return audio.As<Asset>();
    }

    void AudioSerializer::Shutdown()
    {
        Log<Severity::Info>("[AudioSerializer] Shutdown");
    }
}
