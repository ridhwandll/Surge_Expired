// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/RendererSettingsInspector.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/ECS/Components/Components.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/RenderGraph/Passes/GeometryPass.hpp"
#include "Utility/EditorUI.hpp"

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <array>

namespace Surge
{
    namespace
    {
        constexpr std::array<const char*, 4> QUALITY_NAMES = { "Low", "Medium", "High", "Ultra" };
        constexpr std::array<const char*, 4> SHADOW_QUALITY_HINTS = {
            "2 cascades of 2048, 3x3 tent PCF (4 taps), dithered cascade blend. Mobile default",
            "3 cascades of 2048, 5x5 tent PCF (9 taps)",
            "3 cascades of 2048, 7x7 tent PCF (16 taps)",
            "3 cascades of 4096, 7x7 tent PCF (16 taps), 96 MB atlas. Desktop default" };
        constexpr std::array<const char*, 4> AO_QUALITY_HINTS = {
            "Half res, 1 slice x 3 steps, derivative normals. Mobile default",
            "Half res, 2 slices x 3 steps",
            "Half res, 3 slices x 4 steps. Desktop default",
            "Full res, 4 slices x 8 steps, wide blur: ~10x the cost of High" };
        constexpr std::array<const char*, 4> BLOOM_QUALITY_HINTS = {
            "Fragment path, 3 mips, dual filter, bilinear composite. Mobile default",
            "Fragment path, 5 mips, 13 tap down + 9 tap tent up, anti flicker",
            "Compute path, 6 mips, 13 tap down + 9 tap tent up, anti flicker",
            "Compute path, 8 mips, 13 tap down + 9 tap tent up, anti flicker. Desktop default" };

        // Quality preset combo, the hint describes what the selected preset costs
        template <typename Q, typename Settings>
        void QualityRow(Settings& settings, const std::array<const char*, 4>& descriptions)
        {
            int quality = static_cast<int>(settings.Quality);
            if(EditorUI::TCombo("Quality", &quality, QUALITY_NAMES.data(), static_cast<int>(QUALITY_NAMES.size())))
                settings.ApplyQualityPreset(static_cast<Q>(quality));
            EditorUI::THint(descriptions[std::clamp(static_cast<int>(settings.Quality), 0, 3)]);
        }
    } // namespace

    void RendererSettingsInspector::Draw(Entity entity, RendererSettingsComponent& component)
    {
        // Same pick as Scene::UpdateRendering: the first active component of the registry
        entt::entity winner = entt::null;
        for(const auto& [other, settings] : entity.GetScene()->GetRegistry().view<RendererSettingsComponent>().each())
        {
            if(settings.Active)
            {
                winner = other;
                break;
            }
        }

        if(!component.Active)
            EditorUI::THint(winner == entt::null ? "Inactive: the scene renders with the platform defaults" : "Inactive: another Renderer Settings component is used", EditorUI::Colors::Warning);
        else if(winner != static_cast<entt::entity>(entity))
            EditorUI::THint("Ignored: another active Renderer Settings component comes first in the scene", EditorUI::Colors::Warning);

        EditorUI::ScopedBoldValues boldValues; // Like the Inspector's other rows
        DrawProfileSwitch(component);

        RenderSettings& settings = component.GetProfileSettings(Core::GetRenderer()->GetRenderProfile());
        DrawGeneral(settings);
        DrawShadows(settings);
        DrawAmbientOcclusion(settings);
        DrawBloom(settings);
        DrawPostProcess(settings);
    }

    void RendererSettingsInspector::DrawProfileSwitch(RendererSettingsComponent& component)
    {
        Renderer* renderer = Core::GetRenderer();
        const RenderProfile current = renderer->GetRenderProfile();
        const RenderProfile other = current == RenderProfile::MOBILE ? RenderProfile::DESKTOP : RenderProfile::MOBILE;

        EditorUI::TLabel("Profile");
        const float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        constexpr std::array<std::pair<RenderProfile, const char*>, 2> profiles = { { { RenderProfile::DESKTOP, "Desktop" }, { RenderProfile::MOBILE, "Mobile" } } };
        for(const auto& [profile, name] : profiles)
        {
            const bool selected = profile == current;
            if(selected)
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if(ImGui::Button(name, ImVec2(buttonWidth, 0.0f)))
                renderer->SetRenderProfile(profile);
            if(selected)
                ImGui::PopStyleColor();
            if(profile == RenderProfile::DESKTOP)
                ImGui::SameLine();
        }
        EditorUI::THint("Windows loads Desktop, Android loads Mobile. The profile you edit is the one every viewport previews");

        EditorUI::TLabel("Tools");
        if(ImGui::Button("Reset Profile"))
            component.GetProfileSettings(current) = RenderSettings::Default(current);
        EditorUI::ItemTooltip("Back to the engine defaults of this platform");
        ImGui::SameLine();
        if(ImGui::Button(current == RenderProfile::MOBILE ? "Copy from Desktop" : "Copy from Mobile"))
            component.GetProfileSettings(current) = component.GetProfileSettings(other);
        EditorUI::ItemTooltip("Overwrites this profile with the other one");

        EditorUI::TCheckbox("Show Advanced", &mShowAdvanced);
        EditorUI::THint("Shows the knobs the quality presets set, they are saved with the scene too");
    }

    void RendererSettingsInspector::DrawGeneral(RenderSettings& settings)
    {
        ImGui::PushID("General"); // Labels repeat between sections (Enabled, Quality, Intensity...), they make the widget IDs
        if(!EditorUI::TSection("General"))
        {
            ImGui::PopID();
            return;
        }

        // Applied on release: each change rebuilds the scene targets of every graph
        if(!mEditingRenderScale)
            mRenderScale = settings.RenderScale;
        EditorUI::TSliderFloat("Render Scale", &mRenderScale, RenderScaleSettings::MIN_SCALE, RenderScaleSettings::MAX_SCALE, "%.2fx");
        mEditingRenderScale = ImGui::IsItemActive();
        if(ImGui::IsItemDeactivatedAfterEdit())
            settings.RenderScale = mRenderScale;
        EditorUI::THint("Resolution of the 3D scene (main pass, AO, bloom). UI and post-processing stay at full resolution");

        EditorUI::TDragInteger("Max Lights", &settings.Lighting.MaxLights, 0.25f, 1, static_cast<int>(GeometryPass::MAX_LIGHTS));
        EditorUI::THint("Most relevant lights kept after culling, every lit pixel loops over all of them");

        ImGui::PopID();

        ImGui::PushID("ParticlesAnimation");
        if(!EditorUI::TSection("Particles & Animation"))
        {
            ImGui::PopID();
            return;
        }
        EditorUI::TDragInteger("Max Particles", &settings.Particles.MaxParticles, 64.0f, 0, 1 << 20);
        EditorUI::THint("Live particles of every particle system together, emission stops at this budget");
        EditorUI::TSliderFloat("Emission Quality", &settings.Particles.EmissionQuality, ParticleSettings::MIN_EMISSION_QUALITY, 1.0f, "%.2f");
        EditorUI::THint("Scales every emission rate and burst, a quality knob for weaker devices");
        EditorUI::TDragInteger("Max Skin Matrices", &settings.Animation.MaxSkinMatrices, 16.0f, 0, 1 << 18);
        EditorUI::THint("Joints of every skinned mesh drawn in a frame together (48 bytes each), animators past it are not drawn");

        EditorUI::TSeparator("Debug", true);
        EditorUI::TCheckbox("Show Cascades", &settings.Shadows.ShowCascades);
        EditorUI::THint("Tints each shadow cascade. Not saved");
        ImGui::BeginDisabled(!settings.AO.Enable);
        EditorUI::TCheckbox("Show AO Only", &settings.PostProcess.ShowAOOnly);
        EditorUI::THint("Outputs the upsampled AO buffer. Not saved");
        ImGui::EndDisabled();

        ImGui::PopID();
    }

    void RendererSettingsInspector::DrawShadows(RenderSettings& settings)
    {
        ImGui::PushID("Shadows");
        ShadowSettings& shadows = settings.Shadows;
        if(!EditorUI::TSection("Shadows"))
        {
            ImGui::PopID();
            return;
        }
        EditorUI::TCheckbox("Enabled", &shadows.Enable);
        EditorUI::THint("Cascaded shadow maps of the directional light, one extra render pass");

        ImGui::BeginDisabled(!shadows.Enable);
        QualityRow<ShadowQuality>(shadows, SHADOW_QUALITY_HINTS);
        EditorUI::TDragFloat("Distance", &shadows.Distance, 0.5f, 1.0f, 2000.0f, "%.1f");
        EditorUI::THint("View distance the cascades cover, shorter = sharper shadows");
        EditorUI::TSliderFloat("Fade Range", &shadows.FadeRange, 0.0f, 0.5f, "%.2f");
        EditorUI::THint("Fraction of Distance over which shadows fade out");
        EditorUI::TSliderFloat("Split Lambda", &shadows.CascadeSplitLambda, 0.0f, 1.0f, "%.2f");
        EditorUI::THint("0 = uniform cascade splits, 1 = logarithmic (more resolution near the camera)");
        EditorUI::TSliderFloat("Cascade Blend", &shadows.CascadeBlend, 0.0f, 0.5f, "%.2f");
        EditorUI::THint("Fraction of each cascade blended into the next one, hides the resolution jump");
        EditorUI::TDragFloat("Normal Bias", &shadows.NormalBias, 0.05f, 0.0f, 10.0f, "%.2f");
        EditorUI::THint("Receiver offset along its normal in shadow texels, fixes acne at grazing angles");
        EditorUI::TDragFloat("Bias Constant", &shadows.DepthBiasConstant, 0.05f, 0.0f, 100.0f, "%.2f");
        EditorUI::THint("Constant depth bias of the casters: too low = acne, too high = detached shadows");
        EditorUI::TDragFloat("Bias Slope", &shadows.DepthBiasSlope, 0.05f, 0.0f, 100.0f, "%.2f");
        EditorUI::THint("Depth bias scaled by how steep the caster surface is to the light");

        if(mShowAdvanced)
        {
            EditorUI::TSeparator("Advanced", true);
            EditorUI::TDragFloat("Bias Clamp", &shadows.DepthBiasClamp, 0.05f, 0.0f, 100.0f, "%.2f");
            EditorUI::THint("Upper limit of the depth bias, 0 = no limit");

            constexpr const char* cascadeNames[] = { "1", "2", "3" };
            static_assert(IM_ARRAYSIZE(cascadeNames) == MAX_SHADOW_CASCADE_COUNT);
            int cascadeIndex = std::clamp(shadows.CascadeCount, 1, MAX_SHADOW_CASCADE_COUNT) - 1;
            if(EditorUI::TCombo("Cascades", &cascadeIndex, cascadeNames, IM_ARRAYSIZE(cascadeNames)))
                shadows.CascadeCount = cascadeIndex + 1;
            EditorUI::THint("Every cascade draws its shadow casters again");

            constexpr std::array<Uint, 4> resolutions = { 512, 1024, 2048, 4096 };
            constexpr const char* resolutionNames[] = { "512", "1024", "2048", "4096" };
            static_assert(IM_ARRAYSIZE(resolutionNames) == resolutions.size());
            static_assert(resolutions.front() == ShadowSettings::MIN_RESOLUTION && resolutions.back() == ShadowSettings::MAX_RESOLUTION);
            int resolutionIndex = static_cast<int>(std::lower_bound(resolutions.begin(), resolutions.end(), shadows.Resolution) - resolutions.begin());
            resolutionIndex = std::min(resolutionIndex, static_cast<int>(resolutions.size()) - 1);
            if(EditorUI::TCombo("Resolution", &resolutionIndex, resolutionNames, IM_ARRAYSIZE(resolutionNames)))
                shadows.Resolution = resolutions[resolutionIndex];
            EditorUI::THint("Per cascade, D16: 2048 = 8 MB, 4096 = 32 MB each");

            constexpr const char* filterNames[] = { "3x3 (4 taps)", "5x5 (9 taps)", "7x7 (16 taps)" };
            int filterIndex = std::clamp((shadows.FilterSize - 3) / 2, 0, 2);
            if(EditorUI::TCombo("Filter", &filterIndex, filterNames, IM_ARRAYSIZE(filterNames)))
                shadows.FilterSize = 3 + filterIndex * 2;
            EditorUI::THint("Tent PCF width: wider = softer edges, more taps per lit pixel");

            EditorUI::TCheckbox("Dithered Blend", &shadows.DitheredBlend);
            EditorUI::THint("Picks one cascade per pixel in the blend band (noise) instead of sampling both");
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    }

    void RendererSettingsInspector::DrawAmbientOcclusion(RenderSettings& settings)
    {
        ImGui::PushID("AmbientOcclusion");
        AmbientOcclusionSettings& ao = settings.AO;
        if(!EditorUI::TSection("Ambient Occlusion"))
        {
            ImGui::PopID();
            return;
        }
        EditorUI::TCheckbox("Enabled", &ao.Enable);
        EditorUI::THint("GTAO from depth: 4 extra render passes + a stored depth buffer. Skipped on frames without 3D geometry");

        ImGui::BeginDisabled(!ao.Enable);
        QualityRow<AOQuality>(ao, AO_QUALITY_HINTS);
        EditorUI::TDragFloat("Radius", &ao.Radius, 0.01f, 0.01f, 10.0f, "%.2f");
        EditorUI::THint("World space radius searched for occluders");
        EditorUI::TSliderFloat("Intensity", &ao.Intensity, 0.1f, 4.0f, "%.2f");
        EditorUI::THint("How dark occluded areas get");
        EditorUI::TSliderFloat("Falloff", &ao.FalloffRange, 0.05f, 1.0f, "%.2f");
        EditorUI::THint("Fraction of Radius over which occluders fade out");
        EditorUI::TDragFloat("Max Distance", &ao.MaxDistance, 0.5f, 1.0f, 1000.0f, "%.1f");
        EditorUI::THint("AO fades out completely at this view distance");

        if(mShowAdvanced)
        {
            EditorUI::TSeparator("Advanced", true);
            EditorUI::TSliderFloat("Resolution", &ao.ResolutionScale, 0.25f, 1.0f, "%.2fx");
            EditorUI::THint("AO target size relative to the scene, PostProcess upsamples it edge aware");
            EditorUI::TSliderInt("Slices", &ao.SliceCount, 1, 8);
            EditorUI::THint("Directions searched per pixel");
            EditorUI::TSliderInt("Steps / Slice", &ao.StepsPerSlice, 1, 16);
            EditorUI::THint("Samples along each direction: slices x steps x 2 taps per pixel");
            EditorUI::TSliderInt("Blur Radius", &ao.BlurRadius, 0, 6);
            EditorUI::THint("Bilateral blur radius in AO texels, 0 = no blur passes");
            EditorUI::TSliderFloat("Blur Sharpness", &ao.BlurSharpness, 0.0f, 32.0f, "%.1f");
            EditorUI::THint("How strongly the blur stops at depth edges");
            EditorUI::TCheckbox("Accurate Normals", &ao.AccurateNormals);
            EditorUI::THint("4 tap normal reconstruction instead of screen space derivatives, cleaner edges");
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    }

    void RendererSettingsInspector::DrawBloom(RenderSettings& settings)
    {
        ImGui::PushID("Bloom");
        BloomSettings& bloom = settings.Bloom;
        if(!EditorUI::TSection("Bloom"))
        {
            ImGui::PopID();
            return;
        }
        EditorUI::TCheckbox("Enabled", &bloom.Enable);
        EditorUI::THint("Glow around bright HDR pixels. Skipped when the main pass drew nothing or Intensity is 0");

        ImGui::BeginDisabled(!bloom.Enable);
        QualityRow<BloomQuality>(bloom, BLOOM_QUALITY_HINTS);
        EditorUI::TDragFloat("Threshold", &bloom.Threshold, 0.01f, 0.0f, 20.0f, "%.2f");
        EditorUI::THint("Linear HDR brightness where bloom starts");
        EditorUI::TSliderFloat("Knee", &bloom.Knee, 0.0f, 1.0f, "%.2f");
        EditorUI::THint("Soft threshold: fraction of Threshold faded in smoothly");
        EditorUI::TDragFloat("Intensity", &bloom.Intensity, 0.01f, 0.0f, 10.0f, "%.2f");
        EditorUI::THint("Strength of the glow added back to the scene");
        EditorUI::TSliderFloat("Scatter", &bloom.Scatter, 0.05f, 0.95f, "%.2f");
        EditorUI::THint("How far the glow spreads (mix between mips)");
        EditorUI::TColorEdit3("Tint", glm::value_ptr(bloom.Tint));
        EditorUI::THint("Color multiplied into the glow");

        if(mShowAdvanced)
        {
            EditorUI::TSeparator("Advanced", true);
            const bool supportsCompute = Core::GetRenderer()->GetRHI()->SupportsCompute();
            ImGui::BeginDisabled(!supportsCompute);
            EditorUI::TCheckbox("Use Compute", &bloom.UseCompute);
            EditorUI::THint(supportsCompute ? "Storage image path (desktop). Off: fragment path, tile friendly on mobile"
                                            : "This GPU's queue has no compute support, the fragment path is used");
            ImGui::EndDisabled();
            EditorUI::TSliderInt("Mip Count", &bloom.MipCount, BloomSettings::MIN_MIP_COUNT, BloomSettings::MAX_MIP_COUNT);
            EditorUI::THint("Mips in the chain: more = wider glow, a pass each way per mip");
            EditorUI::TCheckbox("HQ Filter", &bloom.HighQualityFilter);
            EditorUI::THint("13 tap downsample + 9 tap tent upsample, otherwise the cheaper dual filter");
            EditorUI::TCheckbox("Anti Flicker", &bloom.AntiFlicker);
            EditorUI::THint("Karis average on the first downsample, kills fireflies from tiny HDR highlights");
            EditorUI::TCheckbox("HQ Composite", &bloom.HighQualityComposite);
            EditorUI::THint("9 tap tent when PostProcess upscales the bloom, otherwise bilinear");
            EditorUI::TDragFloat("Clamp", &bloom.ClampValue, 10.0f, 1.0f, 65000.0f, "%.0f");
            EditorUI::THint("Max HDR value fed into bloom");
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    }

    void RendererSettingsInspector::DrawPostProcess(RenderSettings& settings)
    {
        ImGui::PushID("PostProcess");
        PostProcessSettings& post = settings.PostProcess;
        if(!EditorUI::TSection("Post Process"))
        {
            ImGui::PopID();
            return;
        }
        EditorUI::TSliderFloat("Vignette", &post.VignetteIntensity, 0.0f, 1.0f, "%.2f");
        EditorUI::THint("Darkens the screen edges");
        EditorUI::TSliderFloat("Vignette Softness", &post.VignetteSoftness, 0.01f, 1.0f, "%.2f");
        EditorUI::THint("Width of the vignette falloff");
        EditorUI::TSliderFloat("Grain", &post.Grain, 0.0f, 0.15f, "%.3f");
        EditorUI::THint("Film grain noise strength");
        ImGui::PopID();
    }

} // namespace Surge
