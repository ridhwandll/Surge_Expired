// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Memory.hpp"
#include "Surge/Core/String.hpp"
#include "AudioID.hpp"

namespace Surge
{
    class Audio;

    class AudioEngine
    {
    public:
        void Initialize();
        void Shutdown();

        AudioID CreateAudio(const String& filepath, bool isStreaming, bool isSpatialized);
        void DestroyAudio(AudioID audioId);

        void PlayAudio(AudioID audioId);
        void StopAudio(AudioID audioId);

        void SetAttenuationModel(AudioID audioId, AttenuationModel model);
        void SetLooping(AudioID audioId, bool isLooping);
        void SetVolume(AudioID audioId, float volume);
        void SetPitch(AudioID audioId, float pitch);
        void SetMinDistance(AudioID audioId, float distance);
        void SetMaxDistance(AudioID audioId, float distance);
        void SetListenerPosition(float x, float y, float z);
        void SetPosition(float x, float y, float z, AudioID audioId);
        void PlayOneShot(const Ref<Audio>& audioAsset);
    };
}