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
        AssetManager* am = Core::GetAssetManager();
        const String absPath = am->GetAbsolutePath(metadata.RelativePath);
        const String ktx2Path = am->GetSidecarPath(absPath, AssetType::TEXTURE2D);

#ifdef SURGE_PLATFORM_ANDROID
        SG_ASSERT(Filesystem::Exists(ktx2Path), "[Texture2DSerializer] KTX2 missing for '{}'. Run cook step first.", absPath);
        return LoadFromKTX2(ktx2Path);
#else
        Ref<Asset> ktx2Asset = nullptr;
        if(Filesystem::Exists(ktx2Path))
            ktx2Asset = LoadFromKTX2(ktx2Path);

        if(!ktx2Asset)
        {
            Log<Severity::Warn>("[Texture2DSerializer] No KTX2/Bad KTX2 for {}, using raw source. Run cook step for faster loading!", absPath);
            return LoadFromSource(absPath);
        }
        return ktx2Asset;
#endif
    }

    Ref<Asset> Texture2DSerializer::LoadFromKTX2(const String& ktx2Path) const
    {
        Vector<Byte> fileData;
        if(!Filesystem::ReadBinaryFile(ktx2Path, fileData))
            return nullptr;

        basist::ktx2_transcoder ktx2;
        if(!ktx2.init(fileData.data(), static_cast<Uint>(fileData.size())))
            return nullptr;

        const bool isSRGB = ktx2.get_dfd_transfer_func() == basist::KTX2_KHR_DF_TRANSFER_SRGB;
        if(!ktx2.start_transcoding())
            return nullptr;

        TextureSpecification spec;
        spec.DebugName = Filesystem::GetFilenameWithExt(ktx2Path);

#ifdef SURGE_PLATFORM_ANDROID
        // Transcode to hardware ASTC 4x4
        const basist::transcoder_texture_format transcoderFmt = basist::transcoder_texture_format::cTFASTC_4x4_RGBA;
        spec.Format = isSRGB ? ImageFormat::ASTC4x4_SRGB : ImageFormat::ASTC4x4_UNORM;
#else
        // Transcode to hardware BC7
        const basist::transcoder_texture_format transcoderFmt = basist::transcoder_texture_format::cTFBC7_RGBA;
        spec.Format = isSRGB ? ImageFormat::BC7_SRGB : ImageFormat::BC7_UNORM;
#endif
        spec.GenerateMips = false;

        const Uint targetMips = ktx2.get_levels();
        spec.Mips.reserve(targetMips);

        for(Uint level = 0; level < targetMips; level++)
        {
            basist::ktx2_image_level_info info = {};
            ktx2.get_image_level_info(info, level, 0, 0);

            const Uint blockCount = info.m_total_blocks;
            const Uint levelBytes = blockCount * 16;
            Vector<Byte> levelData(levelBytes);

            if(!ktx2.transcode_image_level(level, 0, 0, levelData.data(), blockCount, transcoderFmt))
            {
                Log<Severity::Error>("[Texture2DSerializer] Block transcode failed at level {} for {}", level, ktx2Path);
                return nullptr;
            }

            spec.Mips.push_back({ std::move(levelData), info.m_orig_width, info.m_orig_height });
        }
        Log<Severity::Info>("[Texture2DSerializer] Created Texture2D form KTX2 {}", ktx2Path);
        return Texture2D::Create(spec).As<Asset>();
    }

    Ref<Asset> Texture2DSerializer::LoadFromSource(const String& absPath) const
    {
        Vector<uint8_t> fileData;
        if(!Filesystem::ReadBinaryFile(absPath, fileData))
        {
            Log<Severity::Error>("[Texture2DSerializer::LoadFromSource] Failed to read binary file at path: {0}", absPath);
            return nullptr;
        }

        int width = 0, height = 0, channels = 0;
        stbi_uc* data = stbi_load_from_memory(fileData.data(), static_cast<int>(fileData.size()), &width, &height, &channels, 4);

        if(!data) return nullptr;

        TextureSpecification spec;
        spec.Format = ImageFormat::RGBA8_SRGB;
        spec.GenerateMips = true;
        spec.DebugName = Filesystem::GetFilenameWithExt(absPath);

        const size_t dataSize = static_cast<size_t>(width * height * 4);

        // Copy into the Level 0 vector
        Vector<Byte> level0Data(data, data + dataSize);
        spec.Mips.push_back({ std::move(level0Data), static_cast<Uint>(width), static_cast<Uint>(height) });

        stbi_image_free(data);

        return Texture2D::Create(spec).As<Asset>();
    }


    void Texture2DSerializer::Shutdown()
    {
        Log<Severity::Info>("[Texture2DSerializer] Shutdown");
    }
}


