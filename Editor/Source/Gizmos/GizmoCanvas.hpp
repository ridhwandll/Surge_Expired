// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include <glm/glm.hpp>
#include <imgui.h>

namespace Surge
{
    // 3D wireframes drawn into an ImGui draw list over one view (Scene views, Particle Editor preview).
    // Lines are clipped against the near plane so nothing behind the camera gets drawn, each wire gets a dark rim
    class GizmoCanvas
    {
    public:
        static constexpr int CIRCLE_SEGMENTS = 64; // Also the max point count of DrawPolyline3D

        // projection is a Vulkan one (y down), like EditorCamera::GetProjectionMatrix
        void Begin(ImDrawList* drawList, const glm::mat4& view, const glm::mat4& projection, float nearClip, const ImVec2& viewportMin, const ImVec2& viewportSize);

        bool ProjectPoint(const glm::vec3& worldPos, ImVec2& outScreen, float* outDepth = nullptr) const;
        float GetWorldPerPixel(const glm::vec3& worldPos) const; // World units covered by one pixel at that depth

        void DrawLine3D(const glm::vec3& a, const glm::vec3& b, ImU32 color, float thickness);
        void DrawPolyline3D(const glm::vec3* points, int count, bool closed, ImU32 color, float thickness);
        void DrawCircle3D(const glm::vec3& center, const glm::vec3& axisU, const glm::vec3& axisV, float radius, ImU32 color, float thickness);
        void DrawWireSphere(const glm::vec3& center, float radius, ImU32 color, float thickness);
        void DrawWireBox(const glm::vec3& min, const glm::vec3& max, ImU32 color, float thickness);         // Axis aligned
        void DrawArrow3D(const glm::vec3& from, const glm::vec3& to, float headSize, ImU32 color, float thickness);

        ImDrawList* GetDrawList() const { return mDrawList; }
        const glm::vec3& GetCameraPosition() const { return mCameraPosition; }
        float GetNearClip() const { return mNearClip; }

        static void GetPerpendicularBasis(const glm::vec3& n, glm::vec3& outU, glm::vec3& outV);

    private:
        ImDrawList* mDrawList = nullptr;
        glm::mat4 mViewProjection = glm::mat4(1.0f);
        glm::vec3 mCameraPosition = glm::vec3(0.0f);
        float mNearClip = 0.1f;
        float mProjectionScaleY = 1.0f;
        ImVec2 mViewportMin = { 0.0f, 0.0f };
        ImVec2 mViewportSize = { 0.0f, 0.0f };
    };
} // namespace Surge
