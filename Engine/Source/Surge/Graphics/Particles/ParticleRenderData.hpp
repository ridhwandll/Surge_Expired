// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Graphics/RenderGraph/RenderSettings.hpp" // ParticleSettings
#include <glm/glm.hpp>

namespace Surge
{
    enum class ParticleRenderMode : uint8_t
    {
        BILLBOARD,  // Faces the camera
        STRETCHED,  // Stretched along the velocity (sparks, rain)
        HORIZONTAL, // Flat on the XZ plane (ground decals, shockwaves)
        VERTICAL    // Faces the camera but stays upright
    };

    enum class ParticleSortMode : uint8_t
    {
        NONE,          // Submission order, right for additive particles
        DISTANCE,      // Back to front per view (ParticlePass), alpha blended smoke
        OLDEST_FIRST,  // Oldest drawn first (newest on top)
        YOUNGEST_FIRST // Youngest drawn first
    };

    // Written by the scene each frame, shown by ParticlePass::OnImGuiRender
    struct ParticleStats
    {
        Uint Systems = 0;        // Particle systems updated this frame
        Uint CulledSystems = 0;  // Outside the camera: not submitted (and paused depending on their culling mode)
        Uint AliveParticles = 0; // Including the culled ones
    };

    // One particle as Particle.glsl reads it (std430), packed on the CPU right after the simulation
    struct ParticleRecord
    {
        glm::vec3 Position;    // World space
        Uint Size;             // half2: width, height
        Uint RotationFrame;    // half: rotation in radians | uint16: flipbook frame in 8.8 fixed point (fraction = blend to the next frame)
        Uint Color;            // RGBA8, sRGB like every authored color
        Uint VelocityXY;       // half2, world space (stretched billboards)
        Uint VelocityZEmitter; // half: velocity z | uint16: index of the emitter in FrameBlackboard::ParticleEmitterList
    };
    static_assert(sizeof(ParticleRecord) == 32, "ParticleRecord must stay 32 bytes, it mirrors Particle.glsl");

    // One emitter of one particle system, pushed by Renderer::SubmitParticles
    struct ParticleEmitterCmd
    {
        Uint FirstRecord = 0; // Into FrameBlackboard::ParticleList, set by Renderer::SubmitParticles
        Uint RecordCount = 0;

        ImageHandle Texture = ImageHandle::Invalid(); // Invalid = the default soft particle
        ParticleRenderMode Mode = ParticleRenderMode::BILLBOARD;
        ParticleSortMode Sort = ParticleSortMode::NONE;
        int SortingOrder = 0; // Higher draws later (on top), before the distance between emitters

        Uint FlipbookColumns = 1;
        Uint FlipbookRows = 1;
        bool FrameBlending = false;

        float AdditiveAmount = 0.0f;    // 0 = alpha blended, 1 = additive, both in one premultiplied pipeline
        float EmissiveIntensity = 1.0f; // HDR multiplier, > 1 feeds Bloom
        float MaxScreenSize = 1.0f;     // Max particle height as a fraction of the screen height
        float StretchSpeedScale = 0.0f;
        float StretchLengthScale = 1.0f;

        glm::vec3 BoundsCenter = glm::vec3(0.0f); // World space, sorts emitters back to front
    };

} // namespace Surge
