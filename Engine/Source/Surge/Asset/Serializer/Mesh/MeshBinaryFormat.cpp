// Copyright (c) - SurgeTechnologies - All rights reserved
#include "MeshBinaryFormat.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Asset/Serializer/BinaryHelpers.hpp"
#include "Surge/Asset/Serializer/Material/MaterialBinaryFormat.hpp"

namespace Surge::MeshBinary
{
    static_assert(std::is_trivially_copyable_v<Vertex>, "Vertex must be trivially copyable for binary sidecar serialization");
    static_assert(std::is_trivially_copyable_v<Index>, "Index must be trivially copyable for binary sidecar serialization");
    static constexpr Uint kSidecarMagic = 0x4D444952; // RIDM
    static constexpr Uint kSidecarVersion = 3; // 3: skeleton, skin and clips section
    struct Header
    {
        Uint Magic;
        Uint Version;
        Uint VertexCount;
        Uint IndexCount;
        Uint SubmeshCount;
        Uint TransientMaterialCount;
        Uint ValidOverrideCount;
        Uint GeomSectionSize;
    };
    static_assert(sizeof(Header) == 32);
    static_assert(std::is_trivially_copyable_v<JointPose> && std::is_trivially_copyable_v<VertexSkin> && std::is_trivially_copyable_v<AnimationTrack>);
#if defined(SURGE_PLATFORM_WINDOWS) && defined(SURGE_RELEASE)
    static_assert(sizeof(JointPose) == 40 && sizeof(VertexSkin) == 8 && sizeof(AnimationTrack) == 12, "Mesh sidecar layout changed, bump kSidecarVersion");
#endif

    //[Animation section] (after the material overrides)
    //    HasSkeleton            Uint      0 = static mesh, nothing follows
    //    NodeCount              Uint      then per node: Name (Str), Parent (int), RestPose (JointPose)
    //    PaletteSize            Uint      then per entry: Node (Uint), InverseBind (mat4), BoundsMin/Max (vec3)
    //    [BLOB: VertexSkin]     VertexCount entries
    //    ClipCount              Uint      then per clip: Name (Str), Duration, SampleRate (float), FrameCount, TrackCount (Uint),
    //                                     [Tracks], TranslationCount + [vec3], RotationCount + [vec4], ScaleCount + [vec3]
    static void WriteAnimation(Vector<Byte>& buffer, const MeshSpecification& spec)
    {
        const Skeleton& skeleton = spec.Skeleton_;
        const Uint hasSkeleton = skeleton.IsValid() && spec.Skin.size() == spec.Vertices.size() ? 1 : 0;
        WriteData(buffer, hasSkeleton);
        if(!hasSkeleton)
            return;

        WriteData(buffer, skeleton.GetNodeCount());
        for(Uint i = 0; i < skeleton.GetNodeCount(); i++)
        {
            WriteStr(buffer, skeleton.NodeNames[i]);
            WriteData(buffer, skeleton.Parents[i]);
            WriteData(buffer, skeleton.RestPose[i]);
        }

        WriteData(buffer, skeleton.GetPaletteSize());
        for(Uint i = 0; i < skeleton.GetPaletteSize(); i++)
        {
            WriteData(buffer, skeleton.PaletteNodes[i]);
            WriteData(buffer, skeleton.InverseBind[i]);
            WriteData(buffer, skeleton.PaletteBounds[i].Min);
            WriteData(buffer, skeleton.PaletteBounds[i].Max);
        }

        WriteDataArray(buffer, spec.Skin.data(), spec.Skin.size());

        WriteData(buffer, static_cast<Uint>(spec.Clips.size()));
        for(const AnimationClip& clip : spec.Clips)
        {
            WriteStr(buffer, clip.Name);
            WriteData(buffer, clip.Duration);
            WriteData(buffer, clip.SampleRate);
            WriteData(buffer, clip.FrameCount);
            WriteData(buffer, static_cast<Uint>(clip.Tracks.size()));
            WriteDataArray(buffer, clip.Tracks.data(), clip.Tracks.size());
            WriteData(buffer, static_cast<Uint>(clip.Translations.size()));
            WriteDataArray(buffer, clip.Translations.data(), clip.Translations.size());
            WriteData(buffer, static_cast<Uint>(clip.Rotations.size()));
            WriteDataArray(buffer, clip.Rotations.data(), clip.Rotations.size());
            WriteData(buffer, static_cast<Uint>(clip.Scales.size()));
            WriteDataArray(buffer, clip.Scales.data(), clip.Scales.size());
        }
    }

    template <typename T>
    static bool ReadCountedArray(BinaryReader& reader, Vector<T>& out)
    {
        Uint count = 0;
        reader.Read(count);
        if(reader.Failed || !reader.Fits(count, sizeof(T)))
            return false;
        out.resize(count);
        reader.ReadArray(out.data(), count);
        return !reader.Failed;
    }

    // Every index is checked: the runtime samples and skins straight from these arrays
    static bool ReadAnimation(BinaryReader& reader, MeshSpecification& spec)
    {
        Uint hasSkeleton = 0;
        reader.Read(hasSkeleton);
        if(reader.Failed)
            return false;
        if(hasSkeleton == 0)
            return true;

        Skeleton& skeleton = spec.Skeleton_;
        Uint nodeCount = 0;
        reader.Read(nodeCount);
        if(reader.Failed || nodeCount == 0 || nodeCount > UINT16_MAX || !reader.Fits(nodeCount, sizeof(Uint) + sizeof(int) + sizeof(JointPose)))
            return false;

        skeleton.NodeNames.resize(nodeCount);
        skeleton.Parents.resize(nodeCount);
        skeleton.RestPose.resize(nodeCount);
        for(Uint i = 0; i < nodeCount && !reader.Failed; i++)
        {
            skeleton.NodeNames[i] = reader.ReadStr();
            reader.Read(skeleton.Parents[i]);
            reader.Read(skeleton.RestPose[i]);
            if(skeleton.Parents[i] >= static_cast<int>(i) || skeleton.Parents[i] < -1)
                return false; // Parents first is what lets the hierarchy be built in one pass
        }

        Uint paletteSize = 0;
        reader.Read(paletteSize);
        if(reader.Failed || paletteSize == 0 || paletteSize > Skeleton::MAX_PALETTE_SIZE || !reader.Fits(paletteSize, sizeof(Uint) + sizeof(glm::mat4) + 2 * sizeof(glm::vec3)))
            return false;

        skeleton.PaletteNodes.resize(paletteSize);
        skeleton.InverseBind.resize(paletteSize);
        skeleton.PaletteBounds.resize(paletteSize);
        for(Uint i = 0; i < paletteSize && !reader.Failed; i++)
        {
            reader.Read(skeleton.PaletteNodes[i]);
            reader.Read(skeleton.InverseBind[i]);
            reader.Read(skeleton.PaletteBounds[i].Min);
            reader.Read(skeleton.PaletteBounds[i].Max);
            if(skeleton.PaletteNodes[i] >= nodeCount)
                return false;
        }

        const size_t vertexCount = spec.Vertices.size();
        if(reader.Failed || !reader.Fits(vertexCount, sizeof(VertexSkin)))
            return false;
        spec.Skin.resize(vertexCount);
        reader.ReadArray(spec.Skin.data(), vertexCount);
        for(const VertexSkin& skin : spec.Skin)
        {
            for(Uint shift = 0; shift < 32; shift += 8)
            {
                if(((skin.Joints >> shift) & 0xFFu) >= paletteSize)
                    return false;
            }
        }

        Uint clipCount = 0;
        reader.Read(clipCount);
        if(reader.Failed || !reader.Fits(clipCount, sizeof(Uint) + 2 * sizeof(float)))
            return false;

        spec.Clips.resize(clipCount);
        for(AnimationClip& clip : spec.Clips)
        {
            clip.Name = reader.ReadStr();
            reader.Read(clip.Duration);
            reader.Read(clip.SampleRate);
            reader.Read(clip.FrameCount);
            if(reader.Failed || !(clip.Duration >= 0.0f) || !(clip.SampleRate > 0.0f) || clip.FrameCount == 0)
                return false;

            if(!ReadCountedArray(reader, clip.Tracks) || !ReadCountedArray(reader, clip.Translations) ||
               !ReadCountedArray(reader, clip.Rotations) || !ReadCountedArray(reader, clip.Scales))
                return false;

            for(const AnimationTrack& track : clip.Tracks)
            {
                if(track.Node >= nodeCount || (track.KeyCount != 1 && track.KeyCount != clip.FrameCount))
                    return false;

                size_t poolSize = 0;
                switch(track.Path)
                {
                    case AnimationPath::TRANSLATION: poolSize = clip.Translations.size(); break;
                    case AnimationPath::ROTATION:    poolSize = clip.Rotations.size(); break;
                    case AnimationPath::SCALE:       poolSize = clip.Scales.size(); break;
                    default:                         return false;
                }
                if(static_cast<size_t>(track.FirstKey) + track.KeyCount > poolSize)
                    return false;
            }
        }
        return !reader.Failed;
    }

    bool Write(const String& path, const AssetStamp& stamp, const MeshSpecification& spec, const Vector<Ref<Material>>& overrides)
    {
        Uint validOverrideCount = 0;
        for(const Ref<Material>& ref : overrides)
        {
            if(ref)
                validOverrideCount++;
        }

        static constexpr Uint sSubmeshPODSize =
            5 * sizeof(Uint) +       // BaseVertex/BaseIndex/MaterialIndex/IndexCount/VertexCount
            2 * sizeof(glm::vec3) +  // BoundsMin + BoundsMax
            2 * sizeof(glm::mat4);   // Transform + LocalTransform

        Uint geomSize = 0;
        geomSize += static_cast<Uint>(spec.Vertices.size()) * sizeof(Vertex);
        geomSize += static_cast<Uint>(spec.Indices.size()) * sizeof(Index);
        for(const Submesh& sm : spec.Submeshes)
        {
            geomSize += sSubmeshPODSize;
            geomSize += sizeof(Uint) + static_cast<Uint>(sm.NodeName.size());
            geomSize += sizeof(Uint) + static_cast<Uint>(sm.MeshName.size());
        }
        for(const Ref<Material>& mat : spec.Materials)
            geomSize += MaterialBinary::CalculateSize(mat);

        Vector<Byte> buffer;
        buffer.reserve(sizeof(AssetStamp) + sizeof(Header) + geomSize + validOverrideCount * (sizeof(Uint) + sizeof(uint64_t)));

        Header header = {};
        header.Magic = kSidecarMagic;
        header.Version = kSidecarVersion;
        header.VertexCount = static_cast<Uint>(spec.Vertices.size());
        header.IndexCount = static_cast<Uint>(spec.Indices.size());
        header.SubmeshCount = static_cast<Uint>(spec.Submeshes.size());
        header.TransientMaterialCount = static_cast<Uint>(spec.Materials.size());
        header.ValidOverrideCount = validOverrideCount;
        header.GeomSectionSize = geomSize;

        WriteData(buffer, stamp);
        WriteData(buffer, header);
        WriteDataArray(buffer, spec.Vertices.data(), spec.Vertices.size());
        WriteDataArray(buffer, spec.Indices.data(), spec.Indices.size());

        for(const Submesh& sm : spec.Submeshes)
        {
            WriteData(buffer, sm.BaseVertex);
            WriteData(buffer, sm.BaseIndex);
            WriteData(buffer, sm.MaterialIndex);
            WriteData(buffer, sm.IndexCount);
            WriteData(buffer, sm.VertexCount);
            WriteData(buffer, sm.BoundingBox.Min);
            WriteData(buffer, sm.BoundingBox.Max);
            WriteData(buffer, sm.Transform);
            WriteData(buffer, sm.LocalTransform);
            WriteStr(buffer, sm.NodeName);
            WriteStr(buffer, sm.MeshName);
        }

        for(const Ref<Material>& mat : spec.Materials)
            MaterialBinary::WriteBuffer(buffer, mat);

        for(Uint i = 0; i < static_cast<Uint>(overrides.size()); i++)
        {
            if(!overrides[i]) continue;
            Uint slotIndex = i;
            uint64_t rawID = overrides[i]->GetID().Get();
            WriteData(buffer, slotIndex);
            WriteData(buffer, rawID);
        }

        WriteAnimation(buffer, spec);

        if(!Filesystem::WriteBinaryFile(path, buffer.data(), buffer.size()))
        {
            Log<Severity::Error>("[MeshBinary] Failed to write sidecar: {}", path);
            return false;
        }
        Log<Severity::Trace>("[MeshBinary] Cooked sidecar: {}", path);
        return true;
    }

    bool Read(const AssetContext& context, const String& path, AssetStamp& outstamp, MeshSpecification& outSpec)
    {
        Vector<Byte> fileData;
        if(!Filesystem::ReadBinaryFile(path, fileData))
            return false;

        if(fileData.size() < sizeof(AssetStamp) + sizeof(Header))
            return false;

        BinaryReader reader(fileData.data(), fileData.size());
        reader.Read(outstamp);

        Header header = {};
        reader.Read(header);

        if(header.Magic != kSidecarMagic)
        {
            Log<Severity::Warn>("[MeshSerializer] {} has bad magic, ignoring sidecar.", path);
            return false;
        }
        if(header.Version != kSidecarVersion)
        {
            Log<Severity::Warn>("[MeshSerializer] {} version mismatch (got {}, want {}), re-cooking!", path, header.Version, kSidecarVersion);
            return false;
        }

        // Every count comes from the file: check it against the bytes left before allocating from it (each entry is at least one byte)
        if(!reader.Fits(header.VertexCount, sizeof(Vertex)) || !reader.Fits(header.IndexCount, sizeof(Index)) ||
           !reader.Fits(header.SubmeshCount, 1) || !reader.Fits(header.TransientMaterialCount, 1))
        {
            Log<Severity::Error>("[MeshSerializer] {} is truncated or corrupted!", path);
            return false;
        }

        outSpec.Vertices.resize(header.VertexCount);
        reader.ReadArray(outSpec.Vertices.data(), header.VertexCount);

        outSpec.Indices.resize(header.IndexCount);
        reader.ReadArray(outSpec.Indices.data(), header.IndexCount);

        outSpec.Submeshes.reserve(header.SubmeshCount);
        for(Uint i = 0; i < header.SubmeshCount && !reader.Failed; i++)
        {
            Submesh& sm = outSpec.Submeshes.emplace_back();
            reader.Read(sm.BaseVertex);
            reader.Read(sm.BaseIndex);
            reader.Read(sm.MaterialIndex);
            reader.Read(sm.IndexCount);
            reader.Read(sm.VertexCount);
            reader.Read(sm.BoundingBox.Min);
            reader.Read(sm.BoundingBox.Max);
            reader.Read(sm.Transform);
            reader.Read(sm.LocalTransform);
            sm.NodeName = reader.ReadStr();
            sm.MeshName = reader.ReadStr();
        }

        outSpec.Materials.resize(header.TransientMaterialCount);
        for(Uint i = 0; i < header.TransientMaterialCount && !reader.Failed; i++)
            outSpec.Materials[i] = MaterialBinary::ReadBuffer(context, reader);

        outSpec.MaterialOverrides.assign(outSpec.Submeshes.size(), AssetID::INVALID);
        for(Uint i = 0; i < header.ValidOverrideCount && !reader.Failed; i++)
        {
            Uint slotIndex = 0;
            uint64_t rawID = 0;
            reader.Read(slotIndex);
            reader.Read(rawID);
            if(!reader.Failed && slotIndex < static_cast<Uint>(outSpec.Submeshes.size()))
                outSpec.MaterialOverrides[slotIndex] = AssetID(rawID);
        }

        if(!reader.Failed && !ReadAnimation(reader, outSpec))
        {
            Log<Severity::Error>("[MeshSerializer] {} has a corrupted animation section!", path);
            return false;
        }

        if(reader.Failed)
        {
            Log<Severity::Error>("[MeshSerializer] {} is truncated or corrupted!", path);
            return false;
        }
        return true;
    }
}