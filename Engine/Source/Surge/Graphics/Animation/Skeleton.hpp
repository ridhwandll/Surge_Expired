// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Vector.hpp"
#include "Surge/Graphics/HighLevel/DefaultMeshes.hpp"
#include "SurgeMath/AABB.hpp"
#include <glm/glm.hpp>

namespace Surge
{
    // Row major 3x4 matrix (the last row of a skin matrix is always 0 0 0 1): 48 bytes per palette entry on the GPU (Skinning.glsli)
    struct SkinMatrix
    {
        glm::vec4 Rows[3];

        static SkinMatrix FromMat4(const glm::mat4& m)
        {
            return { { glm::vec4(m[0][0], m[1][0], m[2][0], m[3][0]),
                       glm::vec4(m[0][1], m[1][1], m[2][1], m[3][1]),
                       glm::vec4(m[0][2], m[1][2], m[2][2], m[3][2]) } };
        }
    };
    static_assert(sizeof(SkinMatrix) == 48, "SkinMatrix is mirrored by Skinning.glsli");

    // Local transform of one node. Rotation is a quaternion stored as (x, y, z, w)
    struct JointPose
    {
        glm::vec3 Translation = glm::vec3(0.0f);
        glm::vec4 Rotation = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        glm::vec3 Scale = glm::vec3(1.0f);

        glm::mat4 ToMatrix() const;
    };

    // Skin influences of one vertex, interleaved behind the Vertex in a skinned mesh's GPU buffer (aJoints/aWeights in Skinning.glsli)
    struct VertexSkin
    {
        Uint Joints;  // 4 x u8 palette indices
        Uint Weights; // 4 x unorm8, they add up to 255
    };

    struct SkinnedVertex
    {
        Vertex Base;
        VertexSkin Skin;
    };
    static_assert(sizeof(SkinnedVertex) == sizeof(Vertex) + sizeof(VertexSkin), "SkinnedVertex must stay tightly packed (shader reflection stride)");

    // Where linear blend skinning puts a vertex with this palette, the CPU side of Skinning.glsli (picking, colliders)
    inline glm::vec3 SkinPosition(const SkinMatrix* palette, const VertexSkin& skin, const glm::vec3& position)
    {
        glm::vec4 rows[3] = { glm::vec4(0.0f), glm::vec4(0.0f), glm::vec4(0.0f) };
        for(Uint i = 0; i < 4; i++)
        {
            const float weight = static_cast<float>((skin.Weights >> (i * 8)) & 0xFFu) / 255.0f;
            if(weight == 0.0f)
                continue;

            const SkinMatrix& m = palette[(skin.Joints >> (i * 8)) & 0xFFu];
            for(int r = 0; r < 3; r++)
                rows[r] += m.Rows[r] * weight;
        }

        const glm::vec4 p(position, 1.0f);
        return glm::vec3(glm::dot(rows[0], p), glm::dot(rows[1], p), glm::dot(rows[2], p));
    }

    // The glTF nodes that move or carry geometry, parents first. Vertices index the palette: a skin joint (node + inverse bind matrix)
    // or a node rigid geometry hangs from (identity inverse bind), so animated props and skinned characters share one path
    struct Skeleton
    {
        static constexpr Uint MAX_PALETTE_SIZE = 256; // u8 joint indices

        Vector<String> NodeNames;
        Vector<int> Parents;         // -1 = root, always lower than the node's own index
        Vector<JointPose> RestPose;  // Local transform of every node when no clip drives it

        Vector<Uint> PaletteNodes;       // Palette entry -> node
        Vector<glm::mat4> InverseBind;   // Per palette entry
        Vector<AABB> PaletteBounds;      // Per palette entry: the vertices it moves, in vertex space (runtime bounds)

        bool IsValid() const { return !PaletteNodes.empty(); }
        Uint GetNodeCount() const { return static_cast<Uint>(Parents.size()); }
        Uint GetPaletteSize() const { return static_cast<Uint>(PaletteNodes.size()); }
        int FindNode(const String& name) const;

        // pose: one local transform per node -> outWorld (model space, one per node) -> outPalette (one per palette entry)
        void BuildPalette(const JointPose* pose, glm::mat4* outWorld, SkinMatrix* outPalette) const;
        // Model space bounds of the skinned mesh for this palette
        AABB ComputeBounds(const SkinMatrix* palette) const;
    };

} // namespace Surge
