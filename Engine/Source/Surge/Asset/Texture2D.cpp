// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Texture2D.hpp"
#include "Surge/Core/Core.hpp"

#ifdef SURGE_PLATFORM_ANDROID
#include "Surge/Platform/Android/AndroidApp.hpp"
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <android/asset_manager.h>
#endif

#include <stb_image.h>
#include "../Utility/Filesystem.hpp"

namespace Surge
{
    Texture2D::Texture2D(const String& path)
    {
        Renderer* renderer = Core::GetRenderer();

        stbi_set_flip_vertically_on_load(true);

        int width = 0, height = 0, channels = 0;
        stbi_uc* data = nullptr;
#ifdef SURGE_PLATFORM_WINDOWS
        data = stbi_load(path.c_str(), &width, &height, &channels, 4);
#elif defined(SURGE_PLATFORM_ANDROID)
        android_app* app = Android::GAndroidApp;
        AAssetManager* assetManager = app->activity->assetManager;
        AAsset* asset = AAssetManager_open(assetManager, path.c_str(), AASSET_MODE_BUFFER);

        Vector<unsigned char> buffer;
        int bufferSize = AAsset_getLength(asset);
        buffer.resize(bufferSize);

        AAsset_read(asset, buffer.data(), bufferSize);
        AAsset_close(asset);

        data = stbi_load_from_memory(buffer.data(), bufferSize, &width, &height, &channels, 4);
#endif

        if(!data)
            Log<Severity::Error>("Failed to load texture at path: {0}", path);

        ImageDesc desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.Format = ImageFormat::RGBA8_SRGB;
        desc.Usage = ImageUsage::SAMPLED | ImageUsage::TRANSFER_DST;
        desc.DebugName = Filesystem::GetNameWithExtension(path);
        desc.GenerateImGuiID = true;
        desc.InitialData = data;
        desc.DataSize = width * height * 4;
        desc.Sampler = renderer->GetDefaultSampler();
        mImageHandle = renderer->GetRHI()->CreateImage(desc);
        stbi_image_free(data);
    }

    Texture2D::~Texture2D()
    {
        Core::GetRenderer()->GetRHI()->DestroyImage(mImageHandle);
    }

    Ref<Texture2D> Texture2D::Create(const String& path)
    {
        return Ref<Texture2D>::Create(path);
    }

}