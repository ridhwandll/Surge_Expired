// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Texture2DFormat.hpp"
#include "Surge/Asset/Format/BinarySerialization.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Graphics/RHI/RHIDescs.hpp"
#include "Surge/Asset/AssetContext.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "SurgeReflect/Enum.hpp"

#include <basisu/transcoder/basisu_transcoder.h>

namespace Surge
{
    namespace Texture2DBinary
    {
        void Write(Rapt0R::Writer& writer, std::span<const Byte> ktx2Data)
        {
            writer.WriteRawBytes(ktx2Data.data(), ktx2Data.size());
        }

        bool Read(Rapt0R::Reader& reader, std::span<const Byte>& outKtx2Data)
        {
            outKtx2Data = reader.UnreadSpan();
            return reader.Skip(outKtx2Data.size()) && !outKtx2Data.empty();
        }
    }

    Texture2DFormat::Texture2DFormat()
    {
        basist::basisu_transcoder_init();
    }

    Ref<Asset> Texture2DFormat::Read(CookedFile& file, const AssetMetadata& metadata, const AssetContext& context) const
    {
        std::span<const Byte> ktx2Data;
        Rapt0R::Reader payload = file.GetPayload();
        Texture2DBinary::Read(payload, ktx2Data);
        TextureSpecification spec = LoadFromKTX2(context.Renderer_->GetRHI().get(), ktx2Data, metadata.RelativePath);
        if(spec.Mips.empty())
            return nullptr; // LoadFromKTX2 logged why, the asset is marked MISSING
        return Texture2D::Create(*context.Renderer_, spec).As<Asset>();
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
                    Log<Severity::Info>("[Texture2DFormat] Transcoding KTX2 textures to {}", SurgeReflect::EnumToString(candidate.UNORM).data());
                    return candidate;
                }
            }

            Log<Severity::Warn>("[Texture2DFormat] No supported block compressed format, textures are transcoded to uncompressed RGBA8 (4x memory)");
            return TranscodeTarget{ basist::transcoder_texture_format::cTFRGBA32, ImageFormat::RGBA8_SRGB, ImageFormat::RGBA8_UNORM, false };
        }();
        return sTarget;
    }

    TextureSpecification Texture2DFormat::LoadFromKTX2(GraphicsRHI* rhi, std::span<const Byte> ktx2Data, const String& debugStr)
    {
        basist::ktx2_transcoder ktx2;
        if(ktx2Data.empty() || !ktx2.init(ktx2Data.data(), static_cast<Uint>(ktx2Data.size())))
        {
            Log<Severity::Error>("[Texture2DFormat] Corrupted KTX2 file: {}", debugStr);
            return {};
        }

        const bool isSRGB = ktx2.get_dfd_transfer_func() == basist::KTX2_KHR_DF_TRANSFER_SRGB;
        if(!ktx2.start_transcoding())
        {
            Log<Severity::Error>("[Texture2DFormat] Failed to start transcoding(from KTX2) {}!", debugStr);
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
                Log<Severity::Error>("[Texture2DFormat] Block transcode failed at level {} for {}", level, debugStr);
                return {};
            }

            spec.Mips.push_back({ std::move(levelData), info.m_orig_width, info.m_orig_height });
        }
        Log<Severity::Trace>("[Texture2DFormat] Created Texture2D form KTX2 {}", Filesystem::GetFilenameWithExt(debugStr));
        return spec;
    }
}
