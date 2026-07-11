// Copyright (c) - SurgeTechnologies - All rights reserved
#include "AudioCooker.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Core/Core.hpp"

namespace Surge
{
    CookResult AudioCooker::Cook(const String& sourceAbsPath, AssetID id) const
    {
        CookResult result;
        AssetManager* am = Core::GetAssetManager();
        const String outputPath = am->GetSidecarPath(id, AssetType::AUDIO);

        // Audio assets are just the raw file copied to internal directory, no processing is done here
        // The AudioEngine will handle the actual decoding of the audio file at runtime
        Filesystem::CopyFile(sourceAbsPath, outputPath);

        result.Success = true;
        result.OutputPath = outputPath;
        result.InputMegaBytes = Filesystem::FileSizeInMB(sourceAbsPath);
        result.OutputMegaBytes = Filesystem::FileSizeInMB(outputPath);
        return result;
    }
}