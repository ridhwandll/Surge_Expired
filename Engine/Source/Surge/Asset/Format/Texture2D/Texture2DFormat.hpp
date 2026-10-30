// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Format/AssetFormat.hpp"
#include "Surge/Graphics/HighLevel/Texture2D.hpp"
#include <span>

namespace Rapt0R
{
    class Reader;
}

namespace Surge
{
    class GraphicsRHI;

    // Texture2DBinary (.rTEXTURE2D) payload: the KTX2 (UASTC) file basisu wrote, transcoded at load to what the GPU supports
    namespace Texture2DBinary
    {
        inline constexpr uint16_t VERSION = 1;

        void Write(Rapt0R::Writer& writer, std::span<const Byte> ktx2Data);
        bool Read(Rapt0R::Reader& reader, std::span<const Byte>& outKtx2Data); // A view of the payload, false when it is empty
    }

    class Texture2DFormat final : public AssetFormat
    {
    public:
        Texture2DFormat();

        virtual AssetType GetType() const override { return AssetType::TEXTURE2D; }
        virtual uint16_t GetFormatVersion() const override { return Texture2DBinary::VERSION; }
        virtual Ref<Asset> Read(CookedFile& file, const AssetMetadata& metadata, const AssetContext& context) const override;

        // Transcodes a KTX2 file to the GPU's preferred block format (picked once from what rhi supports). No mips when it fails, debugName names it in the log
        static TextureSpecification LoadFromKTX2(GraphicsRHI* rhi, std::span<const Byte> ktx2Data, const String& debugName = "");
    };

} // namespace Surge
