// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Graphics/RHI/RHISettings.hpp"
#include <glm/glm.hpp>
#include <Surge/Graphics/Renderer/Lights.hpp>
#include <Surge/Graphics/Mesh/Mesh.hpp>

namespace Surge
{
    struct FrameUBO
    {
        glm::mat4 ViewProjection;  // 64 bytes
        glm::vec3 CameraPos;       // 12 bytes
        float _pad0;               // 4 bytes pads to 16-byte boundary
    };
    static_assert(sizeof(FrameUBO) % 16 == 0, "Size of 'FrameUBO' struct must be 16 bytes aligned!");

    // -------------------------------------------------------
    // CPU-side submit commands
    // Pushed by Renderer::Submit*(), consumed by nodes in Execute()
    // -------------------------------------------------------

    struct MeshSubmitCmd  // Pushed by Renderer::SubmitMesh()
    {
        glm::mat4 Transform;
        Ref<Mesh> Mesh_;
    };

    struct OutlineSubmitCmd // Pushed by Renderer::SubmitOutlinedMesh()
    {
        glm::mat4 Transform;
        Ref<Mesh> Mesh_;
        glm::vec3 Color; // Outline color
        float Thickness; // Scale factor
    };

    struct LightSubmitCmd // Pushed by Renderer::SubmitLight()
    {
        Light GPULight; // Pre-converted from LightComponent at submit time
    };

    struct QuadSubmitCmd // Pushed by Renderer::SubmitQuad()
    {
        glm::mat4 Transform;
        glm::vec4 Color;
        ImageHandle Texture;
    };

    struct FrameBlackboard
    {
        // Written by Renderer::BeginFrame()
        glm::vec3 CameraPosition;
        glm::mat4 ViewMatrix;
        glm::mat4 ProjectionMatrix;
        glm::mat4 ViewProjection;
        glm::vec2 CameraNearFarPlane;

        Uint FrameIndex;
        BufferHandle FrameUBOs[RHISettings::FRAMES_IN_FLIGHT];

        ImageHandle WhiteImage;

        ImageHandle MainPassColorImage;
        ImageHandle MainPassDepthImage;

        FramebufferHandle PostProcessFramebuffer;
        FramebufferHandle MainPassFramebuffer;

        ImageHandle FinalImage; // Null if RHISettings::RENDER_TO_SWAPCHAIN is true

        SamplerHandle DefaultSampler;

        PipelineHandle MaterialPipeline; // TODO: Remove this (It is currently GeometryPassPipeline(set by GeometryPass::Setup))

        glm::vec4 ClearColor = { 0.1f, 0.1f, 0.1f, 1.0f };

        Vector<MeshSubmitCmd> MeshList;
        Vector<LightSubmitCmd> LightList;
        Vector<QuadSubmitCmd> QuadList;
        Vector<OutlineSubmitCmd> OutlineList;

        void ClearLists()
        {
            MeshList.clear();
            LightList.clear();
            QuadList.clear();
            OutlineList.clear();
        }
    };
}
