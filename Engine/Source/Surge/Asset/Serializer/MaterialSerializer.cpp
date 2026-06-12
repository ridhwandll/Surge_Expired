// Copyright (c) - SurgeTechnologies - All rights reserved
#include "MaterialSerializer.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Asset/Serializer/BinaryHelpers.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include <json/json.hpp>

namespace Surge
{
    void MaterialSerializer::Initialize()
    {
        mSerializerType = AssetType::MATERIAL;
    }

    bool MaterialSerializer::Serialize(Ref<Asset> asset) const
    {
#ifdef SURGE_PLATFORM_ANDROID
        Log<Severity::Error>("[MaterialSerializer] Serialization is unsupported on Android runtime. Pre-cook the assets!");
        return false;
#else
        AssetManager* am = Core::GetAssetManager();
        const AssetMetadata& meta = am->GetMetadata(asset->GetID());
        const String absolutePath = am->GetAbsolutePath(meta.RelativePath);

        Ref<Material> mat = asset.As<Material>();

        nlohmann::json j;
        j["Name"] = mat->mName;
        j["Shader"] = mat->mShaderName;

        // Save the props and texture to the .surgemat file
        nlohmann::json& props = j["Properties"];
        for(const ShaderBufferMember& member : mat->mRefletedBuffer.Members)
        {
            switch(member.DataType)
            {
                case ShaderDataType::FLOAT:
                    props[member.Name] = mat->mCPUData.Read<float>(member.MemoryOffset);
                    break;
                case ShaderDataType::INT:
                    props[member.Name] = mat->mCPUData.Read<int>(member.MemoryOffset);
                    break;
                case ShaderDataType::FLOAT2:
                {
                    const glm::vec2 v = mat->mCPUData.Read<glm::vec2>(member.MemoryOffset);
                    props[member.Name] = { v.x, v.y };
                    break;
                }
                case ShaderDataType::FLOAT3:
                {
                    const glm::vec3 v = mat->mCPUData.Read<glm::vec3>(member.MemoryOffset);
                    props[member.Name] = { v.x, v.y, v.z };
                    break;
                }
                case ShaderDataType::FLOAT4:
                {
                    const glm::vec4 v = mat->mCPUData.Read<glm::vec4>(member.MemoryOffset);
                    props[member.Name] = { v.x, v.y, v.z, v.w };
                    break;
                }
                default:
                    Log<Severity::Warn>("[MaterialSerializer] Skipping '{}', unsupported type {}!", member.Name, SurgeReflect::EnumToString(member.DataType).data());
                    break;
            }
        }

        nlohmann::json& textures = j["Textures"];
        for(const auto& [name, tex] : mat->mTextures)
        {
            if(tex.Data1) // owned Ref<Texture2D> with valid AssetID
                textures[name] = tex.Data1->GetID().Get();
        }

        Filesystem::WriteTextFile(absolutePath, j.dump(4));
        return true;
#endif
    }

    Ref<Asset> MaterialSerializer::Deserialize(const AssetMetadata& metadata) const
    {
        AssetManager* am = Core::GetAssetManager();
        const String absolutePath = am->GetAbsolutePath(metadata.RelativePath);
        String fileData;
        Filesystem::ReadTextFile(absolutePath, fileData);

        nlohmann::json j = nlohmann::json::parse(fileData, nullptr, false);
        SG_ASSERT(!j.is_discarded(), "[MaterialSerializer] Failed to parse JSON: {}", absolutePath);

        Ref<Material> material = Material::Create(j.value("Name", "Unnamed"));

        // Set the props and texture from the .surgemat file
        if(j.contains("Properties"))
        {
            for(const auto& [propName, propValue] : j["Properties"].items())
            {
                const ShaderBufferMember* member = material->mRefletedBuffer.GetMember(propName);
                if(!member)
                {
                    Log<Severity::Warn>("[MaterialSerializer] Property '{}' not in reflection, skipping...", propName);
                    continue;
                }

                switch(member->DataType)
                {
                    case ShaderDataType::FLOAT:
                    {
                        const float val = propValue.get<float>();
                        material->mCPUData.Write((void*)&val, sizeof(float), member->MemoryOffset);
                        break;
                    }
                    case ShaderDataType::INT:
                    {
                        const int val = propValue.get<int>();
                        material->mCPUData.Write((void*)&val, sizeof(int), member->MemoryOffset);
                        break;
                    }
                    case ShaderDataType::FLOAT2:
                    {
                        const glm::vec2 val = { propValue[0].get<float>(), propValue[1].get<float>() };
                        material->mCPUData.Write((void*)&val, sizeof(glm::vec2), member->MemoryOffset);
                        break;
                    }
                    case ShaderDataType::FLOAT3:
                    {
                        const glm::vec3 val = { propValue[0].get<float>(), propValue[1].get<float>(), propValue[2].get<float>() };
                        material->mCPUData.Write((void*)&val, sizeof(glm::vec3), member->MemoryOffset);
                        break;
                    }
                    case ShaderDataType::FLOAT4:
                    {
                        const glm::vec4 val = { propValue[0].get<float>(), propValue[1].get<float>(), propValue[2].get<float>(), propValue[3].get<float>() };
                        material->mCPUData.Write((void*)&val, sizeof(glm::vec4), member->MemoryOffset);
                        break;
                    }
                    default:
                        Log<Severity::Warn>("[MaterialSerializer] Skipping '{}', unsupported type {}!", propName, SurgeReflect::EnumToString(member->DataType).data());
                        break;
                }
            }
        }

        if(j.contains("Textures"))
        {
            for(const auto& [texName, texIDVal] : j["Textures"].items())
            {
                const AssetID id = texIDVal.get<uint64_t>();
                Ref<Texture2D> texture = am->Load<Texture2D>(id);
                if(texture)
                    material->SetTexture(texName, texture);
                else
                    Log<Severity::Warn>("[MaterialSerializer] Texture '{}' (ID: {}) failed to load!", texName, id.Get());
            }
        }

        material->MarkDirty();
        return material.As<Asset>();
    }

    void MaterialSerializer::Shutdown()
    {
        Log<Severity::Info>("[MaterialSerializer] Shutdown");
    }

    Uint MaterialSerializer::CalculateMaterialSize(const Ref<Material>& mat)
    {
        Uint size = 1; // hasData boolean flag (1 byte)
        if(!mat)
            return size;

        // Material Name (Uint length + string chars)
        size += sizeof(Uint) + static_cast<Uint>(mat->mName.size());

        // UBO / CPU Data (Uint length + buffer bytes)
        size += sizeof(Uint) + static_cast<Uint>(mat->mCPUData.GetSize());

        // Textures
        size += sizeof(Uint); // texCount
        for(const auto& [name, tex] : mat->mTextures)
        {
            if(tex.Data1 && tex.Data1->GetID().IsValid())
            {
                size += sizeof(Uint) + static_cast<Uint>(name.size()); // String length + chars
                size += sizeof(uint64_t); // rawID
            }
        }
        return size;
    }

    void MaterialSerializer::WriteTransientMaterial(Vector<Byte>& buffer, const Ref<Material>& mat)
    {
        const Byte hasData = mat ? 1 : 0;
        WriteData(buffer, hasData);

        if(!hasData)
            return;

        WriteStr(buffer, mat->mName);

        // Raw CPU buffer direct UBO layout, ready for GPU upload
        const Uint propsSize = static_cast<Uint>(mat->mCPUData.GetSize());
        WriteData(buffer, propsSize);
        WriteDataArray(buffer, mat->mCPUData.As<Byte>(), propsSize);

        // Only owned Ref<Texture2D> slots with a valid AssetID
        Uint texCount = 0;
        for(const auto& [name, tex] : mat->mTextures)
        {
            if(tex.Data1 && tex.Data1->GetID().IsValid())
                texCount++;
        }

        WriteData(buffer, texCount);
        for(const auto& [name, tex] : mat->mTextures)
        {
            if(!tex.Data1 || !tex.Data1->GetID().IsValid())
                continue;

            WriteStr(buffer, name);
            const uint64_t rawID = tex.Data1->GetID().Get();
            WriteData(buffer, rawID);
        }
    }

    Ref<Material> MaterialSerializer::ReadTransientMaterial(const Byte*& ptr)
    {
        Byte hasData = 0;
        ReadData(ptr, hasData);
        if(!hasData)
            return nullptr;

        const String name = ReadStr(ptr);

        Uint propsSize = 0;
        ReadData(ptr, propsSize);

        // Create with hardcoded pipeline/shader allocates CPU buffer via shader reflection
        Ref<Material> mat = Material::Create(name);

        SG_ASSERT(propsSize == static_cast<Uint>(mat->mCPUData.GetSize()),
                  "[MeshSerializer] Inline material {} props size mismatch ({} vs {}). Shader UBO layout changed, delete sidecar and reimport",
                  name, propsSize, static_cast<Uint>(mat->mCPUData.GetSize()));

        // Overwrite CPU buffer with stored UBO data
        ReadDataArray(ptr, mat->mCPUData.As<Byte>(), propsSize);

        Uint texCount = 0;
        ReadData(ptr, texCount);

        AssetManager* am = Core::GetAssetManager();
        for(Uint i = 0; i < texCount; i++)
        {
            const String texName = ReadStr(ptr);
            uint64_t rawID = 0;
            ReadData(ptr, rawID);

            Ref<Texture2D> tex = am->Load<Texture2D>(AssetID(rawID));
            if(tex)
                mat->SetTexture(texName, tex);
            else
                Log<Severity::Warn>("[MeshSerializer] Material '{}': texture ID {} failed to load.", name, rawID);
        }

        mat->MarkDirty();
        return mat;
    }
}


