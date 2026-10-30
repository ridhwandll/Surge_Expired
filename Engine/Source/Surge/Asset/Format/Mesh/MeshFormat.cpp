// Copyright (c) - SurgeTechnologies - All rights reserved
#include "MeshFormat.hpp"
#include "Surge/Asset/AssetContext.hpp"
#include "Surge/Asset/Format/BinarySerialization.hpp"
#include "Surge/Asset/Format/Material/MaterialFormat.hpp"

namespace Surge
{
    SURGE_RAPTOR_FIELDS(Submesh, v.BaseVertex, v.BaseIndex, v.MaterialIndex, v.IndexCount, v.VertexCount, v.BoundingBox, v.Transform, v.LocalTransform, v.NodeName, v.MeshName)
    SURGE_RAPTOR_FIELDS(Skeleton, v.NodeNames, v.Parents, v.RestPose, v.PaletteNodes, v.InverseBind, v.PaletteBounds)
    SURGE_RAPTOR_FIELDS(AnimationClip, v.Name, v.Duration, v.SampleRate, v.FrameCount, v.Tracks, v.Translations, v.Rotations, v.Scales)
}

namespace Surge::MeshBinary
{
    SURGE_RAPTOR_RAW(Vertex, 56);
    SURGE_RAPTOR_RAW(Index, 12);
    SURGE_RAPTOR_RAW(AABB, 24);
    SURGE_RAPTOR_RAW(JointPose, 40);
    SURGE_RAPTOR_RAW(VertexSkin, 8);
    SURGE_RAPTOR_RAW(AnimationTrack, 12);

    // Every index is checked: the runtime samples and skins straight from these arrays
    static bool IsValidAnimation(const MeshSpecification& spec)
    {
        const Skeleton& skeleton = spec.Skeleton_;
        const size_t nodeCount = skeleton.Parents.size();
        const size_t paletteSize = skeleton.PaletteNodes.size();
        if(nodeCount == 0 || nodeCount > UINT16_MAX || skeleton.NodeNames.size() != nodeCount || skeleton.RestPose.size() != nodeCount)
            return false;
        for(size_t i = 0; i < nodeCount; i++)
        {
            if(skeleton.Parents[i] >= static_cast<int>(i) || skeleton.Parents[i] < -1)
                return false; // Parents first is what lets the hierarchy be built in one pass
        }

        if(paletteSize == 0 || paletteSize > Skeleton::MAX_PALETTE_SIZE || skeleton.InverseBind.size() != paletteSize || skeleton.PaletteBounds.size() != paletteSize)
            return false;
        for(Uint node : skeleton.PaletteNodes)
        {
            if(node >= nodeCount)
                return false;
        }

        if(spec.Skin.size() != spec.Vertices.size())
            return false;
        for(const VertexSkin& skin : spec.Skin)
        {
            for(Uint shift = 0; shift < 32; shift += 8)
            {
                if(((skin.Joints >> shift) & 0xFFu) >= paletteSize)
                    return false;
            }
        }

        for(const AnimationClip& clip : spec.Clips)
        {
            if(!(clip.Duration >= 0.0f) || !(clip.SampleRate > 0.0f) || clip.FrameCount == 0)
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
        return true;
    }

    void Write(Rapt0R::Writer& writer, const MeshSpecification& spec, const Vector<AssetID>& overrides)
    {
        Uint overrideCount = 0;
        for(const AssetID& id : overrides)
            overrideCount += id.IsValid() ? 1 : 0;

        writer.Write(spec.Vertices, spec.Indices, spec.Submeshes, static_cast<Uint>(spec.Materials.size()));
        for(const Ref<Material>& mat : spec.Materials)
            MaterialBinary::Write(writer, mat.Raw());

        writer.Write(overrideCount);
        for(Uint i = 0; i < static_cast<Uint>(overrides.size()); i++)
        {
            if(overrides[i].IsValid())
                writer.Write(i, overrides[i].Get());
        }

        const bool hasSkeleton = spec.Skeleton_.IsValid() && spec.Skin.size() == spec.Vertices.size();
        writer.Write(hasSkeleton);
        if(hasSkeleton)
            writer.Write(spec.Skeleton_, spec.Skin, spec.Clips);
    }

    bool Read(const AssetContext& context, Rapt0R::Reader& reader, MeshSpecification& outSpec)
    {
        Uint materialCount = 0;
        if(reader.Read(outSpec.Vertices, outSpec.Indices, outSpec.Submeshes, materialCount) && reader.EnsureElements(materialCount))
        {
            outSpec.Materials.resize(materialCount);
            for(Uint i = 0; i < materialCount && !reader.HasError(); i++)
                outSpec.Materials[i] = MaterialBinary::Read(context, reader);
        }

        Uint overrideCount = 0;
        outSpec.MaterialOverrides.assign(outSpec.Submeshes.size(), AssetID::INVALID);
        if(reader.Read(overrideCount) && reader.EnsureElements(overrideCount, sizeof(Uint) + sizeof(uint64_t)))
        {
            for(Uint i = 0; i < overrideCount; i++)
            {
                Uint slotIndex = 0;
                uint64_t rawID = 0;
                if(reader.Read(slotIndex, rawID) && slotIndex < static_cast<Uint>(outSpec.Submeshes.size()))
                    outSpec.MaterialOverrides[slotIndex] = AssetID(rawID);
            }
        }

        bool hasSkeleton = false;
        if(reader.Read(hasSkeleton) && hasSkeleton && reader.Read(outSpec.Skeleton_, outSpec.Skin, outSpec.Clips) && !IsValidAnimation(outSpec))
        {
            Log<Severity::Error>("[MeshBinary] Corrupted animation section!");
            return false;
        }
        return !reader.HasError();
    }
}

namespace Surge
{
    static bool GenerateDefaultMesh(Renderer& renderer, const String& name, MeshSpecification& outSpec)
    {
        MeshGenerator::MeshData meshData = MeshGenerator::GenerateDefaultMesh(name);
        if(meshData.Vertices.empty())
            return false;

        outSpec.Vertices = std::move(meshData.Vertices);
        outSpec.Indices = std::move(meshData.Indices);

        outSpec.Submeshes.emplace_back();
        Submesh& submesh = outSpec.Submeshes.back();
        submesh.BaseVertex = 0;
        submesh.BaseIndex = 0;
        submesh.MaterialIndex = 0;

        submesh.VertexCount = (Uint)outSpec.Vertices.size();
        submesh.IndexCount = (Uint)outSpec.Indices.size() * 3;

        submesh.Transform = glm::mat4(1.0f);
        submesh.LocalTransform = glm::mat4(1.0f);
        submesh.MeshName = "DefaultMesh";
        submesh.NodeName = "DefaultMesh_Node";

        submesh.BoundingBox.Reset();
        glm::vec3 minBound(std::numeric_limits<float>::max());
        glm::vec3 maxBound(-std::numeric_limits<float>::max());
        for(const auto& v : outSpec.Vertices)
        {
            minBound = glm::min(minBound, v.Position);
            maxBound = glm::max(maxBound, v.Position);
        }
        submesh.BoundingBox.Min = minBound;
        submesh.BoundingBox.Max = maxBound;

        outSpec.Materials.emplace_back(Material::Create(renderer, "DefaultMaterial"));
        outSpec.Materials[0]->SetName("DefaultMaterial");
        outSpec.Materials[0]->Set<glm::vec3>("Albedo", glm::vec3(0.8f));
        outSpec.Materials[0]->Set<float>("Metallic", 0.5f);
        outSpec.Materials[0]->Set<float>("Roughness", 0.5f);
        outSpec.Materials[0]->Set<float>("Reflectance", 0.5f);
        return true;
    }

    Ref<Asset> MeshFormat::Read(CookedFile& file, const AssetMetadata& metadata, const AssetContext& context) const
    {
        SCOPED_TIMER("MeshFormat::Read: {}", metadata.RelativePath);
        MeshSpecification spec;
        Rapt0R::Reader payload = file.GetPayload();
        if(!MeshBinary::Read(context, payload, spec))
        {
            Log<Severity::Error>("[MeshFormat] {} is truncated or corrupted, returning a default sweet cube!", metadata.RelativePath);
            spec = {};
            GenerateDefaultMesh(*context.Renderer_, DefaultMesh::CUBE, spec);
        }
        return Mesh::Create(*context.Renderer_, *context.AssetManager_, std::move(spec)).As<Asset>();
    }

    Ref<Asset> MeshFormat::CreateBuiltin(const AssetMetadata& metadata, const AssetContext& context) const
    {
        MeshSpecification spec;
        if(!GenerateDefaultMesh(*context.Renderer_, metadata.RelativePath, spec))
            return nullptr;
        return Mesh::Create(*context.Renderer_, *context.AssetManager_, std::move(spec)).As<Asset>();
    }
}
