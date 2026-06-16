// Copyright (c) - SurgeTechnologies - All rights reserved
#include "MeshCooker.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Asset/Serializer/Mesh/MeshBinaryFormat.hpp"

#include <cgltf.h>
#include <meshoptimizer.h>
#include <glm/gtc/type_ptr.hpp>

namespace Surge
{
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

                if(!texID.IsValid())
                    return false;

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

    static void FreeGLTF(cgltf_data* data)
    {
        cgltf_free(data);
    }

    static void OptimizeMesh(MeshSpecification& spec)
    {
        for(Submesh& sm : spec.Submeshes)
        {
            if(sm.IndexCount == 0 || sm.VertexCount == 0)
                continue;

            Uint* indices = reinterpret_cast<Uint*>(spec.Indices.data()) + sm.BaseIndex;
            Vertex* vertices = spec.Vertices.data() + sm.BaseVertex;

            Vector<Uint> remap(sm.VertexCount);
            size_t uniqueVerts = meshopt_generateVertexRemap(remap.data(), indices, sm.IndexCount, vertices, sm.VertexCount, sizeof(Vertex));

            if (sm.VertexCount != uniqueVerts)
                Log<Severity::Warn>("[MeshCooker] Old Vertex Count: {} | New Vertex Count: {}", sm.VertexCount, uniqueVerts);

            meshopt_remapIndexBuffer(indices, indices, sm.IndexCount, remap.data());
            meshopt_remapVertexBuffer(vertices, vertices, sm.VertexCount, sizeof(Vertex), remap.data());
            meshopt_optimizeVertexCache(indices, indices, sm.IndexCount, uniqueVerts);
            meshopt_optimizeOverdraw(indices, indices, sm.IndexCount, &vertices[0].Position.x, uniqueVerts, sizeof(Vertex), 1.05f);
            meshopt_optimizeVertexFetch(vertices, indices, sm.IndexCount, vertices, uniqueVerts, sizeof(Vertex));

            sm.VertexCount = uniqueVerts;
        }
    }

    CookResult MeshCooker::Cook(const String & sourceAbsPath, AssetID id) const
    {
        cgltf_data* data = ParseGLTF(sourceAbsPath);
        if(!data)
            return {};

        AssetManager* am = Core::GetAssetManager();

        MeshSpecification spec;
        ExtractGeometry(data, spec);
        ExtractMaterials(sourceAbsPath, data, spec);
        FreeGLTF(data);
        OptimizeMesh(spec);

        const AssetStamp stamp = AssetStampWriter::Build(sourceAbsPath, GetCookerVersion());

        const String sidecarPath = am->GetSidecarPath(id);
        bool res = MeshBinary::Write(sidecarPath, stamp, spec, {});

        CookResult result;
        result.Success = res;
        result.OutputPath = sidecarPath;
        result.InputMegaBytes = Filesystem::FileSizeInMB(sourceAbsPath);
        result.OutputMegaBytes = Filesystem::FileSizeInMB(sidecarPath);
        return result;
    }
}
