// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Graphics/RHI/RHISettings.hpp"
#include "Surge/Graphics/RenderGraph/RenderSettings.hpp"
#include "Surge/Graphics/Renderer/Lights.hpp"
#include "Surge/Graphics/HighLevel/Mesh.hpp"
#include "Surge/Graphics/HighLevel/Font.hpp"
#include "Surge/Graphics/Renderer/Text.hpp"
#include "Surge/Graphics/Particles/ParticleRenderData.hpp"
#include "SurgeMath/Frustum.hpp"

#include <glm/glm.hpp>


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

    // Mirrored in Renderer3D.glsl (set 2, binding 1)
    struct ShadowUBO
    {
        glm::mat4 LightSpaceMatrix;                         // World -> cascade 0 (xy: [-1, 1], z: [0, 1])
        glm::vec4 CascadeScale[MAX_SHADOW_CASCADE_COUNT];   // xyz: cascade 0 -> cascade i, w: world size of one shadow texel
        glm::vec4 CascadeOffset[MAX_SHADOW_CASCADE_COUNT];  // xyz: cascade 0 -> cascade i
        glm::vec4 CascadeEnds;                              // Positive view depth where each cascade ends
        glm::vec2 AtlasTexelSize;                           // 1 / atlas size
        float NormalBias;                                   // In shadow texels
        float CascadeBlend;
        float FadeStart;                                    // View depth where shadows start fading out
        float Distance;                                     // View depth where they are gone
        Uint CascadeCount;
        int FilterSize;                                     // Tent PCF width in texels: 3, 5 or 7
        int DitheredBlend;
        int ShowCascades;
        int Enabled;
        float _pad0;
    };
    static_assert(sizeof(ShadowUBO) % 16 == 0, "Size of 'ShadowUBO' struct must be 16 bytes aligned!");

    struct Environnment
    {
        bool HasEnvironment = false;
        bool ShowSky = true; // false: only the ambient below lights the scene, the background stays clear

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

    // CPU-side submit commands
    // Pushed by Renderer::Submit*(), consumed by nodes in Execute()

    static constexpr Uint INVALID_SKIN_OFFSET = UINT32_MAX;

    struct MeshSubmitCmd  // Pushed by Renderer::SubmitMesh() / SubmitSkinnedMesh()
    {
        glm::mat4 Transform;
        const Mesh* Mesh_;
        bool DropShadow;
        Uint SkinOffset = INVALID_SKIN_OFFSET; // Skinned meshes: first palette entry in FrameBlackboard::SkinMatrices
        AABB SkinBounds = {};                  // Skinned meshes: model space bounds of this pose, culls the whole mesh
    };

    struct AnimationStats
    {
        Uint Animators = 0;  // Written by the scene each frame
        Uint Culled = 0;     // Off screen: time advanced, pose not evaluated
        Uint OverBudget = 0; // Submissions the Renderer dropped (AnimationSettings::MaxSkinMatrices)
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
        bool Billboard;
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
        TextVerticalAlignment VerticalAlignment = TextVerticalAlignment::CENTER;

        bool Italic = false;
        bool Underline = false;
        bool Billboard = false;

        // Shadows
        bool EnableShadow = false;
        glm::vec2 ShadowOffset = { 0.0f, 0.0f };
        glm::vec4 ShadowColor = { 0.0f, 0.0f, 0.0f, 0.0f };

        const Font* FontAsset;
    };

    // Screen space UI sprite, pushed by UI::Manager (pixels, (0,0) = top left of the UI target)
    struct UISpriteCmd
    {
        glm::vec2 Min;
        glm::vec2 Max;
        glm::vec2 UVMin = { 0.0f, 0.0f }; // Sub-rect of Texture, lets 9-slice and filled images reuse the same quad path
        glm::vec2 UVMax = { 1.0f, 1.0f };
        glm::vec4 Color;
        ImageHandle Texture;
        float CornerRadius = 0.0f; // Pixels, rounded in UI.glsl (signed distance), 0 = sharp
    };

    // Screen space UI draw order: sprites and text are interleaved in hierarchy order
    struct UIDrawItem
    {
        bool IsText;
        uint16_t ClipIndex; // UI_NO_CLIP or index + 1 into UIClipRects, applied on the CPU (no scissor, batches stay intact)
        Uint Index;         // Into UITextList or UISpriteList
    };
    static constexpr uint16_t UI_NO_CLIP = 0;

    struct FrameBlackboard
    {
        // Written by Renderer::BeginFrame()
        float ScreenWidth, ScreenHeight;
        float RenderScale = RenderScaleSettings::Default(PLATFORM_RENDER_PROFILE);
        glm::vec3 CameraPosition;
        glm::mat4 ViewMatrix;
        glm::mat4 ProjectionMatrix;
        glm::mat4 ViewProjection;
        glm::mat4 InverseViewProjection;
        glm::vec2 CameraNearFarPlane;
        glm::vec3 CameraRight;
        glm::vec3 CameraUp;
        Frustum CameraFrustum; // Of ViewProjection, culls the particle systems before they submit

        bool HasDirectionalLight = false;
        glm::vec3 DirectionalLightDir; //Set by Renderer.cpp

        Uint FrameIndex;
        BufferHandle FrameUBOs[RHISettings::FRAMES_IN_FLIGHT];
        BufferHandle ShadowUBOs[RHISettings::FRAMES_IN_FLIGHT];
        ImageHandle WhiteImage;
        ImageHandle ParticleDefaultImage; // Soft round particle, used by emitters without a texture

        ImageHandle MainPassColorImage;
        ImageHandle MainPassDepthImage;
        ImageHandle AOImage;       // R8, AO resolution, 1.0 = unoccluded
        ImageHandle AOLinearDepth; // R32F, AO resolution, positive view space depth
        ImageHandle BloomImage;    // Half res, final (top) mip of the bloom upsample chain
        ImageHandle UIOverlayImage;
        ImageHandle FinalImage;
        ImageHandle ShadowAtlas;     // D16, cascades side by side (ShadowPass)
        Uint ShadowAtlasVersion = 0; // Bumped when the atlas is recreated (resolution/cascade count change), samplers rewrite their descriptors

        // Skinning palettes of the frame (SkinMatrices), uploaded by the Renderer once per graph before the passes prepare
        BufferHandle SkinBuffers[RHISettings::FRAMES_IN_FLIGHT];
        Uint SkinBufferCapacity[RHISettings::FRAMES_IN_FLIGHT] = {}; // In SkinMatrix entries
        Uint SkinBufferVersion[RHISettings::FRAMES_IN_FLIGHT] = {};  // Bumped when a frame's buffer is recreated, passes rewrite their descriptors

        FramebufferHandle PostProcessFramebuffer;
        FramebufferHandle UIOverlayFramebuffer;
        FramebufferHandle MainPassFramebuffer;

        SamplerHandle DefaultSampler;
        SamplerHandle TextSampler;
        SamplerHandle PointClampSampler;
        SamplerHandle LinearClampSampler;

        PipelineHandle MaterialPipeline; // TODO: Remove this (It is currently GeometryPassPipeline(set by GeometryPass::Setup))

        //Shadow Settings
        ShadowSettings ShadowSettings_ = ShadowSettings::Default(PLATFORM_RENDER_PROFILE);

        // GI & skybox
        Environnment Env;

        LightingSettings Lighting = LightingSettings::Default(PLATFORM_RENDER_PROFILE);
        ParticleSettings Particles = ParticleSettings::Default(PLATFORM_RENDER_PROFILE);
        ParticleStats ParticleStatistics; // Written by the scene each frame
        AnimationSettings Animation = AnimationSettings::Default(PLATFORM_RENDER_PROFILE);
        AnimationStats AnimationStatistics;

        // Screen Space options
        PostProcessSettings PostProcessSettings_;
        AmbientOcclusionSettings AOSettings = AmbientOcclusionSettings::Default(PLATFORM_RENDER_PROFILE);
        BloomSettings Bloom = BloomSettings::Default(PLATFORM_RENDER_PROFILE);
        bool HasSceneGeometry = false; // 3D draws survived culling this frame (GeometryPass::Prepare), reset by RenderGraph::Execute

        // CMD lists
        Vector<MeshSubmitCmd> MeshList;
        Vector<LightSubmitCmd> LightList;
        Vector<QuadSubmitCmd> QuadList;
        Vector<LineSubmitCmd> LineList;
        Vector<TextSubmitCmd> TextList;
        Vector<ParticleRecord> ParticleList;            // Renderer::SubmitParticles
        Vector<ParticleEmitterCmd> ParticleEmitterList; // One per submitted emitter, records are contiguous per emitter
        Vector<SkinMatrix> SkinMatrices;                 // Palettes of the skinned meshes in MeshList (MeshSubmitCmd::SkinOffset)

        // UI
        bool ShowUI = true;
        glm::vec2 UIResolution = { 0.0f, 0.0f }; // Size the UI was laid out at, 0 = ScreenWidth/Height (Editor views share one layout)
        Vector<UISpriteCmd> UISpriteList;
        Vector<TextSubmitCmd> UITextList;
        Vector<UIDrawItem> UIDrawOrder;
        Vector<glm::vec4> UIClipRects; // (minX, minY, maxX, maxY) pixels

        // Screen space effects only run when the main pass has something for them, a UI only scene skips them (PostProcess composites what ran)
        bool RunsAO() const { return AOSettings.Enable && HasSceneGeometry; }
        bool RunsBloom() const { return Bloom.Enable && Bloom.Intensity > 0.0f && HasMainPassContent(); }
        bool HasMainPassContent() const // 3D, sky, sprites/text/lines or particles
        {
            return HasSceneGeometry || (Env.HasEnvironment && Env.ShowSky) || !QuadList.empty() || !TextList.empty() || !LineList.empty() || !ParticleEmitterList.empty();
        }

        // Size of the scene targets (main pass, AO, bloom) for a given output size
        glm::uvec2 GetSceneSize(Uint outputWidth, Uint outputHeight) const
        {
            const float scale = glm::clamp(RenderScale, RenderScaleSettings::MIN_SCALE, RenderScaleSettings::MAX_SCALE);
            return glm::max(glm::uvec2(glm::vec2(static_cast<float>(outputWidth), static_cast<float>(outputHeight)) * scale + 0.5f), glm::uvec2(1));
        }

        void ClearLists()
        {
            MeshList.clear();
            LightList.clear();
            TextList.clear();
            QuadList.clear();
            LineList.clear();
            ParticleList.clear();
            ParticleEmitterList.clear();
            SkinMatrices.clear();

            UISpriteList.clear();
            UITextList.clear();
            UIDrawOrder.clear();
            UIClipRects.clear();
        }
    };
}
