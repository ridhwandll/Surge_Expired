// Copyright (c) - SurgeTechnologies - All rights reserved
#include "AudioSerializer.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Audio/Audio.hpp"
#include "AudioBinaryFormat.hpp"

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
        AssetManager* am = mContext->AssetManager_;

        AssetStamp stamp;
        Vector<Byte> encodedData;
        if(!AudioBinary::Read(am->GetSidecarPath(metadata.ID), stamp, encodedData))
            return nullptr;

        return Ref<Audio>::Create(std::move(encodedData)).As<Asset>();
    }

    void AudioSerializer::Shutdown()
    {
        Log<Severity::Info>("[AudioSerializer] Shutdown");
    }
}
