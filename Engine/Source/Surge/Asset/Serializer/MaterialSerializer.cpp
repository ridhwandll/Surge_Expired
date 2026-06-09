// Copyright (c) - SurgeTechnologies - All rights reserved
#include "MaterialSerializer.hpp"
#include "Surge/Core/Core.hpp"
#include <json/json.hpp>


#ifdef SURGE_PLATFORM_ANDROID
#include "Surge/Platform/Android/AndroidApp.hpp"
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <android/asset_manager.h>
#else
#include <fstream>
#endif

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

        std::ofstream file(absolutePath, std::ios::out | std::ios::trunc);
        SG_ASSERT(file.is_open(), "[MaterialSerializer] Failed to write: '{}'", absolutePath);
        file << j.dump(4);
        return true;
#endif
    }

    Ref<Asset> MaterialSerializer::Deserialize(const AssetMetadata& metadata) const
    {
        AssetManager* am = Core::GetAssetManager();
        const String absolutePath = am->GetAbsolutePath(metadata.RelativePath);

        nlohmann::json j;
#ifdef SURGE_PLATFORM_ANDROID
        android_app* app = Android::GAndroidApp;
        AAssetManager* androidAssetMgr = app->activity->assetManager;

        AAsset* asset = AAssetManager_open(androidAssetMgr, absolutePath.c_str(), AASSET_MODE_BUFFER);
        SG_ASSERT(asset, "[MaterialSerializer] Failed to open: '{}'", absolutePath);
        if(!asset)
            return nullptr;

        size_t size = AAsset_getLength(asset);
        String fileData(size, '\0');
        AAsset_read(asset, fileData.data(), size);
        AAsset_close(asset);

        j = nlohmann::json::parse(fileData, nullptr, false);
#else
        std::ifstream file(absolutePath);
        SG_ASSERT(file.is_open(), "[MaterialSerializer] Failed to open: '{}'", absolutePath);

        j = nlohmann::json::parse(file, nullptr, false);
#endif

        SG_ASSERT(!j.is_discarded(), "[MaterialSerializer] Failed to parse JSON: '{}'", absolutePath);

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
}


