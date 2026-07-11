// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Audio/AudioEngine.hpp"
#include "Audio.hpp"
#include <miniaudio/miniaudio.h>
#include <glm/common.hpp>

#ifdef SURGE_PLATFORM_ANDROID
#include "Surge/Platform/Android/AndroidApp.hpp"
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <android/asset_manager.h>
#endif

namespace Surge
{
#ifdef SURGE_PLATFORM_ANDROID
    static ma_result AndroidAudioOpen(ma_vfs* pVFS, const char* pFilePath, ma_uint32 openMode, ma_vfs_file* pFile)
    {
        // Get the global Asset Manager you initialized in your Android platform layer
        AAssetManager* assetManager = Android::GAndroidApp->activity->assetManager;
        AAsset* asset = AAssetManager_open(assetManager, pFilePath, AASSET_MODE_RANDOM);
        if(!asset)
            return MA_DOES_NOT_EXIST;

        *pFile = (ma_vfs_file)asset;
        return MA_SUCCESS;
    }
    static ma_result AndroidAudioRead(ma_vfs* pVFS, ma_vfs_file file, void* pDst, size_t sizeInBytes, size_t* pBytesRead)
    {
        AAsset* asset = (AAsset*)file;
        int bytes = AAsset_read(asset, pDst, sizeInBytes);
        if(bytes < 0)
            return MA_ERROR;
        if(pBytesRead)
            *pBytesRead = bytes;
        return MA_SUCCESS;
    }
    static ma_result AndroidAudioSeek(ma_vfs* pVFS, ma_vfs_file file, ma_int64 offset, ma_seek_origin origin)
    {
        AAsset* asset = (AAsset*)file;
        int whence = (origin == ma_seek_origin_start) ? SEEK_SET : (origin == ma_seek_origin_current) ? SEEK_CUR : SEEK_END;
        off_t res = AAsset_seek(asset, offset, whence);
        return (res != (off_t)-1) ? MA_SUCCESS : MA_ERROR;
    }
    static ma_result AndroidAudioTell(ma_vfs* pVFS, ma_vfs_file file, ma_int64* pCursor)
    {
        AAsset* asset = (AAsset*)file;
        off_t res = AAsset_seek(asset, 0, SEEK_CUR); // Seek 0 bytes to get current pos
        if(res == (off_t)-1)
            return MA_ERROR;
        if(pCursor)
            *pCursor = res;
        return MA_SUCCESS;
    }
    static ma_result AndroidAudioInfo(ma_vfs* pVFS, ma_vfs_file file, ma_file_info* pInfo)
    {
        AAsset* asset = (AAsset*)file;
        if(pInfo)
            pInfo->sizeInBytes = AAsset_getLength(asset);
        return MA_SUCCESS;
    }
    static ma_result AndroidAudioClose(ma_vfs* pVFS, ma_vfs_file file)
    {
        AAsset* asset = (AAsset*)file;
        AAsset_close(asset);
        return MA_SUCCESS;
    }
    static ma_vfs_callbacks sAndroidVFS = {
        AndroidAudioOpen,
        NULL, // onOpenW (wchar_t version not needed for Android)
        AndroidAudioClose,
        AndroidAudioRead,
        NULL, // onWrite
        AndroidAudioSeek,
        AndroidAudioTell,
        AndroidAudioInfo
    };
#endif

    static ma_engine* sAudioEngine;

    void AudioEngine::Initialize()
    {
        ma_result result;
        sAudioEngine = new ma_engine();
        ma_engine_config config = ma_engine_config_init();

#ifdef SURGE_PLATFORM_ANDROID
        config.pResourceManagerVFS = &sAndroidVFS;
#endif

        result = ma_engine_init(&config, sAudioEngine);
        SG_ASSERT(result == MA_SUCCESS, "Failed to initialize miniaudio!");

        Log<Severity::Info>("AudioEngine initialized");
    }

    void AudioEngine::Shutdown()
    {
        ma_engine_uninit(sAudioEngine);
        delete sAudioEngine;
    }

    Surge::AudioID AudioEngine::CreateAudio(const String& filepath, bool isStreaming, bool isSpatialized)
    {
        ma_sound* sound = new ma_sound();
        ma_uint32 flags = isStreaming ? MA_SOUND_FLAG_STREAM : MA_SOUND_FLAG_DECODE;
        if(!isSpatialized)
            flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;

        ma_result result = ma_sound_init_from_file(sAudioEngine, filepath.c_str(), flags, NULL, NULL, sound);

        if(result != MA_SUCCESS)
        {
            Log<Severity::Error>("Failed to initialize audio file: %s", filepath.c_str());
            delete sound;
            return nullptr;
        }
        return (AudioID)sound;
    }

    void AudioEngine::DestroyAudio(AudioID audioId)
    {
        if(!audioId)
            return;

        ma_sound* sound = (ma_sound*)audioId;
        ma_sound_uninit(sound);
        delete sound;
    }

    void AudioEngine::PlayAudio(AudioID audioId)
    {
        SG_ASSERT(audioId, "AudioID is NULL!");
        ma_sound* sound = (ma_sound*)audioId;
        ma_sound_start(sound);
    }

    void AudioEngine::StopAudio(AudioID audioId)
    {
        SG_ASSERT(audioId, "AudioID is NULL!");
        ma_sound* sound = (ma_sound*)audioId;
        ma_sound_stop(sound);
    }

    void AudioEngine::SetAttenuationModel(AudioID audioId, AttenuationModel model)
    {
        SG_ASSERT(audioId, "AudioID is NULL!");
        ma_sound* sound = (ma_sound*)audioId;
        ma_sound_set_attenuation_model(sound, (ma_attenuation_model)model);
    }

    void AudioEngine::SetLooping(AudioID audioId, bool isLooping)
    {
        SG_ASSERT(audioId, "AudioID is NULL!");
        ma_sound* sound = (ma_sound*)audioId;
        ma_sound_set_looping(sound, isLooping);
    }

    void AudioEngine::SetVolume(AudioID audioId, float volume)
    {
        SG_ASSERT(audioId, "AudioID is NULL!");
        ma_sound* sound = (ma_sound*)audioId;
        ma_sound_set_volume(sound, glm::clamp(volume, 0.0f, 2.0f));
    }

    void AudioEngine::SetPitch(AudioID audioId, float pitch)
    {
        SG_ASSERT(audioId, "AudioID is NULL!");
        ma_sound* sound = (ma_sound*)audioId;
        ma_sound_set_pitch(sound, glm::clamp(pitch, 0.1f, 3.0f));
    }

    void AudioEngine::SetMinDistance(AudioID audioId, float distance)
    {
        SG_ASSERT(audioId, "AudioID is NULL!");
        ma_sound_set_min_distance((ma_sound*)audioId, distance);
    }

    void AudioEngine::SetMaxDistance(AudioID audioId, float distance)
    {
        SG_ASSERT(audioId, "AudioID is NULL!");
        ma_sound_set_max_distance((ma_sound*)audioId, distance);
    }

    void AudioEngine::SetListenerPosition(float x, float y, float z)
    {
        ma_engine_listener_set_position(sAudioEngine, 0, x, y, z);
    }

    void AudioEngine::SetPosition(float x, float y, float z, AudioID audioId)
    {
        ma_sound* sound = (ma_sound*)audioId;
        if(sound)
            ma_sound_set_position(sound, x, y, z);
        else
            Log<Severity::Warn>("[AudioEngine::SetPosition] Invalid AudioID!");
    }

    void AudioEngine::PlayOneShot(const Ref<Audio>& audioAsset)
    {
        if(!audioAsset)
            return;

        ma_engine_play_sound(sAudioEngine, audioAsset->GetFilepath().c_str(), NULL);
    }

}
