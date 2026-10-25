// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Picking/ScenePicker.hpp"
#include "Surge/ECS/Components/Components.hpp"
#include "Surge/Graphics/HighLevel/Mesh.hpp"
#include "Surge/Graphics/HighLevel/Font.hpp"
#include "Surge/Core/Profiler.hpp"

#include <algorithm>

namespace Surge
{
    static constexpr float CYCLE_CLICK_RADIUS = 3.0f; // Pixels, a click this close to the previous one cycles through the hits

    // (Rid)
    // Instead of transforming the model vertices to world space and intersecting the ray there, we transform the ray to the model's local space
    // and intersect it with the model's local vertices. LocalRay is a ray in a primitive's local space: the direction is transformed as-is (not renormalized),
    // so a hit parameter t found there is the same world space distance along the original normalized ray
    struct LocalRay
    {
        glm::vec3 Origin;
        glm::vec3 Direction;
    };

    static bool ToLocalRay(const glm::mat4& localToWorld, const PickRay& ray, LocalRay& outRay)
    {
        if(glm::abs(glm::determinant(localToWorld)) < 1e-12f) // Zero scale, nothing to hit
            return false;

        const glm::mat4 worldToLocal = glm::inverse(localToWorld);
        outRay.Origin = glm::vec3(worldToLocal * glm::vec4(ray.Origin, 1.0f));
        outRay.Direction = glm::vec3(worldToLocal * glm::vec4(ray.Direction, 0.0f));
        return true;
    }

    static bool IntersectAABB(const LocalRay& ray, const AABB& box, float& outEnter)
    {
        float tMin = 0.0f;
        float tMax = FLT_MAX;
        for(int axis = 0; axis < 3; axis++)
        {
            if(glm::abs(ray.Direction[axis]) < 1e-8f)
            {
                if(ray.Origin[axis] < box.Min[axis] || ray.Origin[axis] > box.Max[axis])
                    return false;
                continue;
            }

            const float invDirection = 1.0f / ray.Direction[axis];
            float t0 = (box.Min[axis] - ray.Origin[axis]) * invDirection;
            float t1 = (box.Max[axis] - ray.Origin[axis]) * invDirection;
            if(t0 > t1)
                std::swap(t0, t1);

            tMin = glm::max(tMin, t0);
            tMax = glm::min(tMax, t1);
            if(tMin > tMax)
                return false;
        }
        outEnter = tMin;
        return true;
    }

    // Moller-Trumbore, double sided (the editor should pick a mesh seen from inside or with flipped winding too)
    static bool IntersectTriangle(const LocalRay& ray, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, float& outT)
    {
        const glm::vec3 edge1 = v1 - v0;
        const glm::vec3 edge2 = v2 - v0;
        const glm::vec3 p = glm::cross(ray.Direction, edge2);
        const float determinant = glm::dot(edge1, p);
        if(glm::abs(determinant) < 1e-12f)
            return false;

        const float invDeterminant = 1.0f / determinant;
        const glm::vec3 s = ray.Origin - v0;
        const float u = glm::dot(s, p) * invDeterminant;
        if(u < 0.0f || u > 1.0f)
            return false;

        const glm::vec3 q = glm::cross(s, edge1);
        const float v = glm::dot(ray.Direction, q) * invDeterminant;
        if(v < 0.0f || u + v > 1.0f)
            return false;

        const float t = glm::dot(edge2, q) * invDeterminant;
        if(t < 0.0f)
            return false;

        outT = t;
        return true;
    }

    // Skinned meshes, in the pose they are drawn with: the vertices go through the palette (any rotation/scale the glTF hierarchy has
    // reaches them only that way, Z-up files for example), one box (the pose's bounds) for the whole mesh
    static bool IntersectSkinnedMesh(const Mesh& mesh, const glm::mat4& world, const SkinMatrix* palette, const AABB& bounds, const PickRay& ray, float& outDistance)
    {
        const Vector<Vertex>& vertices = mesh.GetVertices();
        const Vector<VertexSkin>& skin = mesh.GetSkin();
        const Vector<Index>& triangles = mesh.GetIndices();
        const Uint vertexCount = static_cast<Uint>(std::min(vertices.size(), skin.size()));
        const Uint triangleCount = static_cast<Uint>(triangles.size());

        LocalRay localRay;
        float enter;
        if(!ToLocalRay(world, ray, localRay) || !IntersectAABB(localRay, bounds, enter))
            return false;

        float closest = FLT_MAX;
        for(const Submesh& submesh : mesh.GetSubmeshes())
        {
            const Uint firstTriangle = submesh.BaseIndex / 3;
            const Uint lastTriangle = std::min(firstTriangle + submesh.IndexCount / 3, triangleCount);
            for(Uint i = firstTriangle; i < lastTriangle; i++)
            {
                const Index& triangle = triangles[i];
                const Uint i0 = submesh.BaseVertex + triangle.V1;
                const Uint i1 = submesh.BaseVertex + triangle.V2;
                const Uint i2 = submesh.BaseVertex + triangle.V3;
                if(i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount)
                    continue;

                float t;
                if(IntersectTriangle(localRay, SkinPosition(palette, skin[i0], vertices[i0].Position), SkinPosition(palette, skin[i1], vertices[i1].Position),
                                     SkinPosition(palette, skin[i2], vertices[i2].Position), t) && t < closest)
                    closest = t;
            }
        }

        outDistance = closest;
        return closest < FLT_MAX;
    }

    static bool IntersectMesh(const Mesh& mesh, const glm::mat4& world, const PickRay& ray, float& outDistance)
    {
        const Vector<Vertex>& vertices = mesh.GetVertices();
        const Vector<Index>& triangles = mesh.GetIndices(); // One Index = one triangle
        const Uint vertexCount = static_cast<Uint>(vertices.size());
        const Uint triangleCount = static_cast<Uint>(triangles.size());

        float closest = FLT_MAX;
        for(const Submesh& submesh : mesh.GetSubmeshes())
        {
            LocalRay localRay;
            float enter;
            if(!ToLocalRay(world * submesh.Transform, ray, localRay) || !IntersectAABB(localRay, submesh.BoundingBox, enter) || enter >= closest)
                continue;

            const Uint firstTriangle = submesh.BaseIndex / 3;
            const Uint lastTriangle = std::min(firstTriangle + submesh.IndexCount / 3, triangleCount);
            if(firstTriangle >= lastTriangle)
            {
                // (Rid) No CPU side triangles (Mesh::mVertices/mIndices may be dropped soon, waste of memory in Mesh.cpp), the bounds are the best we have.
                // Submeshes without vertices get "infinite" bounds so they're never culled, those must not swallow every click
                if(vertices.empty() && submesh.BoundingBox.Max.x < FLT_MAX * 0.25f)
                    closest = enter;
                continue;
            }

            for(Uint i = firstTriangle; i < lastTriangle; i++)
            {
                const Index& triangle = triangles[i];
                const Uint i0 = submesh.BaseVertex + triangle.V1;
                const Uint i1 = submesh.BaseVertex + triangle.V2;
                const Uint i2 = submesh.BaseVertex + triangle.V3;
                if(i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount)
                    continue;

                float t;
                if(IntersectTriangle(localRay, vertices[i0].Position, vertices[i1].Position, vertices[i2].Position, t) && t < closest)
                    closest = t;
            }
        }

        outDistance = closest;
        return closest < FLT_MAX;
    }

    // Flat rectangle on the local XY plane (sprites, text)
    static bool IntersectQuad(const glm::mat4& localToWorld, const PickRay& ray, const glm::vec2& min, const glm::vec2& max, float& outDistance)
    {
        LocalRay localRay;
        if(!ToLocalRay(localToWorld, ray, localRay) || glm::abs(localRay.Direction.z) < 1e-8f)
            return false;

        const float t = -localRay.Origin.z / localRay.Direction.z;
        if(t < 0.0f)
            return false;

        const glm::vec3 p = localRay.Origin + localRay.Direction * t;
        if(p.x < min.x || p.x > max.x || p.y < min.y || p.y > max.y)
            return false;

        outDistance = t;
        return true;
    }

    // Billboards keep position and scale but face the camera, built exactly like Renderer2DPass does
    static glm::mat4 GetFacingTransform(const glm::mat4& world, bool billboard, const glm::vec3& cameraRight, const glm::vec3& cameraUp)
    {
        if(!billboard)
            return world;

        const glm::vec3 forward = glm::normalize(glm::cross(cameraRight, cameraUp));
        return glm::mat4(glm::vec4(cameraRight * glm::length(glm::vec3(world[0])), 0.0f),
                         glm::vec4(cameraUp * glm::length(glm::vec3(world[1])), 0.0f),
                         glm::vec4(forward * glm::length(glm::vec3(world[2])), 0.0f),
                         glm::vec4(glm::vec3(world[3]), 1.0f));
    }

    // Local bounds of a text block, mirrors the line measurement of Renderer2DPass (wrapping, kerning, alignment)
    static bool MeasureText(const TextComponent& text, const Font& font, glm::vec2& outMin, glm::vec2& outMax)
    {
        float lineWidth = 0.0f;
        float minX = FLT_MAX;
        float maxX = -FLT_MAX;
        Uint lineCount = 0;
        Uint prevChar = 0;

        auto endLine = [&]() {
            float startX = 0.0f;
            if(text.Alignment == TextAlignment::CENTER)
                startX = -lineWidth * 0.5f;
            else if(text.Alignment == TextAlignment::RIGHT)
                startX = -lineWidth;

            minX = glm::min(minX, startX);
            maxX = glm::max(maxX, startX + lineWidth);
            lineCount++;
            lineWidth = 0.0f;
            prevChar = 0;
        };

        for(size_t i = 0; i < text.Text.size(); i++)
        {
            const char c = text.Text[i];
            if(text.MaxWidth > 0.0f && c == ' ')
            {
                float nextWordLength = 0.0f;
                for(size_t j = i + 1; j < text.Text.size() && text.Text[j] != ' ' && text.Text[j] != '\n'; j++)
                {
                    if(const FontGlyph* nextGlyph = font.GetGlyph(text.Text[j]))
                        nextWordLength += nextGlyph->Advance;
                }

                if(lineWidth + nextWordLength > text.MaxWidth)
                {
                    endLine();
                    continue;
                }
            }
            if(c == '\n')
            {
                endLine();
                continue;
            }

            const FontGlyph* glyph = font.GetGlyph(c);
            if(!glyph)
                continue;

            if(prevChar != 0)
                lineWidth += font.GetKerning(prevChar, c);

            lineWidth += glyph->Advance + text.LetterSpacing;
            prevChar = c;
        }
        endLine();

        if(maxX <= minX)
            return false;

        const float lineHeight = font.GetLineHeight();
        const float capHeight = lineHeight * 0.7f;
        const float descent = lineHeight * 0.2f;
        const float blockDrop = static_cast<float>(lineCount - 1) * (lineHeight + text.LineSpacing);

        float baseline = 0.0f; // BASELINE
        if(text.VerticalAlignment == TextVerticalAlignment::CENTER)
            baseline = (blockDrop + descent - capHeight) * 0.5f;
        else if(text.VerticalAlignment == TextVerticalAlignment::TOP)
            baseline = -capHeight;
        else if(text.VerticalAlignment == TextVerticalAlignment::BOTTOM)
            baseline = blockDrop + descent;

        // Lines go down from the first baseline, glyphs reach about one cap height above it
        outMin = glm::vec2(minX, baseline - blockDrop - descent);
        outMax = glm::vec2(maxX, baseline + capHeight);
        return true;
    }

    PickRay ScenePicker::ScreenToRay(const glm::vec2& screenPos, const glm::vec2& viewportMin, const glm::vec2& viewportSize, const glm::mat4& view, const glm::mat4& projection)
    {
        // Vulkan clip space: y down like the screen, depth 0..1
        const glm::vec2 ndc = (screenPos - viewportMin) / viewportSize * 2.0f - 1.0f;
        const glm::mat4 inverseViewProjection = glm::inverse(projection * view);

        glm::vec4 nearPoint = inverseViewProjection * glm::vec4(ndc, 0.0f, 1.0f);
        glm::vec4 farPoint = inverseViewProjection * glm::vec4(ndc, 1.0f, 1.0f);
        nearPoint /= nearPoint.w;
        farPoint /= farPoint.w;

        return PickRay { glm::vec3(nearPoint), glm::normalize(glm::vec3(farPoint - nearPoint)) };
    }

    void ScenePicker::CollectHits(Scene& scene, const PickRay& ray, const glm::mat4& view)
    {
        mHits.clear();
        entt::registry& registry = scene.GetRegistry();

        for(const auto& [entity, meshComp, transform] : registry.view<MeshComponent, TransformComponent>().each())
        {
            if(!meshComp.Active || !meshComp.MeshAsset)
                continue;

            Ref<Mesh> mesh = meshComp.MeshAsset.As<Mesh>();
            if(!mesh)
                continue;

            float distance;
            bool hit;
            if(mesh->IsSkinned() && !mesh->GetSkin().empty())
            {
                // The animator's pose when it drew one, the rest pose otherwise
                const AnimatorComponent* animator = registry.try_get<AnimatorComponent>(entity);
                const bool posed = animator && animator->Active && animator->Runtime.HasPose() && animator->Runtime.GetMesh() == mesh.Raw();
                const SkinMatrix* palette = posed ? animator->Runtime.GetPalette().data() : mesh->GetRestPalette().data();
                const AABB& bounds = posed ? animator->Runtime.GetBounds() : mesh->GetRestBounds();
                hit = IntersectSkinnedMesh(*mesh, transform.GetTransform(), palette, bounds, ray, distance);
            }
            else
                hit = IntersectMesh(*mesh, transform.GetTransform(), ray, distance);

            if(hit)
                mHits.push_back({ entity, distance });
        }

        const glm::vec3 cameraRight = { view[0][0], view[1][0], view[2][0] };
        const glm::vec3 cameraUp = { view[0][1], view[1][1], view[2][1] };
        for(const auto& [entity, sprite, transform] : registry.view<SpriteRendererComponent, TransformComponent>().each())
        {
            if(!sprite.Active)
                continue;

            float distance;
            if(IntersectQuad(GetFacingTransform(transform.GetTransform(), sprite.Billboard, cameraRight, cameraUp), ray, glm::vec2(-0.5f), glm::vec2(0.5f), distance))
                mHits.push_back({ entity, distance });
        }

        for(const auto& [entity, textComp, transform] : registry.view<TextComponent, TransformComponent>().each())
        {
            if(!textComp.Active || !textComp.FontAsset || textComp.Text.empty())
                continue;

            Ref<Font> font = textComp.FontAsset.As<Font>();
            glm::vec2 min, max;
            float distance;
            if(font && MeasureText(textComp, *font, min, max) &&
               IntersectQuad(GetFacingTransform(transform.GetTransform(), textComp.Billboard, cameraRight, cameraUp), ray, min, max, distance))
                mHits.push_back({ entity, distance });
        }

        // Nearest first, one entry per entity (an entity can carry a mesh and a sprite)
        std::sort(mHits.begin(), mHits.end(), [](const Hit& a, const Hit& b) { return a.Distance < b.Distance; });
        for(size_t i = 1; i < mHits.size();)
        {
            const entt::entity handle = mHits[i].Handle;
            const bool duplicate = std::any_of(mHits.begin(), mHits.begin() + i, [handle](const Hit& hit) { return hit.Handle == handle; });
            if(duplicate)
                mHits.erase(mHits.begin() + i);
            else
                i++;
        }
    }

    Entity ScenePicker::Pick(Scene& scene, const PickRay& ray, const glm::mat4& view, const glm::vec2& clickPos, Entity currentSelection)
    {
        SURGE_PROFILE_FUNC("ScenePicker::Pick");

        CollectHits(scene, ray, view);

        const bool sameSpot = mHasLastClick && glm::length(clickPos - mLastClickPos) <= CYCLE_CLICK_RADIUS;
        mLastClickPos = clickPos;
        mHasLastClick = true;

        if(mHits.empty())
            return {};

        size_t index = 0;
        if(sameSpot && currentSelection)
        {
            const entt::entity selectedHandle = currentSelection;
            for(size_t i = 0; i < mHits.size(); i++)
            {
                if(mHits[i].Handle == selectedHandle)
                {
                    index = (i + 1) % mHits.size();
                    break;
                }
            }
        }
        return Entity(mHits[index].Handle, &scene);
    }

} // namespace Surge
