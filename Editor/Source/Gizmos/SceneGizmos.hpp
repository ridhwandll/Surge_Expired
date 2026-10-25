// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/ECS/Scene.hpp"
#include "Gizmos/GizmoIcons.hpp"
#include "Gizmos/GizmoCanvas.hpp"
#include <glm/glm.hpp>
#include <imgui.h>

namespace Surge
{
    class RuntimeCamera;
    class AnimatorRuntime;
    struct ParticleSystemComponent;

    struct GizmoSettings
    {
        bool ShowIcons = true;
        float IconSize = 30.0f;      // Pixels
        bool ShowWireForAll = false; // Frustums and light/audio ranges for every entity, otherwise only for the selection
        bool ShowCameraFrustums = true;
        bool ShowLightGizmos = true;
        bool ShowAudioGizmos = true;
        bool ShowParticleGizmos = true; // Emitter shapes
        bool ShowColliders = true;
        bool ShowSkeletons = true; // Joint hierarchy of the selected animator
    };

    // Editor-only scene visualization. Component icons and wire gizmos (camera frustums, light and audio ranges) are an ImGui overlay (ImGuizmo)
    class SceneGizmos
    {
    public:
        SceneGizmos() = default;
        ~SceneGizmos() = default;
        SURGE_DISABLE_COPY_AND_MOVE(SceneGizmos);

        void Shutdown();

        // (Rid) Called through Scene::Update(EditorCamera&, editorDraw), editorDraw callback, in Editor.cpp
        void SubmitSceneDebug(Scene& scene, Entity selected);

        // (Rid) Called before ImGuizmo so the manipulator stays on top, in Viewport/SceneViewport.cpp
        void DrawOverlay(Scene& scene, Entity selected, const glm::mat4& view, const glm::mat4& projection, float nearClip, const ImVec2& viewportMin, const ImVec2& viewportSize, bool allowHover);

        // Entity whose icon is under the mouse (nearest one wins), invalid when none. Valid after DrawOverlay() for the current frame
        Entity GetHoveredEntity() const { return mHoveredEntity; }

        void OnSettingsGUI();
        GizmoSettings& GetSettings() { return mSettings; }

    private:
        struct IconCmd
        {
            entt::entity Handle;
            ImVec2 ScreenPos;
            float Depth;
            GizmoIcon Icon;
            ImU32 Tint;
            bool Inactive;
        };

        void DrawCameraFrustum(const glm::mat4& cameraWorld, const RuntimeCamera& camera, ImU32 color, float thickness);
        void DrawDirectionalLight(const glm::mat4& lightWorld, ImU32 color, float thickness);
        void DrawParticleShapes(const ParticleSystemComponent& particles, const glm::mat4& world, ImU32 color, float thickness);
        void DrawSkeleton(const AnimatorRuntime& runtime, ImU32 color, float thickness);
        void DrawIcons(Scene& scene, entt::entity selectedHandle, bool allowHover);

    private:
        GizmoSettings mSettings;
        Vector<IconCmd> mIcons;
        Entity mHoveredEntity;

        // Per-overlay state
        GizmoCanvas mCanvas;
        ImDrawList* mDrawList = nullptr;
    };
} // namespace Surge
