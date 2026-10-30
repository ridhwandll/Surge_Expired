// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Gizmos/ParticleGizmos.hpp"
#include "Utility/EditorUI.hpp"
#include "Surge/Graphics/Particles/ParticleEffect.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Surge
{
    namespace ParticleGizmos
    {
        static constexpr int SEGMENTS = GizmoCanvas::CIRCLE_SEGMENTS;

        glm::mat4 GetShapeTransform(const ParticleShapeModule& shape)
        {
            return glm::translate(glm::mat4(1.0f), shape.Position) * glm::mat4_cast(glm::quat(glm::radians(shape.Rotation)));
        }

        void DrawEmitterShape(GizmoCanvas& canvas, const ParticleShapeModule& shape, const glm::mat4& world, ImU32 color, float thickness)
        {
            const glm::mat4 shapeWorld = world * GetShapeTransform(shape);
            const auto toWorld = [&shapeWorld](const glm::vec3& p) { return glm::vec3(shapeWorld * glm::vec4(p, 1.0f)); };
            const float arc = glm::radians(glm::clamp(shape.Arc, 1.0f, 360.0f));
            const bool fullCircle = shape.Arc >= 359.9f;

            // Arc of radius r in the XZ plane at height y
            const auto drawArc = [&](float r, float y, ImU32 arcColor) {
                glm::vec3 points[SEGMENTS];
                const int count = fullCircle ? SEGMENTS : SEGMENTS - 1;
                for(int i = 0; i < count; i++)
                {
                    const float angle = static_cast<float>(i) / static_cast<float>(fullCircle ? count : count - 1) * arc;
                    points[i] = toWorld(glm::vec3(glm::cos(angle) * r, y, glm::sin(angle) * r));
                }
                canvas.DrawPolyline3D(points, count, fullCircle, arcColor, thickness);
            };

            switch(shape.Type)
            {
                case ParticleShapeType::POINT:
                {
                    const glm::vec3 center = glm::vec3(shapeWorld[3]);
                    const float size = canvas.GetWorldPerPixel(center) * 8.0f;
                    canvas.DrawLine3D(center - glm::vec3(size, 0.0f, 0.0f), center + glm::vec3(size, 0.0f, 0.0f), color, thickness);
                    canvas.DrawLine3D(center - glm::vec3(0.0f, size, 0.0f), center + glm::vec3(0.0f, size, 0.0f), color, thickness);
                    canvas.DrawLine3D(center - glm::vec3(0.0f, 0.0f, size), center + glm::vec3(0.0f, 0.0f, size), color, thickness);
                    break;
                }
                case ParticleShapeType::SPHERE:
                {
                    const glm::mat3 basis = glm::mat3(shapeWorld);
                    const float scale = (glm::length(basis[0]) + glm::length(basis[1]) + glm::length(basis[2])) / 3.0f;
                    canvas.DrawWireSphere(glm::vec3(shapeWorld[3]), shape.Radius * scale, color, thickness);
                    break;
                }
                case ParticleShapeType::HEMISPHERE:
                {
                    drawArc(shape.Radius, 0.0f, color);
                    for(int side = 0; side < 2; side++)
                    {
                        glm::vec3 points[SEGMENTS / 2 + 1];
                        for(int i = 0; i <= SEGMENTS / 2; i++)
                        {
                            const float angle = static_cast<float>(i) / static_cast<float>(SEGMENTS / 2) * glm::pi<float>();
                            const glm::vec2 xy = glm::vec2(glm::cos(angle), glm::sin(angle)) * shape.Radius;
                            points[i] = toWorld(side == 0 ? glm::vec3(xy.x, xy.y, 0.0f) : glm::vec3(0.0f, xy.y, xy.x));
                        }
                        canvas.DrawPolyline3D(points, SEGMENTS / 2 + 1, false, EditorUI::ScaleAlpha(color, 0.6f), thickness);
                    }
                    break;
                }
                case ParticleShapeType::CONE:
                {
                    // Base circle and the circle one unit up, where the edge directions lead
                    const float length = glm::max(shape.Radius, 1.0f);
                    const float topRadius = shape.Radius + glm::tan(glm::radians(glm::clamp(shape.Angle, 0.0f, 89.0f))) * length;
                    drawArc(shape.Radius, 0.0f, color);
                    drawArc(topRadius, length, EditorUI::ScaleAlpha(color, 0.6f));
                    for(int i = 0; i < 4; i++)
                    {
                        const float angle = static_cast<float>(i) * glm::half_pi<float>();
                        if(angle > arc + 1e-3f)
                            break;
                        const glm::vec2 direction = { glm::cos(angle), glm::sin(angle) };
                        canvas.DrawLine3D(toWorld(glm::vec3(direction.x * shape.Radius, 0.0f, direction.y * shape.Radius)),
                                          toWorld(glm::vec3(direction.x * topRadius, length, direction.y * topRadius)), color, thickness);
                    }
                    break;
                }
                case ParticleShapeType::CIRCLE:
                    drawArc(shape.Radius, 0.0f, color);
                    break;
                case ParticleShapeType::BOX:
                {
                    const glm::vec3 h = shape.BoxSize * 0.5f;
                    glm::vec3 corners[8];
                    for(int i = 0; i < 8; i++)
                        corners[i] = toWorld(glm::vec3((i & 1) ? h.x : -h.x, (i & 2) ? h.y : -h.y, (i & 4) ? h.z : -h.z));
                    static constexpr int edges[12][2] = { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
                    for(const auto& edge : edges)
                        canvas.DrawLine3D(corners[edge[0]], corners[edge[1]], color, thickness);
                    break;
                }
                case ParticleShapeType::EDGE:
                    canvas.DrawLine3D(toWorld(glm::vec3(-shape.Radius, 0.0f, 0.0f)), toWorld(glm::vec3(shape.Radius, 0.0f, 0.0f)), color, thickness);
                    break;
            }
        }

        void DrawEmitDirection(GizmoCanvas& canvas, const ParticleShapeModule& shape, const glm::mat4& world, ImU32 color, float thickness)
        {
            if(shape.Type == ParticleShapeType::SPHERE || shape.Type == ParticleShapeType::CIRCLE)
                return;

            const glm::mat4 shapeWorld = world * GetShapeTransform(shape);
            const glm::vec3 origin = glm::vec3(shapeWorld[3]);
            const glm::vec3 up = glm::vec3(shapeWorld[1]);
            const float upLength = glm::length(up);
            if(upLength < 1e-6f)
                return;

            const float unit = canvas.GetWorldPerPixel(origin);
            canvas.DrawArrow3D(origin, origin + (up / upLength) * unit * 70.0f, unit * 12.0f, color, thickness);
        }

        void DrawCollisionPlane(GizmoCanvas& canvas, const ParticleCollisionPlane& plane, const glm::vec3& focus, float halfSize, ImU32 color, float thickness)
        {
            const float normalLength = glm::length(plane.Normal);
            if(normalLength < 1e-6f)
                return;

            // Dot(Normal, p) = Offset, the normal is not required to be unit length
            const glm::vec3 n = plane.Normal / normalLength;
            const glm::vec3 center = focus - n * (glm::dot(n, focus) - plane.Offset / normalLength);
            glm::vec3 u, v;
            GizmoCanvas::GetPerpendicularBasis(n, u, v);

            constexpr int lines = 6;
            const ImU32 gridColor = EditorUI::ScaleAlpha(color, 0.45f);
            for(int i = 0; i <= lines; i++)
            {
                const float t = (static_cast<float>(i) / static_cast<float>(lines) * 2.0f - 1.0f) * halfSize;
                const bool border = i == 0 || i == lines;
                canvas.DrawLine3D(center + u * t - v * halfSize, center + u * t + v * halfSize, border ? color : gridColor, thickness);
                canvas.DrawLine3D(center + v * t - u * halfSize, center + v * t + u * halfSize, border ? color : gridColor, thickness);
            }

            const float unit = canvas.GetWorldPerPixel(center);
            canvas.DrawArrow3D(center, center + n * unit * 45.0f, unit * 10.0f, color, thickness);
        }

    } // namespace ParticleGizmos
} // namespace Surge
