// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Mesh.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <cgltf.h>

#ifdef SURGE_PLATFORM_ANDROID
#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#include <android/log.h>
#endif

namespace Surge
{
    static void LoadTexture(const Path& meshPath, cgltf_texture_view& texView,
                            Ref<Material>& material, const String& texName)
    {
        if (!texView.texture || !texView.texture->image || !texView.texture->image->uri)
            return;

        Path texturePath = Filesystem::GetParentPath(meshPath) / String(texView.texture->image->uri);
        Log<Severity::Trace>("{0} path: {1}", texName, texturePath);

        TextureSpecification spec;
        spec.UseMips = true;
        Ref<Texture2D> texture = Texture2D::Create(texturePath, spec);
        material->Set<Ref<Texture2D>>(texName, texture);
    }

    Mesh::Mesh(const Path& filepath)
        : mPath(filepath)
    {
        return; // TODO(Rid): Temporary, until we have a proper Mobile Renderer is in place
        cgltf_options options = {};
        cgltf_data* data = nullptr;

#ifdef SURGE_PLATFORM_ANDROID
        auto clientOptions = Core::GetClient()->GeClientOptions();
        android_app* app = static_cast<android_app*>(clientOptions.AndroidApp);
        AAssetManager* assetManager = app->activity->assetManager;

        options.file.user_data = assetManager;
        options.file.read = [](const struct cgltf_memory_options* memory_options, const struct cgltf_file_options* file_options, const char* path, cgltf_size* size, void** data) -> cgltf_result
        {
            AAssetManager* mgr = (AAssetManager*)file_options->user_data;
            AAsset* asset = AAssetManager_open(mgr, path, AASSET_MODE_BUFFER);
            if (!asset) return cgltf_result_file_not_found;

            *size = AAsset_getLength(asset);
            *data = malloc(*size);
            AAsset_read(asset, *data, *size);
            AAsset_close(asset);
            return cgltf_result_success;
        };
        options.file.release = [](const cgltf_memory_options*, const cgltf_file_options*, void* data, cgltf_size size)
        {
            free(data);
        };


        // Now both parse and load_buffers go through AAssetManager
        if (cgltf_parse_file(&options, filepath.Str().c_str(), &data) != cgltf_result_success)
        {
            Log<Severity::Error>("Failed to parse glTF: {0}", filepath);
            return;
        }
        if (cgltf_load_buffers(&options, data, filepath.Str().c_str()) != cgltf_result_success)
        {
            Log<Severity::Error>("Failed to load buffers: {0}", filepath);
            cgltf_free(data);
            return;
        }

#elif defined(SURGE_PLATFORM_WINDOWS)
        if (cgltf_parse_file(&options, filepath.Str().c_str(), &data) != cgltf_result_success)
        {
            Log<Severity::Error>("Failed to parse glTF file: {0}", filepath);
            return;
        }

        if (cgltf_load_buffers(&options, data, filepath.Str().c_str()) != cgltf_result_success)
        {
            Log<Severity::Error>("Failed to load glTF buffers: {0}", filepath);
            cgltf_free(data);
            return;
        }
#endif
        // Map each mesh to its first submesh index
        // each primitive within a cgltf_mesh = one Submesh
        Vector<Uint> meshSubmeshStart(data->meshes_count);
        Uint totalSubmeshes = 0;
        for (size_t i = 0; i < data->meshes_count; i++)
        {
            meshSubmeshStart[i] = totalSubmeshes;
            totalSubmeshes += (Uint)data->meshes[i].primitives_count;
        }
        mSubmeshes.reserve(totalSubmeshes);

        Uint vertexCount = 0;
        Uint indexCount = 0;

        // Geometry extraction
        for (size_t m = 0; m < data->meshes_count; m++)
        {
            cgltf_mesh& mesh = data->meshes[m];

            for (size_t p = 0; p < mesh.primitives_count; p++)
            {
                cgltf_primitive& prim = mesh.primitives[p];
                SG_ASSERT(prim.type == cgltf_primitive_type_triangles, "Only triangle primitives supported");
                SG_ASSERT(prim.indices, "Mesh primitive requires indices");

                // Find per-attribute accessors
                cgltf_accessor* posAccessor = nullptr;
                cgltf_accessor* normalAccessor = nullptr;
                cgltf_accessor* tangentAccessor = nullptr;
                cgltf_accessor* texcoordAccessor = nullptr;

                for (size_t a = 0; a < prim.attributes_count; a++)
                {
                    cgltf_attribute& attr = prim.attributes[a];
                    switch (attr.type)
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
                            if (attr.index == 0)
                                texcoordAccessor = attr.data;
                            break;
                        default: break;
                    }
                }

                SG_ASSERT(posAccessor, "Mesh primitive requires positions");
                SG_ASSERT(normalAccessor, "Mesh primitive requires normals");

                mSubmeshes.emplace_back();
                Submesh& submesh = mSubmeshes.back();
                submesh.BaseVertex = vertexCount;
                submesh.BaseIndex = indexCount;
                submesh.VertexCount = (Uint)posAccessor->count;
                submesh.IndexCount = (Uint)prim.indices->count;
                submesh.MaterialIndex = prim.material ? (Uint)(prim.material - data->materials) : 0;
                submesh.MeshName = mesh.name ? mesh.name : "Unnamed";
                submesh.BoundingBox.Reset();

                SG_ASSERT(submesh.IndexCount % 3 == 0, "Index count must be a multiple of 3");

                // Vertices
                for (size_t i = 0; i < posAccessor->count; i++)
                {
                    Vertex vertex = {};

                    cgltf_accessor_read_float(posAccessor, i, &vertex.Position.x, 3);
                    cgltf_accessor_read_float(normalAccessor, i, &vertex.Normal.x, 3);

                    if (tangentAccessor)
                    {
                        float t[4];
                        cgltf_accessor_read_float(tangentAccessor, i, t, 4);
                        vertex.Tangent = {t[0], t[1], t[2]};
                        // w component is handedness used to reconstruct bitangent
                        vertex.Bitangent = glm::cross(vertex.Normal, vertex.Tangent) * t[3];
                    }

                    if (texcoordAccessor)
                        cgltf_accessor_read_float(texcoordAccessor, i, &vertex.TexCoord.x, 2);

                    submesh.BoundingBox.Min.x = glm::min(vertex.Position.x, submesh.BoundingBox.Min.x);
                    submesh.BoundingBox.Min.y = glm::min(vertex.Position.y, submesh.BoundingBox.Min.y);
                    submesh.BoundingBox.Min.z = glm::min(vertex.Position.z, submesh.BoundingBox.Min.z);
                    submesh.BoundingBox.Max.x = glm::max(vertex.Position.x, submesh.BoundingBox.Max.x);
                    submesh.BoundingBox.Max.y = glm::max(vertex.Position.y, submesh.BoundingBox.Max.y);
                    submesh.BoundingBox.Max.z = glm::max(vertex.Position.z, submesh.BoundingBox.Max.z);

                    mVertices.push_back(vertex);
                }

                // Indices; read as triangles (3 at a time)
                for (size_t i = 0; i < prim.indices->count; i += 3)
                {
                    Index index = {
                        (Uint)cgltf_accessor_read_index(prim.indices, i),
                        (Uint)cgltf_accessor_read_index(prim.indices, i + 1),
                        (Uint)cgltf_accessor_read_index(prim.indices, i + 2)};
                    mIndices.push_back(index);
                }

                vertexCount += submesh.VertexCount;
                indexCount += submesh.IndexCount;
            }
        }

        // Pass 2: node traversal — assign world/local transforms to submeshes
        for (size_t n = 0; n < data->nodes_count; n++)
        {
            cgltf_node& node = data->nodes[n];
            if (!node.mesh)
                continue;

            float worldMat[16];
            cgltf_node_transform_world(&node, worldMat);
            glm::mat4 worldTransform = glm::make_mat4(worldMat);

            float localMat[16];
            cgltf_node_transform_local(&node, localMat);
            glm::mat4 localTransform = glm::make_mat4(localMat);

            size_t meshIdx = node.mesh - data->meshes;
            Uint startSubmesh = meshSubmeshStart[meshIdx];

            for (size_t p = 0; p < node.mesh->primitives_count; p++)
            {
                Submesh& submesh = mSubmeshes[startSubmesh + p];
                submesh.NodeName = node.name ? node.name : "Unnamed";
                submesh.Transform = worldTransform;
                submesh.LocalTransform = localTransform;
            }
        }

        // Materials
        // glTF PBR metallic-roughness:
        //   base_color_texture = AlbedoMap
        //   normal_texture = NormalMap
        //   metallic_roughness_texture = R = occlusion G = roughness B = metalness
        if (data->materials_count > 0)
        {
            mMaterials.resize(data->materials_count);

            for (size_t i = 0; i < data->materials_count; i++)
            {
                cgltf_material& mat = data->materials[i];
                const String materialName = mat.name ? mat.name : "NoName";

                Ref<Material> material = Material::Create("PBR", materialName);
                mMaterials[i] = material;

                if (mat.has_pbr_metallic_roughness)
                {
                    auto& pbr = mat.pbr_metallic_roughness;

                    material->Set("Material.Albedo", glm::vec3(pbr.base_color_factor[0], pbr.base_color_factor[1], pbr.base_color_factor[2]));
                    material->Set("Material.Roughness", pbr.roughness_factor);
                    material->Set("Material.Metalness", pbr.metallic_factor);

                    LoadTexture(mPath, pbr.base_color_texture, material, "AlbedoMap");
                    // glTF packs roughness (G) and metalness (B) into one texture
                    LoadTexture(mPath, pbr.metallic_roughness_texture, material, "MetalnessMap");
                    LoadTexture(mPath, pbr.metallic_roughness_texture, material, "RoughnessMap");
                }

                LoadTexture(mPath, mat.normal_texture, material, "NormalMap");
            }
        }

        mVertexBuffer = VertexBuffer::Create(mVertices.data(), static_cast<Uint>(mVertices.size()) * sizeof(Vertex));
        mIndexBuffer = IndexBuffer::Create(mIndices.data(), static_cast<Uint>(mIndices.size()) * sizeof(Index));
        LOGI("Mesh vertex count: %zu", mVertices.size());
        LOGI("Mesh index count: %zu", mIndices.size());
        cgltf_free(data);
    }

} // namespace Surge