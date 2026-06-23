// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Texture2DSerializer.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Graphics/RHI/RHIDescs.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "AssetStamp.hpp"

#include <basisu/transcoder/basisu_transcoder.h>

namespace Surge
{
    void Texture2DSerializer::Initialize()
    {
        mSerializerType = AssetType::TEXTURE2D;
        basist::basisu_transcoder_init();
    }

    bool Texture2DSerializer::Serialize([[maybe_unused]] Ref<Asset> asset) const
    {
        bool result = false;
        SG_ASSERT_INTERNAL("[Texture2DSerializer] Serialize: You can not serialize Texture2Ds yet");
        return result;
    }

    Ref<Asset> Texture2DSerializer::Deserialize(const AssetMetadata& metadata) const
    {
        AssetManager* am = Core::GetAssetManager();
        const String ktx2Path = am->GetSidecarPath(metadata.ID);

        Ref<Texture2D> texture = LoadFromKTX2(ktx2Path);
        SG_ASSERT(texture, "[Texture2DSerializer] Failed to deserialize Texture2D: {}", metadata.RelativePath);
        return texture.As<Asset>();
    }

    Ref<Texture2D> Texture2DSerializer::LoadFromKTX2(const String& ktx2Path)
    {
        Vector<Byte> fileData;
        if(!Filesystem::ReadBinaryFile(ktx2Path, fileData))
        {
            Log<Severity::Error>("[Texture2DSerializer] Failed to read KTX2 file at path: {0} Maybe the file doesn't exist?", ktx2Path);
            return {};
        }

        TextureSpecification spec = LoadFromKTX2(fileData, ktx2Path, false);
        return Texture2D::Create(spec);
    }

    TextureSpecification Texture2DSerializer::LoadFromKTX2(const Vector<Byte>& ktx2Data, const String& debugStr, bool raw)
    {
        const Byte* ktx2Start = raw ? ktx2Data.data() : ktx2Data.data() + sizeof(AssetStamp);
        const Uint ktx2Size = raw ? static_cast<Uint>(ktx2Data.size()) : static_cast<Uint>(ktx2Data.size() - sizeof(AssetStamp));
        basist::ktx2_transcoder ktx2;
        if(!ktx2.init(ktx2Start, ktx2Size))
        {
            Log<Severity::Error>("[Texture2DSerializer] Corrupted KTX2 file: {}", debugStr);
            return {};
        }

        const bool isSRGB = ktx2.get_dfd_transfer_func() == basist::KTX2_KHR_DF_TRANSFER_SRGB;
        if(!ktx2.start_transcoding())
        {
            Log<Severity::Error>("[Texture2DSerializer] Failed to start transcoding(from KTX2) {}!", debugStr);
            return {};
        }

        TextureSpecification spec;
        spec.DebugName = Filesystem::GetFilenameWithExt(debugStr);

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
                Log<Severity::Error>("[Texture2DSerializer] Block transcode failed at level {} for {}", level, debugStr);
                return {};
            }

            spec.Mips.push_back({ std::move(levelData), info.m_orig_width, info.m_orig_height });
        }
        Log<Severity::Trace>("[Texture2DSerializer] Created Texture2D form KTX2 {}", Filesystem::GetFilenameWithExt(debugStr));
        return spec;
    }

    void Texture2DSerializer::Shutdown()
    {
        Log<Severity::Info>("[Texture2DSerializer] Shutdown");
    }
}


