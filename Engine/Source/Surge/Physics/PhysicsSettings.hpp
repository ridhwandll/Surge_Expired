// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/String.hpp"
#include <glm/ext/vector_float3.hpp>
#include <array>
#include <cstdint>

namespace Surge
{
    // Project wide (saved in the .surgeproj, cooked into Project.rPROJ by the export, so every build ships them), applied with Physics::ApplySettings
    struct PhysicsSettings
    {
        static constexpr int MAX_LAYERS = 16;
        static constexpr int MIN_FIXED_RATE = 20;
        static constexpr int MAX_FIXED_RATE = 240;
        static constexpr int MAX_SUBSTEPS = 4;

        glm::vec3 Gravity = { 0.0f, -9.81f, 0.0f };
        int FixedRate = 60; // Simulation steps per second: 30 halves the physics CPU cost, at the price of stiffer/less precise contacts
        int CollisionSubsteps = 2;

        // Layer 0 is "Default", a layer without a name is unused. Bit b of CollisionMatrix[a]: layers a and b collide (kept symmetric)
        std::array<String, MAX_LAYERS> LayerNames;
        std::array<uint16_t, MAX_LAYERS> CollisionMatrix;

        PhysicsSettings()
        {
            LayerNames[0] = "Default";
            CollisionMatrix.fill(0xFFFF);
        }

        /* (Rid) Example of Collision matrix bit manipulation, for a 16-layer system:
         *
         * Let a = 0 and b = 2; collide = true
         * 
         * ShouldCollide(a, b):
         * Formula = (CollisionMatrix[a] >> b) & 1u
         * CollisionMatrix[a] >> b = 0000 0000 0000 0001 >> 2 = 0000 0000 0000 0000
         *                                               & 1u = 0000 0000 0000 0000 & 0000 0000 0000 0001 = 0 (return false)
         * 
         * SetCollision(a, b, collide):
         * 
         * bitA = 0000 0000 0000 0001
         * bitB = 0000 0000 0000 0100
         *
         * CollisionMatrix[a] = stored in the matrix for layer 0, bit 2 is set to '1' if collide with layer b is true, otherwise it is set to 0
         * CollisionMatrix[b] = stored in the matrix for layer 2, bit 0 is set to '1' if collide with layer a is true, otherwise it is set to 0
         * 
         */

        bool ShouldCollide(int a, int b) const { return (CollisionMatrix[a] >> b) & 1u; }
        void SetCollision(int a, int b, bool collide)
        {
            const uint16_t bitA = static_cast<uint16_t>(1u << a); 
            const uint16_t bitB = static_cast<uint16_t>(1u << b);
            CollisionMatrix[a] = collide ? (CollisionMatrix[a] | bitB) : (CollisionMatrix[a] & ~bitB);
            CollisionMatrix[b] = collide ? (CollisionMatrix[b] | bitA) : (CollisionMatrix[b] & ~bitA);
        }

        float GetFixedTimestep() const { return 1.0f / static_cast<float>(FixedRate); }

        // Index of the named layer, -1 when no layer has that name
        int FindLayer(const String& name) const
        {
            for(int i = 0; i < MAX_LAYERS; i++)
            {
                if(!LayerNames[i].empty() && LayerNames[i] == name)
                    return i;
            }

            return -1;
        }

        static int ClampLayer(int layer) { return layer < 0 || layer >= MAX_LAYERS ? 0 : layer; }
    };

} // namespace Surge
