// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/RendererSettingsInspector.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/ECS/Components/Components.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/RenderGraph/Passes/GeometryPass.hpp"
#include "Utility/ImGuiAux.hpp"

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <array>

namespace Surge
{
    namespace
    {
        const ImVec4 HINT_COLOR(0.55f, 0.55f, 0.55f, 1.0f);
        const ImVec4 WARNING_COLOR(0.95f, 0.65f, 0.30f, 1.0f);
        constexpr float HINT_FONT_SIZE = 13.0f;

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

        // Name cell, then the value cell sized for the widget that follows
        void Label(const char* label)
        {
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
        }

        void Hint(const char* text, const ImVec4& color = HINT_COLOR)
        {
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::PushTextWrapPos(0.0f);
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0], HINT_FONT_SIZE);
            ImGui::TextColored(color, "%s", text);
            ImGui::PopFont();
            ImGui::PopTextWrapPos();
        }

        bool Checkbox(const char* label, bool* value, const char* hint)
        {
            ImGui::PushID(label);
            Label(label);
            const bool changed = ImGui::Checkbox("##Value", value);
            ImGui::PopID();
            Hint(hint);
            return changed;
        }

        bool SliderFloat(const char* label, float* value, float min, float max, const char* format, const char* hint)
        {
            ImGui::PushID(label);
            Label(label);
            bool changed = false;
            {
                ImGuiAux::ScopedBoldFont font;
                changed = ImGui::SliderFloat("##Value", value, min, max, format, ImGuiSliderFlags_AlwaysClamp);
            }
            ImGui::PopID();
            Hint(hint);
            return changed;
        }

        bool DragFloat(const char* label, float* value, float speed, float min, float max, const char* format, const char* hint)
        {
            ImGui::PushID(label);
            Label(label);
            bool changed = false;
            {
                ImGuiAux::ScopedBoldFont font;
                changed = ImGui::DragFloat("##Value", value, speed, min, max, format, ImGuiSliderFlags_AlwaysClamp);
            }
            ImGui::PopID();
            Hint(hint);
            return changed;
        }

        bool SliderInt(const char* label, int* value, int min, int max, const char* hint)
        {
            ImGui::PushID(label);
            Label(label);
            bool changed = false;
            {
                ImGuiAux::ScopedBoldFont font;
                changed = ImGui::SliderInt("##Value", value, min, max, "%d", ImGuiSliderFlags_AlwaysClamp);
            }
            ImGui::PopID();
            Hint(hint);
            return changed;
        }

        bool DragUint(const char* label, Uint* value, float speed, int min, int max, const char* hint)
        {
            ImGui::PushID(label);
            Label(label);
            int edited = static_cast<int>(std::min<Uint>(*value, static_cast<Uint>(max)));
            bool changed = false;
            {
                ImGuiAux::ScopedBoldFont font;
                changed = ImGui::DragInt("##Value", &edited, speed, min, max, "%d", ImGuiSliderFlags_AlwaysClamp);
            }
            if(changed)
                *value = static_cast<Uint>(edited);
            ImGui::PopID();
            Hint(hint);
            return changed;
        }

        bool Combo(const char* label, int* index, const char* const* items, int count, const char* hint)
        {
            ImGui::PushID(label);
            Label(label);
            bool changed = false;
            {
                ImGuiAux::ScopedBoldFont font;
                changed = ImGui::Combo("##Value", index, items, count);
            }
            ImGui::PopID();
            Hint(hint);
            return changed;
        }

        bool Color(const char* label, glm::vec3* value, const char* hint)
        {
            ImGui::PushID(label);
            Label(label);
            const bool changed = ImGui::ColorEdit3("##Value", glm::value_ptr(*value));
            ImGui::PopID();
            Hint(hint);
            return changed;
        }

        // Quality preset combo, the hint describes what the selected preset costs
        template <typename Q, typename Settings>
        void QualityRow(Settings& settings, const std::array<const char*, 4>& descriptions)
        {
            ImGui::PushID("Quality");
            Label("Quality");
            int quality = static_cast<int>(settings.Quality);
            {
                ImGuiAux::ScopedBoldFont font;
                if(ImGui::Combo("##Value", &quality, QUALITY_NAMES.data(), static_cast<int>(QUALITY_NAMES.size())))
                    settings.ApplyQualityPreset(static_cast<Q>(quality));
            }
            ImGui::PopID();
            Hint(descriptions[std::clamp(static_cast<int>(settings.Quality), 0, 3)]);
        }

        // Collapsible section title spanning both columns, ImGui keeps the open state per ID
        bool Section(const char* title)
        {
            ImGuiAux::ScopedBoldFont font;
            ImGui::TableNextColumn();
            const bool open = ImGui::TreeNodeEx(title, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAllColumns | ImGuiTreeNodeFlags_NoTreePushOnOpen);
            ImGui::TableNextColumn();
            return open;
        }

        void AdvancedSeparator(bool show)
        {
            if(show)
                ImGuiAux::TSperator("Advanced", true);
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
            Hint(winner == entt::null ? "Inactive: the scene renders with the platform defaults" : "Inactive: another Renderer Settings component is used", WARNING_COLOR);
        else if(winner != static_cast<entt::entity>(entity))
            Hint("Ignored: another active Renderer Settings component comes first in the scene", WARNING_COLOR);

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

        Label("Profile");
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
        Hint("Windows loads Desktop, Android loads Mobile. The profile you edit is the one every viewport previews");

        Label("Tools");
        if(ImGui::Button("Reset Profile"))
            component.GetProfileSettings(current) = RenderSettings::Default(current);
        ImGuiAux::ItemTooltip("Back to the engine defaults of this platform");
        ImGui::SameLine();
        if(ImGui::Button(current == RenderProfile::MOBILE ? "Copy from Desktop" : "Copy from Mobile"))
            component.GetProfileSettings(current) = component.GetProfileSettings(other);
        ImGuiAux::ItemTooltip("Overwrites this profile with the other one");

        Checkbox("Show Advanced", &mShowAdvanced, "Shows the knobs the quality presets set, they are saved with the scene too");
    }

    void RendererSettingsInspector::DrawGeneral(RenderSettings& settings)
    {
        ImGui::PushID("General"); // Labels repeat between sections (Enabled, Quality, Intensity...), they make the widget IDs
        if(!Section("General"))
        {
            ImGui::PopID();
            return;
        }

        // Applied on release: each change rebuilds the scene targets of every graph
        if(!mEditingRenderScale)
            mRenderScale = settings.RenderScale;
        ImGui::PushID("RenderScale");
        Label("Render Scale");
        {
            ImGuiAux::ScopedBoldFont font;
            ImGui::SliderFloat("##Value", &mRenderScale, RenderScaleSettings::MIN_SCALE, RenderScaleSettings::MAX_SCALE, "%.2fx", ImGuiSliderFlags_AlwaysClamp);
        }
        mEditingRenderScale = ImGui::IsItemActive();
        if(ImGui::IsItemDeactivatedAfterEdit())
            settings.RenderScale = mRenderScale;
        ImGui::PopID();
        Hint("Resolution of the 3D scene (main pass, AO, bloom). UI and post-processing stay at full resolution");

        DragUint("Max Lights", &settings.Lighting.MaxLights, 0.25f, 1, static_cast<int>(GeometryPass::MAX_LIGHTS),
                 "Most relevant lights kept after culling, every lit pixel loops over all of them");

        ImGui::PopID();

        ImGui::PushID("ParticlesAnimation");
        if(!Section("Particles & Animation"))
        {
            ImGui::PopID();
            return;
        }
        DragUint("Max Particles", &settings.Particles.MaxParticles, 64.0f, 0, 1 << 20,
                 "Live particles of every particle system together, emission stops at this budget");
        SliderFloat("Emission Quality", &settings.Particles.EmissionQuality, ParticleSettings::MIN_EMISSION_QUALITY, 1.0f, "%.2f",
                    "Scales every emission rate and burst, a quality knob for weaker devices");
        DragUint("Max Skin Matrices", &settings.Animation.MaxSkinMatrices, 16.0f, 0, 1 << 18,
                 "Joints of every skinned mesh drawn in a frame together (48 bytes each), animators past it are not drawn");

        ImGuiAux::TSperator("Debug", true);
        Checkbox("Show Cascades", &settings.Shadows.ShowCascades, "Tints each shadow cascade. Not saved");
        ImGui::BeginDisabled(!settings.AO.Enable);
        Checkbox("Show AO Only", &settings.PostProcess.ShowAOOnly, "Outputs the upsampled AO buffer. Not saved");
        ImGui::EndDisabled();

        ImGui::PopID();
    }

    void RendererSettingsInspector::DrawShadows(RenderSettings& settings)
    {
        ImGui::PushID("Shadows");
        ShadowSettings& shadows = settings.Shadows;
        if(!Section("Shadows"))
        {
            ImGui::PopID();
            return;
        }
        Checkbox("Enabled", &shadows.Enable, "Cascaded shadow maps of the directional light, one extra render pass");

        ImGui::BeginDisabled(!shadows.Enable);
        QualityRow<ShadowQuality>(shadows, SHADOW_QUALITY_HINTS);
        DragFloat("Distance", &shadows.Distance, 0.5f, 1.0f, 2000.0f, "%.1f", "View distance the cascades cover, shorter = sharper shadows");
        SliderFloat("Fade Range", &shadows.FadeRange, 0.0f, 0.5f, "%.2f", "Fraction of Distance over which shadows fade out");
        SliderFloat("Split Lambda", &shadows.CascadeSplitLambda, 0.0f, 1.0f, "%.2f", "0 = uniform cascade splits, 1 = logarithmic (more resolution near the camera)");
        SliderFloat("Cascade Blend", &shadows.CascadeBlend, 0.0f, 0.5f, "%.2f", "Fraction of each cascade blended into the next one, hides the resolution jump");
        DragFloat("Normal Bias", &shadows.NormalBias, 0.05f, 0.0f, 10.0f, "%.2f", "Receiver offset along its normal in shadow texels, fixes acne at grazing angles");
        DragFloat("Bias Constant", &shadows.DepthBiasConstant, 0.05f, 0.0f, 100.0f, "%.2f", "Constant depth bias of the casters: too low = acne, too high = detached shadows");
        DragFloat("Bias Slope", &shadows.DepthBiasSlope, 0.05f, 0.0f, 100.0f, "%.2f", "Depth bias scaled by how steep the caster surface is to the light");

        AdvancedSeparator(mShowAdvanced);
        if(mShowAdvanced)
        {
            DragFloat("Bias Clamp", &shadows.DepthBiasClamp, 0.05f, 0.0f, 100.0f, "%.2f", "Upper limit of the depth bias, 0 = no limit");

            constexpr const char* cascadeNames[] = { "1", "2", "3" };
            static_assert(IM_ARRAYSIZE(cascadeNames) == MAX_SHADOW_CASCADE_COUNT);
            int cascadeIndex = std::clamp(shadows.CascadeCount, 1, MAX_SHADOW_CASCADE_COUNT) - 1;
            if(Combo("Cascades", &cascadeIndex, cascadeNames, IM_ARRAYSIZE(cascadeNames), "Every cascade draws its shadow casters again"))
                shadows.CascadeCount = cascadeIndex + 1;

            constexpr std::array<Uint, 4> resolutions = { 512, 1024, 2048, 4096 };
            constexpr const char* resolutionNames[] = { "512", "1024", "2048", "4096" };
            static_assert(IM_ARRAYSIZE(resolutionNames) == resolutions.size());
            static_assert(resolutions.front() == ShadowSettings::MIN_RESOLUTION && resolutions.back() == ShadowSettings::MAX_RESOLUTION);
            int resolutionIndex = static_cast<int>(std::lower_bound(resolutions.begin(), resolutions.end(), shadows.Resolution) - resolutions.begin());
            resolutionIndex = std::min(resolutionIndex, static_cast<int>(resolutions.size()) - 1);
            if(Combo("Resolution", &resolutionIndex, resolutionNames, IM_ARRAYSIZE(resolutionNames), "Per cascade, D16: 2048 = 8 MB, 4096 = 32 MB each"))
                shadows.Resolution = resolutions[resolutionIndex];

            constexpr const char* filterNames[] = { "3x3 (4 taps)", "5x5 (9 taps)", "7x7 (16 taps)" };
            int filterIndex = std::clamp((shadows.FilterSize - 3) / 2, 0, 2);
            if(Combo("Filter", &filterIndex, filterNames, IM_ARRAYSIZE(filterNames), "Tent PCF width: wider = softer edges, more taps per lit pixel"))
                shadows.FilterSize = 3 + filterIndex * 2;

            Checkbox("Dithered Blend", &shadows.DitheredBlend, "Picks one cascade per pixel in the blend band (noise) instead of sampling both");
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    }

    void RendererSettingsInspector::DrawAmbientOcclusion(RenderSettings& settings)
    {
        ImGui::PushID("AmbientOcclusion");
        AmbientOcclusionSettings& ao = settings.AO;
        if(!Section("Ambient Occlusion"))
        {
            ImGui::PopID();
            return;
        }
        Checkbox("Enabled", &ao.Enable, "GTAO from depth: 4 extra render passes + a stored depth buffer. Skipped on frames without 3D geometry");

        ImGui::BeginDisabled(!ao.Enable);
        QualityRow<AOQuality>(ao, AO_QUALITY_HINTS);
        DragFloat("Radius", &ao.Radius, 0.01f, 0.01f, 10.0f, "%.2f", "World space radius searched for occluders");
        SliderFloat("Intensity", &ao.Intensity, 0.1f, 4.0f, "%.2f", "How dark occluded areas get");
        SliderFloat("Falloff", &ao.FalloffRange, 0.05f, 1.0f, "%.2f", "Fraction of Radius over which occluders fade out");
        DragFloat("Max Distance", &ao.MaxDistance, 0.5f, 1.0f, 1000.0f, "%.1f", "AO fades out completely at this view distance");

        AdvancedSeparator(mShowAdvanced);
        if(mShowAdvanced)
        {
            SliderFloat("Resolution", &ao.ResolutionScale, 0.25f, 1.0f, "%.2fx", "AO target size relative to the scene, PostProcess upsamples it edge aware");
            SliderInt("Slices", &ao.SliceCount, 1, 8, "Directions searched per pixel");
            SliderInt("Steps / Slice", &ao.StepsPerSlice, 1, 16, "Samples along each direction: slices x steps x 2 taps per pixel");
            SliderInt("Blur Radius", &ao.BlurRadius, 0, 6, "Bilateral blur radius in AO texels, 0 = no blur passes");
            SliderFloat("Blur Sharpness", &ao.BlurSharpness, 0.0f, 32.0f, "%.1f", "How strongly the blur stops at depth edges");
            Checkbox("Accurate Normals", &ao.AccurateNormals, "4 tap normal reconstruction instead of screen space derivatives, cleaner edges");
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    }

    void RendererSettingsInspector::DrawBloom(RenderSettings& settings)
    {
        ImGui::PushID("Bloom");
        BloomSettings& bloom = settings.Bloom;
        if(!Section("Bloom"))
        {
            ImGui::PopID();
            return;
        }
        Checkbox("Enabled", &bloom.Enable, "Glow around bright HDR pixels. Skipped when the main pass drew nothing or Intensity is 0");

        ImGui::BeginDisabled(!bloom.Enable);
        QualityRow<BloomQuality>(bloom, BLOOM_QUALITY_HINTS);
        DragFloat("Threshold", &bloom.Threshold, 0.01f, 0.0f, 20.0f, "%.2f", "Linear HDR brightness where bloom starts");
        SliderFloat("Knee", &bloom.Knee, 0.0f, 1.0f, "%.2f", "Soft threshold: fraction of Threshold faded in smoothly");
        DragFloat("Intensity", &bloom.Intensity, 0.01f, 0.0f, 10.0f, "%.2f", "Strength of the glow added back to the scene");
        SliderFloat("Scatter", &bloom.Scatter, 0.05f, 0.95f, "%.2f", "How far the glow spreads (mix between mips)");
        Color("Tint", &bloom.Tint, "Color multiplied into the glow");

        AdvancedSeparator(mShowAdvanced);
        if(mShowAdvanced)
        {
            const bool supportsCompute = Core::GetRenderer()->GetRHI()->SupportsCompute();
            ImGui::BeginDisabled(!supportsCompute);
            Checkbox("Use Compute", &bloom.UseCompute, supportsCompute ? "Storage image path (desktop). Off: fragment path, tile friendly on mobile"
                                                                       : "This GPU's queue has no compute support, the fragment path is used");
            ImGui::EndDisabled();
            SliderInt("Mip Count", &bloom.MipCount, BloomSettings::MIN_MIP_COUNT, BloomSettings::MAX_MIP_COUNT, "Mips in the chain: more = wider glow, a pass each way per mip");
            Checkbox("HQ Filter", &bloom.HighQualityFilter, "13 tap downsample + 9 tap tent upsample, otherwise the cheaper dual filter");
            Checkbox("Anti Flicker", &bloom.AntiFlicker, "Karis average on the first downsample, kills fireflies from tiny HDR highlights");
            Checkbox("HQ Composite", &bloom.HighQualityComposite, "9 tap tent when PostProcess upscales the bloom, otherwise bilinear");
            DragFloat("Clamp", &bloom.ClampValue, 10.0f, 1.0f, 65000.0f, "%.0f", "Max HDR value fed into bloom");
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    }

    void RendererSettingsInspector::DrawPostProcess(RenderSettings& settings)
    {
        ImGui::PushID("PostProcess");
        PostProcessSettings& post = settings.PostProcess;
        if(!Section("Post Process"))
        {
            ImGui::PopID();
            return;
        }
        SliderFloat("Vignette", &post.VignetteIntensity, 0.0f, 1.0f, "%.2f", "Darkens the screen edges");
        SliderFloat("Vignette Softness", &post.VignetteSoftness, 0.01f, 1.0f, "%.2f", "Width of the vignette falloff");
        SliderFloat("Grain", &post.Grain, 0.0f, 0.15f, "%.3f", "Film grain noise strength");
        ImGui::PopID();
    }

} // namespace Surge
