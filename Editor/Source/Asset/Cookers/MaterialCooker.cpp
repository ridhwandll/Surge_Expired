// Copyright (c) - SurgeTechnologies - All rights reserved
#include "MaterialCooker.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Asset/Serializer/Material/MaterialBinaryFormat.hpp"
#include "Surge/Utility/Filesystem.hpp"

#include "Asset/SourceWriters/MaterialSourceWriter.hpp"

namespace Surge
{
    CookResult MaterialCooker::Cook(const String& sourceAbsPath, AssetID id) const
    {
        AssetManager* am = Core::GetAssetManager();
        const String sidecarPath = am->GetSidecarPath(id);

        Ref<Material> material = Material::Create("MaterialCooker::Cook");
        MaterialSourceWriter::Read(material, sourceAbsPath);

        bool success = MaterialBinary::Write(sidecarPath, material);

        CookResult result;
        result.Success = success;
        result.OutputPath = sidecarPath;
        result.InputMegaBytes = Filesystem::FileSizeInMB(sourceAbsPath);
        result.OutputMegaBytes = Filesystem::FileSizeInMB(sidecarPath);
        return result;
    }
}
