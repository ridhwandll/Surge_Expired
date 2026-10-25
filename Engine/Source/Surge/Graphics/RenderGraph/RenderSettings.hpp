// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include <glm/glm.hpp>

#define MAX_SHADOW_CASCADE_COUNT 3

namespace Surge
{
    // Which RenderSettings of a RendererSettingsComponent apply: Android uses MOBILE, Windows DESKTOP (the Editor previews either one)
    enum class RenderProfile : uint8_t
    {
        DESKTOP,
        MOBILE
    };

#ifdef SURGE_PLATFORM_ANDROID
    constexpr RenderProfile PLATFORM_RENDER_PROFILE = RenderProfile::MOBILE;
#else
    constexpr RenderProfile PLATFORM_RENDER_PROFILE = RenderProfile::DESKTOP;
#endif

    enum class ShadowQuality : uint8_t
    {
        LOW,    // 2 cascades of 2048, 3x3 tent PCF (4 taps), dithered cascade blend (Mobile default)
        MEDIUM, // 3 cascades of 2048, 5x5 tent PCF (9 taps)
        HIGH,   // 3 cascades of 2048, 7x7 tent PCF (16 taps)
        ULTRA   // 3 cascades of 4096, 7x7 tent PCF (16 taps) (Desktop default)
    };

    // Directional light cascaded shadow maps, every cascade is a region of one D16 atlas rendered in a single render pass
    struct ShadowSettings
    {
        static constexpr Uint MIN_RESOLUTION = 512;
        static constexpr Uint MAX_RESOLUTION = 4096;

        bool Enable = true;
        ShadowQuality Quality = ShadowQuality::LOW;

        // Driven by Quality via ApplyQualityPreset(), can be tweaked individually afterwards
        int CascadeCount = 2;        // Every cascade draws its casters again
        Uint Resolution = 2048;      // Per cascade, the atlas is CascadeCount * Resolution wide
        int FilterSize = 3;          // Tent PCF width in texels: 3 (4 taps), 5 (9 taps) or 7 (16 taps)
        bool DitheredBlend = true;   // Cascade blend picks one cascade per pixel (noise) instead of sampling both

        float Distance = 50.0f;      // View depth covered by the cascades, shadows fade out over the last FadeRange of it
        float FadeRange = 0.1f;
        float CascadeSplitLambda = 0.9f;
        float CascadeBlend = 0.1f;   // Fraction of each cascade blended into the next one, hides the resolution jump
        bool ShowCascades = false;   // Debug, not saved

        float NormalBias = 1.0f;     // Receiver offset along its normal in shadow texels, so it scales with each cascade
        float DepthBiasConstant = 1.25f;
        float DepthBiasClamp = 0.0f;
        float DepthBiasSlope = 1.75f;

        bool operator==(const ShadowSettings&) const = default;

        void ApplyQualityPreset(ShadowQuality quality)
        {
            Quality = quality;
            switch(quality)
            {
                case ShadowQuality::LOW:    CascadeCount = 2; Resolution = 2048; FilterSize = 3; DitheredBlend = true;  break;
                case ShadowQuality::MEDIUM: CascadeCount = 3; Resolution = 2048; FilterSize = 5; DitheredBlend = false; break;
                case ShadowQuality::HIGH:   CascadeCount = 3; Resolution = 2048; FilterSize = 7; DitheredBlend = false; break;
                case ShadowQuality::ULTRA:  CascadeCount = 3; Resolution = 4096; FilterSize = 7; DitheredBlend = false; break;
            }
        }

        static ShadowSettings Default(RenderProfile profile)
        {
            ShadowSettings settings;
            const bool mobile = profile == RenderProfile::MOBILE;
            settings.ApplyQualityPreset(mobile ? ShadowQuality::LOW : ShadowQuality::ULTRA);
            settings.Distance = mobile ? 50.0f : 150.0f;
            return settings;
        }
    };

    enum class AOQuality : uint8_t
    {
        LOW,    // Half res, 1 slice x 3 steps, derivative normals (Mobile default)
        MEDIUM, // Half res, 2 slices x 3 steps
        HIGH,   // Half res, 3 slices x 4 steps (Desktop default)
        ULTRA   // Full res, 4 slices x 8 steps, wide blur
    };

    // GTAO (Ground Truth Ambient Occlusion), computed from depth only
    // Applied as a multiply on the lit scene color in PostProcess
    struct AmbientOcclusionSettings
    {
        bool Enable = true;
        AOQuality Quality = AOQuality::LOW;

        // Driven by Quality via ApplyQualityPreset(), can be tweaked individually afterwards
        float ResolutionScale = 0.5f; // AO render target size relative to the screen (0.25 - 1.0)
        int SliceCount = 1;
        int StepsPerSlice = 3;
        int BlurRadius = 2;           // In AO texels, 0 disables the blur
        bool AccurateNormals = false; // 4 tap normal reconstruction instead of screen space derivatives

        float Radius = 0.75f;      // World space units
        float Intensity = 1.0f;    // Final visibility power
        float FalloffRange = 0.6f; // Fraction of Radius over which occluders fade out
        float MaxDistance = 80.0f; // AO fades out completely at this view distance
        float BlurSharpness = 8.0f;

        bool operator==(const AmbientOcclusionSettings&) const = default;

        void ApplyQualityPreset(AOQuality quality)
        {
            Quality = quality;
            switch(quality)
            {
                case AOQuality::LOW:    ResolutionScale = 0.5f; SliceCount = 1; StepsPerSlice = 3; BlurRadius = 2; AccurateNormals = false; break;
                case AOQuality::MEDIUM: ResolutionScale = 0.5f; SliceCount = 2; StepsPerSlice = 3; BlurRadius = 3; AccurateNormals = true;  break;
                case AOQuality::HIGH:   ResolutionScale = 0.5f; SliceCount = 3; StepsPerSlice = 4; BlurRadius = 3; AccurateNormals = true;  break;
                case AOQuality::ULTRA:  ResolutionScale = 1.0f; SliceCount = 4; StepsPerSlice = 8; BlurRadius = 4; AccurateNormals = true;  break;
            }
        }

        static AmbientOcclusionSettings Default(RenderProfile profile)
        {
            AmbientOcclusionSettings settings;
            if(profile == RenderProfile::MOBILE)
            {
                // 4 extra render passes + a stored full-res depth buffer, opt-in on mobile
                settings.ApplyQualityPreset(AOQuality::LOW);
                settings.Enable = false;
            }
            else
                settings.ApplyQualityPreset(AOQuality::HIGH); // Half res, PostProcess upsamples it bilaterally. ULTRA (full res) costs ~10x more
            return settings;
        }
    };

    enum class BloomQuality : uint8_t
    {
        LOW,    // Fragment, 3 mips, dual filter (5/8 taps), bilinear composite (Mobile default)
        MEDIUM, // Fragment, 5 mips, 13 tap down + 9 tap tent up, anti flicker
        HIGH,   // Compute, 6 mips, 13 tap down + 9 tap tent up, anti flicker
        ULTRA   // Compute, 8 mips, 13 tap down + 9 tap tent up, anti flicker (Desktop default)
    };

    // Physically based style bloom (CoD: Advanced Warfare / Unity URP / Hazel), mip chain starts at half resolution
    // Fragment path renders each mip with a renderpass (tile friendly, keeps framebuffer compression on mobile, R11G11B10)
    // Compute path writes storage images (RGBA16F), used on desktop
    struct BloomSettings
    {
        static constexpr int MIN_MIP_COUNT = 2;
        static constexpr int MAX_MIP_COUNT = 8;

        bool Enable = true;
        BloomQuality Quality = BloomQuality::LOW;

        // Driven by Quality via ApplyQualityPreset(), can be tweaked individually afterwards
        bool UseCompute = false;           // Falls back to the fragment path if the device queue has no compute support
        int MipCount = 3;                  // 2 - 8, clamped further so the smallest mip stays >= 2px
        bool HighQualityFilter = false;    // 13 tap downsample + 9 tap tent upsample, otherwise ARM dual filter
        bool AntiFlicker = false;          // Karis average on the first downsample, kills fireflies from tiny HDR highlights
        bool HighQualityComposite = false; // 9 tap tent when upscaling the half res bloom in PostProcess

        float Threshold = 1.0f;  // Linear HDR brightness where bloom starts
        float Knee = 0.5f;       // Soft threshold, fraction of Threshold
        float Intensity = 0.8f;
        float Scatter = 0.7f;    // How far the glow spreads (mix factor between mips)
        float ClampValue = 65000.0f; // Max HDR value fed into bloom
        glm::vec3 Tint = glm::vec3(1.0f);

        bool operator==(const BloomSettings&) const = default;

        void ApplyQualityPreset(BloomQuality quality)
        {
            Quality = quality;
            switch(quality)
            {
                case BloomQuality::LOW:    UseCompute = false; MipCount = 3; HighQualityFilter = false; AntiFlicker = false; HighQualityComposite = false; break;
                case BloomQuality::MEDIUM: UseCompute = false; MipCount = 5; HighQualityFilter = true;  AntiFlicker = true;  HighQualityComposite = true;  break;
                case BloomQuality::HIGH:   UseCompute = true;  MipCount = 6; HighQualityFilter = true;  AntiFlicker = true;  HighQualityComposite = true;  break;
                case BloomQuality::ULTRA:  UseCompute = true;  MipCount = 8; HighQualityFilter = true;  AntiFlicker = true;  HighQualityComposite = true;  break;
            }
        }

        static BloomSettings Default(RenderProfile profile)
        {
            BloomSettings settings;
            if(profile == RenderProfile::MOBILE)
            {
                settings.Enable = false;
                settings.ApplyQualityPreset(BloomQuality::LOW);
            }
            else
                settings.ApplyQualityPreset(BloomQuality::ULTRA);
            return settings;
        }
    };

    // Resolution of the 3D scene (main pass, AO, bloom) relative to the output, UI and post-processing stay at full resolution
    struct RenderScaleSettings
    {
        static constexpr float MIN_SCALE = 0.5f;
        static constexpr float MAX_SCALE = 1.0f;

        static float Default(RenderProfile profile) { return profile == RenderProfile::MOBILE ? 0.8f : 1.0f; }
    };

    struct PostProcessSettings
    {
        float VignetteIntensity = 0.0f;
        float VignetteSoftness = 0.25f;
        float Grain = 0.0f;
        bool ShowAOOnly = false; // Debug view, outputs the upsampled AO buffer, not saved

        bool operator==(const PostProcessSettings&) const = default;
    };

    // Forward lighting budget: every lit pixel loops over these, so only the most relevant lights reach the shader
    struct LightingSettings
    {
        Uint MaxLights = 64;

        bool operator==(const LightingSettings&) const = default;
        static LightingSettings Default(RenderProfile profile) { return { profile == RenderProfile::MOBILE ? 8u : 64u }; }
    };

    // Global particle budget of a frame
    struct ParticleSettings
    {
        static constexpr float MIN_EMISSION_QUALITY = 0.1f;

        Uint MaxParticles = 131072;
        float EmissionQuality = 1.0f; // Scales every emission rate and burst count (0.1 - 1)

        bool operator==(const ParticleSettings&) const = default;
        static ParticleSettings Default(RenderProfile profile) { return { profile == RenderProfile::MOBILE ? 8192u : 131072u, 1.0f }; }
    };

    // Skin matrices uploaded per frame (48 bytes each) by everything the scene animates, past the budget animators are not drawn
    struct AnimationSettings
    {
        Uint MaxSkinMatrices = 32768; // 1.5 MB per frame (Mobile: 4096, 192 KB)

        bool operator==(const AnimationSettings&) const = default;
        static AnimationSettings Default(RenderProfile profile) { return { profile == RenderProfile::MOBILE ? 4096u : 32768u }; }
    };

    // Everything a scene saves about rendering: RendererSettingsComponent holds one per RenderProfile, Renderer::SubmitRenderSettings applies it
    struct RenderSettings
    {
        float RenderScale = 1.0f;
        LightingSettings Lighting;
        ParticleSettings Particles;
        AnimationSettings Animation;
        ShadowSettings Shadows;
        AmbientOcclusionSettings AO;
        BloomSettings Bloom;
        PostProcessSettings PostProcess;

        bool operator==(const RenderSettings&) const = default;

        static RenderSettings Default(RenderProfile profile)
        {
            RenderSettings settings;
            settings.RenderScale = RenderScaleSettings::Default(profile);
            settings.Lighting = LightingSettings::Default(profile);
            settings.Particles = ParticleSettings::Default(profile);
            settings.Animation = AnimationSettings::Default(profile);
            settings.Shadows = ShadowSettings::Default(profile);
            settings.AO = AmbientOcclusionSettings::Default(profile);
            settings.Bloom = BloomSettings::Default(profile);
            return settings;
        }

        static const RenderSettings& GetDefault(RenderProfile profile)
        {
            static const RenderSettings sDesktop = Default(RenderProfile::DESKTOP);
            static const RenderSettings sMobile = Default(RenderProfile::MOBILE);
            return profile == RenderProfile::MOBILE ? sMobile : sDesktop;
        }

        // Every saved field once, fn(group, name, field&): drives the JSON scene source, the cooked SceneBinary and its schema hash
        // Fields are bool, int, Uint, float, glm::vec3 or a quality enum. Appending is fine
        template <typename Self, typename Fn>
        static void VisitSavedFields(Self& settings, Fn&& fn)
        {
            fn("General", "RenderScale", settings.RenderScale);
            fn("General", "MaxLights", settings.Lighting.MaxLights);

            fn("Particles", "MaxParticles", settings.Particles.MaxParticles);
            fn("Particles", "EmissionQuality", settings.Particles.EmissionQuality);
            fn("Animation", "MaxSkinMatrices", settings.Animation.MaxSkinMatrices);

            auto& shadows = settings.Shadows;
            fn("Shadows", "Enable", shadows.Enable);
            fn("Shadows", "Quality", shadows.Quality);
            fn("Shadows", "CascadeCount", shadows.CascadeCount);
            fn("Shadows", "Resolution", shadows.Resolution);
            fn("Shadows", "FilterSize", shadows.FilterSize);
            fn("Shadows", "DitheredBlend", shadows.DitheredBlend);
            fn("Shadows", "Distance", shadows.Distance);
            fn("Shadows", "FadeRange", shadows.FadeRange);
            fn("Shadows", "CascadeSplitLambda", shadows.CascadeSplitLambda);
            fn("Shadows", "CascadeBlend", shadows.CascadeBlend);
            fn("Shadows", "NormalBias", shadows.NormalBias);
            fn("Shadows", "DepthBiasConstant", shadows.DepthBiasConstant);
            fn("Shadows", "DepthBiasClamp", shadows.DepthBiasClamp);
            fn("Shadows", "DepthBiasSlope", shadows.DepthBiasSlope);

            auto& ao = settings.AO;
            fn("AO", "Enable", ao.Enable);
            fn("AO", "Quality", ao.Quality);
            fn("AO", "ResolutionScale", ao.ResolutionScale);
            fn("AO", "SliceCount", ao.SliceCount);
            fn("AO", "StepsPerSlice", ao.StepsPerSlice);
            fn("AO", "BlurRadius", ao.BlurRadius);
            fn("AO", "AccurateNormals", ao.AccurateNormals);
            fn("AO", "Radius", ao.Radius);
            fn("AO", "Intensity", ao.Intensity);
            fn("AO", "FalloffRange", ao.FalloffRange);
            fn("AO", "MaxDistance", ao.MaxDistance);
            fn("AO", "BlurSharpness", ao.BlurSharpness);

            auto& bloom = settings.Bloom;
            fn("Bloom", "Enable", bloom.Enable);
            fn("Bloom", "Quality", bloom.Quality);
            fn("Bloom", "UseCompute", bloom.UseCompute);
            fn("Bloom", "MipCount", bloom.MipCount);
            fn("Bloom", "HighQualityFilter", bloom.HighQualityFilter);
            fn("Bloom", "AntiFlicker", bloom.AntiFlicker);
            fn("Bloom", "HighQualityComposite", bloom.HighQualityComposite);
            fn("Bloom", "Threshold", bloom.Threshold);
            fn("Bloom", "Knee", bloom.Knee);
            fn("Bloom", "Intensity", bloom.Intensity);
            fn("Bloom", "Scatter", bloom.Scatter);
            fn("Bloom", "ClampValue", bloom.ClampValue);
            fn("Bloom", "Tint", bloom.Tint);

            fn("PostProcess", "VignetteIntensity", settings.PostProcess.VignetteIntensity);
            fn("PostProcess", "VignetteSoftness", settings.PostProcess.VignetteSoftness);
            fn("PostProcess", "Grain", settings.PostProcess.Grain);
        }
    };

} // namespace Surge
