// Copyright (c) - SurgeTechnologies - All rights reserved
#include "FontBinaryFormat.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Asset/Serializer/BinaryHelpers.hpp"

namespace Surge::FontBinary
{
    static constexpr Uint kFontBinHeaderMagic = 0x46444952; // RIDF
    static constexpr Uint kFontBinHeaderVersion = 1;
    struct Header
    {
        Uint Magic;       // RIDF
        Uint Version;     // 1
        float LineHeight;
        Uint GlyphCount;
        Uint Ktx2DataSize;
    };

    bool Write(const String& path, const AssetStamp& stamp, const FontSpecification& spec)
    {
        SG_ASSERT(!spec.KTX2Data.empty(), "[FontBinary] Write: KTX2 data is empty!");

        Vector<Byte> buffer;
        buffer.reserve(sizeof(AssetStamp) + sizeof(Header) + spec.Glyphs.size() * sizeof(FontGlyph) + spec.KTX2Data.size());

        Header header {};
        header.Magic = kFontBinHeaderMagic;
        header.Version = kFontBinHeaderVersion;
        header.LineHeight = spec.LineHeight;
        header.GlyphCount = static_cast<Uint>(spec.Glyphs.size());
        header.Ktx2DataSize = static_cast<Uint>(spec.KTX2Data.size());

        WriteData(buffer, stamp);
        WriteData(buffer, header);

        for(const auto& [unicode, glyph] : spec.Glyphs)
            WriteData(buffer, glyph);

        WriteDataArray(buffer, spec.KTX2Data.data(), spec.KTX2Data.size());

        if (!Filesystem::WriteBinaryFile(path, buffer.data(), buffer.size()))
            return false;

        return true;
    }

    bool Read(const String& path, AssetStamp& outstamp, FontSpecification& outSpec)
    {
        Vector<Byte> fileData;
        if(!Filesystem::ReadBinaryFile(path, fileData))
            return false;

        const Byte* ptr = fileData.data();
        const Byte* endPtr = fileData.data() + fileData.size();

        if(fileData.size() < sizeof(AssetStamp) + sizeof(Header))
            return false;

        ReadData(ptr, outstamp);

        Header header = {};
        ReadData(ptr, header);

        if(header.Magic != kFontBinHeaderMagic)
        {
            Log<Severity::Warn>("[FontBinary] {} has bad magic, ignoring file!", path);
            return false;
        }
        if(header.Version != kFontBinHeaderVersion)
        {
            Log<Severity::Warn>("[FontBinary] {} version mismatch (got {}, want {}), re-cooking!", path, header.Version, kFontBinHeaderVersion);
            return false;
        }

        outSpec.LineHeight = header.LineHeight;
        outSpec.Glyphs.clear();
        outSpec.Glyphs.reserve(header.GlyphCount);

        const FontGlyph* glyphArray = reinterpret_cast<const FontGlyph*>(ptr);
        for(Uint i = 0; i < header.GlyphCount; ++i)
            outSpec.Glyphs[glyphArray[i].UnicodeID] = glyphArray[i];
        ptr += header.GlyphCount * sizeof(FontGlyph);

        outSpec.KTX2Data.resize(header.Ktx2DataSize);
        ReadDataArray(ptr, outSpec.KTX2Data.data(), header.Ktx2DataSize);

        return ptr <= endPtr;
    }
}