// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Texture2DSerializer.hpp"
#include "Surge/Core/Core.hpp"

#ifdef SURGE_PLATFORM_ANDROID
#include "Surge/Platform/Android/AndroidApp.hpp"
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <android/asset_manager.h>
#endif

#include <stb_image.h>
#include "Surge/Utility/Filesystem.hpp"


namespace Surge
{
    void Texture2DSerializer::Initialize()
    {
        mSerializerType = AssetType::TEXTURE2D;
    }

    bool Texture2DSerializer::Serialize(Ref<Asset> asset) const
    {
        bool result = false;
        //AssetManager* assetManager = Core::GetAssetManager();
        //const AssetMetadata& metadata = assetManager->GetMetadata(asset->GetID());
        //String absolutePath = assetManager->GetAbsolutePath(metadata.RelativePath);
        SG_ASSERT_INTERNAL("[Texture2DSerializer] Serialize: You can not serialize Texture2Ds yet");
        return result;
    }

    Ref<Asset> Texture2DSerializer::Deserialize(const AssetMetadata& metadata) const
    {
        AssetManager* assetManager = Core::GetAssetManager();
        String absolutePath = assetManager->GetAbsolutePath(metadata.RelativePath);

        int width = 0, height = 0, channels = 0;
        stbi_uc* data = nullptr;
#ifdef SURGE_PLATFORM_WINDOWS
        data = stbi_load(absolutePath.c_str(), &width, &height, &channels, 4);
#elif defined(SURGE_PLATFORM_ANDROID)
        android_app* app = Android::GAndroidApp;
        AAssetManager* androidAssetManager = app->activity->assetManager;
        AAsset* asset = AAssetManager_open(androidAssetManager, absolutePath.c_str(), AASSET_MODE_BUFFER);

        Vector<unsigned char> buffer;
        int bufferSize = AAsset_getLength(asset);
        buffer.resize(bufferSize);

        AAsset_read(asset, buffer.data(), bufferSize);
        AAsset_close(asset);

        data = stbi_load_from_memory(buffer.data(), bufferSize, &width, &height, &channels, 4);
#endif

        if(!data)
        {
            Log<Severity::Error>("Failed to load texture at path: {0}", absolutePath);
            return nullptr;
        }

        TextureSpecification spec;
        spec.Content = data;
        spec.Width = width;
        spec.Height = height;
        spec.GenerateMips = true;
        spec.Format = ImageFormat::RGBA8_SRGB;
        spec.DebugName = Filesystem::GetNameWithExtension(absolutePath);
        Ref<Texture2D> texture2D = Texture2D::Create(spec);

        stbi_image_free(spec.Content);

        return texture2D.As<Asset>();
    }

    void Texture2DSerializer::Shutdown()
    {
        Log<Severity::Info>("[Texture2DSerializer] Shutdown");
    }
}


