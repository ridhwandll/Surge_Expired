// Copyright (c) - SurgeTechnologies - All rights reserved
#include "AssetCooker/Cookers/Texture2DCooker.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Core/Process.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Core/Core.hpp"

namespace Surge
{
    CookResult Texture2DCooker::Cook(const String& sourceAbsPath, AssetID id) const
    {
        AssetManager* am = Core::GetAssetManager();
        CookResult result;
        const String exe = FindBasisuExe();
        const String outputPath = am->GetSidecarPath(id);
        const bool linear = IsLinearColorSpace(sourceAbsPath);

        // Build basisu.exe arguments
        // -ktx2        KTX2 output container
        // -uastc       UASTC supercompression (runtime transcodes to ASTC/BC7/RGBA)
        // -uastc_level 2  quality 0–4, 2 is a good balance for game content
        // -mipmap      bake all mip levels into the file
        // -linear      omit for sRGB (albedo/emissive), include for linear (normals/roughness)
        String args;
        args += "-ktx2 -uastc -uastc_level 2 -mipmap ";
        linear ? args += "-linear " : args += "-srgb ";
        args += "-output_file \"" + outputPath + "\" ";
        args += "\"" + sourceAbsPath + "\"";
        const String fullCmd = "\"" + exe + "\" " + args;

        int exitCode = 0;
        Process::OutputOf(fullCmd, exitCode);
        if(exitCode != 0)
        {
            Log<Severity::Error>("[Texture2DCooker] Failed to cook '{}'. Command: {}", sourceAbsPath, fullCmd);
            result.Success = false;
            return result;
        }

        result.Success = true;
        result.OutputPath = outputPath;
        result.InputMegaBytes = (float)(std::filesystem::file_size(sourceAbsPath) / (1000.0 * 1000.0));
        result.OutputMegaBytes = (float)(std::filesystem::file_size(outputPath) / (1000.0 * 1000.0));

        Log<Severity::Info>("[Texture2DCooker] Cooked: '{}'", outputPath);
        return result;
    }

    bool Texture2DCooker::NeedsCook(AssetID id) const
    {
        AssetManager* am = Core::GetAssetManager();
        return !Filesystem::Exists(am->GetSidecarPath(id));
    }

    String Texture2DCooker::FindBasisuExe()
    {
        const String basisuPath = "Engine/Tools/basisu.exe";
        if(Filesystem::Exists(basisuPath))
            return basisuPath;

        Log<Severity::Error>("Basisu executable not found at {}", basisuPath);
        return "basisu";
    }

    bool Texture2DCooker::IsLinearColorSpace(const String& sourceAbsPath)
    {
        // Take from filename linear: normal maps, roughness, metallic, AO
        const String name = Filesystem::GetFilenameWithoutExt(sourceAbsPath);

        String lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        return lower.find("normal") != String::npos || lower.find("roughness") != String::npos || lower.find("metallic") != String::npos ||
            lower.find("metalness") != String::npos || lower.find("_ao") != String::npos || lower.find("occlusion") != String::npos ||
            lower.find("_n.") != String::npos || lower.find("_r.") != String::npos || lower.find("_m.") != String::npos || lower.find("_orm.") != String::npos;

    }
}