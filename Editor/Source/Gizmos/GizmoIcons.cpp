// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Gizmos/GizmoIcons.hpp"
#include "Utility/EditorUI.hpp"
#include "Surge/ECS/Scene.hpp"
#include "Surge/ECS/Components/Components.hpp"
#include "Surge/ECS/Components/ScriptComponent.hpp"
#include <glm/glm.hpp>

namespace Surge::GizmoIcons
{
    static constexpr float PI = 3.14159265358979f;

    // (Rid) A square from -1 to 1 on both axes. (0, 0) is the center, (-1, -1) the top left, (1, 1) the bottom right. Y POINTS DOWN
    struct GlyphFrame
    {
        ImVec2 Center;
        float HalfSize;

        ImVec2 operator()(float x, float y) const { return ImVec2(Center.x + x * HalfSize, Center.y + y * HalfSize); }
        float operator()(float length) const { return length * HalfSize; }
    };

    static void DrawCamera(ImDrawList* drawList, const GlyphFrame& p, ImU32 color, ImU32 cutoutColor)
    {
        // Film reels, body and lens
        drawList->AddCircleFilled(p(-0.48f, -0.45f), p(0.26f), color, 20);
        drawList->AddCircleFilled(p(0.04f, -0.45f), p(0.26f), color, 20);
        drawList->AddCircleFilled(p(-0.48f, -0.45f), p(0.09f), cutoutColor, 12);
        drawList->AddCircleFilled(p(0.04f, -0.45f), p(0.09f), cutoutColor, 12);
        drawList->AddRectFilled(p(-0.86f, -0.15f), p(0.36f, 0.70f), color, p(0.12f));
        drawList->AddQuadFilled(p(0.36f, 0.16f), p(0.88f, -0.10f), p(0.88f, 0.66f), p(0.36f, 0.40f), color);
    }

    static void DrawPointLight(ImDrawList* drawList, const GlyphFrame& p, ImU32 color, ImU32 cutoutColor, float stroke)
    {
        // Bulb, neck and screw base
        drawList->AddCircleFilled(p(0.0f, -0.24f), p(0.55f), color, 28);
        drawList->AddQuadFilled(p(-0.36f, 0.10f), p(0.36f, 0.10f), p(0.24f, 0.44f), p(-0.24f, 0.44f), color);
        drawList->AddRectFilled(p(-0.25f, 0.51f), p(0.25f, 0.62f), color, p(0.04f));
        drawList->AddRectFilled(p(-0.17f, 0.68f), p(0.17f, 0.80f), color, p(0.05f));

        // Glass highlight
        drawList->PathArcTo(p(0.0f, -0.24f), p(0.34f), PI * 1.05f, PI * 1.45f, 8);
        drawList->PathStroke(cutoutColor, ImDrawFlags_None, stroke * 0.8f);
    }

    static void DrawDirectionalLight(ImDrawList* drawList, const GlyphFrame& p, ImU32 color, float stroke)
    {
        drawList->AddCircleFilled(p(0.0f, 0.0f), p(0.36f), color, 24);
        for(int i = 0; i < 8; i++)
        {
            const float angle = (float)i * PI * 0.25f;
            const float c = glm::cos(angle), s = glm::sin(angle);
            drawList->AddLine(p(c * 0.56f, s * 0.56f), p(c * 0.90f, s * 0.90f), color, stroke);
        }
    }

    static void DrawAudioSource(ImDrawList* drawList, const GlyphFrame& p, ImU32 color, float stroke)
    {
        // Speaker and two sound waves
        drawList->AddRectFilled(p(-0.82f, -0.26f), p(-0.42f, 0.26f), color, p(0.06f));
        drawList->AddQuadFilled(p(-0.42f, -0.26f), p(0.02f, -0.68f), p(0.02f, 0.68f), p(-0.42f, 0.26f), color);

        drawList->PathArcTo(p(0.06f, 0.0f), p(0.38f), -PI * 0.25f, PI * 0.25f, 10);
        drawList->PathStroke(color, ImDrawFlags_None, stroke);
        drawList->PathArcTo(p(0.06f, 0.0f), p(0.72f), -PI * 0.28f, PI * 0.28f, 14);
        drawList->PathStroke(color, ImDrawFlags_None, stroke);
    }

    static void DrawAudioListener(ImDrawList* drawList, const GlyphFrame& p, ImU32 color, float stroke)
    {
        // Headphones: band and ear cups
        drawList->PathArcTo(p(0.0f, 0.04f), p(0.68f), PI, PI * 2.0f, 24);
        drawList->PathStroke(color, ImDrawFlags_None, stroke * 1.1f);
        drawList->AddRectFilled(p(-0.88f, -0.02f), p(-0.48f, 0.66f), color, p(0.14f));
        drawList->AddRectFilled(p(0.48f, -0.02f), p(0.88f, 0.66f), color, p(0.14f));
    }

    static void DrawEnvironment(ImDrawList* drawList, const GlyphFrame& p, ImU32 color, float stroke)
    {
        // Sun rising over the horizon
        drawList->PathArcTo(p(0.0f, 0.16f), p(0.42f), PI, PI * 2.0f, 20);
        drawList->PathFillConvex(color);
        for(int i = 1; i <= 5; i++)
        {
            const float angle = PI + (float)i * PI / 6.0f;
            const float c = glm::cos(angle), s = glm::sin(angle);
            drawList->AddLine(p(c * 0.60f, 0.16f + s * 0.60f), p(c * 0.86f, 0.16f + s * 0.86f), color, stroke);
        }
        drawList->AddLine(p(-0.90f, 0.28f), p(0.90f, 0.28f), color, stroke);
        drawList->AddLine(p(-0.55f, 0.52f), p(0.55f, 0.52f), color, stroke);
        drawList->AddLine(p(-0.25f, 0.74f), p(0.25f, 0.74f), color, stroke);
    }

    // Isometric cube outline: hexagon silhouette + the three edges meeting at the front corner
    static void DrawEntity(ImDrawList* drawList, const GlyphFrame& p, ImU32 color, float stroke)
    {
        const ImVec2 hexagon[] = { p(0.0f, -0.86f), p(0.78f, -0.43f), p(0.78f, 0.43f), p(0.0f, 0.86f), p(-0.78f, 0.43f), p(-0.78f, -0.43f) };
        drawList->AddPolyline(hexagon, 6, color, ImDrawFlags_Closed, stroke);
        drawList->AddLine(p(0.0f, 0.0f), p(-0.78f, -0.43f), color, stroke * 0.85f);
        drawList->AddLine(p(0.0f, 0.0f), p(0.78f, -0.43f), color, stroke * 0.85f);
        drawList->AddLine(p(0.0f, 0.0f), p(0.0f, 0.86f), color, stroke * 0.85f);
    }

    // Solid isometric cube, faces separated by cutout edges
    static void DrawMesh(ImDrawList* drawList, const GlyphFrame& p, ImU32 color, ImU32 cutoutColor, float stroke)
    {
        const ImVec2 hexagon[] = { p(0.0f, -0.86f), p(0.78f, -0.43f), p(0.78f, 0.43f), p(0.0f, 0.86f), p(-0.78f, 0.43f), p(-0.78f, -0.43f) };
        drawList->AddConvexPolyFilled(hexagon, 6, color);
        drawList->AddLine(p(0.0f, 0.0f), p(-0.78f, -0.43f), cutoutColor, stroke * 0.7f);
        drawList->AddLine(p(0.0f, 0.0f), p(0.78f, -0.43f), cutoutColor, stroke * 0.7f);
        drawList->AddLine(p(0.0f, 0.0f), p(0.0f, 0.86f), cutoutColor, stroke * 0.7f);
    }

    static void DrawSprite(ImDrawList* drawList, const GlyphFrame& p, ImU32 color, float stroke)
    {
        // Picture frame, two mountains and a sun
        drawList->AddRect(p(-0.84f, -0.70f), p(0.84f, 0.70f), color, p(0.14f), ImDrawFlags_None, stroke);
        drawList->AddTriangleFilled(p(-0.62f, 0.48f), p(-0.16f, -0.14f), p(0.30f, 0.48f), color);
        drawList->AddTriangleFilled(p(0.04f, 0.48f), p(0.36f, 0.06f), p(0.64f, 0.48f), color);
        drawList->AddCircleFilled(p(0.36f, -0.30f), p(0.15f), color, 12);
    }

    static void DrawTextGlyph(ImDrawList* drawList, const GlyphFrame& p, ImU32 color)
    {
        drawList->AddRectFilled(p(-0.72f, -0.80f), p(0.72f, -0.50f), color, p(0.06f));
        drawList->AddRectFilled(p(-0.16f, -0.80f), p(0.16f, 0.80f), color, p(0.06f));
        drawList->AddRectFilled(p(-0.38f, 0.56f), p(0.38f, 0.80f), color, p(0.06f));
    }

    static void DrawUICanvas(ImDrawList* drawList, const GlyphFrame& p, ImU32 color, float stroke)
    {
        // A window: title bar, a wide and a short widget
        drawList->AddRect(p(-0.86f, -0.74f), p(0.86f, 0.74f), color, p(0.14f), ImDrawFlags_None, stroke);
        drawList->AddRectFilled(p(-0.86f, -0.74f), p(0.86f, -0.36f), color, p(0.14f), ImDrawFlags_RoundCornersTop);
        drawList->AddRectFilled(p(-0.56f, -0.10f), p(0.56f, 0.14f), color, p(0.05f));
        drawList->AddRectFilled(p(-0.56f, 0.32f), p(0.10f, 0.50f), color, p(0.05f));
    }

    static void DrawScript(ImDrawList* drawList, const GlyphFrame& p, ImU32 color, float stroke)
    {
        // </>
        const ImVec2 left[] = { p(-0.34f, -0.52f), p(-0.84f, 0.0f), p(-0.34f, 0.52f) };
        const ImVec2 right[] = { p(0.34f, -0.52f), p(0.84f, 0.0f), p(0.34f, 0.52f) };
        drawList->AddPolyline(left, 3, color, ImDrawFlags_None, stroke);
        drawList->AddPolyline(right, 3, color, ImDrawFlags_None, stroke);
        drawList->AddLine(p(0.16f, -0.72f), p(-0.16f, 0.72f), color, stroke);
    }

    static void DrawTransform(ImDrawList* drawList, const GlyphFrame& p, ImU32 color, float stroke)
    {
        // Move cross: four arrows from the center
        drawList->AddLine(p(-0.62f, 0.0f), p(0.62f, 0.0f), color, stroke);
        drawList->AddLine(p(0.0f, -0.62f), p(0.0f, 0.62f), color, stroke);
        drawList->AddTriangleFilled(p(0.0f, -0.92f), p(0.26f, -0.56f), p(-0.26f, -0.56f), color);
        drawList->AddTriangleFilled(p(0.0f, 0.92f), p(-0.26f, 0.56f), p(0.26f, 0.56f), color);
        drawList->AddTriangleFilled(p(-0.92f, 0.0f), p(-0.56f, -0.26f), p(-0.56f, 0.26f), color);
        drawList->AddTriangleFilled(p(0.92f, 0.0f), p(0.56f, 0.26f), p(0.56f, -0.26f), color);
    }

    static void DrawRigidbody(ImDrawList* drawList, const GlyphFrame& p, ImU32 color, float stroke)
    {
        // A ball and its motion lines
        drawList->AddCircleFilled(p(0.30f, 0.0f), p(0.56f), color, 24);
        drawList->AddLine(p(-0.92f, -0.34f), p(-0.42f, -0.34f), color, stroke);
        drawList->AddLine(p(-0.92f, 0.0f), p(-0.36f, 0.0f), color, stroke);
        drawList->AddLine(p(-0.92f, 0.34f), p(-0.42f, 0.34f), color, stroke);
    }

    static void DrawCollider(ImDrawList* drawList, const GlyphFrame& p, ImU32 color, float stroke)
    {
        // Bounds corners around a shape
        const float c = 0.82f, l = 0.36f;
        const ImVec2 corners[4][3] = {
            { p(-c, -c + l), p(-c, -c), p(-c + l, -c) },
            { p(c - l, -c), p(c, -c), p(c, -c + l) },
            { p(c, c - l), p(c, c), p(c - l, c) },
            { p(-c + l, c), p(-c, c), p(-c, c - l) },
        };
        for(const auto& corner : corners)
            drawList->AddPolyline(corner, 3, color, ImDrawFlags_None, stroke);
        drawList->AddCircle(p(0.0f, 0.0f), p(0.36f), color, 20, stroke * 0.85f);
    }

    static void DrawLink(ImDrawList* drawList, const GlyphFrame& p, ImU32 color, float stroke)
    {
        // Two chain links
        drawList->AddRect(p(-0.92f, -0.32f), p(0.12f, 0.32f), color, p(0.32f), ImDrawFlags_None, stroke);
        drawList->AddRect(p(-0.12f, -0.32f), p(0.92f, 0.32f), color, p(0.32f), ImDrawFlags_None, stroke);
    }

    // Closed path through 'points' (glyph space) with every corner rounded: cut 'radius' back along both edges, joined by a curve through the corner
    static void PathRoundedPolygon(ImDrawList* drawList, const GlyphFrame& p, const ImVec2* points, int count, float radius)
    {
        auto towards = [radius](const ImVec2& from, const ImVec2& to) -> ImVec2 {
            const float dx = to.x - from.x, dy = to.y - from.y;
            const float length = glm::sqrt(dx * dx + dy * dy);
            const float t = glm::min(radius, length * 0.45f) / length; // Short edges never get cut past their middle
            return ImVec2(from.x + dx * t, from.y + dy * t);
        };

        for(int i = 0; i < count; i++)
        {
            const ImVec2& corner = points[i];
            const ImVec2 in = towards(corner, points[(i + count - 1) % count]);
            const ImVec2 out = towards(corner, points[(i + 1) % count]);
            drawList->PathLineTo(p(in.x, in.y));
            drawList->PathBezierQuadraticCurveTo(p(corner.x, corner.y), p(out.x, out.y), 6);
        }
    }

    static void DrawParticles(ImDrawList* drawList, const GlyphFrame& p, ImU32 color)
    {
        // A four point sparkle (two crossed diamonds) and two smaller sparks
        const ImVec2 vertical[] = { p(-0.22f, -0.86f), p(-0.06f, 0.06f), p(-0.22f, 0.86f), p(-0.38f, 0.06f) };
        const ImVec2 horizontal[] = { p(-0.92f, 0.06f), p(-0.22f, -0.12f), p(0.48f, 0.06f), p(-0.22f, 0.24f) };
        drawList->AddConvexPolyFilled(vertical, 4, color);
        drawList->AddConvexPolyFilled(horizontal, 4, color);
        drawList->AddCircleFilled(p(0.56f, -0.56f), p(0.17f), color, 12);
        drawList->AddCircleFilled(p(0.62f, 0.58f), p(0.12f), color, 10);
    }

    static void DrawPlay(ImDrawList* drawList, const GlyphFrame& p, ImU32 color)
    {
        // Nudged left so the triangle's mass sits in the middle, not its bounding box
        const ImVec2 points[] = { ImVec2(-0.62f, -0.84f), ImVec2(0.86f, 0.0f), ImVec2(-0.62f, 0.84f) };
        PathRoundedPolygon(drawList, p, points, 3, 0.22f);
        drawList->PathFillConvex(color);
    }

    static void DrawAdd(ImDrawList* drawList, const GlyphFrame& p, ImU32 color)
    {
        // A plus sign
        drawList->AddLine(p(-0.70f, 0.0f), p(0.70f, 0.0f), color, p(0.3f));
        drawList->AddLine(p(0.0f, -0.70f), p(0.0f, 0.70f), color, p(0.3f));
    }

    static void DrawStop(ImDrawList* drawList, const GlyphFrame& p, ImU32 color)
    {
        drawList->AddRectFilled(p(-0.70f, -0.70f), p(0.70f, 0.70f), color, p(0.22f));
    }

    static void DrawPause(ImDrawList* drawList, const GlyphFrame& p, ImU32 color)
    {
        drawList->AddRectFilled(p(-0.66f, -0.78f), p(-0.18f, 0.78f), color, p(0.16f));
        drawList->AddRectFilled(p(0.18f, -0.78f), p(0.66f, 0.78f), color, p(0.16f));
    }

    static void DrawStep(ImDrawList* drawList, const GlyphFrame& p, ImU32 color)
    {
        // Play triangle against a bar: one frame forward
        const ImVec2 points[] = { ImVec2(-0.82f, -0.80f), ImVec2(0.34f, 0.0f), ImVec2(-0.82f, 0.80f) };
        PathRoundedPolygon(drawList, p, points, 3, 0.18f);
        drawList->PathFillConvex(color);
        drawList->AddRectFilled(p(0.44f, -0.80f), p(0.82f, 0.80f), color, p(0.14f));
    }

    void DrawGlyph(ImDrawList* drawList, const ImVec2& center, float size, GizmoIcon icon, ImU32 color, ImU32 cutoutColor)
    {
        const GlyphFrame frame { center, size * 0.5f };
        const float stroke = glm::max(1.25f, frame.HalfSize * 0.15f);

        switch(icon)
        {
            case GizmoIcon::CAMERA: DrawCamera(drawList, frame, color, cutoutColor); break;
            case GizmoIcon::POINT_LIGHT: DrawPointLight(drawList, frame, color, cutoutColor, stroke); break;
            case GizmoIcon::DIRECTIONAL_LIGHT: DrawDirectionalLight(drawList, frame, color, stroke); break;
            case GizmoIcon::AUDIO_SOURCE: DrawAudioSource(drawList, frame, color, stroke); break;
            case GizmoIcon::AUDIO_LISTENER: DrawAudioListener(drawList, frame, color, stroke); break;
            case GizmoIcon::ENVIRONMENT: DrawEnvironment(drawList, frame, color, stroke); break;
            case GizmoIcon::ENTITY: DrawEntity(drawList, frame, color, stroke); break;
            case GizmoIcon::MESH: DrawMesh(drawList, frame, color, cutoutColor, stroke); break;
            case GizmoIcon::SPRITE: DrawSprite(drawList, frame, color, stroke); break;
            case GizmoIcon::TEXT: DrawTextGlyph(drawList, frame, color); break;
            case GizmoIcon::UI_CANVAS: DrawUICanvas(drawList, frame, color, stroke); break;
            case GizmoIcon::SCRIPT: DrawScript(drawList, frame, color, stroke); break;
            case GizmoIcon::PARTICLES: DrawParticles(drawList, frame, color); break;
            case GizmoIcon::TRANSFORM: DrawTransform(drawList, frame, color, stroke); break;
            case GizmoIcon::RIGIDBODY: DrawRigidbody(drawList, frame, color, stroke); break;
            case GizmoIcon::COLLIDER: DrawCollider(drawList, frame, color, stroke); break;
            case GizmoIcon::LINK: DrawLink(drawList, frame, color, stroke); break;
            case GizmoIcon::PLAY: DrawPlay(drawList, frame, color); break;
            case GizmoIcon::STOP: DrawStop(drawList, frame, color); break;
            case GizmoIcon::PAUSE: DrawPause(drawList, frame, color); break;
            case GizmoIcon::STEP: DrawStep(drawList, frame, color); break;
            case GizmoIcon::ADD: DrawAdd(drawList, frame, color); break;
        }
    }

    // Invisible button + its background from the current ImGuiCol_Button* colors, returns the background color (glyph cutouts use it)
    static ImU32 ButtonBehind(const char* strId, const ImVec2& size, bool& outPressed)
    {
        outPressed = ImGui::InvisibleButton(strId, size);
        const bool hovered = ImGui::IsItemHovered();
        const bool held = ImGui::IsItemActive();
        const ImU32 background = ImGui::GetColorU32(held && hovered ? ImGuiCol_ButtonActive : (hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
        ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), background, ImGui::GetStyle().FrameRounding);
        return background;
    }

    bool IconButton(const char* strId, GizmoIcon icon, const ImVec2& size, const ImVec4& glyphColor, float glyphScale)
    {
        bool pressed = false;
        const ImU32 background = ButtonBehind(strId, size, pressed);

        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
        DrawGlyph(ImGui::GetWindowDrawList(), center, glm::min(size.x, size.y) * glyphScale, icon, ImGui::GetColorU32(glyphColor), background);
        return pressed;
    }

    bool IconButton(const char* strId, ImTextureID texture, const ImVec2& size, const ImVec4& tint, float iconScale)
    {
        bool pressed = false;
        ButtonBehind(strId, size, pressed);

        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const float halfIcon = glm::floor(glm::min(size.x, size.y) * iconScale * 0.5f);
        const ImVec2 center(glm::floor((min.x + max.x) * 0.5f), glm::floor((min.y + max.y) * 0.5f)); // Whole pixels keep the downscaled icon sharp
        ImGui::GetWindowDrawList()->AddImage(texture, ImVec2(center.x - halfIcon, center.y - halfIcon), ImVec2(center.x + halfIcon, center.y + halfIcon), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), ImGui::GetColorU32(tint));
        return pressed;
    }

    EntityIcon GetEntityIcon(Entity e)
    {
        if(e.HasComponent<CameraComponent>())
            return { GizmoIcon::CAMERA, EditorUI::ToU32(EditorUI::Colors::Component::Camera) };
        if(const LightComponent* light = e.TryGetComponent<LightComponent>())
            return { light->Type == LightType::DIRECTIONAL ? GizmoIcon::DIRECTIONAL_LIGHT : GizmoIcon::POINT_LIGHT, EditorUI::ToU32(EditorUI::Colors::Component::Light) };
        if(e.HasComponent<EnvironmentComponent>())
            return { GizmoIcon::ENVIRONMENT, EditorUI::ToU32(EditorUI::Colors::Component::Environment) };
        if(e.HasComponent<ParticleSystemComponent>())
            return { GizmoIcon::PARTICLES, EditorUI::ToU32(EditorUI::Colors::Component::Particles) };
        if(e.HasComponent<AudioSourceComponent>())
            return { GizmoIcon::AUDIO_SOURCE, EditorUI::ToU32(EditorUI::Colors::Component::Audio) };
        if(e.HasComponent<AudioListenerComponent>())
            return { GizmoIcon::AUDIO_LISTENER, EditorUI::ToU32(EditorUI::Colors::Component::Audio) };
        if(e.HasComponent<UICanvasComponent>())
            return { GizmoIcon::UI_CANVAS, EditorUI::ToU32(EditorUI::Colors::Component::UICanvas) };
        if(e.HasComponent<TextComponent>())
            return { GizmoIcon::TEXT, EditorUI::ToU32(EditorUI::Colors::Component::Text) };
        if(e.HasComponent<SpriteRendererComponent>())
            return { GizmoIcon::SPRITE, EditorUI::ToU32(EditorUI::Colors::Component::Sprite) };
        if(e.HasComponent<MeshComponent>())
            return { GizmoIcon::MESH, EditorUI::ToU32(EditorUI::Colors::Component::Mesh) };
        return { GizmoIcon::ENTITY, EditorUI::ToU32(EditorUI::Colors::Component::Entity) };
    }

    void DrawBadge(ImDrawList* drawList, const ImVec2& center, float radius, GizmoIcon icon, ImU32 glyphColor, const BadgeStyle& style)
    {
        drawList->AddCircleFilled(ImVec2(center.x, center.y + 1.5f), radius + 1.0f, style.Shadow, 32);
        drawList->AddCircleFilled(center, radius, style.Background, 32);
        DrawGlyph(drawList, center, radius * 1.22f, icon, glyphColor, style.Background);
        drawList->AddCircle(center, radius - style.RimThickness * 0.5f, style.Rim, 32, style.RimThickness);
    }

} // namespace Surge::GizmoIcons
