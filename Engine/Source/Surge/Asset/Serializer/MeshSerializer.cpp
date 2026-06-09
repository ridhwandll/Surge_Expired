// Copyright (c) - SurgeTechnologies - All rights reserved
#include "MeshSerializer.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Utility/Filesystem.hpp"

#ifdef SURGE_PLATFORM_ANDROID
#include "Surge/Platform/Android/AndroidApp.hpp"
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <android/asset_manager.h>
#endif
#include <cgltf.h>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>

namespace Surge
{
    // [Header: 28 bytes]
    //     Magic            uint32  'SRGM'
    //     Version          uint32  1
    //     VertexCount      uint32
    //     IndexCount       uint32
    //     SubmeshCount     uint32
    //     ValidOverrides   uint32  sparse count
    //     GeomSectionSize  uint32  bytes from end of header to end of submesh section
    // 
    // [Vertices: VertexCount × 56 bytes]
    //     Position / Normal / Tangent / Bitangent / TexCoord
    // 
    // [Indices: IndexCount × 12 bytes]
    //     V1 / V2 / V3
    // 
    // [Submeshes: SubmeshCount entries, variable]
    //     Per submesh : BaseVertex, BaseIndex, MaterialIndex, IndexCount, VertexCount  5 * uint32
    //     BoundsMin, BoundsMax                                                         2 * vec3 (24 bytes)
    //     Transform, LocalTransform                                                    2 * mat4 (128 bytes)
    //     NodeName uint32 len + chars
    //     MeshName uint32 len + chars
    // 
    // [Overrides: ValidOverrides entries]
    //     Per override :
    //     slotIndex    uint32
    //     assetID      uint64

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
        Uint GeomSectionSize; // bytes from end of header to end of submesh section
    };
    static_assert(sizeof(SurgeMeshHeader) == 28);

    //
    // Static helpers
    //

    static String GetSidecarPath(const String& meshAbsPath)
    {
        const size_t lastDot = meshAbsPath.rfind('.');
        if(lastDot != String::npos)
            return meshAbsPath.substr(0, lastDot) + ".RAsset";

        return meshAbsPath + ".RAsset";
    }

    static void WriteStr(std::ofstream& f, const String& s)
    {
        Uint len = static_cast<Uint>(s.size());
        f.write(reinterpret_cast<const char*>(&len), sizeof(Uint));
        f.write(s.data(), len);
    }

    static String ReadStr(std::ifstream& f)
    {
        Uint len = 0;
        f.read(reinterpret_cast<char*>(&len), sizeof(Uint));
        String s(len, '\0');
        if(len > 0)
            f.read(s.data(), len);
        return s;
    }

    static cgltf_data* ParseGLTF(const String& filepath)
    {
        cgltf_options options = {};
        cgltf_data* data = nullptr;

#ifdef SURGE_PLATFORM_ANDROID
        android_app* app = Android::GAndroidApp;
        AAssetManager* androidAssetMgr = app->activity->assetManager;

        options.file.user_data = androidAssetMgr;
        options.file.read = [](const cgltf_memory_options*, const cgltf_file_options* fopt, const char* path, cgltf_size* size, void** data) -> cgltf_result {
            AAssetManager* mgr = static_cast<AAssetManager*>(fopt->user_data);
            AAsset* asset = AAssetManager_open(mgr, path, AASSET_MODE_BUFFER);
            if(!asset) return cgltf_result_file_not_found;
            *size = AAsset_getLength(asset);
            *data = malloc(*size);
            AAsset_read(asset, *data, *size);
            AAsset_close(asset);
            return cgltf_result_success;
            };
        options.file.release = [](const cgltf_memory_options*, const cgltf_file_options*, void* data, cgltf_size) { free(data); };
#endif

        if(cgltf_parse_file(&options, filepath.c_str(), &data) != cgltf_result_success)
        {
            Log<Severity::Error>("[MeshSerializer] Failed to parse glTF: '{}'", filepath.c_str());
            return nullptr;
        }
        if(cgltf_load_buffers(&options, data, filepath.c_str()) != cgltf_result_success)
        {
            Log<Severity::Error>("[MeshSerializer] Failed to load buffers: '{}'", filepath.c_str());
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

        // Only one submesh for default meshes
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
        //Compute AABB
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
        //SCOPED_TIMER("MeshSerializer::ExtractGeometry");

        // Map each mesh to its first submesh index
        // each primitive within a cgltf_mesh = one Submesh
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

        // Geometry extraction
        for(size_t m = 0; m < data->meshes_count; m++)
        {
            cgltf_mesh& mesh = data->meshes[m];

            for(size_t p = 0; p < mesh.primitives_count; p++)
            {
                cgltf_primitive& prim = mesh.primitives[p];
                SG_ASSERT(prim.type == cgltf_primitive_type_triangles, "Only triangle primitives supported");
                SG_ASSERT(prim.indices, "Mesh primitive requires indices");

                // Find per-attribute accessors
                cgltf_accessor* posAccessor = nullptr;
                cgltf_accessor* normalAccessor = nullptr;
                cgltf_accessor* tangentAccessor = nullptr;
                cgltf_accessor* texcoordAccessor = nullptr;

                for(size_t a = 0; a < prim.attributes_count; a++)
                {
                    cgltf_attribute& attr = prim.attributes[a];
                    switch(attr.type)
                    {
                        case cgltf_attribute_type_position:
                            posAccessor = attr.data;
                            break;
                        case cgltf_attribute_type_normal:
                            normalAccessor = attr.data;
                            break;
                        case cgltf_attribute_type_tangent:
                            tangentAccessor = attr.data;
                            break;
                        case cgltf_attribute_type_texcoord:
                            if(attr.index == 0)
                                texcoordAccessor = attr.data;
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

                // Vertices
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
                        // w component is handedness used to reconstruct bitangent
                        v.Bitangent = glm::cross(v.Normal, v.Tangent) * t[3];
                    }

                    if(texcoordAccessor)
                        cgltf_accessor_read_float(texcoordAccessor, i, &v.TexCoord.x, 2);

                    submesh.BoundingBox.Min.x = glm::min(v.Position.x, submesh.BoundingBox.Min.x);
                    submesh.BoundingBox.Min.y = glm::min(v.Position.y, submesh.BoundingBox.Min.y);
                    submesh.BoundingBox.Min.z = glm::min(v.Position.z, submesh.BoundingBox.Min.z);
                    submesh.BoundingBox.Max.x = glm::max(v.Position.x, submesh.BoundingBox.Max.x);
                    submesh.BoundingBox.Max.y = glm::max(v.Position.y, submesh.BoundingBox.Max.y);
                    submesh.BoundingBox.Max.z = glm::max(v.Position.z, submesh.BoundingBox.Max.z);
                }

                // Indices; read as triangles (3 at a time)
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

        // Node traversal: assign world/local transforms to submeshes
        for(size_t n = 0; n < data->nodes_count; n++)
        {
            cgltf_node& node = data->nodes[n];
            if(!node.mesh)
                continue;

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
        auto SetMaterialTexture = [&](const Path& meshPath, cgltf_texture_view& texView, Ref<Material>& material, const String& texName) -> bool
            {
                if(!texView.texture || !texView.texture->image)
                    return false;

                cgltf_image* image = texView.texture->image;
                AssetManager* assetManager = Core::GetAssetManager();

                // Embedded Texture (typically from a .glb file)
                if(image->buffer_view)
                {
                    const uint8_t* rawData = (const uint8_t*)image->buffer_view->buffer->data + image->buffer_view->offset;
                    size_t dataSize = image->buffer_view->size;
                    if(rawData && dataSize > 0)
                    {
                        //TODO
                        Log<Severity::Error>("[Mesh] Embedded textures(.glb) are not supported yet, mesh is loading without textures", texName);
                    }
                    return false;
                }
                // External Texture (typically from a .gltf file)
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
                    if(!texID.IsValid())
                        return false;

                    material->SetTexture(texName, assetManager->Load<Texture2D>(texID));

                    return true;
                }
                return false;
            };

        //SCOPED_TIMER("MeshSerializer::ExtractMaterials");
        if(data->materials_count > 0)
        {
            spec.Materials.resize(data->materials_count);

            for(size_t i = 0; i < data->materials_count; i++)
            {
                cgltf_material& mat = data->materials[i];
                const String materialName = mat.name ? mat.name : "Unnamed";

                // Must not be from AssetManager (loaded form mesh)
                Ref<Material> material = Material::Create(materialName);
                spec.Materials[i] = material;

                // Defaults
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
                    //glTF packs roughness (G) and metalness (B) into one texture
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
            if(ref)
                validOverrideCount++;
        }

        // Compute geometry section size
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

        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        SG_ASSERT(f.is_open(), "[MeshSerializer] Failed to open sidecar for writing: '{}'", path.c_str());

        // Header
        f.write(reinterpret_cast<const char*>(&header), sizeof(SurgeMeshHeader));

        // Vertices (raw blob)
        if(!spec.Vertices.empty())
            f.write(reinterpret_cast<const char*>(spec.Vertices.data()), spec.Vertices.size() * sizeof(Vertex));

        // Indices (raw blob)
        if(!spec.Indices.empty())
            f.write(reinterpret_cast<const char*>(spec.Indices.data()), spec.Indices.size() * sizeof(Index));

        // Submeshes
        for(const Submesh& sm : spec.Submeshes)
        {
            f.write(reinterpret_cast<const char*>(&sm.BaseVertex), sizeof(Uint));
            f.write(reinterpret_cast<const char*>(&sm.BaseIndex), sizeof(Uint));
            f.write(reinterpret_cast<const char*>(&sm.MaterialIndex), sizeof(Uint));
            f.write(reinterpret_cast<const char*>(&sm.IndexCount), sizeof(Uint));
            f.write(reinterpret_cast<const char*>(&sm.VertexCount), sizeof(Uint));
            f.write(reinterpret_cast<const char*>(&sm.BoundingBox.Min), sizeof(glm::vec3));
            f.write(reinterpret_cast<const char*>(&sm.BoundingBox.Max), sizeof(glm::vec3));
            f.write(reinterpret_cast<const char*>(&sm.Transform), sizeof(glm::mat4));
            f.write(reinterpret_cast<const char*>(&sm.LocalTransform), sizeof(glm::mat4));
            WriteStr(f, sm.NodeName);
            WriteStr(f, sm.MeshName);
        }

        // Overrides (sparse: only valid slots)
        for(Uint i = 0; i < static_cast<Uint>(overrides.size()); i++)
        {
            if(!overrides[i])
                continue;

            Uint slotIndex = i;
            uint64_t rawID = overrides[i]->GetID().Get();
            f.write(reinterpret_cast<const char*>(&slotIndex), sizeof(Uint));
            f.write(reinterpret_cast<const char*>(&rawID), sizeof(uint64_t));
        }
    }

    static bool LoadSidecar(const String& path, MeshSpecification& outSpec)
    {
        std::ifstream f(path, std::ios::binary);
        if(!f.is_open())
            return false;

        SurgeMeshHeader header = {};
        f.read(reinterpret_cast<char*>(&header), sizeof(SurgeMeshHeader));

        if(!f || header.Magic != kSidecarMagic)
        {
            Log<Severity::Warn>("[MeshSerializer] '{}' has bad magic, ignoring sidecar.", path.c_str());
            return false;
        }
        if(header.Version != kSidecarVersion)
        {
            Log<Severity::Warn>("[MeshSerializer] '{}' version mismatch (got {}, want {}), re-cooking.", path.c_str(), header.Version, kSidecarVersion);
            return false;
        }

        // Vertices
        outSpec.Vertices.resize(header.VertexCount);
        if(header.VertexCount > 0)
            f.read(reinterpret_cast<char*>(outSpec.Vertices.data()), header.VertexCount * sizeof(Vertex));

        // Indices
        outSpec.Indices.resize(header.IndexCount);
        if(header.IndexCount > 0)
            f.read(reinterpret_cast<char*>(outSpec.Indices.data()), header.IndexCount * sizeof(Index));

        // Submeshes
        outSpec.Submeshes.reserve(header.SubmeshCount);
        for(Uint i = 0; i < header.SubmeshCount; i++)
        {
            Submesh& sm = outSpec.Submeshes.emplace_back();
            f.read(reinterpret_cast<char*>(&sm.BaseVertex), sizeof(Uint));
            f.read(reinterpret_cast<char*>(&sm.BaseIndex), sizeof(Uint));
            f.read(reinterpret_cast<char*>(&sm.MaterialIndex), sizeof(Uint));
            f.read(reinterpret_cast<char*>(&sm.IndexCount), sizeof(Uint));
            f.read(reinterpret_cast<char*>(&sm.VertexCount), sizeof(Uint));
            f.read(reinterpret_cast<char*>(&sm.BoundingBox.Min), sizeof(glm::vec3));
            f.read(reinterpret_cast<char*>(&sm.BoundingBox.Max), sizeof(glm::vec3));
            f.read(reinterpret_cast<char*>(&sm.Transform), sizeof(glm::mat4));
            f.read(reinterpret_cast<char*>(&sm.LocalTransform), sizeof(glm::mat4));
            sm.NodeName = ReadStr(f);
            sm.MeshName = ReadStr(f);
        }

        // Overrides
        outSpec.MaterialOverrides.assign(outSpec.Submeshes.size(), AssetID::INVALID);
        for(Uint i = 0; i < header.ValidOverrideCount; i++)
        {
            Uint slotIndex = 0;
            uint64_t rawID = 0;
            f.read(reinterpret_cast<char*>(&slotIndex), sizeof(Uint));
            f.read(reinterpret_cast<char*>(&rawID), sizeof(uint64_t));
            if(slotIndex < static_cast<Uint>(outSpec.Submeshes.size()))
                outSpec.MaterialOverrides[slotIndex] = AssetID(rawID);
        }

        return f.good() || f.eof();
    }

    // 
    // Mesh Serializer
    // 

    void MeshSerializer::Initialize()
    {
        mSerializerType = AssetType::MESH;
    }

    /// Serialize ///
    bool MeshSerializer::Serialize(Ref<Asset> asset) const
    {
        AssetManager* am = Core::GetAssetManager();
        const AssetMetadata& meta = am->GetMetadata(asset->GetID());

        if(HasFlag(meta.Flags, AssetFlags::MEMORY))
            return true; // Memory meshes have no sidecar

        SCOPED_TIMER("MeshSerializer::Serialize");
        const String absPath = am->GetAbsolutePath(meta.RelativePath);
        const String sidecarPath = GetSidecarPath(absPath);

        // Read existing geometry needed to re-write the full sidecar with new overrides
        MeshSpecification existingGeom;
        if(!LoadSidecar(sidecarPath, existingGeom))
        {
            Log<Severity::Warn>("[MeshSerializer] Serialize: sidecar missing for '{}'. Load the mesh first to cook it.", meta.RelativePath.c_str());
            return false;
        }

        // Re-write: existing geometry + updated override list from the live Mesh
        Ref<Mesh> mesh = asset.As<Mesh>();
        WriteSidecar(sidecarPath, existingGeom, mesh->GetMaterialOverrides());
        return true;
    }

    /// Deserialize ///
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
            // Fast path: geometry from sidecar
            // Materials still come from glTF, vertex loops are skipped so this is quick
            cgltf_data* data = ParseGLTF(absPath);
            if(data)
            {
                ExtractMaterials(absPath, data, spec);
                FreeGLTF(data);
            }
        }
        else  // First import/Bad sidecar
        {
            SCOPED_TIMER("MeshSerializer::Deserialize [SLOW PATH]: {} {}", metadata.ID.Get(), metadata.RelativePath);
            cgltf_data* data = ParseGLTF(absPath);
            if(!data)
                return nullptr;

            ExtractGeometry(data, spec);
            ExtractMaterials(absPath, data, spec);
            FreeGLTF(data);

            WriteSidecar(sidecarPath, spec, {});
            Log<Severity::Info>("[MeshSerializer] Cooked sidecar: '{}'", sidecarPath.c_str());
        }

        return Mesh::Create(std::move(spec)).As<Asset>();
    }

    void MeshSerializer::Shutdown()
    {
        Log<Severity::Info>("[MeshSerializer] Shutdown");
    }
}


