// Copyright (c) - SurgeTechnologies - All rights reserved
#include "FontCooker.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Core/Process.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Asset/Format/BinarySerialization.hpp"
#include "Surge/Asset/Format/Font/FontFormat.hpp"
#include "Surge/Graphics/HighLevel/Font.hpp"
#include "AssetCooker.hpp"
#include <json/json.hpp>
#include "Texture2DCooker.hpp"

namespace Surge
{
    bool FontCooker::Cook(const CookContext& context) const
    {
        if(!CookAtlas(context.SourcePath.generic_string(), context.OutputPath.generic_string(), context.Header, 512, 48, 8.0f))
            return false;

        Log<Severity::Info>("[FontCooker] Cooked: {}", context.OutputPath.string());
        return true;
    }

    void FontCooker::CookEngineFontIfOutdated()
    {
        Vector<Byte> source;
        if(!Filesystem::ReadBinaryFile(EngineFont::SOURCE_PATH, source))
        {
            Log<Severity::Error>("[FontCooker] Engine font source {} is missing", EngineFont::SOURCE_PATH);
            return;
        }

        uint64_t hash = 14695981039346656037ull; // FNV-1a
        for(Byte byte : source)
            hash = (hash ^ byte) * 1099511628211ull;

        // A content hash instead of the write time: a fresh clone (new write times) keeps the committed file
        CookedHeader header;
        if(CookedFileIO::ReadHeader(EngineFont::COOKED_PATH, header) && header.Type == AssetType::FONT && header.FormatVersion == FontBinary::VERSION &&
           header.CookerVersion == ENGINE_FONT_VERSION && header.InputHash == hash)
            return;

        header = {};
        header.Type = AssetType::FONT;
        header.CookerVersion = ENGINE_FONT_VERSION;
        header.InputHash = hash;

        // ASCII at 32 px fits a 384 atlas (~600 KB): small glyphs, the overlay draws at 11 - 17 UI units
        Filesystem::CreateOrEnsureDirectories(Filesystem::GetParentPath(EngineFont::COOKED_PATH));
        if(CookAtlas(EngineFont::SOURCE_PATH, EngineFont::COOKED_PATH, header, 384, 32, 6.0f))
            Log<Severity::Info>("[FontCooker] Cooked the engine UI font: {} (commit it)", EngineFont::COOKED_PATH);
    }

    bool FontCooker::CookAtlas(const String& sourcePath, const String& outputPath, const CookedHeader& header, Uint atlasSize, Uint glyphSize, float pxRange)
    {
        const String msdfAtlasGenExe = FindMSDFAtlasGenExe();

        Path tempPng = outputPath + ".tmp.png";
        Path tempJson = outputPath + ".tmp.json";

        const String charsetArg = "[0x0020, 0x007E]";

        String msdfCmd = std::format(
            "\"{}\" -font \"{}\" -type msdf -dimensions {} {} -size {} -pxrange {:.1f} -kerning "
            "-chars \"{}\" -json \"{}\" -imageout \"{}\"",
            msdfAtlasGenExe, sourcePath, atlasSize, atlasSize, glyphSize, pxRange, charsetArg,
            tempJson.string(), tempPng.string());

        int exitCode = 0;
        Process::OutputOf(msdfCmd, exitCode);
        if (exitCode != 0)
        {
            Log<Severity::Error>("[FontCooker] msdf-atlas-gen.exe failed to generate atlas for {}", sourcePath);
            return false;
        }

        // Parse MSDF JSON into the Runtime Specification
        String jsonStr;
        Filesystem::ReadTextFile(tempJson.string(), jsonStr);
        nlohmann::json fontJson = nlohmann::json::parse(jsonStr);

        FontSpecification spec;
        spec.PxRange = fontJson["atlas"]["distanceRange"].get<float>();
        spec.LineHeight = fontJson["metrics"]["lineHeight"].get<float>();

        float atlasWidth = fontJson["atlas"]["width"].get<float>();
        float atlasHeight = fontJson["atlas"]["height"].get<float>();

        for(const auto& jGlyph : fontJson["glyphs"])
        {
            FontGlyph bg {};
            bg.UnicodeID = jGlyph["unicode"].get<Uint>();
            bg.Advance = jGlyph["advance"].get<float>();

            if(jGlyph.contains("planeBounds"))
            {
                bg.PlaneBounds[0] = { jGlyph["planeBounds"]["left"].get<float>(), jGlyph["planeBounds"]["bottom"].get<float>() };
                bg.PlaneBounds[1] = { jGlyph["planeBounds"]["right"].get<float>(), jGlyph["planeBounds"]["top"].get<float>() };
            }

            if(jGlyph.contains("atlasBounds"))
            {
                // Normalize UVs to 0.0 -> 1.0 Vulkan Coordinate Space
                bg.UVBounds[0] = { jGlyph["atlasBounds"]["left"].get<float>() / atlasWidth, jGlyph["atlasBounds"]["bottom"].get<float>() / atlasHeight };
                bg.UVBounds[1] = { jGlyph["atlasBounds"]["right"].get<float>() / atlasWidth, jGlyph["atlasBounds"]["top"].get<float>() / atlasHeight };
            }
            spec.Glyphs[bg.UnicodeID] = bg;

            if(fontJson.contains("kerning"))
            {
                for(const auto& jKerning : fontJson["kerning"])
                {
                    Uint u1 = jKerning["unicode1"].get<Uint>();
                    Uint u2 = jKerning["unicode2"].get<Uint>();
                    float advance = jKerning["advance"].get<float>();

                    uint64_t key = ((uint64_t)u1 << 32) | u2;
                    spec.Kerning[key] = advance;
                }
            }
        }

        TextureSpecification texSpec = Texture2DCooker::LoadFromSource(tempPng.string());
        Uint size = texSpec.Width * texSpec.Height * 4;
        spec.UncompressedData.resize(size);
        std::memcpy(spec.UncompressedData.data(), texSpec.Content, size);
        spec.Width = texSpec.Width;
        spec.Height = texSpec.Height;

        // Pack runtime file
        Rapt0R::Writer payload(sizeof(FontGlyph) * spec.Glyphs.size() * 2 + spec.UncompressedData.size() + 256);
        FontBinary::Write(payload, spec);
        const bool written = !payload.HasError() && CookedFileIO::Write(outputPath, header, FontBinary::VERSION, payload.WrittenSpan());

        Texture2DCooker::FreeLoadedSource(texSpec);
        Filesystem::RemoveFile(tempJson);
        Filesystem::RemoveFile(tempPng);

        if(!written)
            Log<Severity::Error>("[FontCooker] Could not write {}", outputPath);
        return written;
    }

    String FontCooker::FindMSDFAtlasGenExe()
    {
        const String msdfAtlasGenPath = "Editor/Tools/msdf-atlas-gen.exe";
        if(Filesystem::Exists(msdfAtlasGenPath))
            return msdfAtlasGenPath;

        Log<Severity::Error>("[FontCooker] msdf-atlas-gen.exe not found at {}", msdfAtlasGenPath);
        return "msdf-atlas-gen.exe";
    }
}