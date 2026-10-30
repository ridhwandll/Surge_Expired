// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Format/AssetFormat.hpp"
#include "Surge/Graphics/HighLevel/Font.hpp"

namespace Rapt0R
{
    class Reader;
}

namespace Surge
{
    // FontBinary (.rFONT, EngineUI.sFONT) payload
    // PxRange, LineHeight, Width, Height
    // Glyphs        Uint count + (unicode, FontGlyph) pairs
    // Atlas         Uint count + the uncompressed MSDF atlas pixels
    namespace FontBinary
    {
        inline constexpr uint16_t VERSION = 1;

        void Write(Rapt0R::Writer& writer, const FontSpecification& spec);
        bool Read(Rapt0R::Reader& reader, FontSpecification& outSpec);
    }

    class FontFormat final : public AssetFormat
    {
    public:
        virtual AssetType GetType() const override { return AssetType::FONT; }
        virtual uint16_t GetFormatVersion() const override { return FontBinary::VERSION; }
        virtual Ref<Asset> Read(CookedFile& file, const AssetMetadata& metadata, const AssetContext& context) const override;
        virtual bool Write(Rapt0R::Writer& payload, const Asset& asset) const override;
    };

} // namespace Surge
