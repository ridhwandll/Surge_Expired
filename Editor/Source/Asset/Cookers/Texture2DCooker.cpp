// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Texture2DCooker.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Core/Process.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Asset/Serializer/AssetStamp.hpp"
#include "Surge/Core/Core.hpp"

#include "AssetCooker.hpp"
#include <stb_image.h>

namespace Surge
{
    CookResult Texture2DCooker::Cook(const String& sourceAbsPath, AssetID id) const
    {
        AssetManager* am = Core::GetAssetManager();
        CookResult result;
        const String exe = FindBasisuExe();
        const String outputPath = am->GetSidecarPath(id);
        const bool linear = IsLinearColorSpace(sourceAbsPath);

        const String tmpPath = outputPath + ".tmp";
        // Build basisu.exe arguments
        // -ktx2        KTX2 output container
        // -uastc       UASTC supercompression (runtime transcodes to ASTC/BC7/RGBA)
        // -uastc_level 2  quality 0–4, 2 is a good balance for game content
        // -mipmap      bake all mip levels into the file
        // -linear      omit for sRGB (albedo/emissive), include for linear (normals/roughness)
        String args;
        args += "-ktx2 -uastc -uastc_level 2 -mipmap ";
        linear ? args += "-linear " : args += "-srgb ";
        args += "-output_file \"" + tmpPath + "\" ";
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

        Vector<Byte> ktx2Data;
        if(!Filesystem::ReadBinaryFile(tmpPath, ktx2Data))
        {
            result.Success = false;
            return result;
        }
        Filesystem::RemoveFile(tmpPath);

        // Write the asset stamp + ktx2 data to the sidecar
        const AssetStamp stamp = AssetStampWriter::Build(sourceAbsPath, GetCookerVersion());
        Vector<Byte> final;
        final.resize(sizeof(AssetStamp) + ktx2Data.size());
        memcpy(final.data(), &stamp, sizeof(AssetStamp));
        memcpy(final.data() + sizeof(AssetStamp), ktx2Data.data(), ktx2Data.size());
        Filesystem::WriteBinaryFile(outputPath, final.data(), final.size());

        result.Success = true;
        result.OutputPath = outputPath;
        result.InputMegaBytes = Filesystem::FileSizeInMB(sourceAbsPath);
        result.OutputMegaBytes = Filesystem::FileSizeInMB(outputPath);

        Log<Severity::Info>("[Texture2DCooker] Cooked: {}", outputPath);
        return result;
    }

    String Texture2DCooker::FindBasisuExe()
    {
        const String basisuPath = "Editor/Tools/basisu.exe";
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

    //Ref<Asset> Texture2DCooker::LoadFromSource(const String& absPath) const
    //{
    //    Vector<uint8_t> fileData;
    //    if(!Filesystem::ReadBinaryFile(absPath, fileData))
    //    {
    //        Log<Severity::Error>("[Texture2DSerializer::LoadFromSource] Failed to read binary file at path: {0}", absPath);
    //        return nullptr;
    //    }
    //
    //    int width = 0, height = 0, channels = 0;
    //    stbi_uc* data = stbi_load_from_memory(fileData.data(), static_cast<int>(fileData.size()), &width, &height, &channels, 4);
    //
    //    if(!data)
    //        return nullptr;
    //
    //    TextureSpecification spec;
    //    spec.Format = ImageFormat::RGBA8_SRGB;
    //    spec.DebugName = Filesystem::GetFilenameWithExt(absPath);
    //    spec.Width = static_cast<Uint>(width);
    //    spec.Height = static_cast<Uint>(height);
    //    spec.Content = data;
    //    spec.GenerateMips = true;
    //    Ref<Texture2D> texture = Texture2D::Create(spec);
    //
    //    stbi_image_free(data);
    //
    //    Log<Severity::Trace>("[Texture2DSerializer] Created Texture2D form SOURCE {}", Filesystem::GetFilenameWithExt(absPath));
    //    return texture.As<Asset>();
    //}
}