// Copyright (c) - SurgeTechnologies - All rights reserved
#include "ScriptSerializer.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Asset/Serializer/AssetStamp.hpp"
#include "Surge/Asset/Serializer/Script/ScriptBinaryFormat.hpp"
#include "Surge/ScriptEngine/ScriptAsset.hpp"
#include <Surge/Asset/Serializer/BinaryHelpers.hpp>

namespace Surge
{
    void ScriptSerializer::Initialize()
    {
        mSerializerType = AssetType::SCRIPT;
    }

    bool ScriptSerializer::Serialize(Ref<Asset> asset) const
    {
#ifdef SURGE_PLATFORM_ANDROID
        Log<Severity::Error>("[ScriptSerializer] Serialization is unsupported on Android runtime. Pre-cook the assets!");
        return false;
#else
        AssetManager* am = Core::GetAssetManager();

        SCOPED_TIMER("ScriptSerializer::Serialize");
        Ref<Script> script = asset.As<Script>();
        const String sidecarPath = am->GetSidecarPath(asset->GetID());

        AssetStamp existingStamp;
        AssetStampWriter::Read(sidecarPath, existingStamp);

        return ScriptBinary::Write(sidecarPath, existingStamp, script);
#endif
    }

    Ref<Asset> ScriptSerializer::Deserialize(const AssetMetadata& metadata) const
    {
        AssetManager* am = Core::GetAssetManager();
        const String sidecarPath = am->GetSidecarPath(metadata.ID);

        Vector<Byte> scriptCodeBuffer;
        if(Filesystem::ReadBinaryFile(sidecarPath, scriptCodeBuffer))
        {
            if(scriptCodeBuffer.size() < sizeof(AssetStamp) + sizeof(uint64_t))
            {
                Log<Severity::Error>("[ScriptSerializer] Corrupted script sidecar (Too small): {}", sidecarPath);
                return nullptr;
            }

            const Byte* ptr = scriptCodeBuffer.data();
            const Byte* endPtr = scriptCodeBuffer.data() + scriptCodeBuffer.size();

            AssetStamp stamp;
            ReadData(ptr, stamp);

            uint64_t bytecodeSize = 0;
            ReadData(ptr, bytecodeSize);

            if(ptr + bytecodeSize > endPtr)
            {
                Log<Severity::Error>("[ScriptSerializer] Corrupted script sidecar (Bytecode size mismatch): {}", sidecarPath);
                return nullptr;
            }

            Vector<Byte> bytecode(ptr, ptr + bytecodeSize);

            // Create the Script Asset and MUST assign its metadata ID
            Ref<Script> script = Script::Create(std::move(bytecode));
            return script;
        }

        Log<Severity::Error>("[ScriptSerializer] Failed to read script sidecar at path: {}", sidecarPath);
        return nullptr;
    }

    void ScriptSerializer::Shutdown()
    {
    }
}


