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
        float PxRange;
        float LineHeight;
        Uint GlyphCount;
        Uint UncomporessedDataSize;
        Uint Width;
        Uint Height;
    };

    bool Write(const String& path, const AssetStamp& stamp, const FontSpecification& spec)
    {
        SG_ASSERT(!spec.UncompressedData.empty(), "[FontBinary] Write: UncompressedData data is empty!");

        Vector<Byte> buffer;
        buffer.reserve(sizeof(AssetStamp) + sizeof(Header) + spec.Glyphs.size() * sizeof(FontGlyph) + spec.UncompressedData.size());

        Header header {};
        header.Magic = kFontBinHeaderMagic;
        header.Version = kFontBinHeaderVersion;
        header.PxRange = spec.PxRange;
        header.LineHeight = spec.LineHeight;
        header.GlyphCount = static_cast<Uint>(spec.Glyphs.size());
        header.UncomporessedDataSize = static_cast<Uint>(spec.UncompressedData.size());
        header.Width = spec.Width;
        header.Height = spec.Height;

        WriteData(buffer, stamp);
        WriteData(buffer, header);

        for(const auto& [unicode, glyph] : spec.Glyphs)
            WriteData(buffer, glyph);

        WriteDataArray(buffer, spec.UncompressedData.data(), spec.UncompressedData.size());

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
            Log<Severity::Warn>("[FontBinary] {} version mismatch (got {}, want {})!", path, header.Version, kFontBinHeaderVersion);
            return false;
        }

        outSpec.LineHeight = header.LineHeight;
        outSpec.PxRange = header.PxRange;
        outSpec.Width = header.Width;
        outSpec.Height = header.Height;
        outSpec.Glyphs.clear();
        outSpec.Glyphs.reserve(header.GlyphCount);

        const FontGlyph* glyphArray = reinterpret_cast<const FontGlyph*>(ptr);
        for(Uint i = 0; i < header.GlyphCount; ++i)
            outSpec.Glyphs[glyphArray[i].UnicodeID] = glyphArray[i];
        ptr += header.GlyphCount * sizeof(FontGlyph);

        outSpec.UncompressedData.resize(header.UncomporessedDataSize);
        ReadDataArray(ptr, outSpec.UncompressedData.data(), header.UncomporessedDataSize);

        return ptr <= endPtr;
    }
}