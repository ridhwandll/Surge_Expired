// Copyright (c) - SurgeTechnologies - All rights reserved
#include "ParticleEffectBinaryFormat.hpp"
#include "Surge/Asset/Serializer/BinaryHelpers.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Graphics/HighLevel/Texture2D.hpp"
#include "Surge/Graphics/Particles/ParticleEffect.hpp"
#include "Surge/Utility/Filesystem.hpp"

namespace Surge::ParticleEffectBinary
{
    static constexpr Uint MAGIC = 0x50444952; // RIDP

    struct Header
    {
        Uint Magic;
        Uint Version;
        Uint EmitterCount;
    };

    // Release builds of the Windows toolchain (no iterator debugging, so the std containers have their shipping size): a module struct that
    // changes size breaks the build here. Update Write/Read below, bump VERSION, then update the size
#if defined(SURGE_PLATFORM_WINDOWS) && defined(_ITERATOR_DEBUG_LEVEL) && _ITERATOR_DEBUG_LEVEL == 0
    static_assert(sizeof(ParticleEmitterDesc) == 8224, "ParticleEmitterDesc changed: update ParticleEffectBinary Write/Read, bump VERSION, then this size");
#endif

    static_assert(std::is_trivially_copyable_v<ParticleCurve::Key>);
    static_assert(std::is_trivially_copyable_v<ParticleGradient::ColorKey>);
    static_assert(std::is_trivially_copyable_v<ParticleGradient::AlphaKey>);

    //////////////////////////////////////////////////////////////////////////
    // Values
    //////////////////////////////////////////////////////////////////////////

    template <typename E>
    static void WriteEnum(Vector<Byte>& buffer, E value)
    {
        WriteData(buffer, static_cast<int32_t>(value));
    }

    template <typename E>
    static E ReadEnum(BinaryReader& reader)
    {
        int32_t value = 0;
        reader.Read(value);
        return static_cast<E>(value);
    }

    static void WriteBool(Vector<Byte>& buffer, bool value)
    {
        WriteData(buffer, static_cast<Byte>(value ? 1 : 0));
    }

    static bool ReadBool(BinaryReader& reader)
    {
        Byte value = 0;
        reader.Read(value);
        return value != 0;
    }

    static void WriteCurve(Vector<Byte>& buffer, const ParticleCurve& curve)
    {
        const Uint count = std::min(curve.KeyCount, ParticleCurve::MAX_KEYS);
        WriteData(buffer, count);
        WriteDataArray(buffer, curve.Keys.data(), count);
    }

    static void ReadCurve(BinaryReader& reader, ParticleCurve& curve)
    {
        Uint count = 0;
        reader.Read(count);
        if(count > ParticleCurve::MAX_KEYS)
        {
            reader.Failed = true;
            return;
        }

        curve.KeyCount = count;
        reader.ReadArray(curve.Keys.data(), count);
        if(count == 0)
            curve.SetConstant(1.0f);
    }

    static void WriteGradient(Vector<Byte>& buffer, const ParticleGradient& gradient)
    {
        const Uint colorCount = std::min(gradient.ColorKeyCount, ParticleGradient::MAX_KEYS);
        const Uint alphaCount = std::min(gradient.AlphaKeyCount, ParticleGradient::MAX_KEYS);
        WriteData(buffer, colorCount);
        WriteDataArray(buffer, gradient.ColorKeys.data(), colorCount);
        WriteData(buffer, alphaCount);
        WriteDataArray(buffer, gradient.AlphaKeys.data(), alphaCount);
    }

    static void ReadGradient(BinaryReader& reader, ParticleGradient& gradient)
    {
        const ParticleGradient defaults;

        Uint colorCount = 0;
        reader.Read(colorCount);
        if(colorCount > ParticleGradient::MAX_KEYS)
        {
            reader.Failed = true;
            return;
        }
        gradient.ColorKeyCount = colorCount;
        reader.ReadArray(gradient.ColorKeys.data(), colorCount);

        Uint alphaCount = 0;
        reader.Read(alphaCount);
        if(alphaCount > ParticleGradient::MAX_KEYS)
        {
            reader.Failed = true;
            return;
        }
        gradient.AlphaKeyCount = alphaCount;
        reader.ReadArray(gradient.AlphaKeys.data(), alphaCount);

        // An empty key list keeps the defaults, like the JSON reader
        if(colorCount == 0)
        {
            gradient.ColorKeys = defaults.ColorKeys;
            gradient.ColorKeyCount = defaults.ColorKeyCount;
        }
        if(alphaCount == 0)
        {
            gradient.AlphaKeys = defaults.AlphaKeys;
            gradient.AlphaKeyCount = defaults.AlphaKeyCount;
        }
    }

    static void WriteFloat(Vector<Byte>& buffer, const ParticleFloat& value)
    {
        WriteEnum(buffer, value.Mode);
        WriteData(buffer, value.Min);
        WriteData(buffer, value.Max);
        WriteData(buffer, value.CurveMultiplier);
        WriteCurve(buffer, value.CurveMin);
        WriteCurve(buffer, value.CurveMax);
    }

    static void ReadFloat(BinaryReader& reader, ParticleFloat& value)
    {
        value.Mode = ReadEnum<ParticleValueMode>(reader);
        reader.Read(value.Min);
        reader.Read(value.Max);
        reader.Read(value.CurveMultiplier);
        ReadCurve(reader, value.CurveMin);
        ReadCurve(reader, value.CurveMax);
    }

    static void WriteColor(Vector<Byte>& buffer, const ParticleColor& color)
    {
        WriteEnum(buffer, color.Mode);
        WriteData(buffer, color.ColorA);
        WriteData(buffer, color.ColorB);
        WriteGradient(buffer, color.Gradient);
    }

    static void ReadColor(BinaryReader& reader, ParticleColor& color)
    {
        color.Mode = ReadEnum<ParticleColorMode>(reader);
        reader.Read(color.ColorA);
        reader.Read(color.ColorB);
        ReadGradient(reader, color.Gradient);
    }

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

    //////////////////////////////////////////////////////////////////////////
    // Emitters
    //////////////////////////////////////////////////////////////////////////

    static void WriteEmitter(Vector<Byte>& buffer, const ParticleEmitterDesc& e)
    {
        WriteStr(buffer, e.Name);
        WriteBool(buffer, e.Enabled);

        // Main
        WriteData(buffer, e.Main.Duration);
        WriteBool(buffer, e.Main.Loop);
        WriteBool(buffer, e.Main.Prewarm);
        WriteData(buffer, e.Main.StartDelay);
        WriteFloat(buffer, e.Main.StartLifetime);
        WriteFloat(buffer, e.Main.StartSpeed);
        WriteFloat(buffer, e.Main.StartSize);
        WriteFloat(buffer, e.Main.StartRotation);
        WriteColor(buffer, e.Main.StartColor);
        WriteData(buffer, e.Main.GravityScale);
        WriteEnum(buffer, e.Main.SimulationSpace);
        WriteData(buffer, e.Main.MaxParticles);

        // Emission
        WriteBool(buffer, e.Emission.Enabled);
        WriteFloat(buffer, e.Emission.RateOverTime);
        WriteFloat(buffer, e.Emission.RateOverDistance);
        WriteData(buffer, static_cast<Uint>(e.Emission.Bursts.size()));
        for(const ParticleBurst& burst : e.Emission.Bursts)
        {
            WriteData(buffer, burst.Time);
            WriteData(buffer, burst.MinCount);
            WriteData(buffer, burst.MaxCount);
            WriteData(buffer, static_cast<int32_t>(burst.Cycles));
            WriteData(buffer, burst.Interval);
            WriteData(buffer, burst.Probability);
        }

        // Shape
        WriteBool(buffer, e.Shape.Enabled);
        WriteEnum(buffer, e.Shape.Type);
        WriteData(buffer, e.Shape.Radius);
        WriteData(buffer, e.Shape.RadiusThickness);
        WriteData(buffer, e.Shape.Angle);
        WriteData(buffer, e.Shape.Arc);
        WriteData(buffer, e.Shape.BoxSize);
        WriteData(buffer, e.Shape.Position);
        WriteData(buffer, e.Shape.Rotation);
        WriteData(buffer, e.Shape.RandomizeDirection);

        // Velocity
        WriteBool(buffer, e.Velocity.Enabled);
        WriteData(buffer, e.Velocity.Linear);
        WriteEnum(buffer, e.Velocity.Space);
        WriteCurve(buffer, e.Velocity.SpeedModifier);

        // LimitVelocity
        WriteBool(buffer, e.LimitVelocity.Enabled);
        WriteData(buffer, e.LimitVelocity.SpeedLimit);
        WriteData(buffer, e.LimitVelocity.Dampen);
        WriteData(buffer, e.LimitVelocity.Drag);

        // Force
        WriteBool(buffer, e.Force.Enabled);
        WriteData(buffer, e.Force.Force);
        WriteEnum(buffer, e.Force.Space);

        // Noise
        WriteBool(buffer, e.Noise.Enabled);
        WriteData(buffer, e.Noise.Strength);
        WriteData(buffer, e.Noise.Frequency);
        WriteData(buffer, e.Noise.ScrollSpeed);

        // ColorOverLifetime, SizeOverLifetime, RotationOverLifetime
        WriteBool(buffer, e.ColorOverLifetime.Enabled);
        WriteGradient(buffer, e.ColorOverLifetime.Gradient);
        WriteBool(buffer, e.SizeOverLifetime.Enabled);
        WriteCurve(buffer, e.SizeOverLifetime.Size);
        WriteBool(buffer, e.RotationOverLifetime.Enabled);
        WriteFloat(buffer, e.RotationOverLifetime.AngularVelocity);

        // TextureSheet
        WriteBool(buffer, e.TextureSheet.Enabled);
        WriteData(buffer, e.TextureSheet.Columns);
        WriteData(buffer, e.TextureSheet.Rows);
        WriteEnum(buffer, e.TextureSheet.Timing);
        WriteData(buffer, e.TextureSheet.Cycles);
        WriteData(buffer, e.TextureSheet.FPS);
        WriteBool(buffer, e.TextureSheet.RandomStartFrame);
        WriteBool(buffer, e.TextureSheet.FrameBlending);

        // Collision
        const Uint planeCount = std::min(e.Collision.PlaneCount, ParticleCollisionModule::MAX_PLANES);
        WriteBool(buffer, e.Collision.Enabled);
        WriteData(buffer, planeCount);
        for(Uint i = 0; i < planeCount; i++)
        {
            WriteData(buffer, e.Collision.Planes[i].Normal);
            WriteData(buffer, e.Collision.Planes[i].Offset);
        }
        WriteData(buffer, e.Collision.Bounce);
        WriteData(buffer, e.Collision.Dampen);
        WriteData(buffer, e.Collision.LifetimeLoss);
        WriteData(buffer, e.Collision.RadiusScale);

        // SubEmitters
        WriteBool(buffer, e.SubEmitters.Enabled);
        WriteData(buffer, static_cast<Uint>(e.SubEmitters.Entries.size()));
        for(const ParticleSubEmitter& entry : e.SubEmitters.Entries)
        {
            WriteEnum(buffer, entry.Event);
            WriteData(buffer, static_cast<int32_t>(entry.EmitterIndex));
            WriteData(buffer, entry.Count);
            WriteData(buffer, entry.Probability);
            WriteData(buffer, entry.InheritVelocity);
        }

        // Renderer
        WriteEnum(buffer, e.Renderer.Mode);
        WriteData(buffer, e.Renderer.Texture ? e.Renderer.Texture->GetID().Get() : static_cast<uint64_t>(AssetID::INVALID));
        WriteData(buffer, e.Renderer.AdditiveAmount);
        WriteData(buffer, e.Renderer.EmissiveIntensity);
        WriteEnum(buffer, e.Renderer.Sort);
        WriteData(buffer, static_cast<int32_t>(e.Renderer.SortingOrder));
        WriteData(buffer, e.Renderer.MaxScreenSize);
        WriteData(buffer, e.Renderer.StretchSpeedScale);
        WriteData(buffer, e.Renderer.StretchLengthScale);
    }

    static int ReadInt(BinaryReader& reader)
    {
        int32_t value = 0;
        reader.Read(value);
        return value;
    }

    static void ReadEmitter(AssetManager& assetManager, BinaryReader& reader, ParticleEmitterDesc& e)
    {
        e.Name = reader.ReadStr();
        e.Enabled = ReadBool(reader);

        // Main
        reader.Read(e.Main.Duration);
        e.Main.Loop = ReadBool(reader);
        e.Main.Prewarm = ReadBool(reader);
        reader.Read(e.Main.StartDelay);
        ReadFloat(reader, e.Main.StartLifetime);
        ReadFloat(reader, e.Main.StartSpeed);
        ReadFloat(reader, e.Main.StartSize);
        ReadFloat(reader, e.Main.StartRotation);
        ReadColor(reader, e.Main.StartColor);
        reader.Read(e.Main.GravityScale);
        e.Main.SimulationSpace = ReadEnum<ParticleSimulationSpace>(reader);
        reader.Read(e.Main.MaxParticles);

        // Emission
        e.Emission.Enabled = ReadBool(reader);
        ReadFloat(reader, e.Emission.RateOverTime);
        ReadFloat(reader, e.Emission.RateOverDistance);
        Uint burstCount = 0;
        reader.Read(burstCount);
        if(!reader.Fits(burstCount, 1))
        {
            reader.Failed = true;
            return;
        }
        e.Emission.Bursts.resize(burstCount);
        for(ParticleBurst& burst : e.Emission.Bursts)
        {
            reader.Read(burst.Time);
            reader.Read(burst.MinCount);
            reader.Read(burst.MaxCount);
            burst.Cycles = ReadInt(reader);
            reader.Read(burst.Interval);
            reader.Read(burst.Probability);
        }

        // Shape
        e.Shape.Enabled = ReadBool(reader);
        e.Shape.Type = ReadEnum<ParticleShapeType>(reader);
        reader.Read(e.Shape.Radius);
        reader.Read(e.Shape.RadiusThickness);
        reader.Read(e.Shape.Angle);
        reader.Read(e.Shape.Arc);
        reader.Read(e.Shape.BoxSize);
        reader.Read(e.Shape.Position);
        reader.Read(e.Shape.Rotation);
        reader.Read(e.Shape.RandomizeDirection);

        // Velocity
        e.Velocity.Enabled = ReadBool(reader);
        reader.Read(e.Velocity.Linear);
        e.Velocity.Space = ReadEnum<ParticleSimulationSpace>(reader);
        ReadCurve(reader, e.Velocity.SpeedModifier);

        // LimitVelocity
        e.LimitVelocity.Enabled = ReadBool(reader);
        reader.Read(e.LimitVelocity.SpeedLimit);
        reader.Read(e.LimitVelocity.Dampen);
        reader.Read(e.LimitVelocity.Drag);

        // Force
        e.Force.Enabled = ReadBool(reader);
        reader.Read(e.Force.Force);
        e.Force.Space = ReadEnum<ParticleSimulationSpace>(reader);

        // Noise
        e.Noise.Enabled = ReadBool(reader);
        reader.Read(e.Noise.Strength);
        reader.Read(e.Noise.Frequency);
        reader.Read(e.Noise.ScrollSpeed);

        // ColorOverLifetime, SizeOverLifetime, RotationOverLifetime
        e.ColorOverLifetime.Enabled = ReadBool(reader);
        ReadGradient(reader, e.ColorOverLifetime.Gradient);
        e.SizeOverLifetime.Enabled = ReadBool(reader);
        ReadCurve(reader, e.SizeOverLifetime.Size);
        e.RotationOverLifetime.Enabled = ReadBool(reader);
        ReadFloat(reader, e.RotationOverLifetime.AngularVelocity);

        // TextureSheet
        e.TextureSheet.Enabled = ReadBool(reader);
        reader.Read(e.TextureSheet.Columns);
        reader.Read(e.TextureSheet.Rows);
        e.TextureSheet.Timing = ReadEnum<ParticleSheetTiming>(reader);
        reader.Read(e.TextureSheet.Cycles);
        reader.Read(e.TextureSheet.FPS);
        e.TextureSheet.RandomStartFrame = ReadBool(reader);
        e.TextureSheet.FrameBlending = ReadBool(reader);

        // Collision
        e.Collision.Enabled = ReadBool(reader);
        Uint planeCount = 0;
        reader.Read(planeCount);
        if(planeCount > ParticleCollisionModule::MAX_PLANES)
        {
            reader.Failed = true;
            return;
        }
        e.Collision.PlaneCount = planeCount;
        for(Uint i = 0; i < planeCount; i++)
        {
            reader.Read(e.Collision.Planes[i].Normal);
            reader.Read(e.Collision.Planes[i].Offset);
        }
        reader.Read(e.Collision.Bounce);
        reader.Read(e.Collision.Dampen);
        reader.Read(e.Collision.LifetimeLoss);
        reader.Read(e.Collision.RadiusScale);

        // SubEmitters
        e.SubEmitters.Enabled = ReadBool(reader);
        Uint entryCount = 0;
        reader.Read(entryCount);
        if(!reader.Fits(entryCount, 1))
        {
            reader.Failed = true;
            return;
        }
        e.SubEmitters.Entries.resize(entryCount);
        for(ParticleSubEmitter& entry : e.SubEmitters.Entries)
        {
            entry.Event = ReadEnum<ParticleSubEmitterEvent>(reader);
            entry.EmitterIndex = ReadInt(reader);
            reader.Read(entry.Count);
            reader.Read(entry.Probability);
            reader.Read(entry.InheritVelocity);
        }

        // Renderer
        e.Renderer.Mode = ReadEnum<ParticleRenderMode>(reader);
        uint64_t textureID = 0;
        reader.Read(textureID);
        reader.Read(e.Renderer.AdditiveAmount);
        reader.Read(e.Renderer.EmissiveIntensity);
        e.Renderer.Sort = ReadEnum<ParticleSortMode>(reader);
        e.Renderer.SortingOrder = ReadInt(reader);
        reader.Read(e.Renderer.MaxScreenSize);
        reader.Read(e.Renderer.StretchSpeedScale);
        reader.Read(e.Renderer.StretchLengthScale);

        // Last, a corrupted file never loads a texture from a garbage ID
        if(!reader.Failed)
            e.Renderer.Texture = LoadTexture(assetManager, textureID);
    }

    //////////////////////////////////////////////////////////////////////////
    // ParticleEffectBinary
    //////////////////////////////////////////////////////////////////////////

    bool Write(const String& path, const AssetStamp& stamp, const ParticleEffect& effect)
    {
        const Uint emitterCount = std::min(static_cast<Uint>(effect.Emitters.size()), ParticleEffect::MAX_EMITTERS);

        Vector<Byte> buffer;
        buffer.reserve(sizeof(AssetStamp) + sizeof(Header) + emitterCount * 1024);
        WriteData(buffer, stamp);

        const Header header = { MAGIC, VERSION, emitterCount };
        WriteData(buffer, header);
        for(Uint i = 0; i < emitterCount; i++)
            WriteEmitter(buffer, effect.Emitters[i]);

        if(!Filesystem::WriteBinaryFile(path, buffer.data(), buffer.size()))
        {
            Log<Severity::Error>("[ParticleEffectBinary] Failed to write {}", path);
            return false;
        }
        Log<Severity::Trace>("[ParticleEffectBinary] Cooked {}", path);
        return true;
    }

    bool Read(AssetManager& assetManager, const String& path, AssetStamp& outStamp, ParticleEffect& outEffect)
    {
        Vector<Byte> fileData;
        if(!Filesystem::ReadBinaryFile(path, fileData))
        {
            Log<Severity::Error>("[ParticleEffectBinary] Failed to read {}", path);
            return false;
        }

        BinaryReader reader(fileData.data(), fileData.size());
        reader.Read(outStamp);

        Header header = {};
        reader.Read(header);
        if(reader.Failed || header.Magic != MAGIC)
        {
            Log<Severity::Error>("[ParticleEffectBinary] {} is not a cooked particle effect!", path);
            return false;
        }
        if(header.Version != VERSION)
        {
            Log<Severity::Error>("[ParticleEffectBinary] {} has version {}, expected {}. Re-cook it in the Editor (or export the game again)", path, header.Version, VERSION);
            return false;
        }
        if(header.EmitterCount > ParticleEffect::MAX_EMITTERS)
        {
            Log<Severity::Error>("[ParticleEffectBinary] {} is corrupted ({} emitters)!", path, header.EmitterCount);
            return false;
        }

        Vector<ParticleEmitterDesc> emitters(header.EmitterCount);
        for(Uint i = 0; i < header.EmitterCount && !reader.Failed; i++)
            ReadEmitter(assetManager, reader, emitters[i]);

        if(reader.Failed)
        {
            Log<Severity::Error>("[ParticleEffectBinary] {} is truncated or corrupted!", path);
            return false;
        }

        outEffect.Emitters = std::move(emitters);
        outEffect.MarkChanged(true);
        return true;
    }
}
