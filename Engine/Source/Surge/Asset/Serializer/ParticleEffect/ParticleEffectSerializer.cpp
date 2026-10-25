// Copyright (c) - SurgeTechnologies - All rights reserved
#include "ParticleEffectSerializer.hpp"
#include "ParticleEffectBinaryFormat.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Graphics/HighLevel/Texture2D.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include <json/json.hpp>

namespace Surge
{
    static constexpr int PARTICLE_EFFECT_VERSION = 1;

    using Json = nlohmann::json;

    // Enums are saved by name, so reordering an enum never breaks saved effects
    template<typename T, size_t N>
    static const char* EnumName(T value, const std::array<const char*, N>& names)
    {
        const size_t index = static_cast<size_t>(value);
        return index < N ? names[index] : names[0];
    }

    template<typename T, size_t N>
    static T EnumFromJson(const Json& j, const char* key, T fallback, const std::array<const char*, N>& names)
    {
        if(!j.contains(key) || !j[key].is_string())
            return fallback;

        const String value = j[key].get<String>();
        for(size_t i = 0; i < N; i++)
        {
            if(value == names[i])
                return static_cast<T>(i);
        }
        return fallback;
    }

    static constexpr std::array<const char*, 4> sValueModeNames = { "CONSTANT", "RANDOM_BETWEEN_CONSTANTS", "CURVE", "RANDOM_BETWEEN_CURVES" };
    static constexpr std::array<const char*, 4> sColorModeNames = { "CONSTANT", "RANDOM_BETWEEN_COLORS", "GRADIENT", "RANDOM_IN_GRADIENT" };
    static constexpr std::array<const char*, 2> sSpaceNames = { "LOCAL", "WORLD" };
    static constexpr std::array<const char*, 7> sShapeNames = { "POINT", "SPHERE", "HEMISPHERE", "CONE", "BOX", "CIRCLE", "EDGE" };
    static constexpr std::array<const char*, 2> sSheetTimingNames = { "LIFETIME", "FPS" };
    static constexpr std::array<const char*, 3> sSubEmitterEventNames = { "BIRTH", "DEATH", "COLLISION" };
    static constexpr std::array<const char*, 4> sRenderModeNames = { "BILLBOARD", "STRETCHED", "HORIZONTAL", "VERTICAL" };
    static constexpr std::array<const char*, 4> sSortModeNames = { "NONE", "DISTANCE", "OLDEST_FIRST", "YOUNGEST_FIRST" };

    static Json Vec3ToJson(const glm::vec3& v) { return Json::array({ v.x, v.y, v.z }); }
    static Json Vec4ToJson(const glm::vec4& v) { return Json::array({ v.x, v.y, v.z, v.w }); }

    static glm::vec3 JsonToVec3(const Json& j, const char* key, const glm::vec3& fallback)
    {
        if(!j.contains(key) || !j[key].is_array() || j[key].size() != 3)
            return fallback;
        return { j[key][0].get<float>(), j[key][1].get<float>(), j[key][2].get<float>() };
    }

    static glm::vec4 JsonToVec4(const Json& j, const char* key, const glm::vec4& fallback)
    {
        if(!j.contains(key) || !j[key].is_array() || j[key].size() != 4)
            return fallback;
        return { j[key][0].get<float>(), j[key][1].get<float>(), j[key][2].get<float>(), j[key][3].get<float>() };
    }

    template<typename T>
    static T Value(const Json& j, const char* key, T fallback)
    {
        if(!j.is_object() || !j.contains(key))
            return fallback;

        const Json& v = j[key];
        if constexpr(std::is_same_v<T, bool>)
            return v.is_boolean() ? v.get<bool>() : fallback;
        else if constexpr(std::is_same_v<T, String>)
            return v.is_string() ? v.get<String>() : fallback;
        else
            return v.is_number() ? v.get<T>() : fallback;
    }

    //////////////////////////////////////////////////////////////////////////
    // Curves, gradients, values
    //////////////////////////////////////////////////////////////////////////

    // "Curve": [[t, v], ...]
    static Json CurveToJson(const ParticleCurve& curve)
    {
        Json keys = Json::array();
        for(Uint i = 0; i < curve.KeyCount; i++)
            keys.push_back(Json::array({ curve.Keys[i].Time, curve.Keys[i].Value }));
        return keys;
    }

    static void CurveFromJson(const Json& j, const char* key, ParticleCurve& out)
    {
        if(!j.contains(key) || !j[key].is_array() || j[key].empty())
            return;

        out.KeyCount = 0;
        for(const Json& item : j[key])
        {
            if(item.is_array() && item.size() == 2 && out.KeyCount < ParticleCurve::MAX_KEYS)
                out.Keys[out.KeyCount++] = { glm::clamp(item[0].get<float>(), 0.0f, 1.0f), item[1].get<float>() };
        }
        if(out.KeyCount == 0)
            out.SetConstant(1.0f);
        out.SortKeys();
    }

    // { "Colors": [[t, r, g, b], ...], "Alphas": [[t, a], ...] }
    static Json GradientToJson(const ParticleGradient& gradient)
    {
        Json j;
        Json& colors = j["Colors"];
        colors = Json::array();
        for(Uint i = 0; i < gradient.ColorKeyCount; i++)
        {
            const ParticleGradient::ColorKey& key = gradient.ColorKeys[i];
            colors.push_back(Json::array({ key.Time, key.Color.r, key.Color.g, key.Color.b }));
        }

        Json& alphas = j["Alphas"];
        alphas = Json::array();
        for(Uint i = 0; i < gradient.AlphaKeyCount; i++)
            alphas.push_back(Json::array({ gradient.AlphaKeys[i].Time, gradient.AlphaKeys[i].Alpha }));
        return j;
    }

    static void GradientFromJson(const Json& j, const char* key, ParticleGradient& out)
    {
        if(!j.contains(key) || !j[key].is_object())
            return;

        const Json& g = j[key];
        if(g.contains("Colors") && g["Colors"].is_array() && !g["Colors"].empty())
        {
            out.ColorKeyCount = 0;
            for(const Json& item : g["Colors"])
            {
                if(item.is_array() && item.size() == 4 && out.ColorKeyCount < ParticleGradient::MAX_KEYS)
                    out.ColorKeys[out.ColorKeyCount++] = { glm::clamp(item[0].get<float>(), 0.0f, 1.0f), { item[1].get<float>(), item[2].get<float>(), item[3].get<float>() } };
            }
        }
        if(g.contains("Alphas") && g["Alphas"].is_array() && !g["Alphas"].empty())
        {
            out.AlphaKeyCount = 0;
            for(const Json& item : g["Alphas"])
            {
                if(item.is_array() && item.size() == 2 && out.AlphaKeyCount < ParticleGradient::MAX_KEYS)
                    out.AlphaKeys[out.AlphaKeyCount++] = { glm::clamp(item[0].get<float>(), 0.0f, 1.0f), item[1].get<float>() };
            }
        }
        if(out.ColorKeyCount == 0 || out.AlphaKeyCount == 0)
            out.SetConstant(glm::vec4(1.0f));
        out.SortKeys();
    }

    // Constants are written as plain numbers, everything else as an object
    static Json FloatToJson(const ParticleFloat& value)
    {
        if(value.Mode == ParticleValueMode::CONSTANT)
            return value.Max;

        Json j;
        j["Mode"] = EnumName(value.Mode, sValueModeNames);
        switch(value.Mode)
        {
            case ParticleValueMode::RANDOM_BETWEEN_CONSTANTS:
                j["Min"] = value.Min;
                j["Max"] = value.Max;
                break;
            case ParticleValueMode::RANDOM_BETWEEN_CURVES:
                j["CurveMin"] = CurveToJson(value.CurveMin);
                [[fallthrough]];
            case ParticleValueMode::CURVE:
                j["CurveMax"] = CurveToJson(value.CurveMax);
                j["Multiplier"] = value.CurveMultiplier;
                break;
            default: break;
        }
        return j;
    }

    static void FloatFromJson(const Json& j, const char* key, ParticleFloat& out)
    {
        if(!j.contains(key))
            return;

        const Json& v = j[key];
        if(v.is_number())
        {
            out.Mode = ParticleValueMode::CONSTANT;
            out.Min = out.Max = v.get<float>();
            return;
        }
        if(!v.is_object())
            return;

        out.Mode = EnumFromJson(v, "Mode", ParticleValueMode::CONSTANT, sValueModeNames);
        out.Min = Value(v, "Min", out.Min);
        out.Max = Value(v, "Max", out.Max);
        out.CurveMultiplier = Value(v, "Multiplier", out.CurveMultiplier);
        CurveFromJson(v, "CurveMin", out.CurveMin);
        CurveFromJson(v, "CurveMax", out.CurveMax);
    }

    static Json ColorToJson(const ParticleColor& color)
    {
        Json j;
        j["Mode"] = EnumName(color.Mode, sColorModeNames);
        switch(color.Mode)
        {
            case ParticleColorMode::CONSTANT:
                j["Color"] = Vec4ToJson(color.ColorA);
                break;
            case ParticleColorMode::RANDOM_BETWEEN_COLORS:
                j["Color"] = Vec4ToJson(color.ColorA);
                j["ColorB"] = Vec4ToJson(color.ColorB);
                break;
            default:
                j["Gradient"] = GradientToJson(color.Gradient);
                break;
        }
        return j;
    }

    static void ColorFromJson(const Json& j, const char* key, ParticleColor& out)
    {
        if(!j.contains(key) || !j[key].is_object())
            return;

        const Json& c = j[key];
        out.Mode = EnumFromJson(c, "Mode", ParticleColorMode::CONSTANT, sColorModeNames);
        out.ColorA = JsonToVec4(c, "Color", out.ColorA);
        out.ColorB = JsonToVec4(c, "ColorB", out.ColorB);
        GradientFromJson(c, "Gradient", out.Gradient);
    }

    //////////////////////////////////////////////////////////////////////////
    // Emitter
    //////////////////////////////////////////////////////////////////////////

    static Json EmitterToJson(const ParticleEmitterDesc& e)
    {
        Json j;
        j["Name"] = e.Name;
        j["Enabled"] = e.Enabled;

        {
            Json& m = j["Main"];
            m["Duration"] = e.Main.Duration;
            m["Loop"] = e.Main.Loop;
            m["Prewarm"] = e.Main.Prewarm;
            m["StartDelay"] = e.Main.StartDelay;
            m["StartLifetime"] = FloatToJson(e.Main.StartLifetime);
            m["StartSpeed"] = FloatToJson(e.Main.StartSpeed);
            m["StartSize"] = FloatToJson(e.Main.StartSize);
            m["StartRotation"] = FloatToJson(e.Main.StartRotation);
            m["StartColor"] = ColorToJson(e.Main.StartColor);
            m["GravityScale"] = e.Main.GravityScale;
            m["SimulationSpace"] = EnumName(e.Main.SimulationSpace, sSpaceNames);
            m["MaxParticles"] = e.Main.MaxParticles;
        }
        {
            Json& m = j["Emission"];
            m["Enabled"] = e.Emission.Enabled;
            m["RateOverTime"] = FloatToJson(e.Emission.RateOverTime);
            m["RateOverDistance"] = FloatToJson(e.Emission.RateOverDistance);
            Json& bursts = m["Bursts"];
            bursts = Json::array();
            for(const ParticleBurst& burst : e.Emission.Bursts)
            {
                Json b;
                b["Time"] = burst.Time;
                b["MinCount"] = burst.MinCount;
                b["MaxCount"] = burst.MaxCount;
                b["Cycles"] = burst.Cycles;
                b["Interval"] = burst.Interval;
                b["Probability"] = burst.Probability;
                bursts.push_back(std::move(b));
            }
        }
        {
            Json& m = j["Shape"];
            m["Enabled"] = e.Shape.Enabled;
            m["Type"] = EnumName(e.Shape.Type, sShapeNames);
            m["Radius"] = e.Shape.Radius;
            m["RadiusThickness"] = e.Shape.RadiusThickness;
            m["Angle"] = e.Shape.Angle;
            m["Arc"] = e.Shape.Arc;
            m["BoxSize"] = Vec3ToJson(e.Shape.BoxSize);
            m["Position"] = Vec3ToJson(e.Shape.Position);
            m["Rotation"] = Vec3ToJson(e.Shape.Rotation);
            m["RandomizeDirection"] = e.Shape.RandomizeDirection;
        }
        {
            Json& m = j["Velocity"];
            m["Enabled"] = e.Velocity.Enabled;
            m["Linear"] = Vec3ToJson(e.Velocity.Linear);
            m["Space"] = EnumName(e.Velocity.Space, sSpaceNames);
            m["SpeedModifier"] = CurveToJson(e.Velocity.SpeedModifier);
        }
        {
            Json& m = j["LimitVelocity"];
            m["Enabled"] = e.LimitVelocity.Enabled;
            m["SpeedLimit"] = e.LimitVelocity.SpeedLimit;
            m["Dampen"] = e.LimitVelocity.Dampen;
            m["Drag"] = e.LimitVelocity.Drag;
        }
        {
            Json& m = j["Force"];
            m["Enabled"] = e.Force.Enabled;
            m["Force"] = Vec3ToJson(e.Force.Force);
            m["Space"] = EnumName(e.Force.Space, sSpaceNames);
        }
        {
            Json& m = j["Noise"];
            m["Enabled"] = e.Noise.Enabled;
            m["Strength"] = e.Noise.Strength;
            m["Frequency"] = e.Noise.Frequency;
            m["ScrollSpeed"] = e.Noise.ScrollSpeed;
        }
        {
            Json& m = j["ColorOverLifetime"];
            m["Enabled"] = e.ColorOverLifetime.Enabled;
            m["Gradient"] = GradientToJson(e.ColorOverLifetime.Gradient);
        }
        {
            Json& m = j["SizeOverLifetime"];
            m["Enabled"] = e.SizeOverLifetime.Enabled;
            m["Size"] = CurveToJson(e.SizeOverLifetime.Size);
        }
        {
            Json& m = j["RotationOverLifetime"];
            m["Enabled"] = e.RotationOverLifetime.Enabled;
            m["AngularVelocity"] = FloatToJson(e.RotationOverLifetime.AngularVelocity);
        }
        {
            Json& m = j["TextureSheet"];
            m["Enabled"] = e.TextureSheet.Enabled;
            m["Columns"] = e.TextureSheet.Columns;
            m["Rows"] = e.TextureSheet.Rows;
            m["Timing"] = EnumName(e.TextureSheet.Timing, sSheetTimingNames);
            m["Cycles"] = e.TextureSheet.Cycles;
            m["FPS"] = e.TextureSheet.FPS;
            m["RandomStartFrame"] = e.TextureSheet.RandomStartFrame;
            m["FrameBlending"] = e.TextureSheet.FrameBlending;
        }
        {
            Json& m = j["Collision"];
            m["Enabled"] = e.Collision.Enabled;
            Json& planes = m["Planes"];
            planes = Json::array();
            for(Uint i = 0; i < e.Collision.PlaneCount; i++)
            {
                Json p;
                p["Normal"] = Vec3ToJson(e.Collision.Planes[i].Normal);
                p["Offset"] = e.Collision.Planes[i].Offset;
                planes.push_back(std::move(p));
            }
            m["Bounce"] = e.Collision.Bounce;
            m["Dampen"] = e.Collision.Dampen;
            m["LifetimeLoss"] = e.Collision.LifetimeLoss;
            m["RadiusScale"] = e.Collision.RadiusScale;
        }
        {
            Json& m = j["SubEmitters"];
            m["Enabled"] = e.SubEmitters.Enabled;
            Json& entries = m["Entries"];
            entries = Json::array();
            for(const ParticleSubEmitter& entry : e.SubEmitters.Entries)
            {
                Json s;
                s["Event"] = EnumName(entry.Event, sSubEmitterEventNames);
                s["Emitter"] = entry.EmitterIndex;
                s["Count"] = entry.Count;
                s["Probability"] = entry.Probability;
                s["InheritVelocity"] = entry.InheritVelocity;
                entries.push_back(std::move(s));
            }
        }
        {
            Json& m = j["Renderer"];
            m["Mode"] = EnumName(e.Renderer.Mode, sRenderModeNames);
            m["Texture"] = e.Renderer.Texture ? e.Renderer.Texture->GetID().Get() : static_cast<uint64_t>(AssetID::INVALID);
            m["AdditiveAmount"] = e.Renderer.AdditiveAmount;
            m["EmissiveIntensity"] = e.Renderer.EmissiveIntensity;
            m["Sort"] = EnumName(e.Renderer.Sort, sSortModeNames);
            m["SortingOrder"] = e.Renderer.SortingOrder;
            m["MaxScreenSize"] = e.Renderer.MaxScreenSize;
            m["StretchSpeedScale"] = e.Renderer.StretchSpeedScale;
            m["StretchLengthScale"] = e.Renderer.StretchLengthScale;
        }
        return j;
    }

    static Ref<Asset> LoadTexture(AssetManager& assetManager, uint64_t rawID)
    {
        if(rawID == AssetID::INVALID)
            return nullptr;

        const AssetID id(rawID);
        const AssetMetadata& metadata = assetManager.GetMetadata(id);
        if(!metadata.IsValid() || metadata.IsMissing() || metadata.Type != AssetType::TEXTURE2D)
        {
            Log<Severity::Warn>("[ParticleEffectSerializer] Texture {} is missing or not a texture, using the default particle", rawID);
            return nullptr;
        }
        return assetManager.Load<Texture2D>(id);
    }

    static void EmitterFromJson(AssetManager& assetManager, const Json& j, ParticleEmitterDesc& e)
    {
        e.Name = Value(j, "Name", e.Name);
        e.Enabled = Value(j, "Enabled", e.Enabled);

        if(j.contains("Main"))
        {
            const Json& m = j["Main"];
            e.Main.Duration = Value(m, "Duration", e.Main.Duration);
            e.Main.Loop = Value(m, "Loop", e.Main.Loop);
            e.Main.Prewarm = Value(m, "Prewarm", e.Main.Prewarm);
            e.Main.StartDelay = Value(m, "StartDelay", e.Main.StartDelay);
            FloatFromJson(m, "StartLifetime", e.Main.StartLifetime);
            FloatFromJson(m, "StartSpeed", e.Main.StartSpeed);
            FloatFromJson(m, "StartSize", e.Main.StartSize);
            FloatFromJson(m, "StartRotation", e.Main.StartRotation);
            ColorFromJson(m, "StartColor", e.Main.StartColor);
            e.Main.GravityScale = Value(m, "GravityScale", e.Main.GravityScale);
            e.Main.SimulationSpace = EnumFromJson(m, "SimulationSpace", e.Main.SimulationSpace, sSpaceNames);
            e.Main.MaxParticles = Value(m, "MaxParticles", e.Main.MaxParticles);
        }
        if(j.contains("Emission"))
        {
            const Json& m = j["Emission"];
            e.Emission.Enabled = Value(m, "Enabled", e.Emission.Enabled);
            FloatFromJson(m, "RateOverTime", e.Emission.RateOverTime);
            FloatFromJson(m, "RateOverDistance", e.Emission.RateOverDistance);
            e.Emission.Bursts.clear();
            if(m.contains("Bursts") && m["Bursts"].is_array())
            {
                for(const Json& b : m["Bursts"])
                {
                    ParticleBurst& burst = e.Emission.Bursts.emplace_back();
                    burst.Time = Value(b, "Time", burst.Time);
                    burst.MinCount = Value(b, "MinCount", burst.MinCount);
                    burst.MaxCount = Value(b, "MaxCount", burst.MaxCount);
                    burst.Cycles = Value(b, "Cycles", burst.Cycles);
                    burst.Interval = Value(b, "Interval", burst.Interval);
                    burst.Probability = Value(b, "Probability", burst.Probability);
                }
            }
        }
        if(j.contains("Shape"))
        {
            const Json& m = j["Shape"];
            e.Shape.Enabled = Value(m, "Enabled", e.Shape.Enabled);
            e.Shape.Type = EnumFromJson(m, "Type", e.Shape.Type, sShapeNames);
            e.Shape.Radius = Value(m, "Radius", e.Shape.Radius);
            e.Shape.RadiusThickness = Value(m, "RadiusThickness", e.Shape.RadiusThickness);
            e.Shape.Angle = Value(m, "Angle", e.Shape.Angle);
            e.Shape.Arc = Value(m, "Arc", e.Shape.Arc);
            e.Shape.BoxSize = JsonToVec3(m, "BoxSize", e.Shape.BoxSize);
            e.Shape.Position = JsonToVec3(m, "Position", e.Shape.Position);
            e.Shape.Rotation = JsonToVec3(m, "Rotation", e.Shape.Rotation);
            e.Shape.RandomizeDirection = Value(m, "RandomizeDirection", e.Shape.RandomizeDirection);
        }
        if(j.contains("Velocity"))
        {
            const Json& m = j["Velocity"];
            e.Velocity.Enabled = Value(m, "Enabled", e.Velocity.Enabled);
            e.Velocity.Linear = JsonToVec3(m, "Linear", e.Velocity.Linear);
            e.Velocity.Space = EnumFromJson(m, "Space", e.Velocity.Space, sSpaceNames);
            CurveFromJson(m, "SpeedModifier", e.Velocity.SpeedModifier);
        }
        if(j.contains("LimitVelocity"))
        {
            const Json& m = j["LimitVelocity"];
            e.LimitVelocity.Enabled = Value(m, "Enabled", e.LimitVelocity.Enabled);
            e.LimitVelocity.SpeedLimit = Value(m, "SpeedLimit", e.LimitVelocity.SpeedLimit);
            e.LimitVelocity.Dampen = Value(m, "Dampen", e.LimitVelocity.Dampen);
            e.LimitVelocity.Drag = Value(m, "Drag", e.LimitVelocity.Drag);
        }
        if(j.contains("Force"))
        {
            const Json& m = j["Force"];
            e.Force.Enabled = Value(m, "Enabled", e.Force.Enabled);
            e.Force.Force = JsonToVec3(m, "Force", e.Force.Force);
            e.Force.Space = EnumFromJson(m, "Space", e.Force.Space, sSpaceNames);
        }
        if(j.contains("Noise"))
        {
            const Json& m = j["Noise"];
            e.Noise.Enabled = Value(m, "Enabled", e.Noise.Enabled);
            e.Noise.Strength = Value(m, "Strength", e.Noise.Strength);
            e.Noise.Frequency = Value(m, "Frequency", e.Noise.Frequency);
            e.Noise.ScrollSpeed = Value(m, "ScrollSpeed", e.Noise.ScrollSpeed);
        }
        if(j.contains("ColorOverLifetime"))
        {
            const Json& m = j["ColorOverLifetime"];
            e.ColorOverLifetime.Enabled = Value(m, "Enabled", e.ColorOverLifetime.Enabled);
            GradientFromJson(m, "Gradient", e.ColorOverLifetime.Gradient);
        }
        if(j.contains("SizeOverLifetime"))
        {
            const Json& m = j["SizeOverLifetime"];
            e.SizeOverLifetime.Enabled = Value(m, "Enabled", e.SizeOverLifetime.Enabled);
            CurveFromJson(m, "Size", e.SizeOverLifetime.Size);
        }
        if(j.contains("RotationOverLifetime"))
        {
            const Json& m = j["RotationOverLifetime"];
            e.RotationOverLifetime.Enabled = Value(m, "Enabled", e.RotationOverLifetime.Enabled);
            FloatFromJson(m, "AngularVelocity", e.RotationOverLifetime.AngularVelocity);
        }
        if(j.contains("TextureSheet"))
        {
            const Json& m = j["TextureSheet"];
            e.TextureSheet.Enabled = Value(m, "Enabled", e.TextureSheet.Enabled);
            e.TextureSheet.Columns = Value(m, "Columns", e.TextureSheet.Columns);
            e.TextureSheet.Rows = Value(m, "Rows", e.TextureSheet.Rows);
            e.TextureSheet.Timing = EnumFromJson(m, "Timing", e.TextureSheet.Timing, sSheetTimingNames);
            e.TextureSheet.Cycles = Value(m, "Cycles", e.TextureSheet.Cycles);
            e.TextureSheet.FPS = Value(m, "FPS", e.TextureSheet.FPS);
            e.TextureSheet.RandomStartFrame = Value(m, "RandomStartFrame", e.TextureSheet.RandomStartFrame);
            e.TextureSheet.FrameBlending = Value(m, "FrameBlending", e.TextureSheet.FrameBlending);
        }
        if(j.contains("Collision"))
        {
            const Json& m = j["Collision"];
            e.Collision.Enabled = Value(m, "Enabled", e.Collision.Enabled);
            if(m.contains("Planes") && m["Planes"].is_array())
            {
                e.Collision.PlaneCount = 0;
                for(const Json& p : m["Planes"])
                {
                    if(e.Collision.PlaneCount >= ParticleCollisionModule::MAX_PLANES)
                        break;
                    ParticleCollisionPlane& plane = e.Collision.Planes[e.Collision.PlaneCount++];
                    plane.Normal = JsonToVec3(p, "Normal", plane.Normal);
                    plane.Offset = Value(p, "Offset", plane.Offset);
                }
            }
            e.Collision.Bounce = Value(m, "Bounce", e.Collision.Bounce);
            e.Collision.Dampen = Value(m, "Dampen", e.Collision.Dampen);
            e.Collision.LifetimeLoss = Value(m, "LifetimeLoss", e.Collision.LifetimeLoss);
            e.Collision.RadiusScale = Value(m, "RadiusScale", e.Collision.RadiusScale);
        }
        if(j.contains("SubEmitters"))
        {
            const Json& m = j["SubEmitters"];
            e.SubEmitters.Enabled = Value(m, "Enabled", e.SubEmitters.Enabled);
            e.SubEmitters.Entries.clear();
            if(m.contains("Entries") && m["Entries"].is_array())
            {
                for(const Json& s : m["Entries"])
                {
                    ParticleSubEmitter& entry = e.SubEmitters.Entries.emplace_back();
                    entry.Event = EnumFromJson(s, "Event", entry.Event, sSubEmitterEventNames);
                    entry.EmitterIndex = Value(s, "Emitter", entry.EmitterIndex);
                    entry.Count = Value(s, "Count", entry.Count);
                    entry.Probability = Value(s, "Probability", entry.Probability);
                    entry.InheritVelocity = Value(s, "InheritVelocity", entry.InheritVelocity);
                }
            }
        }
        if(j.contains("Renderer"))
        {
            const Json& m = j["Renderer"];
            e.Renderer.Mode = EnumFromJson(m, "Mode", e.Renderer.Mode, sRenderModeNames);
            e.Renderer.Texture = LoadTexture(assetManager, Value(m, "Texture", static_cast<uint64_t>(AssetID::INVALID)));
            e.Renderer.AdditiveAmount = Value(m, "AdditiveAmount", e.Renderer.AdditiveAmount);
            e.Renderer.EmissiveIntensity = Value(m, "EmissiveIntensity", e.Renderer.EmissiveIntensity);
            e.Renderer.Sort = EnumFromJson(m, "Sort", e.Renderer.Sort, sSortModeNames);
            e.Renderer.SortingOrder = Value(m, "SortingOrder", e.Renderer.SortingOrder);
            e.Renderer.MaxScreenSize = Value(m, "MaxScreenSize", e.Renderer.MaxScreenSize);
            e.Renderer.StretchSpeedScale = Value(m, "StretchSpeedScale", e.Renderer.StretchSpeedScale);
            e.Renderer.StretchLengthScale = Value(m, "StretchLengthScale", e.Renderer.StretchLengthScale);
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // ParticleEffectSerializer
    //////////////////////////////////////////////////////////////////////////

    String ParticleEffectSerializer::ToJson(const ParticleEffect& effect)
    {
        Json j;
        j["Version"] = PARTICLE_EFFECT_VERSION;
        Json& emitters = j["Emitters"];
        emitters = Json::array();
        for(const ParticleEmitterDesc& emitter : effect.Emitters)
            emitters.push_back(EmitterToJson(emitter));
        return j.dump(4);
    }

    bool ParticleEffectSerializer::FromJson(AssetManager& assetManager, const String& json, ParticleEffect& outEffect)
    {
        const Json j = Json::parse(json, nullptr, false);
        if(j.is_discarded() || !j.is_object() || !j.contains("Emitters") || !j["Emitters"].is_array())
            return false;

        outEffect.Emitters.clear();
        for(const Json& item : j["Emitters"])
        {
            if(outEffect.Emitters.size() >= ParticleEffect::MAX_EMITTERS)
                break;
            if(item.is_object())
                EmitterFromJson(assetManager, item, outEffect.Emitters.emplace_back());
        }

        outEffect.MarkChanged(true);
        return true;
    }

    void ParticleEffectSerializer::Initialize()
    {
        mSerializerType = AssetType::PARTICLE_EFFECT;
    }

    bool ParticleEffectSerializer::Serialize([[maybe_unused]] Ref<Asset> asset) const
    {
#ifdef SURGE_PLATFORM_ANDROID
        Log<Severity::Error>("[ParticleEffectSerializer] Serialize is unsupported on Android runtime. APK assets are readonly!");
        return false;
#else
        AssetManager* am = mContext->AssetManager_;
        const AssetMetadata& metadata = am->GetMetadata(asset->GetID());
        const String absolutePath = am->GetAbsolutePath(metadata.RelativePath);
        const ParticleEffect& effect = *asset.As<ParticleEffect>();

        // JSON source first: the cooked file is stamped with its new modification time, so the next Load does not re-cook it
        if(!Filesystem::WriteTextFile(absolutePath, ToJson(effect)))
        {
            Log<Severity::Error>("[ParticleEffectSerializer] Failed to write {}", absolutePath);
            return false;
        }

        const AssetStamp stamp = AssetStampWriter::Build(absolutePath, ParticleEffectBinary::VERSION);
        return ParticleEffectBinary::Write(am->GetSidecarPath(metadata.ID), stamp, effect);
#endif
    }

    Ref<Asset> ParticleEffectSerializer::Deserialize(const AssetMetadata& metadata) const
    {
        AssetManager* am = mContext->AssetManager_;

        Ref<ParticleEffect> effect = ParticleEffect::Create();
        AssetStamp stamp;
        if(!ParticleEffectBinary::Read(*am, am->GetSidecarPath(metadata.ID), stamp, *effect))
            return nullptr;
        return effect;
    }

    void ParticleEffectSerializer::Shutdown()
    {
        Log<Severity::Info>("[ParticleEffectSerializer] Shutdown");
    }
}
