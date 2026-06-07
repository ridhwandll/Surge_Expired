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
    void MeshSerializer::Initialize()
    {
        mSerializerType = AssetType::MESH;
    }

    static String GetSidecarPath(const String& meshPath)
    {
        return meshPath + ".sasset";
    }

    bool MeshSerializer::Serialize(Ref<Asset> asset) const
    {
        Ref<Mesh> mesh = asset.As<Mesh>();
        AssetManager* am = Core::GetAssetManager();
        const String sidecarPath = GetSidecarPath(am->GetAbsolutePath(am->GetMetadata(asset->GetID()).RelativePath));

        std::ofstream f(sidecarPath, std::ios::binary | std::ios::trunc);
        if (!f.is_open())
        {
            Log<Severity::Error>("[MeshSerializer] Failed to write sidecar!");
            return false;
        }

        // Only write slots that actually have a user override
        const Vector<Ref<Material>>& overrides = mesh->GetMaterialOverrides();

        Uint overrideCount = overrides.size();
        f.write(reinterpret_cast<const char*>(&overrideCount), sizeof(Uint));
        for(Uint i = 0; i < overrideCount; i++)
        {
            if(!overrides[i])
                continue;

            Uint slotIndex = i;
            uint64_t rawID = overrides[i]->GetID().Get();
            f.write(reinterpret_cast<const char*>(&slotIndex), sizeof(Uint));
            f.write(reinterpret_cast<const char*>(&rawID), sizeof(uint64_t));
        }

        return true;
    }

    Ref<Asset> MeshSerializer::Deserialize(const AssetMetadata& metadata) const
    {
        AssetManager* assetManager = Core::GetAssetManager();
        String absolutePath;

        if(HasFlag(metadata.Flags, AssetFlags::MEMORY))
            absolutePath = metadata.RelativePath; //metadata.RelativePath contains the memoryStr
        else
            absolutePath = assetManager->GetAbsolutePath(metadata.RelativePath);

        MeshSpecification spec = LoadMesh(absolutePath);

        const String sidecarPath = GetSidecarPath(absolutePath);
        std::ifstream sidecar(sidecarPath, std::ios::binary);
        if(sidecar.is_open())
        {
            Uint overrideCount = 0;
            sidecar.read(reinterpret_cast<char*>(&overrideCount), sizeof(Uint));

            spec.MaterialOverrides.resize(spec.Materials.size(), AssetID::INVALID);

            for(Uint i = 0; i < overrideCount; i++)
            {
                Uint slotIndex = 0;
                uint64_t rawID = 0;
                sidecar.read(reinterpret_cast<char*>(&slotIndex), sizeof(Uint));
                sidecar.read(reinterpret_cast<char*>(&rawID), sizeof(uint64_t));

                if(slotIndex < spec.Materials.size())
                    spec.MaterialOverrides[slotIndex] = AssetID(rawID);
            }
        }
        return Mesh::Create(std::move(spec)).As<Asset>();
    }

    void MeshSerializer::Shutdown()
    {
        Log<Severity::Info>("[MeshSerializer] Shutdown");
    }

    static bool SetMaterialTexture(const Path& meshPath, cgltf_texture_view& texView, Ref<Material>& material, const String& texName)
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
    }

    bool MeshSerializer::CheckAndGenerateDefaultMesh(const String& filepath, MeshSpecification& outSpec) const
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

    MeshSpecification MeshSerializer::LoadMesh(const String& filepath) const
    {
        MeshSpecification spec;
        if (CheckAndGenerateDefaultMesh(filepath, spec))
            return spec;

        cgltf_options options = {};
        cgltf_data* data = nullptr;

#ifdef SURGE_PLATFORM_ANDROID
        android_app* app = Android::GAndroidApp;
        AAssetManager* assetManager = app->activity->assetManager;

        options.file.user_data = assetManager;
        options.file.read = [](const struct cgltf_memory_options* memory_options, const struct cgltf_file_options* file_options, const char* path, cgltf_size* size, void** data) -> cgltf_result {
            AAssetManager* mgr = (AAssetManager*)file_options->user_data;
            AAsset* asset = AAssetManager_open(mgr, path, AASSET_MODE_BUFFER);
            if(!asset) return cgltf_result_file_not_found;

            *size = AAsset_getLength(asset);
            *data = malloc(*size);
            AAsset_read(asset, *data, *size);
            AAsset_close(asset);
            return cgltf_result_success;
            };
        options.file.release = [](const cgltf_memory_options*, const cgltf_file_options*, void* data, cgltf_size size) {
            free(data);
            };


        // Now both parse and load_buffers go through AAssetManager
        if(cgltf_parse_file(&options, filepath.c_str(), &data) != cgltf_result_success)
        {
            Log<Severity::Error>("Failed to parse glTF: {0}", filepath);
            return{};
        }
        if(cgltf_load_buffers(&options, data, filepath.c_str()) != cgltf_result_success)
        {
            Log<Severity::Error>("Failed to load buffers: {0}", filepath);
            cgltf_free(data);
            return {};
        }

#elif defined(SURGE_PLATFORM_WINDOWS)
        if(cgltf_parse_file(&options, filepath.c_str(), &data) != cgltf_result_success)
        {
            Log<Severity::Error>("Failed to parse glTF file: {0}", filepath);
            return {};
        }

        if(cgltf_load_buffers(&options, data, filepath.c_str()) != cgltf_result_success)
        {
            Log<Severity::Error>("Failed to load glTF buffers: {0}", filepath);
            cgltf_free(data);
            return {};
        }
#endif
        // Map each mesh to its first submesh index
        // each primitive within a cgltf_mesh = one Submesh
        Vector<Uint> meshSubmeshStart(data->meshes_count);
        Uint totalSubmeshes = 0;
        for(size_t i = 0; i < data->meshes_count; i++)
        {
            meshSubmeshStart[i] = totalSubmeshes;
            totalSubmeshes += (Uint)data->meshes[i].primitives_count;
        }
        spec.Submeshes.reserve(totalSubmeshes);

        Uint vertexCount = 0;
        Uint indexCount = 0;

        // Geometry extraction
        {
            SCOPED_TIMER("MeshSerializer::LoadMesh  Geometry Extraction");
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

                    spec.Submeshes.emplace_back();
                    Submesh& submesh = spec.Submeshes.back();
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
                        spec.Vertices.emplace_back();
                        Vertex& v = spec.Vertices.back();

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
                        spec.Indices.push_back({
                            (Uint)cgltf_accessor_read_index(prim.indices, i),
                            (Uint)cgltf_accessor_read_index(prim.indices, i + 1),
                            (Uint)cgltf_accessor_read_index(prim.indices, i + 2)});
                    }
                    vertexCount += submesh.VertexCount;
                    indexCount += submesh.IndexCount;
                }
            }
        }

        // Pass 2: node traversal: assign world/local transforms to submeshes
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
                Submesh& submesh = spec.Submeshes[startSubmesh + p];
                submesh.NodeName = node.name ? node.name : "Unnamed";
                submesh.Transform = worldTransform;
                submesh.LocalTransform = localTransform;
            }
        }
        // Materials
        // glTF PBR metallic-roughness:
        //    base_color_texture = AlbedoMap
        //    normal_texture = NormalMap
        //    metallic_roughness_texture = R = occlusion G = roughness B = metalness

        {
            SCOPED_TIMER("MeshSerializer::LoadMesh Material Extraction");
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

                        if(SetMaterialTexture(filepath, pbr.base_color_texture, material, "AlbedoMap"))
                            material->Set<int>("UseAlbedoMap", 1);
                        //glTF packs roughness (G) and metalness (B) into one texture
                        if(SetMaterialTexture(filepath, pbr.metallic_roughness_texture, material, "RoughnessMetallicMap"))
                        {
                            material->Set<int>("UseRoughnessMap", 1);
                            material->Set<int>("UseMetallicMap", 1);
                        }
                    }
                    if(SetMaterialTexture(filepath, mat.normal_texture, material, "NormalMap"))
                        material->Set<int>("UseNormalMap", 1);
                    else
                        material->Set<int>("UseNormalMap", 0);
                }
            }
        }
        cgltf_free(data);
        return spec;
    }

}


