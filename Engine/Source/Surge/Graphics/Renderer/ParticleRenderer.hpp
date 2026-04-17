// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include <glm/glm.hpp>

// ---------------------------------------------------------------------------
// ParticleRenderer.hpp
//   Stub particle renderer.
//   GPU-driven particle simulation and rendering will be implemented here
//   once the compute-dispatch and instanced-draw paths are wired in.
// ---------------------------------------------------------------------------

namespace Surge
{
    struct ParticleEmitterDesc
    {
        glm::vec3 Position      = {};
        float     EmitRate      = 100.0f; // particles per second
        float     ParticleSize  = 0.1f;
        float     Lifetime      = 2.0f;   // seconds
        glm::vec4 StartColor    = {1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec4 EndColor      = {1.0f, 1.0f, 1.0f, 0.0f};
        glm::vec3 Velocity      = {0.0f, 1.0f, 0.0f};
        float     VelocitySpread = 0.3f;
    };

    class SURGE_API ParticleRenderer
    {
    public:
        ParticleRenderer()  = default;
        ~ParticleRenderer() = default;

        SURGE_DISABLE_COPY_AND_MOVE(ParticleRenderer);

        void Initialize() {}
        void Shutdown()   {}

        void Emit(const ParticleEmitterDesc& /*desc*/) {}
        void Update(float /*dt*/)                       {}
        void Render(RHI::CommandBufferHandle /*cmd*/)   {}
    };

} // namespace Surge
