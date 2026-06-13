// Copyright (c) - SurgeTechnologies - All rights reserved
#include "MeshSerializer.hpp"
#include "Surge/Core/Core.hpp"
#include "MeshBinaryFormat.hpp"

namespace Surge
{
    static bool CheckAndGenerateDefaultMesh(const String& filepath, MeshSpecification& outSpec)
    {
        MeshGenerator::MeshData meshData = MeshGenerator::GenerateDefaultMesh(filepath);
        if(meshData.Vertices.empty())
            return false;

        outSpec.Vertices = std::move(meshData.Vertices);
        outSpec.Indices = std::move(meshData.Indices);

        outSpec.Submeshes.emplace_back();
        Submesh& submesh = outSpec.Submeshes.back();
        submesh.BaseVertex = 0;
        submesh.BaseIndex = 0;
        submesh.MaterialIndex = 0;

        submesh.VertexCount = (Uint)outSpec.Vertices.size();
        submesh.IndexCount = (Uint)outSpec.Indices.size() * 3;

        submesh.Transform = glm::mat4(1.0f);
        submesh.LocalTransform = glm::mat4(1.0f);
        submesh.MeshName = "DefaultMesh";
        submesh.NodeName = "DefaultMesh_Node";

        submesh.BoundingBox.Reset();
        glm::vec3 minBound(std::numeric_limits<float>::max());
        glm::vec3 maxBound(-std::numeric_limits<float>::max());
        for(const auto& v : outSpec.Vertices)
        {
            minBound = glm::min(minBound, v.Position);
            maxBound = glm::max(maxBound, v.Position);
        }
        submesh.BoundingBox.Min = minBound;
        submesh.BoundingBox.Max = maxBound;

        outSpec.Materials.emplace_back(Material::Create("DefaultMaterial"));
        outSpec.Materials[0]->SetName("DefaultMaterial");
        outSpec.Materials[0]->Set<glm::vec3>("Albedo", glm::vec3(0.8f));
        outSpec.Materials[0]->Set<float>("Metallic", 0.5f);
        outSpec.Materials[0]->Set<float>("Roughness", 0.5f);
        outSpec.Materials[0]->Set<float>("Reflectance", 0.5f);
        return true;
    }

    // 
    // Mesh Serializer
    // 

    void MeshSerializer::Initialize()
    {
        mSerializerType = AssetType::MESH;
    }

    bool MeshSerializer::Serialize(Ref<Asset> asset) const
    {
#ifdef SURGE_PLATFORM_ANDROID
        Log<Severity::Error>("[MeshSerializer] Serialization is unsupported on Android runtime. Pre-cook the assets!");
        return false;
#else
        AssetManager* am = Core::GetAssetManager();
        const AssetMetadata& meta = am->GetMetadata(asset->GetID());
        if(HasFlag(meta.Flags, AssetFlags::MEMORY))
            return true;

        SCOPED_TIMER("MeshSerializer::Serialize");
        Ref<Mesh> mesh = asset.As<Mesh>();
        const String sidecarPath = am->GetSidecarPath(meta.ID);

        AssetStamp existingStamp;
        MeshSpecification existingGeom;
        bool result = MeshBinary::Read(sidecarPath, existingStamp, existingGeom);

        MeshBinary::Write(sidecarPath, existingStamp, existingGeom, mesh->GetMaterialOverrides()); //Just override the materials
        return result;
#endif
    }

    Ref<Asset> MeshSerializer::Deserialize(const AssetMetadata& metadata) const
    {
        MeshSpecification spec;
        if(HasFlag(metadata.Flags, AssetFlags::MEMORY))
        {
            CheckAndGenerateDefaultMesh(metadata.RelativePath, spec);
            return Mesh::Create(std::move(spec)).As<Asset>();
        }

        AssetManager* am = Core::GetAssetManager();
        const String sidecarPath = am->GetSidecarPath(metadata.ID);

        SCOPED_TIMER("MeshSerializer::Deserialize: {}", sidecarPath);
        AssetStamp existingStamp;
        if(!MeshBinary::Read(sidecarPath, existingStamp, spec))
        {
            Log<Severity::Error>("[MeshSerializer] Failed to read sidecar for mesh {}, returing a default sweet cube!", metadata.RelativePath);
            CheckAndGenerateDefaultMesh(DefaultMesh::CUBE, spec);
        }
        return Mesh::Create(std::move(spec)).As<Asset>();
    }

    void MeshSerializer::Shutdown()
    {
        Log<Severity::Info>("[MeshSerializer] Shutdown");
    }
}