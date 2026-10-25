// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Texture2DSerializer.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Graphics/RHI/RHIDescs.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "SurgeReflect/Enum.hpp"
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
        AssetManager* am = mContext->AssetManager_;
        const String ktx2Path = am->GetSidecarPath(metadata.ID);

        // Null marks the asset MISSING (LoadFromKTX2 logs why), no assert: a bad cooked file must not take the app down
        Ref<Texture2D> texture = LoadFromKTX2(*mContext->Renderer_, ktx2Path);
        if(!texture)
            return nullptr;
        return texture.As<Asset>();
    }

    Ref<Texture2D> Texture2DSerializer::LoadFromKTX2(Renderer& renderer, const String& ktx2Path)
    {
        Vector<Byte> fileData;
        if(!Filesystem::ReadBinaryFile(ktx2Path, fileData))
        {
            Log<Severity::Error>("[Texture2DSerializer] Failed to read KTX2 file at path: {0} Maybe the file doesn't exist?", ktx2Path);
            return {};
        }

        TextureSpecification spec = LoadFromKTX2(renderer.GetRHI().get(), fileData, ktx2Path, false);
        if(spec.Mips.empty())
            return {};
        return Texture2D::Create(renderer, spec);
    }

    struct TranscodeTarget
    {
        basist::transcoder_texture_format Format;
        ImageFormat SRGB;
        ImageFormat UNORM;
        bool IsBlockCompressed; // 16 bytes per 4x4 block, otherwise 4 bytes per pixel (RGBA32)
    };

    // Preferred hardware format per platform, falling back to what the GPU actually supports
    // Android: ASTC 4x4 -> ETC2 RGBA (guaranteed on GLES3-class/Vulkan Android GPUs) -> RGBA32
    // Desktop: BC7 -> RGBA32
    static const TranscodeTarget& SelectTranscodeTarget(GraphicsRHI* rhi)
    {
        static const TranscodeTarget sTarget = [rhi]()
        {
            const TranscodeTarget candidates[] = {
#ifdef SURGE_PLATFORM_ANDROID
                { basist::transcoder_texture_format::cTFASTC_4x4_RGBA, ImageFormat::ASTC4x4_SRGB, ImageFormat::ASTC4x4_UNORM, true },
                { basist::transcoder_texture_format::cTFETC2_RGBA, ImageFormat::ETC2_RGBA8_SRGB, ImageFormat::ETC2_RGBA8_UNORM, true },
#else
                { basist::transcoder_texture_format::cTFBC7_RGBA, ImageFormat::BC7_SRGB, ImageFormat::BC7_UNORM, true },
#endif
            };

            for(const TranscodeTarget& candidate : candidates)
            {
                if(rhi->IsFormatSupported(candidate.SRGB) && rhi->IsFormatSupported(candidate.UNORM))
                {
                    Log<Severity::Info>("[Texture2DSerializer] Transcoding KTX2 textures to {}", SurgeReflect::EnumToString(candidate.UNORM).data());
                    return candidate;
                }
            }

            Log<Severity::Warn>("[Texture2DSerializer] No supported block compressed format, textures are transcoded to uncompressed RGBA8 (4x memory)");
            return TranscodeTarget{ basist::transcoder_texture_format::cTFRGBA32, ImageFormat::RGBA8_SRGB, ImageFormat::RGBA8_UNORM, false };
        }();
        return sTarget;
    }

    TextureSpecification Texture2DSerializer::LoadFromKTX2(GraphicsRHI* rhi, const Vector<Byte>& ktx2Data, const String& debugStr, bool raw)
    {
        if(!raw && ktx2Data.size() <= sizeof(AssetStamp))
        {
            Log<Severity::Error>("[Texture2DSerializer] Corrupted KTX2 file (too small): {}", debugStr);
            return {};
        }

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

        const TranscodeTarget& target = SelectTranscodeTarget(rhi);
        const basist::transcoder_texture_format transcoderFmt = target.Format;
        spec.Format = isSRGB ? target.SRGB : target.UNORM;
        spec.GenerateMips = false;

        const Uint targetMips = ktx2.get_levels();
        spec.Mips.reserve(targetMips);

        for(Uint level = 0; level < targetMips; level++)
        {
            basist::ktx2_image_level_info info = {};
            ktx2.get_image_level_info(info, level, 0, 0);

            // Block formats: output size in 16 byte blocks, RGBA32: output size in pixels
            const Uint outputUnits = target.IsBlockCompressed ? info.m_total_blocks : info.m_orig_width * info.m_orig_height;
            const Uint levelBytes = target.IsBlockCompressed ? outputUnits * 16 : outputUnits * 4;
            Vector<Byte> levelData(levelBytes);

            if(!ktx2.transcode_image_level(level, 0, 0, levelData.data(), outputUnits, transcoderFmt))
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


