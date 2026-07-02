// Copyright (c) - SurgeTechnologies - All rights reserved
#include "FontCooker.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Core/Process.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Asset/Serializer/AssetStamp.hpp"
#include "Surge/Asset/Serializer/Font/FontBinaryFormat.hpp"
#include "Surge/Graphics/HighLevel/Font.hpp"
#include "AssetCooker.hpp"
#include <json/json.hpp>
#include "Texture2DCooker.hpp"

namespace Surge
{
    CookResult FontCooker::Cook(const String& sourceAbsPath, AssetID id) const
    {
        AssetManager* am = Core::GetAssetManager();
        CookResult result;
        const String msdfAtlasGenExe = FindMSDFAtlasGenExe();
        const String outputPath = am->GetSidecarPath(id);

        Path tempPng = sourceAbsPath + ".tmp.png";
        Path tempJson = sourceAbsPath + ".tmp.json";
        Path tempKtx2 = sourceAbsPath + ".tmp.ktx2";

        const String charsetArg = "[0x0020, 0x007E]";

        String msdfCmd = std::format(
            "\"{}\" -font \"{}\" -type msdf -dimensions 512 512 -size 48 -pxrange 8.0 -kerning "
            "-chars \"{}\" -json \"{}\" -imageout \"{}\"",
            msdfAtlasGenExe, sourceAbsPath, charsetArg,
            tempJson.string(), tempPng.string());

        int exitCode = 0;
        Process::OutputOf(msdfCmd, exitCode);
        if (exitCode != 0)
        {
            Log<Severity::Error>("[FontCooker] msdf-atlas-gen.exe failed to generate atlas for {}", sourceAbsPath);
            result.Success = false;
            return result;
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
        AssetStamp stamp = AssetStampWriter::Build(sourceAbsPath, GetCookerVersion());
        FontBinary::Write(outputPath, stamp, spec);

        Texture2DCooker::FreeLoadedSource(texSpec);
        Filesystem::RemoveFile(tempJson);
        Filesystem::RemoveFile(tempPng);
        Filesystem::RemoveFile(tempKtx2);

        result.Success = true;
        result.OutputPath = outputPath;
        result.InputMegaBytes = Filesystem::FileSizeInMB(sourceAbsPath);
        result.OutputMegaBytes = Filesystem::FileSizeInMB(outputPath);

        Log<Severity::Info>("[FontCooker] Cooked: {}", outputPath);
        return result;
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