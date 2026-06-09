// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Texture2DSerializer.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Utility/Filesystem.hpp"

#include <basisu/transcoder/basisu_transcoder.h>
#include <stb_image.h>

namespace Surge
{
    void Texture2DSerializer::Initialize()
    {
        mSerializerType = AssetType::TEXTURE2D;
        basist::basisu_transcoder_init();
    }

    bool Texture2DSerializer::Serialize(Ref<Asset> asset) const
    {
        bool result = false;
        SG_ASSERT_INTERNAL("[Texture2DSerializer] Serialize: You can not serialize Texture2Ds yet");
        return result;
    }

    Ref<Asset> Texture2DSerializer::Deserialize(const AssetMetadata& metadata) const
    {
        AssetManager* assetManager = Core::GetAssetManager();
        String absolutePath = assetManager->GetAbsolutePath(metadata.RelativePath);

        Vector<uint8_t> fileData;
        if(!Filesystem::ReadBinaryFile(absolutePath, fileData))
        {
            Log<Severity::Error>("[Texture2DSerializer] Failed to read binary file at path: {0}", absolutePath);
            return nullptr;
        }

        int width = 0, height = 0, channels = 0;
        stbi_uc* data = stbi_load_from_memory(fileData.data(), static_cast<int>(fileData.size()), &width, &height, &channels, 4);
        if(!data)
        {
            Log<Severity::Error>("[Texture2DSerializer] STB failed to decode texture at path: {0}", absolutePath);
            return nullptr;
        }

        TextureSpecification spec;
        spec.Content = data;
        spec.Width = width;
        spec.Height = height;
        spec.GenerateMips = true;
        spec.Format = ImageFormat::RGBA8_SRGB;
        spec.DebugName = Filesystem::GetFilenameWithExt(absolutePath);
        Ref<Texture2D> texture2D = Texture2D::Create(spec);

        stbi_image_free(spec.Content);

        return texture2D.As<Asset>();
    }

    void Texture2DSerializer::Shutdown()
    {
        Log<Severity::Info>("[Texture2DSerializer] Shutdown");
    }
}


