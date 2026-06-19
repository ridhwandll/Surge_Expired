// Copyright (c) - SurgeTechnologies - All rights reserved
#include "ScriptCooker.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Core/Process.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Asset/Serializer/AssetStamp.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/ScriptEngine/ScriptEngine.hpp"
#include "Surge/ScriptEngine/ScriptAsset.hpp"
#include "Surge/Asset/Serializer/Script/ScriptBinaryFormat.hpp"

namespace Surge
{
    CookResult ScriptCooker::Cook(const String& sourceAbsPath, AssetID id) const
    {
        CookResult result;
        AssetManager* am = Core::GetAssetManager();
        const String outputPath = am->GetSidecarPath(id, AssetType::SCRIPT);

        String sourceCodeStr;
        if(Filesystem::ReadTextFile(sourceAbsPath, sourceCodeStr))
        {
            Vector<Byte> sourceBytecode = Core::GetScriptEngine()->Compile(sourceCodeStr);
            if (!sourceBytecode.empty())
            {
                Ref<Script> scriptAsset = Script::Create(std::move(sourceBytecode));
                ScriptBinary::Write(outputPath, AssetStampWriter::Build(sourceAbsPath, GetCookerVersion()), scriptAsset);
            }
            else
            {
                Log<Severity::Error>("[ScriptCooker] Failed to compile script: {}", sourceAbsPath);
                result.Success = false;
                return result;
            }
        }

        result.Success = true;
        result.OutputPath = outputPath;
        result.InputMegaBytes = Filesystem::FileSizeInMB(sourceAbsPath);
        result.OutputMegaBytes = Filesystem::FileSizeInMB(outputPath);
        return result;
    }
}