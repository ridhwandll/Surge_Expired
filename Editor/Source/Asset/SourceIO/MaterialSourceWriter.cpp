// Copyright (c) - SurgeTechnologies - All rights reserved
#include "MaterialSourceWriter.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include <json/json.hpp>
#include <glm/glm.hpp>
#include <SurgeReflect/Enum.hpp>
#include <Surge/Asset/AssetManager.hpp>
#include "Surge/Core/Core.hpp"

namespace Surge::MaterialSourceWriter
{
    bool Write(const Ref<Material>& mat, const String& absolutePath)
    {
        nlohmann::json j;
        j["Name"] = mat->GetName();

        const MemoryBlock& cpuData = mat->GetCPUData();
        const ShaderBuffer& reflectedBuffer = mat->GetReflectedBuffer();

        // Save the props and texture to the .smat file
        nlohmann::json& props = j["Properties"];
        for(const ShaderBufferMember& member : reflectedBuffer.Members)
        {
            switch(member.DataType)
            {
                case ShaderDataType::FLOAT:
                    props[member.Name] = cpuData.Read<float>(member.MemoryOffset);
                    break;
                case ShaderDataType::INT:
                    props[member.Name] = cpuData.Read<int>(member.MemoryOffset);
                    break;
                case ShaderDataType::FLOAT2:
                {
                    const glm::vec2 v = cpuData.Read<glm::vec2>(member.MemoryOffset);
                    props[member.Name] = { v.x, v.y };
                    break;
                }
                case ShaderDataType::FLOAT3:
                {
                    const glm::vec3 v = cpuData.Read<glm::vec3>(member.MemoryOffset);
                    props[member.Name] = { v.x, v.y, v.z };
                    break;
                }
                case ShaderDataType::FLOAT4:
                {
                    const glm::vec4 v = cpuData.Read<glm::vec4>(member.MemoryOffset);
                    props[member.Name] = { v.x, v.y, v.z, v.w };
                    break;
                }
                default:
                    Log<Severity::Warn>("[MaterialSourceWriter] Skipping '{}', unsupported type {}!", member.Name, SurgeReflect::EnumToString(member.DataType).data());
                    break;
            }
        }

        nlohmann::json& textures = j["Textures"];
        for(const auto& [name, tex] : mat->GetTextures())
        {
            if(tex.Data1) // owned Ref<Texture2D> with valid AssetID
                textures[name] = tex.Data1->GetID().Get();
        }

        return Filesystem::WriteTextFile(absolutePath, j.dump(4));
    }

    bool Read(Ref<Material>& mat, const String& absolutePath)
    {
        AssetManager* am = Core::GetAssetManager();

        String fileData;
        bool read = Filesystem::ReadTextFile(absolutePath, fileData);

        nlohmann::json j = nlohmann::json::parse(fileData, nullptr, false);
        SG_ASSERT(!j.is_discarded(), "[MaterialSourceWriter] Failed to parse JSON: {}", absolutePath);

        mat->SetName(j.value("Name", "Unnamed"));

        MemoryBlock& cpuData = mat->GetCPUData();
        const ShaderBuffer& reflectedBuffer = mat->GetReflectedBuffer();

        // Set the props and texture from the .surgemat file
        if(j.contains("Properties"))
        {
            for(const auto& [propName, propValue] : j["Properties"].items())
            {
                const ShaderBufferMember* member = reflectedBuffer.GetMember(propName);
                if(!member)
                {
                    Log<Severity::Warn>("[MaterialSourceWriter] Property '{}' not in reflection, skipping...", propName);
                    continue;
                }

                switch(member->DataType)
                {
                    case ShaderDataType::FLOAT:
                    {
                        const float val = propValue.get<float>();
                        cpuData.Write((void*)&val, sizeof(float), member->MemoryOffset);
                        break;
                    }
                    case ShaderDataType::INT:
                    {
                        const int val = propValue.get<int>();
                        cpuData.Write((void*)&val, sizeof(int), member->MemoryOffset);
                        break;
                    }
                    case ShaderDataType::FLOAT2:
                    {
                        const glm::vec2 val = { propValue[0].get<float>(), propValue[1].get<float>() };
                        cpuData.Write((void*)&val, sizeof(glm::vec2), member->MemoryOffset);
                        break;
                    }
                    case ShaderDataType::FLOAT3:
                    {
                        const glm::vec3 val = { propValue[0].get<float>(), propValue[1].get<float>(), propValue[2].get<float>() };
                        cpuData.Write((void*)&val, sizeof(glm::vec3), member->MemoryOffset);
                        break;
                    }
                    case ShaderDataType::FLOAT4:
                    {
                        const glm::vec4 val = { propValue[0].get<float>(), propValue[1].get<float>(), propValue[2].get<float>(), propValue[3].get<float>() };
                        cpuData.Write((void*)&val, sizeof(glm::vec4), member->MemoryOffset);
                        break;
                    }
                    default:
                        Log<Severity::Warn>("[MaterialSourceWriter] Skipping '{}', unsupported type {}!", propName, SurgeReflect::EnumToString(member->DataType).data());
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
                    mat->SetTexture(texName, texture);
                else
                    Log<Severity::Warn>("[MaterialSourceWriter] Texture {} (ID: {}) failed to load!", texName, id.Get());
            }
        }

        mat->MarkDirty();
        return read;
    }
}

