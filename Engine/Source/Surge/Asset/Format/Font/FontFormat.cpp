// Copyright (c) - SurgeTechnologies - All rights reserved
#include "FontFormat.hpp"
#include "Surge/Asset/AssetContext.hpp"
#include "Surge/Asset/Format/BinarySerialization.hpp"

namespace Surge
{
    SURGE_RAPTOR_RAW(FontGlyph, 40);

    namespace FontBinary
    {
        void Write(Rapt0R::Writer& writer, const FontSpecification& spec)
        {
            writer.Write(spec.PxRange, spec.LineHeight, spec.Width, spec.Height, spec.Glyphs, spec.UncompressedData);
        }

        bool Read(Rapt0R::Reader& reader, FontSpecification& outSpec)
        {
            return reader.Read(outSpec.PxRange, outSpec.LineHeight, outSpec.Width, outSpec.Height, outSpec.Glyphs, outSpec.UncompressedData) &&
                   outSpec.UncompressedData.size() == static_cast<size_t>(outSpec.Width) * outSpec.Height * 4;
        }
    }

    Ref<Asset> FontFormat::Read(CookedFile& file, const AssetMetadata& metadata, const AssetContext& context) const
    {
        FontSpecification spec;
        Rapt0R::Reader payload = file.GetPayload();
        if(!FontBinary::Read(payload, spec))
        {
            Log<Severity::Error>("[FontFormat] {} is truncated or corrupted!", metadata.RelativePath);
            return nullptr;
        }
        return Font::Create(*context.Renderer_, std::move(spec)).As<Asset>();
    }

    bool FontFormat::Write(Rapt0R::Writer& payload, const Asset& asset) const
    {
        FontBinary::Write(payload, static_cast<const Font&>(asset).GetSpecification());
        return !payload.HasError();
    }

} // namespace Surge
