// Copyright (c) - SurgeTechnologies - All rights reserved
#include "ParticleEffectFormat.hpp"
#include "Surge/Asset/AssetContext.hpp"
#include "Surge/Asset/Format/BinarySerialization.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Graphics/HighLevel/Texture2D.hpp"
#include "Surge/Graphics/Particles/ParticleEffect.hpp"

namespace Surge
{
    // Release builds of the Windows toolchain (no iterator debugging, so the std containers have their shipping size): a module struct that
    // changes size breaks the build here. Update the field lists below, bump ParticleEffectBinary::VERSION, then update the size
#if defined(SURGE_PLATFORM_WINDOWS) && defined(_ITERATOR_DEBUG_LEVEL) && _ITERATOR_DEBUG_LEVEL == 0
    static_assert(sizeof(ParticleEmitterDesc) == 8224, "ParticleEmitterDesc changed: update the ParticleEffectBinary field lists, bump VERSION, then this size");
#endif

    SURGE_RAPTOR_RAW(ParticleCurve::Key, 8);
    SURGE_RAPTOR_RAW(ParticleGradient::ColorKey, 16);
    SURGE_RAPTOR_RAW(ParticleGradient::AlphaKey, 8);
    SURGE_RAPTOR_RAW(ParticleCollisionPlane, 16);
    SURGE_RAPTOR_RAW(ParticleBurst, 24);

    // The first `count` entries of a fixed capacity array: Uint count + raw entries, a count above the capacity is corrupt data
    template <typename T, size_t N>
    static void WriteFixedArray(Rapt0R::Writer& writer, const std::array<T, N>& items, Uint count)
    {
        writer.WriteSpan(std::span<const T>(items.data(), std::min(count, static_cast<Uint>(N))));
    }

    template <typename T, size_t N>
    static bool ReadFixedArray(Rapt0R::Reader& reader, std::array<T, N>& items, Uint& outCount)
    {
        Uint count = 0;
        if(!reader.Read(count))
            return false;

        if(count > N)
        {
            reader.SetError(Rapt0R::ErrorCode::CORRUPT_DATA);
            return false;
        }
        if(!reader.ReadRawBytes(items.data(), count * sizeof(T)))
            return false;

        outCount = count;
        return true;
    }

    // Curves/gradients store their keys only, ParticleEffect::Bake() rebuilds the lookup tables after loading
    static void Rapt0RSerialize(Rapt0R::Writer& writer, const ParticleCurve& curve)
    {
        WriteFixedArray(writer, curve.Keys, curve.KeyCount);
    }

    static void Rapt0RDeserialize(Rapt0R::Reader& reader, ParticleCurve& curve)
    {
        if(ReadFixedArray(reader, curve.Keys, curve.KeyCount) && curve.KeyCount == 0)
            curve.SetConstant(1.0f);
    }

    static void Rapt0RSerialize(Rapt0R::Writer& writer, const ParticleGradient& gradient)
    {
        WriteFixedArray(writer, gradient.ColorKeys, gradient.ColorKeyCount);
        WriteFixedArray(writer, gradient.AlphaKeys, gradient.AlphaKeyCount);
    }

    static void Rapt0RDeserialize(Rapt0R::Reader& reader, ParticleGradient& gradient)
    {
        if(!ReadFixedArray(reader, gradient.ColorKeys, gradient.ColorKeyCount) || !ReadFixedArray(reader, gradient.AlphaKeys, gradient.AlphaKeyCount))
            return;

        // An empty key list keeps the defaults, like the JSON reader
        if(gradient.ColorKeyCount == 0 || gradient.AlphaKeyCount == 0)
        {
            const ParticleGradient defaults;
            if(gradient.ColorKeyCount == 0)
            {
                gradient.ColorKeys = defaults.ColorKeys;
                gradient.ColorKeyCount = defaults.ColorKeyCount;
            }
            if(gradient.AlphaKeyCount == 0)
            {
                gradient.AlphaKeys = defaults.AlphaKeys;
                gradient.AlphaKeyCount = defaults.AlphaKeyCount;
            }
        }
    }

    static void Rapt0RSerialize(Rapt0R::Writer& writer, const ParticleCollisionModule& m)
    {
        writer.Write(m.Enabled);
        WriteFixedArray(writer, m.Planes, m.PlaneCount);
        writer.Write(m.Bounce, m.Dampen, m.LifetimeLoss, m.RadiusScale);
    }

    static void Rapt0RDeserialize(Rapt0R::Reader& reader, ParticleCollisionModule& m)
    {
        if(reader.Read(m.Enabled) && ReadFixedArray(reader, m.Planes, m.PlaneCount))
            reader.Read(m.Bounce, m.Dampen, m.LifetimeLoss, m.RadiusScale);
    }

    SURGE_RAPTOR_FIELDS(ParticleFloat, v.Mode, v.Min, v.Max, v.CurveMultiplier, v.CurveMin, v.CurveMax)
    SURGE_RAPTOR_FIELDS(ParticleColor, v.Mode, v.ColorA, v.ColorB, v.Gradient)

    SURGE_RAPTOR_FIELDS(ParticleMainModule, v.Duration, v.Loop, v.Prewarm, v.StartDelay, v.StartLifetime, v.StartSpeed, v.StartSize, v.StartRotation, v.StartColor, v.GravityScale, v.SimulationSpace, v.MaxParticles)
    SURGE_RAPTOR_FIELDS(ParticleEmissionModule, v.Enabled, v.RateOverTime, v.RateOverDistance, v.Bursts)
    SURGE_RAPTOR_FIELDS(ParticleShapeModule, v.Enabled, v.Type, v.Radius, v.RadiusThickness, v.Angle, v.Arc, v.BoxSize, v.Position, v.Rotation, v.RandomizeDirection)
    SURGE_RAPTOR_FIELDS(ParticleVelocityModule, v.Enabled, v.Linear, v.Space, v.SpeedModifier)
    SURGE_RAPTOR_FIELDS(ParticleLimitVelocityModule, v.Enabled, v.SpeedLimit, v.Dampen, v.Drag)
    SURGE_RAPTOR_FIELDS(ParticleForceModule, v.Enabled, v.Force, v.Space)
    SURGE_RAPTOR_FIELDS(ParticleNoiseModule, v.Enabled, v.Strength, v.Frequency, v.ScrollSpeed)
    SURGE_RAPTOR_FIELDS(ParticleColorOverLifetimeModule, v.Enabled, v.Gradient)
    SURGE_RAPTOR_FIELDS(ParticleSizeOverLifetimeModule, v.Enabled, v.Size)
    SURGE_RAPTOR_FIELDS(ParticleRotationOverLifetimeModule, v.Enabled, v.AngularVelocity)
    SURGE_RAPTOR_FIELDS(ParticleTextureSheetModule, v.Enabled, v.Columns, v.Rows, v.Timing, v.Cycles, v.FPS, v.RandomStartFrame, v.FrameBlending)
    SURGE_RAPTOR_FIELDS(ParticleSubEmitter, v.Event, v.EmitterIndex, v.Count, v.Probability, v.InheritVelocity)
    SURGE_RAPTOR_FIELDS(ParticleSubEmitterModule, v.Enabled, v.Entries)

    // Every saved field but Texture: an asset, written by ID next to the emitter and loaded by ReadEmitter
    SURGE_RAPTOR_FIELDS(ParticleRendererModule, v.Mode, v.AdditiveAmount, v.EmissiveIntensity, v.Sort, v.SortingOrder, v.MaxScreenSize, v.StretchSpeedScale, v.StretchLengthScale)

    SURGE_RAPTOR_FIELDS(ParticleEmitterDesc, v.Name, v.Enabled, v.Main, v.Emission, v.Shape, v.Velocity, v.LimitVelocity, v.Force, v.Noise, v.ColorOverLifetime, v.SizeOverLifetime, v.RotationOverLifetime, v.TextureSheet, v.Collision, v.SubEmitters, v.Renderer)
}

namespace Surge::ParticleEffectBinary
{
    static Ref<Asset> LoadTexture(AssetManager& assetManager, uint64_t rawID)
    {
        if(rawID == AssetID::INVALID)
            return nullptr;

        const AssetID id(rawID);
        const AssetMetadata& metadata = assetManager.GetMetadata(id);
        if(!metadata.IsValid() || metadata.IsMissing() || metadata.Type != AssetType::TEXTURE2D)
        {
            Log<Severity::Warn>("[ParticleEffectBinary] Texture {} is missing or not a texture, using the default particle", rawID);
            return nullptr;
        }
        return assetManager.Load<Texture2D>(id);
    }

    static void WriteEmitter(Rapt0R::Writer& writer, const ParticleEmitterDesc& e)
    {
        writer.Write(e, e.Renderer.Texture ? e.Renderer.Texture->GetID().Get() : static_cast<uint64_t>(AssetID::INVALID));
    }

    // Every enum an emitter saves, against its last enumerator (update it when the enum grows)
    static bool HasValidEnums(const ParticleEmitterDesc& e)
    {
        auto ValidFloat = [](const ParticleFloat& value) { return IsEnumInRange(value.Mode, ParticleValueMode::RANDOM_BETWEEN_CURVES); };
        auto ValidSpace = [](ParticleSimulationSpace space) { return IsEnumInRange(space, ParticleSimulationSpace::WORLD); };

        bool valid = ValidFloat(e.Main.StartLifetime) && ValidFloat(e.Main.StartSpeed) && ValidFloat(e.Main.StartSize) && ValidFloat(e.Main.StartRotation) &&
                     IsEnumInRange(e.Main.StartColor.Mode, ParticleColorMode::RANDOM_IN_GRADIENT) && ValidSpace(e.Main.SimulationSpace) &&
                     ValidFloat(e.Emission.RateOverTime) && ValidFloat(e.Emission.RateOverDistance) &&
                     IsEnumInRange(e.Shape.Type, ParticleShapeType::EDGE) && ValidSpace(e.Velocity.Space) && ValidSpace(e.Force.Space) &&
                     ValidFloat(e.RotationOverLifetime.AngularVelocity) && IsEnumInRange(e.TextureSheet.Timing, ParticleSheetTiming::FPS) &&
                     IsEnumInRange(e.Renderer.Mode, ParticleRenderMode::VERTICAL) && IsEnumInRange(e.Renderer.Sort, ParticleSortMode::YOUNGEST_FIRST);
        for(const ParticleSubEmitter& entry : e.SubEmitters.Entries)
            valid = valid && IsEnumInRange(entry.Event, ParticleSubEmitterEvent::COLLISION);
        return valid;
    }

    static void ReadEmitter(AssetManager& assetManager, Rapt0R::Reader& reader, ParticleEmitterDesc& e)
    {
        uint64_t textureID = AssetID::INVALID;
        if(!reader.Read(e, textureID))
            return;

        if(!HasValidEnums(e))
        {
            reader.SetError(Rapt0R::ErrorCode::CORRUPT_DATA);
            return;
        }
        // Loaded last, a corrupted file never loads a texture from a garbage ID
        e.Renderer.Texture = LoadTexture(assetManager, textureID);
    }

    void Write(Rapt0R::Writer& writer, const ParticleEffect& effect)
    {
        const Uint emitterCount = std::min(static_cast<Uint>(effect.Emitters.size()), ParticleEffect::MAX_EMITTERS);
        writer.Write(emitterCount);
        for(Uint i = 0; i < emitterCount; i++)
            WriteEmitter(writer, effect.Emitters[i]);
    }

    bool Read(AssetManager& assetManager, Rapt0R::Reader& reader, ParticleEffect& outEffect)
    {
        Uint emitterCount = 0;
        if(!reader.Read(emitterCount) || emitterCount > ParticleEffect::MAX_EMITTERS)
            return false;

        Vector<ParticleEmitterDesc> emitters(emitterCount);
        for(Uint i = 0; i < emitterCount && !reader.HasError(); i++)
            ReadEmitter(assetManager, reader, emitters[i]);
        if(reader.HasError())
            return false;

        outEffect.Emitters = std::move(emitters);
        outEffect.MarkChanged(true);
        return true;
    }
}

namespace Surge
{
    Ref<Asset> ParticleEffectFormat::Read(CookedFile& file, const AssetMetadata& metadata, const AssetContext& context) const
    {
        Ref<ParticleEffect> effect = ParticleEffect::Create();
        Rapt0R::Reader payload = file.GetPayload();
        if(!ParticleEffectBinary::Read(*context.AssetManager_, payload, *effect))
        {
            Log<Severity::Error>("[ParticleEffectFormat] {} is truncated or corrupted!", metadata.RelativePath);
            return nullptr;
        }
        return effect.As<Asset>();
    }

    bool ParticleEffectFormat::Write(Rapt0R::Writer& payload, const Asset& asset) const
    {
        ParticleEffectBinary::Write(payload, static_cast<const ParticleEffect&>(asset));
        return !payload.HasError();
    }
}
