// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Gizmos/GizmoCanvas.hpp"
#include "Surge/Core/Core.hpp"
#include "Utility/EditorUI.hpp"

#include <glm/gtc/constants.hpp>

namespace Surge
{
    static constexpr ImU32 LINE_RIM_COLOR = EditorUI::ToU32(EditorUI::Colors::Gizmo::LineRim);
    static constexpr float LINE_RIM_EXTRA = 2.0f;

    static ImVec2 ClipToScreen(const glm::vec4& clip, const ImVec2& viewportMin, const ImVec2& viewportSize)
    {
        // Vulkan clip space: y points down, same as ImGui
        const float invW = 1.0f / clip.w;
        return ImVec2(viewportMin.x + (clip.x * invW * 0.5f + 0.5f) * viewportSize.x, viewportMin.y + (clip.y * invW * 0.5f + 0.5f) * viewportSize.y);
    }

    static bool ClipSegmentToNear(glm::vec4& a, glm::vec4& b, float nearW)
    {
        if(a.w < nearW && b.w < nearW)
            return false;

        if(a.w < nearW)
            a = glm::mix(a, b, (nearW - a.w) / (b.w - a.w));
        else if(b.w < nearW)
            b = glm::mix(b, a, (nearW - b.w) / (a.w - b.w));
        return true;
    }

    void GizmoCanvas::GetPerpendicularBasis(const glm::vec3& n, glm::vec3& outU, glm::vec3& outV)
    {
        const glm::vec3 reference = glm::abs(n.y) < 0.99f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
        outU = glm::normalize(glm::cross(n, reference));
        outV = glm::cross(n, outU);
    }

    void GizmoCanvas::Begin(ImDrawList* drawList, const glm::mat4& view, const glm::mat4& projection, float nearClip, const ImVec2& viewportMin, const ImVec2& viewportSize)
    {
        mDrawList = drawList;
        mViewProjection = projection * view;
        mCameraPosition = glm::vec3(glm::inverse(view)[3]);
        mNearClip = glm::max(nearClip, 1e-3f);
        mProjectionScaleY = glm::max(glm::abs(projection[1][1]), 1e-4f);
        mViewportMin = viewportMin;
        mViewportSize = viewportSize;
    }

    bool GizmoCanvas::ProjectPoint(const glm::vec3& worldPos, ImVec2& outScreen, float* outDepth) const
    {
        const glm::vec4 clip = mViewProjection * glm::vec4(worldPos, 1.0f);
        if(clip.w < mNearClip)
            return false;

        outScreen = ClipToScreen(clip, mViewportMin, mViewportSize);
        if(outDepth)
            *outDepth = clip.w;
        return true;
    }

    float GizmoCanvas::GetWorldPerPixel(const glm::vec3& worldPos) const
    {
        // Perspective: clip.w is the view depth, the visible height at that depth is 2 * w / P[1][1]
        const float depth = glm::max((mViewProjection * glm::vec4(worldPos, 1.0f)).w, mNearClip);
        return (2.0f * depth) / (mProjectionScaleY * glm::max(mViewportSize.y, 1.0f));
    }

    void GizmoCanvas::DrawLine3D(const glm::vec3& a, const glm::vec3& b, ImU32 color, float thickness)
    {
        glm::vec4 clipA = mViewProjection * glm::vec4(a, 1.0f);
        glm::vec4 clipB = mViewProjection * glm::vec4(b, 1.0f);
        if(!ClipSegmentToNear(clipA, clipB, mNearClip))
            return;

        const ImVec2 screenA = ClipToScreen(clipA, mViewportMin, mViewportSize);
        const ImVec2 screenB = ClipToScreen(clipB, mViewportMin, mViewportSize);
        mDrawList->AddLine(screenA, screenB, LINE_RIM_COLOR, thickness + LINE_RIM_EXTRA);
        mDrawList->AddLine(screenA, screenB, color, thickness);
    }

    void GizmoCanvas::DrawPolyline3D(const glm::vec3* points, int count, bool closed, ImU32 color, float thickness)
    {
        SG_ASSERT(count >= 2 && count <= CIRCLE_SEGMENTS, "[GizmoCanvas] Polyline point count out of range");

        glm::vec4 clip[CIRCLE_SEGMENTS];
        bool allVisible = true;
        for(int i = 0; i < count; i++)
        {
            clip[i] = mViewProjection * glm::vec4(points[i], 1.0f);
            allVisible &= clip[i].w >= mNearClip;
        }

        ImVec2 strip[CIRCLE_SEGMENTS + 1];
        auto flush = [&](int pointCount, ImDrawFlags flags) {
            if(pointCount < 2)
                return;
            mDrawList->AddPolyline(strip, pointCount, LINE_RIM_COLOR, flags, thickness + LINE_RIM_EXTRA);
            mDrawList->AddPolyline(strip, pointCount, color, flags, thickness);
        };

        if(allVisible) // Common case: one polyline with proper joints
        {
            for(int i = 0; i < count; i++)
                strip[i] = ClipToScreen(clip[i], mViewportMin, mViewportSize);
            flush(count, closed ? ImDrawFlags_Closed : ImDrawFlags_None);
            return;
        }

        // Crosses the near plane: split into visible runs
        int stripCount = 0;
        const int segmentCount = closed ? count : count - 1;
        for(int i = 0; i < segmentCount; i++)
        {
            glm::vec4 a = clip[i];
            glm::vec4 b = clip[(i + 1) % count];
            const bool aVisible = a.w >= mNearClip;
            const bool bVisible = b.w >= mNearClip;
            if(!ClipSegmentToNear(a, b, mNearClip))
            {
                flush(stripCount, ImDrawFlags_None);
                stripCount = 0;
                continue;
            }

            if(stripCount == 0 || !aVisible)
            {
                flush(stripCount, ImDrawFlags_None);
                stripCount = 0;
                strip[stripCount++] = ClipToScreen(a, mViewportMin, mViewportSize);
            }
            strip[stripCount++] = ClipToScreen(b, mViewportMin, mViewportSize);

            if(!bVisible)
            {
                flush(stripCount, ImDrawFlags_None);
                stripCount = 0;
            }
        }
        flush(stripCount, ImDrawFlags_None);
    }

    void GizmoCanvas::DrawCircle3D(const glm::vec3& center, const glm::vec3& axisU, const glm::vec3& axisV, float radius, ImU32 color, float thickness)
    {
        glm::vec3 points[CIRCLE_SEGMENTS];
        for(int i = 0; i < CIRCLE_SEGMENTS; i++)
        {
            const float angle = static_cast<float>(i) / static_cast<float>(CIRCLE_SEGMENTS) * glm::two_pi<float>();
            points[i] = center + (axisU * glm::cos(angle) + axisV * glm::sin(angle)) * radius;
        }
        DrawPolyline3D(points, CIRCLE_SEGMENTS, true, color, thickness);
    }

    void GizmoCanvas::DrawWireSphere(const glm::vec3& center, float radius, ImU32 color, float thickness)
    {
        if(radius <= 0.0f)
            return;

        // Faint great circles for the volume, the silhouette (what the eye reads as the sphere outline) at full strength
        const ImU32 axisColor = EditorUI::ScaleAlpha(color, 0.5f);
        DrawCircle3D(center, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), radius, axisColor, thickness);
        DrawCircle3D(center, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), radius, axisColor, thickness);
        DrawCircle3D(center, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), radius, axisColor, thickness);

        const glm::vec3 toCamera = mCameraPosition - center;
        const float distance = glm::length(toCamera);
        if(distance <= radius)
            return; // Inside the sphere, there is no silhouette

        const glm::vec3 n = toCamera / distance;
        glm::vec3 u, v;
        GetPerpendicularBasis(n, u, v);
        const float silhouetteRadius = radius * glm::sqrt(distance * distance - radius * radius) / distance;
        DrawCircle3D(center + n * (radius * radius / distance), u, v, silhouetteRadius, color, thickness);
    }

    void GizmoCanvas::DrawWireBox(const glm::vec3& min, const glm::vec3& max, ImU32 color, float thickness)
    {
        glm::vec3 corners[8];
        for(int i = 0; i < 8; i++)
            corners[i] = glm::vec3((i & 1) ? max.x : min.x, (i & 2) ? max.y : min.y, (i & 4) ? max.z : min.z);

        static constexpr int edges[12][2] = { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
        for(const auto& edge : edges)
            DrawLine3D(corners[edge[0]], corners[edge[1]], color, thickness);
    }

    void GizmoCanvas::DrawArrow3D(const glm::vec3& from, const glm::vec3& to, float headSize, ImU32 color, float thickness)
    {
        const glm::vec3 delta = to - from;
        const float length = glm::length(delta);
        if(length < 1e-5f)
            return;

        const glm::vec3 direction = delta / length;
        glm::vec3 u, v;
        GetPerpendicularBasis(direction, u, v);
        const glm::vec3 headBase = to - direction * headSize;
        const float headRadius = headSize * 0.45f;

        DrawLine3D(from, to, color, thickness);
        DrawLine3D(to, headBase + u * headRadius, color, thickness);
        DrawLine3D(to, headBase - u * headRadius, color, thickness);
        DrawLine3D(to, headBase + v * headRadius, color, thickness);
        DrawLine3D(to, headBase - v * headRadius, color, thickness);
    }

} // namespace Surge
