// Copyright (c) - SurgeTechnologies - All rights reserved
#include "MeshSerializer.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Utility/Filesystem.hpp"

#include <cgltf.h>
#include <glm/gtc/type_ptr.hpp>
#include <cstring> // For memcpy

namespace Surge
{
    // [Header: 28 bytes]
    //     Magic            uint32  "RIDM"
    //     Version          uint32  1
    //     VertexCount      uint32
    //     IndexCount       uint32
    //     SubmeshCount     uint32
    //     ValidOverrides   uint32  sparse count
    //     GeomSectionSize  uint32  bytes from end of header to end of submesh section

    static_assert(std::is_trivially_copyable_v<Vertex>, "Vertex must be trivially copyable for binary sidecar serialization");
    static_assert(std::is_trivially_copyable_v<Index>, "Index must be trivially copyable for binary sidecar serialization");
    static constexpr Uint kSidecarMagic = 0x4D444952; // RIDM
    static constexpr Uint kSidecarVersion = 1;

    struct SurgeMeshHeader
    {
        Uint Magic;
        Uint Version;
        Uint VertexCount;
        Uint IndexCount;
        Uint SubmeshCount;
        Uint ValidOverrideCount;
        Uint GeomSectionSize;
    };
    static_assert(sizeof(SurgeMeshHeader) == 28);

    //
    // Static helpers
    //

    static String GetSidecarPath(const String& meshAbsPath)
    {
        return Filesystem::ReplaceExtension(meshAbsPath, ".RAsset").string();
    }

    // Binary Buffer Writers
    template <typename T>
    static void WriteData(Vector<Byte>& buffer, const T& data)
    {
        const Byte* ptr = reinterpret_cast<const Byte*>(&data);
        buffer.insert(buffer.end(), ptr, ptr + sizeof(T));
    }

    template <typename T>
    static void WriteDataArray(Vector<Byte>& buffer, const T* data, size_t count)
    {
        if(count == 0) return;
        const Byte* ptr = reinterpret_cast<const Byte*>(data);
        buffer.insert(buffer.end(), ptr, ptr + (sizeof(T) * count));
    }

    static void WriteStr(Vector<Byte>& buffer, const String& s)
    {
        Uint len = static_cast<Uint>(s.size());
        WriteData(buffer, len);
        WriteDataArray(buffer, s.data(), s.size());
    }

    // Binary Buffer Readers
    template <typename T>
    static void ReadData(const Byte*& ptr, T& data)
    {
        memcpy(&data, ptr, sizeof(T));
        ptr += sizeof(T);
    }

    template <typename T>
    static void ReadDataArray(const Byte*& ptr, T* data, size_t count)
    {
        if(count == 0) return;
        memcpy(data, ptr, sizeof(T) * count);
        ptr += sizeof(T) * count;
    }

    static String ReadStr(const Byte*& ptr)
    {
        Uint len = 0;
        ReadData(ptr, len);
        String s(len, '\0');
        if(len > 0)
            ReadDataArray(ptr, s.data(), len);
        return s;
    }

    static cgltf_data* ParseGLTF(const String& filepath)
    {
        cgltf_options options = {};

        options.file.read = [](const cgltf_memory_options*, const cgltf_file_options*, const char* path, cgltf_size* size, void** data) -> cgltf_result {
            Vector<Byte> buffer;
            if(!Filesystem::ReadBinaryFile(path, buffer))
                return cgltf_result_file_not_found;

            *size = buffer.size();
            *data = malloc(*size);
            if(!*data)
                return cgltf_result_out_of_memory;

            memcpy(*data, buffer.data(), *size);
            return cgltf_result_success;
            };

        options.file.release = [](const cgltf_memory_options*, const cgltf_file_options*, void* data, cgltf_size) { free(data); };

        cgltf_data* data = nullptr;
        if(cgltf_parse_file(&options, filepath.c_str(), &data) != cgltf_result_success)
        {
            Log<Severity::Error>("[MeshSerializer] Failed to parse glTF: {}", filepath);
            return nullptr;
        }
        if(cgltf_load_buffers(&options, data, filepath.c_str()) != cgltf_result_success)
        {
            Log<Severity::Error>("[MeshSerializer] Failed to load buffers: {}", filepath);
            cgltf_free(data);
            return nullptr;
        }
        return data;
    }

    static void FreeGLTF(cgltf_data* data)
    {
        cgltf_free(data);
    }

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

    static MeshSpecification ExtractGeometry(cgltf_data* data, MeshSpecification& outSpec)
    {
        Vector<Uint> meshSubmeshStart(data->meshes_count);
        Uint totalSubmeshes = 0;
        for(size_t i = 0; i < data->meshes_count; i++)
        {
            meshSubmeshStart[i] = totalSubmeshes;
            totalSubmeshes += (Uint)data->meshes[i].primitives_count;
        }
        outSpec.Submeshes.reserve(totalSubmeshes);

        Uint vertexCount = 0;
        Uint indexCount = 0;

        for(size_t m = 0; m < data->meshes_count; m++)
        {
            cgltf_mesh& mesh = data->meshes[m];
            for(size_t p = 0; p < mesh.primitives_count; p++)
            {
                cgltf_primitive& prim = mesh.primitives[p];
                SG_ASSERT(prim.type == cgltf_primitive_type_triangles, "Only triangle primitives supported");
                SG_ASSERT(prim.indices, "Mesh primitive requires indices");

                cgltf_accessor* posAccessor = nullptr;
                cgltf_accessor* normalAccessor = nullptr;
                cgltf_accessor* tangentAccessor = nullptr;
                cgltf_accessor* texcoordAccessor = nullptr;

                for(size_t a = 0; a < prim.attributes_count; a++)
                {
                    cgltf_attribute& attr = prim.attributes[a];
                    switch(attr.type)
                    {
                        case cgltf_attribute_type_position: posAccessor = attr.data; break;
                        case cgltf_attribute_type_normal: normalAccessor = attr.data; break;
                        case cgltf_attribute_type_tangent: tangentAccessor = attr.data; break;
                        case cgltf_attribute_type_texcoord:
                            if(attr.index == 0) texcoordAccessor = attr.data;
                            break;
                        default: break;
                    }
                }

                SG_ASSERT(posAccessor, "Mesh primitive requires positions");
                SG_ASSERT(normalAccessor, "Mesh primitive requires normals");

                outSpec.Submeshes.emplace_back();
                Submesh& submesh = outSpec.Submeshes.back();
                submesh.BaseVertex = vertexCount;
                submesh.BaseIndex = indexCount;
                submesh.VertexCount = (Uint)posAccessor->count;
                submesh.IndexCount = (Uint)prim.indices->count;
                submesh.MaterialIndex = prim.material ? (Uint)(prim.material - data->materials) : 0;
                submesh.MeshName = mesh.name ? mesh.name : "Unnamed";
                submesh.BoundingBox.Reset();

                SG_ASSERT(submesh.IndexCount % 3 == 0, "Index count must be a multiple of 3");

                for(size_t i = 0; i < posAccessor->count; i++)
                {
                    outSpec.Vertices.emplace_back();
                    Vertex& v = outSpec.Vertices.back();

                    cgltf_accessor_read_float(posAccessor, i, &v.Position.x, 3);
                    cgltf_accessor_read_float(normalAccessor, i, &v.Normal.x, 3);

                    if(tangentAccessor)
                    {
                        float t[4];
                        cgltf_accessor_read_float(tangentAccessor, i, t, 4);
                        v.Tangent = { t[0], t[1], t[2] };
                        v.Bitangent = glm::cross(v.Normal, v.Tangent) * t[3];
                    }

                    if(texcoordAccessor)
                        cgltf_accessor_read_float(texcoordAccessor, i, &v.TexCoord.x, 2);

                    submesh.BoundingBox.Min = glm::min(v.Position, submesh.BoundingBox.Min);
                    submesh.BoundingBox.Max = glm::max(v.Position, submesh.BoundingBox.Max);
                }

                for(size_t i = 0; i < prim.indices->count; i += 3)
                {
                    outSpec.Indices.push_back({
                        (Uint)cgltf_accessor_read_index(prim.indices, i),
                        (Uint)cgltf_accessor_read_index(prim.indices, i + 1),
                        (Uint)cgltf_accessor_read_index(prim.indices, i + 2) });
                }
                vertexCount += submesh.VertexCount;
                indexCount += submesh.IndexCount;
            }
        }

        for(size_t n = 0; n < data->nodes_count; n++)
        {
            cgltf_node& node = data->nodes[n];
            if(!node.mesh) continue;

            float worldMat[16];
            cgltf_node_transform_world(&node, worldMat);
            glm::mat4 worldTransform = glm::make_mat4(worldMat);

            float localMat[16];
            cgltf_node_transform_local(&node, localMat);
            glm::mat4 localTransform = glm::make_mat4(localMat);

            size_t meshIdx = node.mesh - data->meshes;
            Uint startSubmesh = meshSubmeshStart[meshIdx];

            for(size_t p = 0; p < node.mesh->primitives_count; p++)
            {
                Submesh& submesh = outSpec.Submeshes[startSubmesh + p];
                submesh.NodeName = node.name ? node.name : "Unnamed";
                submesh.Transform = worldTransform;
                submesh.LocalTransform = localTransform;
            }
        }

        return outSpec;
    }

    static void ExtractMaterials(const String& absPath, cgltf_data* data, MeshSpecification& spec)
    {
        auto SetMaterialTexture = [&](const Path& meshPath, cgltf_texture_view& texView, Ref<Material>& material, const String& texName) -> bool {
            if(!texView.texture || !texView.texture->image)
                return false;

            cgltf_image* image = texView.texture->image;
            AssetManager* assetManager = Core::GetAssetManager();

            if(image->buffer_view)
            {
                Log<Severity::Error>("[Mesh] Embedded textures(.glb) are not supported yet, mesh is loading without textures", texName);
                return false;
            }
            else if(image->uri)
            {
                const Path absTexturePath = (Filesystem::GetParentPath(meshPath) / image->uri).lexically_normal();
                const Path relTexturePath = absTexturePath.lexically_relative(Path(assetManager->GetAssetsDirectory()));

                if(relTexturePath.empty() || *relTexturePath.begin() == "..")
                {
                    Log<Severity::Warn>("[Mesh] Texture '{}' is outside the assets directory, skipping.", absTexturePath.string());
                    return false;
                }

                const AssetID texID = assetManager->Import(relTexturePath.generic_string(), AssetType::TEXTURE2D);
                if(!texID.IsValid()) return false;

                material->SetTexture(texName, assetManager->Load<Texture2D>(texID));
                return true;
            }
            return false;
            };

        if(data->materials_count > 0)
        {
            spec.Materials.resize(data->materials_count);

            for(size_t i = 0; i < data->materials_count; i++)
            {
                cgltf_material& mat = data->materials[i];
                const String materialName = mat.name ? mat.name : "Unnamed";

                Ref<Material> material = Material::Create(materialName);
                spec.Materials[i] = material;

                ImageHandle whiteTexture = Core::GetRenderer()->GetWhiteTexture();
                material->SetTexture("AlbedoMap", whiteTexture);
                material->SetTexture("NormalMap", whiteTexture);
                material->SetTexture("RoughnessMetallicMap", whiteTexture);

                if(mat.has_pbr_metallic_roughness)
                {
                    auto& pbr = mat.pbr_metallic_roughness;
                    material->Set<glm::vec3>("Albedo", glm::vec3(pbr.base_color_factor[0], pbr.base_color_factor[1], pbr.base_color_factor[2]));
                    material->Set<float>("Roughness", pbr.roughness_factor);
                    material->Set<float>("Metallic", pbr.metallic_factor);
                    material->Set<float>("Reflectance", 0.5f);

                    if(SetMaterialTexture(absPath, pbr.base_color_texture, material, "AlbedoMap"))
                        material->Set<int>("UseAlbedoMap", 1);
                    if(SetMaterialTexture(absPath, pbr.metallic_roughness_texture, material, "RoughnessMetallicMap"))
                    {
                        material->Set<int>("UseRoughnessMap", 1);
                        material->Set<int>("UseMetallicMap", 1);
                    }
                }
                if(SetMaterialTexture(absPath, mat.normal_texture, material, "NormalMap"))
                    material->Set<int>("UseNormalMap", 1);
                else
                    material->Set<int>("UseNormalMap", 0);
            }
        }
    }

    static void WriteSidecar(const String& path, const MeshSpecification& spec, const Vector<Ref<Material>>& overrides)
    {
        Uint validOverrideCount = 0;
        for(const Ref<Material>& ref : overrides)
        {
            if(ref) validOverrideCount++;
        }

        static constexpr Uint sSubmeshPODSize =
            5 * sizeof(Uint) +       // BaseVertex/BaseIndex/MaterialIndex/IndexCount/VertexCount
            2 * sizeof(glm::vec3) +  // BoundsMin + BoundsMax
            2 * sizeof(glm::mat4);   // Transform + LocalTransform

        Uint geomSize = 0;
        geomSize += static_cast<Uint>(spec.Vertices.size()) * sizeof(Vertex);
        geomSize += static_cast<Uint>(spec.Indices.size()) * sizeof(Index);
        for(const Submesh& sm : spec.Submeshes)
        {
            geomSize += sSubmeshPODSize;
            geomSize += sizeof(Uint) + static_cast<Uint>(sm.NodeName.size());
            geomSize += sizeof(Uint) + static_cast<Uint>(sm.MeshName.size());
        }

        SurgeMeshHeader header = {};
        header.Magic = kSidecarMagic;
        header.Version = kSidecarVersion;
        header.VertexCount = static_cast<Uint>(spec.Vertices.size());
        header.IndexCount = static_cast<Uint>(spec.Indices.size());
        header.SubmeshCount = static_cast<Uint>(spec.Submeshes.size());
        header.ValidOverrideCount = validOverrideCount;
        header.GeomSectionSize = geomSize;

        Vector<Byte> buffer;
        buffer.reserve(sizeof(SurgeMeshHeader) + geomSize + validOverrideCount * (sizeof(Uint) + sizeof(uint64_t)));

        WriteData(buffer, header);
        WriteDataArray(buffer, spec.Vertices.data(), spec.Vertices.size());
        WriteDataArray(buffer, spec.Indices.data(), spec.Indices.size());

        for(const Submesh& sm : spec.Submeshes)
        {
            WriteData(buffer, sm.BaseVertex);
            WriteData(buffer, sm.BaseIndex);
            WriteData(buffer, sm.MaterialIndex);
            WriteData(buffer, sm.IndexCount);
            WriteData(buffer, sm.VertexCount);
            WriteData(buffer, sm.BoundingBox.Min);
            WriteData(buffer, sm.BoundingBox.Max);
            WriteData(buffer, sm.Transform);
            WriteData(buffer, sm.LocalTransform);
            WriteStr(buffer, sm.NodeName);
            WriteStr(buffer, sm.MeshName);
        }

        for(Uint i = 0; i < static_cast<Uint>(overrides.size()); i++)
        {
            if(!overrides[i]) continue;
            Uint slotIndex = i;
            uint64_t rawID = overrides[i]->GetID().Get();
            WriteData(buffer, slotIndex);
            WriteData(buffer, rawID);
        }

        Filesystem::WriteBinaryFile(path, buffer.data(), buffer.size());
    }

    static bool LoadSidecar(const String& path, MeshSpecification& outSpec)
    {
        Vector<Byte> fileData;
        if(!Filesystem::ReadBinaryFile(path, fileData))
            return false;

        const Byte* ptr = fileData.data();
        const Byte* endPtr = fileData.data() + fileData.size();

        if(fileData.size() < sizeof(SurgeMeshHeader))
            return false;

        SurgeMeshHeader header = {};
        ReadData(ptr, header);

        if(header.Magic != kSidecarMagic)
        {
            Log<Severity::Warn>("[MeshSerializer] {} has bad magic, ignoring sidecar.", path);
            return false;
        }
        if(header.Version != kSidecarVersion)
        {
            Log<Severity::Warn>("[MeshSerializer] {} version mismatch (got {}, want {}), re-cooking!", path, header.Version, kSidecarVersion);
            return false;
        }

        outSpec.Vertices.resize(header.VertexCount);
        if(header.VertexCount > 0)
            ReadDataArray(ptr, outSpec.Vertices.data(), header.VertexCount);

        outSpec.Indices.resize(header.IndexCount);
        if(header.IndexCount > 0)
            ReadDataArray(ptr, outSpec.Indices.data(), header.IndexCount);

        outSpec.Submeshes.reserve(header.SubmeshCount);
        for(Uint i = 0; i < header.SubmeshCount; i++)
        {
            Submesh& sm = outSpec.Submeshes.emplace_back();
            ReadData(ptr, sm.BaseVertex);
            ReadData(ptr, sm.BaseIndex);
            ReadData(ptr, sm.MaterialIndex);
            ReadData(ptr, sm.IndexCount);
            ReadData(ptr, sm.VertexCount);
            ReadData(ptr, sm.BoundingBox.Min);
            ReadData(ptr, sm.BoundingBox.Max);
            ReadData(ptr, sm.Transform);
            ReadData(ptr, sm.LocalTransform);
            sm.NodeName = ReadStr(ptr);
            sm.MeshName = ReadStr(ptr);
        }

        outSpec.MaterialOverrides.assign(outSpec.Submeshes.size(), AssetID::INVALID);
        for(Uint i = 0; i < header.ValidOverrideCount; i++)
        {
            Uint slotIndex = 0;
            uint64_t rawID = 0;
            ReadData(ptr, slotIndex);
            ReadData(ptr, rawID);
            if(slotIndex < static_cast<Uint>(outSpec.Submeshes.size()))
                outSpec.MaterialOverrides[slotIndex] = AssetID(rawID);
        }

        return ptr <= endPtr;
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
        const String absPath = am->GetAbsolutePath(meta.RelativePath);
        const String sidecarPath = GetSidecarPath(absPath);

        MeshSpecification existingGeom;
        if(!LoadSidecar(sidecarPath, existingGeom))
        {
            Log<Severity::Warn>("[MeshSerializer] Serialize: sidecar missing for '{}'. Load the mesh first to cook it.", meta.RelativePath.c_str());
            return false;
        }

        Ref<Mesh> mesh = asset.As<Mesh>();
        WriteSidecar(sidecarPath, existingGeom, mesh->GetMaterialOverrides());
        return true;
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
        const String absPath = am->GetAbsolutePath(metadata.RelativePath);
        const String sidecarPath = GetSidecarPath(absPath);

        if(LoadSidecar(sidecarPath, spec))
        {
            SCOPED_TIMER("MeshSerializer::Deserialize [FAST PATH]: {} {}", metadata.ID.Get(), metadata.RelativePath);
            cgltf_data* data = ParseGLTF(absPath);
            if(data)
            {
                ExtractMaterials(absPath, data, spec);
                FreeGLTF(data);
            }
        }
        else
        {
            SCOPED_TIMER("MeshSerializer::Deserialize [SLOW PATH]: {} {}", metadata.ID.Get(), metadata.RelativePath);
            cgltf_data* data = ParseGLTF(absPath);
            if(!data) return nullptr;

            ExtractGeometry(data, spec);
            ExtractMaterials(absPath, data, spec);
            FreeGLTF(data);

            WriteSidecar(sidecarPath, spec, {});
            Log<Severity::Trace>("[MeshSerializer] Cooked sidecar: '{}'", sidecarPath.c_str());
        }

        return Mesh::Create(std::move(spec)).As<Asset>();
    }

    void MeshSerializer::Shutdown()
    {
        Log<Severity::Info>("[MeshSerializer] Shutdown");
    }
}