// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Animation/Skeleton.hpp"
#include <glm/gtc/quaternion.hpp>

namespace Surge
{
    glm::mat4 JointPose::ToMatrix() const
    {
        const glm::mat3 rotation = glm::mat3_cast(glm::quat(Rotation.w, Rotation.x, Rotation.y, Rotation.z));
        glm::mat4 result;
        result[0] = glm::vec4(rotation[0] * Scale.x, 0.0f);
        result[1] = glm::vec4(rotation[1] * Scale.y, 0.0f);
        result[2] = glm::vec4(rotation[2] * Scale.z, 0.0f);
        result[3] = glm::vec4(Translation, 1.0f);
        return result;
    }

    int Skeleton::FindNode(const String& name) const
    {
        for(Uint i = 0; i < static_cast<Uint>(NodeNames.size()); i++)
        {
            if(NodeNames[i] == name)
                return static_cast<int>(i);
        }
        return -1;
    }

    void Skeleton::BuildPalette(const JointPose* pose, glm::mat4* outWorld, SkinMatrix* outPalette) const
    {
        const Uint nodeCount = GetNodeCount();
        for(Uint i = 0; i < nodeCount; i++)
        {
            const glm::mat4 local = pose[i].ToMatrix();
            outWorld[i] = Parents[i] < 0 ? local : outWorld[Parents[i]] * local; // Parents first: the parent is done already
        }

        const Uint paletteSize = GetPaletteSize();
        for(Uint i = 0; i < paletteSize; i++)
            outPalette[i] = SkinMatrix::FromMat4(outWorld[PaletteNodes[i]] * InverseBind[i]);
    }

    AABB Skeleton::ComputeBounds(const SkinMatrix* palette) const
    {
        // A blended vertex lies between the positions its palette entries give it, so the union of every entry's moved box contains it
        AABB bounds;
        bounds.Reset();
        const Uint paletteSize = GetPaletteSize();
        for(Uint i = 0; i < paletteSize; i++)
        {
            const AABB& local = PaletteBounds[i];
            if(local.Min.x > local.Max.x)
                continue; // Moves no vertex

            const glm::vec3 center = (local.Min + local.Max) * 0.5f;
            const glm::vec3 extents = (local.Max - local.Min) * 0.5f;
            const SkinMatrix& m = palette[i];
            glm::vec3 movedCenter, movedExtents;
            for(int r = 0; r < 3; r++)
            {
                movedCenter[r] = glm::dot(glm::vec3(m.Rows[r]), center) + m.Rows[r].w;
                movedExtents[r] = glm::dot(glm::abs(glm::vec3(m.Rows[r])), extents);
            }
            bounds.Min = glm::min(bounds.Min, movedCenter - movedExtents);
            bounds.Max = glm::max(bounds.Max, movedCenter + movedExtents);
        }

        if(bounds.Min.x > bounds.Max.x)
            bounds = AABB(glm::vec3(0.0f), glm::vec3(0.0f));
        return bounds;
    }

} // namespace Surge
