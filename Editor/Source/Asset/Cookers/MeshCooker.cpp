// Copyright (c) - SurgeTechnologies - All rights reserved
#include "MeshCooker.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Asset/Format/BinarySerialization.hpp"
#include "Surge/Asset/Format/Mesh/MeshFormat.hpp"
#include "Asset/AssetDatabase.hpp"

#include <cgltf.h>
#include <meshoptimizer.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>

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

    // Reads one triangle primitive (vertices in its own space) at the end of outSpec and returns its submesh
    static Submesh& AppendPrimitive(cgltf_data* data, const cgltf_mesh& mesh, const cgltf_primitive& prim, MeshSpecification& outSpec, Uint& vertexCount, Uint& indexCount)
    {
        SG_ASSERT(prim.type == cgltf_primitive_type_triangles, "Only triangle primitives supported");
        SG_ASSERT(prim.indices, "Mesh primitive requires indices");

        cgltf_accessor* posAccessor = nullptr;
        cgltf_accessor* normalAccessor = nullptr;
        cgltf_accessor* tangentAccessor = nullptr;
        cgltf_accessor* texcoordAccessor = nullptr;
        [[maybe_unused]] cgltf_accessor* colorAccessor = nullptr;

        for(size_t a = 0; a < prim.attributes_count; a++)
        {
            cgltf_attribute& attr = prim.attributes[a];
            switch(attr.type)
            {
                case cgltf_attribute_type_position: posAccessor = attr.data; break;
                case cgltf_attribute_type_normal: normalAccessor = attr.data; break;
                case cgltf_attribute_type_tangent: tangentAccessor = attr.data; break;
                case cgltf_attribute_type_color: colorAccessor = attr.data; break;
                case cgltf_attribute_type_texcoord:
                    if(attr.index == 0) texcoordAccessor = attr.data;
                    break;
                default:
                    break;
            }
        }

        SG_ASSERT(posAccessor, "Mesh primitive requires positions");
        SG_ASSERT(normalAccessor, "Mesh primitive requires normals");

        Submesh& submesh = outSpec.Submeshes.emplace_back();
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
        return submesh;
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
                AppendPrimitive(data, mesh, mesh.primitives[p], outSpec, vertexCount, indexCount);
        }

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

    //////////////////////////////////////////////////////////////////////////
    // Animation: skeleton, skin weights and clips
    //////////////////////////////////////////////////////////////////////////

    struct SkeletonBuild
    {
        Vector<int> NodeToJoint;          // glTF node -> skeleton node, -1 = not part of it
        Vector<Vector<Uint>> SkinPalette; // glTF skin -> palette entry of each of its joints
        Vector<int> RigidPalette;         // glTF node -> palette entry its unskinned geometry hangs from, -1 = none
    };

    static JointPose ReadRestPose(const cgltf_node& node)
    {
        JointPose pose;
        if(node.has_matrix)
        {
            // TRS out of the matrix by hand: column lengths are the scale, a mirrored basis flips one axis
            const glm::mat4 m = glm::make_mat4(node.matrix);
            glm::vec3 scale(glm::length(glm::vec3(m[0])), glm::length(glm::vec3(m[1])), glm::length(glm::vec3(m[2])));
            if(glm::determinant(glm::mat3(m)) < 0.0f)
                scale.x = -scale.x;
            const glm::vec3 divisor = glm::mix(scale, glm::vec3(1.0f), glm::lessThan(glm::abs(scale), glm::vec3(1e-8f))); // Degenerate axes
            const glm::quat rotation = glm::normalize(glm::quat_cast(glm::mat3(glm::vec3(m[0]) / divisor.x, glm::vec3(m[1]) / divisor.y, glm::vec3(m[2]) / divisor.z)));
            pose.Translation = glm::vec3(m[3]);
            pose.Rotation = glm::vec4(rotation.x, rotation.y, rotation.z, rotation.w);
            pose.Scale = scale;
            return pose;
        }

        if(node.has_translation)
            pose.Translation = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
        if(node.has_rotation)
            pose.Rotation = glm::vec4(node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]);
        if(node.has_scale)
            pose.Scale = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
        return pose;
    }

    static bool IsAnimatedPath(cgltf_animation_path_type path)
    {
        return path == cgltf_animation_path_type_translation || path == cgltf_animation_path_type_rotation || path == cgltf_animation_path_type_scale;
    }

    // Nodes that matter (skin joints, nodes with geometry, animated nodes and all their ancestors) in parents first order, plus the palette.
    // False when the file has nothing animated: the mesh is cooked static as before
    static bool BuildSkeleton(cgltf_data* data, MeshSpecification& spec, SkeletonBuild& build, const String& sourcePath)
    {
        bool hasAnimatedNode = false;
        for(size_t a = 0; a < data->animations_count && !hasAnimatedNode; a++)
        {
            for(size_t c = 0; c < data->animations[a].channels_count; c++)
            {
                const cgltf_animation_channel& channel = data->animations[a].channels[c];
                if(channel.target_node && IsAnimatedPath(channel.target_path))
                {
                    hasAnimatedNode = true;
                    break;
                }
            }
        }

        if(data->skins_count == 0 && !hasAnimatedNode)
            return false;

        const size_t nodeCount = data->nodes_count;
        const auto indexOf = [data](const cgltf_node* node) -> size_t { return static_cast<size_t>(node - data->nodes); };

        Vector<bool> needed(nodeCount, false);
        for(size_t s = 0; s < data->skins_count; s++)
        {
            for(size_t j = 0; j < data->skins[s].joints_count; j++)
                needed[indexOf(data->skins[s].joints[j])] = true;
        }
        for(size_t n = 0; n < nodeCount; n++)
        {
            if(data->nodes[n].mesh)
                needed[n] = true;
        }
        for(size_t a = 0; a < data->animations_count; a++)
        {
            for(size_t c = 0; c < data->animations[a].channels_count; c++)
            {
                if(const cgltf_animation_channel& channel = data->animations[a].channels[c]; channel.target_node && IsAnimatedPath(channel.target_path))
                    needed[indexOf(channel.target_node)] = true;
            }
        }
        for(size_t n = 0; n < nodeCount; n++)
        {
            if(!needed[n])
                continue;
            for(const cgltf_node* parent = data->nodes[n].parent; parent; parent = parent->parent)
                needed[indexOf(parent)] = true;
        }

        // Depth first from the roots: a parent always gets its index before its children
        Skeleton& outSkeleton = spec.Skeleton_;
        build.NodeToJoint.assign(nodeCount, -1);
        Vector<const cgltf_node*> stack;
        for(size_t n = nodeCount; n-- > 0;)
        {
            if(!data->nodes[n].parent)
                stack.push_back(&data->nodes[n]);
        }

        while(!stack.empty())
        {
            const cgltf_node* node = stack.back();
            stack.pop_back();

            const size_t index = indexOf(node);
            if(!needed[index])
                continue;

            build.NodeToJoint[index] = static_cast<int>(outSkeleton.Parents.size());
            outSkeleton.NodeNames.push_back(node->name ? node->name : std::format("Node{}", index));
            outSkeleton.Parents.push_back(node->parent ? build.NodeToJoint[indexOf(node->parent)] : -1);
            outSkeleton.RestPose.push_back(ReadRestPose(*node));

            for(size_t c = node->children_count; c-- > 0;)
                stack.push_back(node->children[c]);
        }

        if(outSkeleton.Parents.size() > UINT16_MAX)
        {
            Log<Severity::Warn>("[MeshCooker] '{}' has too many nodes to animate ({}), cooked static", sourcePath, outSkeleton.Parents.size());
            spec.Skeleton_ = {};
            return false;
        }

        const auto addEntry = [&outSkeleton](Uint joint, const glm::mat4& inverseBind) -> int {
            for(Uint e = 0; e < outSkeleton.GetPaletteSize(); e++)
            {
                if(outSkeleton.PaletteNodes[e] == joint && outSkeleton.InverseBind[e] == inverseBind)
                    return static_cast<int>(e);
            }
            if(outSkeleton.GetPaletteSize() >= Skeleton::MAX_PALETTE_SIZE)
                return -1;

            AABB bounds;
            bounds.Reset();
            outSkeleton.PaletteNodes.push_back(joint);
            outSkeleton.InverseBind.push_back(inverseBind);
            outSkeleton.PaletteBounds.push_back(bounds);
            return static_cast<int>(outSkeleton.GetPaletteSize() - 1);
        };

        bool paletteFull = false;
        build.SkinPalette.resize(data->skins_count);
        for(size_t s = 0; s < data->skins_count; s++)
        {
            const cgltf_skin& skin = data->skins[s];
            for(size_t j = 0; j < skin.joints_count; j++)
            {
                glm::mat4 inverseBind(1.0f);
                if(skin.inverse_bind_matrices)
                    cgltf_accessor_read_float(skin.inverse_bind_matrices, j, glm::value_ptr(inverseBind), 16);

                const int entry = addEntry(static_cast<Uint>(build.NodeToJoint[indexOf(skin.joints[j])]), inverseBind);
                if(entry < 0)
                    paletteFull = true;
                build.SkinPalette[s].push_back(entry < 0 ? 0u : static_cast<Uint>(entry));
            }
        }

        build.RigidPalette.assign(nodeCount, -1);
        for(size_t n = 0; n < nodeCount; n++)
        {
            if(!data->nodes[n].mesh || data->nodes[n].skin)
                continue;
            build.RigidPalette[n] = addEntry(static_cast<Uint>(build.NodeToJoint[n]), glm::mat4(1.0f));
            if(build.RigidPalette[n] < 0)
                paletteFull = true;
        }

        if(paletteFull)
        {
            Log<Severity::Warn>("[MeshCooker] '{}' needs more than {} joints, cooked static", sourcePath, Skeleton::MAX_PALETTE_SIZE);
            spec.Skeleton_ = {};
            return false;
        }
        return true;
    }

    // 4 weights -> unorm8 that add up to exactly 255 (the largest takes the rounding error)
    static VertexSkin PackSkin(const Uint entries[4], const float weights[4])
    {
        float sum = 0.0f;
        for(int i = 0; i < 4; i++)
            sum += glm::max(weights[i], 0.0f);

        int quantized[4] = { 255, 0, 0, 0 };
        if(sum > 1e-6f)
        {
            int total = 0;
            int largest = 0;
            for(int i = 0; i < 4; i++)
            {
                quantized[i] = static_cast<int>(glm::round(glm::max(weights[i], 0.0f) / sum * 255.0f));
                total += quantized[i];
                if(quantized[i] > quantized[largest])
                    largest = i;
            }
            quantized[largest] += 255 - total;
        }

        VertexSkin skin = { 0, 0 };
        for(int i = 0; i < 4; i++)
        {
            const Uint entry = quantized[i] > 0 ? entries[i] : 0u;
            skin.Joints |= (entry & 0xFFu) << (i * 8);
            skin.Weights |= static_cast<Uint>(glm::clamp(quantized[i], 0, 255)) << (i * 8);
        }
        return skin;
    }

    // Every vertex is skinned: JOINTS_0/WEIGHTS_0 for skinned nodes, one palette entry with full weight for rigid ones.
    // Skinned geometry is in model space already (Transform = identity), rigid geometry keeps its node transform for the static users
    // (picking, colliders: both see the rest pose)
    static void ExtractSkinnedGeometry(cgltf_data* data, MeshSpecification& outSpec, const SkeletonBuild& build)
    {
        Skeleton& skeleton = outSpec.Skeleton_;
        Uint vertexCount = 0;
        Uint indexCount = 0;
        for(size_t n = 0; n < data->nodes_count; n++)
        {
            const cgltf_node& node = data->nodes[n];
            if(!node.mesh)
                continue;

            float worldMat[16];
            cgltf_node_transform_world(&node, worldMat);
            float localMat[16];
            cgltf_node_transform_local(&node, localMat);

            const size_t skinIndex = node.skin ? static_cast<size_t>(node.skin - data->skins) : 0;
            for(size_t p = 0; p < node.mesh->primitives_count; p++)
            {
                const cgltf_primitive& prim = node.mesh->primitives[p];
                Submesh& submesh = AppendPrimitive(data, *node.mesh, prim, outSpec, vertexCount, indexCount);
                submesh.NodeName = node.name ? node.name : "Unnamed";
                submesh.Transform = node.skin ? glm::mat4(1.0f) : glm::make_mat4(worldMat);
                submesh.LocalTransform = node.skin ? glm::mat4(1.0f) : glm::make_mat4(localMat);

                cgltf_accessor* jointsAccessor = nullptr;
                cgltf_accessor* weightsAccessor = nullptr;
                for(size_t a = 0; a < prim.attributes_count; a++)
                {
                    if(prim.attributes[a].index != 0)
                        continue; // JOINTS_1/WEIGHTS_1 (more than 4 influences) are dropped
                    if(prim.attributes[a].type == cgltf_attribute_type_joints)
                        jointsAccessor = prim.attributes[a].data;
                    if(prim.attributes[a].type == cgltf_attribute_type_weights)
                        weightsAccessor = prim.attributes[a].data;
                }

                const bool skinned = node.skin && jointsAccessor && weightsAccessor;
                const Vector<Uint>* skinPalette = skinned ? &build.SkinPalette[skinIndex] : nullptr;
                const Uint rigidEntry = build.RigidPalette[n] >= 0 ? static_cast<Uint>(build.RigidPalette[n]) : 0u;

                for(Uint v = 0; v < submesh.VertexCount; v++)
                {
                    Uint entries[4] = { rigidEntry, 0, 0, 0 };
                    float weights[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
                    if(skinned)
                    {
                        cgltf_uint joints[4] = { 0, 0, 0, 0 };
                        cgltf_accessor_read_uint(jointsAccessor, v, joints, 4);
                        cgltf_accessor_read_float(weightsAccessor, v, weights, 4);
                        for(int i = 0; i < 4; i++)
                        {
                            const bool valid = joints[i] < skinPalette->size();
                            entries[i] = valid ? (*skinPalette)[joints[i]] : 0u;
                            if(!valid)
                                weights[i] = 0.0f;
                        }
                    }

                    const VertexSkin skin = PackSkin(entries, weights);
                    outSpec.Skin.push_back(skin);

                    // Bounds of what every palette entry moves, the runtime turns them into the skinned bounds
                    const glm::vec3& position = outSpec.Vertices[submesh.BaseVertex + v].Position;
                    for(Uint i = 0; i < 4; i++)
                    {
                        if(((skin.Weights >> (i * 8)) & 0xFFu) == 0)
                            continue;
                        AABB& bounds = skeleton.PaletteBounds[(skin.Joints >> (i * 8)) & 0xFFu];
                        bounds.Min = glm::min(bounds.Min, position);
                        bounds.Max = glm::max(bounds.Max, position);
                    }
                }
            }
        }
    }

    // One glTF sampler evaluated at t (STEP, LINEAR, CUBICSPLINE), rotations as (x, y, z, w) quaternions
    static glm::vec4 EvaluateSampler(const Vector<float>& times, const Vector<glm::vec4>& values, cgltf_interpolation_type interpolation, bool rotation, float t)
    {
        const bool cubic = interpolation == cgltf_interpolation_type_cubic_spline;
        const size_t keyCount = times.size();
        const auto value = [&](size_t key) { return values[cubic ? key * 3 + 1 : key]; };
        if(keyCount == 1 || t <= times.front())
            return value(0);
        if(t >= times.back())
            return value(keyCount - 1);

        const size_t i = static_cast<size_t>(std::upper_bound(times.begin(), times.end(), t) - times.begin()) - 1;
        const float dt = times[i + 1] - times[i];
        const float u = dt > 0.0f ? (t - times[i]) / dt : 0.0f;

        if(interpolation == cgltf_interpolation_type_step)
            return value(i);

        if(cubic)
        {
            const float u2 = u * u, u3 = u2 * u;
            const glm::vec4 result = (2.0f * u3 - 3.0f * u2 + 1.0f) * value(i) + (u3 - 2.0f * u2 + u) * dt * values[i * 3 + 2] +
                                     (-2.0f * u3 + 3.0f * u2) * value(i + 1) + (u3 - u2) * dt * values[(i + 1) * 3];
            return rotation ? glm::normalize(result) : result;
        }

        if(rotation)
        {
            const glm::vec4 a = value(i), b = value(i + 1);
            const glm::quat q = glm::slerp(glm::quat(a.w, a.x, a.y, a.z), glm::quat(b.w, b.x, b.y, b.z), u);
            return glm::vec4(q.x, q.y, q.z, q.w);
        }
        return glm::mix(value(i), value(i + 1), u);
    }

    // Resampled at AnimationClip::DEFAULT_SAMPLE_RATE, a track that never changes keeps one key
    static void ExtractClips(cgltf_data* data, MeshSpecification& spec, const SkeletonBuild& build)
    {
        const float rate = AnimationClip::DEFAULT_SAMPLE_RATE;
        for(size_t a = 0; a < data->animations_count; a++)
        {
            const cgltf_animation& animation = data->animations[a];

            float start = FLT_MAX, end = -FLT_MAX;
            for(size_t c = 0; c < animation.channels_count; c++)
            {
                const cgltf_animation_channel& channel = animation.channels[c];
                if(!channel.target_node || !IsAnimatedPath(channel.target_path) || !channel.sampler || channel.sampler->input->count == 0)
                    continue;

                float first = 0.0f, last = 0.0f;
                cgltf_accessor_read_float(channel.sampler->input, 0, &first, 1);
                cgltf_accessor_read_float(channel.sampler->input, channel.sampler->input->count - 1, &last, 1);
                start = std::min(start, first);
                end = std::max(end, last);
            }
            if(start > end)
                continue; // Nothing this engine animates (morph target weights only)

            AnimationClip clip;
            clip.Name = animation.name ? animation.name : std::format("Clip{}", a);
            clip.SampleRate = rate;
            clip.Duration = end - start;
            clip.FrameCount = static_cast<Uint>(std::ceil(clip.Duration * rate - 1e-3f)) + 1;

            Vector<float> times;
            Vector<glm::vec4> values;
            Vector<glm::vec4> samples(clip.FrameCount);
            for(size_t c = 0; c < animation.channels_count; c++)
            {
                const cgltf_animation_channel& channel = animation.channels[c];
                if(!channel.target_node || !IsAnimatedPath(channel.target_path) || !channel.sampler || channel.sampler->input->count == 0)
                    continue;

                const cgltf_animation_sampler& sampler = *channel.sampler;
                const bool rotation = channel.target_path == cgltf_animation_path_type_rotation;
                const int components = rotation ? 4 : 3;
                const bool cubic = sampler.interpolation == cgltf_interpolation_type_cubic_spline;

                times.resize(sampler.input->count);
                for(size_t k = 0; k < times.size(); k++)
                    cgltf_accessor_read_float(sampler.input, k, &times[k], 1);

                values.assign(sampler.output->count, glm::vec4(0.0f));
                for(size_t k = 0; k < values.size(); k++)
                    cgltf_accessor_read_float(sampler.output, k, glm::value_ptr(values[k]), components);
                if(values.size() < times.size() * (cubic ? 3 : 1))
                    continue; // Malformed sampler

                bool constant = true;
                for(Uint k = 0; k < clip.FrameCount; k++)
                {
                    const float t = start + std::min(static_cast<float>(k) / rate, clip.Duration);
                    samples[k] = EvaluateSampler(times, values, sampler.interpolation, rotation, t);
                    const float difference = rotation ? glm::min(glm::length(samples[k] - samples[0]), glm::length(samples[k] + samples[0]))
                                                      : glm::length(samples[k] - samples[0]);
                    if(difference >= 1e-5f)
                        constant = false;
                }

                AnimationTrack track = {};
                track.Node = static_cast<uint16_t>(build.NodeToJoint[static_cast<size_t>(channel.target_node - data->nodes)]);
                track.KeyCount = constant ? 1 : clip.FrameCount;
                switch(channel.target_path)
                {
                    case cgltf_animation_path_type_translation:
                        track.Path = AnimationPath::TRANSLATION;
                        track.FirstKey = static_cast<Uint>(clip.Translations.size());
                        for(Uint k = 0; k < track.KeyCount; k++) clip.Translations.push_back(glm::vec3(samples[k]));
                        break;
                    case cgltf_animation_path_type_rotation:
                        track.Path = AnimationPath::ROTATION;
                        track.FirstKey = static_cast<Uint>(clip.Rotations.size());
                        for(Uint k = 0; k < track.KeyCount; k++) clip.Rotations.push_back(samples[k]);
                        break;
                    default:
                        track.Path = AnimationPath::SCALE;
                        track.FirstKey = static_cast<Uint>(clip.Scales.size());
                        for(Uint k = 0; k < track.KeyCount; k++) clip.Scales.push_back(glm::vec3(samples[k]));
                        break;
                }
                clip.Tracks.push_back(track);
            }

            if(!clip.Tracks.empty())
                spec.Clips.push_back(std::move(clip));
        }
    }

    static const char* GetImageExtension(const Byte* bytes, size_t size)
    {
        if(size >= 8 && memcmp(bytes, "\x89PNG", 4) == 0)
            return ".png";
        if(size >= 3 && bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF)
            return ".jpg";
        return nullptr;
    }

    // Leaves an identical file untouched: its write time is an input of the texture's cooked file, rewriting it would re-cook the texture
    static bool WriteFileIfChanged(const Path& path, const Byte* bytes, size_t size)
    {
        Vector<Byte> existing;
        if(Filesystem::Exists(path) && Filesystem::ReadBinaryFile(path, existing) && existing.size() == size && memcmp(existing.data(), bytes, size) == 0)
            return true;

        return Filesystem::WriteBinaryFile(path, bytes, size);
    }

    // Embedded images (.glb buffer views, base64 data URIs) have no source file the texture pipeline could cook, so they are written next to
    // the mesh as "<Mesh>_Textures/Image<N>_<Usage>.png|jpg" and imported like any other texture. The usage suffix picks the color space
    // (Texture2DCooker::IsLinearColorSpace reads the file name). Returns the absolute path, empty on failure
    static Path ExtractEmbeddedImage(const Path& meshPath, cgltf_data* data, cgltf_image* image, const char* usage)
    {
        const Uint imageIndex = static_cast<Uint>(cgltf_image_index(data, image));
        const Byte* bytes = nullptr;
        size_t size = 0;
        void* decoded = nullptr;

        if(image->buffer_view)
        {
            bytes = cgltf_buffer_view_data(image->buffer_view);
            size = image->buffer_view->size;
        }
        else // data:image/png;base64,...
        {
            const char* comma = strchr(image->uri, ',');
            if(!comma || !strstr(image->uri, ";base64"))
            {
                Log<Severity::Warn>("[MeshCooker] Image {} of '{}' has an unsupported data URI, skipped", imageIndex, meshPath.string());
                return {};
            }

            const char* base64 = comma + 1;
            const size_t length = strlen(base64);
            size_t padding = 0;
            while(padding < 2 && padding < length && base64[length - 1 - padding] == '=')
                padding++;

            size = length / 4 * 3;
            size = size > padding ? size - padding : 0;
            cgltf_options options = {};
            if(size == 0 || cgltf_load_buffer_base64(&options, size, base64, &decoded) != cgltf_result_success)
            {
                Log<Severity::Warn>("[MeshCooker] Image {} of '{}' has a malformed data URI, skipped", imageIndex, meshPath.string());
                return {};
            }
            bytes = static_cast<const Byte*>(decoded);
        }

        Path imagePath;
        const char* extension = bytes ? GetImageExtension(bytes, size) : nullptr;
        if(extension)
        {
            const Path directory = Filesystem::GetParentPath(meshPath) / (Filesystem::GetFilenameWithoutExt(meshPath) + "_Textures");
            imagePath = directory / std::format("Image{}_{}{}", imageIndex, usage, extension);

            Filesystem::CreateOrEnsureDirectories(directory);
            if(!Filesystem::Exists(directory) || !WriteFileIfChanged(imagePath, bytes, size))
            {
                Log<Severity::Error>("[MeshCooker] Failed to write the embedded image '{}'", imagePath.string());
                imagePath.clear();
            }
        }
        else
            Log<Severity::Warn>("[MeshCooker] Image {} of '{}' is not a PNG or JPEG ({}), skipped", imageIndex, meshPath.string(), image->mime_type ? image->mime_type : "unknown type");

        free(decoded); // (Rid IMPORTANT) cgltf's default allocator is malloc Look here if someone ever implements a custom allocator that doesn't use malloc/free
        return imagePath;
    }

    static void ExtractMaterials(const String& absPath, cgltf_data* data, MeshSpecification& spec)
    {
        AssetManager* assetManager = Core::GetAssetManager();
        bool importedTextures = false;

        auto SetMaterialTexture = [&](const Path& meshPath, cgltf_texture_view& texView, Ref<Material>& material, const String& texName, const char* usage) -> bool {
            if(!texView.texture || !texView.texture->image)
                return false;

            cgltf_image* image = texView.texture->image;

            Path absTexturePath;
            if(image->buffer_view || (image->uri && strncmp(image->uri, "data:", 5) == 0))
                absTexturePath = ExtractEmbeddedImage(meshPath, data, image, usage);
            else if(image->uri)
            {
                String uri = image->uri;
                uri.resize(cgltf_decode_uri(uri.data())); // "My%20Texture.png" -> "My Texture.png"
                absTexturePath = (Filesystem::GetParentPath(meshPath) / uri).lexically_normal();
            }

            if(absTexturePath.empty())
                return false;

            const Path relTexturePath = absTexturePath.lexically_relative(Path(assetManager->GetAssetsDirectory()));
            if(relTexturePath.empty() || *relTexturePath.begin() == "..")
            {
                Log<Severity::Warn>("[Mesh] Texture '{}' is outside the assets directory, skipping.", absTexturePath.string());
                return false;
            }

            const String relativePath = relTexturePath.generic_string();
            if(!assetManager->GetIDFromPath(relativePath).IsValid())
                importedTextures = true;
            const AssetID texID = AssetDatabase::Get().Import(relativePath, AssetType::TEXTURE2D, false);

            if(!texID.IsValid())
                return false;

            material->SetTexture(texName, assetManager->Load<Texture2D>(texID));
            return true;
            };

        if(data->materials_count > 0)
        {
            spec.Materials.resize(data->materials_count);

            for(size_t i = 0; i < data->materials_count; i++)
            {
                cgltf_material& mat = data->materials[i];
                const String materialName = mat.name ? mat.name : "Unnamed";

                Ref<Material> material = Material::Create(*Core::GetRenderer(), materialName);
                spec.Materials[i] = material;

                ImageHandle whiteTexture = Core::GetRenderer()->GetWhiteTexture();
                material->SetTexture("AlbedoMap", whiteTexture);
                material->SetTexture("NormalMap", whiteTexture);
                material->SetTexture("RoughnessMetallicMap", whiteTexture);
                material->SetTexture("EmissionMap", whiteTexture);

                if(mat.has_pbr_metallic_roughness)
                {
                    auto& pbr = mat.pbr_metallic_roughness;
                    material->Set<glm::vec3>("Albedo", glm::vec3(pbr.base_color_factor[0], pbr.base_color_factor[1], pbr.base_color_factor[2]));
                    material->Set<float>("Roughness", pbr.roughness_factor);
                    material->Set<float>("Metallic", pbr.metallic_factor);
                    material->Set<float>("Reflectance", 0.5f);

                    if(SetMaterialTexture(absPath, pbr.base_color_texture, material, "AlbedoMap", "BaseColor"))
                        material->Set<int>("UseAlbedoMap", 1);
                    if(SetMaterialTexture(absPath, pbr.metallic_roughness_texture, material, "RoughnessMetallicMap", "MetallicRoughness"))
                    {
                        material->Set<int>("UseRoughnessMap", 1);
                        material->Set<int>("UseMetallicMap", 1);
                    }
                }
                if(SetMaterialTexture(absPath, mat.normal_texture, material, "NormalMap", "Normal"))
                    material->Set<int>("UseNormalMap", 1);
                else
                    material->Set<int>("UseNormalMap", 0);

                material->Set<glm::vec3>("Emission", glm::vec3(mat.emissive_factor[0], mat.emissive_factor[1], mat.emissive_factor[2]));
                float emissiveStrength = mat.has_emissive_strength ? mat.emissive_strength.emissive_strength : 1.0f;
                material->Set<float>("EmissionIntensity", emissiveStrength);
                if(SetMaterialTexture(absPath, mat.emissive_texture, material, "EmissionMap", "Emission"))
                    material->Set<int>("UseEmissionMap", 1);
            }
        }

        // The mesh binary references the textures by ID, which must survive an Editor restart
        if(importedTextures)
            AssetDatabase::Get().SaveRegistry();
    }

    static void FreeGLTF(cgltf_data* data)
    {
        cgltf_free(data);
    }

    static const float* PositionOf(const Vertex& v) { return &v.Position.x; }
    static const float* PositionOf(const SkinnedVertex& v) { return &v.Base.Position.x; }

    template <typename V>
    static void OptimizeSubmeshes(Vector<Submesh>& submeshes, Vector<Index>& allIndices, V* allVertices)
    {
        for(Submesh& sm : submeshes)
        {
            if(sm.IndexCount == 0 || sm.VertexCount == 0)
                continue;

            Uint* indices = reinterpret_cast<Uint*>(allIndices.data()) + sm.BaseIndex;
            V* vertices = allVertices + sm.BaseVertex;

            Vector<Uint> remap(sm.VertexCount);
            size_t uniqueVerts = meshopt_generateVertexRemap(remap.data(), indices, sm.IndexCount, vertices, sm.VertexCount, sizeof(V));

            if (sm.VertexCount != uniqueVerts)
                Log<Severity::Warn>("[MeshCooker] Old Vertex Count: {} | New Vertex Count: {}", sm.VertexCount, uniqueVerts);

            meshopt_remapIndexBuffer(indices, indices, sm.IndexCount, remap.data());
            meshopt_remapVertexBuffer(vertices, vertices, sm.VertexCount, sizeof(V), remap.data());
            meshopt_optimizeVertexCache(indices, indices, sm.IndexCount, uniqueVerts);
            meshopt_optimizeOverdraw(indices, indices, sm.IndexCount, PositionOf(vertices[0]), uniqueVerts, sizeof(V), 1.05f);
            meshopt_optimizeVertexFetch(vertices, indices, sm.IndexCount, vertices, uniqueVerts, sizeof(V));

            sm.VertexCount = uniqueVerts;
        }
    }

    static void OptimizeMesh(MeshSpecification& spec)
    {
        if(spec.Skin.empty())
        {
            OptimizeSubmeshes(spec.Submeshes, spec.Indices, spec.Vertices.data());
            return;
        }

        // The skin influences are part of a vertex's identity: two vertices only merge when their weights match too
        Vector<SkinnedVertex> vertices(spec.Vertices.size());
        for(size_t i = 0; i < vertices.size(); i++)
            vertices[i] = { spec.Vertices[i], spec.Skin[i] };

        OptimizeSubmeshes(spec.Submeshes, spec.Indices, vertices.data());

        for(size_t i = 0; i < vertices.size(); i++)
        {
            spec.Vertices[i] = vertices[i].Base;
            spec.Skin[i] = vertices[i].Skin;
        }
    }

    bool MeshCooker::Cook(const CookContext& context) const
    {
        const String sourceAbsPath = context.SourcePath.generic_string();
        cgltf_data* data = ParseGLTF(sourceAbsPath);
        if(!data)
            return false;

        MeshSpecification spec;

        if(SkeletonBuild skeletonBuild; BuildSkeleton(data, spec, skeletonBuild, sourceAbsPath))
        {
            ExtractSkinnedGeometry(data, spec, skeletonBuild);
            ExtractClips(data, spec, skeletonBuild);
            Log<Severity::Info>("[MeshCooker] '{}': {} nodes, {} joints, {} clips", sourceAbsPath, spec.Skeleton_.GetNodeCount(), spec.Skeleton_.GetPaletteSize(), spec.Clips.size());
        }
        else
            ExtractGeometry(data, spec);

        ExtractMaterials(sourceAbsPath, data, spec);
        FreeGLTF(data);
        OptimizeMesh(spec);

        // Material overrides are the model's import settings (Inspector), baked into the cooked mesh
        Vector<AssetID> overrides(spec.Materials.size(), AssetID(AssetID::INVALID));
        for(const auto& [slot, materialID] : context.Settings.MaterialOverrides)
        {
            if(slot < overrides.size())
                overrides[slot] = materialID;
            else
                Log<Severity::Warn>("[MeshCooker] '{}' has an override for material slot {}, the model only has {}", sourceAbsPath, slot, overrides.size());
        }

        Rapt0R::Writer payload(spec.Vertices.size() * (sizeof(Vertex) + sizeof(VertexSkin)) + spec.Indices.size() * sizeof(Index) + 4096);
        MeshBinary::Write(payload, spec, overrides);
        return context.Write(MeshBinary::VERSION, payload);
    }
}
