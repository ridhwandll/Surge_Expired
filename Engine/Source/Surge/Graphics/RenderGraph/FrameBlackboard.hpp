// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Graphics/RHI/RHISettings.hpp"
#include "Surge/Graphics/Renderer/Lights.hpp"
#include "Surge/Graphics/HighLevel/Mesh.hpp"
#include "Surge/Graphics/HighLevel/Font.hpp"
#include "Surge/Graphics/Renderer/Text.hpp"

#include <glm/glm.hpp>

#define MAX_SHADOW_CASCADE_COUNT 3

namespace Surge
{
    struct FrameUBO
    {
        glm::mat4 View;
        glm::mat4 ViewProjection;        // 64 bytes
        glm::mat4 InverseViewProjection; // 64 bytes
        glm::vec3 CameraPos;             // 12 bytes
        float _pad0;                     // 4 bytes pads to 16-byte boundary
    };
    static_assert(sizeof(FrameUBO) % 16 == 0, "Size of 'FrameUBO' struct must be 16 bytes aligned!");

    struct ShadowUBO
    {
        glm::vec4 CascadeEnds;
        glm::mat4 LightSpaceMatrix[MAX_SHADOW_CASCADE_COUNT];
        Uint CascadeCount;
        int ShowCascades;
        float _pad0, pad1;
    };
    static_assert(sizeof(ShadowUBO) % 16 == 0, "Size of 'FrameUBO' struct must be 16 bytes aligned!");

    struct ShadowSettings
    {
        int CascadeCount = MAX_SHADOW_CASCADE_COUNT;
        float CascadeSplitLambda = 0.9f;
        bool ShowCascades = false;
    };

    struct Environnment
    {
        bool HasEnvironment = false;

        // Sky
        float Elevation = 30.0f; // In degrees
        float Azimuth = 0.0f;   // In degrees
        float Turbidity = 2.0f;
        float Exposure = 0.02f;
        float SunIntensity = 5.0f;
        bool EnableSunDisk = true;

        // GI
        glm::vec3 SkyAmbient { 0.35f, 0.55f, 0.90f };
        glm::vec3 HorizonAmbient { 0.45f, 0.52f, 0.60f };
        glm::vec3 GroundAmbient { 0.12f, 0.11f, 0.10f };
    };

    struct VignetteGrainConfig
    {
        float Intensity = 0.0f;
        float Softness = 0.25f;
        float Grain = 0.0f;
        float _Padding = 0.0f;
    };

    // CPU-side submit commands
    // Pushed by Renderer::Submit*(), consumed by nodes in Execute()

    struct MeshSubmitCmd  // Pushed by Renderer::SubmitMesh()
    {
        glm::mat4 Transform;
        Ref<Mesh> Mesh_;
        bool DropShadow;
    };

    struct OutlineSubmitCmd // Pushed by Renderer::SubmitOutlinedMesh()
    {
        glm::mat4 Transform;
        Ref<Mesh> Mesh_;
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

    struct LineSubmitCmd // Pushed by Renderer::SubmitLine()
    {
        glm::vec3 P0;
        glm::vec3 P1;
        glm::vec4 Color;
    };

    struct TextSubmitCmd // Pushed by Renderer::SubmitText()
    {
        glm::mat4 Transform;
        String Text;
        glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };

        float MaxWidth = 0.0f; // 0.0 means no wrapping

        float LetterSpacing = 0.0f;
        float LineSpacing = 0.0f;
        TextAlignment Alignment = TextAlignment::LEFT;

        bool Italic = false;
        bool Underline = false;

        // Shadows
        bool EnableShadow = false;
        glm::vec2 ShadowOffset = { 0.0f, 0.0f };
        glm::vec4 ShadowColor = { 0.0f, 0.0f, 0.0f, 0.0f };

        Ref<Font> FontAsset;
    };

    struct FrameBlackboard
    {
        // Written by Renderer::BeginFrame()
        glm::vec3 CameraPosition;
        glm::mat4 ViewMatrix;
        glm::mat4 ProjectionMatrix;
        glm::mat4 ViewProjection;
        glm::mat4 InverseViewProjection;
        glm::vec2 CameraNearFarPlane;

        bool HasDirectionalLight = false;
        glm::vec3 DirectionalLightDir; //Set by Renderer.cpp

        Uint FrameIndex;
        BufferHandle FrameUBOs[RHISettings::FRAMES_IN_FLIGHT];
        BufferHandle ShadowUBOs[RHISettings::FRAMES_IN_FLIGHT];
        ImageHandle WhiteImage;

        ImageHandle MainPassColorImage;
        ImageHandle MainPassDepthImage;
        ImageHandle OutlineMask;
        ImageHandle FinalImage;
        ImageHandle ShadowMap[MAX_SHADOW_CASCADE_COUNT];

        FramebufferHandle OutlineFramebuffer;
        FramebufferHandle PostProcessFramebuffer;
        FramebufferHandle MainPassFramebuffer;

        SamplerHandle DefaultSampler;
        SamplerHandle TextSampler;

        PipelineHandle MaterialPipeline; // TODO: Remove this (It is currently GeometryPassPipeline(set by GeometryPass::Setup))

        //Shadow Settings
        ShadowSettings ShadowSettings_;

        // GI & skybox
        Environnment Env;

        // Screen Space options
        glm::vec3 OutlineColor = glm::vec3(1.0f, 0.6f, 0.1f);
        float OutlineThickness = 1.1f;
        bool EnableFXAA;
        VignetteGrainConfig VignetteGrain;

        // CMD lists
        Vector<MeshSubmitCmd> MeshList;
        Vector<LightSubmitCmd> LightList;
        Vector<QuadSubmitCmd> QuadList;
        Vector<LineSubmitCmd> LineList;
        Vector<TextSubmitCmd> TextList;
        Vector<OutlineSubmitCmd> OutlineList;

        void ClearLists()
        {
            MeshList.clear();
            LightList.clear();
            TextList.clear();
            QuadList.clear();
            LineList.clear();
            OutlineList.clear();
        }
    };
}
